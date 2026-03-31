// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphView.h"
namespace ForceGraph {
ForceGraphView::ForceGraphView(QWidget *parent) : QGraphicsView(parent) {}
void ForceGraphView::setEngine(ForceLayoutEngine *) {}
void ForceGraphView::setNodes(const QVector<GraphNode> &) {}
void ForceGraphView::setEdges(const QVector<GraphEdge> &) {}
void ForceGraphView::setNodeColor(const QString &, const QColor &) {}
void ForceGraphView::setHighlightedNode(const QString &) {}
void ForceGraphView::clearHighlight() {}
void ForceGraphView::zoomToFit() {}
void ForceGraphView::wheelEvent(QWheelEvent *e) { QGraphicsView::wheelEvent(e); }
void ForceGraphView::mousePressEvent(QMouseEvent *e) { QGraphicsView::mousePressEvent(e); }
void ForceGraphView::mouseMoveEvent(QMouseEvent *e) { QGraphicsView::mouseMoveEvent(e); }
void ForceGraphView::mouseReleaseEvent(QMouseEvent *e) { QGraphicsView::mouseReleaseEvent(e); }
void ForceGraphView::keyPressEvent(QKeyEvent *e) { QGraphicsView::keyPressEvent(e); }
} // namespace ForceGraph
