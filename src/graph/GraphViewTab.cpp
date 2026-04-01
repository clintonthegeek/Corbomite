// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphViewTab.h"
#include "GraphControlsPanel.h"
#include "GraphDataBuilder.h"

#include <forcegraph/ForceLayoutEngine.h>
#include <forcegraph/ForceGraphView.h>

#include <QIcon>
#include <QResizeEvent>
#include <QSet>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

#include <KLocalizedString>

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
    setupControlsPanel();
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

    // Cache full graph data before filtering
    m_allNodes = data.nodes;
    m_allEdges = data.edges;

    m_graphView->setNodes(data.nodes);
    m_graphView->setEdges(data.edges);

    // Zoom to fit IMMEDIATELY so user sees overview from the start
    m_graphView->zoomToFit();

    m_engine->start();

    // Also zoom to fit after layout stabilizes for final framing
    connect(m_engine, &ForceGraph::ForceLayoutEngine::simulationStable,
            this, [this]() {
        m_graphView->zoomToFit();
    }, Qt::SingleShotConnection);
}

void GraphViewTab::setupControlsPanel()
{
    // Panel floats over the graph view — not in the layout
    m_controlsPanel = new GraphControlsPanel(this);
    m_controlsPanel->show();

    // Small toggle button to re-show panel when hidden
    m_showPanelButton = new QToolButton(this);
    m_showPanelButton->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    m_showPanelButton->setToolTip(i18n("Show graph controls"));
    m_showPanelButton->setAutoRaise(true);
    m_showPanelButton->setIconSize(QSize(22, 22));
    m_showPanelButton->hide();

    // --- Force signals → engine ---
    connect(m_controlsPanel, &GraphControlsPanel::centerForceChanged, this, [this](double v) {
        m_engine->setCenterForce(v);
        if (!m_engine->isRunning()) m_engine->start();
    });
    connect(m_controlsPanel, &GraphControlsPanel::repelForceChanged, this, [this](double v) {
        m_engine->setRepelForce(v);
        if (!m_engine->isRunning()) m_engine->start();
    });
    connect(m_controlsPanel, &GraphControlsPanel::linkForceChanged, this, [this](double v) {
        m_engine->setLinkForce(v);
        if (!m_engine->isRunning()) m_engine->start();
    });
    connect(m_controlsPanel, &GraphControlsPanel::linkDistanceChanged, this, [this](double v) {
        m_engine->setLinkDistance(v);
        if (!m_engine->isRunning()) m_engine->start();
    });

    // --- Display signals → view ---
    connect(m_controlsPanel, &GraphControlsPanel::nodeSizeScaleChanged,
            m_graphView, &ForceGraph::ForceGraphView::setNodeSizeScale);
    connect(m_controlsPanel, &GraphControlsPanel::textFadeThresholdChanged,
            m_graphView, &ForceGraph::ForceGraphView::setTextFadeThreshold);
    connect(m_controlsPanel, &GraphControlsPanel::linkThicknessScaleChanged,
            m_graphView, &ForceGraph::ForceGraphView::setEdgeWidthScale);
    connect(m_controlsPanel, &GraphControlsPanel::arrowsToggled,
            m_graphView, &ForceGraph::ForceGraphView::setShowArrows);

    // --- Filter signals → applyFilters ---
    connect(m_controlsPanel, &GraphControlsPanel::searchTextChanged,
            this, [this]() { applyFilters(); });
    connect(m_controlsPanel, &GraphControlsPanel::existingFilesOnlyChanged,
            this, [this]() { applyFilters(); });
    connect(m_controlsPanel, &GraphControlsPanel::orphansToggled,
            this, [this]() { applyFilters(); });

    // --- Animate: re-randomize + restart ---
    connect(m_controlsPanel, &GraphControlsPanel::animateRequested, this, [this]() {
        m_engine->stop();
        m_engine->randomizePositions();
        m_engine->start();
    });

    // --- Panel show/hide ---
    connect(m_controlsPanel, &GraphControlsPanel::closeRequested, this, [this]() {
        m_controlsPanel->hide();
        m_showPanelButton->show();
        positionControlsPanel();
    });
    connect(m_showPanelButton, &QToolButton::clicked, this, [this]() {
        m_controlsPanel->show();
        m_showPanelButton->hide();
        positionControlsPanel();
    });

    positionControlsPanel();
}

void GraphViewTab::positionControlsPanel()
{
    if (m_controlsPanel) {
        int x = width() - m_controlsPanel->width() - 8;
        m_controlsPanel->move(x, 8);
    }
    if (m_showPanelButton) {
        int x = width() - m_showPanelButton->width() - 8;
        m_showPanelButton->move(x, 8);
    }
}

void GraphViewTab::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    positionControlsPanel();
}

void GraphViewTab::applyFilters()
{
    QString search = m_controlsPanel->searchText().toLower();
    bool existingOnly = m_controlsPanel->existingFilesOnly();
    bool showOrphans = m_controlsPanel->showOrphans();

    // Build connection counts from the full graph
    QHash<QString, int> connectionCount;
    for (const auto &edge : m_allEdges) {
        connectionCount[edge.sourceId]++;
        connectionCount[edge.targetId]++;
    }

    // Filter nodes
    QVector<ForceGraph::GraphNode> filteredNodes;
    QSet<QString> keptIds;

    for (const auto &node : m_allNodes) {
        // Existing files only: skip unresolved (gray, small radius) nodes
        if (existingOnly && node.color == QColor(136, 136, 136) && node.radius <= 3.0) {
            continue;
        }

        // Orphans: skip nodes with 0 connections
        if (!showOrphans && connectionCount.value(node.id, 0) == 0) {
            continue;
        }

        filteredNodes.append(node);
        keptIds.insert(node.id);
    }

    // Filter edges to only include kept nodes
    QVector<ForceGraph::GraphEdge> filteredEdges;
    for (const auto &edge : m_allEdges) {
        if (keptIds.contains(edge.sourceId) && keptIds.contains(edge.targetId)) {
            filteredEdges.append(edge);
        }
    }

    // Apply to scene and engine
    m_graphView->setNodes(filteredNodes);
    m_graphView->setEdges(filteredEdges);
    m_engine->setNodes(filteredNodes);
    m_engine->setEdges(filteredEdges);

    // Apply search dimming on the scene
    m_graphView->setSearchFilter(search);

    m_graphView->zoomToFit();
    m_engine->start();
}

} // namespace Corbomite
