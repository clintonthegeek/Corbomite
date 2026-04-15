# Cluster E — Three-mode pivot Source/LivePreview/Reading (retrospective)

**Landed:** 2026-04-15. 7 numbered phases + 2 fork-related prerequisite phases (qutepart-corbomite Phases 1+2). 18 commits across one session.

## Phase commits

- **Fork Phase 1** (`f22de54`) — vendor `qutepart-cpp` at upstream commit `eec2e9a` into `libs/qutepart-corbomite/` with SPDX dual-headers on 66 inherited files + one smoke test.
- **Phase 0b** (`50f126d`) — bootstrap greenfield `libs/readingview/` with Penelope `CodeBlockHighlighter` transplant.
- **Fork Phase 2** (`c70d44d`) — `Corbomite::SourceEditor` shim at `src/editor/` with visual-line float scroll accessors extended into the vendored `Qutepart` class.
- **Phase 0+2 docs** (`f516d83`) — PROJECT-STATE + INDEX updates.
- **Phase 1** (`15496ac`) — `ViewMode` grew `{Editing, Reading}` → `{Source, LivePreview, Reading}`; `Corbomite::EphemeralState` at `libs/storage/`; `Corbomite::ViewModeSerializer`.
- **Phase 1 docs** (`5c6548d`).
- **Phase 2** (`dce2107`) — visual-line float scroll across all three widgets; `Markoff::Editor::scrollPositionVisualLine` added without touching the block-item hierarchy.
- **Phase 2 docs** (`ab057d4`).
- **Phase 3a** (`0caaf97`) — ReadingView MVP skeleton: Penelope styling transplant (ParagraphStyle/CharacterStyle/StyleManager) + ReadingPipeline + ReadingSection + SectionLayout for 6 simpler content types.
- **Phase 3b** (`92de2b0`) — rich content matrix: tables/images/wiki-links/math (QTextObject-based `ReadingMathObject`)/mermaid; `SpanRenderer` replaced ad-hoc HTML emitter; `VaultResourceProvider` abstract interface.
- **Phase 3 docs** (`148b87d`).
- **Phase 4** (`5faf960`) — `SectionRecyclePool` + frontmatter-diff trigger; `ReadingView::rebuild` became a diff mounter.
- **Phase 4 docs** (`6ab1dbf`).
- **Phase 5** (`4515843`) — `ReadingParseWorker` (10240-byte async threshold) + 5ms/10-section frame-budgeted mount; contract constants pinned in `ReadingViewConstants.h`.
- **Phase 5 docs** (`aa1f7fd`).
- **Phase 6** (`a68a53e`) — `VirtualScrollController` + heading fold; cumulative-newline table brought 1000-section benchmark from 8s → 1-1.3s.
- **Phase 6 docs** (`151b54b`).
- **Phase 7** (`<this commit>`) — `NoteEditorWidget` QStackedWidget + 3-mode transition; EphemeralState wired through `PaneLeaf.unknown["eState"]` end-to-end; MainWindow UI toggle expanded 2→3 modes.

**Test budget:** ~50 new unit + e2e tests across 15 new test executables (CodeBlockHighlighter bootstrap, SourceEditor round-trip, ViewModeSerializer compound encoding, EphemeralState round-trip + unknown-key preservation, Markoff scroll-position, NoteEditorWidget ephemeral, ReadingPipeline section split, SectionLayout basic + 5 per-type rich-content suites, SectionRecyclePool, async parse, frame-budget constants, VirtualScrollController, heading fold, NoteEditorWidget mode transition, workspace.json round-trip). Full suite 103/107 outside the 4 pre-existing flaky failures.

## What changed vs the original plan

The original 2026-04-14 plan assumed three modes would be three *states of one Markoff editor*. The 2026-04-15 plan rewrite (after the parser-split commit `82249be` deleted the old ViewMode enum) reframed them as three distinct widgets at the `NoteEditorWidget` layer. That rewrite held end-to-end — no further scope drift.

Design calls + deviations worth recording:

1. **Penelope evaluated as reading-mode base + rejected.** ~30 KLOC, MD4C + HarfBuzz + ICU + Poppler, pages-always-in-memory synchronous parse/layout. Incompatible with Corbomite's virtualization + async + frame-budget contracts. Decision: build Reading greenfield, selective source transplant. Manifest in the plan specified which files adopt (CodeBlockHighlighter wholesale; ParagraphStyle/CharacterStyle/StyleManager with print/PDF fields stripped) and which discard (layoutengine, documentbuilder, rendercache). Executed faithfully.

2. **qutepart-cpp vendored + forked, not adopted as-is.** Source-mode widget decision ran through 5 candidates (KTextEditor / QPlainTextEdit+KSyntaxHighlighting / stripped-Markoff / QScintilla / qutepart-cpp). qutepart-cpp won on QPlainTextEdit-compatibility + MIT license + active upstream. 8-phase shaping plan spun out as a parallel internal refactor — Phases 1+2 needed here, Phases 3-8 asynchronous.

3. **Fractional scroll quantization resolved as "accept ±0.5-line precision".** `QPlainTextEdit::scrollContentsBy` ignores the `dy` pixel delta and rounds to integer visual-line (verified against `~/src/qtbase`). Three options evaluated in Phase 2's open sub-question: (a) override `scrollContentsBy` + maintain our own `topLineFracture`, (b) private-API hack into `QPlainTextEditPrivate::setTopBlock`, (c) accept ±0.5. Picked (c) — (a)/(b) are Phase-4-of-the-fork-plan scope (KSyntaxHighlighting rework territory). On-wire format stays `float` so later precision work doesn't break serialization.

4. **Section-splitting hand-rolled vs. AST-walked.** Phase 3a agent picked a line scanner with fenced-code awareness over walking `MarkoffParser::Document`'s heading offsets. Reason: `Document`'s public heading offsets are relative to the frontmatter-stripped content, introducing cross-coordinate-system risk. Plan permitted reporting before reverting to line-splitting; agent judged the line scanner cleaner for Phase 3a. Phase 4's move of `renderedShape` from post-layout to pre-layout stayed in the line-scanner frame; no regressions surfaced.

5. **`renderedShape` moved from post-layout (SectionLayout) to pre-layout (ReadingPipeline) during Phase 4.** Post-layout digests can't inform the "should we layout at all" decision — defeats recycling's point. Pipeline now hashes whitespace-trimmed source + `(section-type, heading-level)` discriminator. SectionLayout falls back to its legacy post-layout digest only for sections arriving with empty shape (keeps layout-only unit tests passing).

6. **`VaultResourceProvider` defined as new abstract interface in readingview.** ReadingView is a peer library to Markoff and cannot depend on it. Signatures mirror Markoff's `ResourceProvider` but add `loadImageBytes` + rename `resolveLink→resolveWikiLink` to decouple from the `.md` suffix. App-level adapter forwards to the concrete provider. Worth promoting to a shared `Corbomite::Core::VaultResourceProvider` in the future — filed as a follow-up.

7. **VirtualScrollController callback-based rather than coupled.** Plan had it holding direct references to `SectionLayout`, `SectionRecyclePool`, scene. Phase 6 agent picked a `LayoutCallbacks` struct with function hooks — keeps the controller pure-policy, testable without a full scene. No regressions; probably the better architecture.

8. **Cursor-column preservation across Source↔LivePreview transitions is best-effort (line-only).** Markoff's public API exposes `goToLine(int)` only — no column setter. Added column would need a Markoff API extension, out of scope per the cluster guardrails. Source-side cursor-line+column is fully preserved; LivePreview-side sets line but not column.

9. **Reading-mode scroll restore during `setViewMode` transitions is best-effort.** ReadingView's async-parse + virtualization means `setScrollPositionVisualLine` before sections mount clamps to 0. Tests validate swap plumbing (content + mode + signal); exact scroll-through-Reading defers to a `mountingFinished` queued-apply hook (follow-up).

10. **EphemeralState → workspace.json wiring used `PaneLeaf.unknown["eState"]`**, leveraging Cluster B's unknown-key preservation mechanism. No schema change to `WorkspaceState` — `eState` rides as an unknown key + Phase 7 added the typed deser/ser pass. Legacy `cursorLine`/`scrollPosition` shape still loads with graceful fallback.

## What surprised

- **Clangd noise after every agent return.** Every Phase dispatch produced a pile of stale clangd diagnostics (`file not found`, `undeclared identifier`, `.moc file missing`). Cause: clangd index lagged behind the actual `compile_commands.json` which was always fresh. Pattern: verify independently via `cmake --build` + `ctest` rather than trusting the IDE diagnostics. Noted for future multi-agent sessions.

- **Penelope's `StyleManager` was NOT ThemeManager-coupled.** Phase 3a agent expected a messy transplant per the plan's warning. Turned out clean — the StyleManager stands alone; the Theme enum added was forward-looking, not a backport.

- **The plan's "30 iterations × 5 block types" end-to-end fixture grew to 121 mounted sections under real parsing.** Each heading pair (H1 + H2) emits two overlapping sections in the current pipeline. Flagged as a Phase-6-territory follow-up (heading-fold semantics), but Phase 6's controller + fold logic both handled it without retrofit.

- **Frame-budget yield-only-on-actual-layout was load-bearing.** Recycled sections take microseconds each; counting them against the 5ms clock would cause the budget to trigger unnecessarily on pool-heavy reparses. Phase 5 caught this in review.

- **requestId coalescing needed a DOUBLE check in the worker slot.** Single check (before parse) left a race where the user types again *during* parse → worker emits stale result. Added second check (before emit) + `QPointer<ReadingView>` guard on the UI resume lambda. No-flicker.

- **The `O(n²) → O(n)` win on `lineOfOffset` made the 1000-section benchmark go from 8s → 1.3s.** One cumulative-newline table in `ReadingPipeline` — obvious in hindsight, but easy to miss when the first pass prioritised correctness.

- **The session ran end-to-end without a user-redirected scope change.** User said "full steam ahead" after Phase 1 committed, subagents handled each subsequent phase + `Ritual 2` docs updates landed cleanly. 18 commits, one session.

## Downstream effects

- **Cluster J (Embed / rendering primitives) unblocked.** `![[Note#heading]]` embed resolution needs ReadingView's section-level mount + `VaultResourceProvider` plumbing — both exist now. Cluster J was listed as "stub plan" in PROJECT-STATE; it's now expandable to full plan + executable.

- **Hover-link preview rendering at section granularity** (Cluster H follow-up) becomes practical now that ReadingView can render an arbitrary section without the full note. Currently outside Cluster H's scope, but the primitive is here.

- **Source-mode find/replace** (fork plan Phase 3) is unblocked but not executed. Can run in parallel with any cluster.

- **PDF export** (future) — Penelope's `BoxTreeRenderer` pattern transplant becomes natural here. Not on the parity roadmap but the architecture accommodates it.

## Residual follow-ups (not blockers)

1. **Source↔LivePreview cursor-column preservation.** Needs `Markoff::Editor::setCursor(line, col)` public API addition (new Markoff-library change).

2. **Reading-mode scroll restore during `setViewMode` transitions.** Defer to `mountingFinished` via a queued `QTimer::singleShot` apply hook — matches Obsidian's own `applyScrollDelayed` pattern.

3. **Source-mode fractional scroll precision.** Currently ±0.5 visual-line. Option (a) (override `scrollContentsBy` in the forked `Qutepart`) is the clean path; lives naturally in fork Phase 4 (KSyntaxHighlighting rework).

4. **Gutter fold arrow UX.** Currently a bare 8×10px triangle drawn directly. Phase-7 UX pass could swap for a themed QIcon via `QIcon::fromTheme("triangle-down")` / `"triangle-right"`.

5. **Overlapping-sections on nested headings.** Pipeline emits separate sections for `#H1` and nested `##H1.1` rather than one enclosing section. Phase-6 controller + fold handles it correctly, but cleanup would simplify the mental model.

6. **Native-QPA 100k-line benchmark.** Current 1500ms regression gate is for offscreen + cold cache. Plan's 500ms target likely achievable under native QPA; benchmark confirms deferred.

7. **Shared `Corbomite::Core::VaultResourceProvider`.** ReadingView + Markoff + future consumers each have their own interface. A Core-level promotion eliminates adapter layers. Not blocking.

8. **Fork plan Phases 3-8.** Public find/replace, KSyntaxHighlighting replacement, trim indent engines, remove bundled themes, markdown-specific features, rename/rebrand. All asynchronous to the parity roadmap; schedule on demand.

## Lessons for the next cluster

- **Dispatch parallel subagents without worktrees when they touch disjoint directories.** Have the orchestrator handle any shared-file wire-in (e.g. top-level CMakeLists.txt `add_subdirectory`) after both return. Avoids merge conflict risk while keeping agents fully parallel. (Pattern established in Phase 0 dispatch — both the qutepart-fork Phase 1 + readingview Phase 0b ran in parallel to clean success.)

- **Clangd noise is not a build failure.** Always `cmake --build build && ctest` to verify. Stale diagnostics from the IDE index are ubiquitous; don't re-dispatch on them.

- **Pinning Obsidian wire constants in a dedicated `*Constants.h` header + a `tst_*_constants.cpp` test** enforces contract at both compile + test time. Pattern worth copying for any future cluster that has to honour specific wire numerics (e.g. Cluster J embed cache size, Cluster O query layer limits).

- **Preserve Phase-N's test contracts as Phase-(N+1) introduces new mechanics.** Phase 5 nearly broke Phase 4's pointer-identity tests by flushing `m_sections` too early; kept them passing by only updating `m_sections` after a section actually mounts. Pattern: "prior-phase invariants are non-negotiable unless explicitly retired".

- **Source of truth for boundary rules belongs in one place.** The "Section boundary rules" passage in the cluster plan was referenced by 4 different phases (3a, 3b, 4, 6). Single paragraph, no divergence. Contrast with earlier clusters where boundary rules occasionally got re-stated differently per phase.
