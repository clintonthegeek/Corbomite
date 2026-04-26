# Cluster U — File Explorer enhancements (SCOUTING)

**Type:** Scouting doc (not yet dispatchable). Expand to a full plan once the blockers listed below are closed and the user confirms scope.

**Motivation.** Cluster R's 2026-04-19 closeout audit flagged File Explorer right-click as the #1 UX gap vs Obsidian. Obsidian users expect to rename / move / delete / duplicate / create from the Files panel via right-click *and* via F2 / Delete / Enter / Ctrl-click keyboard. Corbomite's File Explorer (`src/plugins/file-explorer/FileExplorerView.*`) currently ships a plain `QTreeView` with no context menu on file / folder rows — users must use the pane hamburger or the command palette.

This cluster brings Corbomite's file browser to Obsidian UX parity while reusing the primitives Cluster R already shipped.

---

## Audit references

- [`../../obsidian-audit/addenda/2026-04-19-file-explorer-context-menu.md`](../../obsidian-audit/addenda/2026-04-19-file-explorer-context-menu.md) — canonical reference. Enumerates file-row / folder-row / empty-area menu items, keyboard exposure, and Obsidian's internal construction path.
- [`../../obsidian-audit/domains/ui-bundle.md`](../../obsidian-audit/domains/ui-bundle.md) §File Explorer — panel rendering (sort, drag-reorder, collapse state).
- [`../../obsidian-audit/domains/leaf-utilities.md`](../../obsidian-audit/domains/leaf-utilities.md) — canonical section order (reused verbatim).
- [`../../cluster-retros/cluster-r.md`](../../cluster-retros/cluster-r.md) §Follow-ups #1 — motivation + primitive inventory.

---

## Scope (rough phasing — revisit on plan expansion)

### Phase 1 — Right-click context menu substrate

- Wire `FileExplorerView::contextMenuEvent(QContextMenuEvent*)` to construct a `QMenu` via `MenuSectionHelper` (same substrate as Cluster R hamburger).
- Distinguish file-row / folder-row / empty-area menus by inspecting the `QModelIndex` at the click position.
- Emit `MenuEventEmitter::fileMenu(menu, file, "file-explorer-context-menu")` *before* `helper.finalize()` — this is the right moment to also fix Cluster R follow-up #2 (plugin injection ordering) so File Explorer and hamburger share one correct injection protocol.

### Phase 2 — File-row items

Reuse Cluster R primitives verbatim — no new modals:

- Open in new tab / Open to the right / Open below (existing `WorkspaceController::openFileInLeaf` with direction param).
- Open in new window (disabled placeholder → Cluster G#6, same as hamburger).
- Make a copy → new `FileManager::duplicateFile(TFile*)` with " 1" suffix collision resolution (closest analogue already in `FileManager::getNewFileParent` collision logic).
- Rename… → `FileManager::promptForFileRename` (existing).
- Move to another folder… → `FileManager::promptForMove` (existing).
- Bookmark… → disabled placeholder → Cluster S (same as hamburger).
- Copy link to file → `PathUtils::wikiLinkFor(TFile*)` (new helper, trivial).
- info.copy submenu: Copy Obsidian URL / Copy vault path / Copy absolute path → reuse `PathUtils::obsidianUrlFor` / existing path accessors.
- system section: Open in default app / Show in system explorer → `Platform::openWithDefaultApp` / `Platform::showInFolder` (existing).
- danger section: Delete → `FileManager::promptForDeletion` (existing).

### Phase 3 — Folder-row items

- New note / New canvas / New base / New folder inside folder → new `FileManager::createMarkdownNoteIn(TFolder*)` / `createCanvasIn` / `createBaseIn` / `createFolderIn`. Collision-resolving suffix logic exists; needs folder-scoped variants.
- Rename… / Move to… / Duplicate folder / Delete → need `TFolder*`-accepting modals. Either overload (`promptForFolderRename`, `promptForFolderDeletion`) or promote modals to take `TAbstractFile*` and downcast. Folder delete modal must count contained files and show the count in the confirm message.
- Search in folder… → dispatch search plugin's `:open` command with a pre-filled `path:` scope filter (depends on search plugin accepting a pre-fill; likely already supported).
- view section: Collapse all / Expand all → native `QTreeView::collapseAll()` / `expandAll()`; persist via existing `SessionManager::setPluginSessionState`.

### Phase 4 — Empty-area menu

Reduced menu (5 items): New note / New canvas / New base / New folder (at vault root), Collapse all folders. Trivial once Phase 3's factories exist.

### Phase 5 — Keyboard exposure

- `F2` → inline rename (turn row label into `QLineEdit`, validate via `FileNameValidator`, commit to `FileManager::renameFile`). Coexists with hamburger's modal rename path.
- `Delete` → `FileManager::promptForDeletion` on focused row.
- `Enter` → open in current tab.
- `Ctrl/Cmd+Click` → open in new tab.
- `Shift+Click` → open in new split.
- `Ctrl/Cmd+A` → intentionally no-op (multi-select delete is a post-MVP item; Obsidian ships it but it's a separate feature).

### Phase 6 — Cluster H follow-up #2 residue

Cluster R's retro flagged three menu construction sites still using raw `QMenu` instead of `MenuSectionHelper`: **EditorViewSpace tab bar**, **TextControl**, **CorbomiteMDI Sidebar**. Cluster U should audit whether any of these share code paths with File Explorer's context menu. If they do, migrate them in the same phase. If not, carry as a standalone Cluster H residue follow-up.

---

## Primitive inventory (mostly already shipped)

**Reused from Cluster R (zero new work):**

- `FileManager::promptForFileRename`, `promptForMove`, `promptForDeletion`, `FileNameValidator`.
- `Platform::openWithDefaultApp`, `showInFolder`.
- `PathUtils::obsidianUrlFor`, `corbomiteUrlFor`.
- `MenuSectionHelper` canonical section order + `addSubmenu`.
- `MenuEventEmitter::fileMenu` (event type exists; emission-timing fix from Cluster R follow-up #2 applies here).

**New primitives needed:**

- `FileManager::duplicateFile(TFile*)` + `duplicateFolder(TFolder*)`.
- `TFolder*` variants (or `TAbstractFile*` base) for rename / move / delete modals.
- Folder-scoped creation: `createMarkdownNoteIn(TFolder*)`, `createCanvasIn`, `createBaseIn`, `createFolderIn` — or a single `createIn(TFolder*, FileKind)` dispatcher.
- `PathUtils::wikiLinkFor(TFile*)` (one-line helper wrapping `FileManager::generateMarkdownLink`).
- `FileExplorerView::beginInlineRename(QModelIndex)` — `QLineEdit`-as-editor lifecycle.
- `file-explorer:*` commands for every item that doesn't have one yet (new-note-in-folder, new-canvas-in-folder, new-base-in-folder, new-folder-in-folder, collapse-all, expand-all, duplicate, search-in-folder).

---

## Blockers / prerequisites

1. **Cluster R follow-up #2 (plugin injection ordering)** should land first or land in the same phase. Otherwise File Explorer will replicate the same injection bug, and we'll have to fix it in two places.
2. **TFolder modal signatures.** Decision needed: overload (`promptForFolderRename`) vs promote to `TAbstractFile*` base. Recommend `TAbstractFile*` base for consistency with Obsidian's own `fileManager.*` signatures.
3. **No blockers on sidebar-plugin commands.** Cluster R already registered `:open` commands on 5 sidebar plugins; the search plugin's `:open` is callable.

---

## Parity check against Obsidian

Once this cluster lands, the File Explorer muscle-memory check becomes: a right-click or F2 / Delete / Enter / Ctrl-click on a row does what the Obsidian user expects. That's the bar. Not in scope: drag-and-drop move (already partially working via Qt's native DnD; enhance in a separate cluster if gaps surface), multi-select operations (Obsidian ships this but it's a distinct feature), file-tag filter overlay.

---

## Estimate (rough)

3 – 5 days once expanded to a full plan. Primitive reuse is high; most of the work is menu construction + `TFolder*` variants + inline rename + keyboard handlers. No new modal dialogs. No new confirmation flows.

---

## Expansion triggers

Expand this scouting doc to a full plan when:

1. The user confirms they want File Explorer UX parity as the next cluster (vs Cluster M Canvas audit, Bases follow-ups, or other candidates).
2. Cluster R follow-up #2 has a decided direction (Option A quick-fix vs Option B `MenuSectionHelper*`-to-plugin ABI).
3. `TAbstractFile*` vs overload decision made for modal signatures.

Until then, this doc captures the scope so the context doesn't evaporate.
