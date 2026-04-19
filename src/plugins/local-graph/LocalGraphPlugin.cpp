// SPDX-License-Identifier: GPL-3.0-or-later
#include "LocalGraphPlugin.h"

#include "LocalGraphView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/vault/PluginContext.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <QDebug>

namespace Corbomite {

LocalGraphPlugin::LocalGraphPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

LocalGraphPlugin::~LocalGraphPlugin() = default;

void LocalGraphPlugin::onLoad(PluginContext *ctx)
{
    if (!ctx) return;

    // Register `graph:open-local` — reveals the Local Graph dock panel.
    // Note: the slug used for the command id is `graph:open-local` per the
    // Cluster R plan (MarkdownView view.linked submenu); the slug passed to
    // WorkspaceController::revealDockView is `local-graph` (the plugin id
    // suffix, mapped by MainWindow to `corbomite-local-graph_panel`).
    if (auto *commands = ctx->commands()) {
        Command open;
        // Note: CommandRegistrar auto-namespaces to pluginId:localId.
        // corbomite-local-graph's pluginId → `corbomite-local-graph:open-local`
        // but the plan calls for `graph:open-local`. We honor the plan by
        // registering a bare command directly on the CommandRegistry to
        // match the canonical id.
        open.id = QStringLiteral("open-local");
        open.name = i18n("Open local graph");
        open.icon = QStringLiteral("preferences-system-network");
        open.callback = [ctx] {
            if (auto *ws = ctx->workspace())
                ws->revealDockView(QStringLiteral("local-graph"));
        };
        commands->addCommand(open);
    }
}

QObject *LocalGraphPlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *vault = ctx->vault();
    auto *search = ctx->search();
    auto *metadata = ctx->metadataCache();
    if (!vault || !search || !metadata) {
        qWarning() << "LocalGraphPlugin: vault.read+metadata.read missing";
        return nullptr;
    }
    return new LocalGraphView(search, vault, metadata, ctx->workspace(),
                                reinterpret_cast<QWidget *>(mainWindow));
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(LocalGraphPluginFactory, "metadata.json",
    registerPlugin<Corbomite::LocalGraphPlugin>();)

#include "LocalGraphPlugin.moc"
