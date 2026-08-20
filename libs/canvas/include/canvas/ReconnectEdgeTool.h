// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <graffodil/GraphTool.h>
#include <graffodil/Types.h>
#include <QPointer>
#include <QPointF>
#include <QString>

class QGraphicsLineItem;

namespace Graffodil {
class IGraphEdge;
class IGraphNode;
class AnchorHighlight;
}

namespace Canvas {

/// M3.4 — drag an existing edge's endpoint onto a different node (or drop it
/// on empty space to delete the edge). Mirrors Graffodil::CreateEdgeTool's
/// preview mechanics (dashed line + AnchorHighlight snapping) but the
/// gesture is initiated externally: CanvasEdgeGestureTool hit-tests for "is
/// the press within 8px of an existing edge's terminus" *before* routing
/// into either this tool or CreateEdgeTool (see CanvasEdgeGestureTool for
/// why that dispatch can't be expressed as two ordinary CompositeTool
/// routes), and calls beginReconnect() to hand off the gesture.
///
/// Signal-only: never touches CanvasDocument directly (plan Appendix B rule
/// 1) — CanvasScene turns reconnectRequested()/reconnectDroppedOnEmpty()
/// into CmdReconnectEdge / CmdRemoveEdge.
class ReconnectEdgeTool : public Graffodil::GraphTool {
    Q_OBJECT

public:
    explicit ReconnectEdgeTool(QObject *parent = nullptr);
    ~ReconnectEdgeTool() override;

    void activate(Graffodil::GraphScene *scene) override;
    void deactivate() override;

    /// Called by CanvasEdgeGestureTool once it has decided a press landed
    /// on `edge`'s `movingEnd` terminus. `fixedEndScenePos` is the scene
    /// position of the OTHER (unmoved) endpoint, used as the preview line's
    /// anchor — computed by the caller via EdgeItem::edgePoint() since this
    /// tool only sees the Graffodil-level IGraphEdge interface.
    void beginReconnect(Graffodil::IGraphEdge *edge, Graffodil::ArrowEnd movingEnd,
                         const QPointF &fixedEndScenePos);

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

Q_SIGNALS:
    /// The dragged endpoint was released over `newNode`'s `newAnchorId`
    /// anchor. CanvasScene builds an updated CanvasEdge (moved end's
    /// fromNode/fromSide or toNode/toSide) and pushes CmdReconnectEdge.
    void reconnectRequested(const QString &edgeId, Graffodil::ArrowEnd end,
                             Graffodil::IGraphNode *newNode, const QString &newAnchorId);

    /// The dragged endpoint was released over empty canvas — Obsidian
    /// semantics: delete the edge (CanvasScene pushes CmdRemoveEdge).
    void reconnectDroppedOnEmpty(const QString &edgeId);

private:
    Graffodil::IGraphNode *findNodeAt(const QPointF &scenePos) const;
    void updatePreview(const QPointF &cursorScenePos);
    void cancel();

    Graffodil::IGraphEdge *m_edge = nullptr;
    Graffodil::ArrowEnd m_movingEnd = Graffodil::ArrowEnd::Target;
    Graffodil::IGraphNode *m_otherNode = nullptr; // the fixed end's node (self-loop guard)
    QPointF m_fixedScenePos;
    QGraphicsLineItem *m_previewLine = nullptr;
    QPointer<Graffodil::AnchorHighlight> m_highlight;
};

} // namespace Canvas
