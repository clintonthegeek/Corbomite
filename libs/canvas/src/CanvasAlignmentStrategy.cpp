// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasAlignmentStrategy.h"

#include <graffodil/IGraphNode.h>

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGuiApplication>

#include <algorithm>
#include <cmath>

namespace Canvas {

CanvasAlignmentStrategy::CanvasAlignmentStrategy(QObject *parent)
    : QObject(parent)
{
}

void CanvasAlignmentStrategy::setSnapToGridEnabled(bool enabled)
{
    m_snapToGrid = enabled;
}

void CanvasAlignmentStrategy::setSnapToObjectsEnabled(bool enabled)
{
    m_snapToObjects = enabled;
}

Qt::KeyboardModifiers CanvasAlignmentStrategy::currentModifiers() const
{
    return QGuiApplication::keyboardModifiers();
}

qreal CanvasAlignmentStrategy::gridSpacingForScale(qreal scale)
{
    // Appendix A: 20 at scale >= 1, 40 at >= 0.5, 80 at >= 0.25, else 160 —
    // i.e. the smallest of {20,40,80,160} whose on-screen size (units *
    // scale) is >= ~10px.
    if (scale >= 1.0)
        return 20.0;
    if (scale >= 0.5)
        return 40.0;
    if (scale >= 0.25)
        return 80.0;
    return 160.0;
}

Graffodil::IAlignmentStrategy::Result
CanvasAlignmentStrategy::align(Graffodil::IGraphNode *primary, const QPointF &proposed,
                                const QList<Graffodil::IGraphNode *> &allNodes)
{
    Result result;
    result.position = proposed;

    if (!primary)
        return result;

    QGraphicsItem *primaryItem = primary->graphicsItem();
    if (!primaryItem || !primaryItem->scene() || primaryItem->scene()->views().isEmpty())
        return result;

    const Qt::KeyboardModifiers mods = currentModifiers();

    // No-snap modifier (Appendix A: Alt). Alt-at-press is already claimed
    // by M2.5's duplicate-drag route one layer up in CanvasScene's
    // CompositeTool routing — by the time align() runs mid-drag, an Alt
    // held here unambiguously means "no-snap for this frame."
    if (mods & Qt::AltModifier)
        return result;

    const qreal scale = primaryItem->scene()->views().first()->transform().m11();

    QPointF working = proposed;
    QLineF xGuide;
    QLineF yGuide;
    bool hasXGuide = false;
    bool hasYGuide = false;
    bool xMatched = false;
    bool yMatched = false;

    if (m_snapToGrid || m_snapToObjects) {
        const QRectF primaryRect(proposed, primary->nodeBoundingRect().size());

        if (m_snapToObjects) {
            const qreal tol = 15.0 / scale;
            qreal bestXAbs = tol;
            qreal bestXDelta = 0.0;
            qreal bestXCoord = 0.0;
            QRectF bestXOtherRect;
            qreal bestYAbs = tol;
            qreal bestYDelta = 0.0;
            qreal bestYCoord = 0.0;
            QRectF bestYOtherRect;

            for (auto *other : allNodes) {
                if (!other || other->nodeId() == primary->nodeId())
                    continue;
                QGraphicsItem *otherItem = other->graphicsItem();
                if (!otherItem)
                    continue;
                const QRectF otherRect = otherItem->sceneBoundingRect();

                const qreal xCandidates[3] = {otherRect.left(), otherRect.right(),
                                               otherRect.center().x()};
                const qreal xEdges[3] = {primaryRect.left(), primaryRect.right(),
                                          primaryRect.center().x()};
                for (qreal cx : xCandidates) {
                    for (qreal ex : xEdges) {
                        const qreal diff = cx - ex;
                        if (std::abs(diff) <= bestXAbs) {
                            bestXAbs = std::abs(diff);
                            bestXDelta = diff;
                            bestXCoord = cx;
                            bestXOtherRect = otherRect;
                            xMatched = true;
                        }
                    }
                }

                const qreal yCandidates[3] = {otherRect.top(), otherRect.bottom(),
                                               otherRect.center().y()};
                const qreal yEdges[3] = {primaryRect.top(), primaryRect.bottom(),
                                          primaryRect.center().y()};
                for (qreal cy : yCandidates) {
                    for (qreal ey : yEdges) {
                        const qreal diff = cy - ey;
                        if (std::abs(diff) <= bestYAbs) {
                            bestYAbs = std::abs(diff);
                            bestYDelta = diff;
                            bestYCoord = cy;
                            bestYOtherRect = otherRect;
                            yMatched = true;
                        }
                    }
                }
            }

            if (xMatched) {
                working.setX(working.x() + bestXDelta);
                const qreal top = std::min(primaryRect.top(), bestXOtherRect.top());
                const qreal bottom = std::max(primaryRect.bottom(), bestXOtherRect.bottom());
                xGuide = QLineF(QPointF(bestXCoord, top), QPointF(bestXCoord, bottom));
                hasXGuide = true;
            }
            if (yMatched) {
                working.setY(working.y() + bestYDelta);
                const qreal left = std::min(primaryRect.left(), bestYOtherRect.left());
                const qreal right = std::max(primaryRect.right(), bestYOtherRect.right());
                yGuide = QLineF(QPointF(left, bestYCoord), QPointF(right, bestYCoord));
                hasYGuide = true;
            }
        }

        // Object-snap wins over grid-snap when both apply on the same
        // axis (more specific/intentional alignment) — grid is only a
        // fallback per axis when no object-snap candidate matched.
        if (m_snapToGrid) {
            const qreal spacing = gridSpacingForScale(scale);
            if (!xMatched)
                working.setX(std::round(proposed.x() / spacing) * spacing);
            if (!yMatched)
                working.setY(std::round(proposed.y() / spacing) * spacing);
        }
    }

    // Shift axis-lock (M4.2). No drag-start position reaches align() (only
    // the per-frame `proposed`), so this locks to the DOMINANT AXIS OF THE
    // CURRENT FRAME's delta rather than the whole drag's — proposed minus
    // the primary's current actual position recovers exactly this frame's
    // raw mouse delta (see SelectMoveTool::mouseMoveEvent: `proposed =
    // graphicsItem()->pos() + (scenePos - lastScenePos)`). This is a
    // pragmatic per-frame interpretation: since consecutive frames within
    // one drag almost always share the same dominant direction (mouse
    // deltas don't flip axis every frame in practice), it reads the same
    // as a true drag-start-relative lock without needing this stateless
    // strategy to track its own drag-start snapshot. Applied LAST/after
    // snapping — Shift is a stronger, more deliberate user intent than
    // passive snap, so the locked axis is re-zeroed back to the primary's
    // current position even if grid/object snap had adjusted it, and any
    // guide on the locked axis is dropped since it no longer reflects the
    // applied position.
    if (mods & Qt::ShiftModifier) {
        const QPointF frameDelta = proposed - primaryItem->pos();
        if (std::abs(frameDelta.x()) >= std::abs(frameDelta.y())) {
            working.setY(primaryItem->pos().y());
            hasYGuide = false;
        } else {
            working.setX(primaryItem->pos().x());
            hasXGuide = false;
        }
    }

    result.position = working;
    if (hasXGuide)
        result.guides.append(xGuide);
    if (hasYGuide)
        result.guides.append(yGuide);
    return result;
}

} // namespace Canvas
