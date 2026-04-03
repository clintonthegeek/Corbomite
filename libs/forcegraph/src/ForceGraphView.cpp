// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphView.h"
#include "forcegraph/ForceGraphScene.h"
#include "forcegraph/ForceGraphNode.h"
#include "forcegraph/ForceGraphEdge.h"
#include "forcegraph/ForceLayoutEngine.h"
#include <QEasingCurve>
#include <QTimeLine>
#include <QTimer>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QScrollBar>

namespace ForceGraph {

static constexpr double ZoomFactor = 1.15;
static constexpr int EdgeHideThreshold = 2000; // hide edges during interaction above this node count

ForceGraphView::ForceGraphView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new ForceGraphScene(this);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(AnchorUnderMouse);
    setViewportUpdateMode(SmartViewportUpdate);
    setOptimizationFlags(DontSavePainterState | DontAdjustForAntialiasing);
}

void ForceGraphView::setEngine(ForceLayoutEngine *engine)
{
    if (m_engine) {
        disconnect(m_engine, &ForceLayoutEngine::positionsUpdated,
                   m_scene, &ForceGraphScene::updatePositions);
        disconnect(m_engine, &ForceLayoutEngine::simulationStarted,
                   this, &ForceGraphView::onSimulationStarted);
        disconnect(m_engine, &ForceLayoutEngine::simulationStopped,
                   this, &ForceGraphView::onSimulationStopped);
        disconnect(m_engine, &ForceLayoutEngine::simulationStarted,
                   m_scene, &ForceGraphScene::onSimulationStarted);
        disconnect(m_engine, &ForceLayoutEngine::simulationStopped,
                   m_scene, &ForceGraphScene::onSimulationStopped);
    }
    m_engine = engine;
    if (m_engine) {
        connect(m_engine, &ForceLayoutEngine::positionsUpdated,
                m_scene, &ForceGraphScene::updatePositions);
        connect(m_engine, &ForceLayoutEngine::simulationStarted,
                this, &ForceGraphView::onSimulationStarted);
        connect(m_engine, &ForceLayoutEngine::simulationStopped,
                this, &ForceGraphView::onSimulationStopped);
        connect(m_engine, &ForceLayoutEngine::simulationStarted,
                m_scene, &ForceGraphScene::onSimulationStarted);
        connect(m_engine, &ForceLayoutEngine::simulationStopped,
                m_scene, &ForceGraphScene::onSimulationStopped);
    }
}

void ForceGraphView::onSimulationStarted()
{
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
}

void ForceGraphView::onSimulationStopped()
{
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
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
    if (!m_scene || m_scene->nodeCount() == 0)
        return;

    // Use actual node positions, not itemsBoundingRect() which includes
    // batch items with their full 20K×20K scene-covering bounding rects
    QRectF content = m_scene->contentBoundingRect();
    if (content.isEmpty())
        return;

    fitInView(content.adjusted(-50, -50, 50, 50), Qt::KeepAspectRatio);
}

void ForceGraphView::zoomToNode(const QString &id)
{
    auto *item = m_scene->nodeItem(id);
    if (!item)
        return;

    // Center on node with a reasonable zoom level
    double r = 300.0; // scene units of context around the node
    QRectF target(item->pos().x() - r, item->pos().y() - r, 2 * r, 2 * r);
    animateToRect(target);
}

void ForceGraphView::animateToRect(const QRectF &targetRect)
{
    if (!m_zoomTimeLine) {
        m_zoomTimeLine = new QTimeLine(300, this);
        m_zoomTimeLine->setEasingCurve(QEasingCurve::InOutCubic);
        m_zoomTimeLine->setUpdateInterval(16); // ~60fps

        connect(m_zoomTimeLine, &QTimeLine::valueChanged, this, [this](qreal progress) {
            // Interpolate between start and end rects
            QRectF r(
                m_zoomStartRect.x() + (m_zoomEndRect.x() - m_zoomStartRect.x()) * progress,
                m_zoomStartRect.y() + (m_zoomEndRect.y() - m_zoomStartRect.y()) * progress,
                m_zoomStartRect.width() + (m_zoomEndRect.width() - m_zoomStartRect.width()) * progress,
                m_zoomStartRect.height() + (m_zoomEndRect.height() - m_zoomStartRect.height()) * progress
            );
            fitInView(r, Qt::KeepAspectRatio);
        });
    }

    // Current visible rect in scene coords
    m_zoomStartRect = mapToScene(viewport()->rect()).boundingRect();
    m_zoomEndRect = targetRect;

    // If the timeline is running, restart it from the current interpolated position
    if (m_zoomTimeLine->state() == QTimeLine::Running) {
        m_zoomTimeLine->stop();
        m_zoomStartRect = mapToScene(viewport()->rect()).boundingRect();
    }

    m_zoomTimeLine->start();
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

void ForceGraphView::beginInteraction()
{
    if (m_interacting)
        return;

    if (m_scene->nodeCount() < EdgeHideThreshold)
        return;

    m_interacting = true;
    m_scene->setEdgesVisible(false);

    if (!m_interactionTimer) {
        m_interactionTimer = new QTimer(this);
        m_interactionTimer->setSingleShot(true);
        m_interactionTimer->setInterval(150);
        connect(m_interactionTimer, &QTimer::timeout, this, &ForceGraphView::endInteraction);
    }
}

void ForceGraphView::endInteraction()
{
    if (!m_interacting)
        return;

    m_interacting = false;
    m_scene->setEdgesVisible(true);
    viewport()->update();
}

void ForceGraphView::wheelEvent(QWheelEvent *event)
{
    beginInteraction();
    if (m_interactionTimer)
        m_interactionTimer->start(); // reset the timer

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
        beginInteraction();
        if (m_interactionTimer)
            m_interactionTimer->start(); // reset the timer

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

void ForceGraphView::contextMenuEvent(QContextMenuEvent *event)
{
    auto *item = itemAt(event->pos());
    auto *nodeItem = dynamic_cast<ForceGraphNode *>(item);
    if (nodeItem) {
        Q_EMIT nodeContextMenuRequested(nodeItem->nodeId(), event->globalPos());
        event->accept();
        return;
    }
    QGraphicsView::contextMenuEvent(event);
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
