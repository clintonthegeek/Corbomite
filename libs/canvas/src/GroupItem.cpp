// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/GroupItem.h"
#include "canvas/CanvasScene.h"
#include <QPainter>

namespace Canvas {

GroupItem::GroupItem(const CanvasNode &data, CanvasScene *scene)
    : QGraphicsObject()
    , m_data(data)
    , m_canvasScene(scene)
{
    setPos(data.x, data.y);
}

QVector<QGraphicsItem *> GroupItem::containedItems() const
{
    return {}; // TODO: implement
}

void GroupItem::setNodeData(const CanvasNode &data)
{
    m_data = data;
    setPos(data.x, data.y);
    update();
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
    Q_UNUSED(option);
    Q_UNUSED(widget);
    Q_UNUSED(painter);
    // TODO: implement rendering
}

} // namespace Canvas
