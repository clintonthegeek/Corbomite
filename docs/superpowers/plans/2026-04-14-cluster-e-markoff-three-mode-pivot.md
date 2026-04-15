# Cluster E — Markoff three-mode pivot

> **Living-status note:** This file is the *plan*. Live status (Not started / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file. Edit this file only when the plan itself changes (work breakdown, target classes, references).

**Plan written:** 2026-04-14. Derived from `docs/obsidian-audit/GAP-ANALYSIS.md` §Cluster E.

**Covers:** P1.3 (three-mode encoding for `workspace.json` round-trip), P1.4 (visual-line float scroll persistence), P2.5 (Markoff three-mode live-preview semantics — "cursor in block reveals markdown source"), P2.6 (progressive section rendering + recycling pool).

## Goal

Make Markoff a **behaviourally faithful three-mode editor** at the contract level: source-mode + live-preview-mode (which is *not* a separate mode internally — it's source-mode-with-`source: false`) + reading-mode. Persist mode + scroll position to `workspace.json` in the exact shape Obsidian expects (visual-line float, not pixel offset). Implement the section-recycling preview pipeline so that large notes paint progressively and stay responsive.

The cluster exists because all four gaps are in the editor stack and depend on the same architectural contracts. Doing them in isolation produces a UI that looks right per-test but breaks compat round-trip in subtle ways.

## Audit references

- **Three-mode is two dimensions:** `domains/editor-markdown.md §8 invariant 2` — `{mode: "source"|"preview"}` × `{source: true|false}` (when `mode: "source"`). Live-preview = source-mode + `source: false`. Reading-mode = `mode: "preview"`. **Critical compat invariant.**
- **Mode transition mechanics:** `domains/editor-markdown.md §1` — `setMode` `await`s `save()` before leaving source, captures fold info, restores scroll via ephemeral state, applies the new mode, restores fold + scroll.
- **Visual-line float scroll:** `domains/editor-markdown.md §1` and §3 — scroll position is `42.73` (not pixels), survives reflow + zoom + width changes. **Required for `workspace.json` round-trip.**
- **Preview pipeline:** `domains/editor-markdown.md §1` (preview pipeline section) — HTML-string-equality recycle key, **10240-byte async-parse threshold**, **5ms / 10-section frame budget**, virtual-scroll with selection-window extension, frontmatter-diff forcing `usesFrontMatter=true` section rerender, per-section HTML `{from, to}` line mapping.
- **Section recycling pool:** Pass 1's `01-markoff-gaps.md` Editor section (recycle pool with HTML-string-equality key) — the canonical signal that Markoff currently re-renders whole-doc.
- **Cursor-in-block reveals source:** `domains/editor-markdown.md §1` (mode semantics) — granularity of "block" is per-paragraph for inline rendering and per-block for fenced/embedded content. The decision is in the live-preview StateField (cite `domains/editor.md §1` — `editorLivePreviewField`).
- **Ephemeral state:** `domains/workspace.md §3` (workspace.json schema includes `eState` per leaf) — scroll, cursor, mode, fold-state are stored as ephemeral and restored on layout-restore.
- **Heading-fold per section:** `01-markoff-gaps.md` — `headingCollapsed` tracked per section in `MarkdownPreviewSection`.
- **`Markoff::Editor` current API** lives at `libs/markoff/include/markoff/Editor.h` — current `Mode { Source, LivePreview }` enum needs documenting against the compound encoding.
- **Three-mode wrapper in Corbomite:** `src/editor/NoteEditorWidget.{h,cpp}` already exposes `ViewMode { Source, LivePreview, Reading }` — needs the persistence shim.

## Target classes

| Class | File | Notes |
|---|---|---|
| `Markoff::Editor` (extend) | `libs/markoff/include/markoff/Editor.h` | Mode-transition `setMode(Mode, transitionOptions{captureFold, captureScroll})` matching Obsidian's `await save()` semantics |
| `Markoff::PreviewPipeline` | `libs/markoff/src/PreviewPipeline.{h,cpp}` | Section-by-section render with recycling pool, HTML-equality keying, 5ms/10-section budget, async-parse over 10240 bytes |
| `Markoff::PreviewSection` | `libs/markoff/src/PreviewSection.{h,cpp}` | `{from, to, html, headingCollapsed, level, usesFrontMatter}`. Equality on HTML string drives recycle eligibility |
| `Markoff::VirtualScroll` | `libs/markoff/src/VirtualScroll.{h,cpp}` | Selection-window extension, only mounts visible sections + a margin |
| `Corbomite::ViewModeSerializer` | `src/editor/ViewModeSerializer.{h,cpp}` | Translate `NoteEditorWidget::ViewMode {Source, LivePreview, Reading}` ↔ Obsidian's `{mode, source}` for `workspace.json` round-trip |
| `Corbomite::EphemeralState` | `libs/storage/src/EphemeralState.{h,cpp}` | `{scroll: float (visual-line), cursor: {line,col}, mode: ViewMode, foldedHeadings: vector<int>}` per leaf |
| `Markoff::ScrollPosition` | `libs/markoff/include/markoff/ScrollPosition.h` | `float visualLine` accessor + setter; converts to/from pixel offset given current viewport |

`NoteEditorWidget` integrates `EphemeralState` and `ViewModeSerializer`; `Markoff::Editor` exposes `ScrollPosition` and `setMode` matching the audit-spec'd transition.

## KDE / GPL3-compatible prior art

**Local KDE source convention:** the KDE source tree is checked out locally at `~/src/kde/src/<repo>`. **Always grep there first; never clone from `invent.kde.org` unless a repo is genuinely missing locally.** Verified-present locally: `kate`, `kdevelop`, `kio`, `kconfig`, `kconfigwidgets`, `kparts`, `kxmlgui`, `kwidgetsaddons`, `ktexteditor`, `krunner`, `baloo`, `okular`, `poppler`, `qtkeychain`, `sonnet`.

| Target | Local path | What we're looking for |
|---|---|---|
| Block-widget decoration in editor | `~/src/kde/src/ktexteditor/src/inlinenote/`, `~/src/kde/src/ktexteditor/src/messages/` | Inline DOM widget patterns inside a text editor — direct precedent for live-preview block widgets |
| Folding / collapsing | `~/src/kde/src/ktexteditor/src/buffer/` (folding ranges) | Heading-fold persistence and visual representation |
| Visual-line scroll | `~/src/kde/src/ktexteditor/src/view/` (KateView scroll logic) | Translating a logical line to a viewport-stable position |
| Virtual-scroll / list view recycling | Qt6 native `QAbstractItemView`'s viewport recycling, `QQuickItemView` for QML | Section-recycling pool architecture |
| Mode-transition with state preservation | `~/src/kde/src/ktexteditor/src/view/kateview.cpp` — search for view-mode switches that preserve cursor | Pattern for save-state/swap-mode/restore-state |
| Markdown preview pipeline | external — Markoff is bespoke; no direct KDE prior art beyond Markoff itself | This is mostly Markoff-internal architecture |
| Async section parse | `~/src/kde/src/ktexteditor/` (async hl pass), `~/src/kde/src/baloo/src/file/extractorprocess.cpp` (async file processing) | Threading model for off-main-thread parse |
| Scroll-position persistence (visual-line) | `~/src/kde/src/ktexteditor/src/view/` — search for `restoreScrollPosition` | Concrete prior art for floating-point scroll persistence |

## Work breakdown

**Phase 1 — ViewMode encoding + EphemeralState scaffold:**
1. Define `Corbomite::EphemeralState` struct + JSON serialiser. Round-trip test against an Obsidian-written `workspace.json` fixture (Cluster B Phase 6 produces these).
2. Define `Corbomite::ViewModeSerializer` with two static methods: `toCompound(ViewMode) → {QString mode; bool source}` and `fromCompound({mode, source}) → ViewMode`. Unit tests for every combination.
3. Wire `NoteEditorWidget::saveEphemeralState` / `restoreEphemeralState` to use both. Doesn't change visible behaviour — only persistence shape.

**Phase 2 — ScrollPosition (visual-line float):**
4. Add `Markoff::ScrollPosition { float visualLine; }` to `Markoff::Editor` API.
5. Implement getter: convert current pixel scroll offset to visual-line float (per-line height varies — use `QGraphicsView`'s mapped coords + a binary search over rendered line bounds).
6. Implement setter: convert visual-line float to pixel offset; tolerant of post-reflow viewport differences.
7. Persist via `EphemeralState`; round-trip across reflow+zoom; assert visual-stability by testing `serialise → reflow → deserialise → snapshot` produces the same view.

**Phase 3 — PreviewSection + recycling pool:**
8. Define `Markoff::PreviewSection`. Owned by `Markoff::ReadingView`/`PreviewPipeline`.
9. Implement HTML-string-equality recycle key. When source markdown changes, diff at section granularity; sections whose HTML output is byte-identical retain their `QGraphicsItem` (no re-mount, no re-render).
10. Frontmatter-diff trigger: any change to frontmatter forces all sections with `usesFrontMatter=true` to re-render. Test by editing frontmatter and asserting only those sections re-mount.
11. Heading-fold per section — `headingCollapsed: bool` plus `level: int`. When a heading at level N is collapsed, hide all subsequent sections until the next heading at level ≤ N. Restore from `EphemeralState.foldedHeadings`.

**Phase 4 — Async parse + frame budget:**
12. Implement 10240-byte async-parse threshold: notes under 10kB parse synchronously on main thread; over 10kB go to a worker (Qt `QThread` + `QFutureWatcher`).
13. Implement 5ms / 10-section per-frame budget. Use `QTimer::singleShot(0, ...)` to yield mid-render. Pause render when budget exceeded; resume on next frame.
14. Benchmark: 10k-line note with 500 sections renders progressively, no main-thread block over 16ms during scrolling.

**Phase 5 — VirtualScroll:**
15. Implement `Markoff::VirtualScroll` — only mount sections within `viewport ± selectionWindowMargin` (default: 1× viewport height before + after). Sections outside the window unmount their `QGraphicsItem` (kept in pool for recycle).
16. Test: 100k-line note opens in <500ms, only ~30 sections mounted at a time, scroll smoothly through.

**Phase 6 — Mode transition contract:**
17. `Markoff::Editor::setMode(Mode, transitionOptions)` — implements the `await save() → capture fold/scroll → swap mode → restore fold/scroll` sequence per `editor-markdown.md §1`.
18. Cursor-in-block-reveals-source for live-preview: per-paragraph + per-block granularity. The decision logic lives in a dedicated `Markoff::LivePreviewDecorator` class (cite `domains/editor.md §1` `editorLivePreviewField` for behaviour).
19. End-to-end test: open note in source, switch to live-preview, edit a paragraph, cursor exits paragraph → re-renders to live-preview widget; switch to reading, scroll, back to source, scroll position preserved as visual-line float.

## Explore-agent dispatch prompts

**Prompt 1 — KateView visual-line scroll prior art:**
> Read KateView scroll-position handling at `~/src/kde/src/ktexteditor/src/view/kateview.cpp` and adjacent files. Do NOT clone from upstream — local source is current. Identify: (a) how Kate represents scroll position when persisting view state across reload, (b) whether Kate uses visual-line, document-line, or pixel offset, (c) the mechanics of restoring scroll across word-wrap/zoom changes. Report a translation plan for `Markoff::ScrollPosition`. Under 600 words.

**Prompt 2 — Inline-block-widget pattern in KTextEditor:**
> Read KTextEditor's inline-note implementation at `~/src/kde/src/ktexteditor/src/inlinenote/` and message bar at `~/src/kde/src/ktexteditor/src/messages/`. Identify: (a) how block-widgets are inserted between text lines, (b) lifecycle (creation, attachment, removal), (c) interaction with cursor positioning and scroll, (d) layout-vs-text-vs-cursor coordinate handling. Report whether the pattern transfers to Markoff's QGraphicsView-based live-preview widgets, or whether QGraphicsView's scene model offers a cleaner approach. Under 700 words.

**Prompt 3 — QGraphicsView virtual-scroll feasibility:**
> Investigate Qt6 QGraphicsView's native facilities for viewport-only-mount-of-items. Look at `QGraphicsScene::itemsBoundingRect`, `QGraphicsView::cacheMode`, `QGraphicsScene::setItemIndexMethod`. Cite Qt6 documentation behavior. Compare against a hand-rolled mount/unmount pool driven by `viewportEvent`. Recommend approach for Markoff's `VirtualScroll`. Under 500 words.

**Prompt 4 — Existing Markoff state inventory:**
> Read `libs/markoff/include/markoff/Editor.h`, `libs/markoff/include/markoff/ReadingView.h`, and `libs/markoff/src/Editor.cpp`. Enumerate: (a) current `Mode` enum and what `setMode` does today, (b) current scroll-position API (if any), (c) current section/block representation in ReadingView (if any progressive rendering exists), (d) what `Markoff::ResourceProvider` exposes that the preview pipeline will consume. Output a "current state inventory" + "delta to target" report. Under 700 words.

## Definition of done

- `EphemeralState` + `ViewModeSerializer` round-trip Obsidian-compatible workspace.json shape; ViewMode {Source, LivePreview, Reading} ↔ {mode, source} mapping is byte-stable.
- `Markoff::ScrollPosition` exposes visual-line float; persists across reflow+zoom; restored to within ±0.5 line.
- `PreviewSection` recycling: edits that don't change HTML output don't re-mount sections; edits that change frontmatter re-mount only `usesFrontMatter=true` sections.
- Async parse + frame budget: 10k-line note paints progressively, no main-thread frame over 16ms during scroll.
- `VirtualScroll`: 100k-line note responsive; <50 sections mounted at once.
- `setMode` transition preserves cursor + scroll across all 6 mode-pair transitions.
- Live-preview "cursor in block reveals source" behaves at paragraph + fenced-block granularity.
- All Cluster A + Cluster B Phase 3 (workspace.json) tests still pass — three-mode persistence integrates with workspace.json round-trip.

## Blocks / enables

- **Depends on:** Cluster B Phase 3 (WorkspaceState writes ephemeral state into workspace.json), Cluster A Phase 1 (frontmatter parsing — needed for frontmatter-diff trigger).
- **Blocks:** Cluster H (hover-link preview rendering depends on a mature preview pipeline), Cluster J (embed rendering uses the same recycling pool).
- **Enables:** Obsidian-grade editor responsiveness for large notes; faithful three-mode UX.
- **Estimated effort:** 4–6 weeks. Phase 3 (recycling) and Phase 4 (async parse) are the largest sub-projects; Phase 6 has subtle interaction surface that may need iteration.

## Preserved Obsidian compat quirks

- Live-preview is **not a separate `mode` value**; the `{mode: "source", source: false}` compound encoding is the contract.
- Scroll-position is float visual-line, not pixel offset.
- Section recycle key is HTML-string-equality (not source-string-equality) — a markdown change that produces identical HTML still recycles.
- Frontmatter-diff triggers re-render of *all* sections that read frontmatter (`usesFrontMatter=true`), not only sections that text-changed.
- Async-parse threshold is exactly 10240 bytes; under that is sync. Honour the constant.
- 5ms/10-section frame budget is a defensive throttle, not a hard limit. Sections may finish early; budget enforcement is on the *next* batch.
