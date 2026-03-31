// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/GroupItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

namespace Canvas {

static constexpr qreal kLabelPadding = 8.0;
static constexpr qreal kHandleSize = 6.0;
static constexpr qreal kResizeZone = 8.0;

GroupItem::GroupItem(const CanvasNode &data, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_data(data)
    , m_lastPos(data.x, data.y)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(-1);
    setPos(data.x, data.y);
}

void GroupItem::setNodeData(const CanvasNode &data)
{
    prepareGeometryChange();
    m_data = data;
    setPos(data.x, data.y);
    m_lastPos = pos();
    update();
}

CanvasNode GroupItem::nodeData() const
{
    return m_data;
}

QString GroupItem::nodeId() const
{
    return m_data.id;
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

QPointF GroupItem::connectionPoint(Side side) const
{
    const QRectF rect = boundingRect();
    QPointF local;
    switch (side) {
    case Side::Top:
        local = QPointF(rect.width() / 2.0, 0);
        break;
    case Side::Right:
        local = QPointF(rect.width(), rect.height() / 2.0);
        break;
    case Side::Bottom:
        local = QPointF(rect.width() / 2.0, rect.height());
        break;
    case Side::Left:
        local = QPointF(0, rect.height() / 2.0);
        break;
    }
    return mapToScene(local);
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
        if (qgraphicsitem_cast<GroupItem *>(item))
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

GroupItem::ResizeMode GroupItem::resizeModeAtPos(const QPointF &localPos) const
{
    const QRectF rect = boundingRect();
    const qreal x = localPos.x();
    const qreal y = localPos.y();
    const qreal w = rect.width();
    const qreal h = rect.height();

    const bool nearLeft   = x < kResizeZone;
    const bool nearRight  = x > w - kResizeZone;
    const bool nearTop    = y < kResizeZone;
    const bool nearBottom = y > h - kResizeZone;

    // Corners first (have priority)
    if (nearTop && nearLeft)     return TopLeft;
    if (nearTop && nearRight)    return TopRight;
    if (nearBottom && nearRight) return BottomRight;
    if (nearBottom && nearLeft)  return BottomLeft;

    // Edges
    if (nearTop)    return Top;
    if (nearRight)  return Right;
    if (nearBottom) return Bottom;
    if (nearLeft)   return Left;

    return NoResize;
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

        Q_EMIT positionChanged();
    }
    return QGraphicsObject::itemChange(change, value);
}

void GroupItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    Q_EMIT labelEditRequested();
}

} // namespace Canvas
