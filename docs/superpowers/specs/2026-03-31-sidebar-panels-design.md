# Sidebar Panels (Backlinks, Outlinks, Outline) — Design Specification

## Overview

Add three ToolView panels to the right sidebar that show contextual information about the current note: backlinks, outgoing links, and heading outline. All panels update when the active note changes.

## BacklinksPanel

Shows notes that link TO the current note.

**Widget:** `QListWidget` with clickable items.

**Data source:** `SQLiteIndex::backlinksFor(currentNote.relativePath)` returns `QVector<LinkInfo>`.

**Each item displays:**
- Note name (extracted from `sourcePath`, without `.md` extension)
- Folder path in muted text (if not in vault root)
- Click → opens that note via `onNoteActivated(sourcePath)`

**Empty state:** `QLabel` centered: "No backlinks"

**Header:** Shows count: "Backlinks (N)"

```cpp
// Future: Add context snippets showing the paragraph containing the link.
// This requires loading each source note's content from disk or caching
// link context in the SQLite index (e.g., a 'context' column in the links table).
// Options to explore:
// - Cache context at index time (adds ~100 chars per link to DB, fast retrieval)
// - Lazy-load context on panel expand (slower but no storage cost)
// - Store line numbers in links table, read only the relevant line from disk
```

## OutlinksPanel

Shows notes that the current note links TO.

**Widget:** `QListWidget` with clickable items.

**Data source:** `SQLiteIndex::outlinksFor(currentNote.relativePath)` returns `QVector<LinkInfo>`.

**Each item displays:**
- Target note name
- Link type icon/indicator: wiki, markdown, or embed
- If target note doesn't exist in vault (orphan link): muted/italic styling
- Click on existing link → opens target note
- Click on orphan link → creates the note and opens it

**Empty state:** "No outgoing links"

**Orphan detection:** Check `VaultModel::noteExists(targetPath)` for each outlink.

## OutlinePanel

Shows heading structure of the current note as a tree.

**Widget:** `QTreeWidget` with items representing H1-H6.

**Data source:** Regex on current note's markdown content: `^(#{1,6})\s+(.+)$`

**Each item displays:**
- Heading text, indented by level (H1 at root, H2 as child of H1, etc.)
- Click → scroll editor to that heading's line position

**Updates:** Connected to `NoteDocument::textChanged()` with 500ms debounce to avoid excessive rebuilds during typing.

**Empty state:** "No headings"

## Integration

### Right Sidebar Setup

In `MainWindow::setupSidebars()`, after the left sidebar setup, add three ToolViews on the right:

```cpp
// Right sidebar: Backlinks
auto *backlinksView = createToolView(nullptr, "backlinks_panel",
    KMultiTabBar::Right, QIcon::fromTheme("link"), i18n("Backlinks"));

// Right sidebar: Outlinks
auto *outlinksView = createToolView(nullptr, "outlinks_panel",
    KMultiTabBar::Right, QIcon::fromTheme("go-jump"), i18n("Outlinks"));

// Right sidebar: Outline
auto *outlineView = createToolView(nullptr, "outline_panel",
    KMultiTabBar::Right, QIcon::fromTheme("view-list-tree"), i18n("Outline"));
```

### Active Note Updates

Connect `EditorViewManager::activeEditorChanged` to update all three panels:

```cpp
connect(m_editorManager, &EditorViewManager::activeEditorChanged,
        this, [this](NoteEditorWidget *editor) {
    if (editor && editor->noteDocument()) {
        m_backlinksPanel->setCurrentNote(editor->noteDocument());
        m_outlinksPanel->setCurrentNote(editor->noteDocument());
        m_outlinePanel->setCurrentNote(editor->noteDocument());
    }
});
```

### Panel Dependencies

- BacklinksPanel needs: `SQLiteIndex*` (for backlink queries), signal connection to `onNoteActivated`
- OutlinksPanel needs: `SQLiteIndex*` (for outlink queries), `VaultModel*` (for orphan detection), signal connections to `onNoteActivated` and note creation
- OutlinePanel needs: `NoteDocument*` (for content), signal to scroll editor

## File Structure

```
src/sidebar/
├── BacklinksPanel.h/cpp      # New
├── OutlinksPanel.h/cpp       # New
├── OutlinePanel.h/cpp        # New

src/app/
├── MainWindow.h/cpp          # Modified — add panels, wire connections
├── corbomiteui.rc.in         # No change needed — panels are sidebar ToolViews, not menu actions
```

## Testing

Manual testing — these are UI display panels:
- Open vault, open a note that has backlinks → verify BacklinksPanel shows them
- Click a backlink entry → navigates to that note
- Open a note with wikilinks → verify OutlinksPanel shows targets
- Orphan links shown in muted style
- Open a note with headings → verify OutlinePanel shows heading tree
- Edit a heading → outline updates after debounce
- Switch between notes → all panels update

## What This Does NOT Include
- Context snippets in backlinks (future — see breadcrumb comments)
- Unlinked mentions (future)
- Outline heading collapse persistence (future)
- Local graph mini-view in sidebar (Sub-project 3d)
