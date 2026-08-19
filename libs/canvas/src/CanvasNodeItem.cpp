// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasNodeItem.h"

#include <graffodil/Anchors.h>
#include <graffodil/GraphScene.h>
#include <QGraphicsSceneMouseEvent>

namespace Canvas {

static constexpr qreal kResizeZone = 8.0;

CanvasNodeItem::CanvasNodeItem(const CanvasNode &data, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_data(data)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setPos(data.x, data.y);
}

QString CanvasNodeItem::nodeId() const
{
    return m_data.id;
}

QList<Graffodil::Anchor> CanvasNodeItem::anchors() const
{
    return Graffodil::compassAnchors(sceneBoundingRect());
}

QRectF CanvasNodeItem::nodeBoundingRect() const
{
    return sceneBoundingRect();
}

void CanvasNodeItem::setGeometry(const QRectF &rect)
{
    prepareGeometryChange();
    m_data.x = qRound(rect.x());
    m_data.y = qRound(rect.y());
    m_data.width = qRound(rect.width());
    m_data.height = qRound(rect.height());
    setPos(m_data.x, m_data.y);
    update();
    // setPos() above only triggers itemChange(ItemPositionHasChanged) — and
    // thus edge re-adjustment — when the position actually changed. A
    // right/bottom-edge resize changes width/height with the position
    // unchanged, which would leave edges stale without this explicit call.
    if (auto *graphScene = qobject_cast<Graffodil::GraphScene *>(scene()))
        graphScene->adjustEdgesForNode(nodeId());
}

void CanvasNodeItem::setNodeData(const CanvasNode &data)
{
    prepareGeometryChange();
    m_data = data;
    setPos(data.x, data.y);
    update();
}

CanvasNode CanvasNodeItem::nodeData() const
{
    return m_data;
}

CanvasNodeItem::ResizeMode CanvasNodeItem::resizeModeAtPos(const QPointF &localPos) const
{
    const QRectF rect(0, 0, m_data.width, m_data.height);
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

QVariant CanvasNodeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        // GraphScene does not self-subscribe to node position changes for
        // edge adjustment (only groups get that treatment, via the scene's
        // changed() signal). SelectMoveTool's interactive drag already calls
        // adjustEdgesForNode() per moved node, but programmatic position
        // writes (undo/redo replaying CmdMoveCards/CmdResizeCard) do not go
        // through the tool, so this hook is the only thing that keeps edges
        // attached to their nodes in that path. adjustEdgesForNode() /
        // GraphEdgeItem::adjust() are idempotent, so the occasional double
        // call during an interactive drag is harmless. See spec §6a V2.
        if (auto *graphScene = qobject_cast<Graffodil::GraphScene *>(scene()))
            graphScene->adjustEdgesForNode(nodeId());
    }
    return QGraphicsObject::itemChange(change, value);
}

void CanvasNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    Q_EMIT editRequested();
}

} // namespace Canvas
