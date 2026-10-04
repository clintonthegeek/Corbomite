// SPDX-License-Identifier: GPL-3.0-or-later
#include "PropertiesPlugin.h"

#include "PropertiesView.h"

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

PropertiesPlugin::PropertiesPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

PropertiesPlugin::~PropertiesPlugin() = default;

void PropertiesPlugin::onLoad(PluginContext *ctx)
{
    if (!ctx) return;

    // Obsidian id `properties:open` (app/own/2039-vee.js:532) — reveals the
    // Properties dock panel.
    if (auto *commands = ctx->commands()) {
        Command open;
        open.id = QStringLiteral("properties:open");
        open.name = i18n("Open file properties");
        open.icon = QStringLiteral("document-properties");
        open.callback = [ctx] {
            if (auto *ws = ctx->workspace())
                ws->revealDockView(QStringLiteral("properties"));
        };
        commands->addCommandRaw(open);
        commands->addAlias(QStringLiteral("corbomite-properties:open"),
                           QStringLiteral("properties:open"));
    }
}

QObject *PropertiesPlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *metadata = ctx->metadataCache();
    auto *fileManager = ctx->fileManager();
    if (!metadata || !fileManager) {
        qWarning() << "PropertiesPlugin: metadata.read+vault.write missing; "
                      "view skipped";
        return nullptr;
    }
    return new PropertiesView(metadata, ctx->vault(), fileManager,
                                ctx->workspace(),
                                reinterpret_cast<QWidget *>(mainWindow));
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(PropertiesPluginFactory, "metadata.json",
    registerPlugin<Corbomite::PropertiesPlugin>();)

#include "PropertiesPlugin.moc"
