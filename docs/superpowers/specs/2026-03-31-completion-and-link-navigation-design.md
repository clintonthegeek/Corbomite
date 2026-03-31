# Completion Popups & Link Navigation — Design Specification

## Overview

Add three interactive editor features matching Obsidian's UX:
- **Wikilink autocomplete:** Triggered on `[[`, fuzzy-filters vault notes, inserts `[[NoteName]]`
- **Tag autocomplete:** Triggered on `#`, shows existing vault tags, inserts selected tag
- **Ctrl+Click navigation:** Click a wikilink while holding Ctrl to open the target note

All three are implemented in `NoteEditorWidget` with a shared `CompletionPopup` widget.

## CompletionPopup Widget

Reusable popup for both wikilink and tag completion:

```
CompletionPopup (QFrame, Qt::Popup | Qt::FramelessWindowHint)
├── QListView (results, single column, no header)
│   ├── Source model (provided by caller)
│   └── FuzzyFilterProxyModel (KFuzzyMatcher-based, reuse pattern from QuickSwitcher)
└── CompletionDelegate (highlights matched characters)
```

### Behavior
- Positioned below the text cursor using `cursorRect()` mapped to global coordinates
- `WA_DeleteOnClose` — auto-destroyed when dismissed
- Does NOT steal focus from the editor — keystrokes stay in the editor
- Editor forwards relevant keys to the popup:
  - Arrow Up/Down: navigate selection
  - Enter/Tab: accept selected item
  - Escape: dismiss popup
  - Any other key: update filter text
- Dismisses on: Escape, focus loss, clicking outside, accepting an item
- Width: 300px. Height: grows with results, max 200px (scrollable).

### API
```cpp
class CompletionPopup : public QFrame {
    Q_OBJECT
public:
    explicit CompletionPopup(QAbstractItemModel *sourceModel, QWidget *parent = nullptr);

    void setFilterText(const QString &text);
    void selectNext();
    void selectPrevious();
    QString selectedText() const;  // Returns DisplayRole of current selection
    QString selectedData() const;  // Returns UserRole+1 of current selection (path for wikilinks)
    bool hasSelection() const;

signals:
    void itemSelected(const QString &text, const QString &data);
    void dismissed();
};
```

### CompletionDelegate
Simplified version of QuickSwitcherDelegate — highlights matched characters using KFuzzyMatcher::matchedRanges(). No folder path display (keeps it compact for inline use).

## Wikilink Autocomplete

### Trigger
In `NoteEditorWidget::keyPressEvent()`, after the base class handles the key:
- If `[` was just typed and the character before it is also `[` → trigger wikilink completion
- The `[[` is already in the document; the popup helps complete the note name

### Model
Reuse `QuickSwitcherModel` populated from `VaultModel::allNotes()`. The `NoteNameRole` provides display text, `NotePathRole` provides the relative path for insertion.

### Insertion
On `itemSelected`: insert `NoteName]]` at cursor position (the `[[` is already typed). The closing `]]` is appended automatically.

If the user types `[[` and then continues typing without selecting, the popup filters in real-time. If they type `]]` manually, the popup dismisses.

### Future breadcrumbs
```cpp
// Future: support [[Note|Display]] alias insertion
// Future: match against frontmatter aliases
// Future: respect "use markdown links" setting — insert [text](path.md) instead
```

## Tag Autocomplete

### Trigger
In `NoteEditorWidget::keyPressEvent()`, after the base class handles the key:
- If `#` was just typed and it's not a heading (not at line start, or not followed by space) → trigger tag completion
- The `#` is already in the document; the popup helps complete the tag name

### Model
`QStringListModel` populated from `VaultModel::allTags()`. Simple string matching.

### Insertion
On `itemSelected`: insert the tag name at cursor (the `#` is already typed). E.g., if `#pro` is typed and user selects `project/active`, insert `ject/active` to complete `#project/active`.

### VaultModel::allTags()
New method that extracts all unique tags from the vault:
- Scans all notes (cached documents + uncached files via FileSystemAdapter)
- Regex: `(?<![&\w])#([a-zA-Z_][a-zA-Z0-9_/-]*)` (same as highlighter)
- Returns `QStringList` sorted alphabetically, deduplicated
- Cached internally, invalidated on `noteAdded`/`noteRemoved`/`noteModified` signals

### Future breadcrumbs
```cpp
// Future: include tags from frontmatter properties
// Future: show tag usage count alongside name
```

## Ctrl+Click Navigation

### Detection
In `NoteEditorWidget::mousePressEvent()`:
- If `Qt::ControlModifier` is held and left button clicked
- Get text cursor at click position via `cursorForPosition()`
- Check if cursor position falls within a WikiLink InlineRange (from Batch A highlighting)
- If yes: extract the target note path from the wikilink text, emit `linkActivated(targetPath)`

### Cursor hint
In `NoteEditorWidget::mouseMoveEvent()`:
- If Ctrl is held and cursor is over a wikilink range, set cursor to `Qt::PointingHandCursor`
- Otherwise restore default cursor

### Signal
```cpp
// In NoteEditorWidget:
signals:
    void linkActivated(const QString &targetPath);
```

MainWindow connects this to `onNoteActivated()` — opens the target note in a tab. If the target doesn't exist, creates it (same as Quick Switcher create-on-no-match).

### Wikilink text extraction
Parse the text under cursor to extract the target:
- Find surrounding `[[` and `]]` from cursor position
- Extract content between brackets
- If contains `|`, take the part before `|` (that's the target path)
- If contains `#`, take the part before `#` (heading links — open the note, heading scrolling is future)
- Resolve to relative path: append `.md` if not present

## File Structure

```
src/editor/
├── CompletionPopup.h/cpp       # New — reusable completion popup
├── CompletionDelegate.h/cpp    # New — fuzzy match highlight delegate
├── NoteEditorWidget.h/cpp      # Modified — trigger detection, Ctrl+Click, linkActivated signal

libs/models/
├── VaultModel.h/cpp            # Modified — add allTags()

src/app/
├── MainWindow.cpp              # Modified — connect linkActivated signal

tests/models/
├── tst_vaultmodel.cpp          # Modified — add allTags test
```

## Testing

### Unit tests
- `tst_vaultmodel.cpp`: add `testAllTags()` — vault with notes containing `#tag1`, `#nested/tag`, returns sorted unique list
- `tst_vaultmodel.cpp`: add `testAllTagsExcludesCodeBlocks()` — tags inside ``` code blocks ``` are excluded

### Manual testing
- Type `[[` in editor → popup appears with vault notes, fuzzy filtering, Enter inserts `NoteName]]`
- Type `#` in editor → popup appears with vault tags, Enter completes the tag
- Ctrl+Click on `[[Note]]` → opens that note in a new tab
- Ctrl+hover on wikilink → cursor changes to pointing hand

## What This Does NOT Include
- Hover preview popup (needs markdown renderer — Phase 2c)
- Tag autocomplete from frontmatter `tags:` property (future)
- `[[Note|Display]]` alias insertion mode (future)
- "Use markdown links" setting toggle (future)
- Heading scroll on `[[Note#Heading]]` click (future)
- Alias matching in completion (future)
