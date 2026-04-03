// SPDX-License-Identifier: GPL-3.0-or-later
//
// Benchmark harness for MultilevelLayout::layoutLevel.
// Generates synthetic graphs across a topology x size matrix,
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

// Scale-free (Barabasi-Albert preferential attachment)
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

// Random (Erdos-Renyi)
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

    return { name, level.nodeCount, static_cast<int>(level.edgeSrc.size()), iterations, elapsed };
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

    // Summary: flag any topology x size that exceeds 100ms/iter
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
