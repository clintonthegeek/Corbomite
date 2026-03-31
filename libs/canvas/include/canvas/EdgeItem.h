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

private:
    TextCardItem *m_fromCard = nullptr;
    TextCardItem *m_toCard = nullptr;
    CanvasEdge m_data;
};

} // namespace Canvas
