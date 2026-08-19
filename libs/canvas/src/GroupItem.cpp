// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/GroupItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

namespace Canvas {

static constexpr qreal kLabelPadding = 8.0;
static constexpr qreal kHandleSize = 6.0;

GroupItem::GroupItem(const CanvasNode &data, QGraphicsItem *parent)
    : CanvasNodeItem(data, parent)
    , m_lastPos(data.x, data.y)
{
    setZValue(-1);
}

void GroupItem::setNodeData(const CanvasNode &data)
{
    CanvasNodeItem::setNodeData(data);
    m_lastPos = pos();
}

QRectF GroupItem::boundingRect() const
{
    return QRectF(0, 0, m_data.width, m_data.height);
}

void GroupItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing);

    const QRectF rect = boundingRect();
    const bool selected = (option->state & QStyle::State_Selected);

    // 1. Fill background with group color at 10% opacity, or light gray if no color
    const QColor baseColor = colorFromCanvasColor(m_data.color);
    QColor fillColor;
    if (baseColor.isValid()) {
        fillColor = baseColor;
        fillColor.setAlpha(25); // ~10% opacity
    } else {
        fillColor = QColor(200, 200, 200, 25);
    }
    painter->fillRect(rect, fillColor);

    // 2. Draw border (group color at 40% opacity, or gray; blue 2px when selected)
    if (selected) {
        QPen borderPen(QColor(58, 134, 255));
        borderPen.setWidthF(2.0);
        painter->setPen(borderPen);
    } else {
        QColor borderColor;
        if (baseColor.isValid()) {
            borderColor = baseColor;
            borderColor.setAlpha(102); // ~40% opacity
        } else {
            borderColor = QColor(180, 180, 180);
        }
        QPen borderPen(borderColor);
        borderPen.setWidthF(1.0);
        painter->setPen(borderPen);
    }
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect);

    // 3. Draw label text at top-left with padding, bold, 11pt
    if (!m_data.label.isEmpty()) {
        QFont labelFont = painter->font();
        labelFont.setBold(true);
        labelFont.setPointSizeF(11.0);
        painter->setFont(labelFont);

        QColor textColor;
        if (baseColor.isValid()) {
            textColor = baseColor.darker(120);
        } else {
            textColor = QColor(80, 80, 80);
        }
        painter->setPen(textColor);

        const QRectF textRect(kLabelPadding, kLabelPadding,
                              rect.width() - 2 * kLabelPadding,
                              labelFont.pointSizeF() * 2);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop, m_data.label);
    }

    // 4. If selected, draw resize handles at 8 positions
    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(58, 134, 255));

        const qreal hs = kHandleSize;
        const qreal hh = hs / 2.0;
        const qreal w = rect.width();
        const qreal h = rect.height();

        // Corners
        painter->drawRect(QRectF(-hh, -hh, hs, hs));                          // TopLeft
        painter->drawRect(QRectF(w - hh, -hh, hs, hs));                       // TopRight
        painter->drawRect(QRectF(w - hh, h - hh, hs, hs));                    // BottomRight
        painter->drawRect(QRectF(-hh, h - hh, hs, hs));                       // BottomLeft

        // Edge midpoints
        painter->drawRect(QRectF(w / 2.0 - hh, -hh, hs, hs));                // Top
        painter->drawRect(QRectF(w - hh, h / 2.0 - hh, hs, hs));             // Right
        painter->drawRect(QRectF(w / 2.0 - hh, h - hh, hs, hs));             // Bottom
        painter->drawRect(QRectF(-hh, h / 2.0 - hh, hs, hs));                // Left
    }
}

QVector<QGraphicsItem *> GroupItem::containedItems() const
{
    QVector<QGraphicsItem *> result;
    if (!scene())
        return result;

    const QRectF myRect = sceneBoundingRect();
    for (auto *item : scene()->items()) {
        if (item == this)
            continue;
        if (dynamic_cast<GroupItem *>(item))
            continue; // Skip other groups
        if (!dynamic_cast<QGraphicsObject *>(item))
            continue; // Skip non-objects (edges are QGraphicsPathItem, not QGraphicsObject)
        const QPointF center = item->sceneBoundingRect().center();
        if (myRect.contains(center)) {
            result.append(item);
        }
    }
    return result;
}

QVariant GroupItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged && !m_movingChildren) {
        const QPointF newPos = value.toPointF();
        const QPointF delta = newPos - m_lastPos;
        m_lastPos = newPos;

        m_movingChildren = true;
        for (auto *item : containedItems()) {
            item->moveBy(delta.x(), delta.y());
        }
        m_movingChildren = false;
    }
    // Base handles edge-adjustment-on-move for this node itself (M4 replaces
    // the whole center-test move-children scheme; kept verbatim for M1).
    return CanvasNodeItem::itemChange(change, value);
}

void GroupItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    Q_EMIT labelEditRequested();
}

} // namespace Canvas
