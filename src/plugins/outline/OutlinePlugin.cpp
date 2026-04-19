// SPDX-License-Identifier: GPL-3.0-or-later
#include "OutlinePlugin.h"

#include "OutlineView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <QDebug>

namespace Corbomite {

OutlinePlugin::OutlinePlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

OutlinePlugin::~OutlinePlugin() = default;

void OutlinePlugin::onLoad(PluginContext *ctx)
{
    if (!ctx) return;

    // Register `outline:open` — reveals the Outline dock panel.
    // Consumed by Cluster R MarkdownView.onMoreOptionsMenu view.linked submenu.
    if (auto *commands = ctx->commands()) {
        Command open;
        open.id = QStringLiteral("open");
        open.name = i18n("Open outline");
        open.icon = QStringLiteral("view-list-tree");
        open.callback = [ctx] {
            if (auto *ws = ctx->workspace())
                ws->revealDockView(QStringLiteral("outline"));
        };
        commands->addCommand(open);
    }
}

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
