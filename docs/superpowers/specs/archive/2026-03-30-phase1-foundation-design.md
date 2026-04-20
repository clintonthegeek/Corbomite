# Phase 1: Foundation & Core Editor — Design Specification

## Overview

Build a functioning KDE note editor that opens Obsidian vaults, browses files, edits markdown in tabs, autosaves, watches for external changes, and persists settings and session state. This phase establishes the project scaffold, build system, core libraries, and main window — the foundation everything else builds on.

No Obsidian-specific markdown extensions (wikilinks, callouts, embeds) in this phase. The editor is a competent markdown source editor backed by QMarkdownTextEdit.

## Architecture

### Layered Structure

```
UI Layer (MainWindow, panels, editor widgets, dialogs)
    ↓ reads models, dispatches via KActionCollection
Service Layer (NoteService, VaultService, SettingsService)
    ↓ orchestrates I/O + model updates
Model Layer (NoteDocument, VaultModel, NotesTreeModel, TabModel)
    ↓ state containers, Qt item models, signal-emitting
Storage Layer (FileSystemAdapter)
    ↓ file I/O abstraction
```

Services are the only layer that performs I/O. UI components never touch the filesystem directly.

### Source Layout

```
corbomite/
├── CMakeLists.txt
├── CLAUDE.md
├── libs/
│   ├── core/
│   │   ├── CMakeLists.txt
│   │   ├── include/corbomite/core/
│   │   │   ├── NoteMeta.h
│   │   │   └── NoteDocument.h
│   │   └── src/
│   │       ├── NoteMeta.cpp
│   │       └── NoteDocument.cpp
│   ├── storage/
│   │   ├── CMakeLists.txt
│   │   ├── include/corbomite/storage/
│   │   │   ├── FileSystemAdapter.h
│   │   │   └── VaultScanner.h
│   │   └── src/
│   │       ├── FileSystemAdapter.cpp
│   │       └── VaultScanner.cpp
│   ├── models/
│   │   ├── CMakeLists.txt
│   │   ├── include/corbomite/models/
│   │   │   ├── VaultModel.h
│   │   │   ├── NotesTreeModel.h
│   │   │   └── TabModel.h
│   │   └── src/
│   │       ├── VaultModel.cpp
│   │       ├── NotesTreeModel.cpp
│   │       └── TabModel.cpp
│   └── qmarkdowntextedit/           # Git submodule
├── src/
│   ├── CMakeLists.txt
│   ├── app/
│   │   ├── main.cpp
│   │   ├── CorbomiteApp.h/cpp
│   │   ├── MainWindow.h/cpp
│   │   ├── corbomiteui.rc.in
│   │   ├── corbomite.kcfg
│   │   └── corbomitesettings.kcfgc
│   ├── mdi/
│   │   ├── CorbomiteMDI.h/cpp       # Adapted from Kate's katemdi.h/cpp
│   │   └── CorbomiteSplitter.h/cpp  # Adapted from Kate's katesplitter.h/cpp
│   ├── editor/
│   │   ├── EditorViewManager.h/cpp
│   │   ├── EditorViewSpace.h/cpp
│   │   ├── EditorTabBar.h/cpp       # Adapted from Kate's katetabbar.h/cpp
│   │   └── NoteEditorWidget.h/cpp
│   ├── sidebar/
│   │   └── FileExplorerPanel.h/cpp
│   ├── dialogs/
│   │   └── SettingsDialog.h/cpp
│   └── reactors/
│       ├── AutosaveReactor.h/cpp
│       └── FileWatchReactor.h/cpp
├── data/
│   ├── org.corbomite.Corbomite.desktop
│   └── icons/
├── tests/
│   ├── core/
│   │   ├── CMakeLists.txt
│   │   ├── tst_notemeta.cpp
│   │   └── tst_notedocument.cpp
│   ├── storage/
│   │   ├── CMakeLists.txt
│   │   ├── tst_filesystemadapter.cpp
│   │   └── tst_vaultscanner.cpp
│   ├── models/
│   │   ├── CMakeLists.txt
│   │   ├── tst_notestreemodel.cpp
│   │   └── tst_tabmodel.cpp
│   └── integration/
│       ├── CMakeLists.txt
│       ├── tst_vault_lifecycle.cpp
│       ├── tst_editor_save.cpp
│       ├── tst_filewatch.cpp
│       └── tst_session.cpp
└── docs/
```

Libraries export alias targets: `Corbomite::Core`, `Corbomite::Storage`, `Corbomite::Models`. All static, `POSITION_INDEPENDENT_CODE ON`.

### KDE Framework Dependencies

| Framework | Purpose |
|-----------|---------|
| KF6::CoreAddons | KAboutData, version info |
| KF6::I18n | i18n() for all user-visible strings |
| KF6::XmlGui | KXmlGuiWindow, KActionCollection, KStandardAction |
| KF6::Config | KSharedConfig, KConfigGroup |
| KF6::ConfigWidgets | KConfigXT codegen, KShortcutsDialog |
| KF6::WidgetsAddons | KMessageBox |
| KF6::IconThemes | QIcon::fromTheme with fallbacks |
| KF6::DBusAddons | KDBusService (single instance) |
| Qt6::Core | Fundamental types |
| Qt6::Widgets | UI widgets |
| Qt6::DBus | D-Bus integration |

## Build System

### CMake Configuration (PlanStan Pattern)

Root `CMakeLists.txt`:
- `cmake_minimum_required(VERSION 3.19)`
- `find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets DBus)`
- `find_package(ECM 6.0 REQUIRED NO_MODULE)` with KDEInstallDirs, KDECMakeSettings, KDECompilerSettings
- `find_package(KF6 REQUIRED COMPONENTS CoreAddons I18n XmlGui Config ConfigWidgets WidgetsAddons IconThemes DBusAddons)`
- C++20, AUTOUIC/AUTOMOC ON, `qt_standard_project_setup()`
- `option(CORBOMITE_DEV_BUILD "Use isolated config/data dirs for development" OFF)`
- `add_subdirectory(libs/core)`, `add_subdirectory(libs/storage)`, etc.
- `enable_testing()` with test subdirectories

### Dev/Release Build Isolation

When `CORBOMITE_DEV_BUILD=ON`:

| Aspect | Release | Dev |
|--------|---------|-----|
| Config dir | `~/.config/corbomiterc` | `~/.config/corbomite-devrc` |
| Data dir | `~/.local/share/corbomite/` | `~/.local/share/corbomite-dev/` |
| Window title | "Corbomite" | "Corbomite [Dev]" |
| XMLGUI prefix | `/kxmlgui5/corbomite` | `/kxmlgui5/corbomite-dev` |
| Component name | `corbomite` | `corbomite-dev` |

Implemented via `configure_file()` for the XMLGUI `.rc.in` file and `target_compile_definitions(CORBOMITE_DEV_BUILD)`.

## Data Models

### NoteMeta

Lightweight metadata struct (no content loaded):

```cpp
struct NoteMeta {
    QString relativePath;    // folder/note.md
    QString name;            // note (no extension)
    QDateTime modified;
    qint64 sizeBytes;

    // Derived
    QString absolutePath(const QString &vaultRoot) const;
    static NoteMeta fromFileInfo(const QFileInfo &fi, const QString &vaultRoot);
};
```

### NoteDocument

Represents an open note with content:

```cpp
class NoteDocument : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool modified READ isModified NOTIFY modificationChanged)

public:
    QString filePath() const;
    QString relativePath() const;
    QString name() const;

    QString markdown() const;
    void setMarkdown(const QString &text);
    QTextDocument *textDocument();

    bool isModified() const;
    void setModified(bool modified);
    QDateTime fileModifiedTime() const;

    int wordCount() const;
    int characterCount() const;

signals:
    void textChanged();
    void modificationChanged(bool modified);
    void saved();
};
```

- Owns the `QTextDocument` used by the editor widget
- Word/character count computed on demand (cached, invalidated on text change)
- `modificationChanged` drives tab dirty indicator and autosave reactor

### VaultModel

```cpp
class VaultModel : public QObject {
    Q_OBJECT

public:
    QString path() const;
    QString name() const;
    QString configPath() const;     // .corbomite/

    QVector<NoteMeta> allNotes() const;
    bool noteExists(const QString &relativePath) const;
    NoteDocument *openDocument(const QString &relativePath);  // Cache

signals:
    void noteAdded(const QString &relativePath);
    void noteRemoved(const QString &relativePath);
    void noteRenamed(const QString &oldPath, const QString &newPath);
    void noteModified(const QString &relativePath);
    void vaultScanned();
};
```

- Document cache: `QHash<QString, NoteDocument*>` keyed by relative path
- Documents created on first open, cached for lifetime of vault session

### NotesTreeModel

`QAbstractItemModel` for the file explorer:

- Tree structure mirroring vault folder hierarchy
- Roles: `PathRole`, `NameRole`, `IsDirectoryRole`, `ModifiedTimeRole`, `FileTypeRole`
- Sort modes: Alphabetical, ModifiedNewest, ModifiedOldest
- Drag-drop: produces `text/uri-list` MIME, drop triggers `NoteService::moveNote()`
- Reacts to `VaultModel` signals to insert/remove/update rows

### TabModel

`QAbstractListModel` for open tabs:

```cpp
struct TabState {
    QString notePath;
    int scrollPosition = 0;
    int cursorLine = 0;
    int cursorColumn = 0;
    bool isPinned = false;
    bool isDirty = false;
    quint64 lruCounter = 0;
};
```

- `openTab(path)` — add tab or activate existing
- `closeTab(index)` — remove, push to closed history
- `reopenLastClosed()` — pop from closed history
- `lruSortedTabs()` — for Ctrl+Tab navigation
- Roles: `NotePathRole`, `TitleRole`, `IsPinnedRole`, `IsDirtyRole`

## Main Window

### KateMDI Adaptation

Copy from Kate (`kate/apps/lib/`), adapt to `CorbomiteMDI::` namespace:

| Kate File | Corbomite File | What Changes |
|-----------|---------------|--------------|
| `katemdi.h/cpp` | `src/mdi/CorbomiteMDI.h/cpp` | Namespace rename, remove Kate-specific includes |
| `katesplitter.h/cpp` | `src/mdi/CorbomiteSplitter.h/cpp` | Class rename |
| `katetabbar.h/cpp` | `src/editor/EditorTabBar.h/cpp` | Class rename, remove Kate document type refs |

The MDI framework gives us:
- 4-position sidebar system (left, right, top, bottom)
- Collapsible toolviews with tab buttons
- Session save/restore of sidebar state
- GUIClient for automatic menu generation

### MainWindow

Inherits `CorbomiteMDI::MainWindow` (which extends `KParts::MainWindow`):

- `setupActions()` — register all actions in `KActionCollection`
- `setupSidebars()` — create left sidebar with FileExplorerPanel toolview
- `setupEditor()` — create EditorViewManager as central widget
- `setupStatusBar()` — word count, cursor pos, editor mode
- `saveSession()` / `restoreSession()` — via `.corbomite/session.json`

### RibbonBar

Vertical `QToolBar` at `Qt::LeftToolBarArea`:
- New Note, Open Vault, Settings
- Icon-only, 20x20px, non-movable
- Actions shared with KActionCollection (same shortcuts, same handlers)

### StatusBar

`QStatusBar` with permanent widgets:
- `QLabel` for word count (updated on `textChanged()`)
- `QLabel` for cursor position "Ln X, Col Y" (updated on `cursorPositionChanged()`)
- Connected to active `NoteEditorWidget` — switches when active tab changes

## Editor

### EditorViewManager

Central widget, manages the single `EditorViewSpace` (Phase 1):

```cpp
class EditorViewManager : public QWidget {
public:
    void openNote(NoteDocument *doc);
    void closeNote(const QString &path);
    NoteEditorWidget *activeEditor() const;
    EditorViewSpace *activeViewSpace() const;

signals:
    void activeEditorChanged(NoteEditorWidget *editor);
};
```

### EditorViewSpace

Contains `EditorTabBar` + `QStackedWidget`:

- One `NoteEditorWidget` per open tab in the stacked widget
- Tab activation switches the stacked widget page
- Tab bar and stacked widget stay synchronized

### NoteEditorWidget

Wraps `QMarkdownTextEdit`:

```cpp
class NoteEditorWidget : public QMarkdownTextEdit {
public:
    void setNoteDocument(NoteDocument *doc);
    NoteDocument *noteDocument() const;

    int currentLine() const;
    int currentColumn() const;

signals:
    void cursorInfoChanged(int line, int column, int wordCount);
};
```

- Sets `QMarkdownTextEdit::setPlainText()` from `NoteDocument::markdown()`
- Connects `textChanged()` → `NoteDocument::setMarkdown(toPlainText())`
- Connects `cursorPositionChanged()` → emits `cursorInfoChanged()`
- Inherits from QMarkdownTextEdit: syntax highlighting, search widget, bracket pairing, smart lists, line numbers

## Vault & File System

### FileSystemAdapter

Thin abstraction over `QFile`/`QDir`:

```cpp
class FileSystemAdapter {
public:
    // Returns file content, or std::nullopt on read failure
    std::optional<QString> readFile(const QString &absolutePath) const;
    bool writeFile(const QString &absolutePath, const QString &content);
    bool rename(const QString &oldPath, const QString &newPath);
    bool remove(const QString &absolutePath);
    bool moveToTrash(const QString &absolutePath);
    bool exists(const QString &absolutePath) const;
    bool mkpath(const QString &dirPath);
};
```

Not a QObject — plain class, stateless, testable with `QTemporaryDir`. Uses `std::optional` for fallible reads to distinguish "empty file" from "read error".

### VaultScanner

Scans a vault directory and returns note metadata:

```cpp
class VaultScanner {
public:
    QVector<NoteMeta> scan(const QString &vaultRoot) const;

private:
    bool shouldExclude(const QString &relativePath) const;
    // Excludes: .obsidian/, .corbomite/, .git/, node_modules/, .trash/
};
```

Used by `VaultModel` on vault open. Can run via `QtConcurrent::run()` for large vaults.

### NoteService

Orchestrates note operations:

```cpp
class NoteService : public QObject {
public:
    NoteDocument *openNote(const QString &relativePath);
    NoteDocument *createNote(const QString &name, const QString &folderPath);
    bool renameNote(const QString &oldRelPath, const QString &newRelPath);
    bool deleteNote(const QString &relativePath);
    bool saveNote(NoteDocument *doc);

private:
    VaultModel *m_vault;
    FileSystemAdapter *m_fs;
};
```

### VaultService

Manages vault lifecycle:

```cpp
class VaultService : public QObject {
public:
    bool openVault(const QString &path);
    void closeVault();
    VaultModel *currentVault() const;

    QStringList recentVaults() const;
    void addRecentVault(const QString &path);

signals:
    void vaultOpened(VaultModel *vault);
    void vaultClosed();
};
```

- Stores recent vaults in application-level KConfig
- On open: create VaultModel, scan, start file watcher, restore session
- On close: save session, save dirty notes, stop watcher

### FileWatchReactor

```cpp
class FileWatchReactor : public QObject {
public:
    void startWatching(const QString &vaultRoot);
    void stopWatching();
    void suppressPath(const QString &path);  // Called before our own writes

private:
    QFileSystemWatcher m_watcher;
    QTimer m_debounceTimer;           // 500ms
    QSet<QString> m_pendingChanges;
    QSet<QString> m_suppressedPaths;

    void processPendingChanges();
};
```

- Watches vault root + all subdirectories
- Batches changes, processes after 500ms quiet period
- Suppression prevents reacting to our own writes
- External modification of open dirty note: prompt user via `KMessageBox`

### AutosaveReactor

```cpp
class AutosaveReactor : public QObject {
public:
    void watchDocument(NoteDocument *doc);
    void unwatchDocument(NoteDocument *doc);

private:
    QHash<NoteDocument*, QTimer*> m_timers;  // Per-document debounce
    NoteService *m_noteService;

    void onDocumentModified(NoteDocument *doc, bool modified);
    void onTimerTimeout(NoteDocument *doc);
};
```

- Connects to `NoteDocument::modificationChanged(true)` → starts 2s timer
- Timer restart on each edit (debounce)
- On timeout: `NoteService::saveNote(doc)`
- `suppressPath()` on FileWatchReactor before save

## Settings

### KConfigXT Schema

`corbomite.kcfg`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<kcfg xmlns="http://www.kde.org/standards/kcfg/1.0">
  <kcfgfile name="corbomiterc"/>
  <group name="Editor">
    <entry name="FontSize" type="Int">
      <label>Editor font size in points</label>
      <default>16</default>
    </entry>
    <entry name="FontFamily" type="String">
      <label>Editor font family</label>
      <default></default>
    </entry>
    <entry name="TabSize" type="Int">
      <label>Tab width in spaces</label>
      <default>4</default>
    </entry>
    <entry name="UseTabs" type="Bool">
      <label>Use tab characters instead of spaces</label>
      <default>true</default>
    </entry>
    <entry name="LineNumbers" type="Bool">
      <label>Show line numbers</label>
      <default>false</default>
    </entry>
    <entry name="LineWrap" type="Bool">
      <label>Wrap long lines</label>
      <default>true</default>
    </entry>
    <entry name="ReadableLineLength" type="Bool">
      <label>Constrain text width for readability</label>
      <default>true</default>
    </entry>
    <entry name="AutoPairBrackets" type="Bool">
      <label>Automatically close brackets and quotes</label>
      <default>true</default>
    </entry>
    <entry name="AutoSaveDelayMs" type="Int">
      <label>Autosave delay in milliseconds</label>
      <default>2000</default>
    </entry>
  </group>
  <group name="Files">
    <entry name="TrashOption" type="String">
      <label>Delete behavior: system, vault, or permanent</label>
      <default>system</default>
    </entry>
    <entry name="PromptDelete" type="Bool">
      <label>Show confirmation before deleting notes</label>
      <default>true</default>
    </entry>
  </group>
  <group name="Appearance">
    <entry name="Theme" type="String">
      <label>Color theme: light, dark, or system</label>
      <default>system</default>
    </entry>
    <entry name="AccentColor" type="Color">
      <label>Accent color for interactive elements</label>
      <default>#7b6cd9</default>
    </entry>
  </group>
</kcfg>
```

`corbomitesettings.kcfgc`:
```ini
File=corbomite.kcfg
ClassName=CorbomiteSettings
Mutators=true
Singleton=true
GenerateProperties=true
ParentInConstructor=true
```

### Settings Dialog

`KPageDialog` with three pages:
- **Editor** — Font, tab size, line numbers, line wrap, readable length, auto-pair, autosave delay
- **Files** — Delete behavior (combo: system trash / vault trash / permanent), confirm delete
- **Appearance** — Theme (combo: light / dark / system), accent color picker

Plus `KShortcutsDialog` accessible from Settings menu (KDE provides full shortcut editor UI).

## Session Management

### Session File

`.corbomite/session.json` (per vault):

```json
{
    "windowGeometry": "base64-encoded-QByteArray",
    "windowState": "base64-encoded-QByteArray",
    "leftSidebar": {
        "visible": true,
        "width": 250,
        "activePanel": 0
    },
    "rightSidebar": {
        "visible": false,
        "width": 250,
        "activePanel": 0
    },
    "tabs": [
        {
            "path": "folder/note.md",
            "scrollPosition": 120,
            "cursorLine": 42,
            "cursorColumn": 15,
            "isPinned": false
        }
    ],
    "activeTabIndex": 0,
    "expandedFolders": ["folder", "folder/subfolder"],
    "fileExplorerSortMode": "alphabetical"
}
```

### Save/Restore Triggers

- **Auto-save:** On tab switch, tab open/close, sidebar toggle/resize, window resize. Debounced 2 seconds.
- **Restore:** On vault open, read session.json, restore window geometry, open tabs, set cursor positions, restore sidebar state.
- **First-time vault:** If no `.corbomite/session.json` exists, attempt to read `.obsidian/workspace.json` to get initial tab list.

## Keyboard Shortcuts (Phase 1 Subset)

| Shortcut | Action ID | Description |
|----------|-----------|-------------|
| `Ctrl+N` | `file_new_note` | Create new note |
| `Ctrl+S` | `file_save` | Save current note |
| `Ctrl+W` | `tab_close` | Close current tab |
| `Ctrl+Tab` | `tab_next_lru` | Next tab (LRU) |
| `Ctrl+Shift+Tab` | `tab_prev_lru` | Previous tab (LRU) |
| `Ctrl+Shift+T` | `tab_reopen` | Reopen last closed tab |
| `Ctrl+1`–`Ctrl+8` | `tab_goto_N` | Go to Nth tab |
| `Ctrl+9` | `tab_goto_last` | Go to last tab |
| `Ctrl+F` | `edit_find` | Find in note (QMarkdownTextEdit built-in) |
| `Ctrl+\` | `view_toggle_left_sidebar` | Toggle left sidebar |
| `Ctrl+=` | `view_zoom_in` | Zoom in |
| `Ctrl+-` | `view_zoom_out` | Zoom out |
| `Ctrl+0` | `view_zoom_reset` | Reset zoom |
| `Ctrl+,` | `app_settings` | Open settings |
| `F2` | `file_rename` | Rename current note |

All registered via `KActionCollection` in `MainWindow::setupActions()`. Customizable via `KShortcutsDialog`.

## Testing

### Philosophy

- Tests define **expected behavior**, not observed behavior
- When a test fails, fix the code, not the test
- Tests are the specification — they state what the software should do
- Write tests before or alongside implementation

### Unit Tests

**`tests/core/tst_notemeta.cpp`** — NoteMeta:
- Construction from QFileInfo extracts correct name, relative path
- Path normalization (forward slashes, no leading slash)
- Name extraction strips .md extension
- absolutePath() combines vault root + relative correctly

**`tests/core/tst_notedocument.cpp`** — NoteDocument:
- `setMarkdown()` updates content and emits `textChanged()`
- `isModified()` starts false, becomes true on text change
- `setModified(false)` resets and emits `modificationChanged(false)`
- `wordCount()` counts correctly: empty doc = 0, "hello world" = 2, handles punctuation
- `characterCount()` counts all characters including whitespace

**`tests/storage/tst_filesystemadapter.cpp`** — FileSystemAdapter:
- `writeFile()` + `readFile()` round-trips content exactly (including UTF-8, emoji, newlines)
- `readFile()` on nonexistent path returns empty `QString` and sets error flag via `std::optional` or `std::expected`
- `writeFile()` creates intermediate directories if needed
- `rename()` moves file, old path no longer exists, new path has same content
- `remove()` deletes file, `exists()` returns false after
- `moveToTrash()` removes from original location (platform-dependent behavior)

**`tests/storage/tst_vaultscanner.cpp`** — VaultScanner:
- Scanning temp dir with .md files returns correct NoteMeta list
- Excludes .obsidian/, .corbomite/, .git/, node_modules/
- Handles nested folders correctly
- Empty vault returns empty list
- Non-.md files excluded from results
- .canvas files included in results (FileType differentiation)

**`tests/models/tst_notestreemodel.cpp`** — NotesTreeModel:
- Row count matches number of entries at each level
- Parent/child indices correct for nested folders
- `data(PathRole)` returns correct relative paths
- `data(IsDirectoryRole)` distinguishes files from folders
- Sort mode Alphabetical orders A-Z, folders first
- Sort mode ModifiedNewest orders by mtime descending
- Adding a note to VaultModel inserts row in correct position
- Removing a note removes correct row
- Renaming a note updates in place

**`tests/models/tst_tabmodel.cpp`** — TabModel:
- `openTab()` adds tab, emits `tabAdded`
- Opening already-open note activates existing tab, does not duplicate
- `closeTab()` removes tab, emits `tabRemoved`
- `closeTab()` on dirty tab still closes (autosave handles saving)
- `reopenLastClosed()` restores last closed tab with path and cursor state
- LRU ordering: most recently activated tab first
- `pinTab()` sets pin state, pinned tabs survive Close Others
- `closeOtherTabs()` keeps active + pinned, closes rest
- `moveTab()` reorders, preserves all state

### Integration Tests

**`tests/integration/tst_vault_lifecycle.cpp`:**
- Create temp dir with .md files → open as vault → VaultModel populated correctly
- Create note via NoteService → file appears on disk, VaultModel emits noteAdded, NotesTreeModel has new row
- Rename note via NoteService → old file gone, new file exists, VaultModel emits noteRenamed
- Delete note via NoteService → file removed, VaultModel emits noteRemoved
- Close vault → models cleared, watcher stopped

**`tests/integration/tst_editor_save.cpp`:**
- Open note → NoteDocument has correct content
- Modify text in NoteDocument → isModified() true, tab shows dirty
- Save via NoteService → file on disk matches, isModified() false
- Save preserves UTF-8 encoding exactly

**`tests/integration/tst_filewatch.cpp`:**
- Open vault with watcher → externally write to a .md file → FileWatchReactor detects, VaultModel emits noteModified
- Externally create new .md file → VaultModel emits noteAdded
- Externally delete .md file → VaultModel emits noteRemoved
- Save via NoteService with suppression → FileWatchReactor does NOT fire

**`tests/integration/tst_session.cpp`:**
- Open vault, open tabs → save session → session.json exists with correct structure
- Restore session from session.json → same tabs open, correct cursor positions
- Missing session.json → opens vault with no tabs (clean state)
- Corrupted session.json → opens vault with no tabs, no crash

### Test Infrastructure

- All tests use `QTemporaryDir` for vault fixtures
- Headless: `set_tests_properties(... PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")`
- Each test directory has own `CMakeLists.txt` with `find_package(Qt6 REQUIRED COMPONENTS Test)`
- Link against `Corbomite::Core`, `Corbomite::Storage`, `Corbomite::Models`

## Code from Kate to Adapt (GPLv3)

| Kate Source | Corbomite Destination | Adaptation Needed |
|-------------|----------------------|-------------------|
| `kate/apps/lib/katemdi.h/cpp` | `src/mdi/CorbomiteMDI.h/cpp` | Rename namespace/classes, remove Kate-specific includes and KTextEditor refs |
| `kate/apps/lib/katesplitter.h/cpp` | `src/mdi/CorbomiteSplitter.h/cpp` | Rename class |
| `kate/apps/lib/katetabbar.h/cpp` | `src/editor/EditorTabBar.h/cpp` | Rename, replace `DocOrWidget` with `QString` note paths, remove KTextEditor::Document refs |

Source location: `~/src/kde/src/kate/apps/lib/`

## What This Phase Does NOT Include

- Wikilink/tag/callout/highlight/comment syntax (Phase 2)
- Embeds, block references (Phase 2)
- Property Editor, frontmatter parsing (Phase 2)
- Reading mode, Live Preview mode (Phases 2, 4)
- Search, Quick Switcher, Command Palette (Phase 2)
- Backlinks, outlinks, outline panels (Phase 3)
- Graph view, canvas view (Phases 3, 4)
- Link repair on rename (Phase 3)
- SQLite FTS5 index (Phase 2)
- Daily notes, templates (Phase 4)
- Split panes (Phase 4)
- Plugin system (Phase 5)
