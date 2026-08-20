// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsObject>
#include <QList>
#include <QPointF>

namespace Canvas {

class CanvasNodeItem;

/// M4.4 — single shared chrome overlay (8 resize handles + 4 connection
/// dots) retargeted to whichever node is currently "active" (the sole
/// selected node, or the sole hovered node when nothing is selected).
/// Replaces the old per-item paint() blocks that used to draw resize
/// handles directly in TextCardItem/FileCardItem/GroupItem (not
/// zoom-constant, triplicated) — see plan Phase M4.4 and Appendix A "Node
/// chrome" row.
///
/// Zoom-constancy mechanism: this item sets `ItemIgnoresTransformations`,
/// so its own local coordinate system is NOT scaled by the view's zoom —
/// 1 local unit == 1 device pixel. `setPos()` still places the item's
/// ORIGIN via the normal scene->view transform, so retarget()/syncToTarget()
/// position that origin at the target node's current
/// `sceneBoundingRect().topLeft()`, and boundingRect()/paint() compute the
/// node's on-screen size fresh each call as
/// `target->sceneBoundingRect().size() * currentViewScale()` — i.e. the
/// handle/dot positions are recomputed in screen-pixel space every repaint,
/// not baked in at retarget time. This means a plain viewport repaint
/// (which QGraphicsView triggers on any transform change) keeps the chrome
/// visually locked to the node's on-screen corners across zoom changes
/// without this class needing to know about pan/zoom itself.
///
/// Connection-dot positions come from `CanvasNodeItem::anchors()` (the same
/// `Graffodil::IGraphNode::anchors()` data `CreateEdgeTool`'s anchor-hover
/// hit-test already uses) rather than being re-derived from the bounding
/// rect independently — see connectionDotScenePositions().
class CanvasNodeChromeOverlay : public QGraphicsObject {
    Q_OBJECT

public:
    explicit CanvasNodeChromeOverlay(QGraphicsItem *parent = nullptr);

    /// Retarget the overlay to `node`. `showHandles` draws the 8
    /// resize-handle squares; `showDots` draws the 4 connection-point
    /// dots (from `node->anchors()`). Passing `node == nullptr` (or both
    /// flags false) hides the overlay — same as calling clear().
    void retarget(CanvasNodeItem *node, bool showHandles, bool showDots);

    /// Hide the overlay and drop the target reference. Call this before a
    /// targeted node is destroyed to avoid a dangling pointer.
    void clear();

    /// Re-read the target's current scene geometry and reposition/repaint.
    /// Called on retarget() and whenever the target's geometryChanged()
    /// signal fires (move/resize while it's the active chrome target).
    void syncToTarget();

    CanvasNodeItem *target() const { return m_target; }
    bool handlesVisible() const { return m_showHandles; }
    bool dotsVisible() const { return m_showDots; }

    /// Scene positions of the 4 connection dots — literally
    /// `target->anchors()`'s `scenePos` values, exposed so tests can assert
    /// there is no drift between the drawn dots and
    /// `Graffodil::CreateEdgeTool`'s anchor-hover hit-test (both read the
    /// same `anchors()` call). Empty if there is no target.
    QList<QPointF> connectionDotScenePositions() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    qreal currentScale() const;

    CanvasNodeItem *m_target = nullptr;
    bool m_showHandles = false;
    bool m_showDots = false;
};

} // namespace Canvas
