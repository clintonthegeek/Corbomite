# Vault Session Management — Design Specification

## Overview

Fix vault switching (currently crashes), add per-vault session persistence, and replace the empty startup window with a welcome screen. The app should feel like Obsidian — one vault at a time, full workspace context switch on open/close, session state restored exactly as you left it.

## Problem

Opening a vault when one is already open crashes the app. The root cause: `EditorViewManager` and `EditorViewSpace` are never cleaned up when a vault closes. All editor widgets, tab state, document pointers, and signal connections persist from the old vault. When the new vault opens and restores its session, it collides with stale state — stale `NoteEditorWidget` instances in the `m_editors` hash, dangling signal connections, stale document pointers in panels.

Beyond the crash, the app starts to a blank window with no vault open and no guidance. There's no way to close a vault without quitting.

## Part 1: Vault Lifecycle (Close + Open Sequences)

### Close Sequence

Triggered by the "Close Vault" action, opening a different vault, or app quit. Follows Kate's "switch-with-cleanup" pattern: prompt first, then destroy.

1. **Save session** — write current workspace state to `.corbomite/session.json` (see Part 2)
2. **Prompt unsaved documents** — `EditorViewManager::queryClose()` iterates all panes, checks for modified documents, shows save/discard/cancel dialog. If user cancels, abort the entire close. Nothing is destroyed.
3. **Close all documents** — `EditorViewManager::closeAllDocuments()`:
   - Close all tabs in all `EditorViewSpace` panes
   - Delete all `NoteEditorWidget` and `NotePreviewWidget` instances
   - Clear `m_editors`, `m_previews`, `m_previewModePaths` hashes in each pane
   - Reset split layout: remove all extra panes, collapse back to a single `EditorViewSpace`
   - Clear `TabModel` in each pane
4. **Existing cleanup** — `MainWindow::onVaultClosed()` runs as today: deletes autosave, file watch, session manager, template/daily note services, search index, tree model. Sets panels to nullptr.
5. **Show welcome screen** — swap `m_centralStack` to index 0 (welcome widget). Hide sidebars.

### Open Sequence

Triggered from welcome screen (recent vault click, "Open Folder...", "Create New Vault...") or "Open Recent" menu.

1. If a vault is currently open, run the Close Sequence first. If user cancels the unsaved prompt, abort.
2. **Hide welcome screen** — swap `m_centralStack` to index 1 (editor area). Show sidebars.
3. `VaultService::openVault(path)` runs — emits `vaultOpened()`
4. `MainWindow::onVaultOpened()` creates services, index, tree model as today
5. **Restore session** — read `.corbomite/session.json`, rebuild split layout, open tabs, set active tab, restore cursor positions, scroll offsets, sidebar state, reading/source mode per tab
6. If no session file exists (first open of this vault), start with a single empty pane

### Key Principle

The user prompt for unsaved documents happens *before* any cleanup. If they cancel, nothing changes. This is Kate's `queryClose_internal()` pattern — measure twice, cut once.

## Part 2: Session State Persistence

Each vault stores workspace state in `.corbomite/session.json`. This extends the current `SessionManager` which already saves window geometry, sidebar widths, open tab paths, and expanded folders.

### Schema

```json
{
  "window": {
    "geometry": "base64-encoded QByteArray",
    "state": "base64-encoded QByteArray"
  },
  "sidebar": {
    "leftVisible": true,
    "rightVisible": false,
    "leftWidth": 250,
    "rightWidth": 250,
    "activePanel": "files"
  },
  "fileExplorer": {
    "expandedFolders": ["folder/path", "another/folder"]
  },
  "editor": {
    "panes": [
      {
        "tabs": [
          {
            "path": "notes/Welcome.md",
            "type": "note",
            "mode": "source",
            "cursorLine": 42,
            "cursorColumn": 0,
            "scrollY": 120
          },
          {
            "path": "notes/Ideas.md",
            "type": "note",
            "mode": "reading",
            "cursorLine": 0,
            "cursorColumn": 0,
            "scrollY": 0
          },
          {
            "path": "canvases/Overview.canvas",
            "type": "canvas"
          }
        ],
        "activeTabIndex": 0
      }
    ],
    "splitLayout": {
      "orientation": "horizontal",
      "sizes": [500, 500],
      "children": ["pane:0", "pane:1"]
    }
  }
}
```

### Split Layout Encoding

The `splitLayout` field is a tree mirroring the `QSplitter` nesting:
- **Leaf node:** `"pane:N"` string referencing into the `panes` array
- **Branch node:** object with `orientation` ("horizontal" or "vertical"), `sizes` (splitter sizes array), and `children` (array of leaf strings or nested branch objects)
- **Single pane:** just `"pane:0"`, no nesting

### Tab Types

- `"note"` — markdown note, has mode/cursor/scroll state
- `"canvas"` — canvas file, no cursor state

### Save Triggers

- **Vault close** — explicit save as first step of close sequence
- **App quit** — save in `closeEvent()` before shutdown
- **Auto-save** — 2-second debounced timer on tab/split/sidebar state changes (same pattern as current SessionManager). Uses Kate's reference-counted `AutoSaveBlocker` pattern during vault switch to prevent saves mid-transition.

### Restore Behavior

On vault open:
1. Read `.corbomite/session.json`
2. Rebuild QSplitter tree from `splitLayout`
3. Create `EditorViewSpace` for each pane
4. Open tabs in order for each pane (call `openNote()` / `openCanvas()`)
5. Set active tab per pane
6. Restore cursor position and scroll offset per note tab
7. Restore reading/source mode per note tab
8. Restore sidebar visibility, widths, active panel
9. Restore window geometry

If the file doesn't exist (first time opening vault), start with a single empty pane.

If a referenced file no longer exists, skip that tab silently.

## Part 3: Welcome Screen

A `QWidget` that fills the main window's central area when no vault is open. Shown on startup and after closing a vault.

### Layout

```
+----------------------------------------------------------+
|  Menu bar (always visible)                                |
+----------------------------------------------------------+
|                                                          |
|                                                          |
|              [Generative artwork area]                   |
|              Overlapping translucent shapes               |
|              using KDE color palette                      |
|              ~300px tall                                  |
|                                                          |
|                     Corbomite                             |
|                  (subtle title text)                      |
|                                                          |
|              +----------------------------+              |
|              | My Vault          ~/vaults/ |              |
|              | Work Notes    ~/Documents/  |              |
|              | Research       ~/research/  |              |
|              +----------------------------+              |
|              (recent vaults list, clickable)              |
|                                                          |
|           [Open Folder...]  [Create New Vault...]        |
|                                                          |
+----------------------------------------------------------+
```

### Widget Details

- **Central positioning:** Content area max-width ~500px, centered horizontally and vertically
- **Artwork:** `QPainter`-based generative composition rendered once at widget creation. Overlapping translucent ellipses or soft gradient shapes using colors from the current KDE/Qt palette (`QPalette::Highlight`, `QPalette::Accent`, etc.). Looks different each launch (seeded from current time). Respects dark/light themes.
- **Title:** "Corbomite" (or "Corbomite [Dev]" in dev builds) in a large, light-weight font below the artwork
- **Recent vaults list:** `QListWidget` or custom painted list. Each entry shows vault name (bold, extracted from folder name) and full path (dimmed). Most recent first. Max 8 entries. Click opens that vault.
- **Buttons:** "Open Folder..." opens `QFileDialog` directory picker. "Create New Vault..." opens a dialog to name and locate a new vault directory.
- **Theme-aware:** All colors from `QPalette`, no hardcoded colors

### Behavior

- Sidebars are hidden when welcome screen is showing
- Status bar is hidden or shows minimal info
- Menu bar stays visible — File > Open Vault, Open Recent still work as alternate entry points
- Clicking a recent vault or using buttons triggers the Open Sequence
- Welcome screen is a child of `m_centralStack` (QStackedWidget, index 0)

### Integration with MainWindow

```cpp
// In MainWindow, replace the current central widget setup:
m_centralStack = new QStackedWidget(this);
m_welcomeScreen = new WelcomeScreen(this);
m_editorArea = new QWidget(this);  // Contains m_editorManager + sidebar layout

m_centralStack->addWidget(m_welcomeScreen);  // Index 0
m_centralStack->addWidget(m_editorArea);      // Index 1
setCentralWidget(m_centralStack);

// Vault open: m_centralStack->setCurrentIndex(1);
// Vault close: m_centralStack->setCurrentIndex(0);
```

The existing sidebar layout and editor manager move into `m_editorArea`. Currently MainWindow's central widget is a `QHBoxLayout` holding left sidebar + editor manager + right sidebar. That entire layout becomes `m_editorArea`. The `QStackedWidget` wraps it, swapping between welcome and editor views. No change to how sidebars and editor relate to each other — just an extra container layer above them.

## Part 4: Action & Menu Changes

### New Action: Close Vault

- Action name: `file_close_vault`
- Menu text: `i18n("Close Vault")`
- Shortcut: none (rarely used, don't waste a binding)
- Icon: `QIcon::fromTheme("window-close")` or `"folder-close"` if available
- Enabled: only when a vault is open
- Behavior: triggers the Close Sequence (Part 1)

### Modified: Open Vault

- Existing `file_open_vault` action unchanged
- But now: if a vault is open when triggered, runs Close Sequence first (with unsaved prompt), then shows the directory picker. If close is cancelled, abort.

### Modified: Open Recent

- `KRecentFilesAction` works as today
- But now: if a vault is open, runs Close Sequence first. If cancelled, abort.

### Removed Behavior

- The app no longer starts with an empty editor area and no vault. It starts with the welcome screen.
- No "Open Another Vault" action — "Close Vault" + welcome screen IS the workflow.

### Action Enable/Disable

| Action | No vault open | Vault open |
|---|---|---|
| Open Vault... | Enabled | Enabled (closes first) |
| Open Recent | Enabled | Enabled (closes first) |
| Close Vault | Disabled | Enabled |
| New Note | Disabled | Enabled |
| Save / Save All | Disabled | Enabled |
| Toggle Reading Mode | Disabled | Enabled |

`MainWindow::updateVaultActions()` already handles some of this — extend it to cover the new Close Vault action and ensure all vault-dependent actions are disabled when on the welcome screen.

## What This Does NOT Include

- **"Reopen last vault" startup setting** — future nicety, trivial to add (save last vault path in KConfig, check on startup)
- **Multiple vault windows** — Obsidian supports this but it's a major architectural change (one window per vault, separate processes or careful state isolation)
- **Named workspace presets** — Obsidian's "Workspaces" plugin for saving/loading layout snapshots within a vault
- **Vault creation wizard** — "Create New Vault..." just creates an empty directory with `.corbomite/` config folder
- **Vault-scoped settings** — per-vault editor settings, keybindings, etc. (future)

## Testing

### Unit Tests

**tst_sessionmanager.cpp (new or extended):**
- Save session with multiple panes → JSON has correct splitLayout tree
- Save session with tabs in reading mode → mode field is "reading"
- Save cursor position → cursorLine/cursorColumn/scrollY in JSON
- Restore session → correct number of panes, tabs, active tab indices
- Restore with missing file → tab skipped, no crash
- Restore with no session file → single empty pane
- Round-trip: save → restore → save → compare JSON (should match)

**tst_editorviewmanager.cpp (new or extended):**
- `closeAllDocuments()` → all panes empty, single pane remaining
- `closeAllDocuments()` with modified document → queryClose returns false if user cancels
- `queryClose()` with no modified docs → returns true immediately

### Integration Tests (tst_e2e_gui)

- Open vault → tabs appear → close vault → welcome screen shown → all editors cleared
- Open vault A → switch to vault B → vault A's session saved → vault B's session restored
- Open vault → modify note → close vault → save dialog appears
- App quit with vault open → session saved
- Launch app → welcome screen visible → click recent vault → editor area with restored tabs

### Manual Testing

- Open vault, arrange tabs and splits, close, reopen → same layout restored
- Open vault in dark theme → welcome screen artwork uses dark palette colors
- Close vault with unsaved changes → cancel → nothing happens, vault still open
