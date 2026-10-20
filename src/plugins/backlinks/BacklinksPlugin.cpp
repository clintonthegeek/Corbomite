// SPDX-License-Identifier: GPL-3.0-or-later
#include "BacklinksPlugin.h"

#include "BacklinksView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/PluginContext.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <QDebug>

namespace Corbomite {

BacklinksPlugin::BacklinksPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

BacklinksPlugin::~BacklinksPlugin() = default;

void BacklinksPlugin::onLoad(PluginContext *ctx)
{
    if (!ctx) return;

    // Obsidian id `backlink:open` (app/own/1961-Q3.js:11) — reveals the
    // Backlinks dock panel. Registered raw so the canonical id survives;
    // `.obsidian/hotkeys.json` is keyed by it
    // (docs/obsidian-command-ids.md). The Corbomite-native
    // `corbomite-backlinks:open` that MarkdownFileView and CanvasFileView
    // dispatch stays available as an alias.
    if (auto *commands = ctx->commands()) {
        Command open;
        open.id = QStringLiteral("backlink:open");
        open.name = i18n("Open backlinks");
        open.icon = QStringLiteral("go-previous");
        open.callback = [ctx] {
            if (auto *ws = ctx->workspace())
                ws->revealDockView(QStringLiteral("backlinks"));
        };
        commands->addCommandRaw(open);
        commands->addAlias(QStringLiteral("corbomite-backlinks:open"),
                           QStringLiteral("backlink:open"));
    }
}

QObject *BacklinksPlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *metadata = ctx->metadataCache();
    auto *workspace = ctx->workspace();
    if (!metadata) {
        qWarning() << "BacklinksPlugin: metadata.read permission missing; "
                      "view not created.";
        return nullptr;
    }
    return new BacklinksView(metadata, workspace,
                              reinterpret_cast<QWidget *>(mainWindow));
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(BacklinksPluginFactory, "metadata.json",
    registerPlugin<Corbomite::BacklinksPlugin>();)

#include "BacklinksPlugin.moc"
