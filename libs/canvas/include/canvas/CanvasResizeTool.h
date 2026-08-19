// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <graffodil/GraphTool.h>
#include <QRectF>
#include "CanvasNodeItem.h"

namespace Graffodil {
class GraphScene;
}

namespace Canvas {

/// Find the topmost *selected* CanvasNodeItem whose resize zone contains
/// scenePos, or nullptr if none. Shared between the CompositeTool routing
/// predicate (CanvasScene ctor) and CanvasResizeTool's own press handling,
/// so both agree on exactly what counts as "starting a resize" — see
/// spec §3.5's resizeHit() contract (resize zones only act on an
/// already-selected item, matching pre-M1 UX).
CanvasNodeItem *findResizeTarget(Graffodil::GraphScene *scene,
                                  const QPointF &scenePos,
                                  CanvasNodeItem::ResizeMode *modeOut = nullptr);

/// Consumer resize tool — Graffodil ships no resize tool. Verbatim port of
/// the pre-M1 SelectMoveTool::DragMode::Resize math (same 8-mode geometry,
/// same kMinSize=40 clamp), routed into the scene's CompositeTool ahead of
/// SelectMoveTool. Signal-only: never touches CanvasDocument directly (see
/// spec §3.6 / plan Appendix B rule 1) — CanvasScene pushes CmdResizeCard
/// from resizeCommitted().
class CanvasResizeTool : public Graffodil::GraphTool {
    Q_OBJECT

public:
    using Graffodil::GraphTool::GraphTool;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void deactivate() override;

Q_SIGNALS:
    void resizeCommitted(const QString &nodeId, const QRect &oldRect, const QRect &newRect);

private:
    CanvasNodeItem *m_item = nullptr;
    CanvasNodeItem::ResizeMode m_mode = CanvasNodeItem::NoResize;
    QPointF m_pressScenePos;
    QRectF m_originalRect;   // scene-relative (pos.x, pos.y, w, h)
};

} // namespace Canvas
