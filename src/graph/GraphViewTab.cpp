// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphViewTab.h"
#include "GraphControlsPanel.h"
#include "GraphDataBuilder.h"

#include <corbomite/vault/Vault.h>
#include <corbomite/storage/MetadataCache.h>
#include <forcegraph/ForceLayoutEngine.h>
#include <forcegraph/ForceGraphView.h>
#include <forcegraph/MultilevelLayout.h>

#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QResizeEvent>
#include <QSet>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>

namespace Corbomite {

GraphViewTab::GraphViewTab(SQLiteIndex *index, Vault *vault, QWidget *parent)
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

    connect(m_graphView, &ForceGraph::ForceGraphView::nodeContextMenuRequested,
            this, &GraphViewTab::showNodeContextMenu);

    buildGraph();
}

GraphViewTab::~GraphViewTab()
{
    m_engine->stop();
}

void GraphViewTab::buildGraph()
{
    auto data = GraphDataBuilder::buildGlobalGraph(m_index, m_vault);

    // Cap at 10K — beyond that, GPU acceleration (P8) is needed
    static constexpr int MAX_GRAPH_NODES = 10000;
    if (data.nodes.size() > MAX_GRAPH_NODES) {
        qWarning() << "Graph too large:" << data.nodes.size() << "nodes. Capping at" << MAX_GRAPH_NODES;
        std::sort(data.nodes.begin(), data.nodes.end(),
                  [](const ForceGraph::GraphNode &a, const ForceGraph::GraphNode &b) {
                      return a.radius > b.radius;
                  });
        QSet<QString> kept;
        for (int i = 0; i < MAX_GRAPH_NODES && i < data.nodes.size(); ++i) {
            kept.insert(data.nodes[i].id);
        }
        data.nodes.resize(MAX_GRAPH_NODES);

        QVector<ForceGraph::GraphEdge> filteredEdges;
        for (const auto &edge : data.edges) {
            if (kept.contains(edge.sourceId) && kept.contains(edge.targetId)) {
                filteredEdges.append(edge);
            }
        }
        data.edges = filteredEdges;
    }

    // Multilevel coarsening for large graphs — eliminates local minima and
    // converges in seconds by computing good initial positions
    static constexpr int MULTILEVEL_THRESHOLD = 200;
    if (data.nodes.size() > MULTILEVEL_THRESHOLD) {
        data.nodes = ForceGraph::MultilevelLayout::computeLayout(data.nodes, data.edges);
    }

    // Cache full graph data before filtering
    m_allNodes = data.nodes;
    m_allEdges = data.edges;

    m_graphView->setNodes(data.nodes);
    m_graphView->setEdges(data.edges);

    m_graphView->zoomToFit();

    m_engine->start();

    // Also zoom to fit after layout stabilizes for final framing
    connect(m_engine, &ForceGraph::ForceLayoutEngine::simulationStable,
            this, [this]() {
        m_graphView->zoomToFit();
    }, Qt::SingleShotConnection);
}

void GraphViewTab::setControlsPanel(GraphControlsPanel *panel)
{
    m_controlsPanel = panel;
    if (m_controlsPanel) {
        wireControlsPanel();
    }
}

void GraphViewTab::setMetadataCache(MetadataCache *cache)
{
    if (m_cache) {
        disconnect(m_cache, nullptr, this, nullptr);
    }
    m_cache = cache;
    if (m_cache) {
        // Full rebuild when initial indexing completes or any single note
        // changes. The global graph is cheap enough to recompute on each
        // mutation — Phase 8 can add incremental updates later if needed.
        connect(m_cache, &MetadataCache::indexFinished,
                this, [this]() { buildGraph(); });
        connect(m_cache, &MetadataCache::cacheChanged,
                this, [this](const QString &, const QString &, const CachedMetadata &) {
            buildGraph();
        });
        connect(m_cache, &MetadataCache::cacheDeleted,
                this, [this](const QString &, const CachedMetadata &) { buildGraph(); });
    }
}

void GraphViewTab::wireControlsPanel()
{
    if (!m_controlsPanel) return;

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

    // --- Zoom to fit ---
    connect(m_controlsPanel, &GraphControlsPanel::zoomToFitRequested, this, [this]() {
        m_graphView->zoomToFit();
    });

    // --- Animate: re-randomize + restart ---
    connect(m_controlsPanel, &GraphControlsPanel::animateRequested, this, [this]() {
        m_engine->stop();
        m_engine->randomizePositions();
        m_engine->start();
    });
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
        // Existing files only: skip unresolved link targets
        if (existingOnly && node.type == ForceGraph::NodeType::Unresolved) {
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

    // Zoom to first matching node if searching, otherwise zoom to fit all
    if (!search.isEmpty()) {
        for (const auto &node : filteredNodes) {
            if (node.label.toLower().contains(search)) {
                m_graphView->zoomToNode(node.id);
                break;
            }
        }
    } else {
        m_graphView->zoomToFit();
    }

    m_engine->start();
}

void GraphViewTab::showNodeContextMenu(const QString &nodeId, const QPoint &globalPos)
{
    if (!m_vault) return;

    // Node ID is the vault-relative path
    QString relativePath = nodeId;
    QString absolutePath = m_vault->basePath() + QLatin1Char('/') + relativePath;
    QString displayName = relativePath.mid(relativePath.lastIndexOf(QLatin1Char('/')) + 1);
    if (displayName.endsWith(QStringLiteral(".md"))) displayName.chop(3);

    QMenu menu(this);

    // Header: note name (disabled)
    auto *header = menu.addAction(displayName);
    header->setEnabled(false);
    QFont headerFont = header->font();
    headerFont.setBold(true);
    header->setFont(headerFont);

    menu.addSeparator();

    // Open in new tab
    menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")),
                   i18n("Open in new tab"), this, [this, relativePath]() {
        Q_EMIT openNoteInNewTabRequested(relativePath);
    });

    // TODO: Open in new window (requires multi-window support)

    menu.addSeparator();

    // TODO: Move file to... (requires file move dialog)
    // TODO: Bookmark... (requires bookmark system)
    // TODO: Merge entire file with... (requires merge UI)

    // Copy path submenu
    auto *copyMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("edit-copy")),
                                   i18n("Copy path"));
    copyMenu->addAction(i18n("Copy vault path"), this, [relativePath]() {
        QGuiApplication::clipboard()->setText(relativePath);
    });
    copyMenu->addAction(i18n("Copy absolute path"), this, [absolutePath]() {
        QGuiApplication::clipboard()->setText(absolutePath);
    });

    // TODO: Open linked view (requires local graph as tab)

    menu.addSeparator();

    // Open in default app
    menu.addAction(QIcon::fromTheme(QStringLiteral("document-open")),
                   i18n("Open in default app"), this, [absolutePath]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(absolutePath));
    });

    // Show in system explorer
    menu.addAction(QIcon::fromTheme(QStringLiteral("system-file-manager")),
                   i18n("Show in system explorer"), this, [absolutePath]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(absolutePath).absolutePath()));
    });

    // Reveal in navigation
    menu.addAction(QIcon::fromTheme(QStringLiteral("go-jump")),
                   i18n("Reveal file in navigation"), this, [this, relativePath]() {
        Q_EMIT revealInNavigationRequested(relativePath);
    });

    menu.addSeparator();

    // Delete file
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")),
                   i18n("Delete file"), this, [this, relativePath, displayName]() {
        auto result = KMessageBox::questionTwoActions(
            this,
            i18n("Delete \"%1\"?\n\nThis cannot be undone.", displayName),
            i18n("Delete File"),
            KStandardGuiItem::del(),
            KStandardGuiItem::cancel()
        );
        if (result == KMessageBox::PrimaryAction) {
            Q_EMIT deleteNoteRequested(relativePath);
        }
    });

    menu.exec(globalPos);
}

} // namespace Corbomite
