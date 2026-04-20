# libforcegraph — Force-Directed Graph Library Design Specification

## Overview

A standalone, reusable Qt6/C++ library for interactive force-directed graph visualization. Two layers: a computation engine (Qt6::Core only) and a QGraphicsView-based rendering layer (Qt6::Widgets). Designed for 100-10,000 node graphs with animated physics simulation.

Published as a standalone git repository at `clintonthegeek/forcegraph`, integrated into Corbomite as a git submodule at `libs/forcegraph/`.

## Algorithm: Fruchterman-Reingold with Barnes-Hut

Based on the Handbook of Graph Drawing (Tamassia, ed.), Chapter 12.

### Force Model

```
Attractive force (edges):  f_a(d) = d² / k
Repulsive force (all pairs): f_r(d) = -k² / d
Optimal distance: k = C × √(area / |V|)   where C ≈ 1.0
```

### Temperature Cooling

Linear inverse decay controls maximum displacement per iteration:
```
temperature = initial_T × (1 - iteration / max_iterations)
displacement = min(computed_force_magnitude, temperature)
```
Initial temperature: `canvas_width / 10`

### Oscillation Detection (Frick et al.)

Per-vertex local temperature prevents nodes bouncing back and forth:
```
if dot(current_displacement, previous_displacement) < 0:
    vertex_temperature *= 0.9  // Reduce — oscillating
else:
    vertex_temperature = min(vertex_temperature * 1.1, global_temperature)
```

### Convergence Detection

Simulation is stable when `max_vertex_displacement < area × 0.0001` for 5 consecutive iterations.

### Repulsion Acceleration

**Grid acceleration (simple, always enabled):**
- Divide canvas into cells of size `k/2`
- Compute repulsion only from vertices in same cell + 8 neighbors
- O(|V|) per iteration for uniformly distributed graphs

**Barnes-Hut quadtree (for precise large-graph repulsion):**
- Hierarchical spatial decomposition
- Distant node clusters treated as single aggregate
- Theta parameter (default 0.8) controls accuracy/speed tradeoff
- O(|V| log |V|) per iteration
- Enabled when node count > 500

### Future: Multilevel Coarsening (Tier 2)

```cpp
// TODO: Implement multilevel coarsening for graphs > 2000 nodes.
// Approach: GRIP-style vertex filtration (Gajer et al., 2000)
// - Build 3-4 levels of coarsening via maximal independent set filtration
// - Layout coarse graph first (fewer iterations), project down
// - Refine with FR at each finer level
// This solves the local minima problem that basic FR cannot escape.
// Reference: Handbook of Graph Drawing, Section 12.6
```

### Future: Intelligent Initial Placement (Tier 2)

```cpp
// TODO: Replace random initialization with BFS-based placement.
// - Pick a random seed node, BFS to assign rough positions
// - Nodes at BFS distance d placed at radius proportional to d
// - Cuts convergence iterations by 50-75% (per handbook)
```

## Layer 1: ForceLayoutEngine

Pure computation engine. Depends only on Qt6::Core. No widgets, no rendering.

### Public API

```cpp
namespace ForceGraph {

struct GraphNode {
    QString id;
    QString label;
    double radius = 5.0;
    QColor color = QColor(123, 108, 217);  // Purple default
    QPointF position;
    bool pinned = false;
};

struct GraphEdge {
    QString sourceId;
    QString targetId;
};

class ForceLayoutEngine : public QObject {
    Q_OBJECT
public:
    explicit ForceLayoutEngine(QObject *parent = nullptr);

    // Data
    void setNodes(const QVector<GraphNode> &nodes);
    void setEdges(const QVector<GraphEdge> &edges);
    void clear();

    // Simulation control
    void start();               // Begin simulation on internal thread
    void stop();                // Pause simulation
    void step();                // Single iteration (for testing)
    bool isRunning() const;
    bool isStable() const;

    // Node manipulation
    void pinNode(const QString &id, QPointF position);
    void unpinNode(const QString &id);
    void addNode(const GraphNode &node);
    void removeNode(const QString &id);
    void addEdge(const GraphEdge &edge);
    void removeEdge(const QString &sourceId, const QString &targetId);

    // Parameters (Obsidian-style slider controls)
    void setCenterForce(double force);      // 0.0 - 0.1, default 0.01
    void setRepelForce(double force);       // 0 - 5000, default 1500
    void setLinkForce(double force);        // 0.0 - 0.5, default 0.05
    void setLinkDistance(double distance);  // 20 - 500, default 100
    void setDamping(double damping);        // 0.5 - 1.0, default 0.85

    // Read state
    QVector<GraphNode> nodes() const;
    int nodeCount() const;
    int edgeCount() const;

signals:
    void positionsUpdated(const QHash<QString, QPointF> &positions);
    void simulationStarted();
    void simulationStopped();
    void simulationStable();
};

} // namespace ForceGraph
```

### Internal Components

**QuadTree** (Barnes-Hut spatial partitioning):
```cpp
class QuadTree {
public:
    void build(const QVector<GraphNode> &nodes, const QRectF &bounds);
    QPointF computeRepulsion(const QPointF &nodePos, double repelForce,
                              double theta = 0.8) const;
    void clear();

private:
    struct QuadNode {
        QRectF bounds;
        QPointF centerOfMass;
        double totalMass = 0;
        int nodeIndex = -1;      // Leaf: index into nodes array
        int children[4] = {-1, -1, -1, -1};  // NW, NE, SW, SE
        bool isLeaf() const { return nodeIndex >= 0; }
    };
    QVector<QuadNode> m_nodes;   // Flat array (no heap allocation per node)
};
```

**SimulationWorker** (runs on QThread):
- Timer-driven at ~33ms (30fps)
- Each tick: compute forces → apply displacements → check convergence → emit positionsUpdated
- Auto-stops when stable (emits simulationStable)
- Resumes on data change (addNode, removeNode, etc.)

## Layer 2: QGraphicsView Rendering

Interactive graph visualization widget.

### ForceGraphView (QGraphicsView)

```cpp
namespace ForceGraph {

class ForceGraphView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ForceGraphView(QWidget *parent = nullptr);

    void setEngine(ForceLayoutEngine *engine);
    void setNodes(const QVector<GraphNode> &nodes);
    void setEdges(const QVector<GraphEdge> &edges);

    // Visual customization
    void setNodeColor(const QString &id, const QColor &color);
    void setHighlightedNode(const QString &id);
    void clearHighlight();
    void zoomToFit();

signals:
    void nodeClicked(const QString &id);
    void nodeDoubleClicked(const QString &id);
    void nodeHovered(const QString &id);

protected:
    void wheelEvent(QWheelEvent *event) override;    // Zoom
    void mousePressEvent(QMouseEvent *event) override; // Pan start
    void mouseMoveEvent(QMouseEvent *event) override;  // Pan + drag
    void mouseReleaseEvent(QMouseEvent *event) override;
};

}
```

### ForceGraphScene (QGraphicsScene)

Owns all node and edge items. Updates positions from engine signals.

### ForceGraphNode (QGraphicsEllipseItem)

**LOD rendering** (from Qt "40000 chips" pattern):
```cpp
void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) override {
    double lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
        painter->worldTransform());

    if (lod < 0.1) {
        // Ultra-low: single pixel dot
        painter->fillRect(boundingRect(), m_color);
        return;
    }
    if (lod < 0.4) {
        // Low: filled circle, no label
        painter->setBrush(m_color);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(boundingRect());
        return;
    }
    if (lod < 1.0) {
        // Medium: circle + abbreviated label
        painter->setBrush(m_color);
        painter->drawEllipse(boundingRect());
        painter->drawText(boundingRect().translated(0, m_radius + 4),
                          Qt::AlignHCenter, m_label.left(12));
        return;
    }
    // Full: circle + full label + outline
    painter->setBrush(m_color);
    painter->setPen(QPen(m_color.darker(130), 1));
    painter->drawEllipse(boundingRect());
    painter->drawText(boundingRect().translated(0, m_radius + 4),
                      Qt::AlignHCenter, m_label);
}
```

Node radius scales with connection count: `radius = base + log(1 + connections) × scale`

### ForceGraphEdge (QGraphicsLineItem)

Simple line between node centers. Hidden at very low LOD. Optional: dimmed when not connected to highlighted node.

### Interaction

| Action | Behavior |
|--------|----------|
| Scroll wheel | Zoom in/out (centered on cursor) |
| Click + drag empty | Pan |
| Click node | Emit nodeClicked(id) |
| Double-click node | Emit nodeDoubleClicked(id) |
| Hover node | Highlight node + connected edges, dim others |
| Drag node | Pin node, update engine position |
| Double-click pinned node | Unpin |
| Home key | zoomToFit() |
| +/- keys | Zoom in/out |

## Project Structure

```
libs/forcegraph/
├── CMakeLists.txt
├── LICENSE                       # GPL-3.0
├── README.md
├── include/forcegraph/
│   ├── GraphTypes.h              # GraphNode, GraphEdge structs
│   ├── ForceLayoutEngine.h       # Computation engine
│   ├── QuadTree.h                # Barnes-Hut spatial partitioning
│   ├── ForceGraphView.h          # QGraphicsView widget
│   ├── ForceGraphScene.h         # QGraphicsScene
│   ├── ForceGraphNode.h          # Node QGraphicsItem with LOD
│   └── ForceGraphEdge.h          # Edge QGraphicsItem
├── src/
│   ├── ForceLayoutEngine.cpp
│   ├── QuadTree.cpp
│   ├── ForceGraphView.cpp
│   ├── ForceGraphScene.cpp
│   ├── ForceGraphNode.cpp
│   └── ForceGraphEdge.cpp
└── tests/
    ├── CMakeLists.txt
    ├── tst_forcelayout.cpp       # Engine unit tests
    └── tst_quadtree.cpp          # QuadTree unit tests
```

## CMake

```cmake
project(forcegraph VERSION 0.1.0 LANGUAGES CXX)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(forcegraph STATIC
    src/ForceLayoutEngine.cpp
    src/QuadTree.cpp
    src/ForceGraphView.cpp
    src/ForceGraphScene.cpp
    src/ForceGraphNode.cpp
    src/ForceGraphEdge.cpp
    # Headers for AUTOMOC
    include/forcegraph/ForceLayoutEngine.h
    include/forcegraph/ForceGraphView.h
    include/forcegraph/ForceGraphScene.h
    include/forcegraph/ForceGraphNode.h
    include/forcegraph/ForceGraphEdge.h
)

target_include_directories(forcegraph
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(forcegraph PUBLIC Qt6::Core Qt6::Widgets)
```

## Testing

### tst_forcelayout.cpp (engine tests, headless)

- **testTwoNodesConverge:** Two connected nodes converge to approximately `linkDistance` apart
- **testRepulsionSpreadsNodes:** Unconnected nodes repel to fill available space
- **testTemperatureDecreases:** Temperature drops over iterations
- **testConvergenceDetection:** `isStable()` becomes true after sufficient iterations
- **testPinnedNodeStays:** Pinned node position doesn't change
- **testParameterEffects:** Changing repelForce changes final layout spread
- **testEmptyGraph:** Zero nodes/edges doesn't crash
- **testSingleNode:** Single node stays near origin

### tst_quadtree.cpp (spatial partitioning tests)

- **testBuildFromNodes:** QuadTree builds without crash, contains all nodes
- **testRepulsionApproximation:** Barnes-Hut repulsion within 10% of naive O(n²) computation
- **testEmptyTree:** Empty tree returns zero force
- **testSingleNodeTree:** Single node, query from different position returns repulsive force

## What This Does NOT Include

- Corbomite integration (Sub-project 3d)
- Graph filtering (3d)
- Color groups by query (3d)
- Local graph mode (3d)
- Multilevel coarsening (Tier 2 — breadcrumb comments)
- ForceAtlas2 algorithm (Tier 3)
- GPU rendering (Tier 3)
- 3D graph layout
- Edge labels
- Directed edge arrows
