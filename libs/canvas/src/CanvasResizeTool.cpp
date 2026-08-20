// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasResizeTool.h"

#include <graffodil/GraphScene.h>
#include <QGraphicsSceneMouseEvent>

namespace Canvas {

static constexpr qreal kMinSize = 40.0;

CanvasNodeItem *findResizeTarget(Graffodil::GraphScene *scene,
                                  const QPointF &scenePos,
                                  CanvasNodeItem::ResizeMode *modeOut)
{
    if (modeOut)
        *modeOut = CanvasNodeItem::NoResize;
    if (!scene)
        return nullptr;

    // Topmost selected node whose resize zone contains scenePos. Only
    // selected items offer resize handles (matches pre-M1 UX: you select,
    // then drag a handle — a raw press on an unselected item's edge starts
    // a select/move gesture instead).
    CanvasNodeItem *best = nullptr;
    qreal bestZ = 0.0;
    bool haveBest = false;
    for (auto *node : scene->nodes()) {
        auto *item = dynamic_cast<CanvasNodeItem *>(node);
        if (!item || !item->isSelected())
            continue;
        const QPointF localPos = item->mapFromScene(scenePos);
        if (!item->boundingRect().contains(localPos))
            continue;
        const auto mode = item->resizeModeAtPos(localPos);
        if (mode == CanvasNodeItem::NoResize)
            continue;
        if (!haveBest || item->zValue() > bestZ) {
            best = item;
            bestZ = item->zValue();
            haveBest = true;
            if (modeOut)
                *modeOut = mode;
        }
    }
    return best;
}

void CanvasResizeTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_scene)
        return;

    CanvasNodeItem::ResizeMode mode = CanvasNodeItem::NoResize;
    CanvasNodeItem *item = findResizeTarget(m_scene, event->scenePos(), &mode);
    if (!item || mode == CanvasNodeItem::NoResize)
        return;

    m_item = item;
    m_mode = mode;
    m_pressScenePos = event->scenePos();
    m_originalRect = QRectF(item->pos(), QSizeF(item->nodeData().width, item->nodeData().height));
}

void CanvasResizeTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_item)
        return;

    const QPointF delta = event->scenePos() - m_pressScenePos;

    qreal newX = m_originalRect.x();
    qreal newY = m_originalRect.y();
    qreal newW = m_originalRect.width();
    qreal newH = m_originalRect.height();

    // ResizeMode values: TopLeft=1, Top=2, TopRight=3, Right=4,
    // BottomRight=5, Bottom=6, BottomLeft=7, Left=8 (CanvasNodeItem::ResizeMode).
    const bool resizeLeft   = (m_mode == CanvasNodeItem::TopLeft || m_mode == CanvasNodeItem::BottomLeft || m_mode == CanvasNodeItem::Left);
    const bool resizeRight  = (m_mode == CanvasNodeItem::TopRight || m_mode == CanvasNodeItem::Right || m_mode == CanvasNodeItem::BottomRight);
    const bool resizeTop    = (m_mode == CanvasNodeItem::TopLeft || m_mode == CanvasNodeItem::Top || m_mode == CanvasNodeItem::TopRight);
    const bool resizeBottom = (m_mode == CanvasNodeItem::BottomRight || m_mode == CanvasNodeItem::Bottom || m_mode == CanvasNodeItem::BottomLeft);

    if (resizeLeft) {
        newX += delta.x();
        newW -= delta.x();
    }
    if (resizeRight) {
        newW += delta.x();
    }
    if (resizeTop) {
        newY += delta.y();
        newH -= delta.y();
    }
    if (resizeBottom) {
        newH += delta.y();
    }

    // M4.2 — Shift aspect-lock. Only meaningful on a CORNER handle (an
    // x-side AND a y-side are both dragging); a single-edge handle
    // (Top/Right/Bottom/Left alone) only changes one dimension, so there's
    // nothing to lock. Pick whichever dimension moved proportionally more
    // this frame and recompute the other to match m_originalRect's aspect
    // ratio, then re-derive the anchored coordinate the same way the
    // min-size clamp below does (subtract the size delta from the
    // dragged-edge coordinate when that edge is the one moving). Applied
    // BEFORE the min-size clamp so kMinSize still gets final say.
    const bool isCornerResize = (resizeLeft || resizeRight) && (resizeTop || resizeBottom);
    if (isCornerResize && (event->modifiers() & Qt::ShiftModifier)
        && m_originalRect.width() > 0 && m_originalRect.height() > 0) {
        const qreal aspect = m_originalRect.width() / m_originalRect.height();
        const qreal wRatio = qAbs(newW - m_originalRect.width()) / m_originalRect.width();
        const qreal hRatio = qAbs(newH - m_originalRect.height()) / m_originalRect.height();
        if (wRatio >= hRatio) {
            const qreal adjH = newW / aspect;
            if (resizeTop)
                newY -= (adjH - newH);
            newH = adjH;
        } else {
            const qreal adjW = newH * aspect;
            if (resizeLeft)
                newX -= (adjW - newW);
            newW = adjW;
        }
    }

    // Enforce minimum size
    if (newW < kMinSize) {
        if (resizeLeft)
            newX -= (kMinSize - newW);
        newW = kMinSize;
    }
    if (newH < kMinSize) {
        if (resizeTop)
            newY -= (kMinSize - newH);
        newH = kMinSize;
    }

    m_item->setGeometry(QRectF(qRound(newX), qRound(newY), qRound(newW), qRound(newH)));
}

void CanvasResizeTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    if (!m_item)
        return;

    const QRect oldRect(qRound(m_originalRect.x()), qRound(m_originalRect.y()),
                        qRound(m_originalRect.width()), qRound(m_originalRect.height()));
    const CanvasNode data = m_item->nodeData();
    const QRect newRect(data.x, data.y, data.width, data.height);

    if (oldRect != newRect)
        Q_EMIT resizeCommitted(data.id, oldRect, newRect);

    m_item = nullptr;
    m_mode = CanvasNodeItem::NoResize;
}

void CanvasResizeTool::deactivate()
{
    m_item = nullptr;
    m_mode = CanvasNodeItem::NoResize;
    GraphTool::deactivate();
}

} // namespace Canvas
