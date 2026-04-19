# `fileManager.promptForFileRename` / `promptForMove` modals

**Date:** 2026-04-19
**Discovered during:** Cluster R (View-header menus) scoping. `domains/vault.md §84` references `promptForFileRename(file)` in a table row and `views.md §74` cites it as the inline-rename fallback, but neither doc describes the modal UX itself. `promptForMove` is mentioned nowhere.
**Supersedes / extends:** Extends `domains/vault.md §84-85`, `domains/views.md §67-74` (which covers inline-rename on `titleEl` but not the modal).
**Relevant cluster plans:** `superpowers/plans/2026-04-19-cluster-r-view-header-menus.md` (to be written — R P2 ships both modals as part of the "Rename…" / "Move file to…" menu entries).

> **Source provenance.** Obsidian 1.12.7. Locate via the string `"Rename"` in modal-defining constructs; the modal is a `Modal` subclass instantiated from `FileManager.prototype.promptForFileRename`.

---

## 1. `promptForFileRename(file)`

**Entry points in production:**

- `EditableFileView.setEphemeralState({rename: 'start'})` when the view is not visible (per `views.md §74`). Inline-rename on `titleEl` is primary; this modal is the fallback path only.
- Any menu item labelled "Rename…" registered on a file via `workspace.trigger('file-menu', ...)` or `EditableFileView::onPaneMenu` — the modal is the *sole* path when the file is not currently open in a visible leaf.

**Modal shape:**

```
┌────────────────────────────────────────────┐
│  Rename                               [X]  │
├────────────────────────────────────────────┤
│  Enter new name:                           │
│  [foo.md                             ]     │ ← pre-selected basename, cursor placed before extension
│                                            │
│  (inline validation tooltip when invalid)  │
│                                            │
│                    [ Cancel ]  [ Save ]    │
└────────────────────────────────────────────┘
```

**Pre-selection:** The text field contains the file's full name including extension; the **basename portion** (everything before the final `.`) is selected on open, matching Obsidian's VS Code / macOS Finder convention. For `foo.md`, the selection is `foo`; pressing a letter overwrites `foo` while keeping `.md`. For a file with no extension, the entire name is selected.

**Validation runs live on every keystroke** via `LX(app, file, newName, isFinal=false)`:
- Empty string → "A file name is required." (blocks Save).
- Contains any of `\ / : * ? " < > |` → "File name cannot contain any of the following characters: \\ / : * ? \" < > |" (blocks Save).
- Starts with `.` → allowed but warned.
- Reserved Windows names (`CON`, `PRN`, `AUX`, `NUL`, `COM1-9`, `LPT1-9`) → "That file name is reserved. Please use a different name." (blocks Save on Windows, warn on other platforms).
- Collides with another file in the same folder (case-insensitive per platform policy) → "A file with this name already exists." (blocks Save).

**Save path:** calls `file.getNewPathAfterRename(basename)` (preserves parent folder), then `fileManager.renameFile(file, newPath)` which handles link rewrite and MetadataCache propagation. Returns a `Promise<void>` resolved when the rename completes.

**Cancel path:** resolves the outer `Promise` with no rename.

**Keyboard:** Enter triggers Save (runs final validation with `isFinal=true`); Escape triggers Cancel.

---

## 2. `promptForMove(file)` (Obsidian calls this `promptForMove`; may be listed under `FileManager` or an internal helper)

Opens a folder-picker modal used by the "Move file to…" menu item.

**Modal shape:**

```
┌────────────────────────────────────────────┐
│  Move file                            [X]  │
├────────────────────────────────────────────┤
│  Type folder to search                     │
│  [                                   ]     │ ← filter input
│  ─────────────────────────────────────     │
│  /                                         │
│  /archive                                  │
│  /archive/2024                             │
│  /daily                                    │
│  /notes                                    │
│  ...                                       │
│                                            │
└────────────────────────────────────────────┘
```

- Layout mirrors the Quick Switcher (`SuggestModal`): top filter input, list of fuzzy-matched folder paths below. No explicit Save/Cancel buttons — selection = Save, Escape = Cancel.
- Root folder shows as `/` with display text "(root)".
- Selecting a folder calls `fileManager.renameFile(file, newFolder.path + "/" + file.name)` — i.e. **move is a rename under the hood.** Link rewrite happens automatically through `renameFile`.
- Filter uses the same `FuzzyMatcher` as Quick Switcher.

**Rejection cases:**
- Target folder is the file's current parent → silent no-op (no rename triggered, modal closes).
- Target folder contains a file with the same name → error: "A file with this name already exists in the target folder."

---

## 3. Differences from inline-rename

Inline-rename (`views.md §67-74`) edits `titleEl.contentEditable`, snapshots `fileBeingRenamed`, and commits on blur/Enter with the same validation pipeline. The modal path is otherwise identical in validation + commit behaviour — the distinguishing factor is *where* the text input lives.

Design invariant: inline-rename always takes precedence when available; modal is reached only when the leaf hosting the file is not visible.

---

## 4. Implementation hints for Corbomite

- Both modals live on `Corbomite::FileManager` (libs/vault): `QString promptForFileRename(TFile *f, QWidget *parent)` and `QString promptForMove(TFile *f, QWidget *parent)` returning the new path (or empty QString on cancel).
- Use `KStandardGuiItem::ok()` / `cancel()` for Save/Cancel button styling.
- Validation delegates to a shared free function `Corbomite::validateFileName(const QString &newName, const TFile *file, Vault *vault, bool isFinal)` returning `QString` (empty = valid, non-empty = error message).
- Folder-picker modal reuses `FuzzyMatcher` from Cluster D; walks `Vault::getAllLoadedFiles` filtered to `TFolder`.
- Basename pre-selection: `QLineEdit::setSelection(0, basename.length())` after `setText(name)`.
