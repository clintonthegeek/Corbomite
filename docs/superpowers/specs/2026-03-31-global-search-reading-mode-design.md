# Global Search & Reading Mode — Design Specification

## Overview

Complete Phase 2 with two independent features:
- **Global Search (Ctrl+Shift+F):** SQLite FTS5 full-text search across all vault notes, with results panel in left sidebar.
- **Reading Mode (Ctrl+E):** Rendered markdown preview toggled in the editor area, using a regex-based markdown-to-HTML converter.

## Global Search

### SQLiteIndex

New class in `libs/storage/` — manages the FTS5 search index.

```cpp
struct SearchMatch {
    QString notePath;       // Relative path
    QString snippet;        // Context snippet with match highlighted
    double score;           // FTS5 rank score
};

class SQLiteIndex : public QObject {
    Q_OBJECT
public:
    explicit SQLiteIndex(QObject *parent = nullptr);
    ~SQLiteIndex() override;

    bool open(const QString &dbPath);
    void close();

    void rebuildIndex(const QString &vaultRoot);
    // TODO: Move to background thread for large vaults (>1000 notes)
    void indexNote(const QString &relativePath, const QString &title, const QString &content);
    void removeNote(const QString &relativePath);

    QVector<SearchMatch> search(const QString &query, int maxResults = 100) const;
    // TODO: Support Obsidian search operators (file:, path:, tag:, line:, regex)

signals:
    void indexReady();
};
```

**Database schema:**
```sql
CREATE VIRTUAL TABLE IF NOT EXISTS notes_fts USING fts5(
    path,
    title,
    content,
    tokenize = 'porter unicode61'
);
```

**Dependencies:** Qt6::Sql (QSqlDatabase, QSqlQuery). SQLite is bundled with Qt.

**Index lifecycle:**
- Created/opened on vault open at `.corbomite/index.sqlite`
- Full rebuild on first open or when stale
- Incremental update on note save (`indexNote`), delete (`removeNote`), add (`indexNote`)
- `search()` uses FTS5 `MATCH` with `snippet()` for context and `rank` for scoring

### SearchPanel

New sidebar panel — second tab in left sidebar alongside Files.

```
SearchPanel (QWidget)
├── QLineEdit (search input, placeholder "Search vault...")
├── QLabel (result count: "N matches in M files")
└── QTreeView (results grouped by file)
    └── SearchResultsModel (two-level tree: file → matches)
```

**Behavior:**
- Typing triggers search after 300ms debounce (`QTimer`)
- Results grouped by file — each file row shows note name + match count
- Expanding a file shows individual match snippets with bold highlighted terms
- Click on a match → open note in editor (navigate to match position is future)
- Empty input clears results
- Escape in search input clears and returns focus to editor

**SearchResultsModel** — already declared in `libs/models/` spec but not yet implemented. Two-level `QAbstractItemModel`:
- Level 0: file entries (note name, match count)
- Level 1: individual matches within that file (snippet text, line number)

### Integration

- Register search panel as ToolView in left sidebar: `createToolView(nullptr, "search_panel", KMultiTabBar::Left, searchIcon, "Search")`
- `Ctrl+Shift+F` action focuses the search panel and its input field
- On vault open: `SQLiteIndex::open()` + `rebuildIndex()`
- AutosaveReactor's `noteSaved` signal → `SQLiteIndex::indexNote()`
- FileWatchReactor's `fileCreatedExternally` → `indexNote()`, `fileDeletedExternally` → `removeNote()`

### XMLGUI

Add `search_vault` action to Go menu:
```xml
<Action name="search_vault"/>
```

## Reading Mode

### MarkdownRenderer

New class in `libs/core/` — regex-based markdown-to-HTML converter.

```cpp
class MarkdownRenderer {
public:
    QString renderToHtml(const QString &markdown) const;

    // TODO: Replace regex renderer with cmark-gfm or other proper markdown
    // parser for full CommonMark spec compliance. The regex approach handles
    // common cases but will fail on edge cases like nested emphasis, reference
    // links, and complex list nesting. cmark-gfm provides a proper AST-based
    // pipeline with GFM extensions (tables, strikethrough, autolinks, task lists).

private:
    QString renderBlock(const QString &markdown) const;
    QString processInline(const QString &text) const;
    QString wrapDocument(const QString &body) const;
    static QString escapeHtml(const QString &text);
    static QString defaultStylesheet();
};
```

**Supported syntax:**

| Feature | Markdown | HTML Output |
|---------|----------|-------------|
| Headings | `# H1` through `###### H6` | `<h1>` through `<h6>` |
| Bold | `**text**` | `<strong>` |
| Italic | `*text*` | `<em>` |
| Strikethrough | `~~text~~` | `<del>` |
| Inline code | `` `code` `` | `<code>` |
| Code blocks | ` ```lang ` | `<pre><code class="language-lang">` |
| Links | `[text](url)` | `<a href="url">` |
| Images | `![alt](src)` | `<img src="src" alt="alt">` |
| Unordered lists | `- item` | `<ul><li>` |
| Ordered lists | `1. item` | `<ol><li>` |
| Checkboxes | `- [ ] task` | `<li>` with checkbox |
| Blockquotes | `> quote` | `<blockquote>` |
| Horizontal rules | `---` | `<hr>` |
| Tables | `\| col \|` | `<table>` |
| Wikilinks | `[[Note]]` | `<a class="internal-link" href="Note.md">Note</a>` |
| Highlight | `==text==` | `<mark>text</mark>` |
| Comments | `%%text%%` | stripped from output |
| Callouts | `> [!type] Title` | `<div class="callout callout-type">` |
| Tags | `#tag` | `<span class="tag">#tag</span>` |

**Stylesheet:** Embedded CSS in the HTML output matching Obsidian's visual style — readable line width, heading sizes, code block backgrounds, callout colors, link accent color.

### NotePreviewWidget

New widget in `src/editor/`:

```cpp
class NotePreviewWidget : public QTextBrowser {
    Q_OBJECT
public:
    explicit NotePreviewWidget(QWidget *parent = nullptr);

    void renderDocument(NoteDocument *doc);

signals:
    void internalLinkClicked(const QString &targetPath);

private:
    void onAnchorClicked(const QUrl &url);

    MarkdownRenderer m_renderer;
};
```

- `renderDocument()` calls `m_renderer.renderToHtml(doc->markdown())` and sets as HTML
- Internal links (class `internal-link`) emit `internalLinkClicked` signal
- External links open via `QDesktopServices::openUrl()`
- Read-only by default (`QTextBrowser` is non-editable)
- Images loaded from vault filesystem via `QTextBrowser::setSearchPaths()`

### Editor Mode Toggle

In `EditorViewSpace`:
- New method `toggleEditorMode()` — switches between NoteEditorWidget and NotePreviewWidget for the active tab
- Both widgets share the same `NoteDocument`
- Switching to preview: render current document markdown
- Switching to editor: remove preview, show editor (already has the document)
- Track current mode per tab in TabState or separate map

In MainWindow:
- `Ctrl+E` action `editor_toggle_mode` calls `m_editorManager->activeViewSpace()->toggleEditorMode()`
- Status bar updates to show "Source" or "Reading" based on mode

### Integration with Hover Preview (future)

The `MarkdownRenderer` built here is the same component needed for hover preview (deferred from Batch B). When hover preview is implemented, it will:
```cpp
// Future: HoverPreview uses MarkdownRenderer
// auto html = m_renderer.renderToHtml(linkedDoc->markdown());
// m_hoverPopup->setHtml(html);
```

## File Structure

```
libs/storage/
├── include/corbomite/storage/SQLiteIndex.h      # New
├── src/SQLiteIndex.cpp                           # New

libs/core/
├── include/corbomite/core/MarkdownRenderer.h     # New
├── src/MarkdownRenderer.cpp                      # New

libs/models/
├── include/corbomite/models/SearchResultsModel.h # New
├── src/SearchResultsModel.cpp                    # New

src/sidebar/
├── SearchPanel.h/cpp                             # New

src/editor/
├── NotePreviewWidget.h/cpp                       # New
├── EditorViewSpace.h/cpp                         # Modified — toggle mode

src/app/
├── MainWindow.h/cpp                              # Modified — search panel, toggle action
├── corbomiteui.rc.in                             # Modified — search action, toggle action

tests/storage/
├── tst_sqliteindex.cpp                           # New

tests/core/
├── tst_markdownrenderer.cpp                      # New
```

## Testing

### Unit Tests

**tst_sqliteindex.cpp:**
- Open/close database
- Index a note, search for it, verify match
- Remove a note, search returns empty
- Rebuild index from vault directory
- Search returns snippets with match context
- Search with no matches returns empty
- FTS5 handles multiple words (AND logic)

**tst_markdownrenderer.cpp:**
- Heading `# Title` → `<h1>Title</h1>`
- Bold `**text**` → `<strong>text</strong>`
- Italic `*text*` → `<em>text</em>`
- Link `[text](url)` → `<a href="url">text</a>`
- Code block → `<pre><code>`
- Wikilink `[[Note]]` → `<a class="internal-link">`
- Highlight `==text==` → `<mark>text</mark>`
- Comment `%%hidden%%` → stripped from output
- Callout `> [!warning] Title` → `<div class="callout callout-warning">`
- Tag `#project` → `<span class="tag">`
- Table rendering
- Checkbox rendering

### Manual Testing
- Ctrl+Shift+F opens search panel, type query, see results, click to navigate
- Ctrl+E toggles between source and rendered preview
- Preview renders headings, bold, links, code, lists correctly
- Wikilinks in preview are clickable → open target note
- Status bar shows "Source" or "Reading"

## Keyboard Shortcuts

| Shortcut | Action ID | Description |
|----------|-----------|-------------|
| `Ctrl+Shift+F` | `search_vault` | Focus search panel |
| `Ctrl+E` | `editor_toggle_mode` | Toggle Source/Reading mode |

## What This Does NOT Include
- Obsidian search operators (file:, path:, tag:, regex, line:, section:, block:) — future, breadcrumb comments
- Search result sorting options — future
- Search embed blocks in notes — future
- Live Preview mode (inline rendering) — Phase 4
- Hover preview popup — future, but renderer is now available
- Background indexing for large vaults — future, breadcrumb comment
- Navigate to exact match position on click — future
