// SPDX-License-Identifier: GPL-3.0-or-later
#include "OutlinksPlugin.h"

#include "OutlinksView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <QDebug>

namespace Corbomite {

OutlinksPlugin::OutlinksPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

OutlinksPlugin::~OutlinksPlugin() = default;

void OutlinksPlugin::onLoad(PluginContext *ctx)
{
    if (!ctx) return;

    // Register `outlinks:open` — reveals the Outlinks dock panel.
    // Consumed by Cluster R MarkdownView.onMoreOptionsMenu view.linked submenu.
    if (auto *commands = ctx->commands()) {
        Command open;
        open.id = QStringLiteral("open");
        open.name = i18n("Open outgoing links");
        open.icon = QStringLiteral("go-next");
        open.callback = [ctx] {
            if (auto *ws = ctx->workspace())
                ws->revealDockView(QStringLiteral("outlinks"));
        };
        commands->addCommand(open);
    }
}

QObject *OutlinksPlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *metadata = ctx->metadataCache();
    if (!metadata) {
        qWarning() << "OutlinksPlugin: metadata.read missing; view skipped";
        return nullptr;
    }
    return new OutlinksView(metadata, ctx->vault(), ctx->fileManager(),
                              ctx->workspace(),
                              reinterpret_cast<QWidget *>(mainWindow));
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(OutlinksPluginFactory, "metadata.json",
    registerPlugin<Corbomite::OutlinksPlugin>();)

#include "OutlinksPlugin.moc"
