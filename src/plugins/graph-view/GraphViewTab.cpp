// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphViewTab.h"
#include "GraphControlsPanel.h"
#include "GraphDataBuilder.h"

#include <corbomite/storage/proxies/MetadataCacheReader.h>
#include <corbomite/storage/proxies/SearchProxy.h>
#include <corbomite/vault/proxies/VaultProxy.h>
#include <corbomite/forcegraph/ForceLayoutEngine.h>
#include <corbomite/forcegraph/ForceGraphView.h>
#include <corbomite/forcegraph/MultilevelLayout.h>

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

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>

namespace Corbomite {

GraphViewTab::GraphViewTab(SearchProxy *search, VaultProxy *vault, QWidget *parent)
    : QWidget(parent)
    , m_search(search)
    , m_vault(vault)
{
    m_engine = new Corbomite::ForceGraph::ForceLayoutEngine(this);
    m_graphView = new Corbomite::ForceGraph::ForceGraphView(this);
    m_graphView->setEngine(m_engine);

    // Horizontal, so the per-tab controls panel can sit beside the
    // viewport inside this pane instead of in a MainWindow dock.
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_graphView, 1);

    // The panel is parented to the TAB, which is what makes "no disconnect
    // needed" true: its lifetime is exactly one tab's. Under the old
    // singleton the panel outlived every tab that had wired it.
    m_controlsPanel = new GraphControlsPanel(this);
    m_panelHost = new QWidget(this);
    auto *hostLayout = new QHBoxLayout(m_panelHost);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->addWidget(m_controlsPanel);
    layout->addWidget(m_panelHost);
    wireControlsPanel();
    syncPanelFromEngine();

    connect(m_graphView, &Corbomite::ForceGraph::ForceGraphView::nodeClicked,
            this, [this](const QString &id) {
        Q_EMIT noteActivated(id);
    });
    connect(m_graphView, &Corbomite::ForceGraph::ForceGraphView::nodeDoubleClicked,
            this, [this](const QString &id) {
        Q_EMIT noteActivated(id);
    });

    connect(m_graphView, &Corbomite::ForceGraph::ForceGraphView::nodeContextMenuRequested,
            this, &GraphViewTab::showNodeContextMenu);

    buildGraph();
}

GraphViewTab::~GraphViewTab()
{
    m_engine->stop();
}

void GraphViewTab::buildGraph()
{
    applyPersistedSettings();

    auto data = GraphDataBuilder::buildGlobalGraph(
        m_search, m_vault, GraphDataBuilder::ignoreFilterFor(m_vault));

    // Cap at 10K — beyond that, GPU acceleration is needed
    static constexpr int MAX_GRAPH_NODES = 10000;
    if (data.nodes.size() > MAX_GRAPH_NODES) {
        qWarning() << "Graph too large:" << data.nodes.size() << "nodes. Capping at" << MAX_GRAPH_NODES;
        std::sort(data.nodes.begin(), data.nodes.end(),
                  [](const Corbomite::ForceGraph::GraphNode &a, const Corbomite::ForceGraph::GraphNode &b) {
                      return a.radius > b.radius;
                  });
        QSet<QString> kept;
        for (int i = 0; i < MAX_GRAPH_NODES && i < data.nodes.size(); ++i) {
            kept.insert(data.nodes[i].id);
        }
        data.nodes.resize(MAX_GRAPH_NODES);

        QVector<Corbomite::ForceGraph::GraphEdge> filteredEdges;
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
        data.nodes = Corbomite::ForceGraph::MultilevelLayout::computeLayout(data.nodes, data.edges);
    }

    // Cache full graph data before filtering
    m_allNodes = data.nodes;
    m_allEdges = data.edges;

    m_graphView->setNodes(data.nodes);
    m_graphView->setEdges(data.edges);

    m_graphView->zoomToFit();

    m_engine->start();

    // Also zoom to fit after layout stabilizes for final framing
    connect(m_engine, &Corbomite::ForceGraph::ForceLayoutEngine::simulationStable,
            this, [this]() {
        m_graphView->zoomToFit();
    }, Qt::SingleShotConnection);
}

GraphControlsPanel *GraphViewTab::controlsPanel()
{
    return m_controlsPanel.data();
}

void GraphViewTab::syncPanelFromEngine()
{
    // A fresh panel defaults to its own initial slider values, which
    // are not necessarily the engine's current state. On the first tab both
    // start identically, but once the panel is created lazily (after, say, a
    // settings change) the panel would silently disagree with the engine.
    // Seed the panel from the engine rather than the other way round.
    if (!m_controlsPanel || !m_engine) return;
    m_controlsPanel->setForceValues(m_engine->centerForce(),
                                    m_engine->repelForce(),
                                    m_engine->linkForce(),
                                    m_engine->linkDistance());
}

void GraphViewTab::setMetadataCache(MetadataCacheReader *cache)
{
    if (m_cache) {
        disconnect(m_cache, nullptr, this, nullptr);
    }
    m_cache = cache;
    if (m_cache) {
        // Full rebuild when initial indexing completes or any single note
        // changes. The global graph is cheap enough to recompute on each
        // mutation — incremental updates can be added later if needed.
        connect(m_cache, &MetadataCacheReader::indexFinished,
                this, [this]() { buildGraph(); });
        connect(m_cache, &MetadataCacheReader::cacheChanged,
                this, [this](const QString &) { buildGraph(); });
        connect(m_cache, &MetadataCacheReader::cacheDeleted,
                this, [this](const QString &) { buildGraph(); });
    }
}

void GraphViewTab::wireControlsPanel()
{
    if (!m_controlsPanel) return;

    // --- Persist every panel change (debounced) ---
    connect(m_controlsPanel, &GraphControlsPanel::centerForceChanged,
            this, [this](double) { schedulePersist(); });
    connect(m_controlsPanel, &GraphControlsPanel::repelForceChanged,
            this, [this](double) { schedulePersist(); });
    connect(m_controlsPanel, &GraphControlsPanel::linkForceChanged,
            this, [this](double) { schedulePersist(); });
    connect(m_controlsPanel, &GraphControlsPanel::linkDistanceChanged,
            this, [this](double) { schedulePersist(); });
    connect(m_controlsPanel, &GraphControlsPanel::nodeSizeScaleChanged,
            this, [this](double) { schedulePersist(); });
    connect(m_controlsPanel, &GraphControlsPanel::textFadeThresholdChanged,
            this, [this](double) { schedulePersist(); });
    connect(m_controlsPanel, &GraphControlsPanel::linkThicknessScaleChanged,
            this, [this](double) { schedulePersist(); });
    connect(m_controlsPanel, &GraphControlsPanel::arrowsToggled,
            this, [this](bool) { schedulePersist(); });
    connect(m_controlsPanel, &GraphControlsPanel::searchTextChanged,
            this, [this](const QString &) { schedulePersist(); });
    connect(m_controlsPanel, &GraphControlsPanel::existingFilesOnlyChanged,
            this, [this](bool) { schedulePersist(); });
    connect(m_controlsPanel, &GraphControlsPanel::orphansToggled,
            this, [this](bool) { schedulePersist(); });

    m_persistDebounce.setSingleShot(true);
    m_persistDebounce.setInterval(600);
    connect(&m_persistDebounce, &QTimer::timeout,
            this, &GraphViewTab::writeGraphSettingsNow);

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
            m_graphView, &Corbomite::ForceGraph::ForceGraphView::setNodeSizeScale);
    connect(m_controlsPanel, &GraphControlsPanel::textFadeThresholdChanged,
            m_graphView, &Corbomite::ForceGraph::ForceGraphView::setTextFadeThreshold);
    connect(m_controlsPanel, &GraphControlsPanel::linkThicknessScaleChanged,
            m_graphView, &Corbomite::ForceGraph::ForceGraphView::setEdgeWidthScale);
    connect(m_controlsPanel, &GraphControlsPanel::arrowsToggled,
            m_graphView, &Corbomite::ForceGraph::ForceGraphView::setShowArrows);

    // --- Filter signals → applyFilters ---
    connect(m_controlsPanel, &GraphControlsPanel::searchTextChanged,
            this, [this]() { applyFilters(); });
    connect(m_controlsPanel, &GraphControlsPanel::existingFilesOnlyChanged,
            this, [this]() { applyFilters(); });
    connect(m_controlsPanel, &GraphControlsPanel::orphansToggled,
            this, [this]() { applyFilters(); });

    // --- Zoom to fit ---
    connect(m_controlsPanel, &GraphControlsPanel::zoomToFitRequested,
            this, &GraphViewTab::zoomToFitGraph);

    // --- Animate: re-randomize + restart ---
    connect(m_controlsPanel, &GraphControlsPanel::animateRequested,
            this, &GraphViewTab::animate);
}

void GraphViewTab::animate()
{
    if (!m_engine) return;
    m_engine->stop();
    m_engine->randomizePositions();
    m_engine->start();
}

void GraphViewTab::resetForces()
{
    if (m_controlsPanel) {
        m_controlsPanel->resetToDefaults();
        schedulePersist();
    }
}

void GraphViewTab::applyPersistedSettings()
{
    if (m_vault) {
        const QJsonObject persisted =
            m_vault->readConfigJson(QStringLiteral("graph")).toObject();
        if (!persisted.isEmpty()) {
            // Our ranges vs Obsidian's: the file's centre strength is 0–1,
            // our slider 0–0.05 (×20); the file's repel strength is 0–100,
            // our slider 0–5000 (÷50); the file's link strength is 0–1,
            // our slider 0–0.2 (÷5). Values outside our slider ranges are
            // clamped by QSlider::setValue, which is the correct behaviour
            // for a file another tool authored.
            if (persisted.contains(QStringLiteral("centerStrength")))
                m_engine->setCenterForce(
                    persisted.value(QStringLiteral("centerStrength")).toDouble() / 20.0);
            if (persisted.contains(QStringLiteral("repelStrength")))
                m_engine->setRepelForce(
                    persisted.value(QStringLiteral("repelStrength")).toDouble() * 50.0);
            if (persisted.contains(QStringLiteral("linkStrength")))
                m_engine->setLinkForce(
                    persisted.value(QStringLiteral("linkStrength")).toDouble() / 5.0);
            if (persisted.contains(QStringLiteral("linkDistance")))
                m_engine->setLinkDistance(persisted.value(QStringLiteral("linkDistance")).toDouble());

            // Display scales map 1:1 (both sides 0–3 for text fade, ~1.0
            // for the size scalars — these are literal multipliers).
            auto *view = m_graphView;
            if (persisted.contains(QStringLiteral("nodeSizeMultiplier")))
                view->setNodeSizeScale(persisted.value(
                    QStringLiteral("nodeSizeMultiplier")).toDouble());
            if (persisted.contains(QStringLiteral("textFadeMultiplier")))
                view->setTextFadeThreshold(persisted.value(
                    QStringLiteral("textFadeMultiplier")).toDouble());
            if (persisted.contains(QStringLiteral("lineSizeMultiplier")))
                view->setEdgeWidthScale(persisted.value(
                    QStringLiteral("lineSizeMultiplier")).toDouble());
            if (persisted.contains(QStringLiteral("showArrow")))
                view->setShowArrows(persisted.value(
                    QStringLiteral("showArrow")).toBool());
        }

        // Filters: the checkbox semantics line up exactly (existing-files-
        // only = hide unresolved note nodes), and Obsidian's keys collapse
        // when absent (`existingFilesOnly` etc. are OUR names — the whole
        // point is the file's `hideUnresolved`/`showOrphans`).
        const bool existingOnly =
            persisted.value(QStringLiteral("hideUnresolved")).toBool(false);
        const bool showOrphans =
            persisted.contains(QStringLiteral("showOrphans"))
                ? persisted.value(QStringLiteral("showOrphans")).toBool()
                : true;   // Obsidian's default (showOrphans: true)
        m_controlsPanel->setFiltersState(
            persisted.value(QStringLiteral("search")).toString(),
            existingOnly, showOrphans);

        // Display section needs the SEED to match what was applied above.
        const bool showArrows =
            persisted.contains(QStringLiteral("showArrow"))
                ? persisted.value(QStringLiteral("showArrow")).toBool()
                : false;
        const double textFade = persisted.contains(
            QStringLiteral("textFadeMultiplier"))
            ? persisted.value(QStringLiteral("textFadeMultiplier")).toDouble()
            : 1.0;
        const double nodeSize = persisted.contains(
            QStringLiteral("nodeSizeMultiplier"))
            ? persisted.value(QStringLiteral("nodeSizeMultiplier")).toDouble()
            : 1.0;
        const double lineSize = persisted.contains(
            QStringLiteral("lineSizeMultiplier"))
            ? persisted.value(QStringLiteral("lineSizeMultiplier")).toDouble()
            : 1.0;
        m_controlsPanel->setDisplayState(showArrows, textFade, nodeSize,
                                          lineSize);
    }
    const double center = m_engine->centerForce();
    const double repel = m_engine->repelForce();
    const double link = m_engine->linkForce();
    const double distance = m_engine->linkDistance();
    m_controlsPanel->setForceValues(center, repel, link, distance);
    m_engine->start();

    applyFilters();
}

void GraphViewTab::schedulePersist()
{
    m_persistDebounce.start();
}

void GraphViewTab::writeGraphSettingsNow()
{
    if (!m_vault || !m_controlsPanel) return;

    // Read → merge over → write. Ownership: our four key groups are
    // Filters/Display/Forces; `colorGroups`, `collapse-*` and any key we
    // do not write stay untouched (the vault-format universal rule: merging
    // only the keys we own preserves unknown keys verbatim).
    QJsonObject merged;
    if (m_vault) {
        merged = m_vault->readConfigJson(QStringLiteral("graph")).toObject();
    }
    merged.insert(QStringLiteral("search"), m_controlsPanel->searchText());
    merged.insert(QStringLiteral("hideUnresolved"),
                  m_controlsPanel->existingFilesOnly());
    merged.insert(QStringLiteral("showOrphans"),
                  m_controlsPanel->showOrphans());
    merged.insert(QStringLiteral("showArrow"), m_controlsPanel->showArrows());
    merged.insert(QStringLiteral("textFadeMultiplier"),
                  m_controlsPanel->textFadeThreshold());
    merged.insert(QStringLiteral("nodeSizeMultiplier"),
                  m_controlsPanel->nodeSizeScale());
    merged.insert(QStringLiteral("lineSizeMultiplier"),
                  m_controlsPanel->linkThicknessScale());
    // Our forces are in OUR slider ranges; the file's are Obsidian's, so
    // the direction of the correction is inverted from applyPersisted().
    merged.insert(QStringLiteral("centerStrength"),
                  m_controlsPanel->centerForce() * 20.0);
    merged.insert(QStringLiteral("repelStrength"),
                  m_controlsPanel->repelForce() / 50.0);
    merged.insert(QStringLiteral("linkStrength"),
                  m_controlsPanel->linkForce() * 5.0);
    merged.insert(QStringLiteral("linkDistance"),
                  m_controlsPanel->linkDistance());
    // The scaling factor for observed round-trips; carried in the file so a
    // future reader of THIS file knows what state built the current view.
    merged.insert(QStringLiteral("scale"), 1);

    m_vault->writeConfigJson(QStringLiteral("graph"), merged);
}


void GraphViewTab::zoomToFitGraph()
{
    if (m_graphView) m_graphView->zoomToFit();
}

double GraphViewTab::engineCenterForce() const
{
    return m_engine ? m_engine->centerForce() : 0.0;
}

void GraphViewTab::setEngineCenterForce(double force)
{
    if (m_engine) m_engine->setCenterForce(force);
}

bool GraphViewTab::isGraphBuilt() const
{
    // The engine exists from construction, so its presence proves nothing.
    // What makes a graph meaningful is having nodes to lay out — check the
    // scene, not the widget, so a graph that failed to build reports false.
    return m_graphView && m_graphView->scene()
        && !m_graphView->scene()->items().isEmpty();
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
    QVector<Corbomite::ForceGraph::GraphNode> filteredNodes;
    QSet<QString> keptIds;

    for (const auto &node : m_allNodes) {
        // Existing files only: skip unresolved link targets
        if (existingOnly && node.type == Corbomite::ForceGraph::NodeType::Unresolved) {
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
    QVector<Corbomite::ForceGraph::GraphEdge> filteredEdges;
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

    // Pending: "Open in new window" needs multi-window support.

    menu.addSeparator();

    // Pending: "Move file to…", "Bookmark…" and "Merge entire file with…"
    // need the file-move dialog, the bookmark system and the merge UI
    // respectively. All are filed in the punch list.

    // Copy path submenu
    auto *copyMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("edit-copy")),
                                   i18n("Copy path"));
    copyMenu->addAction(i18n("Copy vault path"), this, [relativePath]() {
        QGuiApplication::clipboard()->setText(relativePath);
    });
    copyMenu->addAction(i18n("Copy absolute path"), this, [absolutePath]() {
        QGuiApplication::clipboard()->setText(absolutePath);
    });

    // Pending: "Open linked view" needs the local graph to be openable as a tab.

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
