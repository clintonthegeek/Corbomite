// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/ReconnectEdgeTool.h"

#include <graffodil/AnchorHighlight.h>
#include <graffodil/GraphScene.h>
#include <graffodil/IGraphEdge.h>
#include <graffodil/IGraphNode.h>

#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QLineF>

namespace Canvas {

ReconnectEdgeTool::ReconnectEdgeTool(QObject *parent)
    : Graffodil::GraphTool(parent)
    , m_highlight(new Graffodil::AnchorHighlight)
{
}

ReconnectEdgeTool::~ReconnectEdgeTool()
{
    // Same QPointer-guarded teardown as CreateEdgeTool: if the scene was
    // destroyed first, the pointer is already cleared and there is nothing
    // to take back.
    if (!m_highlight)
        return;
    if (m_highlight->scene())
        m_highlight->scene()->removeItem(m_highlight);
    delete m_highlight;
}

void ReconnectEdgeTool::activate(Graffodil::GraphScene *scene)
{
    GraphTool::activate(scene);
    if (m_highlight && m_highlight->scene() != scene)
        scene->addItem(m_highlight);
    if (m_highlight)
        m_highlight->hideAnchors();
}

void ReconnectEdgeTool::deactivate()
{
    cancel();
    if (m_highlight && m_highlight->scene())
        m_highlight->scene()->removeItem(m_highlight);
    GraphTool::deactivate();
}

Graffodil::IGraphNode *ReconnectEdgeTool::findNodeAt(const QPointF &scenePos) const
{
    if (!m_scene)
        return nullptr;
    const auto items = m_scene->items(scenePos);
    for (auto *item : items) {
        for (auto *node : m_scene->nodes()) {
            if (node->graphicsItem() == item || node->graphicsItem()->isAncestorOf(item))
                return node;
        }
    }
    return nullptr;
}

void ReconnectEdgeTool::beginReconnect(Graffodil::IGraphEdge *edge, Graffodil::ArrowEnd movingEnd,
                                        const QPointF &fixedEndScenePos)
{
    cancel();
    if (!edge || !m_scene)
        return;

    m_edge = edge;
    m_movingEnd = movingEnd;
    m_fixedScenePos = fixedEndScenePos;
    m_otherNode = (movingEnd == Graffodil::ArrowEnd::Target) ? edge->sourceNode() : edge->targetNode();

    m_previewLine = m_scene->addLine(QLineF(m_fixedScenePos, m_fixedScenePos),
                                      QPen(QColor(0, 120, 215), 1.5, Qt::DashLine));
    m_previewLine->setZValue(1000);
}

void ReconnectEdgeTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // No-op: the gesture is started externally via beginReconnect(), called
    // by CanvasEdgeGestureTool once it decides a press landed on an edge
    // terminus rather than forwarding the press itself.
    Q_UNUSED(event);
}

void ReconnectEdgeTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    updatePreview(event->scenePos());
}

void ReconnectEdgeTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_edge) {
        cancel();
        return;
    }

    const QString edgeId = m_edge->edgeId();
    const Graffodil::ArrowEnd end = m_movingEnd;

    Graffodil::IGraphNode *target = findNodeAt(event->scenePos());
    if (!target || target == m_otherNode) {
        // Empty space, or dropped back onto the fixed end's own node
        // (degenerate self-loop) — Obsidian semantics: dropping an
        // endpoint on nothing deletes the edge. Dropping it back on the
        // same node it's already tied to is treated the same way rather
        // than silently creating a self-loop.
        cancel();
        Q_EMIT reconnectDroppedOnEmpty(edgeId);
        return;
    }

    const Graffodil::Anchor anchor = target->anchorToward(event->scenePos());
    cancel();
    Q_EMIT reconnectRequested(edgeId, end, target, anchor.id);
}

void ReconnectEdgeTool::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        cancel();
}

void ReconnectEdgeTool::updatePreview(const QPointF &cursorScenePos)
{
    Graffodil::IGraphNode *hovered = findNodeAt(cursorScenePos);
    if (hovered == m_otherNode)
        hovered = nullptr; // don't snap-highlight the node we'd treat as a cancel

    Graffodil::Anchor hoverAnchor;
    if (hovered)
        hoverAnchor = hovered->anchorToward(cursorScenePos);

    QPointF end = cursorScenePos;
    if (hovered)
        end = hoverAnchor.scenePos;

    if (m_previewLine)
        m_previewLine->setLine(QLineF(m_fixedScenePos, end));

    if (m_highlight) {
        if (hovered) {
            m_highlight->showAnchors(hovered);
            m_highlight->setActiveAnchor(hoverAnchor.id);
        } else {
            m_highlight->hideAnchors();
        }
    }
}

void ReconnectEdgeTool::cancel()
{
    m_edge = nullptr;
    m_otherNode = nullptr;
    if (m_previewLine && m_scene) {
        m_scene->removeItem(m_previewLine);
        delete m_previewLine;
        m_previewLine = nullptr;
    }
    if (m_highlight)
        m_highlight->hideAnchors();
}

} // namespace Canvas
