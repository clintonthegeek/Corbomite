// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/QuadTree.h"
#include <cmath>

namespace ForceGraph {

void QuadTree::clear()
{
    m_nodes.clear();
    m_masses.clear();
    m_root = -1;
}

void QuadTree::build(const QVector<GraphNode> &nodes, const QRectF &bounds,
                      const QVector<double> &masses)
{
    clear();
    if (nodes.isEmpty()) return;

    m_masses = masses;
    if (m_masses.isEmpty()) {
        m_masses.fill(1.0, nodes.size());
    }

    m_nodes.reserve(5 * nodes.size());

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
    double nodeMass = m_masses.value(nodeIdx, 1.0);
    double newMass = m_nodes[quadNodeIdx].totalMass + nodeMass;
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

QPointF QuadTree::computeRepulsion(const QPointF &pos, double repelForce,
                                    double nodeMass, double theta) const
{
    QPointF totalForce(0, 0);
    if (m_nodes.isEmpty() || m_root < 0) return totalForce;

    // Fixed-size stack (tree depth bounded by ~log4(n) ≈ 10)
    int stack[40];
    int stackSize = 0;
    stack[stackSize++] = m_root;

    while (stackSize > 0) {
        int idx = stack[--stackSize];
        const auto &node = m_nodes[idx];

        if (node.totalMass < 0.001) continue;

        QPointF delta = pos - node.centerOfMass;
        double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());

        if (dist < 0.001) continue;

        double size = std::max(node.bounds.width(), node.bounds.height());

        if (node.isLeaf() || (size / dist < theta)) {
            // Treat as single body
            double force = repelForce * nodeMass * node.totalMass / (dist * dist);
            totalForce += (delta / dist) * force;
        } else {
            // Push children onto stack (reverse order for depth-first)
            for (int i = 3; i >= 0; --i) {
                if (node.children[i] >= 0)
                    stack[stackSize++] = node.children[i];
            }
        }
    }

    return totalForce;
}

} // namespace ForceGraph
