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
