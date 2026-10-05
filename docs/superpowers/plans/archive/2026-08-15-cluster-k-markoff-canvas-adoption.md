# Cluster K — Markoff canvas leaf adoption (D5 part 1, Corbomite side)

**Date:** 2026-08-15
**Type:** Full plan
**Status (2026-08-18): CLOSED — Phase 5.** Phases 0–3 done earlier; user
signed off skipping Phase 4 soft-default and going straight to Phase 5
(QML retirement). Canvas is the sole LivePreview engine; `markoff_live`
unlinked from Corbomite. Callouts remain frozen on Markoff E3. Closeout:
[`docs/decisions-archive.md`](../../decisions-archive.md) § 2026-08-18
Cluster K Phase 5.
**Depends on:** Markoff's canvas production arc (D5 part 1), which closed
2026-08-15 pending its own **G2 (Corbomite adoption)** gate. Markoff spec:
`libs/markoff-family/docs/specs/2026-08-13-canvas-production-design.md`
(fetch via `git -C libs/markoff-family show origin/master:<path>` until
re-pinned — see Phase 0). Status board:
`libs/markoff-family/docs/STATUS.md`.

---

## 1. Goal

Replace `Markoff::Live::EditorWidget` (the QML/QtQuick LivePreview leaf,
`libs/markoff-live`) with `Markoff::Canvas::EditorWidget` (the new
`QAbstractScrollArea`/`QTextLayout` projection-view leaf,
`libs/markoff-canvas`) as Corbomite's LivePreview engine, then retire the
QML leaf entirely. Markoff has already built the canvas leaf out to
feature parity with Live (contract-v2, format verbs, links, completion
seams, ephemeral state, Obsidian Live-Preview-benchmark parity) and closed
its own production arc at **315/315 tests, perf budgets held**. The
remaining work is entirely Corbomite-side: consume it, verify it, dogfood
it, then delete the old leaf. This is explicitly why Markoff calls its own
next step "G2 — Corbomite adoption": the ball is in our court.

**Why now:** the QML leaf has been "bug-fix-only" (frozen) since
2026-08-13; Markoff's own docs describe it as the thing this arc exists to
retire (`libs/markoff-family/CLAUDE.md`: "the hack genre this arc exists
to end"). Every session we stay on Live is a session of feature drift
against a leaf nobody is improving.

**Non-goals for this cluster:**
- Local multi-cursor editing — Markoff deferred this to its own arc; not
  blocking adoption.
- Accessibility (`QAccessibleTextInterface`) — Markoff's G1 was
  user-deferred for the production arc; canvas ships with **no a11y
  support**. This is a known, accepted regression vs. Live (which also had
  none, so no regression in practice — confirm before closing this
  cluster) but must be logged, not silently inherited.
- Retiring `markoff-styled` (Reading mode). Markoff's G3 explicitly treats
  "fold Reading into canvas read-only" as a separate decision from Live
  retirement. Out of scope here unless the user pulls it forward.
- Any change to `libs/markoff-source` (Source mode) — untouched on both
  sides.

## 2. Current state (what we're replacing)

`src/editor/NoteEditorWidget.{h,cpp}` hosts three lazily-constructed
`Markoff::MarkdownView` leaves in a `QStackedWidget`, keyed by
`NoteEditorWidget::ViewMode {Source, LivePreview, Reading}`:

- `m_editor` — `Markoff::Live::EditorWidget`, **constructed eagerly in the
  ctor** (NoteEditorWidget.cpp:54-55), the only leaf that isn't lazy.
- `m_sourceEditor` — `Markoff::Source::Editor`, lazy.
- `m_styledReadingView` — `Markoff::Styled::Editor` (read-only), lazy.

All three share one `wireLeaf()` (theme, `contextChanged`,
`cursorPositionChanged`) and one `Markoff::DefaultLinkService` for link
click/hover. Mode transitions go through `setViewMode()`: capture
ephemeral state → detach doc → swap stack index (lazy-construct if first
visit) → attach doc → restore ephemeral state. **This machinery is already
leaf-agnostic** — `activeLeaf()` returns the `MarkdownView*` base, and
everything downstream (find/replace, format verbs via `addEditorActionBase`
in MainWindow, completion, hover popover, goToLine, ephemeral state,
zoom) dispatches through that base. This is the payoff of the contract-v2
adoption closed 2026-06-10 (Phase 1) — swapping the concrete LivePreview
widget should *not* require touching any of those call sites.

The wire format is unaffected either way: `ViewModeSerializer` persists
Obsidian's `{mode, source}` compound, not a widget identity. Canvas is an
engine swap **under** `ViewMode::LivePreview`, not a fourth mode.

## 3. Target architecture

Same shape, new concrete class for the LivePreview slot:

- `Markoff::Canvas::EditorWidget : Markoff::MarkdownView` (Markoff spec
  §4.1) is the wrapper — "the same shape as `Markoff::Live::EditorWidget`
  wrapping the QML view." It enrolls in the same `ViewContractChecks.h`
  suite the other three leaves use, so contract conformance is
  Markoff-side tested, not something Corbomite needs to re-verify from
  scratch.
- CMake target is `markoff_canvas` / alias `Markoff::Canvas`
  (`libs/markoff-family/libs/markoff-canvas/CMakeLists.txt:22,71`).
  **Naming collision risk (verify, don't assume):** Corbomite already
  links a target literally named `canvas` in `src/CMakeLists.txt:92` —
  that's `libs/canvas`, the `.canvas` whiteboard file library, completely
  unrelated. Different target names (`canvas` vs `markoff_canvas`) so
  CMake won't collide, but grep for bare `canvas` before editing
  `src/CMakeLists.txt` so the new `Markoff::Canvas` link doesn't get
  confused with the existing one in review.
- `libs/markoff-family/CMakeLists.txt` already does
  `add_subdirectory(libs/markoff-canvas)` unconditionally — no submodule
  CMake change needed beyond the re-pin itself.

## 4. Work breakdown

### Phase 0 — Re-pin + build (mechanical, low risk)

1. `cd libs/markoff-family && git fetch origin && git checkout <target-sha>`
   — pin to the arc-close commit (`94f661c3` at time of writing, i.e.
   "canvas(P7.3): arc close — full suite 315/315") or later if Markoff has
   moved further by dispatch time. Re-check `origin/master` log for
   `canvas(` commits past `94f661c3` before pinning.
2. `git add libs/markoff-family && git commit` (submodule pointer bump
   only — same pattern as the 2026-06-10 Phase 1 re-pin and the
   `e9d70a8b` re-pin already on this branch).
3. `cmake --preset dev && cmake --build --preset dev -j 10` — confirm
   `markoff_canvas` builds clean as a new static lib; confirm nothing
   downstream broke (this is a pure submodule-content bump; Corbomite's
   own CMake doesn't change yet in this phase).
4. `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark -j 10` —
   confirm the existing 270ish-test baseline still passes (this re-pin
   pulls in Markoff's own 315/315-tested code but doesn't change any
   Corbomite call site yet, so zero Corbomite-side test deltas expected).

**Definition of done:** submodule re-pinned, full build green, baseline
suite green, no Corbomite source touched yet.

### Phase 1 — Wire the leaf behind a settings-gated toggle

Do **not** make canvas the default on first landing — Markoff's own gate
G2 explicitly says "flip LivePreview to canvas **behind a setting
first**." Mirror that discipline here.

1. `src/CMakeLists.txt`: add `Markoff::Canvas` to `CorbomiteApp`'s
   `target_link_libraries` (near the existing `markoff_live` /
   `markoff_styled` block).
2. `src/editor/NoteEditorWidget.h/.cpp`: add `#include <markoff/canvas/EditorWidget.h>`
   (confirm actual header path once re-pinned) and an `m_canvasEditor`
   member alongside `m_editor`. Do **not** add a new `ViewMode` enum value
   — `LivePreview` must resolve to *either* `m_editor` or `m_canvasEditor`
   depending on the setting, decided once at construction (or read fresh
   each `ensureWidgetConstructed(LivePreview)` call if the setting can
   change at runtime — decide via AskUserQuestion-equivalent judgment call
   at implementation time, but construction-time is simpler and matches
   "restart to change engine" precedent from other settings pages).
3. `leafFor(LivePreview)` / `activeLeaf()` return whichever concrete
   widget backs the slot — same pattern already used for the
   Source/Reading lazy pair, just applied to LivePreview's two possible
   backends instead of always `m_editor`.
4. Settings: add a toggle under the Editor settings page (see
   `docs/PARITY-MATRIX.md` §4 "Settings dialog" for the existing
   page — Editor/Files/Appearance/Daily-notes/Hotkeys/Plugins) — e.g.
   "Live Preview engine: QML (stable) / Canvas (experimental)" — backed
   by KConfig, read once at `NoteEditorWidget` construction.
5. `wireLeaf()` must run identically for whichever concrete widget is
   constructed — no leaf-specific branches beyond the existing
   `ensureWidgetConstructed`-style switch.

**Definition of done:** with the setting off (default), zero behavior
change — full baseline suite green. With the setting on, the app launches
into canvas LivePreview instead of QML Live, and does not crash on
open/type/save/mode-switch.

### Phase 2 — Corbomite-side feature-parity verification

Markoff's spec §5.2 lists everything canvas already implements against
the *old leaf's contract*. Verify each still functions through
Corbomite's specific wiring (not Markoff's generic contract tests) with
the toggle on:

| Corbomite feature | Wiring to re-verify |
|---|---|
| Link click/hover | Shared `m_linkService` (`DefaultLinkService`) — confirm canvas's `EditorWidget` accepts `setLinkService`/emits through the same `Markoff::LinkService` the hover popover and `onLinkActivated` already consume |
| `[[`/`#` completion | `CompletionController::setLeaf(activeLeaf())` on mode/engine switch — base-pointer dispatch, should be free, but the completion popup's `caretRect()` positioning is new code on the canvas side (spike-era, contract v2 §4.1) — eyeball it |
| Find/Replace | `attachFindController`/`detachFindController` base dispatch — should be free per contract-v2 |
| Format verbs (B/I/strike/code/link/heading) | Base dispatch via `addEditorActionBase` — should be free; canvas exposes them via `CanvasActionController` (mirror of `LiveActionController`) per spec §5.2, but Corbomite doesn't call that controller directly (MainWindow.cpp:539,1583 comments confirm base dispatch replaced direct `LiveActionController` calls in Phase 1) — no MainWindow change expected, confirm by testing, not by reading |
| Ephemeral state (scroll/cursor/fold) | `saveEphemeralState`/`restoreEphemeralState` base dispatch — canvas adds fold via `Session::foldedRegions` per spec §5.3; confirm round-trip through `EphemeralState` JSON |
| Theme propagation | `applyThemeToAllLeaves()` iterates a fixed `{m_editor, m_sourceEditor, m_styledReadingView}` list at NoteEditorWidget.cpp:194-195 — **must add whichever canvas member exists to this list**, easy to miss since it's a literal initializer-list, not a loop over constructed leaves |
| Mermaid/Math/Image injection seams | Spec §5.2 lists these as seams canvas exposes (`MermaidRenderer`, jkqtmathtext, image resource provider) — Corbomite's own `VaultResourceProvider` plug-in point is currently **unwired even for Live** (`NoteEditorWidget.cpp:134-144` TODO) — this is a pre-existing gap, not a canvas regression; do not scope-creep it into this cluster unless trivial |
| Word count / Ln,Col status bar | Base `cursorPositionChanged` — should be free |
| goToLine | Base dispatch — should be free (used by template-insertion, daily notes, backlink navigation) |
| Zoom/font scale | Base `setFontScale` — should be free |

**Definition of done:** every row above manually exercised with the
toggle on, in a running (`--nested` or real, not offscreen-only) session;
gaps found get punch-listed, not silently patched over — this phase is
verification, Phase 3 fixes what it finds.

### Phase 3 — Gap-fix pass

Whatever Phase 2 finds. Expect this to be small given Markoff's own
315/315 + F1-audit closure, but the theme-list omission (Phase 2 table,
row 6) is a near-certain one-line miss worth calling out now so it isn't
"discovered" as a surprise bug later.

### Phase 4 — Dogfood (Markoff's G2, exercised)

1. Flip the *dev-build default* to canvas (keep the setting so it's still
   toggleable/reversible), per the road-to-dogfood loop's own precedent
   (Phase 6, started 2026-06-10) — real usage surfaces what test suites
   miss.
2. Run a real editing session across a range of note shapes (tables,
   code blocks, links, tags, headings, lists, checkboxes, math if
   applicable) — same spirit as the first-run dogfood pass that caught
   the welcome-screen SIGSEGV.
3. Log everything to the punch list, severity-ranked, same discipline as
   existing punch-list entries.
4. Do **not** flip the *release preset* default until this phase is
   clean — dev-only exposure first.

### Phase 5 — Retirement (Markoff's G3, mirrored on our side)

Only after Phase 4 is clean and the user explicitly signs off (this is a
user decision on both sides of the seam, per Markoff's own spec §8 — do
not automate past it):

1. Flip the *default* (dev and release) to canvas; keep the setting for
   one release as an escape hatch, or remove it immediately — user's call
   at gate time.
2. Delete `m_editor`/`Markoff::Live::EditorWidget` construction from
   `NoteEditorWidget`, the QML-specific ctor wiring
   (`LiveListModelBinding::AllCapabilities`, the `markoff_liveplugin`
   link lines in `src/CMakeLists.txt:80-85`), and the settings toggle
   from Phase 1 (now dead — one engine only).
3. On the Markoff side (separate repo, coordinate via handoff doc under
   `docs/handoff/` per existing convention): once Corbomite confirms
   retirement, Markoff can move `libs/markoff-live` to
   `bug-fix-only` → fully archived, per its own G3 language ("retire
   markoff-live; fold markoff-styled's Reading mode into canvas
   read-only, or keep styled" — the second half is a **separate**
   decision, do not bundle it into this cluster's close unless the user
   explicitly extends scope).
4. Update `docs/PARITY-MATRIX.md` §3 row "Three modes..." evidence
   pointer away from `Markoff::Live::EditorWidget`; update
   `CLAUDE.md`'s "State of the world" banner.

**Definition of done:** `markoff-live` unreferenced by Corbomite source
and CMake; full suite green; `docs/decisions-archive.md` closeout entry
written; PROJECT-STATE §Current focus updated per the 3-sentence rule.

## 5. Risks / open questions to flag to the user before dispatch

- **Accessibility regression, or not?** Need to confirm whether
  `Markoff::Live::EditorWidget` (QML) had any real `QAccessible` support
  worth losing. If Live never had it either, canvas's G1-deferred gap is
  a non-regression and should be documented as such, not treated as a
  blocker.
- **Runtime engine switching.** Phase 1 assumes construction-time
  (restart-to-apply) is acceptable for the settings toggle. If the user
  wants live A/B switching without restart, `ensureWidgetConstructed`
  needs a re-entrant path that can rebuild the LivePreview slot with the
  document still attached — more work, not assumed here.
- **Collab rendering surface (spec §4.4)** is D5's stated reason for the
  projection architecture, but Corbomite has no real-time multi-user
  collab session today — the surface will sit inert. Not a blocker, just
  noting the feature isn't "wasted," it's future-facing for whenever
  collabtext transport work lands on the Corbomite side.
- **Perf budgets** were validated on Markoff's own `build-perf`
  (RelWithDebInfo) harness with synthetic documents. Corbomite should
  re-check perceived responsiveness on real, large vault notes during
  Phase 4 dogfood rather than trusting the upstream numbers blind.

## 6. Blocks / enables

- **Blocks:** nothing currently in-flight depends on this landing first.
- **Enables:** closes the long-standing "Live leaf is QML/hacky" framing
  in `CLAUDE.md`'s state-of-the-world banner; likely also simplifies
  Cluster E's eventual re-scope (editor plugin API) since it removes one
  of the three leaf implementations plugins might need to special-case.

## 7. Explore-agent dispatch prompts (for the implementation session)

- Phase 0: none needed — mechanical submodule bump, do directly.
- Phase 1: `Explore` — "Find every construction site and header include of
  `Markoff::Live::EditorWidget` and `Markoff::Source::Editor` in
  `src/editor/NoteEditorWidget.{h,cpp}` and `src/CMakeLists.txt`, plus how
  `libs/markoff-family/libs/markoff-canvas/include/markoff/canvas/EditorWidget.h`
  (post re-pin) declares its constructor and public surface, so the new
  member can be added with matching signature conventions."
- Phase 2: no agent — manual, human-eyeball verification pass (offscreen
  Qt can't drive real interaction, same caveat noted throughout
  PARITY-MATRIX for every "pending user eyeball" row).
