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
};

} // namespace Canvas
