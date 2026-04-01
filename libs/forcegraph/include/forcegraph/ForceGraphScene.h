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

    // Display settings — propagate to all items
    void setNodeSizeScale(double scale);
    void setEdgeWidthScale(double scale);
    void setTextFadeThreshold(double threshold);
    void setShowArrows(bool show);

    // Search filter — dims non-matching nodes and edges
    void setSearchFilter(const QString &text);

private:
    QHash<QString, ForceGraphNode *> m_nodeItems;
    QVector<ForceGraphEdge *> m_edgeItems;
    QString m_highlightedId;

    // Cached display settings (applied to newly created items too)
    double m_nodeSizeScale = 1.0;
    double m_edgeWidthScale = 1.0;
    double m_textFadeThreshold = 1.0;
    bool m_showArrows = false;
};
} // namespace ForceGraph
