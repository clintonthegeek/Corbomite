// SPDX-License-Identifier: GPL-3.0-or-later
#include "BacklinksPlugin.h"

#include "BacklinksView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/PluginContext.h"

#include <KPluginFactory>
#include <QDebug>

namespace Corbomite {

BacklinksPlugin::BacklinksPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

BacklinksPlugin::~BacklinksPlugin() = default;

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
