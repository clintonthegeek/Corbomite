# Markoff TODO

Polish items and known issues to address. Newer items at the top of each
section. Items marked **(blocked: spec)** need a design decision before
implementation; the rest are implementation tasks.

## Big-ticket features (need spec or brainstorm first)

- [ ] **Editable tables** — current `TableBlockItem` is read-only. Two known
  paths: (a) keep custom paint, add cell-edit overlay; (b) pivot to
  `QTextTable` in a hybrid layout. See `TODO-tables.md` for the historical
  attempt and its failure modes. **(blocked: spec)**
- [ ] **In-editor image rendering** — `ResourceProvider` is wired to the
  reading view but not the editor. Needs a new `ImageBlockItem` graphics
  item plus `MarkdownSplitter` segment type, paralleling the math
  `TableBlockItem` pattern. **(blocked: spec)**
- [ ] **Math cursor reveal polish** — inline math cursor reveal is
  implemented but the mechanism is complex (~300 lines with reentrancy
  guards). Consider simplifying — e.g., per-item reveal instead of
  per-glyph reveal.
- [ ] **Obsidian-flavored grammar additions** in vendored tree-sitter:
  - `![[embed]]` (embed prefix on wikilinks)
  - `^block-id` block reference
  - `> [!type]` callout block (currently only colored as text)
  Requires forking the vendored grammar.

## Performance

- [ ] Incremental tree-sitter parsing — use `ts_tree_edit()` to update
  the old tree instead of full reparse on every keystroke. Tree-sitter
  is designed for this; only the changed region gets re-parsed.
- [ ] Incremental rehighlight — only rehighlight blocks whose spans
  actually changed, not the entire document. Compare old and new span
  maps to find dirty blocks.
- [ ] Reduce spurious `textChanged` signals — `rehighlight()` modifies
  document formatting which fires `contentsChanged` → `textChanged`
  (currently suppressed by `inReparse` guard but still 3 wasted signal
  emissions per keystroke).
- [ ] Consider `QSyntaxHighlighter::rehighlightBlock()` for targeted
  updates instead of full `rehighlight()` after reparse.

## Live Preview Polish

- [ ] Heading hash prefix visibility: when cursor is on a heading line
  but NOT adjacent to the hashes (e.g., at end of heading text), should
  the hashes be visible? Currently they hide. Obsidian shows them for
  the entire line. May want heading prefix to be line-level, not
  element-level.
- [ ] Footnote superscript rendering — `QTextCharFormat::AlignSuperScript`
  may not render correctly in the editor's graphics-item paint path. May
  need a custom paint pass like the math substitution.

## Style / Theme API

- [ ] Callout colors are now centralized in `Renderer.cpp` (`kCalloutColors`
  table) but still hardcoded — they could move to a `Theme` keyed by
  callout-type string for full theme control. Lower priority.
- [ ] KDE color scheme integration (Breeze Dark, etc.) — load via
  `Theme::fromSchemeFile()` extended to read KDE color schemes alongside
  the existing QOwnNotes INI format.

## Rendering polish

- [ ] Horizontal rules as actual graphical lines (not just styled `---`
  text). Either custom paint in `MarkdownTextItem` or a dedicated
  `HorizontalRuleItem` block in the splitter.
- [ ] Task list checkboxes as graphical widgets that toggle on click.
  Currently rendered as the unicode `☐` / `☑` symbols.
- [ ] Blockquote left border (visual indicator beyond just indent + gray
  text). Custom paint in `MarkdownTextItem`.
- [ ] List bullet rendering — styled bullet character instead of raw `-`.

## Parser / Grammar

## Editor API gaps

- [ ] Cross-item find/replace wraparound now works for both single-item
  and multi-item documents, but doesn't surface "wrapped" feedback to the
  caller. UI can't show "End of file reached, search wrapped".
- [ ] `Editor::wrapSelection` toggle behavior handles two cases (selection
  IS the wrapped form, and selection is INSIDE outer delimiters), but
  doesn't yet handle "selection has the delimiters at the start/end with
  trailing/leading content" or partial-overlap edge cases.

## Recently fixed (for context)

- Code-block folding (v2 sprint 1): fenced code blocks fold to
  `` ```lang (N lines) `` summary rows. Path encoding
  `["Section","code:N"]` reuses heading-path identity; Ctrl+Click
  on a code arrow folds sibling code blocks in the same section;
  auto-unfold on find match when the match's item is a folded code
  block. FoldingModel refactored to a unified `FoldableRegion` list
  (heading + code block) — lists and block quotes slot in without
  further restructuring. Plan:
  `docs/plans/2026-04-15-code-block-folding.md`. Spec:
  `docs/specs/2026-04-15-code-block-folding-design.md`.
- Heading folding (v1): `Editor::fold`, `unfold`, `toggleFold`,
  `toggleFoldAtCursor`, `foldAll`, `unfoldAll`, `foldAllAtLevel`,
  `foldLevel` and persistence hooks (`serializeFoldState` /
  `restoreFoldState`). Left gutter with triangle arrows; Ctrl+Click
  folds all at level. Auto-unfold on `scrollToHeading` and `findText`,
  emitting `foldsAutoExpanded(paths)`. State keyed by heading hierarchy
  path; reconciled per reparse so renames drop folds. Plan:
  `docs/plans/2026-04-15-heading-folding.md`. Spec:
  `docs/specs/2026-04-15-heading-folding-design.md`. Host integration
  options: `docs/2026-04-15-heading-folding-host-integration.md`.
- `Editor::wrapSelection` now toggles off when the selection is already
  wrapped, OR when the selection is inside outer delimiters. So
  `toggleBold` on `**foo**` (or on `foo` inside it) produces `foo`.
- `Editor::findText` now wraps within a single text item. Previously it
  only wrapped across multiple items, so `findText("foo")` after
  `selectAll()` returned false on a single-item document.
- `Editor::setResourceProvider` now forwards to `SceneCoordinator` (was
  a write-to-nowhere field). Stored for future image-renderer consumers.
- `Renderer::setTheme()` added; reading-view CSS now derives blockquote
  border, code-block background, footnote text, horizontal rule color,
  and highlight background from the theme (with hardcoded fallbacks).
- `MarkdownHighlighter` code-block content now uses `Theme::codeFont`
  families instead of hardcoded `"JetBrains Mono", "Fira Code"`.
- Renderer's 13 inline callout colors moved to a `kCalloutColors` table
  (not theme-driven yet, but centralized).
- `tree-sitter-markdown` math node disambiguation: inline `$x^2$` and
  display `$$x^2$$` were both reported with `mathDisplay=true`. Fixed in
  `TreeSitterParser.cpp` by inspecting the source bytes for `$$`.
- Inline math via `QTextObjectInterface` — see `MathTextObject.h`.
- `Editor::scrollToHeading` was treating `sourceOffset` (UTF-8 byte
  offset) as a line number. Now converts via newline counting in the
  utf8 source.
- `Editor::setFontSize` and `setTheme` produced inconsistent state
  (`setTheme` updated colors but not the document default font, so body
  text rendered at the old size). Both paths now mutually consistent.
- `MathRenderer` font size is now configurable; defaults derived from
  `MathRenderer::DefaultInlineFontSize` instead of hardcoded constants.
- `ResourceProvider` wired through to `Renderer` for image-path
  resolution in the reading view (was a no-op pointer).
- Cleanup: stale `StubBlockItem.h` and `<cmath>` includes removed.
