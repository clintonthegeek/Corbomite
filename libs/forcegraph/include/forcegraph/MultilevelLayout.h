// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "GraphTypes.h"
#include <QVector>

namespace ForceGraph {

struct MultilevelConfig {
    double repelForce = 1500.0;
    double linkForce = 0.05;
    double linkDistance = 100.0;
    double centerForce = 0.01;
    int minCoarseNodes = 50;
    int coarsestIterations = 500;
};

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

} // namespace ForceGraph
