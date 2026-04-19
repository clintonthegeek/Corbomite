# `fileManager.promptForDeletion(file)` — delete-confirm modal

**Date:** 2026-04-19
**Discovered during:** Cluster R (View-header menus) scoping. `domains/views.md §73` cites "Delete" as a canonical `danger`-section menu item, and `domains/vault.md §84-85` covers `trashFile`, but the **confirmation modal** that gates user-initiated deletion is undocumented.
**Supersedes / extends:** Extends `domains/vault.md §84-85`, `domains/views.md §73`.
**Relevant cluster plans:** `superpowers/plans/2026-04-19-cluster-r-view-header-menus.md` (to be written — R P2 wires this modal to the Delete menu item).

---

## 1. Function

`fileManager.promptForDeletion(file): Promise<void>` — shows a confirm dialog; on OK calls `vault.trash(file, useSystemTrash)`, on Cancel resolves silently.

Callers:
- `EditableFileView::onPaneMenu` Delete item (per `views.md §73`).
- File Explorer context menu "Delete".
- Keyboard shortcut `Delete`/`Backspace` on a selected File Explorer entry.
- **Not** called from `vault.on('delete')` handlers (those react to completed deletions, not initiate them).

---

## 2. Modal shape

```
┌────────────────────────────────────────────┐
│  ⚠  Delete file                       [X]  │
├────────────────────────────────────────────┤
│  Are you sure you want to delete           │
│  "foo.md"?                                 │
│                                            │
│  It will be moved to your system trash.    │  ← or "It will be moved to Obsidian's trash folder."
│                                            │  ← or "It will be permanently deleted."
│                                            │
│  [ ] Don't ask again                       │
│                                            │
│                  [ Cancel ]  [  Delete  ]  │
└────────────────────────────────────────────┘
```

- Icon: warning (`lucide-alert-triangle`).
- Title: "Delete file" (or "Delete folder" when `file instanceof TFolder`, or "Delete n files" when batched).
- Body first line quotes the filename (escaped).
- Body second line varies by `vault.getConfig('trashOption')`:
  - `'system'` → "It will be moved to your system trash."
  - `'local'` → "It will be moved to Obsidian's trash folder." (the `.trash/` directory inside the vault)
  - `'none'` → "It will be permanently deleted." (uses `vault.adapter.remove` directly)
- "Don't ask again" checkbox writes `vault.setConfig('promptDelete', false)` on save; suppresses this modal for subsequent deletions until re-enabled in settings.
- Default button: **Cancel** (destructive-action convention — Enter does not delete).
- Escape = Cancel; Enter focuses the Delete button only after user tabs to it.
- Delete button is styled with the Obsidian `mod-warning` variant (red background).

---

## 3. Trash routing

After confirmation:

| `trashOption` | Implementation |
|---|---|
| `'system'` | `Platform.shell.moveItemToTrash(file.fullPath)`; on failure, silently falls back to `'local'`. |
| `'local'` | Moves file to `<vault>/.trash/<collision-free-name>`; creates `.trash/` if absent. Collision-free name is `<base> [ N].<ext>` where N increments from 1. |
| `'none'` | `vault.adapter.remove(file.path)` unconditionally. No recovery. |

Folders are trashed recursively; the whole subtree goes as one operation.

---

## 4. "Don't ask again" toggle

Controlled by `vault.getConfig('promptDelete')` (boolean, default `true`). When `false`, Delete actions skip the modal and go straight to `vault.trash(file, ...)`. Re-enabled via `Settings → Files & links → Confirm file deletion`.

Toggling off does **not** apply retroactively — only future deletions are affected. Folder deletions always prompt regardless of the setting, because folder delete is more destructive and recovery harder (source: observed in 1.12.7 behaviour).

---

## 5. Implementation hints for Corbomite

- New signature on `Corbomite::FileManager`: `bool promptForDeletion(TAbstractFile *f, QWidget *parent = nullptr)` returning `true` iff the user confirmed and the trash succeeded.
- Gate on `Vault::getConfig("promptDelete", true)` — if false, skip modal.
- Modal widget: `KMessageBox::warningContinueCancel` with the "Don't ask again" checkbox auto-wired via `KMessageBox::Dangerous` notifyInfo, or a hand-rolled `QDialog` if finer control is needed.
- Icon via `QIcon::fromTheme("dialog-warning")`.
- "Don't ask again" writes `Vault::setConfig("promptDelete", false)`. Settings panel (separate Cluster R P2 follow-up or existing Files settings page) surfaces a checkbox for re-enable.
- Folder-delete ignores the config (matches Obsidian).
