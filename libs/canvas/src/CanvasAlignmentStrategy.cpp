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

void CanvasAlignmentStrategy::endDrag()
{
    m_dragPrimary = nullptr;
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

    // Defensive reseed: if CanvasScene's dragBegan/dragEnded lifecycle
    // wiring didn't run for some reason (or primary changed mid-drag,
    // which shouldn't happen but isn't assumed), treat this as a fresh
    // drag rather than accumulating from a stale/foreign m_freeDragPos.
    if (m_dragPrimary != primary) {
        m_dragPrimary = primary;
        m_freeDragPos = primaryItem->pos();
    }

    // See m_freeDragPos's declaration for why this indirection exists: the
    // node's actual pos() can be pinned at a snap point, so `proposed`
    // (computed from pos()) can't be trusted as "how far the drag has
    // truly moved." The raw per-frame delta (proposed minus the item's
    // CURRENT pos(), pinned or not) is always genuine fresh mouse movement
    // though, so accumulating it here keeps a true, never-pinned running
    // position to search snap candidates against.
    const QPointF freeBeforeThisFrame = m_freeDragPos;
    m_freeDragPos += proposed - primaryItem->pos();

    QPointF working = m_freeDragPos;
    QLineF xGuide;
    QLineF yGuide;
    bool hasXGuide = false;
    bool hasYGuide = false;
    bool xMatched = false;
    bool yMatched = false;

    if (m_snapToGrid || m_snapToObjects) {
        const QRectF primaryRect(m_freeDragPos, primary->nodeBoundingRect().size());

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
                working.setX(std::round(m_freeDragPos.x() / spacing) * spacing);
            if (!yMatched)
                working.setY(std::round(m_freeDragPos.y() / spacing) * spacing);
        }
    }

    // Shift axis-lock (M4.2). Locks to the DOMINANT AXIS OF THE CURRENT
    // FRAME's delta rather than the whole drag's (see the per-frame vs.
    // drag-start discussion this comment used to carry, now superseded by
    // m_freeDragPos existing for the snap-pinning fix above — reusing that
    // same true free-running position as the re-zero reference here too,
    // rather than the primary's possibly-pinned pos(), keeps this
    // consistent with the fix instead of reintroducing the same kind of
    // stale-reference bug on the locked axis). Applied LAST/after snapping
    // — Shift is a stronger, more deliberate user intent than passive
    // snap, so the locked axis is re-zeroed back to the free position even
    // if grid/object snap had adjusted it, and any guide on the locked
    // axis is dropped since it no longer reflects the applied position.
    if (mods & Qt::ShiftModifier) {
        const QPointF frameDelta = proposed - primaryItem->pos();
        if (std::abs(frameDelta.x()) >= std::abs(frameDelta.y())) {
            working.setY(freeBeforeThisFrame.y());
            hasYGuide = false;
        } else {
            working.setX(freeBeforeThisFrame.x());
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
