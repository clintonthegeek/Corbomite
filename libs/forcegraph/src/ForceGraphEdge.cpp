// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphEdge.h"
#include "forcegraph/ForceGraphNode.h"
#include <QPen>

namespace ForceGraph {

ForceGraphEdge::ForceGraphEdge(ForceGraphNode *source, ForceGraphNode *target, QGraphicsItem *parent)
    : QGraphicsLineItem(parent)
    , m_source(source)
    , m_target(target)
{
    setZValue(0); // Behind nodes
    setPen(QPen(QColor(150, 150, 150, 100), 1));
    adjust();
}

void ForceGraphEdge::adjust()
{
    if (!m_source || !m_target) return;
    setLine(QLineF(m_source->pos(), m_target->pos()));
}

ForceGraphNode *ForceGraphEdge::sourceNode() const { return m_source; }
ForceGraphNode *ForceGraphEdge::targetNode() const { return m_target; }

void ForceGraphEdge::setDimmed(bool dimmed)
{
    m_dimmed = dimmed;
    if (dimmed) {
        setPen(QPen(QColor(200, 200, 200, 30), 0.5));
    } else {
        setPen(QPen(QColor(150, 150, 150, 100), 1));
    }
}

} // namespace ForceGraph
