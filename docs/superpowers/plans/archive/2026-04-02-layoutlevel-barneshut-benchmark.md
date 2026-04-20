# layoutLevel Barnes-Hut Fix + Benchmark Harness

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `MultilevelLayout::layoutLevel` survive large node counts by switching from O(n²) brute-force to Barnes-Hut (O(n log n)) repulsion, and build an automated benchmark harness to measure graph layout performance across varying topologies and sizes.

**Architecture:** Add a position-based `build()` overload to the existing `QuadTree` class so `layoutLevel` can use Barnes-Hut without converting to `GraphNode` objects. Gate the switch at a threshold (500 nodes) — below that, brute-force remains. Build a benchmark binary that procedurally generates graphs across a topology × size matrix, times `layoutLevel` on each, and prints structured results.

**Tech Stack:** C++20, Qt6 Test, existing `QuadTree`, existing `MultilevelLayout`

---

### Task 1: Add position-based QuadTree::build overload

**Files:**
- Modify: `libs/forcegraph/include/forcegraph/QuadTree.h`
- Modify: `libs/forcegraph/src/QuadTree.cpp`
- Modify: `libs/forcegraph/tests/tst_quadtree.cpp`

- [ ] **Step 1: Write the failing test**

Add to `tst_quadtree.cpp` after the last test slot:

```cpp
void testPositionBasedBuild()
{
    // Two nodes — repulsion should push them apart
    QVector<QPointF> positions = { QPointF(0, 0), QPointF(10, 0) };
    QVector<double> masses = { 1.0, 1.0 };
    QRectF bounds(-100, -100, 200, 200);

    ForceGraph::QuadTree tree;
    tree.build(positions, bounds, masses);

    QPointF force = tree.computeRepulsion(positions[0], 1000.0, masses[0], 0.8);
    // Force on node 0 should point in -x direction (away from node 1)
    QVERIFY2(force.x() < 0,
             qPrintable(QStringLiteral("Force x: %1").arg(force.x())));

    // Verify it matches the GraphNode-based path
    ForceGraph::GraphNode n1; n1.id = QStringLiteral("a"); n1.position = QPointF(0, 0);
    ForceGraph::GraphNode n2; n2.id = QStringLiteral("b"); n2.position = QPointF(10, 0);
    ForceGraph::QuadTree tree2;
    tree2.build({n1, n2}, bounds, masses);
    QPointF force2 = tree2.computeRepulsion(positions[0], 1000.0, masses[0], 0.8);

    // Forces should be identical
    QVERIFY(std::abs(force.x() - force2.x()) < 0.001);
    QVERIFY(std::abs(force.y() - force2.y()) < 0.001);
}

void testPositionBasedApproximation()
{
    // 50 random nodes — Barnes-Hut should approximate brute-force within 20%
    auto *rng = QRandomGenerator::global();
    QVector<QPointF> positions;
    QVector<double> masses;
    positions.reserve(50);
    masses.reserve(50);
    for (int i = 0; i < 50; ++i) {
        positions.append(QPointF(rng->generateDouble() * 1000.0,
                                  rng->generateDouble() * 1000.0));
        masses.append(1.0);
    }

    QRectF bounds(-100, -100, 1200, 1200);
    ForceGraph::QuadTree tree;
    tree.build(positions, bounds, masses);

    // Brute-force repulsion for node 0
    QPointF bruteForce(0, 0);
    for (int j = 1; j < 50; ++j) {
        QPointF delta = positions[0] - positions[j];
        double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
        dist = std::max(dist, 0.001);
        double f = 1000.0 / (dist * dist);
        bruteForce += (delta / dist) * f;
    }

    QPointF bhForce = tree.computeRepulsion(positions[0], 1000.0, 1.0, 0.8);

    double bruteLen = std::sqrt(bruteForce.x() * bruteForce.x() + bruteForce.y() * bruteForce.y());
    double bhLen = std::sqrt(bhForce.x() * bhForce.x() + bhForce.y() * bhForce.y());

    QVERIFY2(std::abs(bhLen - bruteLen) / bruteLen < 0.2,
             qPrintable(QStringLiteral("Brute: %1, BH: %2").arg(bruteLen).arg(bhLen)));
}
```

Also add the slot declarations in the `private Q_SLOTS:` section:
```cpp
void testPositionBasedBuild();
void testPositionBasedApproximation();
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target tst_quadtree && ctest -R tst_quadtree --output-on-failure`
Expected: Build failure — `build()` overload doesn't exist yet.

- [ ] **Step 3: Add the overload declaration to QuadTree.h**

Add after the existing `build()` declaration (line 11-12 of `QuadTree.h`):

```cpp
void build(const QVector<QPointF> &positions, const QRectF &bounds,
           const QVector<double> &masses);
```

Add a matching private `insert` overload after the existing one (line 29):

```cpp
void insert(int quadNodeIdx, int nodeIdx, const QVector<QPointF> &positions);
```

- [ ] **Step 4: Implement the overload in QuadTree.cpp**

Add after the existing `build()` method (after line 37):

```cpp
void QuadTree::build(const QVector<QPointF> &positions, const QRectF &bounds,
                      const QVector<double> &masses)
{
    clear();
    if (positions.isEmpty()) return;

    m_masses = masses;
    if (m_masses.isEmpty()) {
        m_masses.fill(1.0, positions.size());
    }

    m_nodes.reserve(5 * positions.size());

    QuadNode root;
    root.bounds = bounds;
    m_nodes.append(root);
    m_root = 0;

    for (int i = 0; i < positions.size(); ++i) {
        insert(m_root, i, positions);
    }
}
```

Add the matching `insert` overload after the existing `insert()` method (after line 83):

```cpp
void QuadTree::insert(int quadNodeIdx, int nodeIdx, const QVector<QPointF> &positions)
{
    const QPointF &pos = positions[nodeIdx];

    double nodeMass = m_masses.value(nodeIdx, 1.0);
    double newMass = m_nodes[quadNodeIdx].totalMass + nodeMass;
    m_nodes[quadNodeIdx].centerOfMass =
        (m_nodes[quadNodeIdx].centerOfMass * m_nodes[quadNodeIdx].totalMass + pos) / newMass;
    m_nodes[quadNodeIdx].totalMass = newMass;

    if (m_nodes[quadNodeIdx].isEmpty()) {
        m_nodes[quadNodeIdx].nodeIndex = nodeIdx;
        return;
    }

    if (m_nodes[quadNodeIdx].isLeaf()) {
        int existingIdx = m_nodes[quadNodeIdx].nodeIndex;
        m_nodes[quadNodeIdx].nodeIndex = -1;
        subdivide(quadNodeIdx);
        insert(quadNodeIdx, existingIdx, positions);
    }

    if (m_nodes[quadNodeIdx].children[0] < 0) {
        subdivide(quadNodeIdx);
    }

    double midX = m_nodes[quadNodeIdx].bounds.center().x();
    double midY = m_nodes[quadNodeIdx].bounds.center().y();

    int childIdx;
    if (pos.x() <= midX) {
        childIdx = (pos.y() <= midY) ? 0 : 2;
    } else {
        childIdx = (pos.y() <= midY) ? 1 : 3;
    }

    insert(m_nodes[quadNodeIdx].children[childIdx], nodeIdx, positions);
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cd build && cmake --build . --target tst_quadtree && ctest -R tst_quadtree --output-on-failure`
Expected: All tests PASS including `testPositionBasedBuild` and `testPositionBasedApproximation`.

- [ ] **Step 6: Commit**

```bash
git add libs/forcegraph/include/forcegraph/QuadTree.h \
        libs/forcegraph/src/QuadTree.cpp \
        libs/forcegraph/tests/tst_quadtree.cpp
git commit -m "feat(forcegraph): add position-based QuadTree::build overload

Enables Barnes-Hut approximation without requiring GraphNode objects,
so layoutLevel can use it directly with its integer-indexed positions."
```

---

### Task 2: Integrate Barnes-Hut into layoutLevel

**Files:**
- Modify: `libs/forcegraph/src/MultilevelLayout.cpp:1-2` (add include)
- Modify: `libs/forcegraph/src/MultilevelLayout.cpp:198-215` (repulsion block)
- Modify: `libs/forcegraph/tests/tst_forcelayout.cpp`

- [ ] **Step 1: Write the failing test**

Add to `tst_forcelayout.cpp` after `testMultilevelSkipsSmallGraphs`:

```cpp
void testMultilevelLayoutSurvivesLargeGraph()
{
    // 2000-node scale-free graph — must complete in <30 seconds
    // (was infinite with O(n²) brute-force)
    QVector<ForceGraph::GraphNode> nodes;
    nodes.reserve(2000);
    for (int i = 0; i < 2000; ++i) {
        ForceGraph::GraphNode n;
        n.id = QString::number(i);
        n.label = QStringLiteral("Node %1").arg(i);
        nodes.append(n);
    }

    // Preferential attachment — each new node connects to 1-2 existing nodes
    QVector<ForceGraph::GraphEdge> edges;
    auto *rng = QRandomGenerator::global();
    QVector<int> targets; // Degree-weighted target pool
    targets.append(0);
    for (int i = 1; i < 2000; ++i) {
        int target = targets[rng->bounded(targets.size())];
        edges.append({QString::number(i), QString::number(target)});
        targets.append(i);
        targets.append(target);

        // 30% chance of a second edge
        if (rng->generateDouble() < 0.3 && targets.size() > 1) {
            int target2 = targets[rng->bounded(targets.size())];
            if (target2 != i) {
                edges.append({QString::number(i), QString::number(target2)});
                targets.append(i);
                targets.append(target2);
            }
        }
    }

    ForceGraph::MultilevelConfig config;
    config.minCoarseNodes = 50;

    QElapsedTimer timer;
    timer.start();
    auto result = ForceGraph::MultilevelLayout::computeLayout(nodes, edges, config);
    qint64 elapsed = timer.elapsed();

    qDebug("testMultilevelLayoutSurvivesLargeGraph: %lld ms for %d nodes, %lld edges",
           elapsed, 2000, static_cast<long long>(edges.size()));

    QCOMPARE(result.size(), 2000);

    // Must complete in under 30 seconds (was effectively infinite before)
    QVERIFY2(elapsed < 30000,
             qPrintable(QStringLiteral("Took %1 ms — too slow").arg(elapsed)));

    // Nodes should be spread out
    int nonOrigin = 0;
    for (const auto &n : result) {
        if (std::abs(n.position.x()) > 0.1 || std::abs(n.position.y()) > 0.1)
            ++nonOrigin;
    }
    QVERIFY2(nonOrigin > 1800,
             qPrintable(QStringLiteral("Only %1 nodes moved from origin").arg(nonOrigin)));
}
```

Add the slot declaration:
```cpp
void testMultilevelLayoutSurvivesLargeGraph();
```

Also add required includes at top of tst_forcelayout.cpp:
```cpp
#include <QElapsedTimer>
#include <QRandomGenerator>
```

- [ ] **Step 2: Run test to verify it's too slow**

Run: `cd build && cmake --build . --target tst_forcelayout && timeout 60 ctest -R tst_forcelayout --output-on-failure -T Test`
Expected: Test either times out or fails the 30-second assertion. This confirms the O(n²) problem exists in the test.

- [ ] **Step 3: Add QuadTree include to MultilevelLayout.cpp**

At line 2 of `MultilevelLayout.cpp`, add:
```cpp
#include "forcegraph/QuadTree.h"
```

- [ ] **Step 4: Replace the brute-force repulsion block with Barnes-Hut gated path**

Replace the repulsion block in `layoutLevel` (the `// Repulsive forces: O(n²) brute-force` comment through the closing brace at line 215) with:

```cpp
        // Repulsive forces
        static constexpr int BARNES_HUT_THRESHOLD = 500;
        if (n > BARNES_HUT_THRESHOLD) {
            // Barnes-Hut O(n log n) for large levels
            double minX = level.positions[0].x(), maxX = minX;
            double minY = level.positions[0].y(), maxY = minY;
            for (int i = 1; i < n; ++i) {
                minX = std::min(minX, level.positions[i].x());
                maxX = std::max(maxX, level.positions[i].x());
                minY = std::min(minY, level.positions[i].y());
                maxY = std::max(maxY, level.positions[i].y());
            }
            double margin = std::max(maxX - minX, maxY - minY) * 0.1 + 1.0;
            QRectF bounds(minX - margin, minY - margin,
                          (maxX - minX) + 2 * margin, (maxY - minY) + 2 * margin);

            QuadTree qt;
            qt.build(level.positions, bounds, level.nodeWeight);

            for (int i = 0; i < n; ++i) {
                forces[i] += qt.computeRepulsion(
                    level.positions[i], config.repelForce,
                    level.nodeWeight[i], 0.8);
            }
        } else {
            // Brute-force O(n²) for small levels
            for (int i = 0; i < n; ++i) {
                double wi = level.nodeWeight[i];
                for (int j = i + 1; j < n; ++j) {
                    double dx = level.positions[i].x() - level.positions[j].x();
                    double dy = level.positions[i].y() - level.positions[j].y();
                    double dist = std::sqrt(dx * dx + dy * dy);
                    dist = std::max(dist, EPSILON);

                    double wj = level.nodeWeight[j];
                    double force = config.repelForce * wi * wj / (dist * dist);
                    double fx = (dx / dist) * force;
                    double fy = (dy / dist) * force;

                    forces[i] += QPointF(fx, fy);
                    forces[j] -= QPointF(fx, fy);
                }
            }
        }
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cd build && cmake --build . --target tst_forcelayout && ctest -R tst_forcelayout --output-on-failure`
Expected: All tests PASS. The new large-graph test should complete in seconds, not hang.

- [ ] **Step 6: Also run the QuadTree tests to confirm no regression**

Run: `cd build && ctest -R "tst_quadtree|tst_forcelayout" --output-on-failure`
Expected: All tests PASS.

- [ ] **Step 7: Commit**

```bash
git add libs/forcegraph/src/MultilevelLayout.cpp \
        libs/forcegraph/tests/tst_forcelayout.cpp
git commit -m "fix(forcegraph): use Barnes-Hut in layoutLevel for large graphs

layoutLevel was O(n²) brute-force at all levels, causing multi-hour
freezes when coarsening produced large levels (e.g., 7233 nodes).
Now switches to QuadTree Barnes-Hut approximation above 500 nodes,
bringing each iteration from O(n²) to O(n log n)."
```

---

### Task 3: Build the benchmark harness

**Files:**
- Create: `libs/forcegraph/tests/tst_benchmark_layout.cpp`
- Modify: `libs/forcegraph/tests/CMakeLists.txt`

- [ ] **Step 1: Create the benchmark binary**

Create `libs/forcegraph/tests/tst_benchmark_layout.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Benchmark harness for MultilevelLayout::layoutLevel.
// Generates synthetic graphs across a topology × size matrix,
// times layoutLevel on each, and prints structured results.
//
// Run: ./tst_benchmark_layout
// Or:  ctest -R tst_benchmark_layout --output-on-failure

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QTextStream>
#include <cmath>

#include "forcegraph/MultilevelLayout.h"

using namespace ForceGraph;

// ---------------------------------------------------------------------------
// Graph generators — return Level structs ready for layoutLevel
// ---------------------------------------------------------------------------

static MultilevelLayout::Level makeLevel(int nodeCount,
                                          const QVector<QPair<int,int>> &edgePairs)
{
    MultilevelLayout::Level level;
    level.nodeCount = nodeCount;
    level.nodeWeight.fill(1.0, nodeCount);
    level.positions.resize(nodeCount);

    // Random initial positions (circle layout)
    auto *rng = QRandomGenerator::global();
    double radius = std::sqrt(static_cast<double>(nodeCount)) * 50.0;
    for (int i = 0; i < nodeCount; ++i) {
        double angle = rng->generateDouble() * 2.0 * M_PI;
        double r = rng->generateDouble() * radius;
        level.positions[i] = QPointF(r * std::cos(angle), r * std::sin(angle));
    }

    level.edgeSrc.reserve(edgePairs.size());
    level.edgeTgt.reserve(edgePairs.size());
    level.edgeWeight.reserve(edgePairs.size());
    for (const auto &[s, t] : edgePairs) {
        level.edgeSrc.append(s);
        level.edgeTgt.append(t);
        level.edgeWeight.append(1.0);
    }

    return level;
}

// Scale-free (Barabási–Albert preferential attachment)
static MultilevelLayout::Level generateScaleFree(int n, int edgesPerNode = 2)
{
    QVector<QPair<int,int>> edges;
    auto *rng = QRandomGenerator::global();
    QVector<int> targets;
    targets.reserve(n * edgesPerNode * 2);
    targets.append(0);

    for (int i = 1; i < n; ++i) {
        for (int e = 0; e < edgesPerNode && !targets.isEmpty(); ++e) {
            int target = targets[rng->bounded(targets.size())];
            if (target != i) {
                edges.append({i, target});
                targets.append(i);
                targets.append(target);
            }
        }
    }
    return makeLevel(n, edges);
}

// Random (Erdős–Rényi)
static MultilevelLayout::Level generateRandom(int n, double edgeProbability)
{
    QVector<QPair<int,int>> edges;
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (rng->generateDouble() < edgeProbability) {
                edges.append({i, j});
            }
        }
    }
    return makeLevel(n, edges);
}

// Grid/lattice
static MultilevelLayout::Level generateGrid(int n)
{
    int cols = static_cast<int>(std::ceil(std::sqrt(n)));
    QVector<QPair<int,int>> edges;
    for (int i = 0; i < n; ++i) {
        int row = i / cols;
        int col = i % cols;
        if (col + 1 < cols && i + 1 < n)
            edges.append({i, i + 1});
        if (row + 1 < n / cols && i + cols < n)
            edges.append({i, i + cols});
    }
    return makeLevel(n, edges);
}

// Star (one mega-hub)
static MultilevelLayout::Level generateStar(int n)
{
    QVector<QPair<int,int>> edges;
    for (int i = 1; i < n; ++i) {
        edges.append({0, i});
    }
    return makeLevel(n, edges);
}

// Disconnected components (k clusters of n/k nodes each, chain within each)
static MultilevelLayout::Level generateDisconnected(int n, int clusters = 10)
{
    QVector<QPair<int,int>> edges;
    int clusterSize = n / clusters;
    for (int c = 0; c < clusters; ++c) {
        int start = c * clusterSize;
        int end = (c == clusters - 1) ? n : start + clusterSize;
        for (int i = start; i < end - 1; ++i) {
            edges.append({i, i + 1});
        }
    }
    return makeLevel(n, edges);
}

// Dense clique (complete graph on min(n, 500) then chain the rest)
static MultilevelLayout::Level generateDenseClique(int n)
{
    QVector<QPair<int,int>> edges;
    int cliqueSize = std::min(n, 500);
    for (int i = 0; i < cliqueSize; ++i) {
        for (int j = i + 1; j < cliqueSize; ++j) {
            edges.append({i, j});
        }
    }
    // Chain remaining nodes off the clique
    for (int i = cliqueSize; i < n; ++i) {
        edges.append({i, i - 1});
    }
    return makeLevel(n, edges);
}

// ---------------------------------------------------------------------------
// Benchmark runner
// ---------------------------------------------------------------------------

struct BenchmarkResult {
    QString topology;
    int nodes;
    int edges;
    int iterations;
    qint64 elapsedMs;
};

static BenchmarkResult runBenchmark(const QString &name,
                                     MultilevelLayout::Level level,
                                     int iterations)
{
    QElapsedTimer timer;
    MultilevelConfig config;

    timer.start();
    MultilevelLayout::layoutLevel(level, config, iterations);
    qint64 elapsed = timer.elapsed();

    return { name, level.nodeCount, level.edgeSrc.size(), iterations, elapsed };
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTextStream out(stdout);
    out << "=== ForceGraph layoutLevel Benchmark ===" << Qt::endl;
    out << Qt::endl;
    out << QString("%1  %2  %3  %4  %5  %6")
               .arg("Topology", -20)
               .arg("Nodes", 7)
               .arg("Edges", 7)
               .arg("Iters", 6)
               .arg("Time(ms)", 10)
               .arg("ms/iter", 8)
        << Qt::endl;
    out << QString("-").repeated(62) << Qt::endl;

    struct TestCase {
        QString name;
        std::function<MultilevelLayout::Level(int)> generator;
    };

    QVector<TestCase> topologies = {
        { QStringLiteral("scale-free"),    [](int n) { return generateScaleFree(n); } },
        { QStringLiteral("random-sparse"), [](int n) { return generateRandom(n, 3.0 / n); } },
        { QStringLiteral("random-dense"),  [](int n) { return generateRandom(n, 10.0 / n); } },
        { QStringLiteral("grid"),          [](int n) { return generateGrid(n); } },
        { QStringLiteral("star"),          [](int n) { return generateStar(n); } },
        { QStringLiteral("disconnected"),  [](int n) { return generateDisconnected(n); } },
        { QStringLiteral("dense-clique"),  [](int n) { return generateDenseClique(n); } },
    };

    QVector<int> sizes = { 100, 500, 1000, 2000, 5000 };

    QVector<BenchmarkResult> results;

    for (const auto &topo : topologies) {
        for (int n : sizes) {
            // Scale iterations: enough to see timing, not so many we wait forever
            int iters = std::max(10, static_cast<int>(std::sqrt(n) * 2));

            auto level = topo.generator(n);
            auto result = runBenchmark(topo.name, std::move(level), iters);
            results.append(result);

            double msPerIter = (result.iterations > 0)
                ? static_cast<double>(result.elapsedMs) / result.iterations
                : 0.0;

            out << QString("%1  %2  %3  %4  %5  %6")
                       .arg(result.topology, -20)
                       .arg(result.nodes, 7)
                       .arg(result.edges, 7)
                       .arg(result.iterations, 6)
                       .arg(result.elapsedMs, 10)
                       .arg(msPerIter, 8, 'f', 1)
                << Qt::endl;
            out.flush();
        }
        out << Qt::endl;
    }

    // Summary: flag any topology × size that exceeds 100ms/iter
    out << Qt::endl << "=== Slow cases (>100 ms/iter) ===" << Qt::endl;
    bool anySlow = false;
    for (const auto &r : results) {
        double msPerIter = (r.iterations > 0)
            ? static_cast<double>(r.elapsedMs) / r.iterations : 0.0;
        if (msPerIter > 100.0) {
            out << QString("  SLOW: %1 @ %2 nodes: %3 ms/iter")
                       .arg(r.topology, -20)
                       .arg(r.nodes, 7)
                       .arg(msPerIter, 0, 'f', 1)
                << Qt::endl;
            anySlow = true;
        }
    }
    if (!anySlow) {
        out << "  None — all under 100 ms/iter" << Qt::endl;
    }

    out << Qt::endl << "=== Benchmark complete ===" << Qt::endl;
    return 0;
}
```

- [ ] **Step 2: Expose layoutLevel and Level for testing**

`layoutLevel` and `Level` are currently private to `MultilevelLayout`. The benchmark needs access. Add a public static method and make `Level` public in `libs/forcegraph/include/forcegraph/MultilevelLayout.h`.

Move the `Level` struct and `layoutLevel` declaration from the `private:` section to `public:`:

```cpp
class MultilevelLayout {
public:
    // Compute good initial positions via coarsen -> layout -> uncoarsen pipeline.
    static QVector<GraphNode> computeLayout(
        const QVector<GraphNode> &nodes,
        const QVector<GraphEdge> &edges,
        const MultilevelConfig &config = MultilevelConfig{});

    struct Level {
        int nodeCount = 0;
        QVector<int> edgeSrc;
        QVector<int> edgeTgt;
        QVector<double> edgeWeight;
        QVector<double> nodeWeight;
        QVector<int> fineToCoarse;
        QVector<QPointF> positions;
    };

    // Exposed for benchmarking — runs force-directed layout on a single level.
    static void layoutLevel(Level &level, const MultilevelConfig &config, int iterations);

private:
    static Level buildLevel0(const QVector<GraphNode> &nodes, const QVector<GraphEdge> &edges);
    static Level coarsen(const Level &fine);
    static void interpolate(Level &fine, const Level &coarse);
};
```

- [ ] **Step 3: Register the benchmark in CMakeLists.txt**

Add to `libs/forcegraph/tests/CMakeLists.txt`:

```cmake
add_executable(tst_benchmark_layout tst_benchmark_layout.cpp)
add_test(NAME tst_benchmark_layout COMMAND tst_benchmark_layout)
target_link_libraries(tst_benchmark_layout PRIVATE Qt6::Core forcegraph)
set_tests_properties(tst_benchmark_layout PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 120
    LABELS "benchmark")
```

- [ ] **Step 4: Build and run the benchmark**

Run: `cd build && cmake --build . --target tst_benchmark_layout && ./libs/forcegraph/tests/tst_benchmark_layout`
Expected: Table of timing results. All entries should complete. No entry at 5000 nodes should exceed a few seconds total.

- [ ] **Step 5: Run all forcegraph tests to confirm no regression**

Run: `cd build && cmake -B . .. -DCORBOMITE_DEV_BUILD=ON && cmake --build . && ctest -R "tst_quadtree|tst_forcelayout|tst_benchmark" --output-on-failure`
Expected: All tests PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/forcegraph/tests/tst_benchmark_layout.cpp \
        libs/forcegraph/tests/CMakeLists.txt \
        libs/forcegraph/include/forcegraph/MultilevelLayout.h
git commit -m "feat(forcegraph): add layoutLevel benchmark harness

Procedurally generates graphs across 7 topologies × 5 sizes
(100 to 5000 nodes) and times layoutLevel on each. Provides
baseline measurements for graph layout performance tuning."
```

---

### Task 4: Remove timing instrumentation

**Files:**
- Modify: `libs/forcegraph/src/MultilevelLayout.cpp`
- Modify: `libs/forcegraph/src/ForceLayoutEngine.cpp`
- Modify: `libs/forcegraph/src/ForceGraphScene.cpp`
- Modify: `src/graph/GraphViewTab.cpp`

- [ ] **Step 1: Remove QElapsedTimer instrumentation from all four files**

Remove the `QElapsedTimer` includes, timer variables, and `qDebug` timing lines that were added for measurement. These were diagnostic — the benchmark harness now provides structured measurement.

In `MultilevelLayout.cpp`: remove the `QElapsedTimer` include, `totalTimer`, `phaseTimer` variables and all their `qDebug` lines. Keep the existing `qDebug` that logs level count and coarsest node count (that's structural info, not timing). Remove the `iterTimer` and its `qDebug` from `layoutLevel`.

In `ForceLayoutEngine.cpp`: remove the `QElapsedTimer` include, `stepTimer`, and the per-step `qDebug` at the end of `step()`.

In `ForceGraphScene.cpp`: remove the `QElapsedTimer` include, `timer` variables, and `qDebug` lines from `setNodes` and `setEdges`.

In `GraphViewTab.cpp`: remove the `QElapsedTimer` include, `buildTimer`, `phaseTimer` variables, and all their `qDebug` lines.

- [ ] **Step 2: Build and run all tests**

Run: `cd build && cmake --build . && ctest -R "tst_quadtree|tst_forcelayout|tst_benchmark" --output-on-failure`
Expected: All tests PASS. Clean build with no warnings.

- [ ] **Step 3: Commit**

```bash
git add libs/forcegraph/src/MultilevelLayout.cpp \
        libs/forcegraph/src/ForceLayoutEngine.cpp \
        libs/forcegraph/src/ForceGraphScene.cpp \
        src/graph/GraphViewTab.cpp
git commit -m "chore: remove diagnostic timing instrumentation

Benchmark harness now provides structured measurement;
ad-hoc qDebug timing no longer needed."
```
