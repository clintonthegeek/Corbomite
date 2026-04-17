// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphViewPlugin.h"

#include "GraphControlsPanel.h"
#include "GraphView.h"
#include "GraphViewTab.h"

#include "corbomite/core/proxies/ViewRegistrar.h"
#include "corbomite/vault/PluginContext.h"

#include <KPluginFactory>

namespace Corbomite {

GraphViewPlugin::GraphViewPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

GraphViewPlugin::~GraphViewPlugin() = default;

void GraphViewPlugin::onLoad(PluginContext *ctx)
{
    if (!ctx) return;

    m_vault    = ctx->vault();
    m_search   = ctx->search();
    m_metadata = ctx->metadataCache();

    if (auto *views = ctx->views()) {
        views->registerView(QStringLiteral("graph"),
            [this](WorkspaceLeaf *leaf) -> View * {
                auto *view = new GraphView(leaf);
                view->setSearch(m_search);
                view->setVault(m_vault);
                view->setMetadataCache(m_metadata);
                if (m_controlsPanel) {
                    view->setControlsPanel(m_controlsPanel);
                }
                return view;
            });
    }
}

void GraphViewPlugin::onUnload()
{
    // ViewRegistrar's destructor (owned by PluginContext) unregisters
    // the "graph" type automatically. Controls panel, if still alive,
    // is parented to a MainWindow tool view which tears it down.
    m_vault = nullptr;
    m_search = nullptr;
    m_metadata = nullptr;
    m_controlsPanel.clear();
}

QObject *GraphViewPlugin::createView(MainWindow *)
{
    if (!m_controlsPanel) {
        m_controlsPanel = new GraphControlsPanel(nullptr);
    }
    return m_controlsPanel;
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(GraphViewPluginFactory, "metadata.json",
    registerPlugin<Corbomite::GraphViewPlugin>();)

#include "GraphViewPlugin.moc"
