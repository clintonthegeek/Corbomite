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
