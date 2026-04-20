# Quick Switcher & Command Palette — Design Specification

## Overview

Add two modal navigation dialogs matching Obsidian's UX:
- **Command Palette (Ctrl+P):** Fuzzy search across all registered actions. Uses KDE's built-in `KCommandBar` widget.
- **Quick Switcher (Ctrl+O):** Fuzzy search across all notes in the vault. Custom implementation adapted from Kate's QuickOpen pattern.

Both use `KFuzzyMatcher` from KCoreAddons for scoring and match highlighting.

## Command Palette

### Implementation

Zero custom UI code. `KCommandBar` (from `KF6::ConfigWidgets`, already a dependency) handles everything:
- Fuzzy matching of action names
- Keyboard shortcut display
- Recently-used action tracking
- Visual match highlighting

### Integration

New method `MainWindow::showCommandPalette()`:
```cpp
void MainWindow::showCommandPalette()
{
    auto *bar = new KCommandBar(this);
    // Collect action groups from our KActionCollection
    QList<KCommandBar::ActionGroup> groups;
    groups.append({i18n("File"), fileActions});
    groups.append({i18n("View"), viewActions});
    // etc. — built from actionCollection() categorized by naming convention
    bar->setActions(groups);
    bar->show();
}
```

Bound to `Ctrl+P` via KActionCollection.

### Obsidian Fidelity

- Ctrl+P shortcut matches Obsidian exactly
- Fuzzy matching behavior matches (KFuzzyMatcher uses same algorithm family as Obsidian)
- Shows keyboard shortcuts next to commands (KCommandBar does this)
- Recently-used commands appear at top (KCommandBar tracks this)

## Quick Switcher

### UX Specification (matching Obsidian)

- **Trigger:** Ctrl+O
- **Appearance:** Centered popup at top of window, no title bar, rounded corners, drop shadow. Approximately 600px wide, up to 400px tall.
- **Input:** Single-line text field with placeholder "Open note..." at top
- **Results list:** Below input, showing matching notes
- **Empty state:** When input is empty, show recent notes (from LRU tab history)
- **Fuzzy matching:** As user types, filter all vault notes by fuzzy match on filename
- **Result display:** Each row shows note name (bold matched chars) and folder path (muted)
- **Keyboard navigation:**
  - Arrow Up/Down to move selection
  - Enter to open selected note (or create if no match)
  - Escape to dismiss
  - Type to filter
- **Create on no match:** If input doesn't match any note, show "Create: {input}" as last result. Enter creates the note and opens it.
- **Dismiss:** Escape, clicking outside, or opening a note

### Architecture

```
QuickSwitcher (QFrame, Qt::Popup | Qt::FramelessWindowHint)
├── QLineEdit (search input, placeholder "Open note...")
├── QTreeView (results list, single column, no header)
│   ├── QuickSwitcherModel (QAbstractListModel)
│   │   └── All vault notes as rows (path, name)
│   ├── QSortFilterProxyModel
│   │   └── Uses KFuzzyMatcher::match() for filtering + scoring
│   └── QuickSwitcherDelegate (QStyledItemDelegate)
│       └── Uses KFuzzyMatcher::matchedRanges() for highlighting
```

### QuickSwitcherModel

`QAbstractListModel` backed by VaultModel's note list:

```cpp
class QuickSwitcherModel : public QAbstractListModel {
public:
    enum Roles {
        NotePathRole = Qt::UserRole + 1,
        NoteNameRole,
        FolderPathRole
    };

    void setNotes(const QVector<NoteMeta> &notes);
    void setRecentPaths(const QStringList &recentPaths);
    // Recent paths get a score bonus so they appear first when input is empty
};
```

Roles:
- `Qt::DisplayRole` / `NoteNameRole` — note name without extension (e.g. "My Note")
- `FolderPathRole` — parent folder path (e.g. "Projects/")
- `NotePathRole` — full relative path for opening (e.g. "Projects/My Note.md")

### QuickSwitcherDelegate

Custom delegate that:
- Renders note name with matched characters highlighted (bold + accent color)
- Renders folder path to the right in muted color
- Uses `KFuzzyMatcher::matchedRanges()` to determine which characters to highlight

### QuickSwitcher Widget

```cpp
class QuickSwitcher : public QFrame {
    Q_OBJECT
public:
    explicit QuickSwitcher(VaultModel *vault, const QStringList &recentPaths,
                           QWidget *parent = nullptr);

signals:
    void noteSelected(const QString &relativePath);
    void createNoteRequested(const QString &name);

private:
    void updateFilter(const QString &text);
    void activateSelected();
    // Future: add alias matching from frontmatter properties

    QLineEdit *m_input;
    QTreeView *m_resultList;
    QuickSwitcherModel *m_model;
    QSortFilterProxyModel *m_proxyModel; // or custom subclass for scoring
};
```

### Proxy Model with Fuzzy Scoring

Custom `QSortFilterProxyModel` subclass that:
- Accepts rows where `KFuzzyMatcher::matchSimple(pattern, name)` returns true
- Sorts by `KFuzzyMatcher::match(pattern, name).score` descending
- When pattern is empty, sorts recent paths first (by LRU order)
- Appends a virtual "Create: {pattern}" row when no exact match exists

### File Structure

```
src/dialogs/
├── QuickSwitcher.h/cpp
├── QuickSwitcherModel.h/cpp
└── QuickSwitcherDelegate.h/cpp
tests/models/
└── tst_quickswitchermodel.cpp
```

### MainWindow Integration

```cpp
void MainWindow::showQuickSwitcher()
{
    if (!m_vaultService->isOpen()) return;

    QStringList recent = m_editorManager->activeViewSpace()->tabModel()->lruSortedPaths();
    auto *switcher = new QuickSwitcher(m_vaultService->vault(), recent, this);

    // Position at top-center of window
    QPoint center = rect().center();
    int x = center.x() - 300; // half of 600px width
    int y = 80; // offset from top
    switcher->move(mapToGlobal(QPoint(x, y)));

    connect(switcher, &QuickSwitcher::noteSelected,
            this, &MainWindow::onNoteActivated);
    connect(switcher, &QuickSwitcher::createNoteRequested,
            this, [this](const QString &name) {
        auto *doc = m_vaultService->noteService()->createNote(name, QString());
        if (doc) m_editorManager->openNote(doc);
    });

    switcher->show();
    // Future: connect to QuickSwitcher::aliasMatched for frontmatter alias support
}
```

## Testing

### Unit Tests (tst_quickswitchermodel.cpp)

- Model populates from NoteMeta list with correct roles
- NoteNameRole strips .md extension
- FolderPathRole extracts parent folder
- Empty filter returns all notes
- Fuzzy filter "myn" matches "My Note" but not "Other File"
- Score-based sorting: better matches first
- Recent paths get priority when filter is empty
- Model handles empty vault (zero rows)

### Manual Testing

- Ctrl+P opens command palette with all registered actions
- Typing filters commands, Enter executes
- Ctrl+O opens quick switcher
- Empty shows recent notes
- Typing fuzzy-filters vault notes
- Enter opens note, Escape dismisses
- Arrow keys navigate
- "Create: {name}" appears when no match
- Enter on create row creates and opens new note
- Test with starter vault (60 notes) and hub vault (6573 notes) for performance

## XMLGUI Changes

Add to `corbomiteui.rc.in`:
```xml
<Menu name="go">
  <text>&amp;Go</text>
  <Action name="quick_switcher"/>
  <Action name="command_palette"/>
</Menu>
```

## Keyboard Shortcuts

| Shortcut | Action ID | Description |
|----------|-----------|-------------|
| `Ctrl+O` | `quick_switcher` | Open Quick Switcher |
| `Ctrl+P` | `command_palette` | Open Command Palette |

## What This Does NOT Include

- Wikilink autocomplete popup (Phase 2b — will reuse KFuzzyMatcher and similar popup pattern)
- Tag autocomplete popup (Phase 2b)
- Frontmatter alias matching in Quick Switcher (future — breadcrumb comments left in code)
- Global search panel in sidebar (Phase 2c — different widget, not a popup)
