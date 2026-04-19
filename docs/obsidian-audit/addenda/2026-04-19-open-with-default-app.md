# `app.openWithDefaultApp` / `Platform.shell.openPath` — "Open in default app" menu action

**Date:** 2026-04-19
**Discovered during:** Cluster R (View-header menus) scoping. "Open in default app" is the canonical `system`-section menu item in Obsidian's markdown + canvas + graph hamburger menus. Not covered anywhere in the audit.
**Supersedes / extends:** No prior coverage in audit. Touches `domains/platform.md` §"Shell" (cross-ref to be added), `domains/workspace.md` §6 (one of the workspace-injected items when applicable).
**Relevant cluster plans:** `superpowers/plans/2026-04-19-cluster-r-view-header-menus.md` (R P2 ships this menu item + the underlying `Platform::openWithDefaultApp` primitive).

---

## 1. Function

`app.openWithDefaultApp(path)` — opens the file through the OS default-application association, bypassing Obsidian's own view dispatch.

Path semantics: vault-relative path. Internally resolves through `vault.adapter.getFullPath(path)` to produce an absolute filesystem path, then delegates to `Platform.shell.openPath(fullPath)`.

---

## 2. Platform-specific dispatch

Electron's `shell.openPath(fullPath): Promise<string>` — empty-string resolve = success; non-empty = OS error message.

| Platform | Mechanism |
|---|---|
| macOS | `LSOpenFromURLSpec` under the hood of `shell.openPath` |
| Windows | `ShellExecuteEx` with `"open"` verb |
| Linux | `xdg-open` via freedesktop.org MIME-type resolution |
| iOS/Android (Mobile) | Platform-specific native intent; PDF/image previews use in-app viewer, other types fall back to share-sheet |

No user-visible error surface on Linux if `xdg-open` reports failure — Obsidian silently fails (verified: open a `.md` on a system with no editor registered for `text/markdown`; no Notice fires). Potential QoL gap — addenda author recommends Corbomite show a Notice on non-empty return from the shell API.

---

## 3. Menu entry

**Markdown + Canvas + Graph hamburger menus, section `system`:**

```
Open in default app
```

Icon: `lucide-external-link`. No submenu. Fires `app.openWithDefaultApp(file.path)` for FileView-hosted views; not emitted for views without a file (Graph view included — the graph menu shows this item referring to the `.md` of the active note if one is pinned, but is typically omitted).

**File Explorer context menu, section `system`:** same item, same behaviour.

---

## 4. Obsidian-specific caveats

- For `.md` files the default app is usually *another* markdown editor. Opening-in-default-app of a file currently open in Obsidian is fine; the external editor and Obsidian coexist. If the external editor writes, Obsidian's vault watcher fires `vault.on('modify')` and the active view runs its three-way merge (per `views.md §94`).
- The current leaf is **not** closed after opening in default app.
- No keyboard hotkey by default.

---

## 5. Implementation hints for Corbomite

- New primitive on a shared namespace (e.g. `Corbomite::Platform`) or directly on the shared proxy surface: `bool openWithDefaultApp(const QString &absolutePath)`.
- Implementation: `QDesktopServices::openUrl(QUrl::fromLocalFile(absolutePath))` — this already dispatches through `xdg-open`/`ShellExecute`/`LSOpenFromURLSpec` on the respective platforms.
- Return value: boolean from `QDesktopServices::openUrl` (false = failure); Corbomite should show a `Notice` on failure ("Could not open file with default application.") — an audit-noted UX improvement over Obsidian's silent-fail.
- Menu wiring: `QAction::triggered` → `Platform::openWithDefaultApp(m_file->fullPath())` where `fullPath()` resolves through `Vault::basePath() + / + TFile::path`.
- Exposed to plugins via a new capability on `PluginContext` (permission token `system.open-default-app` — or fold under the existing `system` permission if one is added for `show-in-folder` too).
