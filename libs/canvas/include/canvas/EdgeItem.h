// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsPathItem>
#include "CanvasTypes.h"

namespace Canvas {

class ConnectableItem;

class EdgeItem : public QGraphicsPathItem {
public:
    EdgeItem(ConnectableItem *fromCard, ConnectableItem *toCard, const CanvasEdge &data, QGraphicsItem *parent = nullptr);

    void adjust();
    void setEdgeData(const CanvasEdge &data);
    CanvasEdge edgeData() const;
    QString edgeId() const;
    ConnectableItem *sourceCard() const;
    ConnectableItem *targetCard() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    void drawArrowHead(QPainterPath &path, const QPointF &tip, const QPointF &from) const;

    CanvasEdge m_data;
    ConnectableItem *m_source;
    ConnectableItem *m_target;
};

} // namespace Canvas
