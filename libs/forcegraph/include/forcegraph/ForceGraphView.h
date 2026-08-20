// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsView>
#include "GraphTypes.h"

class QTimeLine;
class QTimer;

namespace ForceGraph {
class ForceLayoutEngine;
class ForceGraphScene;
class ForceGraphView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ForceGraphView(QWidget *parent = nullptr);
    void setEngine(ForceLayoutEngine *engine);
    void setNodes(const QVector<GraphNode> &nodes);
    void setEdges(const QVector<GraphEdge> &edges);
    void setNodeColor(const QString &id, const QColor &color);
    void setHighlightedNode(const QString &id);
    void clearHighlight();
    void zoomToFit();
    void zoomToNode(const QString &id);
    // Corbomite Cluster O Phase O1.T3 — discrete step zoom, same factor as
    // the existing wheel/keyboard handlers (now routed through these so
    // there is one definition). resetTransform() (QGraphicsView, public
    // already) covers zoom-reset — no wrapper needed.
    void zoomIn();
    void zoomOut();

    // Display settings — forwarded to scene
    void setNodeSizeScale(double scale);
    void setEdgeWidthScale(double scale);
    void setTextFadeThreshold(double threshold);
    void setShowArrows(bool show);
    void setSearchFilter(const QString &text);
public Q_SLOTS:
    void onSimulationStarted();
    void onSimulationStopped();
Q_SIGNALS:
    void nodeClicked(const QString &id);
    void nodeDoubleClicked(const QString &id);
    void nodeHovered(const QString &id);
    void nodeContextMenuRequested(const QString &id, const QPoint &globalPos);
protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
private:
    void animateToRect(const QRectF &targetRect);
    void beginInteraction();
    void endInteraction();

    ForceGraphScene *m_scene = nullptr;
    ForceLayoutEngine *m_engine = nullptr;
    QTimeLine *m_zoomTimeLine = nullptr;
    QTimer *m_interactionTimer = nullptr;
    QRectF m_zoomStartRect;
    QRectF m_zoomEndRect;
    bool m_panning = false;
    bool m_interacting = false;
    QPoint m_lastPanPos;
};
} // namespace ForceGraph
