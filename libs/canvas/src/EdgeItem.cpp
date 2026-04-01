// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/EdgeItem.h"
#include "canvas/ConnectableItem.h"

#include <QPainter>
#include <cmath>
#include <QtMath>

namespace Canvas {

static constexpr qreal kArrowSize = 10.0;
static constexpr qreal kDefaultPenWidth = 2.0;

EdgeItem::EdgeItem(ConnectableItem *fromCard, ConnectableItem *toCard, const CanvasEdge &data, QGraphicsItem *parent)
    : QGraphicsPathItem(parent)
    , m_data(data)
    , m_source(fromCard)
    , m_target(toCard)
{
    QPen edgePen(QColor(150, 150, 150));
    edgePen.setWidthF(kDefaultPenWidth);

    // Apply edge color if set
    const QColor edgeColor = colorFromCanvasColor(m_data.color);
    if (edgeColor.isValid()) {
        edgePen.setColor(edgeColor);
    }

    setPen(edgePen);
    setZValue(0);
    adjust();
}

void EdgeItem::adjust()
{
    prepareGeometryChange();

    const QPointF fromScene = m_source->connectionPoint(m_data.fromSide);
    const QPointF toScene = m_target->connectionPoint(m_data.toSide);

    const QPointF from = mapFromScene(fromScene);
    const QPointF to = mapFromScene(toScene);

    QPainterPath edgePath;
    edgePath.moveTo(from);

    // Bezier curve for smooth S-shaped connections (matches Obsidian's visual style)
    double dx = to.x() - from.x();
    double dy = to.y() - from.y();
    double dist = std::sqrt(dx * dx + dy * dy);
    double curvature = std::min(dist * 0.4, 80.0); // Control point offset, capped

    QPointF ctrl1 = from;
    QPointF ctrl2 = to;

    // Control points extend outward from the connection side
    switch (m_data.fromSide) {
    case Side::Right:  ctrl1 += QPointF(curvature, 0); break;
    case Side::Left:   ctrl1 += QPointF(-curvature, 0); break;
    case Side::Bottom: ctrl1 += QPointF(0, curvature); break;
    case Side::Top:    ctrl1 += QPointF(0, -curvature); break;
    }
    switch (m_data.toSide) {
    case Side::Right:  ctrl2 += QPointF(curvature, 0); break;
    case Side::Left:   ctrl2 += QPointF(-curvature, 0); break;
    case Side::Bottom: ctrl2 += QPointF(0, curvature); break;
    case Side::Top:    ctrl2 += QPointF(0, -curvature); break;
    }

    edgePath.cubicTo(ctrl1, ctrl2, to);

    // Arrow at target end (use the curve's tangent direction)
    if (m_data.toEnd == EndType::Arrow) {
        drawArrowHead(edgePath, to, ctrl2);
    }

    // Arrow at source end
    if (m_data.fromEnd == EndType::Arrow) {
        drawArrowHead(edgePath, from, ctrl1);
    }

    setPath(edgePath);
}

void EdgeItem::drawArrowHead(QPainterPath &path, const QPointF &tip, const QPointF &from) const
{
    const QLineF line(from, tip);
    const qreal angle = std::atan2(-line.dy(), line.dx());

    const QPointF p1 = tip - QPointF(std::cos(angle - M_PI / 6) * kArrowSize,
                                     -std::sin(angle - M_PI / 6) * kArrowSize);
    const QPointF p2 = tip - QPointF(std::cos(angle + M_PI / 6) * kArrowSize,
                                     -std::sin(angle + M_PI / 6) * kArrowSize);

    QPolygonF arrowHead;
    arrowHead << tip << p1 << p2 << tip;
    path.addPolygon(arrowHead);
}

void EdgeItem::setEdgeData(const CanvasEdge &data)
{
    m_data = data;

    // Update pen color
    QPen edgePen = pen();
    const QColor edgeColor = colorFromCanvasColor(m_data.color);
    if (edgeColor.isValid()) {
        edgePen.setColor(edgeColor);
    } else {
        edgePen.setColor(QColor(150, 150, 150));
    }
    setPen(edgePen);

    adjust();
}

CanvasEdge EdgeItem::edgeData() const
{
    return m_data;
}

QString EdgeItem::edgeId() const
{
    return m_data.id;
}

ConnectableItem *EdgeItem::sourceCard() const
{
    return m_source;
}

ConnectableItem *EdgeItem::targetCard() const
{
    return m_target;
}

void EdgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // Draw the path (line + arrow heads)
    QGraphicsPathItem::paint(painter, option, widget);

    // Draw label at midpoint if set
    if (!m_data.label.isEmpty()) {
        const QPointF fromScene = m_source->connectionPoint(m_data.fromSide);
        const QPointF toScene = m_target->connectionPoint(m_data.toSide);
        const QPointF from = mapFromScene(fromScene);
        const QPointF to = mapFromScene(toScene);
        const QPointF mid = (from + to) / 2.0;

        painter->save();

        QFont labelFont = painter->font();
        labelFont.setPointSize(9);
        painter->setFont(labelFont);

        const QFontMetricsF fm(labelFont);
        const QRectF textRect = fm.boundingRect(m_data.label);
        const QRectF bgRect(mid.x() - textRect.width() / 2.0 - 4,
                            mid.y() - textRect.height() / 2.0 - 2,
                            textRect.width() + 8,
                            textRect.height() + 4);

        // Background for readability
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 220));
        painter->drawRoundedRect(bgRect, 3, 3);

        // Label text
        painter->setPen(QColor(80, 80, 80));
        painter->drawText(bgRect, Qt::AlignCenter, m_data.label);

        painter->restore();
    }
}

} // namespace Canvas
