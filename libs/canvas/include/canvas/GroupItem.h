// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsObject>
#include "CanvasTypes.h"

namespace Canvas {

class CanvasScene;

class GroupItem : public QGraphicsObject {
    Q_OBJECT

public:
    GroupItem(const CanvasNode &data, CanvasScene *scene);

    QVector<QGraphicsItem *> containedItems() const;
    void setNodeData(const CanvasNode &data);
    QString nodeId() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    CanvasNode m_data;
    CanvasScene *m_canvasScene = nullptr;
};

} // namespace Canvas
