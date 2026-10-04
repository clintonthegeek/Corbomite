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

    // Obsidian id `graph:open-local` (app/own/1848-Y1.js:763) — reveals the
    // Local Graph dock panel. Registered raw: CommandRegistrar would
    // namespace it to `corbomite-local-graph:open-local`, which nothing in a
    // vault's `.obsidian/hotkeys.json` can name. That Corbomite-native id
    // stays available as an alias.
    //
    // The slug passed to WorkspaceController::revealDockView is
    // `local-graph` (mapped by MainWindow to `corbomite-local-graph_panel`),
    // which is not the command id.
    if (auto *commands = ctx->commands()) {
        Command open;
        open.id = QStringLiteral("graph:open-local");
        open.name = i18n("Open local graph");
        open.icon = QStringLiteral("preferences-system-network");
        open.callback = [ctx] {
            if (auto *ws = ctx->workspace())
                ws->revealDockView(QStringLiteral("local-graph"));
        };
        commands->addCommandRaw(open);
        commands->addAlias(QStringLiteral("corbomite-local-graph:open-local"),
                           QStringLiteral("graph:open-local"));
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
