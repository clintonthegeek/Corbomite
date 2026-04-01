// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphScene.h"
#include "forcegraph/ForceGraphNode.h"
#include "forcegraph/ForceGraphEdge.h"

namespace ForceGraph {

ForceGraphScene::ForceGraphScene(QObject *parent)
    : QGraphicsScene(parent)
{
}

void ForceGraphScene::setNodes(const QVector<GraphNode> &nodes)
{
    // clear() deletes all items owned by the scene
    clear();
    m_nodeItems.clear();
    m_edgeItems.clear();
    m_highlightedId.clear();

    for (const auto &node : nodes) {
        auto *item = new ForceGraphNode(node);
        item->setNodeSizeScale(m_nodeSizeScale);
        item->setTextFadeThreshold(m_textFadeThreshold);
        addItem(item);
        m_nodeItems.insert(node.id, item);
    }

    // Optimization for large graphs
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (nodes.size() > 200) {
        setMinimumRenderSize(1.0); // Skip items smaller than 1px on screen
    }
#endif
}

void ForceGraphScene::setEdges(const QVector<GraphEdge> &edges)
{
    // Remove existing edges from scene (scene owns them, so clear via removeItem)
    for (auto *edge : std::as_const(m_edgeItems)) {
        removeItem(edge);
        delete edge;
    }
    m_edgeItems.clear();

    for (const auto &edge : edges) {
        auto *sourceItem = m_nodeItems.value(edge.sourceId);
        auto *targetItem = m_nodeItems.value(edge.targetId);
        if (!sourceItem || !targetItem)
            continue;

        auto *item = new ForceGraphEdge(sourceItem, targetItem);
        item->setWidthScale(m_edgeWidthScale);
        item->setShowArrows(m_showArrows);
        addItem(item);
        m_edgeItems.append(item);
    }
}

void ForceGraphScene::updatePositions(const QHash<QString, QPointF> &positions)
{
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
        auto *item = m_nodeItems.value(it.key());
        if (item) {
            item->setPos(it.value());
        }
    }

    for (auto *edge : std::as_const(m_edgeItems)) {
        edge->adjust();
    }
}

void ForceGraphScene::setHighlightedNode(const QString &id)
{
    m_highlightedId = id;

    auto *targetItem = m_nodeItems.value(id);
    if (!targetItem)
        return;

    // Collect all node IDs connected to the highlighted node
    QSet<QString> connectedIds;
    connectedIds.insert(id);

    for (auto *edge : std::as_const(m_edgeItems)) {
        if (edge->sourceNode()->nodeId() == id) {
            connectedIds.insert(edge->targetNode()->nodeId());
        } else if (edge->targetNode()->nodeId() == id) {
            connectedIds.insert(edge->sourceNode()->nodeId());
        }
    }

    // Set highlight/dim on all nodes
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        if (it.key() == id) {
            it.value()->setHighlighted(true);
            it.value()->setDimmed(false);
        } else if (connectedIds.contains(it.key())) {
            it.value()->setHighlighted(false);
            it.value()->setDimmed(false);
        } else {
            it.value()->setHighlighted(false);
            it.value()->setDimmed(true);
        }
    }

    // Set dim on all edges
    for (auto *edge : std::as_const(m_edgeItems)) {
        bool connected = (edge->sourceNode()->nodeId() == id
                          || edge->targetNode()->nodeId() == id);
        edge->setDimmed(!connected);
    }
}

void ForceGraphScene::clearHighlight()
{
    m_highlightedId.clear();

    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        it.value()->setHighlighted(false);
        it.value()->setDimmed(false);
    }

    for (auto *edge : std::as_const(m_edgeItems)) {
        edge->setDimmed(false);
    }
}

ForceGraphNode *ForceGraphScene::nodeItem(const QString &id) const
{
    return m_nodeItems.value(id, nullptr);
}

void ForceGraphScene::setNodeSizeScale(double scale)
{
    m_nodeSizeScale = scale;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        it.value()->setNodeSizeScale(scale);
    }
}

void ForceGraphScene::setEdgeWidthScale(double scale)
{
    m_edgeWidthScale = scale;
    for (auto *edge : std::as_const(m_edgeItems)) {
        edge->setWidthScale(scale);
    }
}

void ForceGraphScene::setTextFadeThreshold(double threshold)
{
    m_textFadeThreshold = threshold;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        it.value()->setTextFadeThreshold(threshold);
    }
}

void ForceGraphScene::setShowArrows(bool show)
{
    m_showArrows = show;
    for (auto *edge : std::as_const(m_edgeItems)) {
        edge->setShowArrows(show);
    }
}

} // namespace ForceGraph
