// SPDX-License-Identifier: GPL-3.0-or-later
#include "LocalGraphPanel.h"
#include "GraphDataBuilder.h"
#include "corbomite/core/NoteDocument.h"

#include <forcegraph/ForceLayoutEngine.h>
#include <forcegraph/ForceGraphView.h>

#include <QVBoxLayout>

namespace Corbomite {

LocalGraphPanel::LocalGraphPanel(QWidget *parent)
    : QWidget(parent)
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
}

void LocalGraphPanel::setIndex(SQLiteIndex *index)
{
    m_index = index;
    refresh();
}

void LocalGraphPanel::setVaultModel(VaultModel *vault)
{
    m_vault = vault;
}

void LocalGraphPanel::setCurrentNote(NoteDocument *doc)
{
    m_currentDoc = doc;
    refresh();
}

void LocalGraphPanel::refresh()
{
    m_engine->stop();

    if (!m_index || !m_vault || !m_currentDoc) {
        m_engine->clear();
        return;
    }

    auto data = GraphDataBuilder::buildLocalGraph(
        m_index, m_vault, m_currentDoc->relativePath(), 2);

    m_graphView->setNodes(data.nodes);
    m_graphView->setEdges(data.edges);

    if (!data.nodes.isEmpty()) {
        m_engine->start();
        // Highlight center note
        m_graphView->setHighlightedNode(m_currentDoc->relativePath());

        connect(m_engine, &ForceGraph::ForceLayoutEngine::simulationStable,
                this, [this]() {
            m_graphView->zoomToFit();
        }, Qt::SingleShotConnection);
    }
}

} // namespace Corbomite
