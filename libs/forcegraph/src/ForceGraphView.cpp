// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphView.h"
#include "forcegraph/ForceGraphScene.h"
#include "forcegraph/ForceGraphNode.h"
#include "forcegraph/ForceGraphEdge.h"
#include "forcegraph/ForceLayoutEngine.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScrollBar>

namespace ForceGraph {

static constexpr double ZoomFactor = 1.15;

ForceGraphView::ForceGraphView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new ForceGraphScene(this);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(AnchorUnderMouse);
    setViewportUpdateMode(SmartViewportUpdate);
    setOptimizationFlags(DontSavePainterState);
}

void ForceGraphView::setEngine(ForceLayoutEngine *engine)
{
    if (m_engine) {
        disconnect(m_engine, &ForceLayoutEngine::positionsUpdated,
                   m_scene, &ForceGraphScene::updatePositions);
    }
    m_engine = engine;
    if (m_engine) {
        connect(m_engine, &ForceLayoutEngine::positionsUpdated,
                m_scene, &ForceGraphScene::updatePositions);
    }
}

void ForceGraphView::setNodes(const QVector<GraphNode> &nodes)
{
    m_scene->setNodes(nodes);
    if (m_engine) {
        m_engine->setNodes(nodes);
    }
}

void ForceGraphView::setEdges(const QVector<GraphEdge> &edges)
{
    m_scene->setEdges(edges);
    if (m_engine) {
        m_engine->setEdges(edges);
    }
}

void ForceGraphView::setNodeColor(const QString &id, const QColor &color)
{
    auto *item = m_scene->nodeItem(id);
    if (item) {
        GraphNode data;
        data.id = id;
        data.color = color;
        data.radius = item->rect().width() / 2.0;
        data.label = item->nodeId(); // preserve id at minimum
        item->setData(data);
    }
}

void ForceGraphView::setHighlightedNode(const QString &id)
{
    m_scene->setHighlightedNode(id);
}

void ForceGraphView::clearHighlight()
{
    m_scene->clearHighlight();
}

void ForceGraphView::zoomToFit()
{
    if (!scene() || scene()->items().isEmpty())
        return;
    fitInView(scene()->itemsBoundingRect().adjusted(-50, -50, 50, 50),
              Qt::KeepAspectRatio);
}

void ForceGraphView::setNodeSizeScale(double scale)
{
    m_scene->setNodeSizeScale(scale);
}

void ForceGraphView::setEdgeWidthScale(double scale)
{
    m_scene->setEdgeWidthScale(scale);
}

void ForceGraphView::setTextFadeThreshold(double threshold)
{
    m_scene->setTextFadeThreshold(threshold);
}

void ForceGraphView::setShowArrows(bool show)
{
    m_scene->setShowArrows(show);
}

void ForceGraphView::setSearchFilter(const QString &text)
{
    m_scene->setSearchFilter(text);
}

void ForceGraphView::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0) {
        scale(ZoomFactor, ZoomFactor);
    } else {
        scale(1.0 / ZoomFactor, 1.0 / ZoomFactor);
    }
    event->accept();
}

void ForceGraphView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Check if there's a node under the cursor
        auto *item = itemAt(event->pos());
        auto *nodeItem = dynamic_cast<ForceGraphNode *>(item);

        if (nodeItem) {
            // Let QGraphicsView handle node dragging (ItemIsMovable)
            m_panning = false;
            QGraphicsView::mousePressEvent(event);
            return;
        }

        // Empty space: start panning
        m_panning = true;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void ForceGraphView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();

        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());

        event->accept();
        return;
    }

    // Check hover: is cursor over a node?
    auto *item = itemAt(event->pos());
    auto *nodeItem = dynamic_cast<ForceGraphNode *>(item);
    if (nodeItem) {
        m_scene->setHighlightedNode(nodeItem->nodeId());
        Q_EMIT nodeHovered(nodeItem->nodeId());
    } else {
        m_scene->clearHighlight();
    }

    QGraphicsView::mouseMoveEvent(event);
}

void ForceGraphView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_panning) {
            m_panning = false;
            setCursor(Qt::ArrowCursor);
            event->accept();
            return;
        }

        // Check if a node was being dragged (QGraphicsView handled the drag)
        // If the mouse release is on a node, it was either a click or end of drag
        auto *item = itemAt(event->pos());
        auto *nodeItem = dynamic_cast<ForceGraphNode *>(item);
        if (nodeItem) {
            // Pin the node at its new position in the engine
            if (m_engine) {
                m_engine->pinNode(nodeItem->nodeId(), nodeItem->pos());
            }
            Q_EMIT nodeClicked(nodeItem->nodeId());
        }
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void ForceGraphView::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Home:
        zoomToFit();
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        scale(ZoomFactor, ZoomFactor);
        break;
    case Qt::Key_Minus:
        scale(1.0 / ZoomFactor, 1.0 / ZoomFactor);
        break;
    default:
        QGraphicsView::keyPressEvent(event);
        return;
    }
    event->accept();
}

} // namespace ForceGraph
