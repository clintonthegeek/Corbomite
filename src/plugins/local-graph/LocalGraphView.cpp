// SPDX-License-Identifier: GPL-3.0-or-later
#include "LocalGraphView.h"

#include "../../graph/GraphDataBuilder.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/Vault.h"

#include <forcegraph/ForceGraphView.h>
#include <forcegraph/ForceLayoutEngine.h>

#include <QVBoxLayout>

namespace Corbomite {

LocalGraphView::LocalGraphView(SQLiteIndex *index,
                                Vault *vault,
                                MetadataCacheReader *metadata,
                                WorkspaceController *workspace,
                                QWidget *parent)
    : QWidget(parent)
    , m_index(index)
    , m_vault(vault)
    , m_metadata(metadata)
    , m_workspace(workspace)
{
    m_engine = new ForceGraph::ForceLayoutEngine(this);
    m_graphView = new ForceGraph::ForceGraphView(this);
    m_graphView->setEngine(m_engine);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_graphView);

    connect(m_graphView, &ForceGraph::ForceGraphView::nodeClicked, this,
            [this](const QString &id) {
                if (m_workspace) m_workspace->openFile(id);
            });

    if (m_metadata) {
        connect(m_metadata, &MetadataCacheReader::allLinksResolved, this,
                &LocalGraphView::refresh);
        connect(m_metadata, &MetadataCacheReader::cacheDeleted, this,
                [this](const QString &) { refresh(); });
    }
    if (m_workspace) {
        connect(m_workspace, &WorkspaceController::activeFileChanged, this,
                &LocalGraphView::onActiveFileChanged);
        m_currentPath = m_workspace->activeFilePath();
    }
}

void LocalGraphView::onActiveFileChanged(const QString &path)
{
    if (m_currentPath == path) return;
    m_currentPath = path;
    refresh();
}

void LocalGraphView::refresh()
{
    m_engine->stop();
    if (!m_index || !m_vault || m_currentPath.isEmpty()) {
        m_engine->clear();
        return;
    }
    auto data = GraphDataBuilder::buildLocalGraph(m_index, m_vault, m_currentPath, 2);
    m_graphView->setNodes(data.nodes);
    m_graphView->setEdges(data.edges);
    if (!data.nodes.isEmpty()) {
        m_engine->start();
        m_graphView->setHighlightedNode(m_currentPath);
        connect(m_engine, &ForceGraph::ForceLayoutEngine::simulationStable,
                this, [this]() { m_graphView->zoomToFit(); }, Qt::SingleShotConnection);
    }
}

} // namespace Corbomite
