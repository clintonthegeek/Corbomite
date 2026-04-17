// src/plugins/graph-view/GraphView.cpp
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

void GraphView::setSearch(SearchProxy *search) { m_search = search; }
void GraphView::setVault(VaultProxy *vault) { m_vault = vault; }

void GraphView::setMetadataCache(MetadataCacheReader *cache)
{
    m_pendingCache = cache;
    if (m_graphWidget) m_graphWidget->setMetadataCache(cache);
}

void GraphView::setControlsPanel(GraphControlsPanel *panel)
{
    m_pendingPanel = panel;
    if (m_graphWidget) m_graphWidget->setControlsPanel(panel);
}

GraphViewTab *GraphView::graphWidget() const { return m_graphWidget; }

void GraphView::onOpen()
{
    ItemView::onOpen();
    if (!m_graphWidget && m_search && m_vault) {
        m_graphWidget = new GraphViewTab(m_search, m_vault, contentWidget());
        auto *layout = new QVBoxLayout(contentWidget());
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_graphWidget);

        connect(m_graphWidget, &GraphViewTab::noteActivated,
                this, &GraphView::noteActivated);

        // Apply services that were set before the widget existed.
        if (m_pendingCache) m_graphWidget->setMetadataCache(m_pendingCache);
        if (m_pendingPanel) m_graphWidget->setControlsPanel(m_pendingPanel);
    }
}

} // namespace Corbomite
