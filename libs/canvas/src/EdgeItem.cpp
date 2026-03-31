// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/EdgeItem.h"
#include "canvas/TextCardItem.h"

namespace Canvas {

EdgeItem::EdgeItem(TextCardItem *fromCard, TextCardItem *toCard, const CanvasEdge &data, QGraphicsItem *parent)
    : QGraphicsPathItem(parent)
    , m_fromCard(fromCard)
    , m_toCard(toCard)
    , m_data(data)
{
}

void EdgeItem::adjust()
{
    // TODO: implement path computation
}

void EdgeItem::setEdgeData(const CanvasEdge &data)
{
    m_data = data;
    adjust();
}

CanvasEdge EdgeItem::edgeData() const
{
    return m_data;
}

QString EdgeItem::edgeId() const
{
    return m_data.id;
}

} // namespace Canvas
