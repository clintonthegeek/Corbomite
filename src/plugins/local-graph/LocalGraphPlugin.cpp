// SPDX-License-Identifier: GPL-3.0-or-later
#include "LocalGraphPlugin.h"

#include "LocalGraphView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/vault/PluginContext.h"

#include <KPluginFactory>
#include <QDebug>

namespace Corbomite {

LocalGraphPlugin::LocalGraphPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

LocalGraphPlugin::~LocalGraphPlugin() = default;

QObject *LocalGraphPlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *vault = ctx->vaultRaw();
    auto *index = ctx->searchIndex();
    auto *metadata = ctx->metadataCache();
    if (!vault || !index || !metadata) {
        qWarning() << "LocalGraphPlugin: vault.read+metadata.read missing";
        return nullptr;
    }
    return new LocalGraphView(index, vault, metadata, ctx->workspace(),
                                reinterpret_cast<QWidget *>(mainWindow));
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(LocalGraphPluginFactory, "metadata.json",
    registerPlugin<Corbomite::LocalGraphPlugin>();)

#include "LocalGraphPlugin.moc"
