# Replace for Corbomite — design

**Date:** 2026-06-12
**Status:** Approved (brainstorm). Implementation plan to follow.
**Driving context:** road-to-dogfood Phase 2 placebo-removal — the hamburger
**Find… / Replace…** actions (`MarkdownView.cpp:298–312`) have no `connect()`,
and Replace has never existed as a controller verb. Punch list: `docs/punch-list.md`
P3 "Placebo dialogs" (line 212) and "one snippet per file" (line 207).

**Decision summary (from brainstorm):**
- Scope: design **Replace** (the one item with an architectural fork); fold the
  three mechanical items (Find/Replace menu wiring, Insert Table/Callout,
  multi-snippet search) into the implementation plan with no design.
- Architecture: **option C** — a minimal coordinate-correct mutation primitive
  upstream in `markoff-core`; all UI and replace/replace-all policy stay
  consumer-side in Corbomite. `Markoff::FindController` stays mutation-free.
- Replacement semantics: **literal-only** (MVP). No regex backreferences.

---

## 1. Why upstream (and why minimal)

A `Markoff::FindController::Match` is `{BlockAnchor block; quint32 byteOffset;
quint32 byteLength}` where `byteOffset` is **block-local**. Turning a match into a
document edit — and, for Replace-All, applying N edits while earlier edits shift
the byte offsets of later ones — is coordinate-space logic over Markoff's
no-separator flat buffer. The project already ruled on this class of problem once:
the checkbox-in-Reading item (`docs/punch-list.md:178`) was **re-scoped from
"Corbomite-only" to "upstream markoff-styled"** because doing coordinate→model
mapping consumer-side "reimplements the binding's mapping (breaks over code
blocks/tables; INVARIANTS violation)." Replace is the same shape — so the
mutation belongs in Markoff.

It is kept **minimal** (one mutation primitive + one read-only selection helper)
because the find UI is, by the find-ui-port design
(`docs/superpowers/specs/archive/2026-05-20-find-ui-port-design.md`),
**consumer-owned**: `Corbomite::FindBar` drives the controller; Markoff owns the
document and the matching. Replace keeps that split — Markoff gains only the
document mutation; Corbomite owns the Replace UI and the replace/replace-all
policy.

`FindController` is **not** modified to mutate the document. It stays mutation-free
(Markoff invariant D3: "never touches focus, cursors, or scroll"); the only
document mutation lives on `MarkoffDocument`, where document mutation already
lives (`applyFlatEdit`).

## 2. Upstream additions to `markoff-core`

### 2a. `MarkoffDocument::replaceMatches`

```cpp
/// Replace each match's byte span with `replacement` (literal text), as a
/// single undo transaction. Matches are mapped to global no-separator flat
/// offsets and applied in descending start order so earlier-applied edits
/// never shift the offsets of edits not yet applied. Empty list is a no-op.
void replaceMatches(const QList<SearchHit> &matches, const QString &replacement);
```

The primitive takes `Markoff::SearchHit` (`{BlockId blockId; uint32_t matchStart;
uint32_t matchLen}`, from `SearchEngine.h`) — NOT `FindController::Match`, because
`MarkoffDocument.h` is included *by* `FindController.h` (taking the nested type
would be a circular include). `BlockAnchor` is a `using` alias for `BlockId` and
`SearchHit` is structurally identical to `FindController::Match`, so the Corbomite
call site converts in one line (§3). `SearchEngine.h` forward-declares
`MarkoffDocument`, so `MarkoffDocument.h` including it is non-circular.

Mechanism (the coordinate conversion is why this is upstream — it needs the
no-separator flat layout that only the document knows):
- `matchStart`/`matchLen` are **block-local** byte offsets. Build a base-offset
  map by walking the public `iterateBlocks()` and accumulating `blockText(id).size()`;
  a match's **global** start = `base[blockId] + matchStart`. This is
  `applyFlatEdit`'s no-separator coordinate space. (Do NOT use `blockByteRange` —
  that reports parse-source space *with* separators, a different coordinate system.)
- Skip any match whose `blockId` is absent from the current block set (stale match).
- Sort the resulting global ranges **descending by start**; apply each via
  `applyFlatEdit(globalStart, globalStart + matchLen, replacementUtf8, Origin::UserEdit)`.
- Fold into **one** UndoLog entry: after the first `applyFlatEdit`, call
  `coalesceLastUndo()` following each subsequent `applyFlatEdit`, so a single
  `undoD2()` reverses an entire Replace-All.
- Literal replacement only: the matched span (even a regex match) is replaced
  with `replacement` verbatim. The replacement comes from a single-line
  `QLineEdit`, so it carries no newlines; if one is ever present, `applyFlatEdit`'s
  existing canonicalization governs (well-defined, not a special case here).
- **Deterministic post-state:** `applyFlatEdit` emits `d2DocumentChanged` via a
  `QTimer::singleShot(0)` debounce, so after the edits land the active
  `FindController` has *not* yet recomputed. `replaceMatches` ends with a single
  `flushPendingD2Changed()` so `d2DocumentChanged` fires **synchronously** — the
  controller recomputes before `replaceMatches` returns, and the Corbomite caller
  can immediately call `selectMatchAtOrAfter` on a fresh match list. (One flush
  for the whole batch, not one per edit.)

Invariant note (Markoff `docs/INVARIANTS.md`): this primitive touches the
**edit path**, not the focus/caret/block-change seam, so the seam rules (L4
authority, re-entrance guards, `callLater` smells) do not bind it. INVARIANT 4
(falsifiable, production-callsite-first tests) **does** apply — see §5.

### 2b. `FindController::selectMatchAtOrAfter` (mutation-free)

```cpp
/// Move the current-match selection to the first match whose position is
/// at or after (block, offset), wrapping to index 0 if none. Emits
/// currentMatchChanged. Does NOT touch the document, focus, cursor, or scroll.
void selectMatchAtOrAfter(Markoff::BlockAnchor block, quint32 offset);
```

After a replace, the active controller recomputes its match list on the
`d2DocumentChanged` it already listens for. This helper re-anchors the selection
to the replaced position so "Replace" then advances to the *next* match instead
of resetting to match #1. It moves only `m_currentIndex` — D3-safe.

## 3. Corbomite UI — `FindBar` gains a replace row

`Corbomite::FindBar` (`src/editor/FindBar.{h,cpp}`) grows a second row: a
replacement `QLineEdit` + **Replace** and **Replace All** `QPushButton`s, hidden
in find-only mode and shown via `setReplaceMode(bool)`. Layout follows Kate/Okular
(as the find-ui-port design already modeled). No match-count coloring on the
replacement field (same neutral-feedback rule as find).

Corbomite converts `FindController::Match` → `Markoff::SearchHit` at the call
site (one line, fields line up: `SearchHit{m.block, m.byteOffset, m.byteLength}`).

- **Replace:** capture the current match, then
  `noteDoc->markoff()->replaceMatches({SearchHit{cur.block, cur.byteOffset, cur.byteLength}}, replacementText)`;
  the controller recomputes; call `controller->selectMatchAtOrAfter(cur.block,
  cur.byteOffset + replacementText.toUtf8().size())` to advance **past** the
  just-inserted replacement (otherwise a replacement that itself contains the
  needle — e.g. replacing `foo`→`foobar` while searching `foo` — would re-select
  the text we just wrote).
- **Replace All:** convert `controller->matches()` to `QList<SearchHit>` and call
  `replaceMatches(hits, replacementText)` — one undo step.
- Esc → `closeRequested` (unchanged). Find row behavior unchanged.

`NoteEditorWidget` already owns the `FindBar` and the per-`NoteDocument`
`FindController`; no new ownership wiring is introduced.

## 4. Data flow

```
Ctrl+H / Replace… menu
  → MainWindow shows FindBar in replace mode (setReplaceMode(true))
  → user edits needle  → FindController::setNeedle (existing path)
  → user edits replacement field
  → Replace / Replace All
      → MarkoffDocument::replaceMatches(...)   [one undo transaction]
      → d2DocumentChanged → FindController recompute
      → (Replace only) selectMatchAtOrAfter → advance selection
  → per-leaf adapters re-highlight via the normal recompute path
```

## 5. Testing

Markoff (offscreen; `serializeForSave()` assertions on a headless
`MarkoffDocument`):
- **`tst_replace_matches`** (new): single match; multiple matches in one block;
  matches across multiple blocks; **length-changing** replacement (proves the
  descending-order offset-shift correctness — falsifiable by reversing the sort);
  **Replace-All reversed by exactly one `undoD2()`**; empty list no-op.
- **FindController helper test**: `selectMatchAtOrAfter` lands on the correct
  index and wraps.

Corbomite (offscreen):
- **`tst_findbar`** extended: `setReplaceMode(true)` shows the replace row;
  **Replace** calls the primitive and advances the selection; **Replace All**
  replaces the full count; replace row hidden in find-only mode.
- MainWindow wiring: **Find… / Replace…** actions show the bar in the correct
  mode (the placebo-removal item).

## 6. Cross-repo sequencing (Markoff-first, per `~/dev/CLAUDE.md`)

1. **Markoff:** `replaceMatches` + `selectMatchAtOrAfter` — own Markoff spec in
   `Markoff/docs/specs/` (Markoff requires brainstorming-first for upstream
   specs), TDD per §5, push to master.
2. **Corbomite:** re-pin the `libs/markoff-family` submodule to the commit
   containing the primitive.
3. **Corbomite:** `FindBar` replace row + `MainWindow` menu wiring + tests.

## 7. Mechanical items (planned here, not designed — same implementation plan)

- **Wire Find… / Replace… menu** (`MarkdownView.cpp:298–312`) → `MainWindow`
  shows `FindBar` in find-only / replace mode. (Placebo removal — the literal
  "wire" answer to the wire-or-hide decision.)
- **Insert Table / Insert Callout** (`MainWindow.cpp:543–567`, currently
  exec-then-discard) → insert markdown at the caret via `applyFlatEdit`, reusing
  the template-at-cursor `LineResolve` bridge.
- **Multi-snippet search** (`SQLiteIndex.cpp:461`) → emit all snippets per file;
  `SearchResultsModel` already groups by file, so it renders multiple child rows
  with no model change.

## 8. Out of scope (YAGNI)

- Regex backreferences (`$1`/`\1`) in the replacement. The `replaceMatches`
  list-based signature leaves room to add per-match replacement strings later
  without reworking the primitive's call shape.
- Find/replace history, flag-toggle persistence, replacement preview.
