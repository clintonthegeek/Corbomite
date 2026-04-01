// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphEdge.h"
#include "forcegraph/ForceGraphNode.h"
#include <QPen>
#include <QPainter>
#include <QtMath>

namespace ForceGraph {

ForceGraphEdge::ForceGraphEdge(ForceGraphNode *source, ForceGraphNode *target, QGraphicsItem *parent)
    : QGraphicsLineItem(parent)
    , m_source(source)
    , m_target(target)
{
    setZValue(0); // Behind nodes
    updatePen();
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
    updatePen();
}

void ForceGraphEdge::setWidthScale(double scale)
{
    m_widthScale = scale;
    updatePen();
}

void ForceGraphEdge::setShowArrows(bool show)
{
    m_showArrows = show;
    update();
}

void ForceGraphEdge::updatePen()
{
    if (m_dimmed) {
        setPen(QPen(QColor(200, 200, 200, 30), 0.5 * m_widthScale));
    } else {
        setPen(QPen(QColor(150, 150, 150, 100), 1.0 * m_widthScale));
    }
}

void ForceGraphEdge::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // Draw the line
    QGraphicsLineItem::paint(painter, option, widget);

    // Draw arrowhead at target end if enabled
    if (!m_showArrows || m_dimmed) return;

    QLineF edgeLine = line();
    if (edgeLine.length() < 1.0) return;

    double arrowSize = 6.0 * m_widthScale;
    double angle = std::atan2(-edgeLine.dy(), edgeLine.dx());

    QPointF targetPoint = edgeLine.p2();
    QPointF arrowP1 = targetPoint + QPointF(
        std::sin(angle - M_PI / 3) * arrowSize,
        std::cos(angle - M_PI / 3) * arrowSize);
    QPointF arrowP2 = targetPoint + QPointF(
        std::sin(angle - M_PI + M_PI / 3) * arrowSize,
        std::cos(angle - M_PI + M_PI / 3) * arrowSize);

    painter->setPen(Qt::NoPen);
    painter->setBrush(pen().color());
    QPolygonF arrowHead;
    arrowHead << targetPoint << arrowP1 << arrowP2;
    painter->drawPolygon(arrowHead);
}

} // namespace ForceGraph
