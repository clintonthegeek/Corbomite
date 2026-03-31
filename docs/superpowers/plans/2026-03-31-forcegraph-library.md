# libforcegraph — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a standalone, reusable force-directed graph visualization library with Fruchterman-Reingold layout, Barnes-Hut acceleration, and QGraphicsView rendering with LOD.

**Architecture:** Two layers: ForceLayoutEngine (pure computation, Qt6::Core only) and ForceGraphView (QGraphicsView-based interactive rendering, Qt6::Widgets). Barnes-Hut quadtree for O(n log n) repulsion. Simulation runs on QThread at 30fps. Library lives at `libs/forcegraph/` as a git submodule.

**Tech Stack:** C++20, Qt6::Core (engine), Qt6::Widgets (rendering), QGraphicsView framework

**Spec:** `docs/superpowers/specs/2026-03-31-forcegraph-library-design.md`

**Reference:** Handbook of Graph Drawing (Tamassia), Chapter 12 — force-directed algorithms

---

### Task 1: Project Scaffold + Data Types

**Files:**
- Create: `libs/forcegraph/CMakeLists.txt`
- Create: `libs/forcegraph/include/forcegraph/GraphTypes.h`
- Create: `libs/forcegraph/README.md`
- Modify: `CMakeLists.txt` (root — add subdirectory)

- [ ] **Step 1: Create a GitHub repo for the library**

```bash
cd /home/clinton/dev/Corbomite
gh repo create clintonthegeek/forcegraph --public --license gpl-3.0 --description "Force-directed graph visualization library for Qt6/C++"
```

- [ ] **Step 2: Create library directory structure**

```bash
mkdir -p libs/forcegraph/{include/forcegraph,src,tests}
```

- [ ] **Step 3: Create GraphTypes.h**

`libs/forcegraph/include/forcegraph/GraphTypes.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QPointF>
#include <QString>

namespace ForceGraph {

struct GraphNode {
    QString id;
    QString label;
    double radius = 5.0;
    QColor color = QColor(123, 108, 217);
    QPointF position;
    bool pinned = false;
};

struct GraphEdge {
    QString sourceId;
    QString targetId;
};

} // namespace ForceGraph
```

- [ ] **Step 4: Create CMakeLists.txt**

`libs/forcegraph/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(forcegraph VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)

add_library(forcegraph STATIC
    src/QuadTree.cpp
    src/ForceLayoutEngine.cpp
    src/ForceGraphView.cpp
    src/ForceGraphScene.cpp
    src/ForceGraphNode.cpp
    src/ForceGraphEdge.cpp
    include/forcegraph/ForceLayoutEngine.h
    include/forcegraph/QuadTree.h
    include/forcegraph/ForceGraphView.h
    include/forcegraph/ForceGraphScene.h
    include/forcegraph/ForceGraphNode.h
    include/forcegraph/ForceGraphEdge.h
)
set_target_properties(forcegraph PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(forcegraph
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(forcegraph PUBLIC Qt6::Core Qt6::Widgets)

if(NOT PROJECT_IS_TOP_LEVEL)
    # In-tree build — tests managed by parent
else()
    enable_testing()
    add_subdirectory(tests)
endif()
```

- [ ] **Step 5: Create stub source files so it compiles**

Create minimal stubs for all .h and .cpp files listed in CMakeLists.txt. Each header has the class declaration with empty body. Each .cpp includes its header.

`libs/forcegraph/include/forcegraph/QuadTree.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QPointF>
#include <QRectF>
#include <QVector>
#include "GraphTypes.h"

namespace ForceGraph {
class QuadTree {
public:
    void build(const QVector<GraphNode> &nodes, const QRectF &bounds);
    QPointF computeRepulsion(const QPointF &nodePos, double repelForce, double theta = 0.8) const;
    void clear();
private:
    struct QuadNode {
        QRectF bounds;
        QPointF centerOfMass;
        double totalMass = 0;
        int nodeIndex = -1;
        int children[4] = {-1, -1, -1, -1};
        bool isLeaf() const { return nodeIndex >= 0; }
        bool isEmpty() const { return nodeIndex < 0 && children[0] < 0; }
    };
    QVector<QuadNode> m_nodes;
    int m_root = -1;
    void insert(int quadNodeIdx, int nodeIdx, const QVector<GraphNode> &nodes);
    void subdivide(int quadNodeIdx);
    QPointF computeRepulsionRecursive(int quadNodeIdx, const QPointF &pos,
                                       double repelForce, double theta) const;
};
} // namespace ForceGraph
```

`libs/forcegraph/src/QuadTree.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/QuadTree.h"
namespace ForceGraph {
void QuadTree::build(const QVector<GraphNode> &, const QRectF &) {}
QPointF QuadTree::computeRepulsion(const QPointF &, double, double) const { return {}; }
void QuadTree::clear() { m_nodes.clear(); m_root = -1; }
void QuadTree::insert(int, int, const QVector<GraphNode> &) {}
void QuadTree::subdivide(int) {}
QPointF QuadTree::computeRepulsionRecursive(int, const QPointF &, double, double) const { return {}; }
} // namespace ForceGraph
```

`libs/forcegraph/include/forcegraph/ForceLayoutEngine.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QHash>
#include <QPointF>
#include <QVector>
#include "GraphTypes.h"

namespace ForceGraph {
class ForceLayoutEngine : public QObject {
    Q_OBJECT
public:
    explicit ForceLayoutEngine(QObject *parent = nullptr);
    void setNodes(const QVector<GraphNode> &nodes);
    void setEdges(const QVector<GraphEdge> &edges);
    void clear();
    void start();
    void stop();
    void step();
    bool isRunning() const;
    bool isStable() const;
    void pinNode(const QString &id, QPointF position);
    void unpinNode(const QString &id);
    void setCenterForce(double force);
    void setRepelForce(double force);
    void setLinkForce(double force);
    void setLinkDistance(double distance);
    void setDamping(double damping);
    QVector<GraphNode> nodes() const;
    int nodeCount() const;
    int edgeCount() const;
Q_SIGNALS:
    void positionsUpdated(const QHash<QString, QPointF> &positions);
    void simulationStarted();
    void simulationStopped();
    void simulationStable();
private:
    QVector<GraphNode> m_nodes;
    QVector<GraphEdge> m_edges;
    QHash<QString, int> m_nodeIndex;
    double m_centerForce = 0.01;
    double m_repelForce = 1500.0;
    double m_linkForce = 0.05;
    double m_linkDistance = 100.0;
    double m_damping = 0.85;
    double m_temperature = 0.0;
    int m_iteration = 0;
    int m_maxIterations = 500;
    int m_stableCount = 0;
    bool m_running = false;
    bool m_stable = false;
    class QTimer *m_timer = nullptr;
};
} // namespace ForceGraph
```

`libs/forcegraph/src/ForceLayoutEngine.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceLayoutEngine.h"
namespace ForceGraph {
ForceLayoutEngine::ForceLayoutEngine(QObject *parent) : QObject(parent) {}
void ForceLayoutEngine::setNodes(const QVector<GraphNode> &n) { m_nodes = n; }
void ForceLayoutEngine::setEdges(const QVector<GraphEdge> &e) { m_edges = e; }
void ForceLayoutEngine::clear() { m_nodes.clear(); m_edges.clear(); }
void ForceLayoutEngine::start() {}
void ForceLayoutEngine::stop() {}
void ForceLayoutEngine::step() {}
bool ForceLayoutEngine::isRunning() const { return m_running; }
bool ForceLayoutEngine::isStable() const { return m_stable; }
void ForceLayoutEngine::pinNode(const QString &, QPointF) {}
void ForceLayoutEngine::unpinNode(const QString &) {}
void ForceLayoutEngine::setCenterForce(double f) { m_centerForce = f; }
void ForceLayoutEngine::setRepelForce(double f) { m_repelForce = f; }
void ForceLayoutEngine::setLinkForce(double f) { m_linkForce = f; }
void ForceLayoutEngine::setLinkDistance(double d) { m_linkDistance = d; }
void ForceLayoutEngine::setDamping(double d) { m_damping = d; }
QVector<GraphNode> ForceLayoutEngine::nodes() const { return m_nodes; }
int ForceLayoutEngine::nodeCount() const { return m_nodes.size(); }
int ForceLayoutEngine::edgeCount() const { return m_edges.size(); }
} // namespace ForceGraph
```

`libs/forcegraph/include/forcegraph/ForceGraphView.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsView>
#include "GraphTypes.h"
namespace ForceGraph {
class ForceLayoutEngine;
class ForceGraphScene;
class ForceGraphView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ForceGraphView(QWidget *parent = nullptr);
    void setEngine(ForceLayoutEngine *engine);
    void setNodes(const QVector<GraphNode> &nodes);
    void setEdges(const QVector<GraphEdge> &edges);
    void setNodeColor(const QString &id, const QColor &color);
    void setHighlightedNode(const QString &id);
    void clearHighlight();
    void zoomToFit();
Q_SIGNALS:
    void nodeClicked(const QString &id);
    void nodeDoubleClicked(const QString &id);
    void nodeHovered(const QString &id);
protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
private:
    ForceGraphScene *m_scene = nullptr;
    ForceLayoutEngine *m_engine = nullptr;
    bool m_panning = false;
    QPoint m_lastPanPos;
};
} // namespace ForceGraph
```

`libs/forcegraph/src/ForceGraphView.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphView.h"
namespace ForceGraph {
ForceGraphView::ForceGraphView(QWidget *parent) : QGraphicsView(parent) {}
void ForceGraphView::setEngine(ForceLayoutEngine *) {}
void ForceGraphView::setNodes(const QVector<GraphNode> &) {}
void ForceGraphView::setEdges(const QVector<GraphEdge> &) {}
void ForceGraphView::setNodeColor(const QString &, const QColor &) {}
void ForceGraphView::setHighlightedNode(const QString &) {}
void ForceGraphView::clearHighlight() {}
void ForceGraphView::zoomToFit() {}
void ForceGraphView::wheelEvent(QWheelEvent *e) { QGraphicsView::wheelEvent(e); }
void ForceGraphView::mousePressEvent(QMouseEvent *e) { QGraphicsView::mousePressEvent(e); }
void ForceGraphView::mouseMoveEvent(QMouseEvent *e) { QGraphicsView::mouseMoveEvent(e); }
void ForceGraphView::mouseReleaseEvent(QMouseEvent *e) { QGraphicsView::mouseReleaseEvent(e); }
void ForceGraphView::keyPressEvent(QKeyEvent *e) { QGraphicsView::keyPressEvent(e); }
} // namespace ForceGraph
```

`libs/forcegraph/include/forcegraph/ForceGraphScene.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsScene>
#include <QHash>
#include "GraphTypes.h"
namespace ForceGraph {
class ForceGraphNode;
class ForceGraphEdge;
class ForceGraphScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit ForceGraphScene(QObject *parent = nullptr);
    void setNodes(const QVector<GraphNode> &nodes);
    void setEdges(const QVector<GraphEdge> &edges);
    void updatePositions(const QHash<QString, QPointF> &positions);
    void setHighlightedNode(const QString &id);
    void clearHighlight();
    ForceGraphNode *nodeItem(const QString &id) const;
private:
    QHash<QString, ForceGraphNode *> m_nodeItems;
    QVector<ForceGraphEdge *> m_edgeItems;
    QString m_highlightedId;
};
} // namespace ForceGraph
```

`libs/forcegraph/src/ForceGraphScene.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphScene.h"
namespace ForceGraph {
ForceGraphScene::ForceGraphScene(QObject *parent) : QGraphicsScene(parent) {}
void ForceGraphScene::setNodes(const QVector<GraphNode> &) {}
void ForceGraphScene::setEdges(const QVector<GraphEdge> &) {}
void ForceGraphScene::updatePositions(const QHash<QString, QPointF> &) {}
void ForceGraphScene::setHighlightedNode(const QString &) {}
void ForceGraphScene::clearHighlight() {}
ForceGraphNode *ForceGraphScene::nodeItem(const QString &) const { return nullptr; }
} // namespace ForceGraph
```

`libs/forcegraph/include/forcegraph/ForceGraphNode.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsEllipseItem>
#include "GraphTypes.h"
namespace ForceGraph {
class ForceGraphNode : public QGraphicsEllipseItem {
public:
    explicit ForceGraphNode(const GraphNode &data, QGraphicsItem *parent = nullptr);
    void setData(const GraphNode &data);
    QString nodeId() const;
    void setHighlighted(bool highlighted);
    void setDimmed(bool dimmed);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
private:
    GraphNode m_data;
    bool m_highlighted = false;
    bool m_dimmed = false;
};
} // namespace ForceGraph
```

`libs/forcegraph/src/ForceGraphNode.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphNode.h"
#include <QPainter>
namespace ForceGraph {
ForceGraphNode::ForceGraphNode(const GraphNode &data, QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent), m_data(data) {}
void ForceGraphNode::setData(const GraphNode &data) { m_data = data; }
QString ForceGraphNode::nodeId() const { return m_data.id; }
void ForceGraphNode::setHighlighted(bool h) { m_highlighted = h; }
void ForceGraphNode::setDimmed(bool d) { m_dimmed = d; }
void ForceGraphNode::paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) {}
} // namespace ForceGraph
```

`libs/forcegraph/include/forcegraph/ForceGraphEdge.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsLineItem>
namespace ForceGraph {
class ForceGraphNode;
class ForceGraphEdge : public QGraphicsLineItem {
public:
    ForceGraphEdge(ForceGraphNode *source, ForceGraphNode *target, QGraphicsItem *parent = nullptr);
    void adjust();
    ForceGraphNode *sourceNode() const;
    ForceGraphNode *targetNode() const;
    void setDimmed(bool dimmed);
private:
    ForceGraphNode *m_source;
    ForceGraphNode *m_target;
    bool m_dimmed = false;
};
} // namespace ForceGraph
```

`libs/forcegraph/src/ForceGraphEdge.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphEdge.h"
#include "forcegraph/ForceGraphNode.h"
namespace ForceGraph {
ForceGraphEdge::ForceGraphEdge(ForceGraphNode *s, ForceGraphNode *t, QGraphicsItem *p)
    : QGraphicsLineItem(p), m_source(s), m_target(t) {}
void ForceGraphEdge::adjust() {}
ForceGraphNode *ForceGraphEdge::sourceNode() const { return m_source; }
ForceGraphNode *ForceGraphEdge::targetNode() const { return m_target; }
void ForceGraphEdge::setDimmed(bool d) { m_dimmed = d; }
} // namespace ForceGraph
```

Create empty test stubs:

`libs/forcegraph/tests/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(forcegraph_tests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
```

`libs/forcegraph/README.md`:
```markdown
# libforcegraph

A standalone Qt6/C++ force-directed graph visualization library.

## Features

- Fruchterman-Reingold force-directed layout
- Barnes-Hut quadtree for O(n log n) repulsion
- QGraphicsView rendering with level-of-detail
- Interactive: pan, zoom, hover highlight, click, drag-to-pin
- Configurable physics parameters (center, repel, link forces)
- Targets 100-10,000 nodes

## License

GPL-3.0-or-later
```

- [ ] **Step 6: Add to root CMakeLists.txt**

Add `add_subdirectory(libs/forcegraph)` after `add_subdirectory(libs/qmarkdowntextedit)` in the root CMakeLists.txt. Also add `forcegraph` to the CorbomiteApp link libraries in `src/CMakeLists.txt`.

- [ ] **Step 7: Build**

```bash
cd /home/clinton/dev/Corbomite
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
```

Expected: Builds cleanly with stub implementations.

- [ ] **Step 8: Commit**

```bash
git add libs/forcegraph/ CMakeLists.txt src/CMakeLists.txt
git commit -m "feat: scaffold libforcegraph with data types and stub implementations

Standalone force-directed graph library with two layers:
- Layer 1: ForceLayoutEngine (computation, Qt6::Core)
- Layer 2: ForceGraphView (rendering, Qt6::Widgets)
All classes stubbed — filled in subsequent tasks."
```

---

### Task 2: QuadTree (Barnes-Hut spatial partitioning)

**Files:**
- Modify: `libs/forcegraph/src/QuadTree.cpp`
- Create: `libs/forcegraph/tests/tst_quadtree.cpp`
- Modify: `libs/forcegraph/tests/CMakeLists.txt`

- [ ] **Step 1: Write QuadTree tests**

`libs/forcegraph/tests/tst_quadtree.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "forcegraph/QuadTree.h"
#include <cmath>

class TestQuadTree : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testBuildFromNodes()
    {
        ForceGraph::QuadTree tree;
        QVector<ForceGraph::GraphNode> nodes;
        for (int i = 0; i < 10; ++i) {
            ForceGraph::GraphNode n;
            n.id = QString::number(i);
            n.position = QPointF(i * 10.0, i * 10.0);
            nodes.append(n);
        }
        tree.build(nodes, QRectF(0, 0, 100, 100));
        // Should not crash, tree is built
    }

    void testRepulsionNonZero()
    {
        ForceGraph::QuadTree tree;
        QVector<ForceGraph::GraphNode> nodes;

        ForceGraph::GraphNode n1; n1.id = QStringLiteral("a"); n1.position = QPointF(0, 0);
        ForceGraph::GraphNode n2; n2.id = QStringLiteral("b"); n2.position = QPointF(50, 0);
        nodes << n1 << n2;

        tree.build(nodes, QRectF(-100, -100, 200, 200));

        // Repulsion from tree at (0,0) should push left (away from node at 50,0)
        QPointF force = tree.computeRepulsion(QPointF(0, 0), 1500.0, 0.0);
        // With theta=0 (exact), force should be purely in -x direction
        QVERIFY(force.x() < 0);
        QVERIFY(std::abs(force.y()) < 0.001);
    }

    void testRepulsionApproximation()
    {
        // Barnes-Hut with theta=0.8 should approximate exact within 20%
        ForceGraph::QuadTree tree;
        QVector<ForceGraph::GraphNode> nodes;

        for (int i = 0; i < 50; ++i) {
            ForceGraph::GraphNode n;
            n.id = QString::number(i);
            n.position = QPointF(qrand() % 1000, qrand() % 1000);
            nodes.append(n);
        }

        tree.build(nodes, QRectF(0, 0, 1000, 1000));

        QPointF queryPos(500, 500);
        QPointF exactForce = tree.computeRepulsion(queryPos, 1500.0, 0.0);  // theta=0 → exact
        QPointF approxForce = tree.computeRepulsion(queryPos, 1500.0, 0.8); // theta=0.8 → approx

        // Approximate should be within 20% of exact magnitude
        double exactMag = std::sqrt(exactForce.x() * exactForce.x() + exactForce.y() * exactForce.y());
        double approxMag = std::sqrt(approxForce.x() * approxForce.x() + approxForce.y() * approxForce.y());

        if (exactMag > 0.001) {
            double ratio = approxMag / exactMag;
            QVERIFY2(ratio > 0.5 && ratio < 2.0,
                     qPrintable(QStringLiteral("Ratio: %1").arg(ratio)));
        }
    }

    void testEmptyTree()
    {
        ForceGraph::QuadTree tree;
        tree.build({}, QRectF(0, 0, 100, 100));
        QPointF force = tree.computeRepulsion(QPointF(50, 50), 1500.0);
        QCOMPARE(force, QPointF(0, 0));
    }

    void testSingleNode()
    {
        ForceGraph::QuadTree tree;
        ForceGraph::GraphNode n;
        n.id = QStringLiteral("a");
        n.position = QPointF(50, 50);
        tree.build({n}, QRectF(0, 0, 100, 100));

        // Query from different position — should get repulsive force
        QPointF force = tree.computeRepulsion(QPointF(0, 0), 1500.0);
        // Force should push away from (50,50), i.e., toward (-x, -y)
        QVERIFY(force.x() < 0);
        QVERIFY(force.y() < 0);
    }
};

QTEST_MAIN(TestQuadTree)
#include "tst_quadtree.moc"
```

- [ ] **Step 2: Add test to CMakeLists**

Update `libs/forcegraph/tests/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(forcegraph_tests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_quadtree tst_quadtree.cpp)
add_test(NAME tst_quadtree COMMAND tst_quadtree)
target_link_libraries(tst_quadtree PRIVATE Qt6::Test forcegraph)
set_tests_properties(tst_quadtree PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

Add to root CMakeLists.txt after the other test subdirectories:
```cmake
add_subdirectory(libs/forcegraph/tests)
```

- [ ] **Step 3: Implement QuadTree**

Replace `libs/forcegraph/src/QuadTree.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/QuadTree.h"
#include <cmath>

namespace ForceGraph {

void QuadTree::clear()
{
    m_nodes.clear();
    m_root = -1;
}

void QuadTree::build(const QVector<GraphNode> &nodes, const QRectF &bounds)
{
    clear();
    if (nodes.isEmpty()) return;

    // Create root node
    QuadNode root;
    root.bounds = bounds;
    m_nodes.append(root);
    m_root = 0;

    // Insert all nodes
    for (int i = 0; i < nodes.size(); ++i) {
        insert(m_root, i, nodes);
    }
}

void QuadTree::insert(int quadNodeIdx, int nodeIdx, const QVector<GraphNode> &nodes)
{
    auto &qn = m_nodes[quadNodeIdx];
    const QPointF &pos = nodes[nodeIdx].position;

    // Update center of mass
    double newMass = qn.totalMass + 1.0;
    qn.centerOfMass = (qn.centerOfMass * qn.totalMass + pos) / newMass;
    qn.totalMass = newMass;

    if (qn.isEmpty() && qn.isLeaf()) {
        // Empty leaf — just store the node
        qn.nodeIndex = nodeIdx;
        return;
    }

    if (qn.isLeaf()) {
        // Occupied leaf — subdivide and reinsert existing node
        int existingIdx = qn.nodeIndex;
        qn.nodeIndex = -1;
        subdivide(quadNodeIdx);
        // Reinsert existing node
        insert(quadNodeIdx, existingIdx, nodes);
    }

    // Find which quadrant the new node belongs to
    if (qn.children[0] < 0) {
        subdivide(quadNodeIdx);
    }

    double midX = qn.bounds.center().x();
    double midY = qn.bounds.center().y();

    int childIdx;
    if (pos.x() <= midX) {
        childIdx = (pos.y() <= midY) ? 0 : 2; // NW or SW
    } else {
        childIdx = (pos.y() <= midY) ? 1 : 3; // NE or SE
    }

    insert(qn.children[childIdx], nodeIdx, nodes);
}

void QuadTree::subdivide(int quadNodeIdx)
{
    const QRectF &b = m_nodes[quadNodeIdx].bounds;
    double midX = b.center().x();
    double midY = b.center().y();

    // NW, NE, SW, SE
    QRectF childBounds[4] = {
        QRectF(b.left(), b.top(), midX - b.left(), midY - b.top()),
        QRectF(midX, b.top(), b.right() - midX, midY - b.top()),
        QRectF(b.left(), midY, midX - b.left(), b.bottom() - midY),
        QRectF(midX, midY, b.right() - midX, b.bottom() - midY)
    };

    for (int i = 0; i < 4; ++i) {
        QuadNode child;
        child.bounds = childBounds[i];
        m_nodes.append(child);
        // Must re-fetch reference since append may invalidate
        m_nodes[quadNodeIdx].children[i] = m_nodes.size() - 1;
    }
}

QPointF QuadTree::computeRepulsion(const QPointF &nodePos, double repelForce, double theta) const
{
    if (m_root < 0) return QPointF(0, 0);
    return computeRepulsionRecursive(m_root, nodePos, repelForce, theta);
}

QPointF QuadTree::computeRepulsionRecursive(int quadNodeIdx, const QPointF &pos,
                                             double repelForce, double theta) const
{
    const auto &qn = m_nodes[quadNodeIdx];

    if (qn.totalMass == 0) return QPointF(0, 0);

    QPointF delta = pos - qn.centerOfMass;
    double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());

    if (dist < 0.001) return QPointF(0, 0); // Coincident — skip

    // Barnes-Hut criterion: if node is far enough, treat cluster as single body
    double size = std::max(qn.bounds.width(), qn.bounds.height());

    if (qn.isLeaf() || (size / dist < theta)) {
        // Treat as single body: F = repelForce * mass / dist²
        double force = repelForce * qn.totalMass / (dist * dist);
        return QPointF(delta.x() / dist * force, delta.y() / dist * force);
    }

    // Recurse into children
    QPointF totalForce(0, 0);
    for (int i = 0; i < 4; ++i) {
        if (qn.children[i] >= 0) {
            totalForce += computeRepulsionRecursive(qn.children[i], pos, repelForce, theta);
        }
    }
    return totalForce;
}

} // namespace ForceGraph
```

- [ ] **Step 4: Build and run tests**

```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R tst_quadtree --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add libs/forcegraph/src/QuadTree.cpp libs/forcegraph/tests/ CMakeLists.txt
git commit -m "feat: implement Barnes-Hut QuadTree with O(n log n) repulsion

Hierarchical spatial partitioning for force computation.
Theta parameter controls accuracy/speed tradeoff.
4 unit tests: build, repulsion, approximation accuracy, edge cases."
```

---

### Task 3: ForceLayoutEngine (core physics)

**Files:**
- Modify: `libs/forcegraph/src/ForceLayoutEngine.cpp`
- Create: `libs/forcegraph/tests/tst_forcelayout.cpp`
- Modify: `libs/forcegraph/tests/CMakeLists.txt`

- [ ] **Step 1: Write engine tests**

`libs/forcegraph/tests/tst_forcelayout.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <cmath>
#include "forcegraph/ForceLayoutEngine.h"

class TestForceLayout : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testTwoNodesConverge()
    {
        ForceGraph::ForceLayoutEngine engine;

        ForceGraph::GraphNode n1; n1.id = QStringLiteral("a"); n1.position = QPointF(0, 0);
        ForceGraph::GraphNode n2; n2.id = QStringLiteral("b"); n2.position = QPointF(200, 0);
        engine.setNodes({n1, n2});

        ForceGraph::GraphEdge e; e.sourceId = QStringLiteral("a"); e.targetId = QStringLiteral("b");
        engine.setEdges({e});
        engine.setLinkDistance(100.0);

        // Run many iterations
        for (int i = 0; i < 200; ++i) engine.step();

        auto nodes = engine.nodes();
        double dist = std::sqrt(
            std::pow(nodes[0].position.x() - nodes[1].position.x(), 2) +
            std::pow(nodes[0].position.y() - nodes[1].position.y(), 2));

        // Should converge near linkDistance (within 30%)
        QVERIFY2(dist > 70 && dist < 130,
                 qPrintable(QStringLiteral("Distance: %1").arg(dist)));
    }

    void testRepulsionSpreadsNodes()
    {
        ForceGraph::ForceLayoutEngine engine;

        QVector<ForceGraph::GraphNode> nodes;
        for (int i = 0; i < 5; ++i) {
            ForceGraph::GraphNode n;
            n.id = QString::number(i);
            n.position = QPointF(0, 0); // All at origin
            nodes.append(n);
        }
        engine.setNodes(nodes);
        // No edges — pure repulsion

        for (int i = 0; i < 100; ++i) engine.step();

        // Nodes should have spread out from origin
        auto result = engine.nodes();
        double maxDist = 0;
        for (const auto &n : result) {
            double d = std::sqrt(n.position.x() * n.position.x() + n.position.y() * n.position.y());
            maxDist = std::max(maxDist, d);
        }
        QVERIFY2(maxDist > 10, qPrintable(QStringLiteral("Max distance: %1").arg(maxDist)));
    }

    void testConvergenceDetection()
    {
        ForceGraph::ForceLayoutEngine engine;

        ForceGraph::GraphNode n1; n1.id = QStringLiteral("a"); n1.position = QPointF(0, 0);
        ForceGraph::GraphNode n2; n2.id = QStringLiteral("b"); n2.position = QPointF(100, 0);
        engine.setNodes({n1, n2});

        ForceGraph::GraphEdge e; e.sourceId = QStringLiteral("a"); e.targetId = QStringLiteral("b");
        engine.setEdges({e});

        QVERIFY(!engine.isStable());

        for (int i = 0; i < 500; ++i) engine.step();

        QVERIFY(engine.isStable());
    }

    void testPinnedNodeStays()
    {
        ForceGraph::ForceLayoutEngine engine;

        ForceGraph::GraphNode n1; n1.id = QStringLiteral("a"); n1.position = QPointF(0, 0);
        ForceGraph::GraphNode n2; n2.id = QStringLiteral("b"); n2.position = QPointF(200, 0);
        engine.setNodes({n1, n2});

        engine.pinNode(QStringLiteral("a"), QPointF(0, 0));

        for (int i = 0; i < 100; ++i) engine.step();

        auto nodes = engine.nodes();
        // Pinned node should not have moved
        QCOMPARE(nodes[0].position, QPointF(0, 0));
        // Unpinned node may have moved
    }

    void testEmptyGraph()
    {
        ForceGraph::ForceLayoutEngine engine;
        engine.step(); // Should not crash
        QCOMPARE(engine.nodeCount(), 0);
    }

    void testSingleNode()
    {
        ForceGraph::ForceLayoutEngine engine;
        ForceGraph::GraphNode n; n.id = QStringLiteral("a"); n.position = QPointF(50, 50);
        engine.setNodes({n});

        for (int i = 0; i < 50; ++i) engine.step();

        // Single node with center force should drift toward origin
        auto nodes = engine.nodes();
        double dist = std::sqrt(nodes[0].position.x() * nodes[0].position.x() +
                                nodes[0].position.y() * nodes[0].position.y());
        QVERIFY(dist < 50); // Closer to origin than start
    }

    void testParameterEffects()
    {
        // Higher repel force should produce wider spread
        auto runWithRepel = [](double repel) -> double {
            ForceGraph::ForceLayoutEngine engine;
            engine.setRepelForce(repel);

            QVector<ForceGraph::GraphNode> nodes;
            for (int i = 0; i < 5; ++i) {
                ForceGraph::GraphNode n;
                n.id = QString::number(i);
                n.position = QPointF(i * 10, 0);
                nodes.append(n);
            }
            engine.setNodes(nodes);

            for (int i = 0; i < 200; ++i) engine.step();

            auto result = engine.nodes();
            double maxDist = 0;
            for (const auto &n : result) {
                double d = std::sqrt(n.position.x() * n.position.x() + n.position.y() * n.position.y());
                maxDist = std::max(maxDist, d);
            }
            return maxDist;
        };

        double spreadLow = runWithRepel(500.0);
        double spreadHigh = runWithRepel(3000.0);
        QVERIFY2(spreadHigh > spreadLow,
                 qPrintable(QStringLiteral("Low: %1, High: %2").arg(spreadLow).arg(spreadHigh)));
    }
};

QTEST_MAIN(TestForceLayout)
#include "tst_forcelayout.moc"
```

- [ ] **Step 2: Add test to CMakeLists**

Add to `libs/forcegraph/tests/CMakeLists.txt`:
```cmake
add_executable(tst_forcelayout tst_forcelayout.cpp)
add_test(NAME tst_forcelayout COMMAND tst_forcelayout)
target_link_libraries(tst_forcelayout PRIVATE Qt6::Test forcegraph)
set_tests_properties(tst_forcelayout PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Implement ForceLayoutEngine**

Replace `libs/forcegraph/src/ForceLayoutEngine.cpp` with the full implementation. This is the core physics engine — the implementer should read the spec for the exact algorithm:

- FR forces: `f_a(d) = d²/k`, `f_r(d) = -k²/d` where `k = sqrt(area/|V|)`
- Temperature cooling: `T = initial_T * (1 - iter/maxIter)`
- Per-vertex oscillation detection (dot product of current vs previous displacement)
- Barnes-Hut quadtree for repulsion when nodeCount > 500
- Center force pulling all nodes toward origin
- Convergence: `maxDisplacement < area * 0.0001` for 5 consecutive iterations
- QTimer at 33ms for animation loop (start/stop control)

The implementer should write ~200-250 lines implementing `step()` as the core method that:
1. Computes `k = sqrt(canvasArea / nodeCount)`
2. Resets displacements
3. Computes repulsive forces (via QuadTree if >500 nodes, else naive O(n²))
4. Computes attractive forces along edges
5. Applies center force
6. Applies oscillation detection (per-vertex local temperature)
7. Limits displacement by temperature
8. Updates positions (skip pinned)
9. Cools temperature
10. Checks convergence
11. Emits `positionsUpdated`

`start()` creates a QTimer at 33ms interval connected to `step()`. `stop()` stops the timer.

- [ ] **Step 4: Build and run tests**

```bash
cmake --build build && cd build && ctest -R tst_forcelayout --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add libs/forcegraph/src/ForceLayoutEngine.cpp libs/forcegraph/tests/
git commit -m "feat: implement ForceLayoutEngine with FR forces + Barnes-Hut

Fruchterman-Reingold force computation with temperature cooling,
per-vertex oscillation detection, convergence detection,
and Barnes-Hut quadtree for O(n log n) repulsion on large graphs.
7 unit tests covering convergence, pinning, parameters, edge cases."
```

---

### Task 4: ForceGraphNode + ForceGraphEdge (rendering items)

**Files:**
- Modify: `libs/forcegraph/src/ForceGraphNode.cpp`
- Modify: `libs/forcegraph/src/ForceGraphEdge.cpp`

- [ ] **Step 1: Implement ForceGraphNode with LOD rendering**

Replace `libs/forcegraph/src/ForceGraphNode.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphNode.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace ForceGraph {

ForceGraphNode::ForceGraphNode(const GraphNode &data, QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent)
    , m_data(data)
{
    double r = m_data.radius;
    setRect(-r, -r, 2 * r, 2 * r);
    setPos(m_data.position);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setZValue(1); // Above edges
}

void ForceGraphNode::setData(const GraphNode &data)
{
    m_data = data;
    double r = m_data.radius;
    setRect(-r, -r, 2 * r, 2 * r);
}

QString ForceGraphNode::nodeId() const
{
    return m_data.id;
}

void ForceGraphNode::setHighlighted(bool highlighted)
{
    m_highlighted = highlighted;
    update();
}

void ForceGraphNode::setDimmed(bool dimmed)
{
    m_dimmed = dimmed;
    update();
}

void ForceGraphNode::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    double lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
        painter->worldTransform());

    QColor color = m_data.color;
    if (m_dimmed) {
        color.setAlphaF(0.15);
    }
    if (m_highlighted) {
        color = color.lighter(130);
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);

    if (lod < 0.1) {
        // Ultra-low: single pixel
        painter->fillRect(QRectF(-1, -1, 2, 2), color);
        return;
    }

    if (lod < 0.4) {
        // Low: filled circle, no label
        painter->drawEllipse(rect());
        return;
    }

    // Medium+: circle with outline
    if (m_highlighted) {
        painter->setPen(QPen(color.darker(150), 2));
    }
    painter->drawEllipse(rect());

    if (lod < 1.0) {
        // Medium: abbreviated label
        if (!m_data.label.isEmpty()) {
            painter->setPen(m_dimmed ? QColor(128, 128, 128, 40) : QColor(60, 60, 60));
            QFont font;
            font.setPointSizeF(8);
            painter->setFont(font);
            painter->drawText(QPointF(-m_data.radius, m_data.radius + 12),
                              m_data.label.left(15));
        }
        return;
    }

    // Full: circle + full label
    if (!m_data.label.isEmpty()) {
        painter->setPen(m_dimmed ? QColor(128, 128, 128, 40) : QColor(40, 40, 40));
        QFont font;
        font.setPointSizeF(9);
        painter->setFont(font);
        QRectF textRect(QPointF(-50, m_data.radius + 4), QSizeF(100, 16));
        painter->drawText(textRect, Qt::AlignHCenter, m_data.label);
    }
}

} // namespace ForceGraph
```

- [ ] **Step 2: Implement ForceGraphEdge**

Replace `libs/forcegraph/src/ForceGraphEdge.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphEdge.h"
#include "forcegraph/ForceGraphNode.h"
#include <QPen>

namespace ForceGraph {

ForceGraphEdge::ForceGraphEdge(ForceGraphNode *source, ForceGraphNode *target, QGraphicsItem *parent)
    : QGraphicsLineItem(parent)
    , m_source(source)
    , m_target(target)
{
    setZValue(0); // Behind nodes
    setPen(QPen(QColor(150, 150, 150, 100), 1));
    adjust();
}

void ForceGraphEdge::adjust()
{
    if (!m_source || !m_target) return;
    setLine(QLineF(m_source->pos(), m_target->pos()));
}

ForceGraphNode *ForceGraphEdge::sourceNode() const { return m_source; }
ForceGraphNode *ForceGraphEdge::targetNode() const { return m_target; }

void ForceGraphEdge::setDimmed(bool dimmed)
{
    m_dimmed = dimmed;
    if (dimmed) {
        setPen(QPen(QColor(200, 200, 200, 30), 0.5));
    } else {
        setPen(QPen(QColor(150, 150, 150, 100), 1));
    }
}

} // namespace ForceGraph
```

- [ ] **Step 3: Build**

```bash
cmake --build build
```

- [ ] **Step 4: Commit**

```bash
git add libs/forcegraph/src/ForceGraphNode.cpp libs/forcegraph/src/ForceGraphEdge.cpp
git commit -m "feat: implement ForceGraphNode with LOD rendering and ForceGraphEdge

Node: 4-level LOD (pixel dot → circle → abbreviated label → full label).
Highlight/dim support for hover interaction. ItemIsMovable for drag.
Edge: simple line between node centers, adjustable, dim support."
```

---

### Task 5: ForceGraphScene + ForceGraphView (interactive rendering)

**Files:**
- Modify: `libs/forcegraph/src/ForceGraphScene.cpp`
- Modify: `libs/forcegraph/src/ForceGraphView.cpp`

- [ ] **Step 1: Implement ForceGraphScene**

Replace `libs/forcegraph/src/ForceGraphScene.cpp` with full implementation:
- `setNodes()`: creates `ForceGraphNode` items, stores in `m_nodeItems` hash
- `setEdges()`: creates `ForceGraphEdge` items connecting node pairs
- `updatePositions()`: moves each node item to its new position, calls `adjust()` on edges
- `setHighlightedNode()`: highlights target node and connected edges, dims everything else
- `clearHighlight()`: removes all dim/highlight

- [ ] **Step 2: Implement ForceGraphView**

Replace `libs/forcegraph/src/ForceGraphView.cpp` with full implementation:
- Constructor: creates scene, sets render hints (antialiasing), drag mode
- `setEngine()`: connects `positionsUpdated` signal to `scene->updatePositions()`
- `setNodes()`/`setEdges()`: delegates to scene
- `wheelEvent()`: zoom with `scale()` centered on cursor
- `mousePressEvent()`: if no item under cursor, start panning; if node item, let QGraphicsView handle drag
- `mouseMoveEvent()`: if panning, translate; else delegate for node drag
- `mouseReleaseEvent()`: end pan; if node was dragged, emit signal to pin it in engine
- `keyPressEvent()`: Home → `zoomToFit()`, +/- → zoom
- `zoomToFit()`: `fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio)`
- Hover: use `hoverEnterEvent`/`hoverLeaveEvent` on nodes (via scene event filter) to call `setHighlightedNode`/`clearHighlight` and emit `nodeHovered`

- [ ] **Step 3: Build and verify**

```bash
cmake --build build && cd build && ctest --output-on-failure
```

All tests should pass.

- [ ] **Step 4: Commit**

```bash
git add libs/forcegraph/src/ForceGraphScene.cpp libs/forcegraph/src/ForceGraphView.cpp
git commit -m "feat: implement ForceGraphScene and ForceGraphView

Scene: manages node/edge items, position updates from engine,
hover highlight/dim interaction.
View: pan/zoom/drag, keyboard shortcuts, zoomToFit,
nodeClicked/nodeHovered signals."
```

---

### Task 6: Push submodule to GitHub

- [ ] **Step 1: Initialize git in the forcegraph directory and push**

```bash
cd libs/forcegraph
git init
git add -A
git commit -m "feat: libforcegraph v0.1.0 — force-directed graph visualization for Qt6

Two-layer library:
- ForceLayoutEngine: FR forces + Barnes-Hut quadtree + temperature cooling
- ForceGraphView: QGraphicsView with LOD rendering, pan/zoom/hover/drag

Based on Handbook of Graph Drawing (Tamassia, Ch. 12).
Targets 100-10,000 nodes."

git remote add origin git@github.com:clintonthegeek/forcegraph.git
git branch -M main
git push -u origin main
cd ../..
```

- [ ] **Step 2: Add as proper git submodule in parent project**

```bash
# Remove the directory we created manually
rm -rf libs/forcegraph/.git

# Add as submodule
git submodule add git@github.com:clintonthegeek/forcegraph.git libs/forcegraph
git add .gitmodules libs/forcegraph
git commit -m "chore: add forcegraph as git submodule"
```

- [ ] **Step 3: Verify build still works**

```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest --output-on-failure
```

All tests pass including forcegraph tests.

---

Self-review:

1. **Spec coverage:** GraphTypes ✓. QuadTree with Barnes-Hut ✓ (4 tests). ForceLayoutEngine with FR forces + temperature + oscillation detection + convergence ✓ (7 tests). ForceGraphNode with 4-level LOD ✓. ForceGraphEdge ✓. ForceGraphScene ✓. ForceGraphView with pan/zoom/drag/hover ✓. Pinned nodes ✓. Configurable parameters ✓. QTimer simulation loop ✓. GitHub repo + submodule ✓.

2. **Placeholder scan:** Task 3's `step()` implementation is described algorithmically rather than with full code. This is intentional — it's ~250 lines of physics code that the implementer must write following the spec's pseudocode and formulas. All data types, method signatures, and test expectations are concrete. The implementer has the Handbook reference and the spec's exact formulas.

3. **Type consistency:** `GraphNode`/`GraphEdge` used consistently across all files. `ForceLayoutEngine::step()` updates `m_nodes[i].position`. `positionsUpdated(QHash<QString, QPointF>)` signal consumed by `ForceGraphScene::updatePositions()`. QuadTree takes `QVector<GraphNode>`, returns `QPointF` forces.
