// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphViewTab.h"
#include "GraphDataBuilder.h"

#include <forcegraph/ForceLayoutEngine.h>
#include <forcegraph/ForceGraphView.h>

#include <QVBoxLayout>
#include <QSet>
#include <algorithm>

namespace Corbomite {

GraphViewTab::GraphViewTab(SQLiteIndex *index, VaultModel *vault, QWidget *parent)
    : QWidget(parent)
    , m_index(index)
    , m_vault(vault)
{
    m_engine = new ForceGraph::ForceLayoutEngine(this);
    m_graphView = new ForceGraph::ForceGraphView(this);
    m_graphView->setEngine(m_engine);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_graphView);

    connect(m_graphView, &ForceGraph::ForceGraphView::nodeClicked,
            this, [this](const QString &id) {
        Q_EMIT noteActivated(id);
    });
    connect(m_graphView, &ForceGraph::ForceGraphView::nodeDoubleClicked,
            this, [this](const QString &id) {
        Q_EMIT noteActivated(id);
    });

    buildGraph();
}

GraphViewTab::~GraphViewTab()
{
    m_engine->stop();
}

void GraphViewTab::buildGraph()
{
    auto data = GraphDataBuilder::buildGlobalGraph(m_index, m_vault);

    // Cap node count for performance — 6000+ node graphs freeze the UI.
    // TODO: Implement multilevel coarsening (Handbook Ch. 12.6) for large graphs.
    static constexpr int MAX_GRAPH_NODES = 1000;
    if (data.nodes.size() > MAX_GRAPH_NODES) {
        qWarning() << "Graph too large:" << data.nodes.size() << "nodes. Capping at" << MAX_GRAPH_NODES;
        // Keep only the most connected nodes
        std::sort(data.nodes.begin(), data.nodes.end(),
                  [](const ForceGraph::GraphNode &a, const ForceGraph::GraphNode &b) {
                      return a.radius > b.radius; // radius encodes degree
                  });
        QSet<QString> kept;
        for (int i = 0; i < MAX_GRAPH_NODES && i < data.nodes.size(); ++i) {
            kept.insert(data.nodes[i].id);
        }
        data.nodes.resize(MAX_GRAPH_NODES);

        // Filter edges to only include kept nodes
        QVector<ForceGraph::GraphEdge> filteredEdges;
        for (const auto &edge : data.edges) {
            if (kept.contains(edge.sourceId) && kept.contains(edge.targetId)) {
                filteredEdges.append(edge);
            }
        }
        data.edges = filteredEdges;
    }

    m_graphView->setNodes(data.nodes);
    m_graphView->setEdges(data.edges);
    m_engine->start();

    // Zoom to fit after layout settles
    connect(m_engine, &ForceGraph::ForceLayoutEngine::simulationStable,
            this, [this]() {
        m_graphView->zoomToFit();
    }, Qt::SingleShotConnection);
}

} // namespace Corbomite
