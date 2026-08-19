// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <graffodil/GraphTool.h>
#include <QHash>
#include <QPointF>
#include <QString>
#include <QVector>
#include "CanvasNodeItem.h"

namespace Graffodil {
class GraphScene;
}

namespace Canvas {

class CanvasScene;

/// Find the topmost already-selected CanvasNodeItem under scenePos, or
/// nullptr. Used by the CompositeTool routing predicate for Alt-drag
/// duplicate (M2.5): the gesture only starts when Alt is held AND the
/// press lands on a node that is already part of the current selection —
/// mirrors CanvasResizeTool's findResizeTarget() precedent.
CanvasNodeItem *findAltDragDuplicateTarget(Graffodil::GraphScene *scene, const QPointF &scenePos);

/// M2.5 — Alt-drag duplicate. Press-with-Alt on an already-selected node
/// clones the whole selection (fresh 16-hex ids; edges between two
/// selected nodes are cloned too, endpoints remapped) and drags the
/// CLONES instead of the originals, leaving the source nodes untouched.
///
/// The clone graphics items are created directly in the scene at press
/// time (visual only — CanvasScene::add*ItemToScene() calls do not write
/// to CanvasDocument) and committed as a single compound undo command
/// (CmdAddCard/CmdAddEdge) on release. This is the same "tool drives the
/// visible state live, Cmd*::redo() is the only document write"
/// discipline CanvasResizeTool established for geometry, extended here
/// to item creation (spec Appendix B rule 1).
class CanvasDuplicateDragTool : public Graffodil::GraphTool {
    Q_OBJECT

public:
    using Graffodil::GraphTool::GraphTool;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void deactivate() override;

private:
    CanvasScene *m_canvasScene = nullptr;
    QPointF m_pressScenePos;
    bool m_dragging = false;

    // Clone node id -> its live CanvasNodeItem (already added to the scene).
    QHash<QString, CanvasNodeItem *> m_clones;
    // Clone node id -> its press-time (pre-drag) position.
    QHash<QString, QPointF> m_cloneStartPositions;
    // Clone edge data, endpoints already remapped to clone ids.
    QVector<CanvasEdge> m_cloneEdges;
};

} // namespace Canvas
