// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsScene>
#include <QHash>
#include "GraphTypes.h"
namespace ForceGraph {
class ForceGraphNode;
class ForceGraphEdge;
class ForceGraphScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit ForceGraphScene(QObject *parent = nullptr);
    void setNodes(const QVector<GraphNode> &nodes);
    void setEdges(const QVector<GraphEdge> &edges);
    void updatePositions(const QHash<QString, QPointF> &positions);
    void setHighlightedNode(const QString &id);
    void clearHighlight();
    ForceGraphNode *nodeItem(const QString &id) const;
private:
    QHash<QString, ForceGraphNode *> m_nodeItems;
    QVector<ForceGraphEdge *> m_edgeItems;
    QString m_highlightedId;
};
} // namespace ForceGraph
