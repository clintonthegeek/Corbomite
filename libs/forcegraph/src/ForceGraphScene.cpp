// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphScene.h"
#include "forcegraph/ForceGraphNode.h"
#include "forcegraph/ForceGraphEdge.h"
#include "forcegraph/BatchNodeItem.h"
#include "forcegraph/BatchEdgeItem.h"
#include <algorithm>

namespace ForceGraph {

ForceGraphScene::ForceGraphScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-10000, -10000, 20000, 20000);

    m_batchNodes = new BatchNodeItem;
    m_batchEdges = new BatchEdgeItem;
    m_batchNodes->setVisible(false);
    m_batchEdges->setVisible(false);
    addItem(m_batchNodes);
    addItem(m_batchEdges);
}

void ForceGraphScene::onSimulationStarted()
{
    setItemIndexMethod(QGraphicsScene::NoIndex);
    setBatchMode(true);
}

void ForceGraphScene::onSimulationStopped()
{
    setBatchMode(false);
    setItemIndexMethod(QGraphicsScene::BspTreeIndex);
}

void ForceGraphScene::setNodes(const QVector<GraphNode> &nodes)
{
    // clear() deletes all items owned by the scene (including batch items)
    clear();
    m_nodeItems.clear();
    m_edgeItems.clear();
    m_highlightedId.clear();
    m_batchMode = false;

    // Recreate batch items (clear() destroyed them)
    m_batchNodes = new BatchNodeItem;
    m_batchEdges = new BatchEdgeItem;
    m_batchNodes->setVisible(false);
    m_batchEdges->setVisible(false);
    addItem(m_batchNodes);
    addItem(m_batchEdges);

    // Find max degree for semantic zoom
    int maxDeg = 1;
    for (const auto &node : nodes) {
        maxDeg = std::max(maxDeg, node.degree);
    }

    for (const auto &node : nodes) {
        auto *item = new ForceGraphNode(node);
        item->setNodeSizeScale(m_nodeSizeScale);
        item->setTextFadeThreshold(m_textFadeThreshold);
        item->setMaxDegree(maxDeg);
        addItem(item);
        m_nodeItems.insert(node.id, item);
    }

    m_maxDegree = maxDeg;

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
    // Always update individual item positions (needed for hit testing and
    // for when we switch back to individual mode)
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
        auto *item = m_nodeItems.value(it.key());
        if (item) {
            item->setPos(it.value());
        }
    }

    for (auto *edge : std::as_const(m_edgeItems)) {
        edge->adjust();
    }

    // In batch mode, rebuild the batch data from updated positions
    if (m_batchMode) {
        syncBatchData();
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
            it.value()->setScale(1.1);
        } else if (connectedIds.contains(it.key())) {
            it.value()->setHighlighted(false);
            it.value()->setDimmed(false);
            it.value()->setScale(1.0);
        } else {
            it.value()->setHighlighted(false);
            it.value()->setDimmed(true);
            it.value()->setScale(1.0);
        }
    }

    // Set highlight/dim on all edges
    for (auto *edge : std::as_const(m_edgeItems)) {
        bool connected = (edge->sourceNode()->nodeId() == id
                          || edge->targetNode()->nodeId() == id);
        edge->setHighlighted(connected);
        edge->setDimmed(!connected);
    }
}

void ForceGraphScene::clearHighlight()
{
    m_highlightedId.clear();

    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        it.value()->setHighlighted(false);
        it.value()->setDimmed(false);
        it.value()->setScale(1.0);
    }

    for (auto *edge : std::as_const(m_edgeItems)) {
        edge->setHighlighted(false);
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

void ForceGraphScene::setSearchFilter(const QString &text)
{
    QString lower = text.toLower();
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        bool matches = lower.isEmpty() || it.value()->nodeLabel().toLower().contains(lower);
        it.value()->setDimmed(!matches);
    }

    for (auto *edge : std::as_const(m_edgeItems)) {
        bool sourceMatch = lower.isEmpty() || edge->sourceNode()->nodeLabel().toLower().contains(lower);
        bool targetMatch = lower.isEmpty() || edge->targetNode()->nodeLabel().toLower().contains(lower);
        edge->setDimmed(!sourceMatch && !targetMatch);
    }
}

void ForceGraphScene::setEdgesVisible(bool visible)
{
    if (m_batchMode) {
        m_batchEdges->setVisible(visible);
    } else {
        for (auto *edge : std::as_const(m_edgeItems))
            edge->setVisible(visible);
    }
}

void ForceGraphScene::setBatchMode(bool batch)
{
    if (m_batchMode == batch)
        return;

    m_batchMode = batch;

    // Toggle visibility: batch items vs individual items
    m_batchNodes->setVisible(batch);
    m_batchEdges->setVisible(batch);

    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        it.value()->setVisible(!batch);
    }
    for (auto *edge : std::as_const(m_edgeItems)) {
        edge->setVisible(!batch);
    }

    if (batch) {
        m_batchEdges->setWidthScale(m_edgeWidthScale);
        m_batchEdges->setShowArrows(m_showArrows);
        m_batchNodes->setSizeScale(m_nodeSizeScale);
        m_batchNodes->setTextFadeThreshold(m_textFadeThreshold);
        m_batchNodes->setMaxDegree(m_maxDegree);
        syncBatchData();
    }
}

void ForceGraphScene::syncBatchData()
{
    // Build node batch data from individual items
    QVector<BatchNodeItem::NodeData> nodeData;
    nodeData.reserve(m_nodeItems.size());

    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        auto *node = it.value();
        BatchNodeItem::NodeData nd;
        nd.position = node->pos();
        nd.radius = node->nodeRadius();
        nd.color = node->nodeColor();
        nd.label = node->nodeLabel();
        nd.degree = node->nodeDegree();
        nd.dimmed = node->isDimmed();
        nd.highlighted = node->isHighlighted();
        nodeData.append(nd);
    }
    m_batchNodes->setNodes(nodeData);

    // Build edge batch data
    QVector<BatchEdgeItem::EdgeData> edgeData;
    edgeData.reserve(m_edgeItems.size());

    for (auto *edge : std::as_const(m_edgeItems)) {
        BatchEdgeItem::EdgeData ed;
        ed.line = edge->line();
        ed.dimmed = edge->isDimmed();
        edgeData.append(ed);
    }
    m_batchEdges->setEdges(edgeData);
}

} // namespace ForceGraph
