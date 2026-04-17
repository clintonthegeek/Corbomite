// SPDX-License-Identifier: GPL-3.0-or-later
#include "OutlinksPlugin.h"

#include "OutlinksView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KPluginFactory>
#include <QDebug>

namespace Corbomite {

OutlinksPlugin::OutlinksPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

OutlinksPlugin::~OutlinksPlugin() = default;

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
