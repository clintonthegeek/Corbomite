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

    // Register `backlinks:open` command — reveals the Backlinks dock panel.
    // Consumed by Cluster R MarkdownView.onMoreOptionsMenu view.linked submenu.
    if (auto *commands = ctx->commands()) {
        Command open;
        open.id = QStringLiteral("open");
        open.name = i18n("Open backlinks");
        open.icon = QStringLiteral("go-previous");
        open.callback = [ctx] {
            if (auto *ws = ctx->workspace())
                ws->revealDockView(QStringLiteral("backlinks"));
        };
        commands->addCommand(open);
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
