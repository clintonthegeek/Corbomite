# Cluster G Part 2 — Workspace Containers + Advanced Tab Features (Design Spec)

**Date:** 2026-04-15
**Scope:** Workspace container hierarchy (WorkspaceSplit, WorkspaceTabs, WorkspaceWindow), Obsidian-compatible workspace.json persistence, per-leaf navigation history, leaf-close undo, deferred-load stubs, popout windows, stacked tabs, tab pinning + linked-pane groups. Replaces EditorViewManager, EditorViewSpace, and PaneLayoutBridge.
**Audit references:** `domains/workspace.md` §1–§3 (WorkspaceItem tree, workspace.json schema, leaf-close undo, per-leaf history), `domains/views.md` §12 (deferred-load `eD` stub).
**Depends on:** Cluster G Part 1 (View hierarchy, ViewRegistry, WorkspaceLeaf, TextFileView).

---

## 1. Goal

Replace `EditorViewManager` + `EditorViewSpace` + `PaneLayoutBridge` with a formal workspace container tree that matches Obsidian's `WorkspaceItem > WorkspaceParent > {WorkspaceSplit, WorkspaceTabs, WorkspaceWindow}` hierarchy. Each node carries a stable `id` and `dimension` for layout persistence. The tree serializes directly to/from Obsidian's `workspace.json` schema, enabling layout round-tripping between Corbomite and Obsidian. Ship per-leaf navigation history, leaf-close undo, deferred-load stubs, popout windows, stacked tabs, and tab pinning with linked-pane groups.

---

## 2. Key Design Decisions

### 2.1 Full replace via phased migration (Approach C)

No wrapping or dual-identity. New classes (`Workspace`, `WorkspaceSplit`, `WorkspaceTabs`) are built alongside old ones, then EditorViewSpace's internals migrate into WorkspaceTabs, EditorViewManager's internals migrate into Workspace/WorkspaceSplit, and old classes + PaneLayoutBridge are deleted. Each phase compiles and runs.

### 2.2 Obsidian-compatible workspace.json

Serialization matches Obsidian's schema exactly: `{main, left, right, floating, active, lastOpenFiles}` root with recursive `SplitNode` union (`split`, `tabs`, `leaf`, `window`). Field names, nesting, ViewState shape all match the audit §2 schema. Vaults opened in both apps share layout state.

### 2.3 Popout windows as QWidget + Qt::Window

Lighter weight than full QMainWindow. Sufficient for floating tab groups. Shares process/event loop trivially. Intention to promote to QMainWindow when plugin menus need it (noted in code).

### 2.4 Deferred-load for session performance

Non-visible tabs at session restore get `deferred = true` — only `cachedIcon`/`cachedTitle` from workspace.json are used for tab-header paint. Real View constructed on first focus via `loadIfDeferred()`. Main perf win for large sessions.

---

## 3. Architecture

### 3.1 Class Hierarchy

All abstract/container classes in `libs/core/`. `Workspace` coordinator in `libs/core/` (it's domain logic, not app-specific).

```
WorkspaceItem (abstract base — QObject for signals)
  ├─ id: QString (16-char random hex)
  ├─ dimension: std::optional<int> (0-100 flex-grow)
  ├─ parent: WorkspaceItem*
  ├─ virtual widget() → QWidget*
  ├─ virtual serialize() → QJsonObject
  ├─ static deserialize(QJsonObject, Workspace&) → WorkspaceItem*
  │
  ├─ WorkspaceParent (abstract — can hold children)
  │     ├─ children: QVector<WorkspaceItem*>
  │     ├─ addChild(WorkspaceItem*, int index = -1)
  │     ├─ removeChild(WorkspaceItem*)
  │     ├─ moveChild(int from, int to)
  │     ├─ childCount() / childAt(int)
  │     │
  │     ├─ WorkspaceSplit
  │     │     ├─ direction: Qt::Orientation (Horizontal | Vertical)
  │     │     ├─ m_splitter: QSplitter* (internal widget)
  │     │     ├─ syncs child dimension ↔ QSplitter stretch factors
  │     │     ├─ drag handle resize → updates child dimensions
  │     │     └─ JSON: {id, type:"split", direction:"horizontal"|"vertical", children[], dimension?}
  │     │
  │     ├─ WorkspaceTabs
  │     │     ├─ m_currentTab: int
  │     │     ├─ m_stacked: bool
  │     │     ├─ m_tabBar: QTabBar* (internal)
  │     │     ├─ m_stack: QStackedWidget* (internal, used when !stacked)
  │     │     ├─ m_scrollArea: QScrollArea* (internal, used when stacked)
  │     │     ├─ children are WorkspaceLeaf objects exclusively
  │     │     ├─ tab header paints from leaf cachedIcon/cachedTitle (deferred-safe)
  │     │     ├─ context menu: close, close-others, close-all, pin, split-right, split-down, toggle-stacked
  │     │     └─ JSON: {id, type:"tabs", currentTab, stacked?, children[], dimension?}
  │     │
  │     └─ WorkspaceWindow (popout)
  │           ├─ m_widget: QWidget* (Qt::Window flag)
  │           ├─ x, y, width, height: int
  │           ├─ maximized: bool
  │           ├─ holds one child (WorkspaceSplit or WorkspaceTabs)
  │           ├─ close → reparent leaves back to main workspace active tabs
  │           └─ JSON: {id, type:"window", x, y, width, height, maximize?, children[]}
  │           // NOTE: promote to QMainWindow when plugin menus require per-window menu bars
  │
  └─ WorkspaceLeaf (enhanced from Part 1)
        ├─ view: View* (nullptr when deferred)
        ├─ pinned: bool
        ├─ group: QString (linked-pane group id, empty = none)
        ├─ history: LeafHistory
        ├─ activeTime: qint64 (msecsSinceEpoch, updated on focus)
        ├─ deferred: bool
        ├─ cachedIcon, cachedTitle: QString (for deferred tab paint)
        ├─ loadIfDeferred() → constructs real View via ViewRegistry
        ├─ setPinned(bool) → propagates to group members
        ├─ setGroup(QString) → links with other leaves
        ├─ navigate(ViewState) → pushes current to history, loads new state
        ├─ goBack() / goForward()
        └─ JSON: {id, type:"leaf", state:{type, state?, icon?, title?, active?}, pinned?, group?, dimension?}
```

### 3.2 Workspace Coordinator

```cpp
class Workspace : public QObject {
    WorkspaceSplit *m_mainRoot;          // main window content
    WorkspaceSplit *m_leftSidedock;      // left sidebar
    WorkspaceSplit *m_rightSidedock;     // right sidebar
    QVector<WorkspaceWindow *> m_windows; // popout windows
    WorkspaceLeaf *m_activeLeaf;
    QVector<UndoEntry> m_undoHistory;    // cap 10
    QStringList m_lastOpenFiles;         // most-recent-first
    ViewRegistry *m_viewRegistry;

    // Tree operations
    WorkspaceLeaf *createLeafInParent(WorkspaceTabs *parent);
    void closeLeaf(WorkspaceLeaf *leaf);  // pushes undo, removes from tree
    void undoCloseLeaf();                 // Ctrl+Shift+T
    WorkspaceSplit *splitLeaf(WorkspaceLeaf *leaf, Qt::Orientation dir);

    // Popout
    WorkspaceWindow *popoutLeaf(WorkspaceLeaf *leaf);
    void reparentToMain(WorkspaceWindow *window);  // on window close

    // Persistence
    QJsonObject serialize() const;
    void deserialize(const QJsonObject &json);
    void readWorkspaceJson(const QString &vaultPath);
    void writeWorkspaceJson(const QString &vaultPath);

    // Signals
    Q_SIGNAL void activeLeafChanged(WorkspaceLeaf *leaf);
    Q_SIGNAL void layoutChanged();
    Q_SIGNAL void leafPinnedChanged(WorkspaceLeaf *leaf, bool pinned);
    Q_SIGNAL void leafGroupChanged(WorkspaceLeaf *leaf, const QString &group);
};
```

### 3.3 Per-Leaf Navigation History

```cpp
struct LeafHistoryEntry {
    QString title;
    QString icon;
    QJsonObject state;   // ViewState
    QJsonObject eState;  // ephemeral (cursor, scroll, fold, etc.)
};

class LeafHistory {
    static constexpr int Cap = 20;
    QVector<LeafHistoryEntry> m_back;
    QVector<LeafHistoryEntry> m_forward;

public:
    void push(LeafHistoryEntry current);       // back.push(current), forward.clear(), enforce cap
    LeafHistoryEntry goBack(LeafHistoryEntry current);    // back.pop, forward.push(current)
    LeafHistoryEntry goForward(LeafHistoryEntry current);  // forward.pop, back.push(current)
    bool canGoBack() const;
    bool canGoForward() const;
    QJsonObject serialize() const;
    static LeafHistory deserialize(const QJsonObject &json);
};
```

History is session-only (Obsidian doesn't persist it to workspace.json). Navigation triggers: `WorkspaceLeaf::navigate(ViewState)` pushes current state before loading new. Back/forward buttons in `ItemView` header become functional.

### 3.4 Leaf-Close Undo

```cpp
struct UndoEntry {
    QString leafId;
    QJsonObject state;       // ViewState
    QJsonObject eState;      // ephemeral state
    QString parentId;        // original WorkspaceTabs id
    QString rootId;          // main / left / right / window id
    LeafHistory leafHistory; // preserved navigation history
};
```

- `Workspace::closeLeaf()` → captures entry, unshifts to `m_undoHistory`, pops tail if > 10
- `Workspace::undoCloseLeaf()` → pops entry, finds original parent by `parentId` (falls back to active tabs if gone), creates new leaf, restores state + history
- Bound to `Ctrl+Shift+T` via KStandardAction or command registry

### 3.5 Deferred-Load Stubs

On session restore (`Workspace::deserialize`):
1. For each leaf node in workspace.json, if the leaf is NOT the `active` leaf and NOT the `currentTab` of its parent WorkspaceTabs:
   - Set `deferred = true`
   - Store `cachedIcon` and `cachedTitle` from the ViewState's `icon`/`title` fields
   - Do NOT construct the View — `leaf->view()` returns nullptr
2. WorkspaceTabs paints tab headers from `cachedIcon`/`cachedTitle` regardless of deferred state
3. On first tab-bar click or programmatic focus:
   - `WorkspaceLeaf::loadIfDeferred()` → creates View via `ViewRegistry::createView(type)`, calls `view->setState(state)`, sets `deferred = false`
   - Emit `Workspace::layoutChanged()` so UI updates

### 3.6 Stacked Tabs

When `WorkspaceTabs::m_stacked == true`:
- QTabBar hidden
- All child leaves rendered vertically in a QScrollArea, each with a clickable tab-header strip above its content
- Click on any leaf's header strip → that leaf gets focus (scroll-into-view + `Workspace::setActiveLeaf`)
- Toggle via context menu "Toggle stacked tabs" or command
- `lastTabGroupStacked` captured before dissolving a single-tab group, so stacked preference survives tab removal and re-addition

### 3.7 Tab Pinning + Linked-Pane Groups

**Pinning:**
- `WorkspaceLeaf::setPinned(true)` → pinned tabs sort to the left of the tab bar
- Pinned tabs excluded from "Close others" / "Close all"
- Opening a link from a pinned tab → opens in the nearest unpinned leaf in the same WorkspaceTabs (creates one if needed), not in the pinned tab itself
- Tab header shows pin icon overlay

**Linked-pane groups:**
- `WorkspaceLeaf::setGroup(groupId)` → links leaves across any WorkspaceTabs/WorkspaceSplit
- `setPinned` propagates to all group members
- Opening a file in one group member → other members with matching navigation logic react (e.g., outline pane tracks active file)
- Group id serialized per-leaf in workspace.json

### 3.8 Popout Windows

- `Workspace::popoutLeaf(leaf)` → creates `WorkspaceWindow`, reparents leaf into a new `WorkspaceTabs` inside the window, shows window
- Drag-tab-out-of-bar gesture → calls `popoutLeaf()` (stretch goal; can be deferred)
- `WorkspaceWindow::closeEvent` → `Workspace::reparentToMain(window)` moves all leaves back to main root's active WorkspaceTabs, then deletes window
- Popout windows tracked in `Workspace::m_windows`, serialized under `floating` key in workspace.json
- Multiple tabs can be dragged into an existing popout window

### 3.9 workspace.json I/O

**Schema (matches Obsidian exactly):**
```json
{
  "main": { SplitNode },
  "left": { SplitNode },
  "right": { SplitNode },
  "floating": { "type": "window", ... },
  "left-ribbon": { "hiddenItems": {} },
  "active": "16-char-leaf-id",
  "lastOpenFiles": ["path1.md", "path2.md"]
}
```

**Read:** `Workspace::readWorkspaceJson(vaultPath)` → reads `<vault>/.obsidian/workspace.json`, calls `deserialize()` to build the tree. Missing file → default layout (single WorkspaceTabs with one empty leaf).

**Write:** `Workspace::writeWorkspaceJson(vaultPath)` → calls `serialize()`, writes JSON. Triggered on: vault close, periodic autosave (30s debounce after layout change), explicit save command.

**PaneLayoutBridge deletion:** Once `Workspace::serialize/deserialize` is functional, `PaneLayoutBridge` + `PaneLayout` + `PaneLayoutIndex` + `PaneLeaf` are deleted. `EditorViewManager::buildPaneLayout/applyPaneLayout` replaced by `Workspace::readWorkspaceJson/writeWorkspaceJson`.

---

## 4. Migration Phases (Approach C)

### Phase 1: Build workspace tree abstraction
- `WorkspaceItem`, `WorkspaceParent`, `WorkspaceSplit`, `WorkspaceTabs` classes in `libs/core/`
- `Workspace` coordinator in `libs/core/`
- `LeafHistory` and `UndoEntry` types
- Enhance `WorkspaceLeaf` with history, pinned, group, deferred fields
- Unit tests for tree operations, serialization round-trip (workspace.json schema), history, undo
- `Workspace` wraps the existing QSplitter tree read-only at this phase (builds the logical tree from the runtime widget tree on demand)

### Phase 2: Migrate tabs (EditorViewSpace → WorkspaceTabs)
- WorkspaceTabs gains QTabBar + QStackedWidget internals
- Tab open/close/switch, context menu, modified indicators, middle-click close
- All EditorViewSpace tab logic moves into WorkspaceTabs
- EditorViewSpace becomes a thin shell forwarding to WorkspaceTabs
- ViewRegistry-based openFile/openView moves to WorkspaceTabs
- Legacy tab support (NoteEditorWidget direct) absorbed — all tabs go through WorkspaceLeaf

### Phase 3: Migrate splits (EditorViewManager → Workspace + WorkspaceSplit)
- WorkspaceSplit gains QSplitter internals
- Split/unsplit operations move from EditorViewManager to Workspace
- MainWindow wires Workspace instead of EditorViewManager
- Graph/canvas/hover-popover/suggester propagation moves to Workspace
- EditorViewManager becomes a thin shell, then deleted
- EditorViewSpace deleted (fully absorbed into WorkspaceTabs)

### Phase 4: Delete old classes + wire workspace.json
- Delete PaneLayoutBridge, PaneLayout, PaneLayoutIndex, PaneLeaf
- Delete EditorViewManager, EditorViewSpace remnants
- Workspace::readWorkspaceJson/writeWorkspaceJson replace buildPaneLayout/applyPaneLayout
- Vault open → readWorkspaceJson; vault close → writeWorkspaceJson
- Autosave on layout change (30s debounce)

### Phase 5: Deferred-load stubs
- Session restore creates deferred leaves (no View construction)
- Tab headers paint from cached icon/title
- loadIfDeferred() on first focus
- Tests: verify only active tab's View is constructed at restore

### Phase 6: Per-leaf history + undo
- LeafHistory wired into WorkspaceLeaf navigation
- Back/forward buttons in ItemView header enabled
- Workspace undo stack wired to closeLeaf/undoCloseLeaf
- Ctrl+Shift+T bound
- Tests: history cap, undo cap, restore-to-original-parent

### Phase 7: Popout windows
- WorkspaceWindow class (QWidget + Qt::Window)
- popoutLeaf/reparentToMain on Workspace
- Serialization under "floating" key
- Close-window reparent behavior
- Tests: popout lifecycle, serialize round-trip, close-reparent

### Phase 8: Stacked tabs + pinning + linked-pane groups
- WorkspaceTabs stacked mode (QScrollArea with vertical leaf strips)
- Tab pinning (sort-left, exclude from close-others, redirect link-open)
- Linked-pane group propagation
- Tests: stacked toggle, pin sort order, group propagation

---

## 5. Files

### New files in `libs/core/`

| File | Responsibility |
|---|---|
| `include/corbomite/core/WorkspaceItem.h` + `src/WorkspaceItem.cpp` | Abstract base with id, dimension, parent, serialize |
| `include/corbomite/core/WorkspaceParent.h` + `src/WorkspaceParent.cpp` | Abstract parent with children management |
| `include/corbomite/core/WorkspaceSplit.h` + `src/WorkspaceSplit.cpp` | Split container with QSplitter |
| `include/corbomite/core/WorkspaceTabs.h` + `src/WorkspaceTabs.cpp` | Tab container with QTabBar + QStackedWidget |
| `include/corbomite/core/WorkspaceWindow.h` + `src/WorkspaceWindow.cpp` | Popout window container |
| `include/corbomite/core/Workspace.h` + `src/Workspace.cpp` | Coordinator: tree ops, undo, persistence |
| `include/corbomite/core/LeafHistory.h` + `src/LeafHistory.cpp` | Per-leaf back/forward navigation history |

### Enhanced files

| File | Changes |
|---|---|
| `include/corbomite/core/WorkspaceLeaf.h` + `src/WorkspaceLeaf.cpp` | Add history, pinned, group, deferred, cachedIcon/Title, loadIfDeferred(), navigate(), goBack/Forward() |
| `include/corbomite/core/View.h` | Minor — ItemView header back/forward buttons wired |

### Deleted files (Phase 3-4)

| File | Replacement |
|---|---|
| `src/editor/EditorViewSpace.h` + `.cpp` | WorkspaceTabs |
| `src/editor/EditorViewManager.h` + `.cpp` | Workspace + WorkspaceSplit |
| `libs/core/include/corbomite/core/PaneLayoutBridge.h` + `src/PaneLayoutBridge.cpp` | Workspace serialize/deserialize |
| `libs/core/include/corbomite/core/PaneLayout.h` + `src/PaneLayout.cpp` | Workspace serialize/deserialize |

### Modified files

| File | Changes |
|---|---|
| `src/app/MainWindow.h` + `.cpp` | Own Workspace instead of EditorViewManager; wire signals |
| `libs/core/CMakeLists.txt` | Add new sources, remove deleted sources |
| `tests/core/CMakeLists.txt` | Add new test executables |

### New test files

| File | Tests |
|---|---|
| `tests/core/tst_workspace_tree.cpp` | Tree ops: add/remove/move children, split/unsplit |
| `tests/core/tst_workspace_serialize.cpp` | Obsidian workspace.json round-trip |
| `tests/core/tst_workspace_tabs.cpp` | Tab open/close/switch, stacked mode, pinning sort |
| `tests/core/tst_leaf_history.cpp` | Push/back/forward, cap 20, serialize |
| `tests/core/tst_leaf_undo.cpp` | Close undo stack, cap 10, restore-to-parent |
| `tests/core/tst_workspace_deferred.cpp` | Deferred load, loadIfDeferred, cached icon/title |
| `tests/core/tst_workspace_window.cpp` | Popout lifecycle, reparent on close |

---

## 6. Invariants

1. Every WorkspaceItem has a unique 16-char hex id, generated at construction, preserved through serialization round-trips.
2. WorkspaceTabs children are exclusively WorkspaceLeaf instances.
3. WorkspaceSplit children are WorkspaceSplit or WorkspaceTabs instances — never WorkspaceLeaf directly. WorkspaceWindow is a top-level container owned by Workspace, not nested in splits.
4. A deferred leaf's `view()` returns nullptr until `loadIfDeferred()`. Tab paint must not dereference view.
5. Undo history cap is 10. Navigation history cap is 20 per leaf. Both enforce on push.
6. Pinned tabs always sort left of unpinned tabs within the same WorkspaceTabs.
7. workspace.json written by Corbomite must be readable by Obsidian and vice versa.
8. Closing a popout window reparents all its leaves to the main workspace — no leaves are lost.
9. `Workspace::activeLeaf` is never null when any leaf exists in the tree.
