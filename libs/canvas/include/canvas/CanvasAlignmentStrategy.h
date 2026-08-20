// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <graffodil/IAlignmentStrategy.h>
#include <QObject>
#include <Qt>

namespace Canvas {

/// Obsidian-parity move-drag alignment: grid snap + object (corner/center)
/// snap, installed on `Graffodil::SelectMoveTool` via
/// `setAlignmentStrategy()`. Also carries the Shift axis-lock (M4.2's move
/// piece — bundled here since both hook into the same `align()` callback).
///
/// See docs/superpowers/plans/2026-08-19-cluster-m-canvas-authoring-parity.md
/// Phase M4.1 + Appendix A (grid/object-snap constants) + Appendix B.
///
/// `IAlignmentStrategy` is a plain interface (no QObject base), so this
/// class multiply-inherits QObject purely so `CanvasScene` can construct it
/// with `this` as parent and get automatic cleanup, matching the ownership
/// idiom already used for every other scene-owned tool
/// (`m_resizeTool = new CanvasResizeTool(this);` etc. in CanvasScene.cpp).
class CanvasAlignmentStrategy : public QObject, public Graffodil::IAlignmentStrategy {
    Q_OBJECT
public:
    explicit CanvasAlignmentStrategy(QObject *parent = nullptr);

    void setSnapToGridEnabled(bool enabled);
    bool snapToGridEnabled() const { return m_snapToGrid; }

    void setSnapToObjectsEnabled(bool enabled);
    bool snapToObjectsEnabled() const { return m_snapToObjects; }

    Result align(Graffodil::IGraphNode *primary, const QPointF &proposed,
                 const QList<Graffodil::IGraphNode *> &allNodes) override;

    /// Call from CanvasScene's dragBegan/dragEnded handlers to force the
    /// free-running drag-position tracker (see m_freeDragPos) to reseed on
    /// the NEXT align() call, even if the next drag picks the same primary
    /// node again -- align() itself seeds m_freeDragPos from the primary's
    /// actual pos() whenever it sees a primary it doesn't recognize as the
    /// one it's already tracking, but without this reset, dragging the
    /// same node twice in a row would look like "no new drag" to that
    /// check and incorrectly carry over the previous drag's accumulated
    /// free position. Neither SelectMoveTool::dragBegan nor dragEnded
    /// hands us the primary node directly (only the full dragged set), so
    /// there's no primary to seed WITH here -- align() does that itself on
    /// its next call, using the real primary it's actually given.
    void endDrag();

    /// Appendix A grid-spacing ladder: pick the smallest of {20,40,80,160}
    /// scene units whose on-screen size (units * scale) is >= 10px, else
    /// 160. Pure function of the view scale — exposed static so
    /// `tst_canvas_alignment` can test it directly without a scene.
    static qreal gridSpacingForScale(qreal scale);

protected:
    /// Seam for tests: real Alt/Shift modifier state during a live drag can
    /// only be read via QGuiApplication::keyboardModifiers() (align() gets
    /// no event object), but that reflects genuine OS-level modifier state
    /// which an offscreen unit test cannot fake by driving events. Override
    /// this in a test subclass to inject fake modifier state instead.
    virtual Qt::KeyboardModifiers currentModifiers() const;

private:
    bool m_snapToGrid = true;
    bool m_snapToObjects = true;

    // SelectMoveTool computes `proposed` each frame from the PRIMARY NODE'S
    // CURRENT graphicsItem()->pos() plus this frame's raw mouse delta (see
    // SelectMoveTool::mouseMoveEvent). Once a frame snaps `result.position`
    // onto a candidate, pos() becomes pinned exactly at that candidate --
    // and every subsequent frame's `proposed` is then computed relative to
    // that PINNED position, not the drag's true (unsnapped) trajectory. As
    // long as each frame's raw mouse delta stays within the snap tolerance
    // (true for essentially all real mouse movement, which arrives in small
    // per-event increments), the search re-snaps right back to zero offset
    // every single frame -- pos() never advances again, no matter how far
    // the user actually drags, because the reference point it's measured
    // against never moves either. This was an inescapable magnet: once
    // caught, a drag could never release.
    //
    // Fix: track our own free-running (never-snapped) drag position,
    // seeded from the primary's pre-drag pos() in beginDrag() and advanced
    // every align() call by the same raw per-frame delta SelectMoveTool
    // itself derives (`proposed - primaryItem->pos()`) -- that delta is
    // always genuine fresh mouse movement regardless of whether pos() is
    // currently pinned, so m_freeDragPos keeps accumulating real total
    // displacement even while the applied position stays snapped. Snap
    // candidates are evaluated against m_freeDragPos instead of `proposed`
    // directly, so the search sees the drag's true distance from the
    // candidate and releases once that distance exceeds tolerance -- the
    // node then jumps to catch up in one frame, which is correct behavior
    // (a released magnet), not a bug.
    QPointF m_freeDragPos;
    Graffodil::IGraphNode *m_dragPrimary = nullptr;
};

} // namespace Canvas
