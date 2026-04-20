# Replace qmarkdowntextedit with Markoff

**Date:** 2026-04-03
**Status:** Approved

## Goal

Replace the third-party `qmarkdowntextedit` (QPlainTextEdit subclass) with our bespoke `Markoff::Editor` (QGraphicsView-based) as the markdown editing widget. This eliminates an external dependency, gains tree-sitter parsing, live preview mode, Obsidian-native syntax support, and a richer signal API.

## Approach: Composition

NoteEditorWidget changes from inheriting `QMarkdownTextEdit` to owning a `Markoff::Editor*` as a child widget. Its public API and signals remain identical, so all consumers (EditorViewSpace, EditorViewManager, MainWindow) require no changes.

## Changes

### 1. NoteEditorWidget rewrite

**Before:** `class NoteEditorWidget : public QMarkdownTextEdit`
**After:** `class NoteEditorWidget : public QWidget` with a `Markoff::Editor *m_editor` child in a `QVBoxLayout`.

**Public API preserved (no signature changes):**
- `setNoteDocument(NoteDocument *doc)` / `noteDocument()`
- `setVaultModel(VaultModel *vault)`
- `currentLine()` / `currentColumn()`
- Signal: `cursorInfoChanged(int line, int column, int wordCount)`
- Signal: `linkActivated(const QString &targetPath)`

**Removed (handled by Markoff):**
- `keyPressEvent`, `mousePressEvent`, `mouseMoveEvent` overrides — Markoff emits `linkClicked`, `linkHovered`, `wikiLinkTrigger`, `tagTrigger`, `completionDismissHint` signals instead
- `wikiLinkTargetAtCursor()` — Markoff detects link clicks internally
- `resolveWikiLinkTarget()` — moved to VaultResourceProvider

**Signal wiring:**
| Markoff signal | NoteEditorWidget handler |
|---|---|
| `textChanged()` | `onTextChanged()` — syncs content to NoteDocument |
| `cursorPositionChanged(int line, int col)` | emits `cursorInfoChanged(line, col, wordCount)` |
| `wordCountChanged(int count)` | caches word count for cursorInfoChanged |
| `linkClicked(const QString &target)` | resolves target via VaultResourceProvider, emits `linkActivated(resolvedPath)` |
| `wikiLinkTrigger(int pos)` | `triggerWikiLinkCompletion()` |
| `tagTrigger(int pos)` | `triggerTagCompletion()` |
| `completionDismissHint()` | `dismissCompletion()` |

**Completion popup key interception:** NoteEditorWidget installs an event filter on `m_editor`. When the completion popup is visible, Up/Down/Enter/Tab/Escape are intercepted and forwarded to the popup. All other keys pass through to Markoff.

**Popup positioning:** Uses `m_editor->cursorScreenRect()` to get global screen coordinates for popup placement, replacing the old `cursorRect()` + `mapToGlobal()` pattern.

**syncFromDocument():** Calls `m_editor->setPlainText(doc->markdown())` instead of the inherited `setPlainText()`.

### 2. VaultResourceProvider

New file: `src/editor/VaultResourceProvider.h` / `src/editor/VaultResourceProvider.cpp`

```cpp
class VaultResourceProvider : public Markoff::ResourceProvider
```

Implements the four virtual methods using VaultModel and the current note path:

- `resolveImage(name)` — resolves image paths relative to the current note, falling back to vault-root search
- `resolveEmbed(name)` — resolves embed targets, returns file content for transclusion
- `resolveLink(target)` — resolves wiki link targets to file URLs
- `linkExists(target)` — checks if a wiki link target exists in the vault (enables Markoff's broken-link styling)

Constructed with `VaultModel*` and the current note's base path. Set on the editor in `NoteEditorWidget::setNoteDocument()` via `m_editor->setResourceProvider()`.

### 3. CMake changes

**Root CMakeLists.txt:**
- Remove: `add_subdirectory(libs/qmarkdowntextedit)`
- Add `add_subdirectory(libs/markoff)` if not already present

**src/CMakeLists.txt:**
- Replace `qmarkdowntextedit` with `Markoff::Markoff` in `target_link_libraries`

**tests/editor/CMakeLists.txt:**
- Remove the `tst_obsidian_highlighting` test target entirely (Markoff has its own 8-test suite covering the same syntax elements)

### 4. File cleanup

- Delete `tests/editor/tst_obsidian_highlighting.cpp`
- Remove `libs/qmarkdowntextedit` git submodule:
  - Remove entry from `.gitmodules`
  - Remove entry from `.git/config`
  - Remove cached submodule: `git rm --cached libs/qmarkdowntextedit`
  - Delete the directory

### 5. Consumer impact

**No changes required in:**
- `EditorViewSpace` — creates `NoteEditorWidget`, calls `setNoteDocument()`, connects `cursorInfoChanged`
- `EditorViewManager` — passes `NoteEditorWidget*` pointers
- `MainWindow` — connects `linkActivated`, calls `setVaultModel()`, reads `noteDocument()`

All consumers interact with NoteEditorWidget's public API, which is unchanged.

## What this enables (not in scope)

These are future benefits of Markoff that are not part of this migration:

- Source/LivePreview mode toggle via `Editor::setMode()`
- Formatting actions (bold, italic, code, tables, callouts, etc.)
- Find and replace
- Heading outline via `headingsChanged` signal
- Theme customization
- Reading view via `Markoff::ReadingView`
