# Split Panes & Code Block Highlighting — Design Specification

## Overview

Two independent features:
- **Split Panes:** Split the editor area horizontally or vertically to view/edit multiple notes side-by-side
- **Code Block Highlighting:** Use KSyntaxHighlighting in reading mode for 300+ language syntax coloring

## Split Panes

### Architecture

Currently `EditorViewManager` holds a single `EditorViewSpace`. Change to a tree of `QSplitter` + `EditorViewSpace` nodes:

```
EditorViewManager
└── m_rootSplitter (QSplitter)
    ├── EditorViewSpace (pane 1)
    └── QSplitter (nested, created on split)
        ├── EditorViewSpace (pane 2)
        └── EditorViewSpace (pane 3)
```

Each `EditorViewSpace` works independently — own tab bar, own editor stack, own active note.

### EditorViewManager Changes

```cpp
class EditorViewManager : public QWidget {
    // Existing API unchanged:
    void openNote(NoteDocument *doc);
    NoteEditorWidget *activeEditor() const;
    EditorViewSpace *activeViewSpace() const;

    // New:
    void splitActiveHorizontal();    // Split active pane left/right
    void splitActiveVertical();      // Split active pane top/bottom
    void closeActiveViewSpace();     // Close active pane (if >1 pane)
    int viewSpaceCount() const;      // Number of open panes

private:
    QSplitter *m_rootSplitter;
    EditorViewSpace *m_activeViewSpace;  // Currently focused pane
    QVector<EditorViewSpace *> m_viewSpaces;  // All panes
};
```

### Split Operation

`splitActiveHorizontal()`:
1. Get the parent splitter of the active `EditorViewSpace`
2. Create a new `EditorViewSpace`
3. If parent splitter orientation matches (Horizontal): insert new space after active
4. If not: wrap the active space in a new `QSplitter(Qt::Horizontal)`, replace active in parent, add both active and new to the new splitter
5. Clone the current note into the new pane (open the same document)
6. Wire signals (activeEditorChanged, etc.) from new space
7. Add to `m_viewSpaces`

### Close Operation

`closeActiveViewSpace()`:
- Only allowed if `viewSpaceCount() > 1`
- Remove the active space from its parent splitter
- Delete the space
- If the parent splitter has only one child left, unwrap it (replace splitter with its single child in the grandparent)
- Focus moves to the remaining sibling

### Active Pane Tracking

- Click inside a pane (on its editor or tab bar) → that pane becomes active
- `m_activeViewSpace` pointer updated
- All "open note" / "save" / "toggle mode" operations target the active pane
- Visual indicator: active pane has a subtle blue border or the tab bar is highlighted (matching Kate's pattern)

### Tab Context Menu

Add to existing `EditorViewSpace::showTabContextMenu()`:
```
Close
Close Others
Close All
---
Split Right          (Ctrl+Shift+Right)
Split Down           (Ctrl+Shift+Down)
```

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+Shift+Right` | Split active pane right |
| `Ctrl+Shift+Down` | Split active pane down |

### Session Persistence

Breadcrumb for future: save/restore split layout in `session.json`. Currently only saves tabs from the active view space. Full split tree serialization deferred.

## Code Block Highlighting (Reading Mode)

### Integration

Add `KF6SyntaxHighlighting` dependency. Use `KSyntaxHighlighting::Repository` and `KSyntaxHighlighting::HtmlHighlighter` in `MarkdownRenderer` to generate syntax-highlighted HTML for code blocks.

### MarkdownRenderer Changes

In `processBlocks()`, when encountering a fenced code block with a language tag:

```cpp
// Current: escapeHtml(codeContent) inside <pre><code>
// New: use KSyntaxHighlighting to generate colored HTML

KSyntaxHighlighting::Repository repo;
auto def = repo.definitionForName(language);
if (def.isValid()) {
    KSyntaxHighlighting::HtmlHighlighter highlighter;
    highlighter.setDefinition(def);
    highlighter.setTheme(repo.defaultTheme(
        KSyntaxHighlighting::Repository::LightTheme));
    // Generate highlighted HTML
    QString highlighted = highlighter.highlightData(codeContent);
    // Use highlighted HTML instead of escaped text
}
```

Actually, `KSyntaxHighlighting::HtmlHighlighter` works on files/streams. For inline strings, we need `KSyntaxHighlighting::AbstractHighlighter` subclass or use the `highlightLine()` approach:

```cpp
// Simpler: process line by line
auto def = repo.definitionForName(language);
if (def.isValid()) {
    KSyntaxHighlighting::State state;
    auto theme = repo.defaultTheme(KSyntaxHighlighting::Repository::LightTheme);
    QString highlighted;
    for (const auto &line : codeContent.split('\n')) {
        auto result = def.highlightLine(line, state);
        // Build HTML spans from result segments
        for (const auto &segment : result) {
            auto format = theme.textColor(segment.format);
            // Wrap in <span style="color:...">
        }
        state = result.state();
    }
}
```

The exact API depends on the KSyntaxHighlighting version. The implementer should check the available API and use the simplest approach that produces colored `<span>` elements.

### Fallback

If the language isn't recognized by KSyntaxHighlighting (or the definition isn't found), fall back to the existing `escapeHtml()` plain rendering. No visual regression.

### Dependencies

Add to root `CMakeLists.txt`:
```cmake
find_package(KF6SyntaxHighlighting REQUIRED)
```

Add to `libs/core/CMakeLists.txt` link libraries:
```cmake
target_link_libraries(corbomite-core PUBLIC Qt6::Core KF6::SyntaxHighlighting)
```

## File Structure

### Split Panes
```
src/editor/
├── EditorViewManager.h/cpp    # Modified — splitter tree, split/close methods
├── EditorViewSpace.h/cpp      # Modified — tab context menu additions

src/app/
├── MainWindow.h/cpp           # Modified — split actions, shortcuts
├── corbomiteui.rc.in          # Modified — split actions in menu
```

### Code Highlighting
```
libs/core/
├── src/MarkdownRenderer.cpp   # Modified — KSyntaxHighlighting for code blocks
├── CMakeLists.txt             # Modified — link KF6::SyntaxHighlighting
CMakeLists.txt                 # Modified — find_package KF6SyntaxHighlighting
```

## Testing

### Split Panes
Manual testing:
- Right-click tab → Split Right → two panes side-by-side
- Split Down → two panes top/bottom
- Close all tabs in one pane → pane removed
- Click between panes → active indicator switches
- Open note in left pane, different note in right pane
- Nested splits (split a split)

### Code Highlighting
- Open a note with ` ```python ` code block
- Toggle to Reading Mode (Ctrl+E)
- Verify Python keywords are colored (def, class, import, etc.)
- Test with ` ```javascript `, ` ```rust `, ` ```cpp `
- Unknown language (` ```foobar `) → plain monospace (no crash)

## What This Does NOT Include

- Linked panes (scroll sync) — future
- Drag tab to create split — future
- Split layout session persistence — future (breadcrumb)
- KSyntaxHighlighting in source mode editor — future (editor already has 24-language keyword coloring from qmarkdowntextedit)
- Configurable scroll-as-zoom vs scroll-as-pan for canvas — future (breadcrumb)
