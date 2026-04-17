# Spec-Driven Test Audit — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Systematically find bugs by writing tests derived from *what the docs say was implemented* — not from what the code does. A naive agent reads specs first, builds expectations, then writes tests that challenge the implementation.

**Architecture:** Three-phase pipeline per cluster: (1) extract behavioral claims from docs, (2) check if a test covers each claim, (3) write a test for every uncovered claim. Tests are written *before* reading the implementation — they encode spec intent, not code behavior. Failures are investigated: either the test misunderstands the spec, or the code has a bug.

**Tech Stack:** C++20, Qt6, QTest, cmake/ctest

**Critical rule for the executing agent:** You must NOT read the implementation source files (`.cpp` under `src/` or `libs/*/src/`) until AFTER you have written the test. You may read headers (`.h`) to learn the public API. This prevents tests from becoming tautological.

**Do NOT read existing test files** in `tests/` before writing your tests. Existing tests were written alongside the implementation and may have absorbed implementation assumptions. Your tests must come from the spec alone.

---

## How to build & run tests

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON
cmake --build build
cd build && ctest --output-on-failure
```

Run a single test:
```bash
cd build && ctest -R tst_name --output-on-failure
```

Test files live under `tests/` mirroring the lib structure. Each test is a standalone executable using `QTEST_MAIN`. GUI tests need `QT_QPA_PLATFORM=offscreen` (set in CMakeLists).

---

## File Structure

### New test files (one per task)

| File | Tests |
|---|---|
| `tests/core/tst_workspace_integration.cpp` | Task 1: Workspace tree ↔ widget hierarchy integration |
| `tests/core/tst_workspace_session.cpp` | Task 2: workspace.json end-to-end persistence |
| `tests/core/tst_workspace_tabs_lifecycle.cpp` | Task 3: Tab lifecycle (open, close, reorder, deferred load) |
| `tests/core/tst_leaf_service_propagation.cpp` | Task 4: Service propagation to views |
| `tests/core/tst_metadatacache_signals.cpp` | Task 5: MetadataCache signal contract |
| `tests/core/tst_fileview_setState.cpp` | Task 6: FileView::setState file-loading contract |
| `tests/storage/tst_session_manager_roundtrip.cpp` | Task 7: SessionManager ↔ Workspace roundtrip |
| `tests/core/tst_workspace_leaf_navigate.cpp` | Task 8: Leaf navigation (navigate/goBack/goForward) |

### Modified files

| File | Changes |
|---|---|
| `tests/core/CMakeLists.txt` | Add new test executables |
| `tests/storage/CMakeLists.txt` | Add new test executable |

---

## Per-task workflow

Every task follows the same steps:

- [ ] **Step 1: Read the spec sections and headers listed.** Extract every behavioral claim — things the spec says MUST happen, SHOULD happen, or describes as the contract. Write them down as a checklist of assertions.

- [ ] **Step 2: Write the test file.** For each behavioral claim, write one or more QTest test functions that verify the claim. Create stub/mock views as needed (subclass `View` with minimal overrides). Use `QSignalSpy` for signal verification. Do NOT read `.cpp` implementation files.

- [ ] **Step 3: Add to CMakeLists.** Follow the existing pattern in `tests/core/CMakeLists.txt`:
  ```cmake
  add_executable(tst_name tst_name.cpp)
  add_test(NAME tst_name COMMAND tst_name)
  target_link_libraries(tst_name PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
  set_tests_properties(tst_name PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  ```
  For storage tests, link `Corbomite::Storage` instead/additionally.

- [ ] **Step 4: Build and run.** Fix compile errors (you may now read headers more carefully). Once it compiles, run the test. **Do not change the test assertions to match implementation behavior.** If a test fails, investigate: read the `.cpp` now to determine whether the test misunderstands the spec or the code has a bug. Document findings in the commit message.

- [ ] **Step 5: Commit.**

---

## Task 1: Workspace Tree ↔ Widget Hierarchy Integration

**Read these docs:**
- `docs/superpowers/specs/2026-04-15-cluster-g-part2-workspace-design.md` §2 "Workspace tree" and §3 "Widget hierarchy"

**Read these headers (public API only):**
- `libs/core/include/corbomite/core/Workspace.h`
- `libs/core/include/corbomite/core/WorkspaceSplit.h`
- `libs/core/include/corbomite/core/WorkspaceTabs.h`
- `libs/core/include/corbomite/core/WorkspaceLeaf.h`
- `libs/core/include/corbomite/core/WorkspaceParent.h`
- `libs/core/include/corbomite/core/ViewRegistry.h`
- `libs/core/include/corbomite/core/View.h`

**What the spec claims (verify all of these):**
- WorkspaceSplit owns a QSplitter; children's widgets are inserted into it
- WorkspaceTabs owns a QTabBar + QStackedWidget; leaf widgets go into the stack
- The widget tree mirrors the workspace object tree
- Splitting a leaf wraps its parent tabs in a new split
- After `addChild`, the child's `widget()` is parented to the parent's `widget()`
- After `removeChild`, the child's widget is unparented
- Default workspace layout has one WorkspaceTabs with zero leaves
- `createLeafInTabs` adds a leaf to the tree; `closeLeaf` removes it

**Output:** `tests/core/tst_workspace_integration.cpp`

---

## Task 2: workspace.json End-to-End Persistence

**Read these docs:**
- `docs/superpowers/specs/2026-04-15-cluster-g-part2-workspace-design.md` §6 "Persistence"

**Read these headers:**
- `libs/core/include/corbomite/core/Workspace.h`
- `libs/core/include/corbomite/core/WorkspaceLeaf.h`
- `libs/core/include/corbomite/core/ViewRegistry.h`
- `libs/core/include/corbomite/core/View.h`

**What the spec claims (verify all of these):**
- `Workspace::serialize()` produces JSON with `main`, `active`, `lastOpenFiles`
- `Workspace::deserialize()` rebuilds the tree from that JSON
- Non-active leaves are marked deferred after deserialization (view not constructed)
- Active leaf is loaded eagerly (not deferred)
- Round-trip: serialize → deserialize produces equivalent tree (same leaf count, same active leaf)
- `writeWorkspaceJson` / `readWorkspaceJson` hit disk at `<vault>/.obsidian/workspace.json`

**Output:** `tests/core/tst_workspace_session.cpp`

---

## Task 3: Tab Lifecycle (Open, Close, Reorder, Deferred Load)

**Read these docs:**
- `docs/superpowers/specs/2026-04-15-cluster-g-part2-workspace-design.md` §4 "WorkspaceTabs"

**Read these headers:**
- `libs/core/include/corbomite/core/WorkspaceTabs.h`
- `libs/core/include/corbomite/core/WorkspaceLeaf.h`
- `libs/core/include/corbomite/core/View.h`
- `libs/core/include/corbomite/core/ViewRegistry.h`

**What the spec claims (verify all of these):**
- Tab titles come from `View::getDisplayText()`
- Tab icons come from `View::getIcon()`
- Closing a tab (removeChild) removes the leaf and reduces tab count
- Switching tabs calls `loadIfDeferred()` for deferred leaves
- `setCurrentTab(index)` updates tab bar and emits `currentTabChanged`
- Multiple tabs track correct insertion order via `leafAt()`
- Tab title updates when the view changes (viewChanged signal propagation)

**Output:** `tests/core/tst_workspace_tabs_lifecycle.cpp`

---

## Task 4: Service Propagation to Views

**Read these docs:**
- `docs/superpowers/plans/2026-04-14-cluster-h-menus-hover-suggester-ui.md` §"Phase 2"
- `docs/superpowers/specs/2026-04-15-cluster-g-part2-workspace-design.md` (service propagation sections)

**Read these headers:**
- `libs/core/include/corbomite/core/WorkspaceLeaf.h`
- `libs/core/include/corbomite/core/ViewRegistry.h`
- `libs/core/include/corbomite/core/View.h`

**What the spec claims (verify all of these):**
- `WorkspaceLeaf::viewChanged(View*)` fires when `open()` is called
- `viewChanged` fires when `setViewState()` creates a new view
- `viewChanged` fires when a deferred leaf is loaded via `loadIfDeferred()`
- `WorkspaceLeaf::registry()` returns the ViewRegistry it was constructed with
- The viewChanged signal is the hook point for service propagation — MainWindow connects to it to inject services into each new view

**Output:** `tests/core/tst_leaf_service_propagation.cpp`

---

## Task 5: MetadataCache Signal Contract

**Read these docs:**
- `docs/superpowers/plans/2026-04-15-cluster-i-metadatacache-parity.md` §"Phase 4: Signal-contract parity"

**Read these headers:**
- `libs/storage/include/corbomite/storage/MetadataCache.h`
- `libs/storage/include/corbomite/storage/LinkResolver.h`
- `libs/storage/include/corbomite/storage/CachedMetadata.h`

**What the spec claims (verify all of these):**
- MetadataCache emits 5 distinct signals: `cacheChanged`, `cacheDeleted`, `linksResolvedFor`, `allLinksResolved`, `indexFinished`
- `cacheChanged(path, prevHash, cache)` fires after a file is parsed
- `cacheDeleted(path, prevCache)` fires after a file is deleted from the cache
- `indexFinished()` fires after work drains through the link-resolver debounce
- Short-circuit paths (stat unchanged, hash unchanged) are silent — no signals fire
- Signals coalesce during batch operations (debounced `indexFinished`)

**Note:** MetadataCache requires a `LinkResolver` to construct. Check the LinkResolver header for its constructor and `setVaultPaths()` method.

**Note:** MetadataCache uses a background worker thread. Signals may fire asynchronously. Use `QTRY_*` macros or `QTest::qWait()` + event loop pumping to wait for async signals. Check existing test patterns in `tests/storage/` for examples of how to handle this if your tests time out.

**Output:** `tests/core/tst_metadatacache_signals.cpp` (link against `Corbomite::Storage`)

---

## Task 6: FileView::setState File-Loading Contract

**Read these docs:**
- `docs/superpowers/specs/2026-04-15-cluster-g-views-hierarchy-design.md` §"FileView"

**Read these headers:**
- `libs/core/include/corbomite/core/FileView.h`
- `libs/core/include/corbomite/core/ItemView.h`
- `libs/core/include/corbomite/core/View.h`
- `libs/core/include/corbomite/core/WorkspaceLeaf.h`
- `libs/core/include/corbomite/core/ViewRegistry.h`
- `libs/core/include/corbomite/core/NoteDocument.h`

**What the spec claims (verify all of these):**
- `FileView::setState({"file": "path.md"})` resolves the file via the ViewRegistry's FileResolver and calls `loadFile()`
- After successful setState, `FileView::file()` returns the loaded NoteDocument
- `getDisplayText()` returns the file name (from `NoteDocument::name()`) after a file is loaded
- setState with no `"file"` key or an unresolvable path does not crash
- setState with no FileResolver set does not crash

**Note:** `FileView` is abstract (extends `ItemView` which extends `View`). You'll need a concrete subclass to test it. `NoteDocument` constructor is `NoteDocument(vaultRoot, relativePath, parent)` — there is no `setRelativePath()`.

**Output:** `tests/core/tst_fileview_setState.cpp`

---

## Task 7: SessionManager ↔ Workspace Roundtrip

**Read these docs:**
- `docs/superpowers/specs/2026-04-01-vault-session-management-design.md`

**Read these headers:**
- `src/app/SessionManager.h`

**What the spec claims (verify all of these):**
- SessionManager stores window geometry, sidebar state, workspace layout, active leaf ID, expanded folders
- workspace.json preserves unknown Obsidian keys on round-trip
- `_corbomite` namespace holds Corbomite-specific state
- Loading a session then saving produces equivalent JSON for known keys
- Each granular setter (saveWindowGeometry, saveSidebarState, saveExpandedFolders, setWorkspaceLayout) round-trips through saveNow → load

**Note:** SessionManager is in `src/app/`. Check how existing tests link against app code — you may need to link against additional targets or include the source file directly. Look at `tests/` CMakeLists files and the top-level `src/app/CMakeLists.txt` for guidance.

**Output:** `tests/storage/tst_session_manager_roundtrip.cpp`

---

## Task 8: Leaf Navigation (navigate/goBack/goForward)

**Read these docs:**
- `docs/superpowers/specs/2026-04-15-cluster-g-part2-workspace-design.md` §5 "LeafHistory"

**Read these headers:**
- `libs/core/include/corbomite/core/WorkspaceLeaf.h`
- `libs/core/include/corbomite/core/LeafHistory.h`
- `libs/core/include/corbomite/core/ViewRegistry.h`
- `libs/core/include/corbomite/core/View.h`

**What the spec claims (verify all of these):**
- `navigate(viewState)` pushes current state to history, then loads new state
- `goBack()` restores previous state from history
- `goForward()` re-navigates to a state you went back from
- History is capped at 20 entries (`LeafHistory::Cap`)
- `canGoBack()` / `canGoForward()` reflect history state accurately
- `goBack()` with no history is a no-op (does not crash)
- After navigate → goBack → goForward, the view state matches the original forward state

**Output:** `tests/core/tst_workspace_leaf_navigate.cpp`

---

## Self-Review Checklist

After all 8 tasks are complete:

- [ ] All 8 test files compile and run
- [ ] Document any spec/implementation divergences found (bugs or spec inaccuracies)
- [ ] Each commit message notes whether all tests passed or which failures were found
- [ ] No test was modified to match implementation behavior — failures are either spec misunderstandings (documented) or real bugs (filed/fixed)
