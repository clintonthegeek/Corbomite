// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileExplorerPlugin.h"

#include "FileExplorerView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"

#include <KPluginFactory>
#include <QDebug>

namespace Corbomite {

FileExplorerPlugin::FileExplorerPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

FileExplorerPlugin::~FileExplorerPlugin() = default;

QObject *FileExplorerPlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *vault = ctx->vaultRaw();
    auto *fileManager = ctx->fileManager();
    if (!vault || !fileManager) {
        qWarning() << "FileExplorerPlugin: vault.read+vault.write missing";
        return nullptr;
    }
    return new FileExplorerView(vault, fileManager, ctx->workspace(),
                                  reinterpret_cast<QWidget *>(mainWindow));
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(FileExplorerPluginFactory, "metadata.json",
    registerPlugin<Corbomite::FileExplorerPlugin>();)

#include "FileExplorerPlugin.moc"
