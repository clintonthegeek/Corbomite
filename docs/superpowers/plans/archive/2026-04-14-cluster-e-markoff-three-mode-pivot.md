# Cluster E — Three-mode pivot (Source / Live-Preview / Reading)

> **Living-status note:** This file is the *plan*. Live status (Not started / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file. Edit this file only when the plan itself changes (work breakdown, target classes, references).

**Plan written:** 2026-04-14. **Rewritten:** 2026-04-15 after reconciling with the current code (parser-split landed, Markoff is now live-preview-exclusive, old `ReadingView` deleted) and after evaluating `~/dev/Penelope/` as a reading-mode candidate. Derived from `docs/obsidian-audit/GAP-ANALYSIS.md` §Cluster E.

**Covers:** P1.3 (three-mode encoding for `workspace.json` round-trip), P1.4 (visual-line float scroll persistence), P2.5 (three-mode live-preview semantics — Markoff already does "cursor in block reveals markdown source"), P2.6 (progressive section rendering + recycling pool, now scoped to the new Reading widget, not Markoff).

## Reality reconciliation (read first)

The original 2026-04-14 plan assumed the three modes would be three *states of one Markoff editor*. That is wrong for the current codebase:

- **Markoff is the Live-Preview widget, exclusively.** The `enum class Mode { Source, LivePreview }` was deleted during the markoff-parser split (commit `82249be`, 2026-04-13). Markoff is a QGraphicsView-based editor that renders markdown with inline formatting and cursor-in-block reveal-source semantics — that *is* live-preview and it already works. Markoff's `CLAUDE.md` explicitly states the library is encapsulated around this one job.
- **No Source-mode widget exists.** Corbomite has no plain-text-with-markdown-syntax-highlight editor. `NoteEditorWidget::ViewMode` today is `{Editing, Reading}` — two values — and "Editing" means Markoff.
- **No Reading-mode widget exists.** The old `Markoff::ReadingView` (QTextBrowser-based, scroll-synced with the editor) was deleted in the same parser-split commit. What currently masquerades as "Reading" in `NoteEditorWidget` is just `markoffEditor->setReadOnly(true)` — the same live-preview scene, non-editable. That is **not** what Obsidian's reading mode is.
- **Penelope (`~/dev/Penelope/`) was evaluated as a reading-mode candidate and rejected as a base.** Penelope is a well-engineered paginated, PDF-viewer-inspired markdown reader (~30 KLOC, MD4C + HarfBuzz + ICU + Poppler), but its architecture is incompatible with Corbomite's targets: pages always-in-memory (no virtualization), synchronous parse/layout (no frame budget, no worker), MD4C parser (we have tree-sitter), no section identity model (no recycling pool), no heading fold, no frontmatter handling (MD4C skips it), no pluggable ResourceProvider. Decision: **build Reading mode greenfield as a sibling library, with selective source transplant from Penelope where the fit is exact.** See §"Penelope transplant manifest" below.

The three modes become three **different widgets** at the Corbomite layer, coordinated by `NoteEditorWidget` as a stacked container:

| Mode | Widget | Status |
|---|---|---|
| **Source** | New Corbomite plain-text markdown editor (to be designed — KTextEditor::View vs. QPlainTextEdit + KSyntaxHighlighting vs. stripped-Markoff variant) | Needs Phase 0 brainstorm |
| **Live Preview** | `Markoff::Editor` as-is | Done (existing code) |
| **Reading** | New `libs/readingview/` — greenfield, tree-sitter-AST-driven, section-recycling + async + virtualized + frame-budgeted from day 1 | Needs building |

## Goal

Make Corbomite a **behaviourally faithful three-mode Obsidian-compatible editor** at the contract level:

1. Three user-visible modes — Source, Live-Preview, Reading — each a distinct widget in `NoteEditorWidget`, with save-state/swap/restore semantics on transition.
2. Persist mode + scroll + cursor + fold-state to `.obsidian/workspace.json` in the exact compound `{mode: "source"|"preview", source?: true|false}` shape Obsidian expects. Live-Preview = `{mode:"source", source:false}` on the wire.
3. Persist scroll position as **visual-line float** (`42.73`, not pixel offset) so it survives reflow, zoom, and width changes — required for `workspace.json` round-trip.
4. Build a new `libs/readingview/` that meets Obsidian's documented reading-pipeline contracts: HTML-string-equality section recycling, 10240-byte async-parse threshold, 5ms / 10-section per-frame budget, virtual-scroll with a selection-window margin, frontmatter-diff re-render trigger, per-section heading-fold.

All four goals are entangled — mode persistence is meaningless without scroll persistence, and Reading mode is meaningless without progressive rendering for large notes. Hence one cluster.

## Audit references

- **Three-mode is two dimensions:** `domains/editor-markdown.md §8 invariant 2` — `{mode: "source"|"preview"}` × `{source: true|false}` (when `mode: "source"`). Live-preview = source-mode + `source: false`. Reading-mode = `mode: "preview"`. **Critical compat invariant.**
- **Mode transition mechanics:** `domains/editor-markdown.md §1` — `setMode` `await`s `save()` before leaving source, captures fold info, restores scroll via ephemeral state, applies the new mode, restores fold + scroll. *In our architecture this happens at the `NoteEditorWidget` layer, not inside Markoff.*
- **Visual-line float scroll:** `domains/editor-markdown.md §1` and §3 — scroll position is `42.73` (not pixels), survives reflow + zoom + width changes.
- **Preview pipeline:** `domains/editor-markdown.md §1` (preview pipeline section) — HTML-string-equality recycle key, **10240-byte async-parse threshold**, **5ms / 10-section frame budget**, virtual-scroll with selection-window extension, frontmatter-diff forcing `usesFrontMatter=true` section rerender, per-section HTML `{from, to}` line mapping. *All four of these land in `libs/readingview/`.*
- **Section recycling pool:** Pass 1's `01-markoff-gaps.md` Editor section (recycle pool with HTML-string-equality key). The "Markoff gaps" framing is now stale — the gap is in the missing ReadingView, not in Markoff.
- **Cursor-in-block reveals source:** `domains/editor-markdown.md §1`. *Markoff already does this — no work needed.*
- **Ephemeral state:** `domains/workspace.md §3` (workspace.json schema includes `eState` per leaf) — scroll, cursor, mode, fold-state are stored as ephemeral and restored on layout-restore.
- **Heading-fold per section:** `01-markoff-gaps.md` — `headingCollapsed` tracked per section in `MarkdownPreviewSection`.

## Target classes

| Class | File | Notes |
|---|---|---|
| `Corbomite::ViewModeSerializer` | `src/editor/ViewModeSerializer.{h,cpp}` | Translate `NoteEditorWidget::ViewMode {Source, LivePreview, Reading}` ↔ Obsidian's `{mode, source}` compound for `workspace.json` round-trip. Pure functions, fully unit-tested. |
| `Corbomite::EphemeralState` | `libs/storage/include/corbomite/storage/EphemeralState.h` + src | `{scroll: float (visual-line), cursor: {line,col}, mode: ViewMode, foldedHeadings: vector<int>}` per leaf. JSON round-trip. Integrates with `WorkspaceState`. |
| `NoteEditorWidget` (extend) | `src/editor/NoteEditorWidget.{h,cpp}` | (a) grow `ViewMode` enum 2→3 values (b) host QStackedWidget of Source/LivePreview/Reading widgets (c) save-state/swap/restore on `setViewMode()` (d) wire ephemeral-state persistence |
| `Corbomite::SourceEditor` | `src/editor/SourceEditor.{h,cpp}` | Thin app-facing shim around the vendored `libs/qutepart-corbomite/` widget (forked from qutepart-cpp). See [qutepart-corbomite-fork plan Phase 2](2026-04-15-qutepart-corbomite-fork.md) for the shim API + visual-line-float scroll adapter. |
| `libs/qutepart-corbomite/` (whole library) | `libs/qutepart-corbomite/` | Vendored + forked from `~/src/qutepart-cpp/` at commit `eec2e9a`. Multi-phase shaping into our perfect Source-mode widget (KSyntaxHighlighting replaces bundled Kate-XML, markdown-specific fold/wiki-link/tag recognition, trimmed indent engines). Phase 1 vendors; later phases shape. |
| `Markoff::ScrollPosition` | `libs/markoff/include/markoff/ScrollPosition.h` | `float visualLine` accessor + setter; converts to/from pixel offset given current viewport. Minimal addition to Markoff's public API. |
| `Corbomite::ReadingView` | `libs/readingview/include/corbomite/readingview/ReadingView.h` | Top-level reading-mode widget. QGraphicsView-backed. Consumes a `Markoff::Document` (tree-sitter AST from `libs/markoff-parser`). |
| `Corbomite::ReadingPipeline` | `libs/readingview/src/ReadingPipeline.{h,cpp}` | Parse → section-split → layout → render driver. Implements the 10240-byte async threshold, 5ms/10-section frame budget. |
| `Corbomite::ReadingSection` | `libs/readingview/src/ReadingSection.{h,cpp}` | `{from, to, html, renderedHash, headingCollapsed, level, usesFrontMatter}`. HTML-string equality drives recycle eligibility. Owns one `QGraphicsItem` when mounted. |
| `Corbomite::SectionRecyclePool` | `libs/readingview/src/SectionRecyclePool.{h,cpp}` | Pool of unmounted rendered sections keyed by HTML hash. O(1) lookup on mount. |
| `Corbomite::VirtualScrollController` | `libs/readingview/src/VirtualScrollController.{h,cpp}` | Selection-window mounting (viewport ± 1× viewport margin). Mount/unmount driven by `viewportEvent` / scroll signals. |
| `Corbomite::SectionLayout` | `libs/readingview/src/SectionLayout.{h,cpp}` | Laying out a single section — headings, paragraphs, code blocks, math, embeds. Delegates code blocks to `CodeBlockHighlighter` (Penelope transplant). |
| `Corbomite::ReadingParseWorker` | `libs/readingview/src/ReadingParseWorker.{h,cpp}` | QThread + signals for ≥10240-byte async parse. Wraps `libs/markoff-parser` off the main thread. |

`NoteEditorWidget` integrates `EphemeralState` and `ViewModeSerializer`. `Markoff::Editor` gains only `ScrollPosition`; no other Markoff changes. The bulk of new code is `libs/readingview/`.

## Library location + conventions

- `libs/readingview/` — sibling of `libs/markoff/`, `libs/markoff-parser/`, `libs/storage/`, etc.
- Target name: `Corbomite::ReadingView`.
- Namespace: `Corbomite::ReadingView::` *or* plain `Corbomite::` for top-level types. Pick during Phase 0; match Markoff's convention.
- Public headers at `libs/readingview/include/corbomite/readingview/`; implementation at `libs/readingview/src/`.
- Depends on: Qt6 (Core, Gui, Widgets), KF6 (SyntaxHighlighting for code blocks, possibly KTextEditor if the Source-mode widget uses it), `MarkoffParser::MarkoffParser` (tree-sitter AST), `JKQTMathText` (math), `mmdr` (mermaid), `Corbomite::Core` (NoteMeta / NoteDocument types).
- **Does NOT depend on Markoff.** ReadingView and Markoff are peer widgets; sharing should happen through common primitives in `libs/markoff-parser` or `libs/core`, not through one depending on the other.
- Own `CLAUDE.md`, own `docs/`, own `tests/`. Follow Markoff's encapsulation pattern.

## Penelope transplant manifest

Explicit list of what we pull from `~/dev/Penelope/` and what we don't. GPL-compat is verified (GPLv3 both sides).

| Penelope source | Target | Adaptation | Status |
|---|---|---|---|
| `src/markdown/codeblockhighlighter.{h,cpp}` | `libs/readingview/src/CodeBlockHighlighter.{h,cpp}` | Minimal — swap KSyntaxHighlighting theme source from Penelope's ThemeManager to a simple theme enum. | **Adopt wholesale** |
| `src/style/paragraphstyle.{h,cpp}` + `characterstyle.{h,cpp}` | `libs/readingview/src/styling/ParagraphStyle.{h,cpp}` + `CharacterStyle.{h,cpp}` | Strip Penelope's print/PDF-specific fields (page templates, footnote layout, justification). Keep font/color/spacing. | **Adopt with adaptation** |
| `src/style/stylemanager.{h,cpp}` | `libs/readingview/src/styling/StyleManager.{h,cpp}` | Keep the lookup/resolution pattern. Drop Penelope's master-page + footnote-style coupling. | **Adopt with adaptation** |
| `src/app/metadatastore.{h,cpp}` | Reference only — our equivalent is `EphemeralState` + `WorkspaceState` | Adopt the pattern (path-hash keying, per-file JSON in XDG); DO NOT adopt the format (we need Obsidian `workspace.json` shape). | **Reference pattern** |
| `src/render/boxtreerenderer.{h,cpp}` | Reference only | The QPainter/PDF backend split is a good reference for when we want PDF export later. For now we only need QPainter. | **Reference pattern** |
| `src/canvas/documentview.{h,cpp}` text-selection + source-map copy logic | `libs/readingview/src/SelectionCopy.{h,cpp}` (name TBD) | The `plain / Markdown / RTF` copy-flavor logic with source-map preservation. Extract the algorithm, not the class. | **Reference pattern + selective transplant** |
| `src/typography/*` (hyphenation, justification, short-word handling) | Not adopted day 1 | Post-MVP. Obsidian reading mode uses browser-default typography; matching that is enough for parity. | **Deferred** |
| `src/layout/layoutengine.{h,cpp}` | **Not adopted** | Page-based, synchronous, monolithic — architecturally wrong for recycling + virtualization. | **Discard** |
| `src/markdown/documentbuilder.{h,cpp}` + MD4C | **Not adopted** | We use `libs/markoff-parser` (tree-sitter) which gives us YAML frontmatter, heading hierarchy, wiki-links natively. | **Discard** |
| `src/model/contentmodel.h` (Content::Document AST) | **Not adopted** | Parallel to our tree-sitter AST. Maintaining two is waste. | **Discard** |
| `src/pdf/*`, `src/print/*`, `src/export/*` | **Not adopted** | Out of scope for reading mode. PDF export is a separate future feature. | **Discard** |
| `src/canvas/rendercache.{h,cpp}` (Poppler LRU) | **Not adopted** | PDF-specific. | **Discard** |

Total transplant footprint: ~3-5 KLOC out of Penelope's 30.4 KLOC. Every transplanted file gets a header comment citing the Penelope commit hash it was derived from, so diffs stay traceable.

## KDE / GPL3-compatible prior art

**Local KDE source convention:** source tree at `~/src/kde/src/<repo>`. **Always grep there first; never clone from `invent.kde.org` unless a repo is genuinely missing locally.** Verified-present locally: `kate`, `kdevelop`, `kio`, `kconfig`, `kconfigwidgets`, `kparts`, `kxmlgui`, `kwidgetsaddons`, `ktexteditor`, `krunner`, `baloo`, `okular`, `poppler`, `qtkeychain`, `sonnet`.

| Target | Local path | What we're looking for |
|---|---|---|
| Source-mode editor base (if KTextEditor route chosen) | `~/src/kde/src/ktexteditor/` (entry: `kateview.cpp`, `kateview.h`) | Public API, fold model, scroll position serialization, syntax-highlighter wiring |
| Visual-line scroll prior art | `~/src/kde/src/ktexteditor/src/view/` (search for `restoreScrollPosition`, `savedPos`) | Concrete prior art for floating-point scroll persistence |
| Folding / collapsing | `~/src/kde/src/ktexteditor/src/buffer/` (folding ranges) | Heading-fold persistence and visual representation |
| Virtual-scroll / recycling | Qt6 native `QAbstractItemView`'s viewport recycling (reference in Qt6 docs), `QQuickItemView` source (for QML) — not directly in KDE tree. | Section-recycling pool architecture |
| Async section parse | `~/src/kde/src/ktexteditor/` (async highlighting pass), `~/src/kde/src/baloo/src/file/extractorprocess.cpp` (async file processing) | Threading model for off-main-thread parse |
| QPainter-to-QGraphicsItem rendering patterns | `~/src/kde/src/okular/` (page rendering pipeline) | Okular's mount/unmount per visible page is the closest prior art for our section virtualization |

## Work breakdown

### Phase 0 — Source-mode widget decision + bootstrap `libs/readingview/`

**Phase 0a — Source-mode widget choice: DECIDED 2026-04-15.**
- Candidates evaluated: (i) `KTextEditor::View` (heavy, ~20 MB KDE/KIO/QML transitive deps), (ii) `QPlainTextEdit + KSyntaxHighlighting` (lightest but most DIY), (iii) stripped Markoff variant (couples to Markoff internals), (iv) `QScintilla` via `brCreate` fork, (v) `qutepart-cpp` by diegoiast.
- **Decision: vendor + fork `qutepart-cpp`** (MIT, QPlainTextEdit-based, active upstream) into `libs/qutepart-corbomite/`, then shape over 8 phases into our permanent Source-mode widget. See [`docs/superpowers/specs/2026-04-15-qutepart-corbomite-fork-design.md`](../specs/2026-04-15-qutepart-corbomite-fork-design.md) for rationale and [`docs/superpowers/plans/2026-04-15-qutepart-corbomite-fork.md`](2026-04-15-qutepart-corbomite-fork.md) for the phased shaping plan. `QScintilla` rejected (qmake-only build, parallel-widget-world to Qt, no KSyntaxHighlighting integration path). `KTextEditor` rejected for Source-mode specifically (KateVi and plugin-parity surface are nice-to-haves that don't justify the 20 MB transitive-dep cost for a plain-text widget; revisit for Cluster N if plugin parity demands it).
- **Phase 0a execution** = **Phase 1 of the qutepart-corbomite-fork plan** — vendor the upstream source, wire CMake, smoke test. That's a self-contained deliverable; executing it completes Phase 0a here.
- **The `Corbomite::SourceEditor` app-facing shim** (the thing Cluster E Phase 1 will actually manipulate) = **Phase 2 of the qutepart-corbomite-fork plan** — visual-line float scroll, public find/replace, cursor/fold persistence API. Runs before Cluster E Phase 1 starts.

**Phase 0b — Bootstrap `libs/readingview/`.**
- Empty CMakeLists wired into the top-level build.
- `CLAUDE.md` matching Markoff's encapsulation pattern.
- `docs/` subdir with a plan index.
- Target links Qt6, KF6::SyntaxHighlighting, MarkoffParser, JKQTMathText, mmdr, Corbomite::Core.
- Transplant `CodeBlockHighlighter` from Penelope as the first concrete file — validates the build wiring end-to-end.
- One test executable (`tst_readingview_bootstrap`) that instantiates an empty ReadingView and asserts construction.

**Phase 0 completion criteria:** Source-mode widget decision doc merged; `libs/readingview/` builds empty (modulo `CodeBlockHighlighter`); tests pass.

### Phase 1 — ViewMode encoding + EphemeralState scaffold

1. `NoteEditorWidget::ViewMode` grows `{Editing, Reading}` → `{Source, LivePreview, Reading}`. `Editing` callers are migrated to `LivePreview` (likely just a rename — `Editing` was a Corbomite-layer label for what is internally live-preview).
2. Define `Corbomite::EphemeralState` struct in `libs/storage/`. JSON serialiser per `domains/workspace.md §3`. Round-trip test against an Obsidian-written `workspace.json` fixture (Cluster B left fixtures behind; confirm location).
3. Define `Corbomite::ViewModeSerializer` with `toCompound(ViewMode) → {QString mode; bool source}` and `fromCompound({mode, source}) → ViewMode`. Unit tests for every combination including the `{mode:"source", source:true}` = Source case, `{mode:"source", source:false}` = LivePreview case, `{mode:"preview"}` = Reading case, and absent/null `source` defaulting.
4. Wire `NoteEditorWidget::saveEphemeralState` / `restoreEphemeralState` through both. Still no visible behaviour change — only persistence shape.

### Phase 2 — Visual-line float scroll

5. Add `Markoff::ScrollPosition { float visualLine; }` to `Markoff::Editor` public API. Getter: convert pixel scroll offset to visual-line float using rendered-line bounds (binary search across `QGraphicsItem` Y-ranges). Setter: inverse.
6. Source-mode widget exposes the same API in shape. If it's `KTextEditor::View`-based we likely wrap KateView's saved-position mechanism; if `QPlainTextEdit`-based we hand-roll from `QTextBlock` geometry.
7. `ReadingView` exposes the same API. For Reading mode visual-line is a section-relative line count since we virtualize — compute from mounted-section geometry + the pre-layout line offsets of unmounted sections above.
8. Persist via `EphemeralState`. Round-trip test: serialise → reflow (change width) → deserialise → assert visual-stability ±0.5 line. Run against all three widgets.

### Phase 3 — ReadingView MVP (no virtualization, no async, no recycling yet)

9. `ReadingPipeline` parses a markdown string via `libs/markoff-parser`, walks the AST, splits into `ReadingSection`s at heading boundaries (see "Section boundary rules" below).
10. `SectionLayout` renders one section to a `QGraphicsItem` subtree — paragraphs (styled via transplanted `StyleManager`), headings (with level), code blocks (via `CodeBlockHighlighter`), lists, tables, blockquotes, horizontal rules, inline images, wiki-links (resolved via a passed-in `VaultResourceProvider`).
11. Math — invoke `JKQTMathText` on `$...$` / `$$...$$`, place rendered result as a `QGraphicsPixmapItem`.
12. Mermaid — invoke `mmdr` on ` ```mermaid ` fenced blocks, place rendered SVG as a `QGraphicsSvgItem`.
13. ReadingView mounts all sections upfront, tiled vertically, renders on open. No recycling, no virtualization, no async — synchronous first pass. Acceptable for notes ≲ 1 k lines.
14. Wire ReadingView into `NoteEditorWidget` as the Reading-mode widget. `setReadOnly(true)` on Markoff stops being our Reading implementation.

**Phase 3 completion criteria:** a 500-line note opens in Reading mode with correct formatting, including headings, code highlighting, math, mermaid, images, wiki-links. Tests: unit + snapshot (QGraphicsScene geometry dump) for a fixture vault.

### Phase 4 — Section recycling pool + frontmatter-diff trigger

15. `ReadingSection` gains `renderedHash` = SHA-256 of the rendered HTML-equivalent representation. We don't render to HTML; we render to `QGraphicsItem` trees. The "HTML" equivalence from the Obsidian audit translates here to a deterministic string serialization of the section's rendered content (text runs + styles + embedded object types). Call it `renderedShape`; doc the equivalence.
16. `SectionRecyclePool` keyed by `(sectionType, renderedShape-hash)`. On re-parse, diff sections by source-range; unchanged `renderedShape` ⇒ reuse QGraphicsItem subtree unchanged.
17. Frontmatter-diff trigger: AST walker marks sections that reference frontmatter (embeds that pull from `{{title}}`, properties rendered inline, etc.) as `usesFrontMatter=true`. Any frontmatter change forces re-render of all such sections regardless of source-range identity.
18. Tests: edit one paragraph, assert only its section re-rendered; edit frontmatter, assert all `usesFrontMatter=true` sections re-rendered and no others.

### Phase 5 — Async parse + frame budget

19. `ReadingParseWorker` — QThread + `QFutureWatcher`. Notes ≥ **10240 bytes** go to the worker; < 10240 bytes parse sync on the main thread. Honour the constant exactly.
20. `ReadingPipeline` consumes a `5 ms / 10 sections` per-frame budget for the render phase. Implementation: yield via `QTimer::singleShot(0, …)` when `QElapsedTimer::elapsed() ≥ 5ms` OR `sectionsThisFrame ≥ 10`. Resume on next frame.
21. Benchmark: 10k-line note paints progressively, no main-thread frame over 16 ms during scrolling. Measure with a frame-timing test (`QTest::qWait` + `qApp->processEvents` loop + timestamping).

### Phase 6 — VirtualScroll + heading-fold

22. `VirtualScrollController` — mounts sections within `viewport ± 1× viewport height`. Sections outside the window unmount their `QGraphicsItem` subtrees (dropped into the recycle pool). On scroll, mount newly-visible sections from the pool if their hash matches, else build fresh.
23. Benchmark: 100k-line note opens in < 500 ms, ≤ 50 sections mounted at any time, scroll responsive (no frame > 16 ms).
24. Heading-fold — `headingCollapsed: bool` + `level: int` per section. Folding a level-N heading unmounts + hides all subsequent sections until the next heading at level ≤ N. Fold state persisted via `EphemeralState.foldedHeadings`. UI: click on a heading-fold gutter arrow.

### Phase 7 — `NoteEditorWidget` stacked-widget + mode transition

25. `NoteEditorWidget` becomes a `QStackedWidget` host of the three widgets (all constructed lazily, cached after first use).
26. `setViewMode(ViewMode new)` implements the audit-spec'd transition:
    1. `await save()` on the outgoing widget (for Source/LivePreview — content must be flushed before swap).
    2. Capture outgoing widget's ephemeral state (scroll, cursor, fold).
    3. Swap stacked-widget index.
    4. Load content into incoming widget if not already loaded (Source and LivePreview share text; Reading loads from the same NoteDocument).
    5. Restore scroll + fold from `EphemeralState`. Cursor restore only applies to Source/LivePreview.
27. End-to-end test: Source → edit → LivePreview (edit reflected, cursor preserved) → Reading (same content, scroll preserved as visual-line float) → back to Source (cursor and scroll preserved).
28. `workspace.json` round-trip integration test: open Corbomite → switch modes, scroll, fold → close → reopen → state matches byte-for-byte at the `{mode, source, eState}` level.

## Section boundary rules

Placed here because they're a cross-phase contract.

A section is a contiguous run of sibling nodes in the AST bounded by:

- A heading (the heading is the first node of its section; its section extends until the next heading at the same or shallower level).
- Document start / end.
- The frontmatter block is its own section (`usesFrontMatter = false` for itself; it's the *source* of frontmatter for others).

A section boundary is a nice stable unit for recycling because:
- Heading edits and paragraph-inside-a-section edits affect different sections.
- Frontmatter-referring sections are fully self-contained (no cross-section frontmatter reads).
- Fold operates natively on sections (collapse-a-heading = hide-all-sections-after-me-at-deeper-level).

## Explore-agent dispatch prompts

**Prompt 1 — KateView visual-line scroll prior art:**
> Read KateView scroll-position handling at `~/src/kde/src/ktexteditor/src/view/kateview.cpp` and adjacent files. Do NOT clone from upstream — local source is current. Identify: (a) how Kate represents scroll position when persisting view state across reload, (b) whether Kate uses visual-line, document-line, or pixel offset, (c) the mechanics of restoring scroll across word-wrap/zoom changes. Report a translation plan for `Markoff::ScrollPosition` and the Source-mode widget. Under 600 words.

**Prompt 2 — Okular page mount/unmount pattern as virtualization prior art:**
> Read Okular's page rendering and viewport management at `~/src/kde/src/okular/` — focus on how it decides which pages are instantiated vs. in-memory-but-not-mounted. Identify: (a) the mount/unmount lifecycle, (b) the recycling/caching model (if any), (c) how it handles scroll-driven mount predictions. Report whether the pattern transfers to a Markdown-section virtualization model. Under 700 words.

**Prompt 3 — QGraphicsView virtual-scroll feasibility:**
> Investigate Qt6 QGraphicsView's native facilities for viewport-only-mount-of-items. Look at `QGraphicsScene::itemsBoundingRect`, `QGraphicsView::cacheMode`, `QGraphicsScene::setItemIndexMethod`. Cite Qt6 documentation behaviour. Compare against a hand-rolled mount/unmount pool driven by `viewportEvent`. Recommend approach for `VirtualScrollController`. Under 500 words.

**Prompt 4 — Penelope transplant surface (re-verify):**
> Read Penelope's `src/markdown/codeblockhighlighter.{h,cpp}`, `src/style/paragraphstyle.{h,cpp}`, `src/style/characterstyle.{h,cpp}`, `src/style/stylemanager.{h,cpp}` at `~/dev/Penelope/`. Identify: (a) exact file dependencies (do they pull in pagelayout / masterpage / footnotestyle which we want to avoid?), (b) KSyntaxHighlighting integration minutiae, (c) Penelope commit hashes at HEAD so we can record provenance in the transplant headers. Report under 500 words.

**Prompt 5 — Source-mode widget candidates comparison:**
> For a Corbomite "Source mode" plain-text markdown editor (raw markdown, syntax-highlighted, read/write, with fold + scroll persistence), compare three candidates: (a) `KTextEditor::View` from KF6 at `~/src/kde/src/ktexteditor/`, (b) `QPlainTextEdit` + `KF6::SyntaxHighlighting`, (c) a stripped-down Markoff editor with live-preview decorations disabled. Evaluate along: API surface, dep weight, fold support out-of-box, syntax-highlight integration, visual-line-scroll plumbing, undo stack, find/replace, memory cost. Produce a decision matrix. Under 700 words.

## Definition of done

- `NoteEditorWidget::ViewMode` is `{Source, LivePreview, Reading}`; all three modes open with correct content.
- `EphemeralState` + `ViewModeSerializer` round-trip an Obsidian-written `workspace.json` byte-stably at the `{mode, source, eState}` field level.
- Scroll is visual-line float in all three widgets; restored within ±0.5 line after reflow/zoom/width change.
- `setViewMode()` transitions preserve cursor (Source↔LivePreview) and scroll (all six pairs).
- `libs/readingview/` builds standalone; its tests pass in isolation (Markoff-style encapsulation).
- ReadingView renders a 500-line note correctly: headings, paragraphs, code highlighting, math, mermaid, images, wiki-links, blockquotes, lists, tables, horizontal rules.
- Section recycling: an edit that doesn't change a section's rendered shape does not re-mount that section; frontmatter changes re-mount exactly the `usesFrontMatter=true` sections.
- Async-parse + frame budget: 10k-line note paints progressively, no main-thread frame over 16 ms during scroll. Worker trigger is exact at 10240 bytes.
- VirtualScroll: 100k-line note opens < 500 ms, ≤ 50 sections mounted at any time.
- Heading-fold: collapsing a level-N heading hides deeper sections; state persists via `EphemeralState`.
- All Cluster A + Cluster B Phase 3 (`workspace.json`) + Cluster I (MetadataCache) tests still pass.

## Blocks / enables

- **Depends on:** Cluster B Phase 3 (`WorkspaceState`) — **done**. Cluster A Phase 1 (frontmatter parsing via markoff-parser) — **done**. Both prereqs verified landed.
- **Blocks:** Cluster H hover-link preview rendering at section granularity (done, but could benefit from ReadingView reuse in future); Cluster J embed/rendering primitives (will reuse ReadingView's section-layout pipeline).
- **Enables:** Obsidian-grade editor responsiveness for large notes; faithful three-mode UX; PDF export (future — the BoxTreeRenderer pattern from Penelope becomes relevant post-MVP).
- **Estimated effort:** 5–7 weeks. Phase 3 (ReadingView MVP) and Phase 4 (recycling) are the tentpoles. Phase 6 (virtualization + fold) has subtle interaction surface. Phase 0 (Source-mode widget brainstorm) is upfront design tax but de-risks Phase 1.

## Preserved Obsidian compat quirks

- Live-preview is **not a separate `mode` value** on the wire; the `{mode: "source", source: false}` compound encoding is the contract. `ViewModeSerializer` enforces this.
- Scroll position is float visual-line, not pixel offset.
- Section recycle key is rendered-shape-equality (the closest analog to HTML-string-equality given we render to QGraphicsItem trees, not HTML) — a markdown change that produces identical rendered shape still recycles.
- Frontmatter-diff triggers re-render of *all* sections that read frontmatter (`usesFrontMatter=true`), not only sections that text-changed.
- Async-parse threshold is exactly **10240 bytes**; under that is sync. Honour the constant.
- 5ms/10-section frame budget is a defensive throttle, not a hard limit. Sections may finish early; budget enforcement is on the *next* batch.
- Markoff already implements "cursor in block reveals source" — we do **not** re-implement this at the Corbomite layer. That invariant is satisfied by Markoff being our LivePreview widget.
