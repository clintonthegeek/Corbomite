# `Platform.shell.showItemInFolder` — "Show in system explorer" menu action

**Date:** 2026-04-19
**Discovered during:** Cluster R (View-header menus) scoping. `workspace.md §316` mentions "Show in folder" as an injected menu item without describing the primitive. `leaf-utilities.md §482` references a `Platform` capability gate but doesn't detail the API.
**Supersedes / extends:** Extends `domains/workspace.md §316`, `domains/leaf-utilities.md §482`.
**Relevant cluster plans:** `superpowers/plans/2026-04-19-cluster-r-view-header-menus.md` (R P2 ships the menu item + primitive).

---

## 1. Function

`Platform.shell.showItemInFolder(absolutePath)` — opens the OS file manager at the file's parent folder with that file highlighted/selected. Direct wrapper over Electron's `shell.showItemInFolder`.

Distinct from "Open in default app" (which opens the file itself) — this opens the *folder containing* the file, with the file pre-selected.

---

## 2. Platform-specific dispatch

| Platform | Behaviour |
|---|---|
| macOS | Opens Finder at parent folder; file shown selected. |
| Windows | Opens Explorer at parent folder; file shown selected. |
| Linux | **File-manager-dependent.** Electron's `showItemInFolder` tries DBus `org.freedesktop.FileManager1.ShowItems` first (supported by Dolphin, Nautilus, Nemo, Caja, PCManFM-Qt); if that fails, falls back to `xdg-open <parent-folder>` which opens the folder but does not select the file. |
| Mobile | Not supported — menu item is hidden when `Platform.isMobile === true`. |

**Linux file-manager compatibility matrix** (observed 2026-04-19 on Manjaro KDE):

| File manager | `org.freedesktop.FileManager1.ShowItems` | Result |
|---|---|---|
| Dolphin (KDE) | ✓ | Folder opens with file selected |
| Nautilus (GNOME) | ✓ | Folder opens with file selected |
| Nemo (Cinnamon) | ✓ | Folder opens with file selected |
| PCManFM-Qt (LXQt) | ✓ | Folder opens with file selected |
| Thunar (XFCE) | ✗ (no DBus support) | Falls back to xdg-open; file not selected |
| ranger / lf | — (TUI) | Falls back to xdg-open folder |

---

## 3. Menu entry

**Markdown + Canvas hamburger menus, section `system`:**

```
Show in system explorer
```

On macOS the label becomes "Show in Finder"; on Windows it's "Show in Explorer"; on Linux it stays "Show in system explorer" (Obsidian doesn't detect the specific DE).

Icon: `lucide-folder-open`. No submenu.

**File Explorer context menu** also surfaces this entry for both files and folders (folder path fallback: `showItemInFolder` on a folder highlights the folder within its parent).

**Platform gate:** entry is hidden when `Platform.isMobile === true` or the FS adapter reports no filesystem access (web/sandboxed adapters).

---

## 4. Implementation hints for Corbomite

- Primitive: `bool Corbomite::Platform::showInFolder(const QString &absolutePath)`.
- Linux implementation (recommended):
  1. Try `QDBusInterface("org.freedesktop.FileManager1", "/org/freedesktop/FileManager1", "org.freedesktop.FileManager1")` → `call("ShowItems", QStringList{fileUrl}, "corbomite")` (second arg is startup-id); return true on success.
  2. On DBus failure, fall back to `QDesktopServices::openUrl(QUrl::fromLocalFile(parentFolder))`; return true on success.
  3. Return false only if both fail.
- macOS implementation: `QProcess::startDetached("open", {"-R", absolutePath})`.
- Windows implementation: `QProcess::startDetached("explorer", {"/select,", QDir::toNativeSeparators(absolutePath)})`.
- Exposed to plugins via `PluginContext` permission token (same gate as `openWithDefaultApp`).
- Menu label adapts via `QSysInfo::productType()` — "Finder"/"Explorer"/"system explorer".
