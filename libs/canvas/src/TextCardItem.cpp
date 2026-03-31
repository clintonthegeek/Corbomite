// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/TextCardItem.h"
#include "canvas/CanvasScene.h"
#include <QPainter>

namespace Canvas {

TextCardItem::TextCardItem(const CanvasNode &data, CanvasScene *scene)
    : QGraphicsObject()
    , m_data(data)
    , m_canvasScene(scene)
{
    setPos(data.x, data.y);
}

void TextCardItem::setNodeData(const CanvasNode &data)
{
    m_data = data;
    setPos(data.x, data.y);
    update();
}

CanvasNode TextCardItem::nodeData() const
{
    return m_data;
}

QString TextCardItem::nodeId() const
{
    return m_data.id;
}

QRectF TextCardItem::boundingRect() const
{
    return QRectF(0, 0, m_data.width, m_data.height);
}

void TextCardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    Q_UNUSED(painter);
    // TODO: implement rendering
}

TextCardItem::ResizeMode TextCardItem::resizeModeAtPos(const QPointF &pos) const
{
    Q_UNUSED(pos);
    return NoResize; // TODO: implement
}

QPointF TextCardItem::connectionPoint(Side side) const
{
    QRectF rect = boundingRect();
    switch (side) {
    case Side::Top: return QPointF(rect.center().x(), rect.top());
    case Side::Right: return QPointF(rect.right(), rect.center().y());
    case Side::Bottom: return QPointF(rect.center().x(), rect.bottom());
    case Side::Left: return QPointF(rect.left(), rect.center().y());
    }
    return rect.center();
}

} // namespace Canvas
