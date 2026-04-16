# Working on Tables — Developer Guide

**Audience:** Any agent or developer modifying table-related code in Markoff.
Read this before touching `SceneCoordinator`, `MarkdownTextItem`, `TableConverter`,
`TableSerializer`, `MarkdownHighlighter`, or `TextControl` in any table-related context.

## How Tables Work

Tables live as `QTextTable` frames inside `MarkdownTextItem`'s `QTextDocument`.
They are NOT separate scene items. The pipe-delimited markdown is a serialization
format only — the user never sees or edits it directly.

### The Pipeline

```
File on disk (pipe markdown)
  ↓ Editor::setPlainText()
  ↓ SceneCoordinator::loadMarkdown()
  ↓ MarkdownSplitter::split()          — tables stay in Text segments
  ↓ createTextItem(seg.text)           — document has raw pipe text
  ↓ stripInlineSubstitutions()         — remove math glyphs (source form)
  ↓ TableConverter::convert()          — pipe text → QTextTable frames
  ↓ Rebuild span map (offset-mapped)   — see "The Offset Problem" below
  ↓ refreshInlineSubstitutions()       — re-apply math glyphs
  ↓ rehighlight()                      — apply corrected spans to display
```

### The Offset Problem

This is the hardest part of the table implementation. Understand it before
changing anything in the reparse or highlighting pipeline.

**The core tension:** The tree-sitter parser operates on flat markdown text
(including pipe tables serialized by `allMarkdown()`). But the document
contains `QTextTable` frames which occupy a different number of characters
than the pipe text. Every span offset after a table is wrong if used directly.

**The solution (anchor-based offset mapping):** After parsing the pipe text
and building the span map, we compute the exact offset delta by finding a
known text anchor after each table in both the pipe string and the document.
Spans inside table pipe-text regions are dropped (the highlighter skips
table blocks anyway). Spans after tables are shifted by the measured delta.

**Why not parse a "highlighting source" with tables replaced by spaces?**
We tried this (`buildHighlightingSource()`). It produces correct offsets for
non-math content, but tree-sitter's markdown grammar fails to recognize
`$...$` math expressions when preceded by large runs of spaces/newlines.
The anchor-based approach avoids this by parsing the real pipe text that
tree-sitter handles correctly.

### The Source Position Span Cache

`MarkdownTextItem` caches span positions in `m_sourcePositionSpans` so that
`refreshInlineSubstitutions()` can restore source-form spans after stripping.
**This cache must be invalidated** (via `invalidateSourcePositionSpans()`)
whenever you externally set a new span map with different offsets. If you
don't, the cache overwrites your corrected spans on the next substitution
refresh.

This was the cause of a multi-hour debugging session. Don't repeat it.

## Invariants — Do Not Break These

### 1. Once a table, always a table.

Once pipe text is converted to a `QTextTable`, it stays that way. The reparse
cycle must recognize existing tables and not re-convert them. The serialization
round-trip (QTextTable → pipe text → parse → detect table → reconcile) must
be idempotent.

### 2. Signals must be blocked during the strip/convert/rebuild cycle.

`adjustSpanOffsets()` is connected to `QTextDocument::contentsChange`. Every
document mutation fires it. If signals are not blocked during the table
conversion and span rebuild, `adjustSpanOffsets` fires for each mutation AND
`applyInlineSubstitutions` also adjusts spans manually — double adjustment.

Always wrap the cycle in:
```cpp
const bool blocked = doc->blockSignals(true);
// ... strip, convert, set span map, refresh substitutions ...
doc->blockSignals(blocked);
hl->rehighlight(); // after unblocking
```

### 3. Strip inline substitutions before table conversion.

`createTextItem()` applies math/checkbox substitutions (U+FFFC), which change
character positions. The table converter uses offsets from the raw segment text.
If substitutions are present, those offsets point to wrong positions.

Always `stripInlineSubstitutions()` before `convert()`.

### 4. The highlighter skips table blocks.

`MarkdownHighlighter::highlightBlock()` has a frame guard (lines 310-320) that
returns early for any block inside a `QTextTable` frame. This prevents the
span-based formatter from applying markdown formatting to table cell content
(which would hide text via delimiter hiding, apply wrong styles, etc.).

Do not remove this guard. If you want formatting in table cells (scope (c)),
you need a separate per-cell formatting path.

### 5. Table-internal spans are dropped in the offset mapping.

The span map from tree-sitter contains spans for pipe characters (`|`),
separator dashes (`---`), and cell content. These are all dropped during
offset mapping because they describe pipe-text structure, not document
structure. The highlighter skips table blocks anyway, so these spans are
useless.

### 6. allMarkdown() uses the frame iterator, not block iteration.

`QTextDocument::begin()` / `block.next()` skips blocks inside `QTextFrame`
children (including `QTextTable`). The `allMarkdown()` method uses
`QTextFrame::iterator` on the root frame, which visits both regular blocks
AND child frames in document order. Tables are serialized via
`TableSerializer::serialize()`.

## Files and Responsibilities

| File | Role | Fragility |
|------|------|-----------|
| `SceneCoordinator.cpp` | Orchestrates load, reparse, table conversion, offset mapping | **HIGH** — the reparse pipeline is the most delicate code |
| `MarkdownTextItem.cpp` | `allMarkdown()`, `buildHighlightingSource()`, inline substitution | **HIGH** — offset-sensitive |
| `TableConverter.cpp` | Pipe text → QTextTable, reparse reconciliation | Medium |
| `TableSerializer.cpp` | QTextTable → auto-formatted pipe text | Low — standalone utility |
| `TextControl.cpp` | Tab/Enter/Escape/arrow navigation in tables | Low — isolated key handlers |
| `MarkdownHighlighter.cpp` | Frame skip guard, span application | Medium — the guard is critical |
| `Editor.cpp` | Table signals, operation slots, context menu | Low |

## Testing

Run all markoff tests before committing any table-related change:
```bash
cd build && ctest -R markoff --output-on-failure
```

Key test files:
- `tst_table_serializer.cpp` — round-trip serialization
- `tst_table_converter.cpp` — pipe text → QTextTable conversion
- `tst_table_navigation.cpp` — keyboard navigation
- `tst_table_operations.cpp` — insert/delete row/column via API
- `tst_table_integration.cpp` — end-to-end through Editor
- `tst_table_bugs.cpp` — remnant text regression tests
- `tst_table_diagnostics.cpp` — structural diagnostics for the showcase

**Manual smoke test** (always do this for visual changes):
```bash
./build/bin/markoff-testapp libs/markoff/tests/showcase.md
```
Check: tables render, text after tables has correct styling, math renders,
headings are styled, no remnant pipe text fragments.

## Common Mistakes

### Don't parse buildHighlightingSource() for the span map in the reparse path.

Tree-sitter's markdown grammar doesn't correctly identify `$...$` math
expressions when the preceding content is replaced with spaces. Use
`allMarkdown()` for parsing and apply anchor-based offset mapping instead.

`buildHighlightingSource()` is still used in the `loadMarkdown()` path
(initial load) where it works because the initial span map doesn't need
to survive through the reparse cycle.

### Don't forget to invalidate the source position span cache.

After setting a new span map via `setSpanMap()` in the reparse path,
call `textItem->invalidateSourcePositionSpans()`. Otherwise
`refreshInlineSubstitutions()` restores the old cached spans.

### Don't compute table document footprints from frame positions.

The number of characters a `QTextTable` frame occupies in the document
depends on cell content, empty boundary blocks, and paragraph separators.
Calculating this from `firstPosition()` / `lastPosition()` is off by 2-3
characters. Use the anchor-based empirical measurement instead.

### Don't let the table converter's endPos eat into adjacent content.

The tree-sitter table boundary may not include trailing newlines. When
expanding to line boundaries, do NOT include the trailing newline — it
serves as a separator between the table and whatever follows. Including
it causes the converter to consume adjacent tables or text.

### Don't run table conversion without stripping substitutions first.

Math glyphs (U+FFFC) are 1 character each, but the source text they replace
can be 50+ characters. If you convert tables while substitutions are present,
the converter's offsets (from the raw segment text) point past the actual
document positions.

## Where We Go From Here

### Immediate follow-ups

- **Table visual styling** — The tables currently use Qt's default
  `QTextDocumentLayout` rendering (basic grid). `TableStyle` struct exists
  with theming defaults but isn't wired into rendering yet. Header background,
  grid line colors, cell padding need to be applied via `QTextTableFormat`
  and `QTextTableCellFormat`.

- **Reparse stability** — Editing inside table cells triggers the reparse
  timer. The non-structural path reconciles correctly, but structural changes
  (adding/removing tables by typing pipe text) need more testing. Row
  insertion that triggers structural reparse may cause instability.

- **Smart cursor entry** — The spec calls for x-position → nearest column
  mapping when arrowing into a table. Currently falls back to first/last cell.

### Scope (b)

- Column alignment controls (UI in context menu + API slots)
- Row/column move operations
- Column resize drag handles
- Auto-format pipe text on every cell edit (live width normalization)
- Header row bold styling, theming

### Scope (c)

- Spreadsheet formulas
- Column sorting
- CSV import/export
- Full inline objects in cells (requires span map integration for cell content — a significant architectural extension)
- Rectangular paste-into-table

### Architectural notes for scope (c)

Full inline formatting in cells requires the span map to cover table cell
content. This means either:
1. The offset mapping must become bidirectional (document → pipe text for
   cell content spans), or
2. A separate per-cell parse/span-map system that operates in cell-local
   coordinates

Option 2 is cleaner but means each cell effectively has its own mini
highlighter. This is analogous to Obsidian's approach (per-cell CodeMirror
instances) but in Qt terms.
