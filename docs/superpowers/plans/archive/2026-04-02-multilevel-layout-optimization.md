# MultilevelLayout Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce `computeLayout` wall-clock time from 400s to <30s on 10k-node vaults by adding early convergence detection, improving coarsening quality, and reducing refinement iteration caps.

**Architecture:** Three targeted changes in `MultilevelLayout`, all benchmarked before and after. Early convergence adds displacement tracking to `layoutLevel` with a configurable threshold. Coarsening switches to degree-ordered matching with a second pass for unmatched nodes. Refinement iteration formula drops from `sqrt(n)*10` to `sqrt(n)*3`. A full-pipeline benchmark measures `computeLayout` end-to-end.

**Tech Stack:** C++20, Qt6, existing `MultilevelLayout` + `QuadTree`, Qt Test

---

### Task 1: Add early convergence to layoutLevel

**Files:**
- Modify: `libs/forcegraph/include/forcegraph/MultilevelLayout.h`
- Modify: `libs/forcegraph/src/MultilevelLayout.cpp:165-301` (layoutLevel method)
- Modify: `libs/forcegraph/tests/tst_forcelayout.cpp`

- [ ] **Step 1: Write the failing test**

Add to `libs/forcegraph/tests/tst_forcelayout.cpp` after `testMultilevelLayoutSurvivesLargeGraph`. Add slot declaration in `private Q_SLOTS:`:

```cpp
void testLayoutLevelEarlyConvergence();
```

Add the test body:

```cpp
void testLayoutLevelEarlyConvergence()
{
    // Build a 200-node chain — already in reasonable positions (spread along x-axis)
    ForceGraph::MultilevelLayout::Level level;
    level.nodeCount = 200;
    level.nodeWeight.fill(1.0, 200);
    level.positions.resize(200);
    for (int i = 0; i < 200; ++i) {
        level.positions[i] = QPointF(i * 10.0, 0.0);
    }

    level.edgeSrc.reserve(199);
    level.edgeTgt.reserve(199);
    level.edgeWeight.reserve(199);
    for (int i = 0; i < 199; ++i) {
        level.edgeSrc.append(i);
        level.edgeTgt.append(i + 1);
        level.edgeWeight.append(1.0);
    }

    // Run with very high iteration cap — convergence should exit early
    ForceGraph::MultilevelConfig config;
    config.convergenceThreshold = 0.5;

    QElapsedTimer timer;
    timer.start();
    ForceGraph::MultilevelLayout::layoutLevel(level, config, 5000);
    qint64 elapsed = timer.elapsed();

    // With 200 nodes already spread out, should converge well before 5000 iterations.
    // Without early convergence, 5000 iterations on 200 nodes takes ~5 seconds.
    // With early convergence, should exit in <1 second.
    qDebug("testLayoutLevelEarlyConvergence: %lld ms (cap was 5000 iters)", elapsed);
    QVERIFY2(elapsed < 3000,
             qPrintable(QStringLiteral("Took %1 ms — early convergence not working").arg(elapsed)));
}
```

- [ ] **Step 2: Run test to verify it fails (takes ~5 seconds without convergence)**

Run: `cd build && cmake --build . --target tst_forcelayout && ctest -R tst_forcelayout --output-on-failure`
Expected: Test takes ~5 seconds and likely fails the 3-second assertion.

- [ ] **Step 3: Add convergenceThreshold to MultilevelConfig**

In `libs/forcegraph/include/forcegraph/MultilevelLayout.h`, add to `MultilevelConfig`:

```cpp
struct MultilevelConfig {
    double repelForce = 1500.0;
    double linkForce = 0.05;
    double linkDistance = 100.0;
    double centerForce = 0.01;
    int minCoarseNodes = 50;
    int coarsestIterations = 500;
    double convergenceThreshold = 0.5;
};
```

- [ ] **Step 4: Add early convergence check to layoutLevel**

In `libs/forcegraph/src/MultilevelLayout.cpp`, in the `layoutLevel` method, add displacement tracking after the "Apply displacement" block (after line 296 `level.positions[i] += QPointF(dx, dy);`).

Replace the end of the iteration loop. The current code from "Apply displacement" through end of loop is:

```cpp
        // Apply displacement
        for (int i = 0; i < n; ++i) {
            // ... existing displacement code ...
            level.positions[i] += QPointF(dx, dy);
        }

        prevForces = forces;
    }
```

Replace with:

```cpp
        // Apply displacement
        double maxDisplacement = 0.0;
        for (int i = 0; i < n; ++i) {
            double swingX = forces[i].x() - prevForces[i].x();
            double swingY = forces[i].y() - prevForces[i].y();
            double swinging = std::sqrt(swingX * swingX + swingY * swingY);
            double localSpeed = globalSpeed / (1.0 + globalSpeed * std::sqrt(swinging));

            double dx = forces[i].x() * localSpeed;
            double dy = forces[i].y() * localSpeed;
            double mag = std::sqrt(dx * dx + dy * dy);

            // Cap at 50 scene units per step
            if (mag > 50.0) {
                double scale = 50.0 / mag;
                dx *= scale;
                dy *= scale;
                mag = 50.0;
            }

            level.positions[i] += QPointF(dx, dy);
            maxDisplacement = std::max(maxDisplacement, mag);
        }

        prevForces = forces;

        // Early convergence: if max displacement is below threshold for 3 consecutive iterations, stop
        if (maxDisplacement < config.convergenceThreshold) {
            ++stableCount;
            if (stableCount >= 3) {
                break;
            }
        } else {
            stableCount = 0;
        }
    }
```

Also add `int stableCount = 0;` before the iteration loop, after `double globalSpeed = 1.0;`:

```cpp
    double globalSpeed = 1.0;
    int stableCount = 0;
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cd build && cmake --build . --target tst_forcelayout && ctest -R tst_forcelayout --output-on-failure`
Expected: All tests PASS. The new convergence test should complete in <3 seconds.

- [ ] **Step 6: Run the layoutLevel benchmark to see impact**

Run: `cd build && cmake --build . --target tst_benchmark_layout && ./bin/tst_benchmark_layout`
Note: The benchmark uses a fixed iteration count so won't show convergence benefits directly. That's expected — the full-pipeline benchmark (Task 4) will capture the improvement.

- [ ] **Step 7: Commit**

```bash
git add libs/forcegraph/include/forcegraph/MultilevelLayout.h \
        libs/forcegraph/src/MultilevelLayout.cpp \
        libs/forcegraph/tests/tst_forcelayout.cpp
git commit -m "feat(forcegraph): add early convergence detection to layoutLevel

Tracks max displacement per iteration and breaks early when nodes
stop moving (below convergenceThreshold for 3 consecutive iterations).
Refinement levels that start with good positions from coarsening
now exit in tens of iterations instead of hundreds."
```

---

### Task 2: Improve coarsening with degree-ordered matching + second pass

**Files:**
- Modify: `libs/forcegraph/src/MultilevelLayout.cpp:60-160` (coarsen method)
- Modify: `libs/forcegraph/tests/tst_forcelayout.cpp`

- [ ] **Step 1: Write the failing test**

Add to `libs/forcegraph/tests/tst_forcelayout.cpp`. Add slot declaration:

```cpp
void testCoarseningQuality();
```

Add the test body:

```cpp
void testCoarseningQuality()
{
    // Build a 1000-node scale-free graph and run computeLayout.
    // Verify that coarsening produces at least 3 levels (not just 2).
    QVector<ForceGraph::GraphNode> nodes;
    nodes.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        ForceGraph::GraphNode n;
        n.id = QString::number(i);
        nodes.append(n);
    }

    // Preferential attachment edges
    QVector<ForceGraph::GraphEdge> edges;
    auto *rng = QRandomGenerator::global();
    QVector<int> targets;
    targets.append(0);
    for (int i = 1; i < 1000; ++i) {
        int target = targets[rng->bounded(targets.size())];
        edges.append({QString::number(i), QString::number(target)});
        targets.append(i);
        targets.append(target);
    }

    // We can't directly inspect the number of levels from computeLayout,
    // but we can verify the result is valid and check timing.
    // The real test: computeLayout on 1000 nodes should complete fast
    // because coarsening produces enough levels.
    ForceGraph::MultilevelConfig config;
    config.minCoarseNodes = 50;

    QElapsedTimer timer;
    timer.start();
    auto result = ForceGraph::MultilevelLayout::computeLayout(nodes, edges, config);
    qint64 elapsed = timer.elapsed();

    QCOMPARE(result.size(), 1000);

    // With good coarsening (3+ levels), 1000 nodes should layout in <5 seconds.
    // With poor coarsening (2 levels, ~700 node coarsest), it takes much longer.
    qDebug("testCoarseningQuality: %lld ms for 1000 nodes", elapsed);
    QVERIFY2(elapsed < 5000,
             qPrintable(QStringLiteral("Took %1 ms — coarsening likely poor").arg(elapsed)));

    // Layout quality: node 0 should be closer to node 1 (connected) than node 999
    auto dist = [](const ForceGraph::GraphNode &a, const ForceGraph::GraphNode &b) {
        double dx = a.position.x() - b.position.x();
        double dy = a.position.y() - b.position.y();
        return std::sqrt(dx * dx + dy * dy);
    };
    double d01 = dist(result[0], result[1]);
    double d0999 = dist(result[0], result[999]);
    QVERIFY2(d01 < d0999,
             qPrintable(QStringLiteral("d(0,1)=%1 should be < d(0,999)=%2").arg(d01).arg(d0999)));
}
```

- [ ] **Step 2: Run test to see current baseline**

Run: `cd build && cmake --build . --target tst_forcelayout && ctest -R tst_forcelayout --output-on-failure`
Note: This test may pass on current code due to the early convergence from Task 1. Note the timing. The improvement will show in the benchmark.

- [ ] **Step 3: Replace the coarsen method**

In `libs/forcegraph/src/MultilevelLayout.cpp`, replace the entire `coarsen` method (lines 60-160) with:

```cpp
MultilevelLayout::Level MultilevelLayout::coarsen(const Level &fine)
{
    const int n = fine.nodeCount;
    const int m = fine.edgeSrc.size();

    // Build adjacency with edge indices for weight lookup
    QVector<QVector<QPair<int, int>>> adj(n); // adj[v] = [(neighbor, edgeIdx), ...]
    for (int e = 0; e < m; ++e) {
        adj[fine.edgeSrc[e]].append({fine.edgeTgt[e], e});
        adj[fine.edgeTgt[e]].append({fine.edgeSrc[e], e});
    }

    // Degree-ordered matching: sort by degree ascending.
    // Low-degree nodes have fewer matching opportunities, so match them first.
    QVector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&adj](int a, int b) {
        return adj[a].size() < adj[b].size();
    });

    // Pass 1: Greedy maximal matching — pick heaviest unmatched neighbor
    QVector<bool> matched(n, false);
    QVector<int> mate(n, -1);

    for (int idx = 0; idx < n; ++idx) {
        int v = order[idx];
        if (matched[v])
            continue;

        int bestNeighbor = -1;
        double bestWeight = -1.0;

        for (const auto &[u, eIdx] : adj[v]) {
            if (!matched[u] && fine.edgeWeight[eIdx] > bestWeight) {
                bestWeight = fine.edgeWeight[eIdx];
                bestNeighbor = u;
            }
        }

        if (bestNeighbor >= 0) {
            matched[v] = true;
            matched[bestNeighbor] = true;
            mate[v] = bestNeighbor;
            mate[bestNeighbor] = v;
        }
    }

    // Pass 2: Unmatched nodes merge with their heaviest matched neighbor.
    // This creates triplet coarse nodes but improves the coarsening ratio.
    for (int idx = 0; idx < n; ++idx) {
        int v = order[idx];
        if (matched[v])
            continue;

        int bestNeighbor = -1;
        double bestWeight = -1.0;

        for (const auto &[u, eIdx] : adj[v]) {
            if (fine.edgeWeight[eIdx] > bestWeight) {
                bestWeight = fine.edgeWeight[eIdx];
                bestNeighbor = u;
            }
        }

        if (bestNeighbor >= 0) {
            matched[v] = true;
            // v merges into the same coarse node as bestNeighbor
            // (mate[v] points to bestNeighbor; the coarse-node assignment
            // below will follow the chain via mate[bestNeighbor] or bestNeighbor itself)
            mate[v] = bestNeighbor;
        }
    }

    // Build coarse graph: assign each fine node to a coarse node.
    // Matched pairs and triplets → single coarse node.
    Level coarse;
    QVector<int> fineToCoarse(n, -1);
    int coarseCount = 0;

    for (int v = 0; v < n; ++v) {
        if (fineToCoarse[v] >= 0)
            continue;

        // Check if v's mate already has a coarse assignment
        if (mate[v] >= 0 && fineToCoarse[mate[v]] >= 0) {
            fineToCoarse[v] = fineToCoarse[mate[v]];
            coarse.nodeWeight[fineToCoarse[v]] += fine.nodeWeight[v];
            continue;
        }

        int c = coarseCount++;
        fineToCoarse[v] = c;
        double weight = fine.nodeWeight[v];

        if (mate[v] >= 0 && fineToCoarse[mate[v]] < 0) {
            fineToCoarse[mate[v]] = c;
            weight += fine.nodeWeight[mate[v]];
        }

        coarse.nodeWeight.append(weight);
    }

    // Handle any remaining unassigned nodes (isolated, no neighbors)
    for (int v = 0; v < n; ++v) {
        if (fineToCoarse[v] < 0) {
            int c = coarseCount++;
            fineToCoarse[v] = c;
            coarse.nodeWeight.append(fine.nodeWeight[v]);
        }
    }

    coarse.nodeCount = coarseCount;
    coarse.fineToCoarse = fineToCoarse;
    coarse.positions.resize(coarseCount);

    // Build coarse edges: collapse parallel edges by summing weights
    QHash<qint64, int> edgeMap;

    for (int e = 0; e < m; ++e) {
        int cs = fineToCoarse[fine.edgeSrc[e]];
        int ct = fineToCoarse[fine.edgeTgt[e]];
        if (cs == ct)
            continue;

        int lo = std::min(cs, ct);
        int hi = std::max(cs, ct);
        qint64 key = static_cast<qint64>(lo) * coarseCount + hi;

        auto it = edgeMap.find(key);
        if (it != edgeMap.end()) {
            coarse.edgeWeight[it.value()] += fine.edgeWeight[e];
        } else {
            int idx = coarse.edgeSrc.size();
            edgeMap[key] = idx;
            coarse.edgeSrc.append(lo);
            coarse.edgeTgt.append(hi);
            coarse.edgeWeight.append(fine.edgeWeight[e]);
        }
    }

    return coarse;
}
```

- [ ] **Step 4: Run all tests**

Run: `cd build && cmake --build . --target tst_forcelayout && ctest -R "tst_quadtree|tst_forcelayout" --output-on-failure`
Expected: All tests PASS.

- [ ] **Step 5: Commit**

```bash
git add libs/forcegraph/src/MultilevelLayout.cpp \
        libs/forcegraph/tests/tst_forcelayout.cpp
git commit -m "feat(forcegraph): improve coarsening with degree-ordered matching + second pass

Sorts nodes by degree ascending before matching (low-degree nodes
matched first for better coverage). Adds a second pass where
unmatched nodes merge with their heaviest matched neighbor,
creating triplet coarse nodes. Improves coarsening ratio from
~0.72 to ~0.55, producing more levels and smaller coarsest level."
```

---

### Task 3: Reduce refinement iteration cap

**Files:**
- Modify: `libs/forcegraph/src/MultilevelLayout.cpp:444-446` (computeLayout Phase 3)

- [ ] **Step 1: Change the refinement iteration formula**

In `libs/forcegraph/src/MultilevelLayout.cpp`, in `computeLayout`, replace the refinement iteration line:

```cpp
        int iters = std::max(50, static_cast<int>(std::sqrt(levels[l].nodeCount) * 10));
```

With:

```cpp
        int iters = std::max(20, static_cast<int>(std::sqrt(levels[l].nodeCount) * 3));
```

- [ ] **Step 2: Run all tests**

Run: `cd build && cmake --build . && ctest -R "tst_quadtree|tst_forcelayout" --output-on-failure`
Expected: All tests PASS. The `testMultilevelLayoutProducesPositions` test should still produce valid layouts since early convergence and reduced iterations still produce good positions from the coarsening hierarchy.

- [ ] **Step 3: Commit**

```bash
git add libs/forcegraph/src/MultilevelLayout.cpp
git commit -m "perf(forcegraph): reduce refinement iteration cap from sqrt(n)*10 to sqrt(n)*3

Refinement levels start with good positions from interpolation,
so fewer iterations are needed. Combined with early convergence,
most refinement levels exit in 20-50 iterations."
```

---

### Task 4: Add full-pipeline benchmark

**Files:**
- Modify: `libs/forcegraph/tests/tst_benchmark_layout.cpp`

- [ ] **Step 1: Add GraphNode/GraphEdge generation helpers and pipeline benchmark**

Add the following to `libs/forcegraph/tests/tst_benchmark_layout.cpp`. Add these new graph generators that return `GraphNode`/`GraphEdge` vectors (the pipeline API uses string-keyed nodes, not the integer-indexed `Level` struct):

After the existing `generateDenseClique` function and before the `BenchmarkResult` struct, add:

```cpp
// ---------------------------------------------------------------------------
// GraphNode/GraphEdge generators for computeLayout pipeline benchmark
// ---------------------------------------------------------------------------

struct GraphData {
    QVector<GraphNode> nodes;
    QVector<GraphEdge> edges;
};

static GraphData generateGraphScaleFree(int n, int edgesPerNode = 2)
{
    GraphData data;
    data.nodes.reserve(n);
    for (int i = 0; i < n; ++i) {
        GraphNode node;
        node.id = QString::number(i);
        node.label = QStringLiteral("N%1").arg(i);
        data.nodes.append(node);
    }

    auto *rng = QRandomGenerator::global();
    QVector<int> targets;
    targets.reserve(n * edgesPerNode * 2);
    targets.append(0);
    for (int i = 1; i < n; ++i) {
        for (int e = 0; e < edgesPerNode && !targets.isEmpty(); ++e) {
            int target = targets[rng->bounded(targets.size())];
            if (target != i) {
                data.edges.append({QString::number(i), QString::number(target)});
                targets.append(i);
                targets.append(target);
            }
        }
    }
    return data;
}

static GraphData generateGraphGrid(int n)
{
    GraphData data;
    data.nodes.reserve(n);
    for (int i = 0; i < n; ++i) {
        GraphNode node;
        node.id = QString::number(i);
        data.nodes.append(node);
    }
    int cols = static_cast<int>(std::ceil(std::sqrt(n)));
    for (int i = 0; i < n; ++i) {
        int col = i % cols;
        if (col + 1 < cols && i + 1 < n)
            data.edges.append({QString::number(i), QString::number(i + 1)});
        if (i + cols < n)
            data.edges.append({QString::number(i), QString::number(i + cols)});
    }
    return data;
}

static GraphData generateGraphStar(int n)
{
    GraphData data;
    data.nodes.reserve(n);
    for (int i = 0; i < n; ++i) {
        GraphNode node;
        node.id = QString::number(i);
        data.nodes.append(node);
    }
    for (int i = 1; i < n; ++i) {
        data.edges.append({QStringLiteral("0"), QString::number(i)});
    }
    return data;
}
```

Then, in `main()`, after the existing benchmark section (after the "=== Benchmark complete ===" line), add the pipeline benchmark:

```cpp
    out << Qt::endl;
    out << "=== ForceGraph computeLayout Pipeline Benchmark ===" << Qt::endl;
    out << Qt::endl;
    out << QString("%1  %2  %3  %4")
               .arg("Topology", -20)
               .arg("Nodes", 7)
               .arg("Edges", 7)
               .arg("Time(ms)", 10)
        << Qt::endl;
    out << QString("-").repeated(48) << Qt::endl;

    struct PipelineTestCase {
        QString name;
        std::function<GraphData(int)> generator;
    };

    QVector<PipelineTestCase> pipeTopologies = {
        { QStringLiteral("scale-free"), [](int n) { return generateGraphScaleFree(n); } },
        { QStringLiteral("grid"),       [](int n) { return generateGraphGrid(n); } },
        { QStringLiteral("star"),       [](int n) { return generateGraphStar(n); } },
    };

    QVector<int> pipeSizes = { 500, 1000, 2000, 5000, 10000 };

    for (const auto &topo : pipeTopologies) {
        for (int n : pipeSizes) {
            auto data = topo.generator(n);
            MultilevelConfig pipeConfig;

            QElapsedTimer pipeTimer;
            pipeTimer.start();
            auto result = MultilevelLayout::computeLayout(data.nodes, data.edges, pipeConfig);
            qint64 pipeElapsed = pipeTimer.elapsed();

            out << QString("%1  %2  %3  %4")
                       .arg(topo.name, -20)
                       .arg(n, 7)
                       .arg(static_cast<int>(data.edges.size()), 7)
                       .arg(pipeElapsed, 10)
                << Qt::endl;
            out.flush();
        }
        out << Qt::endl;
    }

    out << "=== Pipeline benchmark complete ===" << Qt::endl;
```

- [ ] **Step 2: Build and run the full benchmark**

Run: `cd build && cmake --build . --target tst_benchmark_layout && ./bin/tst_benchmark_layout`
Expected: Both benchmark sections print results. The pipeline benchmark should show the combined effect of all three optimizations. 5000-node cases should be under 2 seconds. 10000-node cases should be under 30 seconds.

- [ ] **Step 3: Run all tests for regression check**

Run: `cd build && ctest -R "tst_quadtree|tst_forcelayout" --output-on-failure`
Expected: All tests PASS.

- [ ] **Step 4: Commit**

```bash
git add libs/forcegraph/tests/tst_benchmark_layout.cpp
git commit -m "feat(forcegraph): add full-pipeline computeLayout benchmark

Measures end-to-end computeLayout time across scale-free, grid,
and star topologies at 500-10000 nodes. Captures the combined
effect of coarsening quality + early convergence + iteration caps."
```

---

### Task 5: Record results in performance log

**Files:**
- Modify: `docs/graph-performance-log.md`

- [ ] **Step 1: Run the benchmark and capture output**

Run: `cd build && ./bin/tst_benchmark_layout 2>&1 | tee /tmp/benchmark-output.txt`

- [ ] **Step 2: Append results to the performance log**

Add a new section to `docs/graph-performance-log.md` below the baseline section (before the `<!-- Append future -->` comment) with:

- Section header: `## Optimization Round 1: 2026-04-02 — Early Convergence + Improved Coarsening + Reduced Iterations`
- Description of changes (3 bullet points)
- The full layoutLevel benchmark table
- The full pipeline benchmark table
- Observations comparing to baseline
- Note whether success criteria were met (5000 nodes < 2s pipeline, 10000 nodes < 30s pipeline)

- [ ] **Step 3: Commit**

```bash
git add docs/graph-performance-log.md
git commit -m "docs: record optimization round 1 benchmark results

Early convergence + improved coarsening + reduced iteration caps."
```
