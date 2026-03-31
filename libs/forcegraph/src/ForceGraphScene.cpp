// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphScene.h"
namespace ForceGraph {
ForceGraphScene::ForceGraphScene(QObject *parent) : QGraphicsScene(parent) {}
void ForceGraphScene::setNodes(const QVector<GraphNode> &) {}
void ForceGraphScene::setEdges(const QVector<GraphEdge> &) {}
void ForceGraphScene::updatePositions(const QHash<QString, QPointF> &) {}
void ForceGraphScene::setHighlightedNode(const QString &) {}
void ForceGraphScene::clearHighlight() {}
ForceGraphNode *ForceGraphScene::nodeItem(const QString &) const { return nullptr; }
} // namespace ForceGraph
