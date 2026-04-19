// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphViewPlugin.h"

#include "GraphControlsPanel.h"
#include "GraphView.h"
#include "GraphViewTab.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/ViewRegistrar.h"
#include "corbomite/vault/PluginContext.h"

#include <KLocalizedString>
#include <KPluginFactory>

#include <QApplication>
#include <QClipboard>
#include <QImage>

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

    // Capture the CommandRegistrar so the factory closure can wire a
    // dispatcher onto each new GraphView. CommandRegistrar::invoke routes
    // through the underlying CommandRegistry (no direct registry leak).
    auto *commandsProxy = ctx->commands();

    if (auto *views = ctx->views()) {
        views->registerView(QStringLiteral("graph"),
            [this, commandsProxy](WorkspaceLeaf *leaf) -> View * {
                auto *view = new GraphView(leaf);
                view->setSearch(m_search);
                view->setVault(m_vault);
                view->setMetadataCache(m_metadata);
                if (m_controlsPanel) {
                    view->setControlsPanel(m_controlsPanel);
                }
                m_views.append(QPointer<GraphView>(view));
                if (commandsProxy) {
                    view->setGraphCommandDispatcher(
                        [commandsProxy](const QString &commandId) {
                            commandsProxy->invoke(commandId);
                        });
                }
                return view;
            });
    }

    // Cluster R Task 3.7 — `copy-screenshot` command. Grabs the last
    // opened GraphView's widget pixmap (best-effort "active graph" —
    // since QPointer-tracked, destroyed views auto-expire).
    if (auto *commands = ctx->commands()) {
        Command cmd;
        cmd.id = QStringLiteral("copy-screenshot");
        cmd.name = i18n("Copy graph screenshot");
        cmd.icon = QStringLiteral("camera-photo");
        cmd.callback = [this] {
            // Walk backwards for the most-recently-added live view.
            for (int i = m_views.size() - 1; i >= 0; --i) {
                GraphView *gv = m_views.at(i).data();
                if (!gv) continue;
                QWidget *target = gv->graphWidget();
                if (!target) target = gv;
                const QImage img = target->grab().toImage();
                if (!img.isNull()) {
                    QApplication::clipboard()->setImage(img);
                }
                return;
            }
        };
        commands->addCommand(cmd);
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
    m_views.clear();
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
