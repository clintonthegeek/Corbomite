// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphNode.h"
#include <QPainter>
namespace ForceGraph {
ForceGraphNode::ForceGraphNode(const GraphNode &data, QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent), m_data(data) {}
void ForceGraphNode::setData(const GraphNode &data) { m_data = data; }
QString ForceGraphNode::nodeId() const { return m_data.id; }
void ForceGraphNode::setHighlighted(bool h) { m_highlighted = h; }
void ForceGraphNode::setDimmed(bool d) { m_dimmed = d; }
void ForceGraphNode::paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) {}
} // namespace ForceGraph
