// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QPointF>
#include <QRectF>
#include <QVector>
#include "GraphTypes.h"

namespace ForceGraph {
class QuadTree {
public:
    void build(const QVector<GraphNode> &nodes, const QRectF &bounds,
                const QVector<double> &masses = {});
    void build(const QVector<QPointF> &positions, const QRectF &bounds,
               const QVector<double> &masses);
    QPointF computeRepulsion(const QPointF &nodePos, double repelForce,
                              double nodeMass = 1.0, double theta = 0.8) const;
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
    QVector<double> m_masses;
    int m_root = -1;
    static constexpr int MAX_DEPTH = 30;
    void insert(int quadNodeIdx, int nodeIdx, const QVector<GraphNode> &nodes, int depth = 0);
    void insert(int quadNodeIdx, int nodeIdx, const QVector<QPointF> &positions, int depth = 0);
    void subdivide(int quadNodeIdx);
};
} // namespace ForceGraph
