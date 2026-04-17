// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileExplorerPlugin.h"

#include "FileExplorerView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"

#include <KPluginFactory>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

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

QJsonObject FileExplorerPlugin::saveSessionState(QObject *view) const
{
    auto *fv = qobject_cast<FileExplorerView *>(view);
    if (!fv) return {};
    const QStringList folders = fv->expandedFolderPaths();
    if (folders.isEmpty()) return {};
    QJsonArray arr;
    for (const QString &p : folders) arr.append(p);
    QJsonObject out;
    out.insert(QStringLiteral("expandedFolders"), arr);
    return out;
}

void FileExplorerPlugin::loadSessionState(QObject *view,
                                          const QJsonObject &state)
{
    auto *fv = qobject_cast<FileExplorerView *>(view);
    if (!fv) return;
    QStringList folders;
    const QJsonArray arr = state.value(QStringLiteral("expandedFolders")).toArray();
    for (const auto &v : arr) if (v.isString()) folders.append(v.toString());
    if (!folders.isEmpty()) fv->setExpandedFolderPaths(folders);
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(FileExplorerPluginFactory, "metadata.json",
    registerPlugin<Corbomite::FileExplorerPlugin>();)

#include "FileExplorerPlugin.moc"
