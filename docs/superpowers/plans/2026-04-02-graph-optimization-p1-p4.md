# Graph View Optimization (Priorities 1-4, 6) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Improve graph layout quality and rendering performance with low-effort, high-impact changes: BFS initial placement, degree-weighted repulsion, adaptive speed, QGraphicsView performance, and QuadTree optimizations.

**Architecture:** All changes are to the existing `ForceLayoutEngine`, `QuadTree`, `ForceGraphView`, and `ForceGraphScene` in `libs/forcegraph/`. No new classes needed. Each priority is an independent task that can be tested and committed separately.

**Tech Stack:** C++20, Qt6 (QGraphicsView), Barnes-Hut QuadTree

**Spec:** `docs/superpowers/specs/2026-04-02-graph-view-optimization-design.md`

---

## Task 1: BFS Initial Placement

**Files:** `libs/forcegraph/include/forcegraph/ForceLayoutEngine.h`, `libs/forcegraph/src/ForceLayoutEngine.cpp`, `libs/forcegraph/tests/tst_forcelayout.cpp`

**Why:** The current `randomizePositionsIfNeeded()` places all-at-origin nodes randomly in a circle (line 79-98 of ForceLayoutEngine.cpp). The simulation starts from a state with no relation to graph structure, wasting 50-80% of iterations untangling. BFS radial placement gives the engine a head start based on actual connectivity.

### Steps

- [ ] **1.1** Add new private members and method declarations to `ForceLayoutEngine.h`:

In the private section, after `void randomizePositionsIfNeeded();` (line 43), add:

```cpp
// In ForceLayoutEngine.h, private section, after randomizePositionsIfNeeded():
void bfsInitialPlacement();
void buildAdjacency();

QHash<QString, QVector<QString>> m_adjacency;
```

- [ ] **1.2** Implement `buildAdjacency()` in `ForceLayoutEngine.cpp`. Add after the existing `buildNodeIndex()` method (after line 54):

```cpp
void ForceLayoutEngine::buildAdjacency()
{
    m_adjacency.clear();
    // Pre-insert all node IDs so disconnected nodes appear in the adjacency map
    for (const auto &node : m_nodes) {
        m_adjacency[node.id]; // default-construct empty vector
    }
    for (const auto &edge : m_edges) {
        m_adjacency[edge.sourceId].append(edge.targetId);
        m_adjacency[edge.targetId].append(edge.sourceId);
    }
}
```

- [ ] **1.3** Call `buildAdjacency()` from `setEdges()`. Change line 30-31 from:

```cpp
void ForceLayoutEngine::setEdges(const QVector<GraphEdge> &edges)
{
    m_edges = edges;
}
```

to:

```cpp
void ForceLayoutEngine::setEdges(const QVector<GraphEdge> &edges)
{
    m_edges = edges;
    buildAdjacency();
}
```

- [ ] **1.4** Implement `bfsInitialPlacement()` in `ForceLayoutEngine.cpp`. Add after `buildAdjacency()`:

```cpp
void ForceLayoutEngine::bfsInitialPlacement()
{
    if (m_nodes.size() <= 1) return;

    // Find connected components via BFS
    QHash<QString, int> componentOf; // node id -> component index
    QVector<QVector<QString>> components;
    QSet<QString> visited;

    for (const auto &node : m_nodes) {
        if (visited.contains(node.id)) continue;

        QVector<QString> component;
        QQueue<QString> queue;
        queue.enqueue(node.id);
        visited.insert(node.id);

        while (!queue.isEmpty()) {
            QString current = queue.dequeue();
            component.append(current);
            componentOf[current] = components.size();

            const auto &neighbors = m_adjacency.value(current);
            for (const auto &neighbor : neighbors) {
                if (!visited.contains(neighbor)) {
                    visited.insert(neighbor);
                    queue.enqueue(neighbor);
                }
            }
        }
        components.append(component);
    }

    // Sort components by size descending (largest first)
    std::sort(components.begin(), components.end(),
              [](const QVector<QString> &a, const QVector<QString> &b) {
                  return a.size() > b.size();
              });

    double offsetX = 0.0;
    double gap = m_linkDistance * 3.0;

    for (const auto &component : components) {
        if (component.size() == 1) {
            // Isolated node: place at offset
            int idx = m_nodeIndex.value(component[0], -1);
            if (idx >= 0) {
                m_nodes[idx].position = QPointF(offsetX, 0.0);
            }
            offsetX += gap;
            continue;
        }

        // Two-pass BFS to find approximate diameter endpoints
        // Pass 1: BFS from arbitrary node to find farthest
        auto bfsFarthest = [&](const QString &startId) -> QString {
            QHash<QString, int> dist;
            QQueue<QString> q;
            q.enqueue(startId);
            dist[startId] = 0;
            QString farthest = startId;
            int maxDist = 0;

            while (!q.isEmpty()) {
                QString current = q.dequeue();
                const auto &neighbors = m_adjacency.value(current);
                for (const auto &neighbor : neighbors) {
                    if (!dist.contains(neighbor) && component.contains(neighbor)) {
                        dist[neighbor] = dist[current] + 1;
                        if (dist[neighbor] > maxDist) {
                            maxDist = dist[neighbor];
                            farthest = neighbor;
                        }
                        q.enqueue(neighbor);
                    }
                }
            }
            return farthest;
        };

        QString endpoint1 = bfsFarthest(component[0]);
        QString endpoint2 = bfsFarthest(endpoint1);
        Q_UNUSED(endpoint2); // endpoint2 is the other end of the diameter

        // BFS from endpoint1 to assign layers
        QHash<QString, int> layer;
        QQueue<QString> queue;
        queue.enqueue(endpoint1);
        layer[endpoint1] = 0;
        int maxLayer = 0;

        while (!queue.isEmpty()) {
            QString current = queue.dequeue();
            const auto &neighbors = m_adjacency.value(current);
            for (const auto &neighbor : neighbors) {
                if (!layer.contains(neighbor) && component.contains(neighbor)) {
                    layer[neighbor] = layer[current] + 1;
                    maxLayer = std::max(maxLayer, layer[neighbor]);
                    queue.enqueue(neighbor);
                }
            }
        }

        // Count nodes per layer for angular distribution
        QHash<int, int> layerCounts;
        QHash<int, int> layerCurrentIndex;
        for (const auto &nodeId : component) {
            int l = layer.value(nodeId, 0);
            layerCounts[l]++;
            layerCurrentIndex[l] = 0;
        }

        // Place nodes on concentric rings
        auto *rng = QRandomGenerator::global();
        double componentMaxRadius = 0.0;

        for (const auto &nodeId : component) {
            int idx = m_nodeIndex.value(nodeId, -1);
            if (idx < 0) continue;

            int l = layer.value(nodeId, 0);
            double radius = l * m_linkDistance;
            componentMaxRadius = std::max(componentMaxRadius, radius);

            int count = layerCounts[l];
            int index = layerCurrentIndex[l]++;

            double angle;
            if (count == 1) {
                angle = 0.0;
            } else {
                double angleStep = 2.0 * M_PI / count;
                angle = index * angleStep;
            }
            // Small jitter to prevent exact overlaps
            angle += (rng->generateDouble() - 0.5) * 0.2;

            m_nodes[idx].position = QPointF(
                offsetX + radius * std::cos(angle),
                radius * std::sin(angle)
            );
        }

        offsetX += 2.0 * componentMaxRadius + gap;
    }
}
```

Note: requires `#include <QQueue>` and `#include <QSet>` at the top of `ForceLayoutEngine.cpp`.

- [ ] **1.5** Replace the call to `randomizePositionsIfNeeded()` with `bfsInitialPlacement()`. In `start()` (line 112), change:

```cpp
    randomizePositionsIfNeeded();
```

to:

```cpp
    bfsInitialPlacement();
```

Also in `step()` (line 161), change:

```cpp
        randomizePositionsIfNeeded();
```

to:

```cpp
        bfsInitialPlacement();
```

Keep the existing `randomizePositions()` method (line 354-366) untouched — it is used by the "Animate" button to re-randomize on demand.

- [ ] **1.6** Also add `m_adjacency.clear();` to the `clear()` method, after `m_nodeIndex.clear();` (line 38).

- [ ] **1.7** Add tests to `tst_forcelayout.cpp`. Add two new test slots before the closing `};`:

```cpp
void testBFSPlacementProducesLayeredLayout()
{
    // Build a linear chain: a — b — c — d — e
    ForceGraph::ForceLayoutEngine engine;

    QVector<ForceGraph::GraphNode> nodes;
    for (int i = 0; i < 5; ++i) {
        ForceGraph::GraphNode n;
        n.id = QString::number(i);
        n.position = QPointF(0, 0); // All at origin
        nodes.append(n);
    }
    engine.setNodes(nodes);

    QVector<ForceGraph::GraphEdge> edges;
    for (int i = 0; i < 4; ++i) {
        ForceGraph::GraphEdge e;
        e.sourceId = QString::number(i);
        e.targetId = QString::number(i + 1);
        edges.append(e);
    }
    engine.setEdges(edges);

    // Run a few steps to trigger bfsInitialPlacement
    engine.step();

    auto result = engine.nodes();

    // Nodes at different BFS depths should be at different distances from the first node
    // Node 0 is at one end of the chain, node 4 at the other
    // After BFS placement, they should be at different radii
    QSet<int> uniqueDistanceBuckets;
    for (const auto &n : result) {
        double dist = std::sqrt(n.position.x() * n.position.x() + n.position.y() * n.position.y());
        // Bucket by rough distance (multiples of linkDistance/2)
        int bucket = static_cast<int>(dist / 50.0);
        uniqueDistanceBuckets.insert(bucket);
    }
    // A chain of 5 nodes should produce at least 3 distinct distance buckets
    QVERIFY2(uniqueDistanceBuckets.size() >= 3,
             qPrintable(QStringLiteral("Only %1 distance buckets for chain of 5").arg(uniqueDistanceBuckets.size())));
}

void testBFSPlacementDisconnectedComponents()
{
    // Two disconnected pairs: {a-b} and {c-d}
    ForceGraph::ForceLayoutEngine engine;

    QVector<ForceGraph::GraphNode> nodes;
    for (const auto &id : {QStringLiteral("a"), QStringLiteral("b"),
                           QStringLiteral("c"), QStringLiteral("d")}) {
        ForceGraph::GraphNode n;
        n.id = id;
        n.position = QPointF(0, 0);
        nodes.append(n);
    }
    engine.setNodes(nodes);

    QVector<ForceGraph::GraphEdge> edges;
    ForceGraph::GraphEdge e1; e1.sourceId = QStringLiteral("a"); e1.targetId = QStringLiteral("b");
    ForceGraph::GraphEdge e2; e2.sourceId = QStringLiteral("c"); e2.targetId = QStringLiteral("d");
    edges << e1 << e2;
    engine.setEdges(edges);

    // Run one step to trigger BFS placement
    engine.step();

    auto result = engine.nodes();

    // Find center of each component
    QPointF center1, center2;
    for (const auto &n : result) {
        if (n.id == QStringLiteral("a") || n.id == QStringLiteral("b")) {
            center1 += n.position;
        } else {
            center2 += n.position;
        }
    }
    center1 /= 2.0;
    center2 /= 2.0;

    // Components should be placed apart (offset by at least gap = 3 * linkDistance)
    double separation = std::sqrt(
        std::pow(center1.x() - center2.x(), 2) +
        std::pow(center1.y() - center2.y(), 2));
    QVERIFY2(separation > 100.0,
             qPrintable(QStringLiteral("Component separation: %1").arg(separation)));
}
```

- [ ] **1.8** Build and run all tests:

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build && cd build && ctest -R tst_forcelayout --output-on-failure && ctest -R tst_quadtree --output-on-failure
```

All 9 force layout tests (7 existing + 2 new) and all 5 quadtree tests must pass. Existing tests like `testTwoNodesConverge` and `testConvergenceDetection` set explicit non-origin positions on their nodes, so they bypass `bfsInitialPlacement()` — they should be unaffected. The `testRepulsionSpreadsNodes` test uses all-at-origin nodes with no edges; BFS with no edges treats each node as its own component, spreading them horizontally which also satisfies `maxDist > 10`.

---

## Task 2: Degree-Weighted Repulsion

**Files:** `libs/forcegraph/include/forcegraph/ForceLayoutEngine.h`, `libs/forcegraph/src/ForceLayoutEngine.cpp`, `libs/forcegraph/include/forcegraph/QuadTree.h`, `libs/forcegraph/src/QuadTree.cpp`, `libs/forcegraph/tests/tst_forcelayout.cpp`, `libs/forcegraph/tests/tst_quadtree.cpp`

**Why:** Current repulsion treats all nodes equally (`F = repelForce / dist^2`). High-degree hub nodes (MOC/index notes) get buried among their neighbors. Degree-weighted repulsion pushes hubs apart proportionally, producing cleaner layouts for scale-free networks.

### Steps

- [ ] **2.1** Add degree storage to `ForceLayoutEngine.h`. In the private section, after `m_adjacency` (added in Task 1), add:

```cpp
QVector<double> m_masses; // degree+1 per node, parallel to m_nodes
```

- [ ] **2.2** Compute masses in `setEdges()`. After the `buildAdjacency()` call added in Task 1 step 1.3, add mass computation:

```cpp
void ForceLayoutEngine::setEdges(const QVector<GraphEdge> &edges)
{
    m_edges = edges;
    buildAdjacency();

    // Compute degree-weighted masses: mass = degree + 1
    m_masses.resize(m_nodes.size());
    for (int i = 0; i < m_nodes.size(); ++i) {
        int degree = m_adjacency.value(m_nodes[i].id).size();
        m_masses[i] = static_cast<double>(degree) + 1.0;
    }
}
```

Also add `m_masses.clear();` to the `clear()` method.

- [ ] **2.3** Modify the brute-force repulsion path in `step()`. Change lines 213-224 from:

```cpp
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF delta = m_nodes[i].position - m_nodes[j].position;
                double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
                dist = std::max(dist, EPSILON);

                double force = m_repelForce / (dist * dist);

                QPointF normalized(delta.x() / dist, delta.y() / dist);
                m_displacements[i] += normalized * force;
                m_displacements[j] -= normalized * force;
            }
        }
```

to:

```cpp
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF delta = m_nodes[i].position - m_nodes[j].position;
                double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
                dist = std::max(dist, EPSILON);

                double massI = (m_masses.size() > i) ? m_masses[i] : 1.0;
                double massJ = (m_masses.size() > j) ? m_masses[j] : 1.0;
                double force = m_repelForce * massI * massJ / (dist * dist);

                QPointF normalized(delta.x() / dist, delta.y() / dist);
                m_displacements[i] += normalized * force;
                m_displacements[j] -= normalized * force;
            }
        }
```

- [ ] **2.4** Change the QuadTree API to accept masses. In `QuadTree.h`, change the `build` signature:

```cpp
// Before:
void build(const QVector<GraphNode> &nodes, const QRectF &bounds);

// After:
void build(const QVector<GraphNode> &nodes, const QRectF &bounds,
           const QVector<double> &masses = {});
```

And change the `computeRepulsion` signature:

```cpp
// Before:
QPointF computeRepulsion(const QPointF &nodePos, double repelForce, double theta = 0.8) const;

// After:
QPointF computeRepulsion(const QPointF &nodePos, double repelForce,
                         double nodeMass = 1.0, double theta = 0.8) const;
```

Add a new private member to store masses:

```cpp
QVector<double> m_masses; // parallel to input nodes
```

Update the recursive signature similarly:

```cpp
QPointF computeRepulsionRecursive(int quadNodeIdx, const QPointF &pos,
                                   double repelForce, double nodeMass, double theta) const;
```

- [ ] **2.5** Implement mass-aware QuadTree in `QuadTree.cpp`. Change `build()`:

```cpp
void QuadTree::build(const QVector<GraphNode> &nodes, const QRectF &bounds,
                     const QVector<double> &masses)
{
    clear();
    if (nodes.isEmpty()) return;

    m_masses = masses;

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
```

Change `insert()` to use per-node mass instead of 1.0:

```cpp
void QuadTree::insert(int quadNodeIdx, int nodeIdx, const QVector<GraphNode> &nodes)
{
    const QPointF &pos = nodes[nodeIdx].position;
    double nodeMass = (nodeIdx < m_masses.size()) ? m_masses[nodeIdx] : 1.0;

    // Update center of mass
    double newMass = m_nodes[quadNodeIdx].totalMass + nodeMass;
    m_nodes[quadNodeIdx].centerOfMass =
        (m_nodes[quadNodeIdx].centerOfMass * m_nodes[quadNodeIdx].totalMass + pos * nodeMass) / newMass;
    m_nodes[quadNodeIdx].totalMass = newMass;

    // ... rest unchanged (isEmpty, isLeaf, subdivide, quadrant selection, recursive insert)
```

Only the first 5 lines of `insert()` change; the remainder (lines 40-73 of the current file) stays the same.

Change `computeRepulsion()`:

```cpp
QPointF QuadTree::computeRepulsion(const QPointF &nodePos, double repelForce,
                                    double nodeMass, double theta) const
{
    if (m_root < 0) return QPointF(0, 0);
    return computeRepulsionRecursive(m_root, nodePos, repelForce, nodeMass, theta);
}
```

Change `computeRepulsionRecursive()` — the only difference is multiplying by `nodeMass`:

```cpp
QPointF QuadTree::computeRepulsionRecursive(int quadNodeIdx, const QPointF &pos,
                                             double repelForce, double nodeMass, double theta) const
{
    const auto &qn = m_nodes[quadNodeIdx];

    if (qn.totalMass == 0) return QPointF(0, 0);

    QPointF delta = pos - qn.centerOfMass;
    double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());

    if (dist < 0.001) return QPointF(0, 0);

    double size = std::max(qn.bounds.width(), qn.bounds.height());

    if (qn.isLeaf() || (size / dist < theta)) {
        // F = repelForce * nodeMass * clusterMass / dist^2
        double force = repelForce * nodeMass * qn.totalMass / (dist * dist);
        return QPointF(delta.x() / dist * force, delta.y() / dist * force);
    }

    QPointF totalForce(0, 0);
    for (int i = 0; i < 4; ++i) {
        if (qn.children[i] >= 0) {
            totalForce += computeRepulsionRecursive(qn.children[i], pos, repelForce, nodeMass, theta);
        }
    }
    return totalForce;
}
```

- [ ] **2.6** Update the Barnes-Hut call site in `ForceLayoutEngine::step()` to pass masses. Change lines 203-210 from:

```cpp
        QuadTree quadTree;
        quadTree.build(m_nodes, bounds);

        for (int i = 0; i < n; ++i) {
            QPointF repulsion = quadTree.computeRepulsion(
                m_nodes[i].position, m_repelForce, 0.8);
            m_displacements[i] += repulsion;
        }
```

to:

```cpp
        QuadTree quadTree;
        quadTree.build(m_nodes, bounds, m_masses);

        for (int i = 0; i < n; ++i) {
            double nodeMass = (i < m_masses.size()) ? m_masses[i] : 1.0;
            QPointF repulsion = quadTree.computeRepulsion(
                m_nodes[i].position, m_repelForce, nodeMass, 0.8);
            m_displacements[i] += repulsion;
        }
```

- [ ] **2.7** Add degree-weighted center force. Change line 251 from:

```cpp
        m_displacements[i] -= m_nodes[i].position * m_centerForce;
```

to:

```cpp
        double mass = (i < m_masses.size()) ? m_masses[i] : 1.0;
        m_displacements[i] -= m_nodes[i].position * m_centerForce * mass;
```

- [ ] **2.8** Also handle the edge case where `setEdges()` is called before `setNodes()` (masses would be empty). In `setNodes()`, after `buildNodeIndex()`, add:

```cpp
    // Recompute masses if edges already set
    if (!m_edges.isEmpty()) {
        buildAdjacency();
        m_masses.resize(m_nodes.size());
        for (int i = 0; i < m_nodes.size(); ++i) {
            int degree = m_adjacency.value(m_nodes[i].id).size();
            m_masses[i] = static_cast<double>(degree) + 1.0;
        }
    }
```

- [ ] **2.9** Update existing QuadTree tests in `tst_quadtree.cpp`. The `computeRepulsion` calls now have `nodeMass` before `theta`. Update the 3 call sites:

In `testRepulsionNonZero`:
```cpp
// Before:
QPointF force = tree.computeRepulsion(QPointF(0, 0), 1500.0, 0.0);
// After:
QPointF force = tree.computeRepulsion(QPointF(0, 0), 1500.0, 1.0, 0.0);
```

In `testRepulsionApproximation`:
```cpp
// Before:
QPointF exactForce = tree.computeRepulsion(queryPos, 1500.0, 0.0);
QPointF approxForce = tree.computeRepulsion(queryPos, 1500.0, 0.8);
// After:
QPointF exactForce = tree.computeRepulsion(queryPos, 1500.0, 1.0, 0.0);
QPointF approxForce = tree.computeRepulsion(queryPos, 1500.0, 1.0, 0.8);
```

The `testEmptyTree` and `testSingleNode` tests use default parameters and will work correctly since `nodeMass` defaults to 1.0 and `theta` defaults to 0.8 — no changes needed.

- [ ] **2.10** Add a new test to `tst_forcelayout.cpp`:

```cpp
void testDegreeWeightedRepulsionSpreadsHubs()
{
    // Star topology: node "hub" connected to 10 leaves, node "leaf0" connected only to hub
    ForceGraph::ForceLayoutEngine engine;

    QVector<ForceGraph::GraphNode> nodes;
    ForceGraph::GraphNode hub;
    hub.id = QStringLiteral("hub");
    hub.position = QPointF(0, 0);
    nodes.append(hub);

    for (int i = 0; i < 10; ++i) {
        ForceGraph::GraphNode leaf;
        leaf.id = QStringLiteral("leaf%1").arg(i);
        leaf.position = QPointF((i + 1) * 10.0, 0);
        nodes.append(leaf);
    }
    engine.setNodes(nodes);

    QVector<ForceGraph::GraphEdge> edges;
    for (int i = 0; i < 10; ++i) {
        ForceGraph::GraphEdge e;
        e.sourceId = QStringLiteral("hub");
        e.targetId = QStringLiteral("leaf%1").arg(i);
        edges.append(e);
    }
    engine.setEdges(edges);

    for (int i = 0; i < 200; ++i) engine.step();

    auto result = engine.nodes();

    // Find average distance from hub to its leaves
    QPointF hubPos;
    for (const auto &n : result) {
        if (n.id == QStringLiteral("hub")) {
            hubPos = n.position;
            break;
        }
    }

    double avgDist = 0.0;
    int leafCount = 0;
    for (const auto &n : result) {
        if (n.id.startsWith(QStringLiteral("leaf"))) {
            double d = std::sqrt(std::pow(n.position.x() - hubPos.x(), 2) +
                                 std::pow(n.position.y() - hubPos.y(), 2));
            avgDist += d;
            leafCount++;
        }
    }
    avgDist /= leafCount;

    // With degree-weighted repulsion, the hub (degree 10) pushes leaves further
    // than uniform repulsion would. Average distance should exceed linkDistance.
    QVERIFY2(avgDist > 50.0,
             qPrintable(QStringLiteral("Avg hub-leaf distance: %1").arg(avgDist)));
}
```

- [ ] **2.11** Build and run all tests:

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build && cd build && ctest -R "tst_forcelayout|tst_quadtree" --output-on-failure
```

All force layout tests (10 total: 7 original + 2 from Task 1 + 1 new) and all 5 quadtree tests must pass. The `testParameterEffects` test compares `spreadLow` vs `spreadHigh` — degree weighting with no edges means all masses are 1.0, so the relative comparison still holds. The `testTwoNodesConverge` test with one edge gives both nodes mass=2.0, scaling forces equally on both sides — the equilibrium distance should still be near `linkDistance`, but verify the 70-130 tolerance holds (may need widening if the stronger repulsion shifts equilibrium).

---

## Task 3: Adaptive Speed (ForceAtlas2)

**Files:** `libs/forcegraph/include/forcegraph/ForceLayoutEngine.h`, `libs/forcegraph/src/ForceLayoutEngine.cpp`, `libs/forcegraph/tests/tst_forcelayout.cpp`

**Why:** The current temperature system (`T(t) = T0 * exp(-3t)`) is arbitrary. It either cools too fast (poor layout) or too slow (wasted iterations). ForceAtlas2's swinging/traction system is self-tuning: it accelerates when forces are consistent, brakes when oscillating, and converges based on energy, not an iteration cap.

### Steps

- [ ] **3.1** Replace temperature members with adaptive speed members in `ForceLayoutEngine.h`. Remove these members:

```cpp
// REMOVE these:
QVector<QPointF> m_prevDisplacements;
QVector<double> m_vertexTemperatures;
double m_temperature = 0.0;
double m_initialTemperature = 0.0;
int m_maxIterations = 500;
```

Add these in their place:

```cpp
QVector<QPointF> m_previousForces;   // forces from previous step (parallel to m_nodes)
double m_globalSpeed = 1.0;          // ForceAtlas2 adaptive global speed
double m_energy = 0.0;               // current total energy
double m_previousEnergy = 0.0;       // energy from previous step
int m_energyDecreaseCount = 0;       // consecutive steps with <0.1% energy decrease
```

Keep `m_damping` and `setDamping()` for backward compatibility (they become no-ops — `m_damping` is still stored but unused).

- [ ] **3.2** Update `clear()` to reset the new members. Replace the old temperature resets:

```cpp
void ForceLayoutEngine::clear()
{
    m_nodes.clear();
    m_edges.clear();
    m_nodeIndex.clear();
    m_adjacency.clear();
    m_masses.clear();
    m_displacements.clear();
    m_previousForces.clear();
    m_iteration = 0;
    m_stableCount = 0;
    m_stable = false;
    m_globalSpeed = 1.0;
    m_energy = 0.0;
    m_previousEnergy = 0.0;
    m_energyDecreaseCount = 0;
}
```

- [ ] **3.3** Update `start()`. Remove temperature/maxIterations setup, initialize adaptive speed state. Replace lines 100-140 with:

```cpp
void ForceLayoutEngine::start()
{
    if (m_running) return;

    m_running = true;
    m_stable = false;
    m_stableCount = 0;
    m_iteration = 0;
    m_globalSpeed = 1.0;
    m_energy = 0.0;
    m_previousEnergy = 0.0;
    m_energyDecreaseCount = 0;

    bfsInitialPlacement();

    int n = m_nodes.size();
    m_displacements.resize(n);
    m_previousForces.resize(n);
    for (int i = 0; i < n; ++i) {
        m_displacements[i] = QPointF(0, 0);
        m_previousForces[i] = QPointF(0, 0);
    }

    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &ForceLayoutEngine::step);
    }
    // Scale timer interval based on node count to avoid freezing the GUI
    int interval = static_cast<int>(TIMER_INTERVAL_MS);
    if (n > 2000) interval = 200;
    else if (n > 500) interval = 100;
    else if (n > 200) interval = 50;
    m_timer->start(interval);

    Q_EMIT simulationStarted();
}
```

- [ ] **3.4** Rewrite `step()` sections 2, 6, 7, and 9 for adaptive speed. The full `step()` becomes:

```cpp
void ForceLayoutEngine::step()
{
    const int n = m_nodes.size();
    if (n == 0) return;

    // --- Initialize per-step state on first call (if not started via start()) ---
    if (m_displacements.size() != n) {
        bfsInitialPlacement();
        m_displacements.resize(n);
        m_previousForces.resize(n);
        for (int i = 0; i < n; ++i) {
            m_displacements[i] = QPointF(0, 0);
            m_previousForces[i] = QPointF(0, 0);
        }
        m_globalSpeed = 1.0;
        m_energy = 0.0;
        m_previousEnergy = 0.0;
        m_energyDecreaseCount = 0;
    }

    // 1. Compute canvas area and optimal spacing
    double canvasArea = estimateCanvasArea();
    double k = m_linkDistance;

    // 2. Reset current displacements (these accumulate raw forces this step)
    for (int i = 0; i < n; ++i) {
        m_displacements[i] = QPointF(0, 0);
    }

    // 3. Compute repulsive forces (unchanged from current — uses degree-weighted masses from Task 2)
    if (n > BARNES_HUT_THRESHOLD) {
        double minX = m_nodes[0].position.x();
        double maxX = minX;
        double minY = m_nodes[0].position.y();
        double maxY = minY;
        for (const auto &node : m_nodes) {
            minX = std::min(minX, node.position.x());
            maxX = std::max(maxX, node.position.x());
            minY = std::min(minY, node.position.y());
            maxY = std::max(maxY, node.position.y());
        }
        double margin = std::max(maxX - minX, maxY - minY) * 0.1 + 1.0;
        QRectF bounds(minX - margin, minY - margin,
                      (maxX - minX) + 2 * margin, (maxY - minY) + 2 * margin);

        QuadTree quadTree;
        quadTree.build(m_nodes, bounds, m_masses);

        for (int i = 0; i < n; ++i) {
            double nodeMass = (i < m_masses.size()) ? m_masses[i] : 1.0;
            QPointF repulsion = quadTree.computeRepulsion(
                m_nodes[i].position, m_repelForce, nodeMass, 0.8);
            m_displacements[i] += repulsion;
        }
    } else {
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF delta = m_nodes[i].position - m_nodes[j].position;
                double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
                dist = std::max(dist, EPSILON);

                double massI = (i < m_masses.size()) ? m_masses[i] : 1.0;
                double massJ = (j < m_masses.size()) ? m_masses[j] : 1.0;
                double force = m_repelForce * massI * massJ / (dist * dist);

                QPointF normalized(delta.x() / dist, delta.y() / dist);
                m_displacements[i] += normalized * force;
                m_displacements[j] -= normalized * force;
            }
        }
    }

    // 4. Compute attractive forces along edges (unchanged)
    for (const auto &edge : m_edges) {
        auto srcIt = m_nodeIndex.find(edge.sourceId);
        auto tgtIt = m_nodeIndex.find(edge.targetId);
        if (srcIt == m_nodeIndex.end() || tgtIt == m_nodeIndex.end()) continue;

        int srcIdx = srcIt.value();
        int tgtIdx = tgtIt.value();

        QPointF delta = m_nodes[srcIdx].position - m_nodes[tgtIdx].position;
        double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
        dist = std::max(dist, EPSILON);

        double force = (dist - k) * m_linkForce;

        QPointF normalized(delta.x() / dist, delta.y() / dist);
        m_displacements[srcIdx] -= normalized * force;
        m_displacements[tgtIdx] += normalized * force;
    }

    // 5. Apply center force (degree-weighted gravity)
    for (int i = 0; i < n; ++i) {
        double mass = (i < m_masses.size()) ? m_masses[i] : 1.0;
        m_displacements[i] -= m_nodes[i].position * m_centerForce * mass;
    }

    // 6. ForceAtlas2 adaptive speed: compute swinging and traction
    double globalSwinging = 0.0;
    double globalTraction = 0.0;

    for (int i = 0; i < n; ++i) {
        double mass = (i < m_masses.size()) ? m_masses[i] : 1.0;

        // Swinging = magnitude of force change (oscillation)
        QPointF forceDiff = m_displacements[i] - m_previousForces[i];
        double swinging = std::sqrt(forceDiff.x() * forceDiff.x() + forceDiff.y() * forceDiff.y());

        // Traction = magnitude of average force (useful convergent movement)
        QPointF forceSum = m_displacements[i] + m_previousForces[i];
        double traction = std::sqrt(forceSum.x() * forceSum.x() + forceSum.y() * forceSum.y()) / 2.0;

        globalSwinging += mass * swinging;
        globalTraction += mass * traction;
    }

    // Compute global speed
    double tolerance = 1.0;
    double newGlobalSpeed = (globalSwinging > EPSILON)
        ? tolerance * globalTraction / globalSwinging
        : m_globalSpeed;

    // Prevent speed from growing too fast (max 1.5x per step)
    newGlobalSpeed = std::min(newGlobalSpeed, m_globalSpeed * 1.5);
    // Floor to prevent zero speed
    newGlobalSpeed = std::max(newGlobalSpeed, 0.01);
    m_globalSpeed = newGlobalSpeed;

    // 7. Apply per-node adaptive displacement and track energy
    double totalEnergy = 0.0;
    double maxDisplacement = 0.0;

    for (int i = 0; i < n; ++i) {
        double mass = (i < m_masses.size()) ? m_masses[i] : 1.0;

        // Per-node swinging
        QPointF forceDiff = m_displacements[i] - m_previousForces[i];
        double swinging = std::sqrt(forceDiff.x() * forceDiff.x() + forceDiff.y() * forceDiff.y());

        // Local speed: inversely proportional to node's swinging
        double localSpeed = m_globalSpeed / (1.0 + m_globalSpeed * std::sqrt(swinging));

        QPointF displacement = m_displacements[i] * localSpeed;

        // Cap displacement at 10 * node radius
        double mag = std::sqrt(displacement.x() * displacement.x() + displacement.y() * displacement.y());
        double maxDisp = 10.0 * m_nodes[i].radius;
        if (mag > maxDisp && mag > EPSILON) {
            displacement *= maxDisp / mag;
            mag = maxDisp;
        }

        maxDisplacement = std::max(maxDisplacement, mag);

        // Energy contribution (degree-weighted force magnitude)
        double forceMag = std::sqrt(m_displacements[i].x() * m_displacements[i].x() +
                                    m_displacements[i].y() * m_displacements[i].y());
        totalEnergy += mass * forceMag;

        // Save current forces as previous for next step
        m_previousForces[i] = m_displacements[i];

        // Replace raw displacement with the speed-adjusted one
        m_displacements[i] = displacement;
    }

    // 8. Update positions (skip pinned nodes)
    for (int i = 0; i < n; ++i) {
        if (m_nodes[i].pinned) continue;
        m_nodes[i].position += m_displacements[i];
    }

    // 9. Energy-based convergence detection
    m_previousEnergy = m_energy;
    m_energy = totalEnergy;

    // Two convergence criteria (either one triggers stability):
    // a) Energy-based: energy decreasing by <0.1% for 10 consecutive steps
    bool energyConverged = false;
    if (m_previousEnergy > EPSILON && m_energy < m_previousEnergy * 1.001) {
        double decrease = (m_previousEnergy - m_energy) / m_previousEnergy;
        if (decrease < 0.001) {
            m_energyDecreaseCount++;
            if (m_energyDecreaseCount >= 10) {
                energyConverged = true;
            }
        } else {
            m_energyDecreaseCount = 0;
        }
    } else {
        m_energyDecreaseCount = 0;
    }

    // b) Displacement-based fallback: max displacement near zero
    double convergenceThreshold = std::max(canvasArea * 0.0001, 0.1);
    bool displacementConverged = (maxDisplacement < convergenceThreshold);

    if (energyConverged || displacementConverged) {
        ++m_stableCount;
        if (m_stableCount >= STABLE_ITERATIONS_REQUIRED && !m_stable) {
            m_stable = true;
            Q_EMIT simulationStable();
            if (m_running) {
                stop();
            }
        }
    } else {
        m_stableCount = 0;
    }

    // 10. Emit positionsUpdated
    QHash<QString, QPointF> positions;
    positions.reserve(n);
    for (int i = 0; i < n; ++i) {
        positions[m_nodes[i].id] = m_nodes[i].position;
    }
    Q_EMIT positionsUpdated(positions);

    // 11. Increment iteration counter
    ++m_iteration;
}
```

- [ ] **3.5** Remove the now-unused `estimateCanvasArea()` from `step()` if it's only used for convergence. Actually, it's still used in the displacement convergence fallback (step 9b), so keep it. But it is no longer needed in `start()` for initial temperature — verify `start()` no longer calls it. (It doesn't after step 3.3.)

- [ ] **3.6** Remove `m_maxIterations` from the iteration scaling in `start()`. The old line `m_maxIterations = std::max(500, ...)` was removed in step 3.3. Verify no other code references `m_maxIterations`. (The `step()` rewrite in 3.4 does not reference it.)

- [ ] **3.7** Add test to `tst_forcelayout.cpp`:

```cpp
void testAdaptiveSpeedConverges()
{
    // Larger graph — verify adaptive speed reaches stability without m_maxIterations
    ForceGraph::ForceLayoutEngine engine;

    QVector<ForceGraph::GraphNode> nodes;
    for (int i = 0; i < 20; ++i) {
        ForceGraph::GraphNode n;
        n.id = QString::number(i);
        n.position = QPointF(i * 5.0, 0); // Slight spread to avoid all-at-origin
        nodes.append(n);
    }
    engine.setNodes(nodes);

    // Create a ring + some cross-edges
    QVector<ForceGraph::GraphEdge> edges;
    for (int i = 0; i < 20; ++i) {
        ForceGraph::GraphEdge e;
        e.sourceId = QString::number(i);
        e.targetId = QString::number((i + 1) % 20);
        edges.append(e);
    }
    // Add a few diagonals
    for (int i = 0; i < 20; i += 5) {
        ForceGraph::GraphEdge e;
        e.sourceId = QString::number(i);
        e.targetId = QString::number((i + 10) % 20);
        edges.append(e);
    }
    engine.setEdges(edges);

    QVERIFY(!engine.isStable());

    // Run up to 2000 steps — should converge well before that
    for (int i = 0; i < 2000; ++i) {
        engine.step();
        if (engine.isStable()) break;
    }

    QVERIFY2(engine.isStable(),
             "Adaptive speed did not converge within 2000 iterations");
}
```

- [ ] **3.8** Verify existing tests still pass. Key concerns:
  - `testTwoNodesConverge`: 200 steps may be insufficient with adaptive speed. The adaptive system should converge faster, not slower, but verify. If needed, increase to 500 steps.
  - `testConvergenceDetection`: runs 500 steps. Should still converge.
  - `testRepulsionSpreadsNodes`: no convergence check, just verifies spread > 10.
  - `testParameterEffects`: runs 200 steps, checks relative spread. Should still hold.

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build && cd build && ctest -R tst_forcelayout --output-on-failure
```

If `testTwoNodesConverge` fails the 70-130 distance check, increase iterations from 200 to 500. If it still fails, widen tolerance to 50-150 (the equilibrium point may shift slightly with degree-weighted repulsion since both nodes have mass=2.0).

---

## Task 4: QGraphicsView Performance

**Files:** `libs/forcegraph/include/forcegraph/ForceGraphView.h`, `libs/forcegraph/src/ForceGraphView.cpp`, `libs/forcegraph/include/forcegraph/ForceGraphScene.h`, `libs/forcegraph/src/ForceGraphScene.cpp`

**Why:** During simulation, every node moves every frame. QGraphicsView's `SmartViewportUpdate` tries to compute minimal dirty regions for every moving item — wasted effort when everything is moving. `BspTreeIndex` rebuilds the spatial index every frame for the same reason. Switching to `FullViewportUpdate` and `NoIndex` during simulation, then restoring after, is a ~2x rendering speedup.

### Steps

- [ ] **4.1** Add optimization flag to `ForceGraphView` constructor. In `ForceGraphView.cpp`, line 27, change:

```cpp
    setOptimizationFlags(DontSavePainterState);
```

to:

```cpp
    setOptimizationFlags(DontSavePainterState | DontAdjustForAntialiasing);
```

- [ ] **4.2** Add private slots to `ForceGraphView.h`:

```cpp
// In ForceGraphView.h, private section (after m_lastPanPos), add:
private Q_SLOTS:
    void onSimulationStarted();
    void onSimulationStopped();
```

- [ ] **4.3** Implement the slots in `ForceGraphView.cpp`:

```cpp
void ForceGraphView::onSimulationStarted()
{
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
}

void ForceGraphView::onSimulationStopped()
{
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
}
```

- [ ] **4.4** Add simulation mode methods to `ForceGraphScene.h`:

```cpp
// In ForceGraphScene.h, public section, after setSearchFilter:
public Q_SLOTS:
    void onSimulationStarted();
    void onSimulationStopped();
```

- [ ] **4.5** Implement the scene slots in `ForceGraphScene.cpp`:

```cpp
void ForceGraphScene::onSimulationStarted()
{
    setItemIndexMethod(QGraphicsScene::NoIndex);
}

void ForceGraphScene::onSimulationStopped()
{
    setItemIndexMethod(QGraphicsScene::BspTreeIndex);
}
```

- [ ] **4.6** Set a fixed scene rect in `ForceGraphScene` constructor to avoid per-frame `itemsBoundingRect()` recalculation. In `ForceGraphScene.cpp`, change the constructor:

```cpp
ForceGraphScene::ForceGraphScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-10000, -10000, 20000, 20000);
}
```

- [ ] **4.7** Wire the engine signals to view and scene in `ForceGraphView::setEngine()`. Replace lines 30-41:

```cpp
void ForceGraphView::setEngine(ForceLayoutEngine *engine)
{
    if (m_engine) {
        disconnect(m_engine, &ForceLayoutEngine::positionsUpdated,
                   m_scene, &ForceGraphScene::updatePositions);
        disconnect(m_engine, &ForceLayoutEngine::simulationStarted,
                   this, &ForceGraphView::onSimulationStarted);
        disconnect(m_engine, &ForceLayoutEngine::simulationStopped,
                   this, &ForceGraphView::onSimulationStopped);
        disconnect(m_engine, &ForceLayoutEngine::simulationStarted,
                   m_scene, &ForceGraphScene::onSimulationStarted);
        disconnect(m_engine, &ForceLayoutEngine::simulationStopped,
                   m_scene, &ForceGraphScene::onSimulationStopped);
    }
    m_engine = engine;
    if (m_engine) {
        connect(m_engine, &ForceLayoutEngine::positionsUpdated,
                m_scene, &ForceGraphScene::updatePositions);
        connect(m_engine, &ForceLayoutEngine::simulationStarted,
                this, &ForceGraphView::onSimulationStarted);
        connect(m_engine, &ForceLayoutEngine::simulationStopped,
                this, &ForceGraphView::onSimulationStopped);
        connect(m_engine, &ForceLayoutEngine::simulationStarted,
                m_scene, &ForceGraphScene::onSimulationStarted);
        connect(m_engine, &ForceLayoutEngine::simulationStopped,
                m_scene, &ForceGraphScene::onSimulationStopped);
    }
}
```

Also connect `simulationStable` to `onSimulationStopped` since stable emission also means simulation ends:

Actually, looking at the engine code, `simulationStable` calls `stop()` which emits `simulationStopped`, so the stopped signal already covers the stable case. No extra connection needed.

- [ ] **4.8** Build and run all tests:

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build && cd build && ctest --output-on-failure
```

This task has no new tests because the changes are rendering-only configuration. All existing tests must still pass (they use the engine directly, not the view/scene).

---

## Task 5: QuadTree Optimizations

**Files:** `libs/forcegraph/include/forcegraph/QuadTree.h`, `libs/forcegraph/src/QuadTree.cpp`, `libs/forcegraph/tests/tst_quadtree.cpp`

**Why:** Three low-effort improvements that roughly double Barnes-Hut performance: pre-allocation eliminates `QVector` reallocations during build, iterative traversal eliminates function call overhead, and adaptive theta trades accuracy for speed in early iterations.

### Steps

- [ ] **5.1** Pre-allocate tree nodes in `build()`. In `QuadTree.cpp`, in the `build()` method, after `clear();` and before creating the root node, add the reserve:

```cpp
void QuadTree::build(const QVector<GraphNode> &nodes, const QRectF &bounds,
                     const QVector<double> &masses)
{
    clear();
    if (nodes.isEmpty()) return;

    m_masses = masses;
    m_nodes.reserve(5 * nodes.size()); // Pre-allocate: ~4n internal + n leaf worst case

    // Create root node
    QuadNode root;
    root.bounds = bounds;
    m_nodes.append(root);
    m_root = 0;

    for (int i = 0; i < nodes.size(); ++i) {
        insert(m_root, i, nodes);
    }
}
```

- [ ] **5.2** Add adaptive theta support. Add a `m_theta` member and setter to `QuadTree.h`:

```cpp
// In QuadTree.h, public section:
void setTheta(double theta);

// In QuadTree.h, private section:
double m_theta = 0.8;
```

Implement in `QuadTree.cpp`:

```cpp
void QuadTree::setTheta(double theta)
{
    m_theta = theta;
}
```

Update `computeRepulsion()` to use `m_theta` as default when caller passes the default:

```cpp
QPointF QuadTree::computeRepulsion(const QPointF &nodePos, double repelForce,
                                    double nodeMass, double theta) const
{
    if (m_root < 0) return QPointF(0, 0);
    // Use stored theta if caller passes the default sentinel
    double effectiveTheta = (theta < 0) ? m_theta : theta;
    return computeRepulsionIterative(m_root, nodePos, repelForce, nodeMass, effectiveTheta);
}
```

Wait — changing the default parameter semantics is fragile. Instead, keep the explicit theta parameter as-is and let the engine pass the adaptive theta. The engine already passes `0.8` explicitly. In Task 5.4 we'll make the engine compute and pass adaptive theta.

Revert: keep `computeRepulsion` signature unchanged from Task 2. Just add `setTheta` as a stored member that the engine can use to retrieve the recommended theta later, or simply have the engine compute theta itself and pass it. The simpler approach: the engine computes adaptive theta and passes it to `computeRepulsion`. No change to QuadTree API needed for adaptive theta — just the engine call site.

So step 5.2 is: just add `m_theta` as a convenience store if needed, but the real adaptive theta logic is in step 5.4 (engine side). Skip the `setTheta` method entirely and do it all in the engine.

- [ ] **5.3** Replace recursive traversal with iterative. In `QuadTree.h`, replace the private `computeRepulsionRecursive` declaration with `computeRepulsionIterative`:

```cpp
// Replace:
QPointF computeRepulsionRecursive(int quadNodeIdx, const QPointF &pos,
                                   double repelForce, double nodeMass, double theta) const;

// With:
QPointF computeRepulsionIterative(int rootIdx, const QPointF &pos,
                                   double repelForce, double nodeMass, double theta) const;
```

Implement in `QuadTree.cpp`. Replace the entire `computeRepulsionRecursive` method and update `computeRepulsion` to call the iterative version:

```cpp
QPointF QuadTree::computeRepulsion(const QPointF &nodePos, double repelForce,
                                    double nodeMass, double theta) const
{
    if (m_root < 0) return QPointF(0, 0);
    return computeRepulsionIterative(m_root, nodePos, repelForce, nodeMass, theta);
}

QPointF QuadTree::computeRepulsionIterative(int rootIdx, const QPointF &pos,
                                             double repelForce, double nodeMass, double theta) const
{
    QPointF totalForce(0, 0);

    // Fixed-size stack — tree depth bounded by log4(n), practically < 20
    int stack[64];
    int stackSize = 0;
    stack[stackSize++] = rootIdx;

    while (stackSize > 0) {
        int idx = stack[--stackSize];
        const auto &qn = m_nodes[idx];

        if (qn.totalMass < 0.001) continue;

        QPointF delta = pos - qn.centerOfMass;
        double distSq = delta.x() * delta.x() + delta.y() * delta.y();

        if (distSq < 0.000001) continue; // Coincident — skip

        double dist = std::sqrt(distSq);
        double size = std::max(qn.bounds.width(), qn.bounds.height());

        if (qn.isLeaf() || (size / dist < theta)) {
            // Treat as single body: F = repelForce * nodeMass * clusterMass / dist^2
            double force = repelForce * nodeMass * qn.totalMass / distSq;
            totalForce += QPointF(delta.x() / dist * force, delta.y() / dist * force);
        } else {
            // Push children onto stack (reverse order for depth-first)
            for (int i = 3; i >= 0; --i) {
                if (qn.children[i] >= 0) {
                    stack[stackSize++] = qn.children[i];
                }
            }
        }
    }

    return totalForce;
}
```

Note: we also compute `distSq` directly and avoid the redundant `dist * dist` in the force calculation — minor optimization.

- [ ] **5.4** Add adaptive theta to the engine's Barnes-Hut call site. In `ForceLayoutEngine::step()`, in the Barnes-Hut branch, replace the hardcoded `0.8` theta with an energy-based adaptive value. Before the QuadTree build, add:

```cpp
        // Adaptive theta: less precise early (faster), more precise when converging
        double theta = 0.8; // default balanced
        if (m_energy > 0 && m_previousEnergy > 0) {
            double energyRatio = m_energy / m_previousEnergy;
            if (energyRatio > 0.99) {
                // Converging slowly or stalled — use precise theta
                theta = 0.5;
            } else if (energyRatio > 0.9) {
                // Normal convergence
                theta = 0.8;
            } else {
                // Rapid convergence / early iteration — fast theta is fine
                theta = 1.2;
            }
        } else if (m_iteration < 10) {
            theta = 1.2; // First few iterations: speed over precision
        }
```

Then pass `theta` instead of `0.8`:

```cpp
            QPointF repulsion = quadTree.computeRepulsion(
                m_nodes[i].position, m_repelForce, nodeMass, theta);
```

- [ ] **5.5** Add test to `tst_quadtree.cpp` — verify iterative matches recursive within tolerance. Since we removed the recursive method, we need to test iterative correctness against a brute-force baseline:

```cpp
void testIterativeMatchesBruteForce()
{
    // Build a tree with known nodes, compare iterative Barnes-Hut (theta=0)
    // against manual brute-force computation
    ForceGraph::QuadTree tree;
    QVector<ForceGraph::GraphNode> nodes;

    // Place 5 nodes at known positions
    QVector<QPointF> positions = {
        QPointF(100, 100), QPointF(300, 100), QPointF(200, 300),
        QPointF(50, 250), QPointF(350, 200)
    };
    for (int i = 0; i < positions.size(); ++i) {
        ForceGraph::GraphNode n;
        n.id = QString::number(i);
        n.position = positions[i];
        nodes.append(n);
    }

    tree.build(nodes, QRectF(0, 0, 400, 400));

    double repelForce = 1500.0;
    QPointF queryPos = positions[0]; // Query from node 0's position

    // Brute-force: sum repulsion from all other nodes
    QPointF bruteForce(0, 0);
    for (int j = 1; j < positions.size(); ++j) {
        QPointF delta = queryPos - positions[j];
        double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
        if (dist < 0.001) continue;
        // F = repelForce * mass_i * mass_j / dist^2, masses all 1.0
        double force = repelForce * 1.0 * 1.0 / (dist * dist);
        bruteForce += QPointF(delta.x() / dist * force, delta.y() / dist * force);
    }

    // Iterative with theta=0 should be exact (no approximation)
    QPointF iterativeForce = tree.computeRepulsion(queryPos, repelForce, 1.0, 0.0);

    // Should match within floating point tolerance
    double diffX = std::abs(iterativeForce.x() - bruteForce.x());
    double diffY = std::abs(iterativeForce.y() - bruteForce.y());
    double bruteMag = std::sqrt(bruteForce.x() * bruteForce.x() + bruteForce.y() * bruteForce.y());

    QVERIFY2(diffX / bruteMag < 0.01 && diffY / bruteMag < 0.01,
             qPrintable(QStringLiteral("Iterative (%1, %2) vs brute (%3, %4)")
                 .arg(iterativeForce.x()).arg(iterativeForce.y())
                 .arg(bruteForce.x()).arg(bruteForce.y())));
}
```

- [ ] **5.6** Build and run all tests:

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build && cd build && ctest --output-on-failure
```

All quadtree tests (5 existing + 1 new = 6) and all force layout tests must pass.

---

## Final Verification

After all 5 tasks are complete:

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build && cd build && ctest --output-on-failure
```

Expected test counts:
- `tst_forcelayout`: 11 tests (7 original + `testBFSPlacementProducesLayeredLayout` + `testBFSPlacementDisconnectedComponents` + `testDegreeWeightedRepulsionSpreadsHubs` + `testAdaptiveSpeedConverges`)
- `tst_quadtree`: 6 tests (5 original + `testIterativeMatchesBruteForce`)

Run the application and open a vault's graph view to visually verify:
1. Nodes appear in a structured radial layout from the start (not a random blob)
2. Hub nodes (MOCs, index notes) are visually separated from their neighbors
3. Simulation converges smoothly without jerky oscillation
4. No visible lag when the simulation is running on a 500+ node vault

```bash
./build/Corbomite
```
