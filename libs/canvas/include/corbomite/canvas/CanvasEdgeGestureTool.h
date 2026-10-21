// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <graffodil/GraphTool.h>
#include <graffodil/Types.h>

namespace Graffodil {
class CreateEdgeTool;
class IGraphEdge;
}

namespace Corbomite::Canvas {

class ReconnectEdgeTool;

/// Dispatcher — routes a single anchor-proximity press to either
/// edge-CREATE (Graffodil::CreateEdgeTool, unchanged) or
/// edge-RECONNECT (ReconnectEdgeTool, new) depending on whether the press
/// landed within 8px of an *existing edge's* terminus point.
///
/// Why this exists instead of two ordinary CompositeTool routes: routing an
/// edge terminus. Graffodil::CompositeTool::addAnchorRoute() always
/// prepends and always uses the same generic "near any node anchor"
/// predicate — it has no notion of "is there an edge attached here", and a
/// second addAnchorRoute() call would just re-register the identical
/// node-anchor test, not an edge-terminus one. A plain addMouseRoute() can
/// never be tried ahead of an addAnchorRoute()-registered route regardless
/// of registration order (CompositeTool::mousePressEvent stops at the first
/// matcher that returns true, no fallthrough on decline). So instead of
/// fighting that structure, this single tool owns both gestures internally
/// and is itself registered as ONE addAnchorRoute() target — see
/// CanvasScene's ctor.
class CanvasEdgeGestureTool : public Graffodil::GraphTool {
    Q_OBJECT

public:
    /// Takes non-owning pointers to the two sub-tools; CanvasScene continues
    /// to own both (same lifetime pattern as CompositeTool's other routes —
    /// sub-tools are scene-parented QObjects, not owned by the router).
    CanvasEdgeGestureTool(Graffodil::CreateEdgeTool *createEdgeTool,
                          ReconnectEdgeTool *reconnectTool,
                          QObject *parent = nullptr);

    void activate(Graffodil::GraphScene *scene) override;
    void deactivate() override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    /// Endpoint hit-test radius (8px), independent of
    /// CreateEdgeTool's 12px node-anchor hover radius.
    static constexpr qreal kEndpointHitRadius = 8.0;

    /// Read-only lock. CompositeTool::addAnchorRoute() has no predicate
    /// overload (and no way to un-register a route), so this tool can't have
    /// its route withdrawn on toggle the way CanvasScene's own resize and
    /// Alt-duplicate routes do — it has to decline the events instead.
    /// Collateral effect, matching Obsidian (`onConnectionPointerdown`'s
    /// `!n.readonly` guard at app/own/2028-n9.js:192): CompositeTool keeps
    /// the press for whichever route matched first, so a press within 12px
    /// of an anchor neither creates an edge nor reaches the select tool.
    void setReadOnly(bool readOnly) { m_readOnly = readOnly; }
    bool isReadOnly() const { return m_readOnly; }

private:
    struct EndpointHit {
        Graffodil::IGraphEdge *edge = nullptr;
        Graffodil::ArrowEnd end = Graffodil::ArrowEnd::Target;
    };
    EndpointHit findEndpointNear(const QPointF &scenePos) const;

    Graffodil::CreateEdgeTool *m_createEdgeTool;
    ReconnectEdgeTool *m_reconnectTool;
    Graffodil::GraphTool *m_activeSubTool = nullptr; // whichever got the press, until release
    bool m_readOnly = false;
};

} // namespace Corbomite::Canvas
