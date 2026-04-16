# Cluster G Part 1 — Views Hierarchy + TextFileView Contract (Design Spec)

**Date:** 2026-04-15
**Scope:** Phases 1–4 of the Cluster G scouting doc. View class hierarchy, ViewRegistry, WorkspaceLeaf (thin), TextFileView debounced-save + three-way merge, rewire EditorViewSpace. Part 2 (deferred-load stubs, popout windows, stacked tabs, tab pin, leaf-close undo, per-leaf history) is a separate spec.
**Audit references:** `domains/views.md` §1–§11, `domains/workspace.md` §1 (WorkspaceLeaf only), `01-markoff-gaps.md` (three-way merge + backup signals).

---

## 1. Goal

Replace the ad-hoc view-creation branching in `EditorViewSpace` with a formal `View` class hierarchy matching Obsidian's `View > ItemView > FileView > EditableFileView > TextFileView` chain, a `ViewRegistry` factory, and a thin `WorkspaceLeaf` that hosts one `View` per tab. Ship `TextFileView`'s debounced-save + three-way-merge-on-external-modify + save-failure-backup contract. Three concrete View subclasses (`MarkdownView`, `CanvasView`, `GraphView`) replace the existing hardcoded creation paths.

---

## 2. Key Design Decisions

### 2.1 View has-a Component (composition, not inheritance)

`Corbomite::Component` is deliberately not a QObject (see `Component.h` lines 82–84: "avoids MOC + multiple-inheritance pitfalls for subclasses that want their own Q_OBJECT"). Since `View` must be a QWidget (it hosts UI), it cannot also inherit `Component` without hitting Qt's no-multiple-QObject-inheritance constraint.

**Solution:** `View` extends `QWidget` and owns a `std::unique_ptr<Component>` member. Lifecycle methods (`load`/`unload`, `registerQObjectConnection`, `addChild`, `registerInterval`) are forwarded to the internal Component. The Obsidian "View is-a Component" relationship becomes "View has-a Component" in Qt.

### 2.2 Go straight to B — full EditorViewSpace rewire

No adapter layer. `EditorViewSpace` is rewritten to host `WorkspaceLeaf > View` instances. The old `openNote()`, `openCanvas()`, `openGraphView()` methods are replaced by `openView(viewType, state)` routing through `ViewRegistry`. The `QTabBar` + `QStackedWidget` machinery stays (it works), but now manages `WorkspaceLeaf` objects instead of raw widgets.

### 2.3 Three-way merge via diff-match-patch

Matches Obsidian exactly (same algorithm, same merge semantics). Use the Apache-2 licensed C++ port of Google's diff-match-patch. Single-file dependency, no external library.

### 2.4 Save-failure backup (minimal)

On write failure, save content to `.obsidian/file-recovery/<filename>-<timestamp>.md` and show a Notice. No recovery UI in this spec — just a data-loss safety net.

### 2.5 WorkspaceLeaf is thin in Part 1

`WorkspaceLeaf` owns one `View`, handles `setViewState`/`getViewState`, and serializes to workspace.json. History stacks, pinning, grouping, and deferred-load are stubs or omitted — they ship in Part 2.

---

## 3. Architecture

### 3.1 Class hierarchy

```
QWidget
  └─ View  ─────────────────────────────  has-a Component
       └─ ItemView  ────────────────────  header chrome (title, actions, "..." menu)
            └─ FileView  ───────────────  bound to one file; breadcrumbs; rename/delete reaction
                 └─ EditableFileView  ──  inline title rename
                      └─ TextFileView  ─  debounced save + three-way merge + backup
```

`WorkspaceLeaf` is not in this hierarchy — it's a container that owns one `View`.

### 3.2 Registration flow

```
App boot:
  ViewRegistry created (lives on App or equivalent singleton)
  ├─ registerViewWithExtensions({"md"}, "markdown", MarkdownView::factory)
  ├─ registerViewWithExtensions({"canvas"}, "canvas", CanvasView::factory)
  ├─ registerView("graph", GraphView::factory)
  └─ (future: image, audio, video, pdf — stub registrations)

Opening a file:
  EditorViewSpace::openFile(path)
    → ext = path.suffix()
    → type = viewRegistry->getTypeByExtension(ext)
    → factory = viewRegistry->getViewCreatorByType(type)
    → leaf = new WorkspaceLeaf(this)
    → view = factory(leaf)
    → leaf->open(view)
    → add leaf to tab bar + stacked widget
```

### 3.3 Workspace tree (current scope)

```
EditorViewManager
  └─ QSplitter tree (existing, unchanged)
       └─ EditorViewSpace  ←  becomes WorkspaceTabs-equivalent
            ├─ WorkspaceLeaf → MarkdownView
            ├─ WorkspaceLeaf → CanvasView
            └─ WorkspaceLeaf → GraphView
```

The `EditorViewManager` splitter tree stays as-is. `EditorViewSpace` evolves from "ad-hoc tab host" to "WorkspaceTabs-equivalent container of WorkspaceLeaf objects."

### 3.4 Save flow (TextFileView)

```
User edits text
  → subclass calls requestSave()
  → 2000ms trailing-edge debounce
  → save()
      → currentData = getViewData()
      → skip if lastSavedData == currentData
      → FileSystemAdapter::write(file, currentData)
      → on success: lastSavedData = currentData, dirty = false
      → on failure: show Notice, write backup to .obsidian/file-recovery/
      → if saveAgain: re-run save()

External modify detected (FileWatchReactor or vault signal):
  → skip if this.saving (our own write echoing)
  → freshDisk = vault.read(file)
  → if lastSavedData == freshDisk: no-op
  → if getViewData() == freshDisk: no-op (caught up)
  → else: merged = diffMatchPatch.threeWayMerge(lastSavedData, getViewData(), freshDisk)
  → show "File changed on disk" Notice
  → setViewData(merged, false)
  → lastSavedData = freshDisk

Tab close / file switch:
  → save(immediate=true) — synchronous, bypasses debounce
  → clear()
```

---

## 4. New Classes

### 4.1 `Corbomite::View` — `libs/core/`

Base class for anything mounted inside a `WorkspaceLeaf`.

**Public API:**
- `explicit View(WorkspaceLeaf *leaf)` — stores leaf pointer, creates internal Component
- `Component *component() const` — access lifecycle manager
- `void registerQObjectConnection(const QMetaObject::Connection &conn)` — delegates to component
- `void addChild(Component *child)` — delegates to component
- `int registerInterval(int ms, std::function<void()> fn)` — delegates to component
- `virtual QString getViewType() const = 0`
- `virtual QString getDisplayText() const = 0`
- `virtual QString getIcon() const` — default `"document"`
- `void open(QWidget *parent)` — reparent containerEl, component->load(), call onOpen()
- `void close()` — call onClose(), component->unload(), deparent
- `virtual QJsonObject getState() const` — default `{}`
- `virtual void setState(const QJsonObject &state)` — default no-op
- `virtual QJsonObject getEphemeralState() const` — default `{}`
- `virtual void setEphemeralState(const QJsonObject &state)` — default no-op
- `virtual void onPaneMenu(QMenu *menu)` — default no-op
- `virtual void onTabMenu(QMenu *menu)` — default adds Close/Close Others/Close All
- `virtual void onResize()` — default no-op

**Protected:**
- `virtual void onOpen()` — subclass async setup (containerEl is in DOM, component is loaded)
- `virtual void onClose()` — subclass cleanup before unload
- `WorkspaceLeaf *m_leaf`
- `QWidget *m_containerWidget` — content area (created in constructor)

**Private:**
- `std::unique_ptr<Component> m_component`

**Invariants (from audit §8):**
- `containerEl` is attached between `open()` and `close()`; subclasses must not reparent it
- `component->load()` runs exactly once per instance, between `open()` and `close()`
- `onload` fires before `onOpen`; `onOpen` runs with containerEl already in the widget tree

### 4.2 `Corbomite::ItemView` — `libs/core/`

Extends `View`. Adds header chrome.

**Public API:**
- Inherits all of `View`
- `QWidget *contentWidget() const` — where subclass content goes (below header)
- `QWidget *headerWidget() const` — the header bar
- `void addAction(const QString &icon, const QString &title, std::function<void()> callback)` — add icon button to header actions area
- `virtual void onMoreOptionsMenu(QMenu *menu)` — subclass hook for the "..." menu

**Layout:** Header bar (icon + title + action buttons + "..." menu) above content widget. Back/forward nav buttons are stubs in Part 1 (enabled in Part 2 when per-leaf history ships).

### 4.3 `Corbomite::FileView` — `libs/core/`

Extends `ItemView`. Bound to one file.

**Public API:**
- `NoteDocument *file() const` — current file (nullptr until first `loadFile`)
- `bool loadFile(NoteDocument *file)` — orchestrate unload-current → set file → onLoadFile → update title/breadcrumbs. Returns false on failure (file stays nullptr, Notice shown).
- `virtual bool canAcceptExtension(const QString &ext) const` — default false
- `QString getDisplayText() const override` — defaults to `file->baseName()` or i18n("No file")
- `QJsonObject getState() const override` — serializes `{file: relativePath}`
- `void setState(const QJsonObject &state) override` — resolves path → loadFile

**Protected (subclass overrides):**
- `virtual void onLoadFile(NoteDocument *file)` — called after `m_file` is set
- `virtual void onUnloadFile(NoteDocument *file)` — called before `m_file` is cleared

**Vault reactions (subscribed in onload via Component):**
- `vault::rename` → update breadcrumbs + title
- `vault::delete` → step back in history if possible, else show empty-tab view

**Navigation:** `m_navigation = true` (leaf can navigate via this view).

### 4.4 `Corbomite::EditableFileView` — `libs/core/`

Extends `FileView`. Inline title rename.

**Public API:**
- Title label is editable (click to enter rename, Enter/Tab saves, Escape reverts)
- Live validation during rename (reserved names, invalid characters)
- Context menu: adds "Rename..." and "Delete" items

**Implementation:** Reuses the existing rename infrastructure from `NoteDocument`/`VaultModel`. Focus/blur handlers on the title label, validation via a helper function. Escape reverts to the original name without triggering a filesystem rename.

### 4.5 `Corbomite::TextFileView` — `libs/core/`

Extends `EditableFileView`. The core contract for text-backed file editors.

**Public API:**
- `void requestSave()` — kick 2000ms trailing-edge debounce
- `void save(bool immediate = false)` — the full save pipeline
- `void saveImmediately()` — public shortcut: if dirty, save now (for quit handlers)

**Pure virtual (subclass contract):**
- `virtual QString getViewData() const = 0` — return current view content as string
- `virtual void setViewData(const QString &data, bool clear) = 0` — set content; `clear=true` means fresh load (reset undo history), `clear=false` means merge/update
- `virtual void clear() = 0` — reset to empty state

**Save pipeline (from audit §1 TextFileView):**
1. Early-exit if no file or file deleted
2. Re-entry guard: if `m_saving`, set `m_saveAgain = true` and return (unless `immediate`)
3. `currentData = getViewData()`. Skip if `m_lastSavedData == currentData` or `m_lastSavedData` is null (never loaded)
4. Snapshot `previousLastSaved`. If `immediate`: null out data fields + call `clear()`
5. Write via `FileSystemAdapter::write(file, currentData)`
6. On failure: restore `m_lastSavedData = previousLastSaved`, show Notice, write backup
7. Finally: clear `m_saving`; if `m_saveAgain && !immediate`, re-run `save()`

**Three-way merge on external modify:**
- Subscribes to vault file-change signals
- Skipped when `m_saving` (self-echo suppression)
- Read fresh content from disk
- If `lastSavedData == freshDisk` → no-op
- If `getViewData() == freshDisk` → no-op
- Else: `merged = DiffMatchPatch::threeWayMerge(lastSavedData, getViewData(), freshDisk)`
- Show "File changed on disk" Notice
- `setViewData(merged, false)`
- Update `lastSavedData = freshDisk`

**Save-failure backup:**
- On write failure, write content to `.obsidian/file-recovery/<basename>-<ISO8601-timestamp>.md`
- Create the directory if it doesn't exist
- Show Notice with the backup path

**Instance fields:**
- `QString m_data` — last known content
- `bool m_dirty = false`
- `bool m_saving = false`
- `bool m_saveAgain = false`
- `QString m_lastSavedData` — null string means "never loaded"
- Debounce timer (QTimer, 2000ms single-shot)

**Lifecycle:**
- `onLoadFile(file)` → read file content → `m_lastSavedData = content` → `setViewData(content, true)`
- `onUnloadFile(file)` → `save(true)` (immediate, synchronous)

### 4.6 `Corbomite::ViewRegistry` — `libs/core/`

Maps viewType→factory and extension→viewType.

**Public API:**
- `using ViewFactory = std::function<View *(WorkspaceLeaf *)>`
- `void registerView(const QString &type, ViewFactory factory)` — throws on duplicate
- `void unregisterView(const QString &type)` — silent no-op if absent
- `void registerExtensions(const QStringList &exts, const QString &type)` — atomic: any existing ext → throw with no mutation
- `void unregisterExtensions(const QStringList &exts)`
- `void registerViewWithExtensions(const QStringList &exts, const QString &type, ViewFactory factory)` — convenience
- `ViewFactory getViewCreatorByType(const QString &type) const` — nullptr if unregistered
- `QString getTypeByExtension(const QString &ext) const` — empty if unregistered
- `bool isExtensionRegistered(const QString &ext) const`

**Signals:**
- `viewRegistered(const QString &type)`
- `viewUnregistered(const QString &type)`
- `extensionsUpdated()`

**Lifecycle:** One per app. Created before workspace layout restore so built-in factories exist at restore time. Plugin-registered entries come/go via register/unregister.

**Built-in registrations (at boot):**

| Extensions | View type | Factory | Base class |
|---|---|---|---|
| `md` | `"markdown"` | `MarkdownView::factory` | `TextFileView` |
| `canvas` | `"canvas"` | `CanvasView::factory` | `FileView` |
| — | `"graph"` | `GraphView::factory` | `ItemView` |

Image, audio, video, PDF view registrations are deferred (stubs or follow-up).

### 4.7 `Corbomite::WorkspaceLeaf` — `libs/core/`

Container that hosts one `View`. Thin in Part 1.

**Public API:**
- `explicit WorkspaceLeaf(QWidget *parent)` — generates 16-char random ID
- `QString id() const`
- `View *view() const`
- `void open(View *newView)` — close current view if any, open new view
- `QJsonObject getViewState() const` — `{type, state, icon?, title?}`
- `void setViewState(const QJsonObject &state)` — look up factory in ViewRegistry, construct View, open it, call setState
- `QJsonObject getEphemeralState() const` — delegates to view
- `void setEphemeralState(const QJsonObject &state)` — delegates to view
- `QJsonObject serialize() const` — full leaf JSON for workspace.json: `{id, type:"leaf", state: ViewState}`
- `static WorkspaceLeaf *deserialize(const QJsonObject &json, ViewRegistry *registry, QWidget *parent)`

**Part 2 additions (stubs or omitted):**
- `pinned`, `group` — fields exist for serialization round-trip but not enforced
- `history` — no history stack in Part 1
- `isDeferred` / `loadIfDeferred()` — Part 2

### 4.8 `Corbomite::DiffMatchPatch` — `libs/core/`

Thin wrapper around the diff-match-patch C++ port.

**Public API:**
- `static QString threeWayMerge(const QString &base, const QString &local, const QString &remote)` — given the common ancestor, local edits, and remote edits, produce merged text. On conflict, remote wins (matches Obsidian behavior).

**Implementation:** Vendor the Apache-2 `diff_match_patch.h` (Neil Fraser's C++ port) into `libs/core/third_party/`. The wrapper provides the specific three-way merge operation Obsidian uses: `diff(base, remote)` → `patchMake` → `patchApply(local)`.

---

## 5. Concrete View Subclasses

### 5.1 `Corbomite::MarkdownView` — `src/editor/`

Extends `TextFileView`. The primary markdown editor.

**Absorbs:** The guts of `NoteEditorWidget` — three-mode stack (Source/LivePreview/Reading), ephemeral state (scroll, cursor, folds, mode), hover popover integration, editor suggest integration.

**Subclass contract:**
- `getViewType()` → `"markdown"`
- `getViewData()` → current text from whichever mode is active
- `setViewData(data, clear)` → push text into active mode; if `clear`, reset undo
- `clear()` → empty all three modes
- `getState()` → `{file: path, mode: "source"|"preview", source?: bool}`
- `setState(state)` → load file + restore mode
- `getEphemeralState()` → scroll, cursor, folds, mode
- `setEphemeralState(state)` → restore scroll, cursor, folds

**Relationship to NoteEditorWidget:** `MarkdownView` embeds `NoteEditorWidget` as a private child widget inside its `contentWidget()`. NoteEditorWidget's three-mode stack, ephemeral state, hover popover, and editor suggest wiring remain intact — MarkdownView delegates to them via forwarding methods. The public `openNote(NoteDocument*)` path through EditorViewSpace is removed; MarkdownView's `onLoadFile()` calls `m_editorWidget->setNoteDocument(file)` directly. This preserves the tested NoteEditorWidget internals while giving MarkdownView ownership of the TextFileView contract (save, merge, backup).

### 5.2 `Corbomite::CanvasView` — `src/canvas/`

Extends `FileView`. Canvas file editor.

**Wraps:** `CanvasViewTab` as the content widget.

**Subclass contract:**
- `getViewType()` → `"canvas"`
- `getIcon()` → `"canvas"` (or appropriate theme icon)
- `onLoadFile(file)` → load .canvas JSON into CanvasViewTab
- `onUnloadFile(file)` → save if modified
- `canAcceptExtension("canvas")` → true

### 5.3 `Corbomite::GraphView` — `src/graph/`

Extends `ItemView` (not FileView — graph isn't bound to one file).

**Wraps:** `GraphViewTab` as the content widget.

**Subclass contract:**
- `getViewType()` → `"graph"`
- `getDisplayText()` → i18n("Graph view")
- `getIcon()` → `"graph"` (or `git-fork` / similar)
- `onOpen()` → build graph from metadata cache
- `getState()` → filter settings, zoom, etc.

---

## 6. EditorViewSpace Transformation

### Before (current)

```cpp
// Ad-hoc branching
void EditorViewSpace::openNote(NoteDocument *doc) { /* create NoteEditorWidget */ }
void EditorViewSpace::openCanvas(const QString &path) { /* create CanvasViewTab */ }
void EditorViewSpace::openGraphView(...) { /* create GraphViewTab */ }
```

### After

```cpp
// ViewRegistry-based factory
WorkspaceLeaf *EditorViewSpace::openFile(const QString &path) {
    QString ext = QFileInfo(path).suffix().toLower();
    QString type = m_viewRegistry->getTypeByExtension(ext);
    if (type.isEmpty()) return nullptr;  // unknown extension
    return openView(type, {{"file", path}});
}

WorkspaceLeaf *EditorViewSpace::openView(const QString &type, const QJsonObject &state) {
    auto factory = m_viewRegistry->getViewCreatorByType(type);
    if (!factory) return nullptr;
    auto *leaf = new WorkspaceLeaf(this);
    auto *view = factory(leaf);
    leaf->open(view);
    view->setState(state);
    // add to tab bar + stacked widget
    addLeaf(leaf);
    return leaf;
}
```

`EditorViewSpace` gains:
- `m_viewRegistry` pointer (set at construction)
- `m_leaves: QVector<WorkspaceLeaf *>` (replaces `m_editors` hash)
- `openFile(path)` — extension dispatch via registry
- `openView(type, state)` — direct type dispatch
- `addLeaf(leaf)` / `removeLeaf(leaf)` — tab bar + stack management
- `activeLeaf()` → `WorkspaceLeaf *`
- `leafForPath(path)` — find existing leaf for a file (reuse if `canAcceptExtension`)

`EditorViewManager` changes:
- `openNote(NoteDocument*)` → `openFile(doc->relativePath())`
- `openCanvas(path)` → `openFile(path)`
- `openGraphView(...)` → `openView("graph", state)`
- `buildPaneLayout()` / `applyPaneLayout()` → updated to serialize/deserialize via `WorkspaceLeaf::serialize()` / `WorkspaceLeaf::deserialize()`

---

## 7. PaneLayout / WorkspaceState Integration

`PaneLeaf` (the existing struct in `PaneLayout.h`) already has the right shape — `id`, `viewType`, `filePath`, `viewState`, `unknown`. The key change:

**Serialization:** `WorkspaceLeaf::serialize()` produces a `QJsonObject` matching the `PaneLeaf` schema. `PaneLayoutBridge::serializeFromSplitter` now asks each `EditorViewSpace` for its `WorkspaceLeaf` objects instead of building `PaneLeaf` structs from raw widget inspection.

**Deserialization:** `PaneLayoutBridge::deserializeIntoSplitter` creates `WorkspaceLeaf` objects from the `PaneLeaf` data, calling `leaf->setViewState(state)` which goes through ViewRegistry to construct the correct View.

`PaneLeaf` remains a data-transfer struct for `WorkspaceState` file I/O. `WorkspaceLeaf` serializes to / deserializes from `QJsonObject` directly (matching the workspace.json schema). `PaneLayoutBridge` converts between `PaneLeaf` (domain model) and `WorkspaceLeaf` (live object) during layout save/restore.

---

## 8. External Modify Detection

Today `FileWatchReactor` detects file creates/deletes but not content changes on open files. `TextFileView`'s three-way merge needs content-change notifications.

**Approach:** Extend `FileWatchReactor` (or add a parallel watcher) to detect mtime changes on files that have open `TextFileView` instances. When a change is detected:
1. Emit a signal (e.g., `fileModifiedExternally(relativePath)`)
2. `TextFileView` subscribes and runs the merge logic from §4.5

**Self-echo suppression:** The existing `FileWatchReactor::suppressPath()` mechanism (1000ms suppression after our own writes) prevents false triggers. `TextFileView::save()` calls suppress before writing.

---

## 9. Testing Strategy

### Unit tests (libs/core)
- `tst_view_lifecycle` — View open/close lifecycle, Component delegation, state round-trip
- `tst_textfileview` — debounced save timing, re-entry guard (`saveAgain`), skip-when-clean, immediate save on unload
- `tst_textfileview_merge` — three-way merge scenarios: clean merge, conflict (remote wins), no-op cases (lastSaved==disk, view==disk)
- `tst_textfileview_backup` — save failure triggers backup file creation with correct content
- `tst_viewregistry` — register/unregister, duplicate throws, atomic extension registration, type/ext lookup
- `tst_workspaceleaf` — serialize/deserialize round-trip, setViewState through registry, view swap

### Integration tests (src/)
- `tst_editorviewspace_views` — open file by extension → correct View type created via registry; tab management with leaves
- `tst_pane_layout_roundtrip` — serialize live layout → workspace.json → deserialize → verify same view types and states

### What's NOT tested here
- Deferred-load stubs (Part 2)
- Per-leaf history (Part 2)
- Popout windows, stacked tabs, tab pinning (Part 2)

---

## 10. Scope Boundaries

### In scope (this spec)
- `View`, `ItemView`, `FileView`, `EditableFileView`, `TextFileView` class hierarchy
- `ViewRegistry` with built-in registrations (markdown, canvas, graph)
- `WorkspaceLeaf` (thin: view ownership, setViewState, serialize)
- `MarkdownView`, `CanvasView`, `GraphView` concrete subclasses
- `DiffMatchPatch` wrapper (three-way merge)
- Save-failure backup to `.obsidian/file-recovery/`
- `EditorViewSpace` rewire to host WorkspaceLeaf > View
- `EditorViewManager` updated to use ViewRegistry paths
- `PaneLayout`/`WorkspaceState` round-trip through new types
- External-modify detection for open TextFileViews
- Unit + integration tests

### Out of scope (Part 2 or later)
- `eD` deferred-load stub, `tD` empty-tab view, `nD` unknown-type fallback
- `WorkspaceSplit` / `WorkspaceTabs` container hierarchy (formal classes)
- Popout windows (`WorkspaceWindow`)
- Stacked tabs mode
- Tab pinning + linked-pane groups
- Leaf-close undo (cap 10) + per-leaf back/forward history (cap 20)
- Image/audio/video/PDF View subclasses
- File-recovery UI (beyond the backup-on-failure safety net)
- Plugin-facing `registerView` / `registerExtensions` wrappers (Cluster N)

---

## 11. File Inventory

### New files in libs/core/
- `include/corbomite/core/View.h` + `src/View.cpp`
- `include/corbomite/core/ItemView.h` + `src/ItemView.cpp`
- `include/corbomite/core/FileView.h` + `src/FileView.cpp`
- `include/corbomite/core/EditableFileView.h` + `src/EditableFileView.cpp`
- `include/corbomite/core/TextFileView.h` + `src/TextFileView.cpp`
- `include/corbomite/core/ViewRegistry.h` + `src/ViewRegistry.cpp`
- `include/corbomite/core/WorkspaceLeaf.h` + `src/WorkspaceLeaf.cpp`
- `include/corbomite/core/DiffMatchPatch.h` + `src/DiffMatchPatch.cpp`
- `third_party/diff_match_patch.h` (vendored, Apache-2)

### New files in src/
- `src/editor/MarkdownView.h` + `src/editor/MarkdownView.cpp`
- `src/canvas/CanvasView.h` + `src/canvas/CanvasView.cpp`
- `src/graph/GraphView.h` + `src/graph/GraphView.cpp`

### Modified files
- `libs/core/CMakeLists.txt` — add new source files
- `src/editor/EditorViewSpace.h` + `.cpp` — rewrite to host WorkspaceLeaf > View
- `src/editor/EditorViewManager.h` + `.cpp` — update to use ViewRegistry paths
- `src/CMakeLists.txt` — add new source files
- `libs/core/src/PaneLayoutBridge.cpp` — updated serialization/deserialization
- `src/reactors/FileWatchReactor.h` + `.cpp` — add content-change detection for open files
- `src/MainWindow.cpp` — create ViewRegistry, register built-ins, wire to EditorViewManager

### New test files
- `tests/tst_view_lifecycle.cpp`
- `tests/tst_textfileview.cpp`
- `tests/tst_textfileview_merge.cpp`
- `tests/tst_textfileview_backup.cpp`
- `tests/tst_viewregistry.cpp`
- `tests/tst_workspaceleaf.cpp`
- `tests/tst_editorviewspace_views.cpp`
- `tests/tst_pane_layout_roundtrip.cpp` (updated)

---

## 12. Dependencies

- **diff-match-patch C++ port** — vendor into `libs/core/third_party/`. Apache-2 license, compatible with GPL-3.
- **No new Qt modules.** Everything uses Qt6::Core, Qt6::Widgets already linked.
- **No new KDE frameworks.**

---

## 13. Migration Notes

### AutosaveReactor
The existing `AutosaveReactor` (2000ms debounced save via `NoteService::saveNote()`) will be superseded by `TextFileView`'s built-in debounced save for markdown files. `AutosaveReactor` can be removed once all text-backed views go through `TextFileView`. During the transition, ensure only one save path is active per document — `MarkdownView` should disconnect from `AutosaveReactor` when it takes ownership.

### NoteEditorWidget
`MarkdownView` absorbs `NoteEditorWidget`'s functionality. `NoteEditorWidget` may be retained as an internal implementation detail of `MarkdownView` (embed the existing widget as the content of the three-mode stack) or its code may be migrated directly. The implementation plan will determine which approach minimizes risk.

### Signal rewiring
Signals currently emitted by `EditorViewSpace` (`activeEditorChanged`, `cursorInfoChanged`, `internalLinkClicked`, etc.) will be re-routed through the View/Leaf layer. `EditorViewSpace` still emits them, but now derives the information from `activeLeaf()->view()` instead of from raw `NoteEditorWidget*` pointers.
