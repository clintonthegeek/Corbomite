# Graph View Integration — Design Specification

## Overview

Wire libforcegraph into Corbomite with two graph views:
- **Global Graph (Ctrl+G):** Full vault graph in a special editor tab
- **Local Graph:** Current note's neighborhood in the right sidebar

Both use `ForceLayoutEngine` + `ForceGraphView` from libforcegraph, fed with data from `SQLiteIndex`.

## GraphDataBuilder

Utility class that transforms SQLiteIndex link data into ForceGraph node/edge data.

```cpp
class GraphDataBuilder {
public:
    struct Result {
        QVector<ForceGraph::GraphNode> nodes;
        QVector<ForceGraph::GraphEdge> edges;
    };

    // Build global graph from entire vault
    static Result buildGlobalGraph(SQLiteIndex *index, VaultModel *vault);

    // Build local graph centered on a note, N hops deep
    static Result buildLocalGraph(SQLiteIndex *index, VaultModel *vault,
                                   const QString &centerNotePath, int depth = 2);
};
```

**Global graph building:**
1. Query all indexed note paths from `notes_fts` table
2. Query all links from `links` table
3. For each note: create `GraphNode` with id=relativePath, label=name, color by type
4. For each link: create `GraphEdge` with sourceId/targetId
5. Add unresolved nodes (link targets that don't exist as notes) with distinct color
6. Node radius scales with connection count: `radius = 4 + log(1 + degree) * 3`

**Node coloring:**
- Regular notes: `QColor(123, 108, 217)` (purple/accent)
- Unresolved (linked but don't exist): `QColor(136, 136, 136)` (gray)
- Orphan (no links at all): `QColor(170, 170, 170)` (light gray)
- Current note (highlighted): handled by ForceGraphView's highlight system

**Local graph building:**
1. Start from center note
2. BFS to depth N, collecting all notes within N hops
3. Include edges only between collected notes
4. Center note gets a larger radius and distinct color

## GraphViewTab

Widget for the global graph tab in `EditorViewSpace`.

```cpp
class GraphViewTab : public QWidget {
    Q_OBJECT
public:
    explicit GraphViewTab(SQLiteIndex *index, VaultModel *vault, QWidget *parent = nullptr);

    void buildGraph();

signals:
    void noteActivated(const QString &relativePath);

private:
    ForceGraph::ForceGraphView *m_graphView;
    ForceGraph::ForceLayoutEngine *m_engine;
    // Future: add filter controls panel overlay
};
```

**Behavior:**
- Constructor creates engine + view, calls `buildGraph()`
- `buildGraph()` uses `GraphDataBuilder::buildGlobalGraph()` to get data, feeds to engine + view, starts simulation
- `ForceGraphView::nodeClicked` → extracts note path from node id → emits `noteActivated`
- `ForceGraphView::nodeDoubleClicked` → same as click (opens note)

## EditorViewSpace Integration

Currently `EditorViewSpace` manages `NoteEditorWidget` and `NotePreviewWidget` per tab. Add support for a `GraphViewTab`:

- New method: `openGraphView()` — creates `GraphViewTab`, adds to stacked widget, adds tab with graph icon
- Only one graph tab allowed at a time — if already open, activate it
- Tab title: "Graph View" with `QIcon::fromTheme("preferences-system-network")` icon
- Graph tab is closeable like any other tab
- Connect `GraphViewTab::noteActivated` → existing note opening flow

## LocalGraphPanel

Right sidebar panel showing a mini force graph of the current note's neighborhood.

```cpp
class LocalGraphPanel : public QWidget {
    Q_OBJECT
public:
    explicit LocalGraphPanel(QWidget *parent = nullptr);

    void setIndex(SQLiteIndex *index);
    void setVaultModel(VaultModel *vault);
    void setCurrentNote(NoteDocument *doc);

signals:
    void noteActivated(const QString &relativePath);

private:
    void refresh();

    ForceGraph::ForceGraphView *m_graphView;
    ForceGraph::ForceLayoutEngine *m_engine;
    SQLiteIndex *m_index = nullptr;
    VaultModel *m_vault = nullptr;
    NoteDocument *m_currentDoc = nullptr;
};
```

**Behavior:**
- Small graph view (fits in sidebar width ~250px)
- Shows 1-2 hop neighborhood of current note
- Updates when active note changes
- Current note highlighted in center
- Click node → navigates to that note
- Simulation auto-stops when stable (saves CPU)

## MainWindow Integration

**New action:**
- `graph_view` (Ctrl+G) → opens global graph tab

**setupSidebars()** — add LocalGraphPanel to right sidebar:
```cpp
auto *localGraphView = createToolView(nullptr, "local_graph_panel",
    KMultiTabBar::Right, graphIcon, i18n("Local Graph"));
```

**onVaultOpened():**
- Set index + vault on LocalGraphPanel
- Connect activeEditorChanged to update LocalGraphPanel

**XMLGUI** — add graph_view action to Go menu and toolbar. Bump version to 4.

## File Structure

```
src/graph/
├── GraphDataBuilder.h/cpp      # New — builds ForceGraph data from SQLiteIndex
├── GraphViewTab.h/cpp          # New — graph tab widget
├── LocalGraphPanel.h/cpp       # New — right sidebar mini graph

src/editor/
├── EditorViewSpace.h/cpp       # Modified — openGraphView() method

src/app/
├── MainWindow.h/cpp            # Modified — graph action, LocalGraphPanel, wiring
├── corbomiteui.rc.in           # Modified — Ctrl+G action, version 4

tests/graph/
├── CMakeLists.txt
├── tst_graphdatabuilder.cpp    # Unit tests for graph data building
```

## Testing

### tst_graphdatabuilder.cpp

- Build global graph from index with 3 notes + 2 links → 3 nodes, 2 edges
- Unresolved link target creates gray unresolved node
- Orphan note (no links) included with light gray color
- Node radius scales with degree
- Local graph with depth=1 includes only direct neighbors
- Local graph with depth=2 includes 2-hop neighborhood
- Empty index → empty graph

### Manual Testing

- Ctrl+G opens graph tab with animated force layout
- Nodes settle into stable positions
- Click node → opens note in new tab
- Hover node → highlights connections
- Pan/zoom works
- Local graph in sidebar updates when switching notes
- Graph with starter vault (30 notes) renders smoothly

## What This Does NOT Include

- Graph filter controls panel (future — breadcrumb)
- Color groups by search query (future)
- Edge direction arrows (future)
- Graph settings persistence (future)
- 3D graph mode (future)
