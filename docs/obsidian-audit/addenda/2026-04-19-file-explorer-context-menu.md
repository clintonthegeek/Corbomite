# File Explorer right-click context menu (Obsidian)

**Status:** Addendum. Fills a gap in `domains/ui-bundle.md` (which covers File Explorer panel rendering but not its right-click menu structure) and `domains/leaf-utilities.md` (which covers pane-menu semantics but not File Explorer row menus).

**Discovered during:** Cluster R closeout audit (2026-04-19). Cluster R wired the hamburger side of every file operation; File Explorer right-click is the second canonical exposure surface in Obsidian and was never documented.

---

## Behaviour

Right-clicking a **file row** (`.md`, `.canvas`, `.base`, images, etc.) in the File Explorer (Files tab) opens a context menu with the following items in canonical section order. This is the `workspace.trigger("file-menu", menu, file, "file-explorer-context-menu")` event emission path — plugins inject into the same menu.

### File rows

**action section:**

- Open in new tab
- Open to the right (opens in a new leaf split vertically to the right)
- Open below (split horizontal)
- Open in new window
- Make a copy (duplicates with " 1" suffix, collision-resolving)

**action section (continued):**

- Rename… (inline-rename on the row, focus + select stem)
- Move to another folder… (folder-picker modal; identical to hamburger's Move to…)
- Bookmark… (emits `bookmarks:bookmark-file-with-options` with the file as context)
- Copy link to file (emits `workspace:copy-url-in-selection` — wiki-link format `[[File]]`)

**info.copy submenu:**

- Copy Obsidian URL
- Copy path (vault-relative)
- Copy absolute path

**system section:**

- Open in default app (`workspace:open-in-default-app` with this file)
- Show in system explorer (opens containing folder with file selected)

**danger section:**

- Delete (same confirm-modal as hamburger; honours vault-config `useSystemTrash` / `trashOption`)

### Folder rows

Right-clicking a folder row shows:

**action section:**

- New note (creates inside folder; inline-rename pattern)
- New canvas
- New base
- New folder (creates as nested child)

**action section (continued):**

- Rename… (inline-rename on the folder)
- Move folder to… (folder-picker modal)
- Duplicate folder (recursive copy; `-1` suffix)
- Search in folder… (opens Search panel scoped to folder path)

**info.copy submenu:**

- Copy path

**system section:**

- Open in default app (macOS/Linux: opens folder in OS file manager; Windows: Explorer)
- Show in system explorer

**view section:**

- Collapse all (toggles all descendant folders collapsed)
- Expand all

**danger section:**

- Delete (recursive; confirm modal lists count of contained files)

### Empty-area / whitespace right-click

Right-clicking in empty space below the last row (or at the vault root) shows a reduced menu:

- New note (at vault root)
- New canvas
- New base
- New folder
- Collapse all folders
- Show attachments (toggle)

---

## Menu construction path (Obsidian internals)

1. `FileExplorerView.onFileContextMenu(event, file)` — invoked on `contextmenu` event on a file row.
2. Constructs a new `Menu()` (same class as pane-hamburger menus).
3. Adds built-in items in canonical section order via `menu.addItem(…).setSection("action")`.
4. Fires `workspace.trigger("file-menu", menu, file, "file-explorer-context-menu")` — **this is the plugin-injection point**. The third argument `source` is the string `"file-explorer-context-menu"`, distinguishing from `"more-options"` (pane hamburger) and `"link-context-menu"` (wiki-link hover).
5. `menu.showAtMouseEvent(event)`.

The same `workspace.trigger("file-menu", …)` event fires for the pane hamburger — listeners that don't care about source see both events and inject in both surfaces. Plugins can filter on the `source` string to inject in only one.

---

## Keyboard exposure

- `F2` on a focused file row → inline rename.
- `Delete` on a focused file row → danger-section Delete (with confirm).
- `Enter` on a focused file row → Open in current tab.
- `Ctrl/Cmd + Click` on a file row → Open in new tab.
- `Shift + Click` on a file row → Open in new split.

---

## Implementation notes for Corbomite (Cluster U scouting)

- **Primitives already shipped (Cluster R):** `FileManager::promptForFileRename`, `FileManager::promptForMove`, `FileManager::promptForDeletion`, `FileNameValidator`, `Platform::openWithDefaultApp`, `Platform::showInFolder`, `PathUtils::obsidianUrlFor`, `PathUtils::corbomiteUrlFor`. These should drive the File Explorer menu items directly — no new modal or validation work needed.
- **New primitives needed:** folder-aware variants of rename/delete (current signatures take `TFile*` — need `TFolder*` overloads or a `TAbstractFile*` base), `FileManager::duplicate(TFile*)`, `FileManager::duplicateFolder(TFolder*)`.
- **New commands to register:** `file-explorer:new-note-in-folder`, `file-explorer:new-canvas-in-folder`, `file-explorer:new-base-in-folder`, `file-explorer:new-folder-in-folder`, `file-explorer:collapse-all`, `file-explorer:expand-all`, `file-explorer:duplicate`, `file-explorer:search-in-folder`.
- **Menu construction:** `FileExplorerView` should use `MenuSectionHelper` with canonical section order — same substrate as Cluster R's hamburger menus. Plugin emission should mirror `ItemView::showMoreOptionsMenu` — emit `MenuEventEmitter::fileMenu(menu, file, "file-explorer-context-menu")` before finalize (note: also fix Cluster R follow-up #2 at the same time).
- **Inline rename (F2):** `FileExplorerView::beginInlineRename(index)` — turns the row's label into an editable `QLineEdit`, validates on commit via `FileNameValidator`, dispatches to `FileManager::renameFile` on accept. Obsidian does this rather than opening the modal dialog for F2; the hamburger's Rename… opens the modal. Both paths coexist.

---

## Why this wasn't in Pass 2

The original audit focused on markdown-view surfaces and pane-level utilities; File Explorer was captured as "panel" in `ui-bundle.md` without enumerating its context menu. Pass 3 synthesis (`GAP-ANALYSIS.md`) flagged File Explorer right-click as known-missing but didn't enumerate items. This addendum is that enumeration.
