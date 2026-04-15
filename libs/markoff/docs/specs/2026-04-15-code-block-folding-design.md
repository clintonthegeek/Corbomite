# Code-Block Folding — Design

> **Status:** Design approved 2026-04-15. Follows v1 heading folding
> ([`2026-04-15-heading-folding-design.md`](./2026-04-15-heading-folding-design.md)).
> Target: first v2 deliverable.

## Scope

Add fold support for **fenced** code blocks (\`\`\` … \`\`\` and ~~~ … ~~~).
Indented code blocks (4-space prefix) are deferred to a later sprint —
they live inside `MarkdownTextItem`s and need block-level AST detection
the v1 infrastructure doesn't yet support. This spec covers fenced only.

## Summary

Extend `FoldingModel` so a fenced code block is a foldable region with
its own path-based identity, reconciled per reparse, folded/unfolded via
the same API surface as headings. When folded, the owning
`TableBlockItem` paints a single-line summary (`` ```cpp (12 lines) ``)
in place of its full content; the gutter shows a collapsed arrow at the
same Y. The in-memory region model unifies headings and code blocks so
later sprints (lists, block quotes) plug in with no further refactoring.

## Key encoding

A code block's path is the enclosing heading's hierarchy path followed
by `code:N` where N is the 0-based ordinal of the code block within
that heading's section.

```
# Intro               ← heading path ["Intro"]
                      ←
```python             ← code-block path ["Intro", "code:0"]
print("a")
```
                      ←
```cpp                ← code-block path ["Intro", "code:1"]
puts("b");
```
                      ←
## Goals              ← heading path ["Intro", "Goals"]
                      ←
```rust               ← code-block path ["Intro", "Goals", "code:0"]
```
```

**Ordinal reset:** the counter resets to 0 on every heading entry
(regardless of level), because the heading path prefix changes.

**Preamble case:** code blocks before any heading get an empty parent
path — e.g. the first preamble code block is `["code:0"]`. This is the
same behaviour as a heading with no ancestors, just applied to a
synthetic empty parent.

**Duplicate-sibling caveat (inherited from v1):** two identical heading
paths already get `#N` suffixes in v1; code-block ordinals live
downstream of the disambiguated heading path, so duplicate headings
don't affect code-block identity.

## `FoldingModel` data shape

Replace the heading-specific cache with a unified region list.

```cpp
struct FoldableRegion {
    enum Type { Heading, CodeBlock };
    Type type;
    FoldRegionKey path;
    int sourceOffset;    // UTF-8 byte offset in toMarkdown()
    int level;           // 1..6 for Heading; 0 for CodeBlock
    // Heading-specific (valid only when type == Heading):
    HeadingInfo info;
    // Code-block-specific (valid only when type == CodeBlock):
    QString language;    // from fence info string, may be empty
    int lineCount;       // lines strictly inside the fence (no fence markers)
};

class FoldingModel : public QObject {
    // ...
    const QList<FoldableRegion> &regions() const { return m_regions; }

    // v1-compatible convenience views — return new QLists filtered by type.
    QList<HeadingEntry> headings() const;          // type == Heading
    QList<FoldableRegion> codeBlockRegions() const; // type == CodeBlock

    // Test hook sibling to setHeadingsForTesting.
    void setRegionsForTesting(QList<FoldableRegion> regions);
private:
    QList<FoldableRegion> m_regions;
    QSet<FoldRegionKey> m_folded; // unchanged
};
```

All v1 internal iteration (`foldAll`, `foldAllAtLevel(level)`, etc.)
switches to iterate `m_regions` with type-aware predicates:

- `foldAll()`: every region, regardless of type.
- `foldAllAtLevel(L)`: headings only, `r.level == L`.
- `foldLevel(n)`: headings only, `r.level >= n`. (Code blocks have no
  level; untouched by these.)
- `foldAllCodeBlocks()` / `unfoldAllCodeBlocks()` (new): only code-block
  regions.
- `foldAllCodeBlocksInSection(const FoldRegionKey &headingPath)` (new):
  code-block regions whose `path[0..path.size()-2] == headingPath`.
  This powers Ctrl+Click on a code-block arrow.

The v1 public `fold(path)`/`unfold(path)`/`toggle(path)` API is unchanged
— paths are opaque `QStringList`s and the model doesn't validate them.

## Region detection

In `SceneCoordinator::ensureHeadingMap` (which already parses
`toMarkdown()` with a fresh `TreeSitterParser`), extend the CST walk to
collect **both** `atx_heading` AND `fenced_code_block` nodes in document
order. For each:

- `atx_heading` → emit a `FoldableRegion{type=Heading, ...}` using the
  existing heading-path computation.
- `fenced_code_block` → emit `FoldableRegion{type=CodeBlock, ...}` with:
  - `path = currentHeadingPath + ("code:" + QString::number(ordinal))`
  - `sourceOffset = ts_node_start_byte(node)`
  - `language = <info-string text>` (or empty)
  - `lineCount = <count of `\n`s between the opening fence line and the
    closing fence line, exclusive of both>`

**Ordinal bookkeeping:** walk regions in document order; whenever a
heading is emitted, reset `codeOrdinal = 0`; whenever a code block is
emitted, assign its ordinal and increment.

`TreeSitterParser::buildDocumentQueries()` already returns `headings`.
Add a sibling `codeBlocks` field (or extend its return shape). The
existing method walks the CST once; adding a second node-type filter is
cheap. Alternative: a new `buildCodeBlockInfo()` method if the existing
shape is tightly coupled to heading-only consumers.

## Reconcile

`FoldingModel::reconcile(QList<HeadingInfo>)` becomes
`reconcile(QList<FoldableRegion>)`. Same loop:

1. Rebuild `m_regions` from the new list.
2. Intersect `m_folded` with the new path set (drops paths that no
   longer exist).
3. Emit `foldStateChanged` if the folded set shrunk.

Callers: `Editor::headingsChanged` currently passes
`QList<HeadingInfo>`. The Editor gains an equivalent
`regionsChanged(QList<FoldableRegion>)` signal and wires that to
`FoldingModel::reconcile`. `headingsChanged(QList<HeadingInfo>)` is kept
as a filter-view emission for any existing Corbomite consumers that
still care about just headings.

## Visual treatment — `TableBlockItem` folded mode

`TableBlockItem` renders fenced code blocks today (yes, the name is
misleading; see the v1 spec's note that MarkdownSplitter routes both
`Table` and `FencedCodeBlock` segments through the same item class).

Add two states to the item:

```cpp
class TableBlockItem {
public:
    void setFolded(bool folded);
    bool isFolded() const;

    // When true, boundingRect reports a single-line height and paint()
    // renders only the summary label.
    //
    // Summary format: "```<lang> (<N> lines)"
    //   - Use "```" even when the original fence was ~~~.
    //   - If lineCount == 1, render "(1 line)".
    //   - If language is empty, render "``` (N lines)" (no language slot).

private:
    bool m_folded = false;
    QString m_foldedLanguage;
    int m_foldedLineCount = 0;
};
```

`SceneCoordinator::applyFoldVisibility` is extended: when a code-block
region is folded, call `tableBlockItem->setFolded(true)` with its
language + line count. When unfolded, revert. (Tables will never be set
folded since there are no table-type fold regions; the method only
fires for items that back a CodeBlock region.)

**Paint details** (fresh paint branch in `TableBlockItem::paint` when
`m_folded == true`):

- Fixed row height = `QFontMetrics(document font).lineSpacing() + 6 px`
  (a few px of vertical padding).
- Background: the same code-block background the unfolded paint uses
  (so the folded row still reads as "code-ish").
- Foreground text: the summary string, in the same monospace font as
  code.
- No border, no syntax colours in the summary.

`TableBlockItem::boundingRect` returns the reduced height when folded.

## Gutter changes

`FoldGutter::paint` today iterates `m_model->headings()` and paints
each arrow at the heading's Y. It switches to iterate
`m_model->regions()`. `SceneCoordinator::headingSceneY(int)` becomes
`regionSceneY(int)` (rename) and the gutter queries Y per region index.

`SceneCoordinator::headingIndexAtSceneY(qreal)` becomes
`regionIndexAtSceneY(qreal)`, still returns an index into `m_regions`.

`FoldArrowColumn::handleClick`:
- For a `Heading` region: existing behaviour (toggle, Ctrl+Click =
  foldAllAtLevel).
- For a `CodeBlock` region: no modifier → toggle this block's fold;
  Ctrl+Click → `foldAllCodeBlocksInSection(region.path[:-1])` OR the
  unfold equivalent if all siblings are already folded (same "toggle
  all" semantics as heading Ctrl+Click).

## Editor public API additions

```cpp
// In include/markoff/Editor.h, under the existing folding block:
QList<QStringList> codeBlockPaths() const;
void foldAllCodeBlocks();
void unfoldAllCodeBlocks();
```

Existing `fold(path)` / `unfold(path)` / `toggleFold(path)` /
`isFolded(path)` / `foldedPaths()` work unchanged — paths are opaque.

Existing `serializeFoldState()` / `restoreFoldState(obj)` unchanged —
the same flat `"folds"` list just carries mixed paths.

Existing `foldStateChanged()` and `foldsAutoExpanded(QList<QStringList>)`
signals unchanged.

## Persistence

JSON schema unchanged from v1: `{"version":1, "folds": [["Intro","Goals"],
["Intro","Goals","code:0"], ...]}`. Paths are type-discriminated by their
last segment's `code:` prefix, but the persistence layer never needs to
discriminate — it's a pure set.

**Forward-compat note:** v1 readers (pre-code-block) loading this JSON
will store the code paths in their folded set but have nothing to match
them against, so they get silently pruned on the next reconcile. No
crash, no data loss for heading folds.

## Auto-unfold

`findText` behaviour: same as v1. When the match's block is inside a
folded region (heading OR code block), call `unfoldAncestors(path)`
where `path` is the enclosing region's full path (the code block's path
if the match is inside a folded code block, or the heading path if
inside a folded heading's body). `foldsAutoExpanded(paths)` is emitted.

`scrollToHeading` behaviour: unchanged. It unfolds heading ancestors
only — there's no analogous "scroll to code block" public entry point,
and wikilink navigation targets are always headings.

## Testing

Two new test binaries plus extensions to existing ones.

### `tst_code_block_folding` (new)

- `region_detection_singleCodeBlock_emitsOneRegion` — loads
  ` ```cpp\nputs("a");\n``` `, asserts one region with correct language
  and lineCount.
- `region_detection_preambleCodeBlock_pathHasNoHeadingPrefix` — code
  block before any heading → path == `["code:0"]`.
- `region_detection_ordinalResetsOnHeading` — `code / code / ## H / code`
  → paths `["code:0"]`, `["code:1"]`, `["H","code:0"]`.
- `region_detection_ordinalResetsAcrossLevels` — `# A / code / ## B /
  code` → paths `["A","code:0"]`, `["A","B","code:0"]`.
- `region_detection_languageExtraction` — ` ```cpp ` and ` ```python `
  and ` ``` ` (empty) all produce the right language string.
- `region_detection_lineCount_excludesFenceLines` — ` ```\nA\nB\n``` `
  has lineCount 2.
- `fold_codeBlock_byPath` — `fold(["Intro","code:0"])` flips state.
- `foldAllCodeBlocks_foldsEveryCodeBlock_leavesHeadingsAlone`.
- `unfoldAllCodeBlocks_mirrors`.
- `foldAllCodeBlocksInSection_foldsOnlyThatSectionsBlocks`.
- `reconcile_renameEnclosingHeading_dropsCodeBlockFold`.
- `reconcile_insertNewCodeBlockBefore_shiftsOrdinals_dropsDownstreamFolds`.
- `reconcile_insertUnrelatedCodeBlockInDifferentSection_preserves`.
- `persistence_mixedPaths_roundTrip` — serialize + restore with a
  heading fold and a code-block fold in the same JSON.

### `tst_code_block_paint` (new)

- `tableBlockItem_setFolded_true_reducesBoundingRect`.
- `tableBlockItem_setFolded_false_restoresBoundingRect`.
- `tableBlockItem_paint_folded_rendersSummaryText` — paint into a
  QImage, find the summary text via pixel sampling or QPainter hook.
- `tableBlockItem_foldedSummary_formatsSingularCorrectly` — 1 line →
  "(1 line)"; 12 → "(12 lines)"; empty language → no language slot.

### Extensions to existing tests

- `tst_folding_integration`: add
  `editor_foldCodeBlock_autoUnfoldOnFindText` — fold a code block,
  `findText(substring inside code)`, assert found + code-block unfolded
  + `foldsAutoExpanded` signal payload contains the code path.
- `tst_fold_gutter`: add
  `click_onCodeBlockArrow_togglesFold` and
  `ctrlClick_onCodeBlockArrow_foldsSiblingCodeBlocksInSection`.
- `tst_folding_model` (existing `TstFoldingModel*`): verify
  `headings()` still returns the same shape after the refactor (no
  breakage of v1 consumers).
- `tst_folding_reconcile`: verify that reconcile now also handles
  code-block regions — rename heading → descendant code-block folds
  drop.

## Non-goals for this sprint

- Indented code blocks (4-space). Deferred.
- Fold-all-by-language ("fold all Python blocks"). Out of scope.
- Hover preview of folded code. Later v2 sprint.
- Animation. Later v2 sprint.
- Incremental CST walk (we still do a fresh parse per `ensureHeadingMap`
  rebuild). Separate sprint.

## Open questions

None blocking. The following will be decided during implementation:

- Whether to extend `TreeSitterParser::DocumentQueryResult` with a
  `codeBlocks` field or add a separate `buildCodeBlockInfo()` method.
  Decide by reading the struct and picking whichever keeps the ownership
  story clearest.
- Exact Y-padding of the folded summary row — tune visually.
- Whether to show the fence's info string (e.g., ` ```cpp {filename.cpp} `)
  with or without post-language attributes. Default: language only,
  discard attributes.
