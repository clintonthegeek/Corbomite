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
    const QPointF &pos = nodes[nodeIdx].position;

    // Update center of mass
    double newMass = m_nodes[quadNodeIdx].totalMass + 1.0;
    m_nodes[quadNodeIdx].centerOfMass =
        (m_nodes[quadNodeIdx].centerOfMass * m_nodes[quadNodeIdx].totalMass + pos) / newMass;
    m_nodes[quadNodeIdx].totalMass = newMass;

    if (m_nodes[quadNodeIdx].isEmpty()) {
        // Empty node — just store the node
        m_nodes[quadNodeIdx].nodeIndex = nodeIdx;
        return;
    }

    if (m_nodes[quadNodeIdx].isLeaf()) {
        // Occupied leaf — subdivide and reinsert existing node
        int existingIdx = m_nodes[quadNodeIdx].nodeIndex;
        m_nodes[quadNodeIdx].nodeIndex = -1;
        subdivide(quadNodeIdx);
        // Re-fetch not needed here since subdivide uses index-based access,
        // but reinsert existing node (this may further subdivide)
        insert(quadNodeIdx, existingIdx, nodes);
    }

    // Find which quadrant the new node belongs to
    // (after subdivide, children are guaranteed to exist)
    if (m_nodes[quadNodeIdx].children[0] < 0) {
        subdivide(quadNodeIdx);
    }

    double midX = m_nodes[quadNodeIdx].bounds.center().x();
    double midY = m_nodes[quadNodeIdx].bounds.center().y();

    int childIdx;
    if (pos.x() <= midX) {
        childIdx = (pos.y() <= midY) ? 0 : 2; // NW or SW
    } else {
        childIdx = (pos.y() <= midY) ? 1 : 3; // NE or SE
    }

    insert(m_nodes[quadNodeIdx].children[childIdx], nodeIdx, nodes);
}

void QuadTree::subdivide(int quadNodeIdx)
{
    const QRectF b = m_nodes[quadNodeIdx].bounds; // Copy — append may invalidate

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
        // Treat as single body: F = repelForce * mass / dist^2
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
