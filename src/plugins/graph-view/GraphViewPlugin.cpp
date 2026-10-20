// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphViewPlugin.h"

#include "GraphControlsPanel.h"
#include "GraphView.h"
#include "GraphViewActions.h"
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
    m_commandsProxy = commandsProxy;

    // Ship the provider through the SAME proxy that carried the view type,
    // which is the whole point: a plugin can now own a ViewActions provider
    // without new plugin-API surface.
    // Ordered after registerView() below because a provider is meaningless
    // without its view type, and the registrar tears providers down first
    // precisely so this ordering stays safe.
    m_viewsProxy = ctx->views();

    if (auto *views = ctx->views()) {
        views->registerView(QStringLiteral("graph"),
            [this, commandsProxy](WorkspaceLeaf *leaf) -> View * {
                auto *view = new GraphView(leaf);
                view->setSearch(m_search);
                view->setVault(m_vault);
                view->setMetadataCache(m_metadata);
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

    // Provider construction is deferred to here (not inline above) so the
    // command dispatcher is fully wired: `graph_open_local` and
    // `graph_copy_screenshot` both delegate through it.
    m_graphActions = new GraphViewActions([this](const QString &commandId) {
        if (m_commandsProxy)
            m_commandsProxy->invoke(commandId);
    });
    if (m_viewsProxy)
        m_viewsProxy->registerViewActions(QStringLiteral("graph"), m_graphActions);

    // The `copy-screenshot` command. Grabs the last
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
    // LIFETIME — this is the load-bearing line. The provider's vtable
    // lives in THIS .so, so it has to be handed back to the registrar while
    // the library is still mapped, or KXMLGUIFactory keeps a client whose
    // code no longer exists. onUnload() is the one hook that provably runs
    // before the .so goes away; the registrar destructor is only the
    // abnormal-teardown fallback.
    //
    // Order matters: unregister the provider BEFORE the controls panel and
    // view pointers are cleared, so nothing can dispatch into a half-torn
    // plugin. Do NOT rely on ~GraphViewPlugin for this.
    if (m_viewsProxy)
        m_viewsProxy->unregisterViewActions(QStringLiteral("graph"));
    m_graphActions = nullptr;
    m_viewsProxy = nullptr;

    // ViewRegistrar's destructor (owned by PluginContext) unregisters
    // the "graph" type automatically. Controls panel, if still alive,
    // is parented to a MainWindow tool view which tears it down.
    m_vault = nullptr;
    m_search = nullptr;
    m_metadata = nullptr;
    m_views.clear();
}

QObject *GraphViewPlugin::createView(MainWindow *)
{
    // Retired. This returned a SINGLETON GraphControlsPanel that
    // MainWindow docked in a right-hand tool view and every GraphViewTab
    // wired with ~13 connect() calls and no disconnect — so with two graph
    // tabs open, one tab's slider drove both engines. Each tab
    // now owns its panel (GraphViewTab::controlsPanel), which fixes the bug
    // by construction. The "X-Corbomite-DockArea: right" metadata key is
    // gone too, so MainWindow stops creating the dock for it.
    return nullptr;
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(GraphViewPluginFactory, "metadata.json",
    registerPlugin<Corbomite::GraphViewPlugin>();)

#include "GraphViewPlugin.moc"
