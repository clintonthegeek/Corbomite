// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasEdgeGestureTool.h"
#include "canvas/EdgeItem.h"
#include "canvas/ReconnectEdgeTool.h"

#include <graffodil/CreateEdgeTool.h>
#include <graffodil/GraphScene.h>

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QLineF>

namespace Canvas {

CanvasEdgeGestureTool::CanvasEdgeGestureTool(Graffodil::CreateEdgeTool *createEdgeTool,
                                              ReconnectEdgeTool *reconnectTool,
                                              QObject *parent)
    : Graffodil::GraphTool(parent)
    , m_createEdgeTool(createEdgeTool)
    , m_reconnectTool(reconnectTool)
{
}

void CanvasEdgeGestureTool::activate(Graffodil::GraphScene *scene)
{
    GraphTool::activate(scene);
    // CompositeTool::activate() already activates every distinct sub-tool
    // reachable from its routes, but m_createEdgeTool/m_reconnectTool are no
    // longer directly registered as routes (this wrapper is), so activate
    // them ourselves.
    if (m_createEdgeTool)
        m_createEdgeTool->activate(scene);
    if (m_reconnectTool)
        m_reconnectTool->activate(scene);
}

void CanvasEdgeGestureTool::deactivate()
{
    m_activeSubTool = nullptr;
    if (m_createEdgeTool)
        m_createEdgeTool->deactivate();
    if (m_reconnectTool)
        m_reconnectTool->deactivate();
    GraphTool::deactivate();
}

CanvasEdgeGestureTool::EndpointHit CanvasEdgeGestureTool::findEndpointNear(const QPointF &scenePos) const
{
    EndpointHit result;
    if (!m_scene)
        return result;

    qreal best = kEndpointHitRadius;
    for (auto *edge : m_scene->edges()) {
        // Concrete-type accessor cast, same precedent as CanvasScene's own
        // edgeItem()/groupItem()/connectableItem() helpers — this is a
        // single-hierarchy cast to reach EdgeItem::edgePoint(), not the
        // banned per-node-type behavior branching (plan Appendix B rule 4).
        auto *item = dynamic_cast<EdgeItem *>(edge);
        if (!item)
            continue;

        const qreal dSource = QLineF(item->edgePoint(0.0), scenePos).length();
        if (dSource <= best) {
            best = dSource;
            result.edge = edge;
            result.end = Graffodil::ArrowEnd::Source;
        }
        const qreal dTarget = QLineF(item->edgePoint(1.0), scenePos).length();
        if (dTarget <= best) {
            best = dTarget;
            result.edge = edge;
            result.end = Graffodil::ArrowEnd::Target;
        }
    }
    return result;
}

void CanvasEdgeGestureTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_activeSubTool = nullptr;
    if (event->button() != Qt::LeftButton || !m_scene)
        return;

    const EndpointHit hit = findEndpointNear(event->scenePos());
    if (hit.edge) {
        auto *item = dynamic_cast<EdgeItem *>(hit.edge);
        const QPointF fixedScenePos = item
            ? (hit.end == Graffodil::ArrowEnd::Source ? item->edgePoint(1.0) : item->edgePoint(0.0))
            : event->scenePos();
        m_activeSubTool = m_reconnectTool;
        if (m_reconnectTool)
            m_reconnectTool->beginReconnect(hit.edge, hit.end, fixedScenePos);
    } else {
        m_activeSubTool = m_createEdgeTool;
        if (m_createEdgeTool)
            m_createEdgeTool->mousePressEvent(event);
    }
}

void CanvasEdgeGestureTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeSubTool) {
        m_activeSubTool->mouseMoveEvent(event);
        return;
    }
    // Hover-style move with no button pressed: broadcast so CreateEdgeTool's
    // anchor-dot highlighting still updates before any press starts a
    // gesture (mirrors CompositeTool's own no-drag broadcast behavior).
    if (m_createEdgeTool)
        m_createEdgeTool->mouseMoveEvent(event);
}

void CanvasEdgeGestureTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeSubTool) {
        m_activeSubTool->mouseReleaseEvent(event);
        m_activeSubTool = nullptr;
    }
}

void CanvasEdgeGestureTool::keyPressEvent(QKeyEvent *event)
{
    if (m_activeSubTool)
        m_activeSubTool->keyPressEvent(event);
    else if (m_createEdgeTool)
        m_createEdgeTool->keyPressEvent(event);
}

} // namespace Canvas
