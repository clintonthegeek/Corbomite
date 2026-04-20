# Cluster G Part 3 — Split semantics + active-pane focus routing — Implementation Plan

> **Scope:** P0 fix derived from the 2026-04-18 Workspace/View audit (see `cluster-retros/cluster-g.md §Follow-ups surfaced 2026-04-18` and `PROJECT-STATE.md §Cluster G follow-ups`). Narrow, single-session task.

**Goal:** Fix "Split Right leaves an empty pane" by bringing the user-facing split command into line with Obsidian's `duplicateLeaf` semantics (clone view state into a new leaf in the new split), add focus-based active-leaf routing so each split pane has usable focus, and fix the first-render blank-screen that forces a vault reopen to see the split.

**Architecture:** `Workspace::splitLeaf` stays as the low-level tree primitive (wraps parent tabs in a new split + inserts an empty sibling tabs). A new `Workspace::duplicateLeaf(leaf, direction)` layers the user-facing semantics on top: snapshot source leaf state, call `splitLeaf`, create a new leaf in the new tabs, re-apply the snapshot, set the new leaf active. MainWindow's Split Right/Down actions call `duplicateLeaf`. Focus routing uses a QEvent::FocusIn filter installed on the leaf's view widget; filter target is the `WorkspaceLeaf` itself, which calls `Workspace::setActiveLeaf(this)`. First-render fix: explicit `show()` on widgets newly parented into the splitter, plus a geometry-refresh nudge in the `layoutChanged` handler.

**Tech Stack:** C++20, Qt6, libs/core/Workspace.

**Audit references:**
- `docs/obsidian-audit/domains/workspace.md §1` (WorkspaceItem hierarchy, `activeLeaf`, `setActiveLeaf`, `duplicateLeaf`)
- `docs/obsidian-audit/domains/workspace.md §6` (`workspace:split-vertical/horizontal → duplicateLeaf(activeLeaf, direction)`)
- `docs/obsidian-audit/domains/views.md §1` (Leaf ↔ View binding; `getViewState` / `setViewState` / ephemeral state / history)

---

## File Structure

### New files
None.

### Modified files

| Path | What changes |
|---|---|
| `libs/core/include/corbomite/core/Workspace.h` | Add `WorkspaceLeaf *duplicateLeaf(WorkspaceLeaf *, Qt::Orientation);` |
| `libs/core/src/Workspace.cpp` | Implement `duplicateLeaf`. |
| `libs/core/include/corbomite/core/WorkspaceLeaf.h` | Add a private `eventFilter` override for FocusIn routing; add `attachFocusFilter(QWidget *)` helper. |
| `libs/core/src/WorkspaceLeaf.cpp` | Implement filter + install on view widget at `open()` / view swap. |
| `src/app/MainWindow.cpp` | Rebind `split_right` / `split_down` actions to `duplicateLeaf`. Add show/update nudge in `layoutChanged` handler. |
| `tests/core/tst_workspace_integration.cpp` | Add 3 `test_workspace_duplicateLeaf_*` cases. |
| `docs/PROJECT-STATE.md` | Mark P0 done in Cluster G follow-ups, update current focus. |
| `docs/superpowers/plans/INDEX.md` | Add row for this plan. |
| `docs/cluster-retros/cluster-g.md` | Replace the "P0 pending" paragraph with the landed-fix summary. |

---

## Tasks

### Task 1 — Implement `Workspace::duplicateLeaf`

- [ ] Add declaration in `Workspace.h` after `splitLeaf`.
- [ ] Implement in `Workspace.cpp`:
  1. Guard `!leaf || !leaf->parentItem()` → return nullptr.
  2. Snapshot: `QJsonObject state = leaf->getViewState(); QJsonObject eState = leaf->getEphemeralState(); LeafHistory hist = leaf->history(); bool pinned = leaf->pinned(); QString group = leaf->group();`
  3. Call `splitLeaf(leaf, direction)` → `WorkspaceSplit *split`. Return nullptr if it fails.
  4. Locate the newly-added empty `WorkspaceTabs`: it is the last child of `split`. Assert it is empty.
  5. `WorkspaceLeaf *newLeaf = createLeafInTabs(newTabs);`
  6. Apply snapshot: `newLeaf->setViewState(state); newLeaf->setEphemeralState(eState); newLeaf->setPinned(pinned); newLeaf->setGroup(group); newLeaf->history() = hist;`
  7. `setActiveLeaf(newLeaf);`
  8. Emit `layoutChanged()` (splitLeaf already emits, createLeafInTabs emits — acceptable double-emit; MainWindow handler is idempotent).
  9. Return `newLeaf`.
- [ ] Verify build.

**Dispatch mode:** Normal task. Keep small.

### Task 2 — Rewire Split Right / Split Down actions

- [ ] In `src/app/MainWindow.cpp` around lines 789-805, change both action lambdas to call `m_workspace->duplicateLeaf(leaf, direction)` instead of `splitLeaf`.
- [ ] Leave the `WorkspaceController::splitLeaf` proxy alone (it is the low-level primitive; a separate `WorkspaceController::duplicateLeaf` may follow in a later cluster when a plugin asks for it).

### Task 3 — Focus-based active-leaf routing

- [ ] In `WorkspaceLeaf`, install a `QEvent::FocusIn` event filter on the view's top-level widget whenever a view is attached. When the filter triggers, call `Workspace::setActiveLeaf(this)` via a stored `Workspace *` back-pointer *or* emit a new `focusRequested(WorkspaceLeaf *)` signal that Workspace wires once.
- [ ] Prefer signal approach — keeps `WorkspaceLeaf` free of Workspace dependency. Add `void focusRequested(WorkspaceLeaf *leaf)` Q_SIGNALS. Workspace subscribes in `createLeafInTabs` + `deserialize` restore loop + `undoCloseLeaf` + `duplicateLeaf` — or simpler, install once in `createLeafInTabs` and ensure every other leaf-creation site routes through a new helper `registerLeaf(WorkspaceLeaf *)`.
- [ ] Test: create two leaves in a split, programmatically focus the non-active leaf's view widget, verify `activeLeafChanged` fires with that leaf. (If test is fiddly with real widgets, exercise the filter path directly by delivering a synthetic `QFocusEvent` and bypassing the widget tree.)

### Task 4 — Fix first-render blank screen

- [ ] Reproduce via `./build/Corbomite` with `-DCORBOMITE_DEV_BUILD=ON`. Open vault, open note, Split Right, observe.
- [ ] Most-likely culprit: the newly-inserted widgets (the new `WorkspaceSplit`'s inner `QSplitter` and the new `WorkspaceTabs`) are not explicitly shown after `splitLeaf` reparents them, and `syncDimensionsToSplitter` runs before the splitter has geometry allocated.
- [ ] Fix attempt 1: in `WorkspaceSplit::addChild`, after `m_splitter->insertWidget(idx, w)`, explicitly `w->show()`.
- [ ] Fix attempt 2: if still blank, in MainWindow's `layoutChanged` handler, call `m_workspace->mainRoot()->widget()->show()` + `updateGeometry()` after the reparent guard.
- [ ] Verify manually: Split Right *without* reopening the vault now shows both panes, with the source note duplicated on the right (Task 1 ensured this) and each pane focusable (Task 3).

### Task 5 — Unit tests

- [ ] In `tests/core/tst_workspace_integration.cpp` add:
  - `test_workspace_duplicateLeaf_returnsNonNullLeaf` — setup, call duplicateLeaf, assert non-null.
  - `test_workspace_duplicateLeaf_clonesViewState` — set a known viewState on source, call duplicateLeaf, assert `newLeaf->getViewState() == sourceState`.
  - `test_workspace_duplicateLeaf_newLeafIsActive` — assert `ws.activeLeaf() == newLeaf` after the call.
  - `test_workspace_duplicateLeaf_newTabsContainsNewLeaf` — assert the new tabs' childCount is 1 and its only child is newLeaf.
- [ ] Run `cd build && ctest -R workspace_integration --output-on-failure`.

### Task 6 — Build, test, smoke

- [ ] `cmake --build build -j 10`
- [ ] `cd build && ctest --output-on-failure -j 10` — baseline suite must stay green modulo the 4 pre-existing known-flaky + `tst_benchmark_layout` timeout.
- [ ] Launch `./build/Corbomite` with a dev vault, verify Split Right and Split Down visually.

### Task 7 — Docs + commit

- [ ] Commit with conventional-commits message: `fix(workspace): duplicateLeaf semantics + active-pane focus + first-render show (P0 cluster-g follow-up)`.
- [ ] Update `docs/PROJECT-STATE.md` current focus section + strike-through the relevant items in Cluster G follow-ups.
- [ ] Update `docs/superpowers/plans/INDEX.md` with a row for this plan (status Done).
- [ ] Update `docs/cluster-retros/cluster-g.md §Follow-ups surfaced 2026-04-18` to reflect landed state.

---

## Definition of done

- `./build/Corbomite` with a dev vault: Split Right opens a second pane showing the same note as the source; clicking into either pane sets it active; subsequent file-open targets the focused pane; vault reopen round-trips the split with both panes populated.
- Full test suite green modulo the documented pre-existing flakes.
- Plan cross-linked from INDEX.md + PROJECT-STATE; retro updated.

## Blocks / enables

- **Blocks nothing** — this is a bug fix on top of shipped Cluster G.
- **Enables** faithful `Workspace::openLinkText` routing (Cluster G follow-up #3) once that lands, because routing needs a reliable "focused pane" concept.

## Preserved compat quirks

- `Workspace::splitLeaf` remains the low-level primitive so `WorkspaceController::splitLeaf` (plugin-facing) stays backwards-compatible.
- Persistence round-trip unchanged (both halves of the split now persist a populated leaf, which was always the format — we just weren't producing it).
