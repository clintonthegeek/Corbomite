// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsObject>
#include "CanvasTypes.h"
#include "ConnectableItem.h"

namespace Canvas {

class TextCardItem : public QGraphicsObject, public ConnectableItem {
    Q_OBJECT

public:
    TextCardItem(const CanvasNode &data, QGraphicsItem *parent = nullptr);

    void setNodeData(const CanvasNode &data);
    CanvasNode nodeData() const;
    QString nodeId() const override;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // Edge connection points
    QPointF connectionPoint(Side side) const override;

    // ConnectableItem interface
    QGraphicsObject *asGraphicsObject() override { return this; }

    // Resize detection (Kdenlive pattern)
    enum ResizeMode { NoResize = 0, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };
    ResizeMode resizeModeAtPos(const QPointF &localPos) const;

Q_SIGNALS:
    void positionChanged();
    void sizeChanged();
    void editRequested();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    CanvasNode m_data;
};

} // namespace Canvas
