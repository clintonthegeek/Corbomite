// SPDX-License-Identifier: GPL-3.0-or-later
#include "OutlinePlugin.h"

#include "OutlineView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KPluginFactory>
#include <QDebug>

namespace Corbomite {

OutlinePlugin::OutlinePlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

OutlinePlugin::~OutlinePlugin() = default;

QObject *OutlinePlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *vault = ctx->vault();
    if (!vault) {
        qWarning() << "OutlinePlugin: vault.read missing; view skipped";
        return nullptr;
    }
    return new OutlineView(ctx->metadataCache(), vault, ctx->workspace(),
                            reinterpret_cast<QWidget *>(mainWindow));
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(OutlinePluginFactory, "metadata.json",
    registerPlugin<Corbomite::OutlinePlugin>();)

#include "OutlinePlugin.moc"
