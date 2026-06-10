# Phase 1 (Markoff re-pin + MarkdownView contract-v2 adoption) — IN-PROGRESS handoff

**Written:** 2026-06-10, mid-execution, for a fresh agent to take over.
**Plan being executed:** [`docs/superpowers/plans/2026-06-10-phase1-markoff-repin-contract-v2.md`](../superpowers/plans/2026-06-10-phase1-markoff-repin-contract-v2.md)
(⚠ this plan file is currently **untracked** in git — commit it in Task 12).
**Branch:** `master` in **both** `~/dev/Corbomite` and `~/dev/Markoff` (user authorized "master + push per plan"). Both repos push to their remotes per the plan.

---

## TL;DR status

| Task | State |
|---|---|
| 0 Preflight | ✅ done |
| 1 Markoff embed image-node fix (TDD) | ✅ committed + **pushed** (`9a6a6b74`) |
| 2 Re-pin #1 → `9a6a6b74` | ✅ committed (`08098ca2`) |
| 3 Find-attach via base | ✅ committed (`bfa2fa16`) |
| 4 Undo/redo via base | ✅ committed (`b5a4b041`) |
| 5 Theme propagation | ✅ committed (`17b2cd00`) |
| 6 Format verbs on base; delete `addEditorActionForwarded` | ✅ committed (`20abc25a`) |
| 7 contextChanged → heading radio + enable-state | ✅ committed (`d98c7abd`) |
| 8 Ln/Col statusbar all modes | ✅ committed (`6509a0b5` re-pin#2, `ad0729e7` code) |
| **9 Ephemeral + goToLine** | 🟡 **IN PROGRESS — code done, 2 Live test slots failing (uncommitted)** |
| 10 Zoom via base + purge leaf-typed code | ⬜ not started |
| 11 Free-rider verification + full gate | ⬜ not started |
| 12 Docs closeout + push | ⬜ not started |

Corbomite full offscreen suite at last green run (Task 8 complete): **258/258** (excl. `benchmark` label). Markoff fast suite: **267/270** (3 known queue-#10 deterministic fails — not regressions).

---

## ⚠ Two upstream Markoff fixes were made this session (both user-approved deviations)

The plan only anticipated **one** Markoff change (Task 1). A **second** was needed and approved:

1. **`9a6a6b74`** — Task 1, planned: normalize `![[…]]` wiki-embeds matched by the `image` grammar rule (`extractLinkFromNode`). Flipped `tst_metadataparser` green.
2. **`23c36aac`** — Task 8, **UNPLANNED, user-approved**: `markoff-source` + `markoff-styled` never emitted the base `MarkdownView::cursorPositionChanged(int,int)` (only `markoff-live` did), despite their CLAUDE.md contract tables claiming they did. Without it Ln/Col only worked in Live mode. Added a constructor-level connect in each leaf's `Editor.cpp` + falsifiable slots in `tst_view_contract_{source,styled}.cpp`.

**➡ The final submodule pin is `23c36aac`, NOT `9a6a6b74`.** Corbomite re-pinned **twice** (`08098ca2` then `6509a0b5`). Task 12 docs (`<REPIN_SHA>`) must cite **`23c36aac`**, and note the cursor-emit fix as a second roadmap deviation alongside Task 1's embed fix.

Markoff `master` is pushed to Codeberg and clean (`## master...origin/master`, no ahead/behind).

---

## Hard rules (carried from the plan — do not violate)

- **NEVER `git add -A` in Corbomite.** `testvaults/` has 6 deliberately-modified files + `testvaults/films-vault/Untitled.md` (triage evidence). Stage every commit by explicit path. Verified still dirty as of this handoff.
- **Leaf-agnosticism (user directive 2026-06-10):** all consumer ops dispatch through `Markoff::MarkdownView*`. After Task 10, `MainWindow.cpp` must have **zero** `markoff/{live,source,styled}` includes and zero `Markoff::{Live,Source,Styled}::` mentions (Task 10 Step 3 grep gate). Leaf-typed code is allowed only in `NoteEditorWidget` at construction sites, each marked `// leaf-specific:`.
- **Naming trap:** Corbomite's own `MarkdownView` (returned by `MainWindow::activeMarkdownView()`) is NOT `Markoff::MarkdownView`. The Markoff leaf is `activeEditor()->activeLeaf()`.
- The plan's `~line` numbers are stale (verified against Markoff `b6ae2c0f`); locate every edit site by content.

---

## Task 9 — exact uncommitted state

**Uncommitted modified files (all Task 9):**
- `src/editor/NoteEditorWidget.cpp` — `leafFor()` added + `activeLeaf()` delegates to it; `captureEphemeralStateFor`/`restoreEphemeralStateFor` implemented (CursorPos + 0.0–1.0 scroll fraction via base); `goToLine` rewritten to base dispatch; `#include <algorithm>` added.
- `src/editor/NoteEditorWidget.h` — `leafFor(ViewMode)` decl added; `goToLine` doc comment updated to contract-v2.
- `src/app/MainWindow.cpp` — template cursor-marker site (~line 2666) now calls `editor->goToLine(line + 1)`.
- `libs/storage/include/corbomite/storage/EphemeralState.h` — `scroll` comment updated to "0.0–1.0 fraction (contract v2)".
- `tests/editor/tst_note_editor_widget_ephemeral.cpp` — fully rewritten to contract-v2.
- `tests/editor/CMakeLists.txt` — re-enabled the `tst_note_editor_widget_ephemeral` target (removed its `if(FALSE)`/`endif()`; added `markoff_styled` + `Corbomite::Core` to its link libs).

Production code is **complete and believed correct**. The plan's Task 9 commit stages exactly these 6 files.

### The blocker: 2 Live-mode test slots fail

`ctest -R tst_note_editor_widget_ephemeral` → **5 passed, 2 failed**:
- ✅ `sourceModeRoundTrip`, `readingModeRoundTrip`, `cursorSurvivesModeSwitch`
- ❌ `liveModeRoundTrip` — `saved.cursor.line` is **1**, expected **7** (after `leaf->setCursorPosition({7,3})`)
- ❌ `goToLineAllModes` — on the **LivePreview** iteration, `cursorPosition().line` is **1**, expected **5**

**Root cause (TEST issue, not a production bug):** the Live (QML) leaf's `setCursorPosition` is **asynchronous** — it routes through `LiveCursorState::requestTextCaretAtRow`, which only resolves when the target delegate registers (scene/layout-dependent; see `libs/markoff-family/libs/markoff-live/src/EditorWidget.cpp:168` and the leaf's CLAUDE.md cursor-delivery notes). In headless `offscreen` with a freshly-attached doc and only `qWait(20)` + no scene warmup, the request never resolves, so `cursorPosition()` returns the default line 1. The two QWidget leaves (Source, Styled/Reading) are synchronous and round-trip **exactly**, which is why they pass.

The now-deleted pre-port `tst_note_editor_widget_mode_transition` handled this with a `waitForLiveScene()` helper + a documented **±3 line tolerance** for Live specifically.

### ⚠ ADDENDUM (2026-06-10, later session) — diagnosis refined; both options below are REFUTED

Empirical follow-up overturned the "headless async-cursor limitation" theory:

1. **`QTRY_COMPARE` polling for 5s does NOT converge** — the cursor stays at line 1
   forever. The write is *dropped*, not delayed, so option 1 below (longer waits)
   cannot work. Option 2 (±3 tolerance) cannot work either: the actual is line **1**
   vs expected 7 — no tolerance covers it.
2. **Re-issuing `setCursorPosition` each poll cycle converges in ~131 ms** — the
   model populates fine in offscreen and the delegate registers; only the one-shot
   request issued during the attach window is lost.
3. **Root cause:** `LiveCursorState::requestTextCaretAtRow` (LiveCursorState.cpp:178)
   early-returns when `expectedRow >= m_model->rowCount()`. The pending-focus queue
   (`m_pendingFocus`/`tryResolvePending`) only covers *delegate-not-yet-registered*,
   not *model-not-yet-populated*. `LiveListModelBinding::setDocument` never syncs the
   model at attach — it waits for `documentLoaded` (only fires for loads *after*
   attach) or the debounced `d2DocumentChanged` (next event-loop spin). So any
   base-contract write issued in the same call stack as document attach is silently
   discarded. **`setScrollPositionVisualLine` drops identically** (markoff-live
   EditorWidget.cpp:459 — computes against a still-zero `contentHeight`).
4. **This is a production-relevant contract violation, not a test artifact.** The
   adoption brief's own restore recipe (§ ephemeral: attach → `setCursorPosition` →
   `setScrollPositionVisualLine`, same stack) loses both writes on the Live leaf —
   in windowed mode too (it's event-loop timing, not rendering). Phase 3 session
   restore will hit this the moment it is wired. Source/Styled honor the same calls
   synchronously, so the base contract is non-uniform across leaves — exactly what
   the leaf-agnosticism directive exists to prevent. Upstream's
   `tst_view_contract_live` passes only because it warms up (waits for rowCount +
   delegate registration) before issuing writes — the attach window is untested.

Current paths in production are believed safe by accident: mode-switch restores run
against long-attached docs (model populated), and the template-marker `goToLine`
(MainWindow.cpp:2664-2668) likely survives because `requestTextCaretAtRow` calls
`flushPendingDocumentChanges()`, which picks up the debounce still pending from the
just-executed `setMarkdown` (verify in Task 11 manual smoke).

**RESOLVED (same day, user-approved upstream fix #3):** the true mechanism was the
LiveView.qml initial-focus seed (`onCountChanged → requestTextCaretAtRow(0,0)`)
firing one frame after model population and clobbering the consumer's pending caret
request (the model populates synchronously at attach via `EditorWidget::setDocument`'s
`flushPendingD2Changed` — the empty-model guard was a red herring at the leaf level).
Scroll was a true drop (zero `contentHeight` fallback). Upstream fix `8112833f`:
seed yields to an explicit consumer write (`initialCaretRequested` flag) + scroll
fraction latched and applied on `contentHeightChanged`; falsifiable suite
`tst_view_contract_live_attach_window` (RED→GREEN). Corbomite re-pinned (`9043135a`),
Task 9 committed (`5d7fcc5e`), suite 259/259. **Task 12 must cite `<REPIN_SHA>` =
`8112833f` and THREE upstream deviations.**

### ~~Recommended next step for Task 9~~ (superseded by the addendum above)

This is the **third** Live-in-headless-tests tolerance issue this phase (the first two were resolved by user decision: re-enable+rewrite the mode-transition file; upstream cursor-emit fix). Suggested resolution, in order of preference:
1. **Try to make Live land exactly first:** add scene warmup before asserting — e.g. `show()` + `qWaitForWindowExposed` are already there; try waiting on the leaf's own `cursorPositionChanged` via `QSignalSpy`/`QTRY_VERIFY`, or `QTRY_COMPARE(leaf->cursorPosition().line, 7)` (longer implicit wait) instead of a fixed `qWait(20)`. If Live then resolves, keep the assertions exact for all three modes.
2. **If Live still won't resolve exactly in offscreen:** make the Live path tolerant — split `roundTripInMode` / `goToLineAllModes` so Source+Reading stay exact (`QCOMPARE`) and Live uses a tolerance (`±3`, matching the retired test) with a comment citing this async limitation. Do **not** weaken the Source/Reading assertions.

Production code should **not** change for this — it's correct (Source/Reading prove the base dispatch works; Live's async cursor model is exercised by Markoff's own `tst_view_contract_live` upstream). If you find a genuine production defect, that's a different (and reportable) finding.

After the 2 slots are green: run the full offscreen suite (`ctest -E benchmark`), expect 100%, then commit Task 9 (the 6 files above, by explicit path).

---

## Remaining tasks 10–12 (gotchas)

- **Task 10 (zoom + purge):** when removing the `markoff/{live,source}` includes from `MainWindow.cpp`, you **must add `#include <markoff/core/MarkdownView.h>` directly** — MainWindow currently gets the complete `Markoff::MarkdownView` type only transitively via those leaf headers (it calls `leaf->undo()/setFontScale()/toggle*()` on the base). Delete the `liveActionControllerFor` helper (anon namespace ~line 209). Run the Step-3 grep gate; expect zero hits. Then add `// leaf-specific:` markers in `NoteEditorWidget.cpp` (Task 10 Step 5).
- **Task 11:** full offscreen suite must be 100% (excl. `benchmark`). Manual smoke is best-effort/headless-limited — report what couldn't be verified to the user's dogfood list. Use a **copy** of a test vault, never the live `testvaults/` evidence files.
- **Task 12 (docs closeout):**
  - Use **`<REPIN_SHA>` = `23c36aac`** everywhere.
  - **Commit the untracked plan file** `docs/superpowers/plans/2026-06-10-phase1-markoff-repin-contract-v2.md` (and you may commit this handoff too).
  - road-to-dogfood Phase 1 status line: note **two** roadmap deviations landed upstream this phase — the `![[…]]` embed fix (Task 1) **and** the Source/Styled `cursorPositionChanged` emit (Task 8).
  - Update `CLAUDE.md` banner + Testing baseline (was "250/251, lone red tst_metadataparser" — now 258/258, no red; that note is obsolete).
  - punch-list: add the tracked item about `eState.scroll` storing a 0.0–1.0 fraction (Phase 3 workspace-fidelity revisit), per Task 12 Step 2.
  - `git push origin master` for Corbomite; Markoff already pushed — just verify. Final check: `testvaults/` still shows `M`/`??`.

---

## Build / test commands

```bash
# Corbomite
cd ~/dev/Corbomite && cmake --build --preset dev -j 10
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10

# Markoff
cd ~/dev/Markoff && cmake --build build-dev -j 8 && scripts/run-tests.sh -E 'tst_realistic|tst_benchmark'
```

Offscreen is mandatory for Corbomite GUI tests. `tst_forcelayout` is **flaky/nondeterministic** (forcegraph, zero Corbomite deps) — re-run once before treating a failure as real; it passed on re-run this session.
