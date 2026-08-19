// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CanvasNodeItem.h"

namespace Canvas {

class GroupItem : public CanvasNodeItem {
    Q_OBJECT

public:
    GroupItem(const CanvasNode &data, QGraphicsItem *parent = nullptr);

    void setNodeData(const CanvasNode &data) override;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QVector<QGraphicsItem *> containedItems() const;

Q_SIGNALS:
    void labelEditRequested();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QPointF m_lastPos;
    bool m_movingChildren = false;
};

} // namespace Canvas
