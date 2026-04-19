// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileExplorerPlugin.h"

#include "FileExplorerView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>

namespace Corbomite {

FileExplorerPlugin::FileExplorerPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

FileExplorerPlugin::~FileExplorerPlugin() = default;

void FileExplorerPlugin::onLoad(PluginContext *ctx)
{
    if (!ctx) return;

    // `file-explorer:reveal-file` — scrolls + selects the file in the
    // current active leaf inside the navigation dock. Consumed by the
    // EditableFileView view-header menu (Cluster R Task 2.8).
    //
    // Command callbacks are zero-arg, so we derive the target path from
    // WorkspaceController::activeFilePath. This matches the common case
    // where the user triggers reveal from the hamburger menu of the view
    // whose file they want to locate — the active leaf IS that view.
    //
    // QPointer-guard the plugin 'this' so a vault close during command
    // dispatch never dereferences freed memory.
    if (auto *commands = ctx->commands()) {
        QPointer<FileExplorerPlugin> self(this);
        Command reveal;
        reveal.id = QStringLiteral("reveal-file");
        reveal.name = i18n("Reveal active file in navigation");
        reveal.icon = QStringLiteral("edit-find");
        reveal.callback = [self, ctx] {
            if (!self) return;
            auto *ws = ctx->workspace();
            if (!ws) return;
            const QString path = ws->activeFilePath();
            if (path.isEmpty()) return;
            if (auto *fv = self->m_view)
                fv->revealPath(path);
        };
        commands->addCommand(reveal);
    }
}

QObject *FileExplorerPlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *vault = ctx->vault();
    auto *fileManager = ctx->fileManager();
    if (!vault || !fileManager) {
        qWarning() << "FileExplorerPlugin: vault.read+vault.write missing";
        return nullptr;
    }
    m_view = new FileExplorerView(
        vault, fileManager, ctx->workspace(),
        reinterpret_cast<QWidget *>(mainWindow));
    QObject::connect(m_view, &QObject::destroyed, this, [this] {
        m_view = nullptr;
    });
    return m_view;
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
