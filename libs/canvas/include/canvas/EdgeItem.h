// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsPathItem>
#include "CanvasTypes.h"

namespace Canvas {

class TextCardItem;

class EdgeItem : public QGraphicsPathItem {
public:
    EdgeItem(TextCardItem *fromCard, TextCardItem *toCard, const CanvasEdge &data, QGraphicsItem *parent = nullptr);

    void adjust();
    void setEdgeData(const CanvasEdge &data);
    CanvasEdge edgeData() const;
    QString edgeId() const;
    TextCardItem *sourceCard() const;
    TextCardItem *targetCard() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    void drawArrowHead(QPainterPath &path, const QPointF &tip, const QPointF &from) const;

    CanvasEdge m_data;
    TextCardItem *m_source;
    TextCardItem *m_target;
};

} // namespace Canvas
