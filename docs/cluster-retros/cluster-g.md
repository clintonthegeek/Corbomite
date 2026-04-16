# Cluster G — Views hierarchy + TextFileView contract (retrospective)

**Landed:** Part 1 on 2026-04-15 (15 commits). Part 2 infrastructure on 2026-04-15 (11 commits). Part 2 Tasks 9-10 (MainWindow migration + legacy-class deletion) on 2026-04-16 (3 new commits this session on top of pre-existing delete `e3143f1`).

**Phase commits (final-session only — see earlier PROJECT-STATE entries for Part 1 and Part 2 infrastructure):**
- Task 9 — `9cdcdf0` (MainWindow onto Workspace) + `c248ebf` (double-nest persistence fix) + `4c63219` (property-name disambiguation + missing-key guard).
- Task 10 — `e3143f1` (pre-existing; ancestor of HEAD). Current session verified disk state + build + zero remaining active references.

## What changed vs the original plan

Largely faithful. Three notable adjustments during Task 9 integration:

1. **Workspace persistence routed through SessionManager, not via `Workspace::readWorkspaceJson`/`writeWorkspaceJson` directly.** The plan called for MainWindow to invoke those methods at vault open/close. `SessionManager` already owns `.obsidian/workspace.json` (from Cluster B Phase 3b) and round-trips Obsidian-unknown keys. Rather than build a second persistence path, the implementation routes `Workspace::serialize()` / `deserialize()` through SessionManager's existing `setWorkspaceLayout` / `workspaceLayout` APIs. Functionally equivalent; avoids two code paths writing the same file.

2. **`m_workspaceContainer` QWidget wrapper around `Workspace::mainRoot()->widget()` instead of a direct `setCentralWidget(...)`.** MainWindow already used a `QStackedWidget` (welcome screen ↔ editor) as its central widget. The container provides a stable stack slot so `Workspace::layoutChanged` restructurings don't have to touch the stack index.

3. **Property-name disambiguation for signal-dedup guards.** Code-quality review caught that both `WorkspaceTabs` and `WorkspaceLeaf` used the same `_mw_connected` property key to prevent duplicate signal connections — a silent aliasing hazard. Renamed to `_mw_tabs_connected` / `_mw_leaf_connected` before closure.

## What surprised

- **Task 10 was already partially done in `e3143f1`.** A prior session had bundled the legacy-class deletion into a "feat+delete" commit before Task 9's MainWindow migration fully landed. When the current session dispatched Task 10, the implementer agent verified the 8 target files were already gone on disk, all three CMakeLists.txt files cleaned, and every remaining reference was comment-only — no new commit needed. A good reminder that pre-existing in-flight state must be verified against the plan before dispatching mechanical work.

- **The `saveSessionState` double-nesting bug.** Spec review caught that `Workspace::serialize()` returns `{main: {...}, active: "...", lastOpenFiles: [...]}` but `SessionManager::setWorkspaceLayout` stored its input under `root["main"]`, producing `{main: {main: {...}, ...}, ...}` on disk. After the first Corbomite-originated save, restore silently failed (received `{main:{split tree}, active:..., lastOpenFiles:[...]}` where a bare split node was expected; `deserializeNode` returned null). Fixed with a one-line extraction (`wsJson["main"].toObject()`) before the set call.

- **`lastOpenFiles` sibling key silently dropped across restore.** Noticed during the fix: SessionManager's load path reconstructs `fullWs["main"] = wsLayout` but never restores the sibling `lastOpenFiles` array — it goes into the unknown-key passthrough bucket on save (correct) but isn't re-fed into `Workspace::deserialize` on load. Pre-existing gap, not introduced by this task; deferred.

## Downstream effects

- **Cluster M (Internal-plugin wrapping) unblocked.** The brainstorm + plan for wrapping built-in features (FileExplorer / Search / Backlinks / Outlinks / Outline / Properties / LocalGraph) in an `InternalPlugin` `Component` wrapper + `core-plugins.json` persistence + Settings toggle page can now proceed. The user's permissions-system design input is already captured in `memory/project_cluster_m_permissions.md`.

- **Cluster N (Plugin-ready surfaces) unblocked on the workspace-leaf front.** Community plugins that expect `workspace.getLeavesOfType("outline")` or similar can now walk a real `Workspace` tree.

- **`workspace.json` round-trip for the editor area now real.** Tab layout, splits, and open files persist across sessions through native `Workspace` serialization + SessionManager's unknown-key preservation. KateMDI's `ToolView` pattern continues to own the sidebar panels (deliberately not migrated — see Cluster M design discussion).

## Lessons for the next cluster

- **Verify pre-existing work before dispatching mechanical tasks.** Both Task 9 and Task 10 had substantial portions already in the working tree or in prior commits. The implementer agents correctly caught this by reading the tree first. Future ritual: an implementer dispatch for a mechanical task should start with "verify current state vs plan expectation" before applying any edits.

- **Spec-compliance review is where persistence-shape bugs live.** The double-nesting bug wasn't visible from reading either commit in isolation — it required tracing `serialize()` → `setWorkspaceLayout` → `doSave` → on-disk shape → `load` → `deserialize` end-to-end. Spec reviewers explicitly tasked with "verify claim X holds" catch these; reviewers only checking "does the file compile and tests pass" don't. Preserve this discipline through Cluster M.

- **Legacy `EditorViewManager`/`EditorViewSpace`/`PaneLayoutBridge`/`PaneLayout` cleanup was the easy part.** The hard part was threading every service (hover popover, suggest manager, vault model, graph controls, metadata cache, link/cursor signals, view-mode) that EditorViewManager implicitly hosted through the new `Workspace::layoutChanged` → per-leaf `WorkspaceLeaf::viewChanged` signal path. `propagateServicesToView` absorbed this; it's the natural extension point for any new service in the future.
