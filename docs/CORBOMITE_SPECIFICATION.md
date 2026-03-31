# Corbomite — Architectural Specification

**A native Qt6/C++ Obsidian-inspired knowledge management application for KDE/Linux**

*Version 0.1 — Draft — 2026-03-30*

---

## Table of Contents

1. [Project Vision & Philosophy](#1-project-vision--philosophy)
2. [Technology Stack](#2-technology-stack)
3. [Architecture Overview](#3-architecture-overview)
4. [Data Models](#4-data-models)
5. [UX Specification](#5-ux-specification)
6. [Editor Subsystem](#6-editor-subsystem)
7. [Vault & File Management](#7-vault--file-management)
8. [Note Linking & Backlinks](#8-note-linking--backlinks)
9. [Graph View](#9-graph-view)
10. [Canvas View](#10-canvas-view)
11. [Search System](#11-search-system)
12. [Properties & Metadata](#12-properties--metadata)
13. [Keyboard Shortcuts & Action System](#13-keyboard-shortcuts--action-system)
14. [Settings & Configuration](#14-settings--configuration)
15. [Session & Workspace Management](#15-session--workspace-management)
16. [Theming & Appearance](#16-theming--appearance)
17. [Class Architecture](#17-class-architecture)
18. [Qt6/KDE Analogue Map](#18-qt6kde-analogue-map)

---

## 1. Project Vision & Philosophy

### 1.1 Mission

Corbomite is a native desktop knowledge management application that faithfully recreates Obsidian's core user experience using Qt6/C++ and KDE frameworks. It targets users who prefer native applications over Electron/web-based tools, while maintaining full compatibility with Obsidian's vault format, markdown extensions, and file organization patterns.

### 1.2 Design Principles

1. **Native first** — Use Qt6 widgets and KDE frameworks (KXmlGui, KTextEditor, KConfig) rather than embedding web views. The application should feel like a first-class KDE citizen.
2. **Vault compatibility** — Read and write the same `.md` files, `.canvas` files, and vault folder structures as Obsidian. A user should be able to switch between Corbomite and Obsidian on the same vault.
3. **Local-first** — All data lives as plain files on the local filesystem. No database is required to use the application (though we use SQLite internally for indexing and caching).
4. **Performance** — Native C++ with efficient data structures. Target <50MB RAM for a 1000-note vault. Sub-100ms startup for cached vaults.
5. **Keyboard-driven** — Every action reachable via keyboard. Full command palette. Customizable shortcuts.
6. **Extensible architecture** — Clean separation of concerns enabling future plugin system.

### 1.3 Non-Goals (v1.0)

- Plugin API compatibility with Obsidian's JavaScript plugin ecosystem
- Obsidian Sync/Publish integration
- Mobile support
- Real-time collaboration / CRDT
- Community plugin marketplace

### 1.4 Compatibility Targets

| Feature | Compatibility Level |
|---------|-------------------|
| Vault folder structure | Full — read/write `.obsidian/` config |
| Markdown files (.md) | Full — CommonMark + GFM + Obsidian extensions |
| Wikilinks `[[]]` | Full |
| Embeds `![[]]` | Full |
| Callouts `> [!type]` | Full |
| Properties/Frontmatter | Full — YAML, types.json |
| Canvas files (.canvas) | Full — read/write JSON format |
| Tags (inline + frontmatter) | Full |
| Block references `^id` | Full |
| Math/LaTeX | Full — KaTeX or MathJax equivalent |
| Mermaid diagrams | Full |
| Hotkeys.json format | Full — read Obsidian keybindings |
| workspace.json | Read — restore Obsidian layouts |
| Community plugin data | Ignored — not loaded/executed |

---

## 2. Technology Stack

### 2.1 Core Dependencies

| Component | Technology | Rationale |
|-----------|-----------|-----------|
| GUI Framework | Qt 6.x (Widgets) | Native performance, mature, KDE ecosystem |
| Application Framework | KXmlGui / KDE Frameworks 6 | Actions, shortcuts, session management, UI merging |
| Text Editor Widget | QMarkdownTextEdit (from QOwnNotes) | Proven Qt markdown editor with syntax highlighting, search, line numbers |
| Markdown Parser | cmark-gfm (C library) + custom extensions | CommonMark + GFM reference implementation; extend for Obsidian syntax |
| Markdown Renderer | QTextDocument + custom rendering pipeline | Native Qt rendering, no web view |
| Search Index | SQLite FTS5 | Full-text search with stemming, proven in Otterly |
| Graph Rendering | QGraphicsView + custom force-directed layout | Native Qt 2D graphics with GPU acceleration |
| Canvas Rendering | QGraphicsView + QGraphicsScene | Native infinite canvas with pan/zoom |
| Math Rendering | KaTeX (via C++ port or subprocess) or QtMath | LaTeX math rendering |
| Diagram Rendering | Mermaid CLI or custom subset | Diagram-to-SVG rendering |
| Configuration | KConfig / KConfigXT | KDE-standard config with .kcfg schema files |
| File Watching | QFileSystemWatcher | Cross-platform file change notifications |
| Build System | CMake | Standard for Qt/KDE projects |
| Language | C++20 | Modern C++ with concepts, ranges, structured bindings |

### 2.2 Optional/Future Dependencies

| Component | Technology | When |
|-----------|-----------|------|
| Spell Checking | Sonnet (KDE) | v1.0 |
| Version Control | libgit2 | v1.1 — Git integration like Otterly |
| PDF Export | QPrinter / QTextDocument::print | v1.0 |
| Syntax Highlighting (code blocks) | KSyntaxHighlighting | v1.0 — 300+ language support |

---

## 3. Architecture Overview

### 3.1 Layered Architecture

Corbomite follows a layered architecture inspired by Otterly's hexagonal pattern, adapted for Qt6/C++:

```
┌─────────────────────────────────────────────────────────────────┐
│                        UI Layer                                 │
│  MainWindow, Sidebars, EditorView, GraphView, CanvasView,      │
│  Dialogs, CommandPalette, StatusBar                             │
│  ── Reads state from Models, dispatches Actions ──              │
├─────────────────────────────────────────────────────────────────┤
│                     Action Registry                             │
│  CorbomiteActionRegistry (central KActionCollection)            │
│  ── Single dispatch surface for all user-triggerable behavior ──│
├─────────────────────────────────────────────────────────────────┤
│                     Service Layer                               │
│  NoteService, VaultService, SearchService, LinkService,         │
│  CanvasService, GraphService, SessionService, SettingsService   │
│  ── Async orchestration, business logic, IO coordination ──     │
├─────────────────────────────────────────────────────────────────┤
│                      Model Layer                                │
│  NoteDocument, VaultModel, NotesModel, TagsModel, LinksModel,   │
│  SearchResultsModel, GraphDataModel, CanvasModel, TabModel      │
│  ── State containers, Qt models, signal-emitting ──             │
├─────────────────────────────────────────────────────────────────┤
│                    Storage / IO Layer                            │
│  FileSystemAdapter, SQLiteIndex, ConfigAdapter,                 │
│  MarkdownParser, CanvasSerializer                               │
│  ── File I/O, database, parsing, serialization ──               │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Key Architectural Patterns

#### Document-View (from KTextEditor)
Each open note is a `NoteDocument` that can have multiple views (split panes, preview alongside source). Documents are independent of their visual representation. Multiple views of the same document share undo history and content state.

#### Model-View (Qt MVC)
All list/tree data uses Qt's model/view framework:
- `QAbstractItemModel` subclasses for notes, tags, folders, search results
- `QSortFilterProxyModel` for filtering and sorting
- Views bind to models via standard Qt interfaces

#### Service-Orchestration (from Otterly)
Services are the only layer that performs async I/O. They read from models, call storage adapters, and update models with results. UI components never perform I/O directly.

#### Signal/Slot Reactors
Persistent connections that watch model changes and trigger follow-up service calls:
- Autosave reactor: watches `NoteDocument::modificationChanged` → triggers save after debounce
- Link sync reactor: watches note saves → triggers backlink reindex
- Tab persistence reactor: watches tab changes → saves session state

#### Action Registry (from KDE/Kate)
All user-triggerable operations are registered as `QAction` objects in a `KActionCollection`. This enables:
- Keyboard shortcuts (customizable via KXmlGui)
- Command palette (fuzzy-search across all actions)
- Menu bar entries
- Toolbar buttons
- Context menu items

All pointing to the same underlying action handler.

### 3.3 Threading Model

| Thread | Responsibility |
|--------|---------------|
| Main (GUI) thread | UI rendering, user input, model updates |
| Index worker thread | SQLite FTS5 indexing, link graph building |
| File I/O thread pool | Note loading/saving, vault scanning |
| Graph layout thread | Force-directed simulation (QtConcurrent) |

Thread communication via `QMetaObject::invokeMethod(Qt::QueuedConnection)` and signals/slots across thread boundaries.

### 3.4 Directory Layout (Source)

```
corbomite/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── CorbomiteApp.h/cpp            # Application singleton
│   │   ├── ActionRegistry.h/cpp           # Central action management
│   │   └── SessionManager.h/cpp           # Workspace session save/restore
│   ├── models/
│   │   ├── NoteDocument.h/cpp             # Single note: text + metadata
│   │   ├── VaultModel.h/cpp               # Vault state and note list
│   │   ├── NotesTreeModel.h/cpp           # File explorer tree model
│   │   ├── TagsModel.h/cpp                # Hierarchical tag model
│   │   ├── LinksModel.h/cpp               # Backlinks/outlinks for a note
│   │   ├── SearchResultsModel.h/cpp       # Search results model
│   │   ├── TabModel.h/cpp                 # Open tabs state
│   │   ├── GraphDataModel.h/cpp           # Note graph nodes + edges
│   │   └── CanvasModel.h/cpp              # Canvas document model
│   ├── services/
│   │   ├── NoteService.h/cpp              # Note CRUD, rename with link repair
│   │   ├── VaultService.h/cpp             # Vault open/close/scan
│   │   ├── SearchService.h/cpp            # Full-text + filename search
│   │   ├── LinkService.h/cpp              # Link extraction, repair, graph
│   │   ├── CanvasService.h/cpp            # Canvas file operations
│   │   ├── GraphService.h/cpp             # Graph data generation
│   │   ├── SettingsService.h/cpp          # Settings read/write
│   │   └── TemplateService.h/cpp          # Template variable expansion
│   ├── storage/
│   │   ├── FileSystemAdapter.h/cpp        # File I/O abstraction
│   │   ├── SQLiteIndex.h/cpp              # FTS5 search index + link cache
│   │   ├── ConfigAdapter.h/cpp            # .obsidian/ config file I/O
│   │   ├── MarkdownParser.h/cpp           # cmark-gfm wrapper + extensions
│   │   └── CanvasSerializer.h/cpp         # .canvas JSON read/write
│   ├── ui/
│   │   ├── MainWindow.h/cpp               # KateMDI-style main window
│   │   ├── editor/
│   │   │   ├── EditorViewManager.h/cpp    # Split pane management
│   │   │   ├── EditorViewSpace.h/cpp      # Tab bar + editor container
│   │   │   ├── NoteEditorWidget.h/cpp     # QMarkdownTextEdit wrapper
│   │   │   ├── NotePreviewWidget.h/cpp    # Rendered markdown view
│   │   │   ├── LivePreviewWidget.h/cpp    # Hybrid live preview editor
│   │   │   ├── PropertyEditorWidget.h/cpp # Frontmatter property editor
│   │   │   └── EditorTabBar.h/cpp         # Custom tab bar (LRU)
│   │   ├── sidebar/
│   │   │   ├── FileExplorerPanel.h/cpp    # Vault folder tree
│   │   │   ├── SearchPanel.h/cpp          # Global search UI
│   │   │   ├── TagsPanel.h/cpp            # Hierarchical tag browser
│   │   │   ├── BookmarksPanel.h/cpp       # Bookmarked items
│   │   │   ├── BacklinksPanel.h/cpp       # Backlinks for current note
│   │   │   ├── OutlinksPanel.h/cpp        # Outgoing links panel
│   │   │   ├── OutlinePanel.h/cpp         # Heading outline tree
│   │   │   └── LocalGraphPanel.h/cpp      # Local graph mini-view
│   │   ├── graph/
│   │   │   ├── GraphView.h/cpp            # QGraphicsView for graph
│   │   │   ├── GraphScene.h/cpp           # QGraphicsScene with nodes/edges
│   │   │   ├── GraphNode.h/cpp            # QGraphicsItem for note nodes
│   │   │   ├── GraphEdge.h/cpp            # QGraphicsItem for link edges
│   │   │   └── ForceLayout.h/cpp          # Force-directed layout engine
│   │   ├── canvas/
│   │   │   ├── CanvasView.h/cpp           # QGraphicsView for canvas
│   │   │   ├── CanvasScene.h/cpp          # QGraphicsScene for canvas
│   │   │   ├── CanvasCard.h/cpp           # Base card item
│   │   │   ├── TextCard.h/cpp             # Standalone text card
│   │   │   ├── NoteCard.h/cpp             # Embedded note card
│   │   │   ├── MediaCard.h/cpp            # Image/media card
│   │   │   ├── LinkCard.h/cpp             # Web URL card
│   │   │   ├── CanvasGroup.h/cpp          # Card group container
│   │   │   └── CanvasConnection.h/cpp     # Edge between cards
│   │   ├── dialogs/
│   │   │   ├── CommandPalette.h/cpp       # Fuzzy command search
│   │   │   ├── QuickSwitcher.h/cpp        # Fuzzy note search
│   │   │   ├── SettingsDialog.h/cpp       # Multi-page settings
│   │   │   ├── VaultPickerDialog.h/cpp    # Vault open/create
│   │   │   ├── LinkSuggestionPopup.h/cpp  # [[wikilink autocomplete
│   │   │   ├── TagSuggestionPopup.h/cpp   # #tag autocomplete
│   │   │   └── PropertyTypeDialog.h/cpp   # Property type picker
│   │   └── widgets/
│   │       ├── StatusBar.h/cpp            # Custom status bar
│   │       ├── RibbonBar.h/cpp            # Left-edge icon strip
│   │       ├── HoverPreview.h/cpp         # Link hover popup
│   │       └── CalloutWidget.h/cpp        # Callout rendering widget
│   └── reactors/
│       ├── AutosaveReactor.h/cpp          # Debounced auto-save
│       ├── LinkSyncReactor.h/cpp          # Reindex links on save
│       ├── TabPersistenceReactor.h/cpp    # Save tab state on change
│       └── FileWatchReactor.h/cpp         # Handle external file changes
├── libs/
│   └── qmarkdowntextedit/                # Git submodule
├── resources/
│   ├── corbomiteui.rc                     # KXmlGui menu/toolbar XML
│   ├── icons/                             # Application icons
│   └── themes/                            # Default CSS themes
└── tests/
    ├── test_markdown_parser.cpp
    ├── test_link_service.cpp
    ├── test_search_service.cpp
    ├── test_canvas_serializer.cpp
    └── test_vault_model.cpp
```

---

## 4. Data Models

### 4.1 NoteDocument

The central data object. Wraps a markdown file with parsed metadata.

```cpp
class NoteDocument : public QObject {
    Q_OBJECT

public:
    // Identity
    QString filePath() const;        // Absolute path: /home/user/vault/folder/note.md
    QString relativePath() const;    // Relative to vault: folder/note.md
    QString name() const;            // Filename without extension: note
    QString title() const;           // From frontmatter title or name()

    // Content
    QString markdown() const;        // Raw markdown text
    void setMarkdown(const QString &text);
    QTextDocument *textDocument();   // Underlying QTextDocument for editor

    // Metadata (parsed from frontmatter)
    QVariantMap properties() const;
    void setProperty(const QString &key, const QVariant &value);
    void removeProperty(const QString &key);
    QStringList tags() const;        // Combined: frontmatter + inline tags
    QStringList aliases() const;     // From frontmatter
    QStringList cssClasses() const;  // From frontmatter

    // State
    bool isModified() const;
    void setModified(bool modified);
    QDateTime fileModifiedTime() const;
    qint64 fileSize() const;

    // Statistics
    int wordCount() const;
    int characterCount() const;
    int lineCount() const;

    // Links (extracted from content)
    QVector<WikiLink> outgoingLinks() const;
    QVector<BlockReference> blockReferences() const;

signals:
    void textChanged();
    void modificationChanged(bool modified);
    void propertiesChanged();
    void titleChanged(const QString &newTitle);
    void saved();
};
```

### 4.2 WikiLink

Represents an internal link found in note content.

```cpp
struct WikiLink {
    enum Type { NoteLink, HeadingLink, BlockLink, Embed };

    Type type;
    QString rawText;           // Full syntax: [[path#heading|display]]
    QString targetPath;        // Resolved target: folder/note.md
    QString heading;           // Optional heading: #Section Title
    QString blockId;           // Optional block ref: ^block-id
    QString displayText;       // Display text override
    bool isEmbed;              // ![[]] vs [[]]
    int startOffset;           // Position in source text
    int endOffset;
};
```

### 4.3 VaultModel

Manages the state of an open vault.

```cpp
class VaultModel : public QObject {
    Q_OBJECT

public:
    // Vault identity
    QString path() const;           // Absolute vault root path
    QString name() const;           // Vault display name (folder name)
    QString configPath() const;     // .obsidian/ (or .corbomite/) path

    // Note collection
    QVector<NoteMeta> allNotes() const;
    NoteMeta noteMeta(const QString &relativePath) const;
    bool noteExists(const QString &relativePath) const;

    // Resolution
    QString resolveWikiLink(const QString &linkTarget,
                           const QString &contextNotePath) const;
    QVector<NoteMeta> resolveAmbiguousLink(const QString &noteName) const;

    // Tag index
    QStringList allTags() const;
    QMap<QString, int> tagCounts() const;  // tag -> count

    // Vault settings
    VaultSettings settings() const;

signals:
    void noteAdded(const QString &relativePath);
    void noteRemoved(const QString &relativePath);
    void noteRenamed(const QString &oldPath, const QString &newPath);
    void noteModified(const QString &relativePath);
    void vaultScanned();  // Initial scan complete
};
```

### 4.4 NoteMeta

Lightweight note metadata (no content loaded).

```cpp
struct NoteMeta {
    QString relativePath;    // folder/note.md
    QString name;            // note
    QString title;           // From frontmatter or name
    QDateTime created;
    QDateTime modified;
    qint64 sizeBytes;
    QStringList tags;        // Cached from index
    QStringList aliases;     // Cached from index
    int linkCount;           // Number of outgoing links
    int backlinkCount;       // Number of incoming links
};
```

### 4.5 NotesTreeModel

Qt item model for the file explorer sidebar.

```cpp
class NotesTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        IsDirectoryRole,
        ModifiedTimeRole,
        TagCountRole,
        LinkCountRole,
        FileTypeRole       // markdown, canvas, image, etc.
    };

    enum SortMode {
        Alphabetical,
        ModifiedNewest,
        ModifiedOldest,
        CreatedNewest,
        CreatedOldest
    };

    // Standard QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Drag and drop (file moves)
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent) override;

    // Operations
    void setSortMode(SortMode mode);
    void setShowHiddenFiles(bool show);
    QModelIndex indexForPath(const QString &relativePath) const;
};
```

### 4.6 TabModel

Manages open tabs across all editor view spaces.

```cpp
struct TabState {
    QString notePath;        // Relative path to note
    int scrollPosition;      // Vertical scroll position
    int cursorLine;          // Cursor line
    int cursorColumn;        // Cursor column
    bool isPinned;
    bool isDirty;
    quint64 lruCounter;      // For LRU tab switching (Kate pattern)
    EditorMode editorMode;   // Source, LivePreview, Reading
};

class TabModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        NotePathRole = Qt::UserRole + 1,
        TitleRole,
        IsPinnedRole,
        IsDirtyRole,
        EditorModeRole
    };

    // Tab operations
    void openTab(const QString &notePath, bool activate = true);
    void closeTab(int index);
    void closeOtherTabs(int keepIndex);
    void closeAllTabs();
    void pinTab(int index, bool pinned);
    void moveTab(int fromIndex, int toIndex);
    void setActiveTab(int index);
    int activeTabIndex() const;

    // LRU navigation
    QList<TabState> lruSortedTabs() const;
    void activateNextLRU();      // Ctrl+Tab behavior
    void activatePreviousLRU();  // Ctrl+Shift+Tab

    // Closed tab history
    void reopenLastClosedTab();

signals:
    void activeTabChanged(int index);
    void tabAdded(int index);
    void tabRemoved(int index);
    void tabDirtyChanged(int index, bool dirty);
};
```

### 4.7 GraphDataModel

Node-edge graph representing note connections.

```cpp
struct GraphNode {
    QString notePath;        // Note relative path (node ID)
    QString title;           // Display label
    NodeType type;           // Regular, Unresolved, Attachment, Tag, Orphan
    int connectionCount;     // Degree — used for node sizing
    QPointF position;        // Layout position
    bool isPinned;           // User pinned position
    QColor groupColor;       // From color groups
};

struct GraphEdge {
    QString sourceNode;
    QString targetNode;
};

enum class NodeType { Regular, Unresolved, Attachment, Tag, Orphan };

class GraphDataModel : public QObject {
    Q_OBJECT

public:
    // Data access
    QVector<GraphNode> nodes() const;
    QVector<GraphEdge> edges() const;
    GraphNode node(const QString &notePath) const;
    QVector<GraphNode> neighbors(const QString &notePath, int depth = 1) const;

    // Building
    void buildFromVault(const VaultModel *vault, const SQLiteIndex *index);
    void buildLocalGraph(const QString &centerNotePath, int depth);

    // Filtering
    void setSearchFilter(const QString &query);
    void setShowTags(bool show);
    void setShowAttachments(bool show);
    void setShowOrphans(bool show);
    void setShowUnresolved(bool show);

    // Color groups
    void addColorGroup(const QString &name, const QString &query, const QColor &color);
    void removeColorGroup(const QString &name);

signals:
    void graphChanged();
    void layoutUpdated();
};
```

### 4.8 CanvasModel

Represents a `.canvas` document — fully compatible with Obsidian's JSON format.

```cpp
struct CanvasNode {
    QString id;              // Unique ID
    CanvasNodeType type;     // Text, File, Link, Group
    double x, y;             // Position
    double width, height;    // Dimensions
    int color;               // 0-6 (Obsidian's color palette)

    // Type-specific data
    QString text;            // For Text nodes: markdown content
    QString filePath;        // For File nodes: relative path
    QString subpath;         // For File nodes: #heading or #^block
    QString url;             // For Link nodes: URL
    QString label;           // For Group nodes: group label
};

struct CanvasEdge {
    QString id;
    QString fromNode;
    QString toNode;
    QString fromSide;        // "top", "right", "bottom", "left"
    QString toSide;
    int color;
    QString label;
};

enum class CanvasNodeType { Text, File, Link, Group };

class CanvasModel : public QObject {
    Q_OBJECT

public:
    // Nodes
    QVector<CanvasNode> nodes() const;
    CanvasNode node(const QString &id) const;
    QString addNode(const CanvasNode &node);
    void updateNode(const QString &id, const CanvasNode &node);
    void removeNode(const QString &id);

    // Edges
    QVector<CanvasEdge> edges() const;
    QString addEdge(const CanvasEdge &edge);
    void removeEdge(const QString &id);

    // Groups
    QVector<CanvasNode> groups() const;
    QVector<CanvasNode> nodesInGroup(const QString &groupId) const;

    // Serialization (Obsidian-compatible JSON)
    static CanvasModel *fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

    // State
    bool isModified() const;

signals:
    void nodeAdded(const QString &id);
    void nodeChanged(const QString &id);
    void nodeRemoved(const QString &id);
    void edgeAdded(const QString &id);
    void edgeRemoved(const QString &id);
    void modificationChanged(bool modified);
};
```

### 4.9 SearchResultsModel

```cpp
struct SearchMatch {
    QString notePath;
    QString contextBefore;   // Text before match
    QString matchText;       // The matched text
    QString contextAfter;    // Text after match
    int lineNumber;
    int columnStart;
    double relevanceScore;
};

struct SearchFileResult {
    NoteMeta note;
    QVector<SearchMatch> matches;
    int totalMatchCount;
};

class SearchResultsModel : public QAbstractItemModel {
    Q_OBJECT

public:
    // Two-level tree: file -> matches within file
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    // Population
    void setResults(const QVector<SearchFileResult> &results);
    void clear();

    // Sorting
    enum SortMode { Relevance, FileName, ModifiedNewest, ModifiedOldest, CreatedTime };
    void setSortMode(SortMode mode);

    // Stats
    int fileCount() const;
    int totalMatchCount() const;

signals:
    void resultsReady();
};
```

---

## 5. UX Specification

### 5.1 Overall Layout

Corbomite uses a KateMDI-inspired layout with collapsible sidebars:

```
┌──────────────────────────────────────────────────────────────────────┐
│                          Title Bar (KDE)                             │
├────┬─────────────────────────────────────────────────────────┬───────┤
│    │              Tab Bar (EditorTabBar)                     │       │
│ R  ├──────────┬──────────────────────────────────────────────┤  R    │
│ I  │          │                                              │  I    │
│ B  │  LEFT    │         Main Editor Area                     │  G    │
│ B  │  SIDE    │        (EditorViewManager)                   │  H    │
│ O  │  BAR     │                                              │  T    │
│ N  │          │    Can be split horizontally/vertically       │       │
│    │ File     │    into multiple EditorViewSpaces,            │  S    │
│ B  │ Explorer │    each with its own tab bar                 │  I    │
│ A  │ Search   │                                              │  D    │
│ R  │ Tags     │                                              │  E    │
│    │ Bookmarks│                                              │  B    │
│    │          │                                              │  A    │
│    │          │                                              │  R    │
├────┴──────────┴──────────────────────────────────────────────┴───────┤
│                         Status Bar                                   │
└──────────────────────────────────────────────────────────────────────┘
```

### 5.2 Ribbon Bar (Left-Edge Icon Strip)

A narrow vertical toolbar on the far left edge (like Obsidian's ribbon):

| Icon | Action | Default |
|------|--------|---------|
| File+ | Create new note | Always visible |
| Folder | Open vault | Always visible |
| Search | Open Quick Switcher | Always visible |
| Graph | Open Graph View | Always visible |
| Canvas+ | Create new canvas | Always visible |
| Calendar | Open today's daily note | If daily notes enabled |
| Command | Open Command Palette | Always visible |
| Settings | Open Settings | Always visible |
| Left Sidebar | Toggle left sidebar | Always visible |

Implementation: `QToolBar` with `Qt::LeftToolBarArea`, vertical orientation, icon-only mode, small icon size (20x20).

### 5.3 Left Sidebar

A `QDockWidget` (or KateMDI::Sidebar) containing a `QStackedWidget` with switchable panels. Panel tabs along the top edge:

**Panels:**
1. **File Explorer** — Vault folder tree (`QTreeView` + `NotesTreeModel`)
   - Create note/folder buttons at top
   - Sort mode dropdown
   - Inline rename (F2)
   - Drag-drop file moves
   - Context menu: New note, New folder, Rename, Delete, Move, Duplicate, Copy path, Reveal in file manager
   - Show/hide hidden files toggle
   - File type icons (note, canvas, image, folder)

2. **Search** — Global vault search
   - Search input with mode indicators
   - Real-time results as you type
   - Results grouped by file with context previews
   - Match highlighting
   - Sort controls (relevance, name, date)
   - Collapse/expand individual results
   - Click to navigate to match

3. **Tags** — Hierarchical tag browser (`QTreeView`)
   - Nested tag tree (parent/child via `/` separator)
   - Count badge per tag
   - Click to filter notes by tag
   - Expand/collapse tag hierarchy

4. **Bookmarks** — Bookmarked items (`QListView`)
   - Bookmarked notes, folders, searches, headings
   - Bookmark groups (folders)
   - Drag-drop reordering
   - Right-click to remove

**Sidebar behavior:**
- Toggle with `Ctrl+\` (matches Obsidian)
- Resizable width (drag edge)
- Minimum width 200px, maximum 500px
- Remembers which panel was active
- Panel switch via tab icons at top

### 5.4 Right Sidebar

Same structure as left sidebar, positioned on right edge:

**Panels:**
1. **Backlinks** — Notes linking to current note
   - Grouped by source note
   - Shows surrounding context paragraph
   - Linked mentions (explicit `[[]]` links)
   - Unlinked mentions (text matches without wiki syntax)
   - Count badge
   - Click to navigate

2. **Outgoing Links** — Links from current note
   - Existing links (resolved targets)
   - Unresolved links (target doesn't exist)
   - Click to navigate; click unresolved to create note

3. **Outline** — Heading structure of current note
   - `QTreeView` with heading hierarchy (H1-H6)
   - Click to scroll to heading
   - Real-time update on edit
   - Collapse/expand heading levels

4. **Local Graph** — Mini graph for current note
   - Small `GraphView` showing 1-2 hop neighborhood
   - Updates as you switch notes
   - Click node to navigate

**Toggle:** `Ctrl+Shift+\`

### 5.5 Tab Bar

Custom `QTabBar` subclass (inspired by Kate's `KateTabBar`):

- Horizontal tabs above editor area
- Each open note = one tab
- Tab shows note title (or filename)
- Close button (X) on hover
- Middle-click to close
- Drag tabs to reorder
- Drag tab out to create split pane
- Right-click context menu:
  - Close
  - Close Others
  - Close All
  - Close to the Right
  - Pin/Unpin Tab
  - Split Right
  - Split Down
  - Copy File Path
  - Reveal in File Explorer
- Pinned tabs: smaller, no close button, left-aligned
- Modified indicator: dot on title when dirty
- New tab button (+) at end
- LRU counter for Ctrl+Tab switching (Kate pattern)
- Tab count limit configurable (default: unlimited)
- Overflow: scroll arrows when too many tabs

### 5.6 Split Panes

`EditorViewManager` manages nested `QSplitter` hierarchy (Kate pattern):

```
EditorViewManager (root QSplitter)
├── EditorViewSpace (tab bar + stacked editor)
├── QSplitter (vertical)
│   ├── EditorViewSpace
│   └── EditorViewSpace
```

- Split horizontally: `Ctrl+Shift+Right` or menu
- Split vertically: `Ctrl+Shift+Down` or menu
- Each pane has its own tab bar and active note
- Panes resizable via splitter handles
- Recursive nesting supported
- Close pane: close all tabs in pane, pane collapses
- Drag tab to another pane to move it
- Session save/restore of split layout

### 5.7 Status Bar

Custom `QStatusBar` with segmented display:

| Segment | Content | Position |
|---------|---------|----------|
| Backlink count | "3 backlinks" — clickable, opens backlinks panel | Left |
| Word count | "1,234 words" | Center-left |
| Character count | "7,891 chars" | Center |
| Cursor position | "Ln 42, Col 15" | Center-right |
| Editor mode | "Source" / "Live Preview" / "Reading" — clickable to switch | Right |
| Encoding | "UTF-8" | Right |
| Spell check | Language indicator | Right |

### 5.8 Hover Preview

When hovering over a `[[wikilink]]` in the editor:

- After 300ms delay, show a floating `QFrame` popup
- Popup contains rendered markdown of the linked note
- Maximum size: 400x300px
- Scrollable if content overflows
- Positioned near cursor, avoiding screen edges
- Dismisses when mouse moves away from link and popup
- `Ctrl+hover` shows immediately (no delay)
- Header bar shows note title

Implementation: `HoverPreview` widget, a `QFrame` with `Qt::Popup` flag, containing a `NotePreviewWidget`.

### 5.9 Modal Dialogs

#### Quick Switcher (`Ctrl+O`)
- Full-width popup at top of window (like Obsidian/Kate QuickOpen)
- Single-line text input with fuzzy matching
- Results list below showing matching note names
- Recent notes shown when input is empty
- Shows folder path for disambiguation
- `Enter` to open, `Esc` to close
- Arrow keys to navigate results
- Create new note if no match (with confirmation)
- Alias matching (matches frontmatter `aliases`)

Implementation: `QuickSwitcher` — `QFrame` with `QLineEdit` + `QListView` + fuzzy `QSortFilterProxyModel`. Fuzzy matching algorithm from KDE's `kfts_fuzzy_match.h`.

#### Command Palette (`Ctrl+P`)
- Same visual style as Quick Switcher
- Fuzzy search across all registered `QAction`s
- Shows action name + assigned keyboard shortcut
- Grouped by category (Editor, File, Navigation, View, etc.)
- `Enter` to execute action

Implementation: `CommandPalette` — same widget as QuickSwitcher but backed by `ActionRegistry` model.

#### Settings Dialog (`Ctrl+,`)
- Multi-page dialog with left sidebar navigation
- Categories:
  - **Editor** — Font, font size, line height, tab size, editor mode, auto-pair, vim mode, spell check, line numbers, readable line length, strict line breaks
  - **Files & Links** — Default note location, attachment folder, link format (wiki vs markdown, shortest/relative/absolute), auto-update links on rename, delete behavior (system trash/vault trash/permanent), excluded files
  - **Appearance** — Theme, accent color, font families (interface, text, monospace), CSS snippets toggle
  - **Hotkeys** — Full shortcut editor: search by command or key, reassign, conflict detection, reset to default
  - **Core Features** — Enable/disable: Daily Notes (+ date format, folder, template), Templates (folder), Graph View, Canvas, Bookmarks, Outline, Tags, Backlinks, etc.
  - **About** — Version, license, links

Implementation: `SettingsDialog` — `QDialog` with `QListWidget` (navigation) + `QStackedWidget` (pages). Uses KConfigXT for persistence.

#### Link Suggestion Popup
- Appears when typing `[[` in editor
- Dropdown below cursor showing matching note names
- Fuzzy-filtered as you type
- Shows note path for disambiguation
- `Tab`/`Enter` to accept, `Esc` to dismiss
- Pipe `|` switches to display text entry

Implementation: `LinkSuggestionPopup` — `QFrame` with `QListView`, positioned via `QTextCursor::position()`.

#### Tag Suggestion Popup
- Appears when typing `#` in editor
- Shows existing tags from vault, fuzzy-filtered
- Hierarchical display for nested tags
- `Tab`/`Enter` to accept

### 5.10 Context Menus

**File Explorer right-click:**
- New Note
- New Folder
- New Canvas
- ---
- Rename (F2)
- Delete
- Move to...
- Duplicate
- ---
- Copy Path
- Copy Relative Path
- Reveal in File Manager
- ---
- Set as Attachment Folder (on folders)

**Editor right-click:**
- Cut / Copy / Paste
- ---
- Toggle Bold / Italic / Strikethrough / Highlight / Code
- ---
- Insert Link
- Insert Embed
- Insert Callout
- Insert Code Block
- Insert Table
- ---
- Search Vault for Selection
- Extract Selection to New Note

**Tab right-click:**
- Close
- Close Others
- Close All
- Close to the Right
- ---
- Pin/Unpin
- ---
- Split Right
- Split Down
- ---
- Copy File Path
- Reveal in File Explorer

**Link right-click (in editor):**
- Open in New Tab
- Open in New Pane
- Copy Link
- Copy Target Path

---

## 6. Editor Subsystem

### 6.1 Editor Modes

Corbomite supports three editor modes matching Obsidian:

#### Source Mode
- Raw markdown text editing with syntax highlighting
- All markdown syntax visible (`[[]]`, `**bold**`, `# heading`, etc.)
- Frontmatter shown as raw YAML between `---` delimiters
- Powered by `QMarkdownTextEdit` with `MarkdownHighlighter`
- This is the primary editing mode and the simplest to implement

#### Live Preview Mode (Default)
- Hybrid mode: the cursor line shows raw markdown; other lines show rendered output
- Headings render at appropriate sizes
- Bold/italic text renders formatted
- Links show as clickable text (reveal `[[]]` syntax when cursor is on them)
- Images render inline
- Code blocks render with syntax highlighting
- Checkboxes are clickable
- Callouts render with icons and colored backgrounds
- Frontmatter displayed as structured Property Editor widget

Implementation approach: Custom `QPlainTextEdit` subclass that uses `QTextDocument` with dynamically-toggled block formatting. When cursor enters a block, switch to source representation; when cursor leaves, switch to rendered representation. This requires a custom document layout or careful use of `QSyntaxHighlighter` + `QTextBlockFormat` + invisible characters.

Alternative approach (simpler, recommended for v1.0): Side-by-side source + preview in a single pane, with synchronized scrolling. The "live preview" is a rendered `QTextBrowser` that updates as you type.

#### Reading Mode
- Fully rendered, non-editable view
- All markdown rendered to final form
- Interactive elements: checkboxes clickable, links navigable
- Code blocks have copy button
- Toggle with `Ctrl+E`

Implementation: `NotePreviewWidget` — a `QTextBrowser` subclass that renders markdown to HTML via the `MarkdownParser`, with custom CSS for Obsidian-compatible styling.

### 6.2 QMarkdownTextEdit Integration

The `qmarkdowntextedit` library provides the Source Mode editor foundation:

**Features we get for free:**
- Markdown syntax highlighting (headings, bold, italic, code, links, lists, etc.)
- 20+ programming language syntax highlighting in code blocks
- Auto-bracket closing (`()`, `[]`, `{}`, `""`, etc.)
- Smart Return: list continuation, ordered list auto-increment, checkbox continuation
- Smart Tab: indent/unindent list items, block indentation
- In-editor search widget (find/replace with regex support)
- Line number area with bookmarks
- Ctrl+Click link opening
- Zoom in/out
- Line duplication, move lines up/down

**Extensions we need to add (subclass as `CorbomiteEditor`):**
- Wikilink syntax highlighting and detection (`[[...]]`, `![[...]]`)
- Wikilink autocomplete popup (triggered on `[[`)
- Tag autocomplete popup (triggered on `#`)
- Callout syntax highlighting and rendering
- Block reference highlighting (`^block-id`)
- Math/LaTeX inline highlighting (`$...$`, `$$...$$`)
- Mermaid code block detection
- Highlight syntax (`==text==`)
- Comment syntax (`%%...%%`)
- Frontmatter/YAML detection and property editor integration
- Hover preview on wikilinks
- Image paste handling (save to attachment folder, insert embed)
- Heading fold/collapse
- Vim mode (optional — can use QVim or similar)

### 6.3 Markdown Rendering Pipeline

For Reading Mode and Live Preview rendering:

```
Markdown text (QString)
    │
    ▼
MarkdownParser::parse(text)          ── cmark-gfm with custom extensions
    │
    ▼
AST (Abstract Syntax Tree)           ── cmark_node tree
    │
    ▼
ObsidianExtensionPass::transform(ast) ── Process wikilinks, callouts, etc.
    │
    ▼
HtmlRenderer::render(ast)            ── Convert to HTML with CSS classes
    │
    ▼
HTML (QString)                        ── With Obsidian-compatible CSS classes
    │
    ▼
QTextBrowser::setHtml(html)          ── Or QTextDocument for richer rendering
```

**Custom AST extensions to handle:**
1. `[[wikilink]]` → `<a class="internal-link" href="...">text</a>`
2. `![[embed]]` → inline content of referenced note/image/media
3. `> [!type] Title` → `<div class="callout callout-type">...</div>`
4. `==highlight==` → `<mark>highlight</mark>`
5. `$math$` / `$$math$$` → rendered math (KaTeX HTML or SVG)
6. `%%comment%%` → stripped from output
7. `#tag` → `<a class="tag" href="...">#tag</a>`
8. `^block-id` → `<span id="block-id"></span>` (hidden anchor)
9. Mermaid code blocks → rendered SVG diagram

### 6.4 Property Editor Widget

When Live Preview or Reading mode is active, frontmatter YAML is replaced with a structured form widget:

```
┌─────────────────────────────────────────────┐
│ Properties                            [raw] │
├─────────────────────────────────────────────┤
│ title:     [My Note Title          ]        │
│ date:      [2024-01-15        ] [📅]        │
│ tags:      [project] [research] [+]         │
│ rating:    [8        ] [▲][▼]               │
│ completed: [✓]                              │
│                                             │
│ [+ Add property]                            │
└─────────────────────────────────────────────┘
```

**Property type → Qt widget mapping:**

| Property Type | Qt Widget |
|---------------|-----------|
| text | `QLineEdit` |
| list/multitext | Custom tag widget (pills with X buttons + add) |
| number | `QSpinBox` or `QDoubleSpinBox` |
| checkbox | `QCheckBox` |
| date | `QDateEdit` with calendar popup |
| datetime | `QDateTimeEdit` with calendar popup |

**Property type inference:**
- Read from `.obsidian/types.json` (vault-wide type definitions)
- Write updated types back to `types.json` when user changes a property type
- Context menu on property name: Rename, Delete, Change Type

Implementation: `PropertyEditorWidget` — a `QWidget` with `QFormLayout`, dynamically populated from parsed YAML frontmatter. Editing updates the underlying `NoteDocument`'s frontmatter.

### 6.5 Heading Folding

- Each heading (H1-H6) gets a collapse/expand arrow in the gutter
- Clicking the arrow hides all content under that heading until the next heading of equal or higher level
- Fold state is per-session (not persisted to file)
- `Ctrl+Click` on fold arrow: fold/unfold all headings at that level
- Commands: Fold All, Unfold All, Toggle Fold at cursor

Implementation: Use `QTextBlock::setVisible(false)` on collapsed blocks, with a custom gutter widget that paints fold indicators.

---

## 7. Vault & File Management

### 7.1 Vault Structure

Corbomite reads and writes the same vault structure as Obsidian:

```
MyVault/
├── .obsidian/                  # Obsidian config (read for compat)
│   ├── app.json                # Read: editor settings
│   ├── hotkeys.json            # Read: custom keybindings
│   ├── workspace.json          # Read: restore layout
│   ├── core-plugins.json       # Read: enabled features
│   ├── types.json              # Read/Write: property type definitions
│   ├── bookmarks.json          # Read/Write: bookmarks
│   ├── graph.json              # Read/Write: graph settings
│   └── snippets/               # Read: CSS snippets
├── .corbomite/                 # Corbomite-specific data
│   ├── index.sqlite            # FTS5 search index + link cache
│   ├── session.json            # Current session state
│   ├── settings.json           # Corbomite-specific settings
│   └── cache/                  # Rendered markdown cache
├── [user folders and notes]
└── [attachments]
```

**Compatibility strategy:**
- Read `.obsidian/` configs for settings, hotkeys, workspace, bookmarks
- Write shared configs back to `.obsidian/` (types.json, bookmarks.json, graph.json)
- Store Corbomite-specific data in `.corbomite/` to avoid conflicts
- Never modify `.obsidian/app.json` or `workspace.json` (let Obsidian own those)
- The `.corbomite/` directory is gitignored by default

### 7.2 Vault Lifecycle

#### Opening a Vault
1. User selects folder via `VaultPickerDialog` or recent vault list
2. `VaultService::openVault(path)`:
   a. Validate directory exists and is readable
   b. Read `.obsidian/` configs (if present)
   c. Create `.corbomite/` if it doesn't exist
   d. Scan all `.md` and `.canvas` files recursively
   e. Build `NotesTreeModel` from file listing
   f. Open/create `SQLiteIndex` at `.corbomite/index.sqlite`
   g. Trigger full reindex if needed (first open or stale index)
   h. Start `QFileSystemWatcher` on vault root
   i. Restore session from `.corbomite/session.json`
   j. Emit `VaultModel::vaultScanned()`

#### Closing a Vault
1. Save all dirty notes
2. Save session state to `.corbomite/session.json`
3. Stop file watcher
4. Close SQLite index
5. Clear all models

### 7.3 File Operations

All file operations go through `NoteService` which coordinates I/O with model updates and link repair:

#### Create Note
```
NoteService::createNote(name, folderPath, template?)
  1. Validate: no conflict at target path
  2. If template: expand template variables ({{title}}, {{date}}, {{time}})
  3. Write .md file to disk via FileSystemAdapter
  4. Add to VaultModel + NotesTreeModel
  5. Index in SQLiteIndex
  6. Open in editor (new tab)
  7. Return NoteDocument*
```

#### Rename Note
```
NoteService::renameNote(oldPath, newPath)
  1. Validate: no conflict at new path
  2. Rename file on disk
  3. Find all notes linking to oldPath (via SQLiteIndex backlinks)
  4. For each linking note:
     a. Replace [[oldPath]] with [[newPath]] in content
     b. Replace [text](oldPath.md) with [text](newPath.md)
     c. Respect link format setting (shortest/relative/absolute)
     d. Save updated note
  5. Update SQLiteIndex (remove old, add new path)
  6. Update VaultModel
  7. Update any open editor tabs/documents
  8. Emit noteRenamed(oldPath, newPath)
```

#### Delete Note
```
NoteService::deleteNote(path, trashOption)
  1. Confirm with user (if confirmation enabled)
  2. Depending on trashOption:
     - SystemTrash: QFile::moveToTrash()
     - VaultTrash: move to .trash/ folder in vault
     - Permanent: QFile::remove()
  3. Remove from SQLiteIndex
  4. Remove from VaultModel + NotesTreeModel
  5. Close any open tabs for this note
  6. Emit noteRemoved(path)
```

#### Move Note
```
NoteService::moveNote(oldPath, newFolderPath)
  → Same as rename but with folder change
  → Link repair updates paths accordingly
```

### 7.4 File Watching

`FileWatchReactor` monitors the vault for external changes:

```cpp
class FileWatchReactor : public QObject {
    QFileSystemWatcher m_watcher;
    QTimer m_debounceTimer;           // 500ms debounce
    QSet<QString> m_pendingChanges;
    QSet<QString> m_suppressedPaths;  // Paths we changed ourselves

    // Excluded patterns (never watch):
    // .corbomite/, .obsidian/workspace.json, node_modules/, .git/

    void onFileChanged(const QString &path);
    void onDirectoryChanged(const QString &path);
    void processPendingChanges();
};
```

**Change handling:**
- File modified externally → reload note content, prompt if open and dirty
- File created externally → add to VaultModel, index
- File deleted externally → remove from VaultModel, close tab if open
- File renamed externally → detect via delete+create pair, update models

**Suppression:** When Corbomite saves a file, add its path to `m_suppressedPaths` to avoid re-processing our own writes. Clear after debounce period.

### 7.5 Attachment Handling

When user pastes an image or drops a file into the editor:

1. Determine attachment folder based on setting:
   - Vault root
   - Same folder as current note
   - Specified folder (e.g., `Attachments/`)
   - Subfolder under current note's folder (e.g., `./assets/`)
2. Generate filename: `Pasted image YYYYMMDDHHMMSS.png` (for clipboard)  or keep original filename (for dropped files)
3. Save file to attachment folder
4. Insert embed syntax at cursor: `![[filename.png]]`
5. If duplicate filename, append counter: `image 1.png`, `image 2.png`

### 7.6 Templates

`TemplateService` handles template expansion:

**Template variables:**

| Variable | Expansion |
|----------|-----------|
| `{{title}}` | Name of the note being created |
| `{{date}}` | Current date in configured format (default: `YYYY-MM-DD`) |
| `{{time}}` | Current time in configured format (default: `HH:mm`) |
| `{{date:FORMAT}}` | Date with custom format string |
| `{{time:FORMAT}}` | Time with custom format string |

**Template folder:** Configurable path in settings. All `.md` files in that folder are available as templates.

**Template insertion:** Via Command Palette "Insert template" → shows list of templates → inserts content at cursor with variables expanded.

### 7.7 Daily Notes

When Daily Notes feature is enabled:

- **Open today's note:** Command/ribbon button creates or opens note named with today's date
- **Date format:** Configurable (e.g., `YYYY-MM-DD`, `DD-MM-YYYY`)
- **Folder:** Configurable (e.g., `Daily Notes/`)
- **Template:** Configurable template to use for new daily notes
- **Open on startup:** Optional setting
- **Navigate:** Commands for next/previous daily note

---

## 8. Note Linking & Backlinks

### 8.1 Link Extraction

`LinkService` extracts links from note content using regex and parser:

**Link types detected:**

| Syntax | Type | Example |
|--------|------|---------|
| `[[Note]]` | Wiki link | `[[My Note]]` |
| `[[Note\|Display]]` | Wiki link with alias | `[[My Note\|displayed text]]` |
| `[[Note#Heading]]` | Heading link | `[[My Note#Section 1]]` |
| `[[Note#^block-id]]` | Block link | `[[My Note#^abc123]]` |
| `[[#Heading]]` | Same-note heading link | `[[#Section 1]]` |
| `![[Note]]` | Note embed | `![[My Note]]` |
| `![[image.png]]` | Image embed | `![[photo.png]]` |
| `![[image.png\|640]]` | Sized image embed | `![[photo.png\|640]]` |
| `[text](note.md)` | Markdown link | `[click](note.md)` |
| `#tag` | Tag | `#project/active` |

**Link resolution algorithm:**
1. If target contains `/` → treat as relative or absolute path
2. If target has no path separator:
   a. Check for exact filename match in vault
   b. If multiple matches, prefer note in same folder
   c. If still ambiguous, use shortest path
3. Apply configured link format (shortest/relative/absolute)

### 8.2 Backlink Index

`SQLiteIndex` maintains a link index table:

```sql
-- Note metadata
CREATE TABLE notes (
    path TEXT PRIMARY KEY,
    title TEXT,
    modified_ms INTEGER,
    size_bytes INTEGER,
    frontmatter_json TEXT     -- Cached parsed frontmatter
);

-- Full-text search
CREATE VIRTUAL TABLE notes_fts USING fts5(
    path,
    title,
    content,
    tags,
    tokenize = 'porter unicode61'
);

-- Link index (for backlinks)
CREATE TABLE links (
    source_path TEXT NOT NULL,
    target_path TEXT NOT NULL,     -- Resolved target
    raw_text TEXT,                 -- Original [[syntax]]
    link_type TEXT,                -- wiki, markdown, embed, tag
    line_number INTEGER,
    PRIMARY KEY (source_path, target_path, line_number)
);

CREATE INDEX idx_links_target ON links(target_path);

-- Tag index
CREATE TABLE note_tags (
    note_path TEXT NOT NULL,
    tag TEXT NOT NULL,
    source TEXT,                   -- 'frontmatter' or 'inline'
    PRIMARY KEY (note_path, tag)
);

CREATE INDEX idx_tags_tag ON note_tags(tag);

-- Block reference index
CREATE TABLE block_refs (
    note_path TEXT NOT NULL,
    block_id TEXT NOT NULL,
    line_number INTEGER,
    PRIMARY KEY (note_path, block_id)
);
```

### 8.3 Link Repair on Rename

When a note is renamed/moved, `LinkService::repairLinks()`:

1. Query `links` table for all rows where `target_path = oldPath`
2. For each source note:
   a. Load note content
   b. Find and replace link references:
      - `[[OldName]]` → `[[NewName]]`
      - `[[OldName|display]]` → `[[NewName|display]]`
      - `[[OldName#heading]]` → `[[NewName#heading]]`
      - `[text](old-path.md)` → `[text](new-path.md)`
   c. Use regex with word boundary checking to avoid substring corruption
   d. Save updated note
   e. Update link index entries

### 8.4 Backlinks Panel

The backlinks panel (`BacklinksPanel`) shows:

**Linked Mentions:** Notes with explicit `[[currentNote]]` links
- Each entry shows: source note title, context paragraph around the link
- Click to navigate to that note at the link location

**Unlinked Mentions:** Notes containing the current note's name as plain text
- Shown separately below linked mentions
- "Link" button to convert plain text to `[[wikilink]]`
- Case-insensitive matching
- Excludes code blocks and already-linked instances

---

## 9. Graph View

### 9.1 Architecture

The graph view uses Qt's `QGraphicsView` framework:

```
GraphView (QGraphicsView)
    └── GraphScene (QGraphicsScene)
            ├── GraphNode items (QGraphicsEllipseItem subclass)
            ├── GraphEdge items (QGraphicsLineItem subclass)
            └── GraphLabel items (QGraphicsTextItem)
```

### 9.2 Force-Directed Layout

`ForceLayout` runs a physics simulation (ForceAtlas2-inspired):

```cpp
class ForceLayout : public QObject {
    Q_OBJECT

public:
    // Parameters (configurable via UI sliders)
    double centerForce = 0.01;     // Pull toward center
    double repelForce = 1500.0;    // Node repulsion (Coulomb)
    double linkForce = 0.05;       // Edge spring constant
    double linkDistance = 100.0;    // Preferred edge length
    double damping = 0.85;         // Velocity damping

    // Control
    void start();                  // Begin simulation
    void stop();                   // Pause simulation
    void reset();                  // Reset to default parameters
    void step();                   // Single simulation step

    // Stability detection
    bool isStable() const;         // Kinetic energy below threshold

signals:
    void layoutUpdated(const QHash<QString, QPointF> &positions);

private:
    void applyForces();            // Compute forces on all nodes
    QPointF computeRepulsion(const GraphNode &a, const GraphNode &b);
    QPointF computeAttraction(const GraphNode &a, const GraphNode &b);
    QPointF computeCenterPull(const GraphNode &node);

    QTimer m_timer;                // 16ms tick (60fps) via QtConcurrent
};
```

### 9.3 Graph Interactions

| Interaction | Behavior |
|-------------|----------|
| Click empty space + drag | Pan |
| Scroll wheel | Zoom in/out |
| Click node | Open note in editor |
| Hover node | Highlight node + direct connections, dim others |
| Drag node | Move node, pin in place |
| Double-click pinned node | Unpin |
| Right-click node | Context menu: Open in new tab, Reveal in explorer |
| Right-click empty | Zoom to fit, Reset layout |

### 9.4 Graph Controls Panel

Floating panel in graph view (collapsible):

**Filters:**
- Search filter text input
- Toggle: Tags as nodes
- Toggle: Attachments
- Toggle: Existing files only
- Toggle: Orphan notes

**Groups:**
- Add group: name + search query + color picker
- Each group colors matching nodes
- Priority ordering (top = highest priority)

**Display:**
- Show arrows (directional edges)
- Node size slider (0.5x to 3x)
- Line thickness slider
- Text fade threshold (zoom level for label visibility)

**Forces:**
- Center force slider (0 to 0.1)
- Repel force slider (0 to 5000)
- Link force slider (0 to 0.5)
- Link distance slider (20 to 500)
- Reset forces button

### 9.5 Global vs. Local Graph

**Global Graph:**
- Full-window view (opened as a special tab)
- Shows all notes in vault
- Full controls panel
- Can become performance-heavy for large vaults
- Optimization: only render visible nodes (viewport culling)

**Local Graph:**
- Small panel in right sidebar
- Centers on current note
- Configurable depth: 1, 2, or 3 hops
- Updates when active note changes
- Simplified controls (depth slider only)
- Click node to navigate + recenter

### 9.6 Node Rendering

```cpp
class GraphNode : public QGraphicsEllipseItem {
    // Visual properties
    NodeType m_type;
    QColor m_color;                // From group or default
    double m_radius;               // Based on connection count
    QString m_label;               // Note title

    // Sizing formula
    double radius() const {
        double base = 5.0;
        double scale = 1.0 + std::log(1.0 + connectionCount) * 0.5;
        return base * scale * m_nodeScaleFactor;
    }

    // Colors by type
    static QColor colorForType(NodeType type) {
        switch (type) {
            case Regular:    return QColor("#7b6cd9");  // Purple (accent)
            case Unresolved: return QColor("#888888");  // Gray, translucent
            case Attachment: return QColor("#4caf50");  // Green
            case Tag:        return QColor("#ff9800");  // Orange
            case Orphan:     return QColor("#aaaaaa");  // Light gray
        }
    }
};
```

---

## 10. Canvas View

### 10.1 Architecture

Canvas uses `QGraphicsView`/`QGraphicsScene` for an infinite 2D workspace:

```
CanvasView (QGraphicsView)
    ├── Zoom/pan controls
    ├── Minimap overlay
    └── CanvasScene (QGraphicsScene)
            ├── CanvasGroup items (background rectangles)
            ├── CanvasCard items (text, note, media, link)
            ├── CanvasConnection items (edges between cards)
            └── Selection rectangle
```

### 10.2 Card Types

#### Text Card (`TextCard : CanvasCard`)
- Standalone markdown text card
- Double-click to edit (inline `QMarkdownTextEdit`)
- Resizable via edge/corner handles
- Background color selectable (7 options: none + 6 colors)
- Renders markdown when not editing

#### Note Card (`NoteCard : CanvasCard`)
- Embeds an existing vault note
- Header bar showing note title
- Content area renders note markdown
- Double-click to edit in-place (changes sync to `.md` file)
- "Open in editor" button to navigate to full editor
- Supports subpath: display specific heading section or block

#### Media Card (`MediaCard : CanvasCard`)
- Embeds images (render via `QPixmap`), PDFs (via `QPdfView`), audio/video
- Resizable with aspect ratio preservation for images
- Drag media files onto canvas to create

#### Link Card (`LinkCard : CanvasCard`)
- Displays a URL
- Shows URL text and optional favicon
- Click to open in system browser
- No iframe/web rendering (native app — no web views)

### 10.3 Connections

- Draw connections by clicking edge dot on card and dragging to another card
- Connection endpoints snap to card edges (top/right/bottom/left)
- Connections rendered as curved paths (`QPainterPath` with bezier curves)
- Optional arrow head on target end
- Editable label (text on the line — `QGraphicsTextItem`)
- Color selectable (same 7-color palette as cards)
- Select connection and press Delete to remove
- Connections route to avoid overlap where possible

### 10.4 Groups

- Select multiple cards → right-click → "Group"
- Group rendered as a labeled rectangle behind cards
- Moving group moves all contained cards
- Resizing group does not resize cards
- Drag cards in/out of groups
- Groups can be nested
- Group label is editable (double-click)
- Group background color selectable

### 10.5 Canvas Interactions

| Interaction | Behavior |
|-------------|----------|
| Click + drag empty space | Pan canvas |
| Scroll wheel | Zoom in/out |
| Click card | Select card |
| Shift+click | Add to selection |
| Drag selection rectangle | Multi-select |
| Double-click text/note card | Edit content |
| Drag card edge/corner | Resize |
| Drag selected cards | Move |
| Right-click empty space | Context menu: Add text card, Add note card, Add link card, Paste |
| Right-click card | Edit, Delete, Set color, Add to group, Convert to note |
| Arrow keys | Nudge selected cards |
| Delete key | Remove selected cards/connections |
| Ctrl+A | Select all |
| Ctrl+Z / Ctrl+Y | Undo/redo |

### 10.6 Canvas File Format

Full compatibility with Obsidian's `.canvas` JSON format:

```json
{
    "nodes": [
        {
            "id": "uuid-string",
            "type": "text|file|link|group",
            "x": 0, "y": 0,
            "width": 250, "height": 60,
            "color": "0",
            "text": "...",
            "file": "path/to/note.md",
            "subpath": "#heading",
            "url": "https://...",
            "label": "Group Label"
        }
    ],
    "edges": [
        {
            "id": "uuid-string",
            "fromNode": "node-id",
            "toNode": "node-id",
            "fromSide": "top|right|bottom|left",
            "toSide": "top|right|bottom|left",
            "color": "0",
            "label": "edge label"
        }
    ]
}
```

Color codes: `"0"` = none, `"1"` = red, `"2"` = orange, `"3"` = yellow, `"4"` = green, `"5"` = cyan, `"6"` = purple.

---

## 11. Search System

### 11.1 Architecture

Three-tier search matching Obsidian's capabilities:

1. **Filename search** — Quick Switcher, fuzzy matching on note names
2. **Full-text search** — SQLite FTS5 across note content
3. **Structured search** — Filter operators (file:, path:, tag:, property filters)

### 11.2 Search Query Parser

`SearchService` parses Obsidian-compatible search syntax:

```
Grammar:
  query      = term (operator term)*
  term       = filter | phrase | regex | word | group
  operator   = 'OR' | 'NOT' | '-'
  filter     = filterName ':' value
  phrase     = '"' text '"'
  regex      = '/' pattern '/'
  group      = '(' query ')'
  word       = text

  filterName = 'file' | 'path' | 'tag' | 'line' | 'section'
             | 'block' | 'content'

  propertyFilter = '[' propertyName ':' comparison ']'
                 | '[' propertyName ']'
                 | '[-' propertyName ']'
  comparison = operator? value
  operator   = '>' | '<' | '>=' | '<='
```

**Implementation:**

```cpp
struct SearchQuery {
    enum Type { And, Or, Not, Word, Phrase, Regex, Filter, PropertyFilter, Group };

    Type type;
    QString value;
    QString filterName;         // For Filter type
    QVector<SearchQuery> children;  // For And/Or/Not/Group

    // Parsed from query string
    static SearchQuery parse(const QString &queryString);

    // Execute against index
    QVector<SearchFileResult> execute(SQLiteIndex *index) const;
};
```

### 11.3 Full-Text Search (SQLite FTS5)

```cpp
class SQLiteIndex {
public:
    // Indexing
    void indexNote(const QString &path, const QString &title,
                   const QString &content, const QStringList &tags);
    void removeNote(const QString &path);
    void reindexAll(const VaultModel *vault);

    // Searching
    QVector<SearchFileResult> search(const SearchQuery &query) const;
    QVector<NoteMeta> searchByFilename(const QString &fuzzyQuery) const;
    QVector<NoteMeta> searchByTag(const QString &tag) const;

    // Link queries
    QVector<NoteMeta> backlinksFor(const QString &notePath) const;
    QVector<WikiLink> outlinksFor(const QString &notePath) const;
    QVector<QString> orphanLinks() const;  // Links to non-existent notes

    // Progress
    void setProgressCallback(std::function<void(int current, int total)> cb);
};
```

### 11.4 Fuzzy Filename Matching

For Quick Switcher, use the KDE fuzzy match algorithm (from `kfts_fuzzy_match.h`):

```cpp
// Score-based fuzzy matching
// Rewards: consecutive matches, word boundary matches, camelCase matches
// Penalizes: gaps between matches, non-boundary matches

bool fuzzyMatch(const QString &pattern, const QString &candidate, int &score);

// Used in QuickSwitcher and CommandPalette
class FuzzyFilterProxyModel : public QSortFilterProxyModel {
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    // Sort by fuzzy match score descending
};
```

### 11.5 Search UI

**Global Search Panel** (left sidebar):
- Search input at top
- Query syntax help tooltip
- Results grouped by file in a tree view
- Each file entry expandable to show individual matches
- Match context: line with highlighted match text
- File count and total match count
- Sort dropdown: Relevance, Filename, Modified date
- Click result → navigate to note at match position

**In-Note Search** (`Ctrl+F`):
- Provided by `QPlainTextEditSearchWidget` (from qmarkdowntextedit)
- Enhanced with: match count display, current match indicator
- Modes: plain text, whole word, regex
- Find next/previous with `Enter`/`Shift+Enter`
- `Ctrl+H` for replace

---

## 12. Properties & Metadata

### 12.1 Frontmatter Parsing

Parse YAML frontmatter between `---` delimiters at start of file:

```cpp
class FrontmatterParser {
public:
    struct ParseResult {
        QVariantMap properties;    // key -> value (typed)
        int startLine;             // Line number of opening ---
        int endLine;               // Line number of closing ---
        QString rawYaml;           // Original YAML text
    };

    static ParseResult parse(const QString &markdown);
    static QString serialize(const QVariantMap &properties);
    static QString updateFrontmatter(const QString &markdown,
                                     const QVariantMap &newProperties);
};
```

### 12.2 Property Types

Read from `.obsidian/types.json`:

```cpp
enum class PropertyType {
    Text,        // QString
    List,        // QStringList
    Number,      // double
    Checkbox,    // bool
    Date,        // QDate
    DateTime     // QDateTime
};

class PropertyTypeRegistry {
public:
    PropertyType typeForProperty(const QString &name) const;
    void setPropertyType(const QString &name, PropertyType type);
    void loadFromTypesJson(const QString &vaultPath);
    void saveToTypesJson(const QString &vaultPath);

private:
    QHash<QString, PropertyType> m_types;
};
```

### 12.3 Special Properties

| Property | Behavior in Corbomite |
|----------|----------------------|
| `tags` | Merged with inline `#tags`, shown in Tags panel |
| `aliases` | Used in Quick Switcher and link autocomplete |
| `cssclasses` | Applied as CSS classes to note preview rendering |
| `title` | Overrides filename as display title |
| `publish` | Ignored (no Publish feature) |
| `permalink` | Ignored (no Publish feature) |

---

## 13. Keyboard Shortcuts & Action System

### 13.1 Action Registry

All user actions are centralized in `ActionRegistry`, built on KDE's `KActionCollection`:

```cpp
class ActionRegistry : public QObject {
    Q_OBJECT

public:
    // Registration
    QAction *registerAction(const QString &id, const QString &text,
                           const QKeySequence &defaultShortcut,
                           const QString &category);

    // Lookup
    QAction *action(const QString &id) const;
    QList<QAction *> allActions() const;
    QList<QAction *> actionsInCategory(const QString &category) const;

    // Categories for Command Palette grouping
    static constexpr auto Cat_File = "File";
    static constexpr auto Cat_Edit = "Editor";
    static constexpr auto Cat_Navigate = "Navigation";
    static constexpr auto Cat_View = "View";
    static constexpr auto Cat_Format = "Formatting";
    static constexpr auto Cat_Search = "Search";
    static constexpr auto Cat_Canvas = "Canvas";
    static constexpr auto Cat_Graph = "Graph";
    static constexpr auto Cat_Workspace = "Workspace";

    // Shortcut persistence
    void loadCustomShortcuts(const QString &hotkeysJsonPath);
    void saveCustomShortcuts(const QString &hotkeysJsonPath);

private:
    KActionCollection *m_collection;
};
```

### 13.2 Default Keyboard Shortcuts

#### File Operations
| Shortcut | Action ID | Description |
|----------|-----------|-------------|
| `Ctrl+N` | `file.new-note` | Create new note |
| `Ctrl+O` | `switcher.open` | Open Quick Switcher |
| `Ctrl+S` | `file.save` | Save current note |
| `Ctrl+W` | `tab.close` | Close current tab |
| `Ctrl+Shift+N` | `file.new-canvas` | Create new canvas |
| `F2` | `file.rename` | Rename current note |

#### Editing
| Shortcut | Action ID | Description |
|----------|-----------|-------------|
| `Ctrl+B` | `editor.toggle-bold` | Toggle bold |
| `Ctrl+I` | `editor.toggle-italic` | Toggle italic |
| `Ctrl+K` | `editor.insert-link` | Insert markdown link |
| `Ctrl+]` | `editor.indent` | Indent |
| `Ctrl+[` | `editor.unindent` | Unindent |
| `Ctrl+D` | `editor.delete-line` | Delete line |
| `Ctrl+Shift+D` | `editor.duplicate-line` | Duplicate line |
| `Ctrl+Z` | `editor.undo` | Undo |
| `Ctrl+Shift+Z` | `editor.redo` | Redo |
| `Ctrl+Enter` | `editor.toggle-checkbox` | Toggle checkbox |
| `Ctrl+Shift+K` | `editor.toggle-strikethrough` | Toggle strikethrough |
| `Ctrl+Shift+H` | `editor.toggle-highlight` | Toggle highlight |
| `` Ctrl+` `` | `editor.toggle-code` | Toggle inline code |
| `Alt+Up` | `editor.swap-line-up` | Move line up |
| `Alt+Down` | `editor.swap-line-down` | Move line down |

#### Navigation
| Shortcut | Action ID | Description |
|----------|-----------|-------------|
| `Ctrl+P` | `command-palette.open` | Command Palette |
| `Ctrl+G` | `graph.open` | Open Graph View |
| `Ctrl+E` | `editor.toggle-mode` | Toggle Reading/Editing mode |
| `Ctrl+Click` | `editor.open-link-new-tab` | Open link in new tab |
| `Ctrl+Alt+Left` | `navigate.back` | Navigate back |
| `Ctrl+Alt+Right` | `navigate.forward` | Navigate forward |
| `Ctrl+Tab` | `tab.next-lru` | Next tab (LRU order) |
| `Ctrl+Shift+Tab` | `tab.prev-lru` | Previous tab (LRU) |
| `Ctrl+1` – `Ctrl+8` | `tab.goto-N` | Go to Nth tab |
| `Ctrl+9` | `tab.goto-last` | Go to last tab |
| `Alt+Enter` | `editor.follow-link` | Follow link under cursor |

#### Search
| Shortcut | Action ID | Description |
|----------|-----------|-------------|
| `Ctrl+F` | `search.find-in-note` | Find in current note |
| `Ctrl+H` | `search.replace-in-note` | Find and replace |
| `Ctrl+Shift+F` | `search.vault-search` | Search in all files |

#### View
| Shortcut | Action ID | Description |
|----------|-----------|-------------|
| `Ctrl+=` | `view.zoom-in` | Zoom in |
| `Ctrl+-` | `view.zoom-out` | Zoom out |
| `Ctrl+0` | `view.zoom-reset` | Reset zoom |
| `Ctrl+\` | `view.toggle-left-sidebar` | Toggle left sidebar |
| `Ctrl+Shift+\` | `view.toggle-right-sidebar` | Toggle right sidebar |
| `Ctrl+,` | `app.open-settings` | Open Settings |

#### Split Panes
| Shortcut | Action ID | Description |
|----------|-----------|-------------|
| `Ctrl+Shift+Right` | `workspace.split-right` | Split pane right |
| `Ctrl+Shift+Down` | `workspace.split-down` | Split pane down |

### 13.3 Obsidian Hotkeys.json Compatibility

Corbomite reads Obsidian's `.obsidian/hotkeys.json` to import custom shortcuts:

```cpp
void ActionRegistry::loadCustomShortcuts(const QString &hotkeysJsonPath) {
    // Parse hotkeys.json
    // Map Obsidian command IDs to Corbomite action IDs
    // "Mod" → Qt::CTRL (on Linux/Windows)
    // Apply custom shortcuts to matching actions
}
```

**Obsidian → Corbomite action ID mapping:**

| Obsidian Command ID | Corbomite Action ID |
|---------------------|---------------------|
| `editor:toggle-bold` | `editor.toggle-bold` |
| `app:go-back` | `navigate.back` |
| `switcher:open` | `switcher.open` |
| `command-palette:open` | `command-palette.open` |
| `workspace:split-horizontal` | `workspace.split-right` |
| `workspace:close` | `tab.close` |
| ... (full mapping table) | ... |

---

## 14. Settings & Configuration

### 14.1 Configuration Architecture

Three-tier configuration following KDE patterns + Obsidian compatibility:

```
Tier 1: Vault-level (.obsidian/ — shared with Obsidian)
├── app.json       → Read for initial settings
├── hotkeys.json   → Read for custom shortcuts
├── types.json     → Read/Write property types
├── bookmarks.json → Read/Write bookmarks
└── graph.json     → Read/Write graph settings

Tier 2: Vault-level (.corbomite/ — Corbomite-specific)
├── settings.json  → Corbomite-specific preferences
└── session.json   → Window state, open tabs, sidebar state

Tier 3: Application-level (~/.config/corbomite/)
├── corbomiterc    → KConfig: recently opened vaults, global preferences
└── shortcuts.rc   → KConfig: custom shortcuts (fallback)
```

### 14.2 Settings Categories

```cpp
struct EditorSettings {
    EditorMode defaultMode = EditorMode::LivePreview;  // Source, LivePreview, Reading
    int fontSize = 16;
    QString fontFamily = "default";           // System default
    QString monoFontFamily = "default";
    double lineHeight = 1.6;
    int tabSize = 4;
    bool useTabs = true;
    bool lineNumbers = false;
    bool lineWrap = true;
    bool readableLineLength = true;          // Constrain text width
    bool autoPairBrackets = true;
    bool autoPairMarkdown = true;
    bool foldHeadings = true;
    bool vimMode = false;
    bool spellCheck = true;
    QStringList spellCheckLanguages = {"en-US"};
    bool strictLineBreaks = false;
    bool showFrontmatter = true;             // Property editor vs raw YAML
    bool showInlineTitle = true;             // Show note title as H1
};

struct FilesSettings {
    QString newNoteLocation = "folder";      // "root", "folder", "specified"
    QString newNoteFolderPath = "";           // If "specified"
    QString attachmentFolder = "Attachments";
    QString attachmentFolderType = "specified"; // "root", "same", "subfolder", "specified"
    bool autoUpdateLinks = true;
    QString linkFormat = "shortest";          // "shortest", "relative", "absolute"
    bool useWikiLinks = true;                // [[]] vs []() for new links
    QString trashOption = "system";           // "system", "vault", "permanent"
    bool promptDelete = true;
    QStringList excludedPatterns;             // Glob patterns for search/graph exclusion
};

struct AppearanceSettings {
    QString theme = "system";                // "light", "dark", "system"
    QColor accentColor = QColor("#7b6cd9");
    QString interfaceFont = "default";
    double zoomLevel = 1.0;
    QStringList enabledSnippets;             // CSS snippet filenames
};

struct FeatureSettings {
    bool dailyNotes = false;
    QString dailyNoteDateFormat = "yyyy-MM-dd";
    QString dailyNoteFolder = "Daily Notes";
    QString dailyNoteTemplate = "";
    bool dailyNoteOnStartup = false;
    QString templateFolder = "Templates";
    bool graphView = true;
    bool canvas = true;
    bool bookmarks = true;
    bool outlineView = true;
    bool tagsView = true;
    bool backlinks = true;
    bool outgoingLinks = true;
    bool pagePreview = true;
    int autoSaveDelayMs = 2000;
};
```

### 14.3 Settings Dialog Structure

```
Settings Dialog
├── Editor
│   ├── Default editing mode
│   ├── Font settings (family, size, line height)
│   ├── Tab settings (size, tabs vs spaces)
│   ├── Display (line numbers, line wrap, readable width)
│   ├── Behavior (auto-pair, fold headings, strict breaks)
│   ├── Advanced (vim mode, spell check, frontmatter display)
│
├── Files & Links
│   ├── New note location
│   ├── Attachment folder
│   ├── Link format (wiki vs markdown, path style)
│   ├── Auto-update links
│   ├── Delete behavior
│   ├── Excluded files
│
├── Appearance
│   ├── Theme (light/dark/system)
│   ├── Accent color picker
│   ├── Font families (interface, text, monospace)
│   ├── Zoom level
│   ├── CSS snippets (toggle list)
│
├── Hotkeys
│   ├── Search bar (search by command name or key)
│   ├── Scrollable list of all actions
│   ├── Each row: action name | current shortcut | edit button
│   ├── Conflict detection (warning when shortcut already assigned)
│   ├── Reset to default button per action
│   ├── Import from Obsidian hotkeys.json
│
├── Core Features
│   ├── Toggle list for each feature (daily notes, templates, graph, etc.)
│   ├── Feature-specific settings (daily note format, template folder)
│
└── About
    ├── Version
    ├── License (GPLv3 or similar)
    ├── Credits
    └── Links (source, issues)
```

---

## 15. Session & Workspace Management

### 15.1 Session State

`SessionManager` saves and restores the full application state:

```cpp
struct SessionState {
    // Window geometry
    QByteArray windowGeometry;
    QByteArray windowState;

    // Sidebar state
    bool leftSidebarVisible;
    int leftSidebarWidth;
    int leftSidebarActivePanel;       // Index of active panel
    bool rightSidebarVisible;
    int rightSidebarWidth;
    int rightSidebarActivePanel;

    // Editor layout (recursive splitter tree)
    struct ViewSpaceState {
        QVector<TabState> tabs;
        int activeTabIndex;
    };
    struct SplitterState {
        Qt::Orientation orientation;
        QVector<int> sizes;           // Splitter handle positions
        QVector<std::variant<ViewSpaceState, SplitterState>> children;
    };
    SplitterState editorLayout;

    // Expanded folders in file explorer
    QSet<QString> expandedFolders;

    // Graph state
    QHash<QString, QPointF> graphNodePositions;  // Pinned nodes
    GraphSettings graphSettings;

    // Serialization
    QJsonObject toJson() const;
    static SessionState fromJson(const QJsonObject &json);
};
```

### 15.2 Session Save/Restore Triggers

**Auto-save session:**
- On every tab switch, open, or close
- On sidebar toggle or resize
- On splitter resize
- On window resize/move
- Debounced: 2 second delay after last change

**Restore session:**
- On vault open, read `.corbomite/session.json`
- Restore window geometry
- Restore sidebar state
- Restore editor split layout
- Re-open tabs (load note documents)
- Restore cursor positions and scroll

**Workspace presets** (like Obsidian's Workspaces):
- Save named workspace: captures full SessionState
- Load workspace: restores saved state
- Stored in `.corbomite/workspaces/NAME.json`
- Command: "Save workspace", "Load workspace", "Manage workspaces"

### 15.3 Obsidian workspace.json Compatibility

Read Obsidian's `.obsidian/workspace.json` on first open to import:
- Which notes were open as tabs
- Split pane arrangement
- Active note
- Sidebar state (left/right visibility)

This enables smooth transition from Obsidian to Corbomite.

---

## 16. Theming & Appearance

### 16.1 Theme System

Corbomite uses Qt stylesheets + QPalette for theming:

**Built-in themes:**
- Light (default system light)
- Dark (default system dark)
- System (follow KDE/OS theme)

**Custom themes:**
- CSS snippets from `.obsidian/snippets/*.css` are read and adapted
- Corbomite maps Obsidian CSS variables to Qt properties where possible
- Custom themes stored in `.corbomite/themes/`

### 16.2 CSS Variable → Qt Property Mapping

| Obsidian CSS Variable | Qt Property/Approach |
|-----------------------|---------------------|
| `--background-primary` | `QPalette::Window` / widget background |
| `--background-secondary` | Sidebar `QWidget::setStyleSheet()` |
| `--text-normal` | `QPalette::WindowText` |
| `--text-muted` | `QPalette::PlaceholderText` |
| `--text-accent` | `QPalette::Link` / custom accent color |
| `--interactive-accent` | `QPalette::Highlight` |
| `--font-text` | `QApplication::setFont()` for editor |
| `--font-monospace` | Code block font |
| `--font-text-size` | Base font size setting |
| `--h1-size` through `--h6-size` | `MarkdownHighlighter` heading sizes |
| `--code-background` | Code block background in highlighter |
| `--callout-*` | Callout widget colors |

### 16.3 Accent Color

User-configurable accent color affects:
- Links in editor
- Active tab indicator
- Selected items in lists/trees
- Toggle switches and checkboxes
- Graph node default color
- Button highlights
- Selection background in editor

Implementation: Store accent color in settings, apply via `QPalette` modifications and targeted stylesheet rules.

---

## 17. Class Architecture

### 17.1 Class Hierarchy Overview

```
QApplication
└── CorbomiteApp (singleton)
    ├── VaultService
    ├── NoteService
    ├── SearchService
    ├── LinkService
    ├── CanvasService
    ├── GraphService
    ├── TemplateService
    ├── SettingsService
    ├── SessionManager
    └── ActionRegistry

KXmlGuiWindow
└── MainWindow
    ├── RibbonBar (QToolBar)
    ├── LeftSidebar (QDockWidget)
    │   ├── FileExplorerPanel
    │   ├── SearchPanel
    │   ├── TagsPanel
    │   └── BookmarksPanel
    ├── RightSidebar (QDockWidget)
    │   ├── BacklinksPanel
    │   ├── OutlinksPanel
    │   ├── OutlinePanel
    │   └── LocalGraphPanel
    ├── EditorViewManager (root QSplitter)
    │   └── EditorViewSpace* (QWidget: TabBar + QStackedWidget)
    │       ├── EditorTabBar (QTabBar)
    │       └── NoteEditorWidget* / NotePreviewWidget* / CanvasView*
    ├── StatusBar (QStatusBar)
    └── Floating widgets
        ├── CommandPalette
        ├── QuickSwitcher
        ├── HoverPreview
        ├── LinkSuggestionPopup
        └── TagSuggestionPopup

QObject
├── VaultModel
├── NoteDocument*
├── NotesTreeModel (QAbstractItemModel)
├── TagsModel (QAbstractItemModel)
├── LinksModel (QAbstractListModel)
├── SearchResultsModel (QAbstractItemModel)
├── TabModel (QAbstractListModel)
├── GraphDataModel
├── CanvasModel
├── FuzzyFilterProxyModel (QSortFilterProxyModel)
└── Reactors
    ├── AutosaveReactor
    ├── LinkSyncReactor
    ├── TabPersistenceReactor
    └── FileWatchReactor

QPlainTextEdit
└── QMarkdownTextEdit
    └── CorbomiteEditor (our subclass)
        ├── WikiLink highlighting + autocomplete
        ├── Tag autocomplete
        ├── Callout rendering
        ├── Hover preview integration
        └── Image paste handling

QGraphicsView
├── GraphView (graph visualization)
└── CanvasView (infinite canvas)

QGraphicsScene
├── GraphScene
└── CanvasScene

QGraphicsItem subclasses
├── GraphNode (QGraphicsEllipseItem)
├── GraphEdge (QGraphicsLineItem)
├── CanvasCard (QGraphicsRectItem)
│   ├── TextCard
│   ├── NoteCard
│   ├── MediaCard
│   └── LinkCard
├── CanvasGroup (QGraphicsRectItem)
└── CanvasConnection (QGraphicsPathItem)
```

### 17.2 Ownership & Lifetime

```
CorbomiteApp (owns)
├── Services (created at app start, destroyed at app exit)
├── ActionRegistry (global)
└── MainWindow(s) (one per window)

MainWindow (owns)
├── UI widgets (Qt parent-child)
├── EditorViewManager (owns EditorViewSpaces)
└── Sidebar panels

VaultModel (owns)
├── NotesTreeModel
├── TagsModel
└── SQLiteIndex

NoteDocument (owned by VaultModel cache)
├── QTextDocument (for editor)
├── Parsed frontmatter
└── Extracted links

EditorViewSpace (owns)
├── TabModel (for this space)
├── Active NoteEditorWidget/NotePreviewWidget/CanvasView
└── EditorTabBar
```

### 17.3 Signal Flow Examples

**Opening a note from Quick Switcher:**
```
QuickSwitcher::accepted(notePath)
  → ActionRegistry::execute("file.open-note", notePath)
    → NoteService::openNote(notePath)
      → FileSystemAdapter::readFile(absolutePath)
      → NoteDocument* created/cached in VaultModel
      → EditorViewManager::openInActiveSpace(noteDoc)
        → EditorViewSpace::addTab(noteDoc)
          → TabModel::openTab(notePath)
          → NoteEditorWidget::setDocument(noteDoc)
            → QMarkdownTextEdit::setPlainText(markdown)
        → EditorTabBar::setCurrentIndex(newTab)
      → emit MainWindow::activeNoteChanged(noteDoc)
        → BacklinksPanel::updateForNote(noteDoc)
        → OutlinksPanel::updateForNote(noteDoc)
        → OutlinePanel::updateForNote(noteDoc)
        → LocalGraphPanel::updateForNote(noteDoc)
        → StatusBar::updateNoteInfo(noteDoc)
```

**Saving a note (autosave):**
```
CorbomiteEditor::textChanged()
  → NoteDocument::setModified(true)
    → TabModel::tabDirtyChanged(index, true)
    → AutosaveReactor detects modification
      → starts 2-second debounce QTimer
      → QTimer::timeout
        → NoteService::saveNote(noteDoc)
          → FrontmatterParser::serialize(properties)
          → FileSystemAdapter::writeFile(path, content)
          → NoteDocument::setModified(false)
          → FileWatchReactor::suppressNext(path)
          → LinkSyncReactor::onNoteSaved(path)
            → LinkService::extractAndIndexLinks(noteDoc)
              → SQLiteIndex::updateLinks(path, newLinks)
```

**Renaming a note:**
```
FileExplorerPanel::renameRequested(oldPath, newName)
  → NoteService::renameNote(oldPath, newPath)
    → FileSystemAdapter::rename(oldAbsPath, newAbsPath)
    → LinkService::repairLinks(oldPath, newPath)
      → SQLiteIndex::backlinksFor(oldPath)
      → For each backlinking note:
        → Load content, regex replace links, save
    → SQLiteIndex::renameNote(oldPath, newPath)
    → VaultModel::noteRenamed(oldPath, newPath)
      → NotesTreeModel updates
      → TabModel::updateNotePath(oldPath, newPath)
      → EditorViewSpace::updateDocumentPath(oldPath, newPath)
```

---

## 18. Qt6/KDE Analogue Map

This section maps every major Obsidian/web-tech concept to its Qt6/KDE native equivalent.

### 18.1 UI Framework Mapping

| Obsidian (Electron/Web) | Corbomite (Qt6/KDE) |
|--------------------------|---------------------|
| Electron BrowserWindow | `KXmlGuiWindow` (MainWindow) |
| CSS Flexbox/Grid layout | `QBoxLayout`, `QGridLayout`, `QSplitter` |
| React/Svelte components | `QWidget` subclasses |
| HTML `<div>` containers | `QFrame`, `QGroupBox`, `QWidget` |
| CSS styling | `QSS` (Qt Style Sheets) + `QPalette` |
| Virtual DOM / reactivity | Qt signals/slots + model/view |
| Web Workers | `QThread`, `QtConcurrent` |
| IndexedDB | SQLite via `QSqlDatabase` |
| localStorage | `QSettings` / `KConfig` |
| Fetch API | `QNetworkAccessManager` |
| WebSocket | `QWebSocket` |
| DOM events | Qt event system (`QEvent`, `eventFilter`) |
| CSS transitions/animations | `QPropertyAnimation`, `QTimeLine` |

### 18.2 Editor Mapping

| Obsidian/Clones (Web) | Corbomite (Qt6) |
|------------------------|-----------------|
| CodeMirror 6 (Obsidian) | `QMarkdownTextEdit` (QPlainTextEdit subclass) |
| TipTap/ProseMirror (Lokus) | `QMarkdownTextEdit` + custom extensions |
| Milkdown/ProseMirror (Otterly) | `QMarkdownTextEdit` + custom extensions |
| ProseMirror Document model | `QTextDocument` |
| ProseMirror Schema (nodes/marks) | `QTextBlock` + `QTextCharFormat` |
| CM6 syntax highlighting | `MarkdownHighlighter` (`QSyntaxHighlighter`) |
| Shiki (code highlighting) | `KSyntaxHighlighting` framework |
| ProseMirror plugins | `CorbomiteEditor` subclass methods |
| ProseMirror transactions | `QTextCursor` operations + undo grouping |
| contenteditable DOM | `QPlainTextEdit` viewport painting |

### 18.3 Data/State Mapping

| Web Pattern | Qt6/KDE Equivalent |
|-------------|-------------------|
| Zustand store (Lokus) | `QObject` with properties + signals |
| Svelte $state (Otterly) | `Q_PROPERTY` with `NOTIFY` signal |
| React useState/useEffect | Signal/slot connections |
| Proxy model / computed | `QSortFilterProxyModel`, `$derived` → `Q_PROPERTY` getter |
| Event emitter | Qt signals |
| Observer pattern | `QObject::connect()` |
| Redux actions | `QAction` + `KActionCollection` |

### 18.4 Graphics/Visualization Mapping

| Web Technology | Qt6 Equivalent |
|----------------|---------------|
| Sigma.js WebGL (Lokus graph) | `QGraphicsView` + custom `QGraphicsItem` |
| Three.js 3D (Lokus 3D graph) | `Qt3DWindow` or `QOpenGLWidget` (if needed) |
| D3.js force simulation | Custom `ForceLayout` class |
| Excalidraw (Lokus canvas) | `QGraphicsView` + `QGraphicsScene` |
| HTML Canvas 2D | `QPainter` / `QGraphicsScene` |
| SVG rendering | `QSvgRenderer` / `QGraphicsSvgItem` |
| CSS Grid (layout) | `QGridLayout` / `QSplitter` |
| Web Animations API | `QPropertyAnimation` / `QTimeLine` |

### 18.5 System Integration Mapping

| Web/Electron Feature | KDE/Qt6 Equivalent |
|-----------------------|-------------------|
| Electron IPC (Tauri invoke) | Direct C++ function calls (no IPC needed!) |
| File dialog | `QFileDialog` |
| System tray | `QSystemTrayIcon` / `KStatusNotifierItem` |
| Notifications | `KNotification` |
| File watcher (notify crate) | `QFileSystemWatcher` |
| System clipboard | `QClipboard` |
| Drag and drop | `QDrag` + `QDropEvent` |
| Global shortcuts | `KGlobalAccel` |
| DBus integration | `QDBusInterface` |
| Single instance | `KDBusService` |
| Session management | `KSessionManager` |
| Spellcheck (browser native) | `Sonnet` (KDE) |
| PDF generation | `QPrinter` + `QPdfWriter` |

### 18.6 Architecture Pattern Mapping

| Otterly Pattern | Corbomite Equivalent |
|-----------------|---------------------|
| Ports & Adapters | Abstract service interfaces + concrete implementations |
| Service layer (async orchestration) | Service classes using `QtConcurrent` / `QThread` |
| Store ($state classes) | `QObject` subclasses with `Q_PROPERTY` + signals |
| Reactors ($effect.root) | `QObject` with persistent signal/slot connections |
| Action Registry | `KActionCollection` + custom `ActionRegistry` |
| OpStore (operation tracking) | `QFutureWatcher` + status signals |
| Revision counters (staleness) | `quint64` generation counter in services |
| Service result types | `std::variant<SuccessResult, ErrorResult>` or custom result structs |

### 18.7 Performance Pattern Mapping

| Web Optimization | Qt6 Equivalent |
|-------------------|---------------|
| Web Workers | `QThread` / `QtConcurrent::run()` |
| requestAnimationFrame | `QTimer` with 16ms interval / `QWindow::requestUpdate()` |
| Virtual scrolling | `QAbstractItemView` with model (inherently virtual) |
| Lazy loading (React.lazy) | Load widgets on-demand, `QStackedWidget` |
| Debouncing (setTimeout) | `QTimer::singleShot()` with restart |
| Memory pools (Lokus graph) | `QCache`, object pools, arena allocators |
| IndexedDB persistence | SQLite via `QSqlDatabase` |
| Blob/ArrayBuffer | `QByteArray` |

---

## Appendix A: Feature Prioritization (Implementation Phases)

### Phase 1: Core Editor (MVP)
- [ ] Vault open/close/scan
- [ ] File explorer tree (left sidebar)
- [ ] Source mode editor (QMarkdownTextEdit)
- [ ] Markdown syntax highlighting
- [ ] Tab management (open, close, switch)
- [ ] Basic keyboard shortcuts
- [ ] Create/rename/delete notes
- [ ] Save/autosave
- [ ] In-note search (Ctrl+F)
- [ ] File watching for external changes
- [ ] Status bar (word count, line/col)
- [ ] Basic settings (font, theme, tab size)
- [ ] Session save/restore (open tabs, window geometry)

### Phase 2: Obsidian Markdown
- [ ] Wikilink highlighting and navigation (`[[]]`)
- [ ] Wikilink autocomplete popup
- [ ] Tag highlighting and autocomplete
- [ ] Embed syntax (`![[]]`) — images at minimum
- [ ] Callout syntax highlighting
- [ ] Highlight syntax (`==text==`)
- [ ] Comment syntax (`%%...%%`)
- [ ] Block references (`^id`)
- [ ] Frontmatter/YAML parsing and display
- [ ] Property Editor widget
- [ ] Reading mode (rendered preview)
- [ ] Global search (Ctrl+Shift+F) with FTS5
- [ ] Quick Switcher (Ctrl+O) with fuzzy matching
- [ ] Command Palette (Ctrl+P)

### Phase 3: Knowledge Graph
- [ ] SQLite link index (backlinks, outlinks)
- [ ] Backlinks panel
- [ ] Outgoing links panel
- [ ] Outline panel
- [ ] Link repair on rename/move
- [ ] Graph view (global)
- [ ] Graph view (local, in sidebar)
- [ ] Force-directed layout
- [ ] Graph filtering and color groups
- [ ] Hover preview on links
- [ ] Tags panel with hierarchical display
- [ ] Bookmarks

### Phase 4: Canvas & Advanced
- [ ] Canvas view (text cards, note cards, connections, groups)
- [ ] Canvas file format (.canvas) read/write
- [ ] Split panes
- [ ] Daily notes
- [ ] Templates
- [ ] Math/LaTeX rendering
- [ ] Mermaid diagram rendering
- [ ] Code block syntax highlighting (KSyntaxHighlighting)
- [ ] Vim mode
- [ ] Live Preview mode
- [ ] Note Composer (extract/merge)
- [ ] PDF export
- [ ] Heading fold/collapse
- [ ] CSS snippets support
- [ ] Workspaces (save/restore named layouts)

### Phase 5: Polish & Ecosystem
- [ ] Full Obsidian search syntax (all operators)
- [ ] Unlinked mentions in backlinks
- [ ] Image resize syntax
- [ ] Footnotes rendering
- [ ] Table editing improvements
- [ ] Drag-drop file import
- [ ] Multiple windows
- [ ] Performance optimization for large vaults (10,000+ notes)
- [ ] Plugin architecture design
- [ ] Obsidian URI scheme (`obsidian://`)
- [ ] Accessibility (screen reader, keyboard-only navigation)

---

## Appendix B: Obsidian Feature Specification Reference

See `OBSIDIAN_SPECIFICATION.md` in this repository for the exhaustive Obsidian feature catalog that this architecture is based on.

---

## Appendix C: Research Sources

This specification was synthesized from analysis of:

1. **Obsidian** (obsidian.md) — Feature research from documentation and usage patterns
2. **Lokus** (`ObsClones/lokus/`) — React 19 + Tauri 2.0 + TipTap 3 Obsidian clone with WebGL graph, Excalidraw canvas, plugin system, manifest-based sync
3. **Otterly** (`ObsClones/otterly/`) — SvelteKit 5 + Tauri v2 clone with Milkdown editor, hexagonal architecture, SQLite FTS5 search, link repair system, git integration
4. **QOwnNotes/qmarkdowntextedit** — Qt5/6 note-taking application with markdown editor widget, syntax highlighting for 20+ languages, note relation graph, tag system, SQLite database
5. **KDE Applications** (`~/src/kde/src/`) — Kate (MDI, tabs, split views, quick open, session management), Dolphin (file tree, bookmarks), Marknote (note models), KTextEditor (document-view pattern), KXmlGui (action/shortcut system), KConfig (settings)
