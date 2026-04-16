// src/graph/GraphView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphView.h"
#include "GraphViewTab.h"

#include <QVBoxLayout>
#include <KLocalizedString>

namespace Corbomite {

GraphView::GraphView(WorkspaceLeaf *leaf, QWidget *parent)
    : ItemView(leaf, parent)
{
}

View *GraphView::factory(WorkspaceLeaf *leaf)
{
    return new GraphView(leaf);
}

QString GraphView::getViewType() const { return QStringLiteral("graph"); }
QString GraphView::getDisplayText() const { return i18n("Graph view"); }
QString GraphView::getIcon() const { return QStringLiteral("network-wired"); }

void GraphView::setIndex(SQLiteIndex *index) { m_index = index; }
void GraphView::setVaultModel(VaultModel *vault) { m_vault = vault; }

void GraphView::setMetadataCache(MetadataCache *cache)
{
    if (m_graphWidget)
        m_graphWidget->setMetadataCache(cache);
}

void GraphView::setControlsPanel(GraphControlsPanel *panel)
{
    if (m_graphWidget)
        m_graphWidget->setControlsPanel(panel);
}

GraphViewTab *GraphView::graphWidget() const { return m_graphWidget; }

void GraphView::onOpen()
{
    ItemView::onOpen();
    if (!m_graphWidget && m_index && m_vault) {
        m_graphWidget = new GraphViewTab(m_index, m_vault, contentWidget());
        auto *layout = new QVBoxLayout(contentWidget());
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_graphWidget);

        connect(m_graphWidget, &GraphViewTab::noteActivated,
                this, &GraphView::noteActivated);
    }
}

} // namespace Corbomite
