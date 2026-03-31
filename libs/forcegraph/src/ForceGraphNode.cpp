// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphNode.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace ForceGraph {

ForceGraphNode::ForceGraphNode(const GraphNode &data, QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent)
    , m_data(data)
{
    double r = m_data.radius;
    setRect(-r, -r, 2 * r, 2 * r);
    setPos(m_data.position);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setZValue(1); // Above edges
}

void ForceGraphNode::setData(const GraphNode &data)
{
    m_data = data;
    double r = m_data.radius;
    setRect(-r, -r, 2 * r, 2 * r);
}

QString ForceGraphNode::nodeId() const
{
    return m_data.id;
}

void ForceGraphNode::setHighlighted(bool highlighted)
{
    m_highlighted = highlighted;
    update();
}

void ForceGraphNode::setDimmed(bool dimmed)
{
    m_dimmed = dimmed;
    update();
}

void ForceGraphNode::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    double lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
        painter->worldTransform());

    QColor color = m_data.color;
    if (m_dimmed) {
        color.setAlphaF(0.15);
    }
    if (m_highlighted) {
        color = color.lighter(130);
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);

    if (lod < 0.1) {
        // Ultra-low: single pixel
        painter->fillRect(QRectF(-1, -1, 2, 2), color);
        return;
    }

    if (lod < 0.4) {
        // Low: filled circle, no label
        painter->drawEllipse(rect());
        return;
    }

    // Medium+: circle with outline
    if (m_highlighted) {
        painter->setPen(QPen(color.darker(150), 2));
    }
    painter->drawEllipse(rect());

    if (lod < 1.0) {
        // Medium: abbreviated label
        if (!m_data.label.isEmpty()) {
            painter->setPen(m_dimmed ? QColor(128, 128, 128, 40) : QColor(60, 60, 60));
            QFont font;
            font.setPointSizeF(8);
            painter->setFont(font);
            painter->drawText(QPointF(-m_data.radius, m_data.radius + 12),
                              m_data.label.left(15));
        }
        return;
    }

    // Full: circle + full label
    if (!m_data.label.isEmpty()) {
        painter->setPen(m_dimmed ? QColor(128, 128, 128, 40) : QColor(40, 40, 40));
        QFont font;
        font.setPointSizeF(9);
        painter->setFont(font);
        QRectF textRect(QPointF(-50, m_data.radius + 4), QSizeF(100, 16));
        painter->drawText(textRect, Qt::AlignHCenter, m_data.label);
    }
}

} // namespace ForceGraph
