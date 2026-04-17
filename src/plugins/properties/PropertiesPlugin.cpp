// SPDX-License-Identifier: GPL-3.0-or-later
#include "PropertiesPlugin.h"

#include "PropertiesView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KPluginFactory>
#include <QDebug>

namespace Corbomite {

PropertiesPlugin::PropertiesPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

PropertiesPlugin::~PropertiesPlugin() = default;

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
