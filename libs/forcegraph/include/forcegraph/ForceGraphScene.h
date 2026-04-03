// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsScene>
#include <QHash>
#include "GraphTypes.h"
namespace ForceGraph {
class ForceGraphNode;
class ForceGraphEdge;
class BatchNodeItem;
class BatchEdgeItem;
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

    /// Bounding rect of actual node positions (not the fixed scene rect)
    QRectF contentBoundingRect() const;

    // Display settings — propagate to all items
    void setNodeSizeScale(double scale);
    void setEdgeWidthScale(double scale);
    void setTextFadeThreshold(double threshold);
    void setShowArrows(bool show);

    // Search filter — dims non-matching nodes and edges
    void setSearchFilter(const QString &text);

    // Edge visibility control for interaction optimization
    void setEdgesVisible(bool visible);

    int nodeCount() const { return m_nodeItems.size(); }

public Q_SLOTS:
    void onSimulationStarted();
    void onSimulationStopped();

private:
    void setBatchMode(bool batch);
    void syncBatchData();

    QHash<QString, ForceGraphNode *> m_nodeItems;
    QVector<ForceGraphEdge *> m_edgeItems;
    QString m_highlightedId;

    // Batch rendering items (visible during simulation)
    BatchNodeItem *m_batchNodes = nullptr;
    BatchEdgeItem *m_batchEdges = nullptr;
    bool m_batchMode = false;

    // Cached display settings (applied to newly created items too)
    double m_nodeSizeScale = 1.0;
    double m_edgeWidthScale = 1.0;
    double m_textFadeThreshold = 1.0;
    bool m_showArrows = false;
    int m_maxDegree = 1;
};
} // namespace ForceGraph
