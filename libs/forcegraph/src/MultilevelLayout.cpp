// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/MultilevelLayout.h"
#include "forcegraph/QuadTree.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPair>
#include <QRandomGenerator>
#include <QQueue>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace ForceGraph {

static constexpr double EPSILON = 0.001;

// ---------------------------------------------------------------------------
// buildLevel0 — convert string-keyed graph data to integer-indexed Level
// ---------------------------------------------------------------------------
MultilevelLayout::Level MultilevelLayout::buildLevel0(
    const QVector<GraphNode> &nodes, const QVector<GraphEdge> &edges)
{
    Level level;
    level.nodeCount = nodes.size();
    level.nodeWeight.resize(nodes.size(), 1.0);
    level.positions.resize(nodes.size());

    QHash<QString, int> idToIndex;
    idToIndex.reserve(nodes.size());
    for (int i = 0; i < nodes.size(); ++i) {
        idToIndex[nodes[i].id] = i;
        level.positions[i] = nodes[i].position;
    }

    level.edgeSrc.reserve(edges.size());
    level.edgeTgt.reserve(edges.size());
    level.edgeWeight.reserve(edges.size());

    for (const auto &edge : edges) {
        auto srcIt = idToIndex.find(edge.sourceId);
        auto tgtIt = idToIndex.find(edge.targetId);
        if (srcIt == idToIndex.end() || tgtIt == idToIndex.end())
            continue;
        int s = srcIt.value();
        int t = tgtIt.value();
        if (s == t)
            continue; // skip self-loops

        level.edgeSrc.append(s);
        level.edgeTgt.append(t);
        level.edgeWeight.append(1.0);
    }

    return level;
}

// ---------------------------------------------------------------------------
// coarsen — maximal edge matching (Walshaw)
// ---------------------------------------------------------------------------
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

    // Shuffle vertex order for better matching quality
    QVector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    auto *rng = QRandomGenerator::global();
    for (int i = n - 1; i > 0; --i) {
        int j = rng->bounded(i + 1);
        std::swap(order[i], order[j]);
    }

    // Greedy maximal matching: pick heaviest unmatched neighbor
    QVector<bool> matched(n, false);
    QVector<int> mate(n, -1); // mate[v] = matched partner, or -1

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

    // Build coarse graph: assign each fine node to a coarse node
    Level coarse;
    QVector<int> fineToCoarse(n, -1);
    int coarseCount = 0;

    // Matched pairs → single coarse node
    for (int v = 0; v < n; ++v) {
        if (fineToCoarse[v] >= 0)
            continue;

        int c = coarseCount++;
        fineToCoarse[v] = c;

        if (mate[v] >= 0) {
            fineToCoarse[mate[v]] = c;
            coarse.nodeWeight.append(fine.nodeWeight[v] + fine.nodeWeight[mate[v]]);
        } else {
            coarse.nodeWeight.append(fine.nodeWeight[v]);
        }
    }

    coarse.nodeCount = coarseCount;
    coarse.fineToCoarse = fineToCoarse;
    coarse.positions.resize(coarseCount);

    // Build coarse edges: collapse parallel edges by summing weights
    // Use a hash to merge edges between the same coarse node pair
    QHash<qint64, int> edgeMap; // (min*N + max) → index in coarse edge arrays

    for (int e = 0; e < m; ++e) {
        int cs = fineToCoarse[fine.edgeSrc[e]];
        int ct = fineToCoarse[fine.edgeTgt[e]];
        if (cs == ct)
            continue; // contracted into same coarse node

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

// ---------------------------------------------------------------------------
// layoutLevel — synchronous brute-force force-directed layout
// ---------------------------------------------------------------------------
void MultilevelLayout::layoutLevel(Level &level, const MultilevelConfig &config, int iterations)
{
    const int n = level.nodeCount;
    if (n <= 1)
        return;

    const int m = level.edgeSrc.size();

    // Build adjacency for degree computation
    QVector<double> degree(n, 0.0);
    for (int e = 0; e < m; ++e) {
        degree[level.edgeSrc[e]] += level.edgeWeight[e];
        degree[level.edgeTgt[e]] += level.edgeWeight[e];
    }

    QVector<QPointF> forces(n);
    QVector<QPointF> prevForces(n, QPointF(0, 0));
    double globalSpeed = 1.0;

    // Log every ~10% of iterations, minimum every 50
    int logInterval = std::max(iterations / 10, 1);
    QElapsedTimer iterTimer;
    iterTimer.start();

    for (int iter = 0; iter < iterations; ++iter) {
        if (iter > 0 && iter % logInterval == 0) {
            qDebug("    layoutLevel: iter %d/%d (%d nodes, %d edges) — %lld ms elapsed",
                   iter, iterations, n, m, iterTimer.elapsed());
        }
        // Reset forces
        for (int i = 0; i < n; ++i)
            forces[i] = QPointF(0, 0);

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

        // Attractive forces along edges
        for (int e = 0; e < m; ++e) {
            int s = level.edgeSrc[e];
            int t = level.edgeTgt[e];
            double dx = level.positions[s].x() - level.positions[t].x();
            double dy = level.positions[s].y() - level.positions[t].y();
            double dist = std::sqrt(dx * dx + dy * dy);
            dist = std::max(dist, EPSILON);

            double force = (dist - config.linkDistance) * config.linkForce * level.edgeWeight[e];
            double fx = (dx / dist) * force;
            double fy = (dy / dist) * force;

            forces[s] -= QPointF(fx, fy);
            forces[t] += QPointF(fx, fy);
        }

        // Center gravity (degree-weighted)
        for (int i = 0; i < n; ++i) {
            double deg = degree[i] + 1.0;
            forces[i] -= level.positions[i] * config.centerForce * deg;
        }

        // Adaptive speed (simplified ForceAtlas2)
        double globalSwinging = 0.0;
        double globalTraction = 0.0;

        for (int i = 0; i < n; ++i) {
            double deg = degree[i] + 1.0;
            double swingX = forces[i].x() - prevForces[i].x();
            double swingY = forces[i].y() - prevForces[i].y();
            double swinging = std::sqrt(swingX * swingX + swingY * swingY);
            double tractX = (forces[i].x() + prevForces[i].x()) / 2.0;
            double tractY = (forces[i].y() + prevForces[i].y()) / 2.0;
            double traction = std::sqrt(tractX * tractX + tractY * tractY);
            globalSwinging += deg * swinging;
            globalTraction += deg * traction;
        }

        if (globalSwinging > EPSILON) {
            globalSpeed = globalTraction / globalSwinging;
        }
        globalSpeed = std::max(globalSpeed, EPSILON);

        // Apply displacement
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
            }

            level.positions[i] += QPointF(dx, dy);
        }

        prevForces = forces;
    }
}

// ---------------------------------------------------------------------------
// interpolate — project coarse positions back to fine level
// ---------------------------------------------------------------------------
void MultilevelLayout::interpolate(Level &fine, const Level &coarse)
{
    // coarse.fineToCoarse maps fine-level index → coarse-level index
    auto *rng = QRandomGenerator::global();
    double jitter = 5.0; // scene units — small offset to separate merged nodes

    for (int i = 0; i < fine.nodeCount; ++i) {
        int c = coarse.fineToCoarse[i];
        double offsetX = (rng->generateDouble() - 0.5) * jitter;
        double offsetY = (rng->generateDouble() - 0.5) * jitter;
        fine.positions[i] = coarse.positions[c] + QPointF(offsetX, offsetY);
    }
}

// ---------------------------------------------------------------------------
// computeLayout — public entry point
// ---------------------------------------------------------------------------
QVector<GraphNode> MultilevelLayout::computeLayout(
    const QVector<GraphNode> &nodes,
    const QVector<GraphEdge> &edges,
    const MultilevelConfig &config)
{
    QElapsedTimer totalTimer;
    totalTimer.start();

    if (nodes.size() <= config.minCoarseNodes) {
        // Too small to benefit from multilevel — return as-is
        return nodes;
    }

    // Phase 1: Build coarsening hierarchy
    QElapsedTimer phaseTimer;
    phaseTimer.start();

    QVector<Level> levels;
    levels.append(buildLevel0(nodes, edges));

    while (levels.last().nodeCount > config.minCoarseNodes) {
        Level coarser = coarsen(levels.last());

        // Diminishing returns: stop if coarsening isn't reducing size enough
        double ratio = static_cast<double>(coarser.nodeCount) / levels.last().nodeCount;
        if (ratio > 0.75) {
            break;
        }

        levels.append(std::move(coarser));
    }

    int L = levels.size() - 1;
    qDebug("MultilevelLayout: %d levels, coarsest has %d nodes (from %lld)",
           L + 1, levels[L].nodeCount, static_cast<long long>(nodes.size()));
    qDebug("  Phase 1 (coarsening): %lld ms", phaseTimer.elapsed());

    // Phase 2: Layout coarsest level
    // BFS radial placement for initial positions at coarsest level
    {
        Level &coarsest = levels[L];
        const int cn = coarsest.nodeCount;

        // Build adjacency for BFS
        QVector<QVector<int>> adj(cn);
        for (int e = 0; e < coarsest.edgeSrc.size(); ++e) {
            adj[coarsest.edgeSrc[e]].append(coarsest.edgeTgt[e]);
            adj[coarsest.edgeTgt[e]].append(coarsest.edgeSrc[e]);
        }

        // Two-pass BFS for diameter endpoints
        auto bfsFarthest = [&](int start) -> int {
            QVector<int> dist(cn, -1);
            QQueue<int> queue;
            queue.enqueue(start);
            dist[start] = 0;
            int farthest = start;
            int maxDist = 0;
            while (!queue.isEmpty()) {
                int v = queue.dequeue();
                for (int u : adj[v]) {
                    if (dist[u] < 0) {
                        dist[u] = dist[v] + 1;
                        if (dist[u] > maxDist) {
                            maxDist = dist[u];
                            farthest = u;
                        }
                        queue.enqueue(u);
                    }
                }
            }
            return farthest;
        };

        int ep1 = bfsFarthest(0);
        // BFS from ep1 to assign layers
        QVector<int> layer(cn, -1);
        QQueue<int> queue;
        queue.enqueue(ep1);
        layer[ep1] = 0;
        int maxLayer = 0;

        while (!queue.isEmpty()) {
            int v = queue.dequeue();
            for (int u : adj[v]) {
                if (layer[u] < 0) {
                    layer[u] = layer[v] + 1;
                    maxLayer = std::max(maxLayer, layer[u]);
                    queue.enqueue(u);
                }
            }
        }

        // Handle disconnected nodes
        for (int i = 0; i < cn; ++i) {
            if (layer[i] < 0)
                layer[i] = maxLayer + 1;
        }

        // Place on concentric rings
        QVector<int> layerCounts(maxLayer + 2, 0);
        for (int i = 0; i < cn; ++i)
            layerCounts[layer[i]]++;

        QVector<int> layerIdx(maxLayer + 2, 0);
        auto *rng = QRandomGenerator::global();

        for (int i = 0; i < cn; ++i) {
            int l = layer[i];
            double radius = l * config.linkDistance;
            int count = layerCounts[l];
            int idx = layerIdx[l]++;

            double angle = (count == 1) ? 0.0 : (2.0 * M_PI * idx / count);
            angle += (rng->generateDouble() - 0.5) * 0.2;

            coarsest.positions[i] = QPointF(radius * std::cos(angle),
                                            radius * std::sin(angle));
        }

        // Run force layout on coarsest level
        phaseTimer.restart();
        layoutLevel(coarsest, config, config.coarsestIterations);
        qDebug("  Phase 2 (coarsest layout, %d nodes, %d iters): %lld ms",
               coarsest.nodeCount, config.coarsestIterations, phaseTimer.elapsed());
    }

    // Phase 3: Uncoarsen and refine
    for (int l = L - 1; l >= 0; --l) {
        interpolate(levels[l], levels[l + 1]);

        // Refinement: fewer iterations at finer levels
        int iters = std::max(50, static_cast<int>(std::sqrt(levels[l].nodeCount) * 10));
        phaseTimer.restart();
        layoutLevel(levels[l], config, iters);
        qDebug("  Phase 3 (refine level %d, %d nodes, %d iters): %lld ms",
               l, levels[l].nodeCount, iters, phaseTimer.elapsed());
    }

    qDebug("  MultilevelLayout total: %lld ms", totalTimer.elapsed());

    // Write positions back to nodes
    QVector<GraphNode> result = nodes;
    for (int i = 0; i < result.size(); ++i) {
        result[i].position = levels[0].positions[i];
    }
    return result;
}

} // namespace ForceGraph
