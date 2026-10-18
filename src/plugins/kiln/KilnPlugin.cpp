// SPDX-License-Identifier: GPL-3.0-or-later
#include "KilnPlugin.h"

#include "KilnRunner.h"
#include "KilnView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <QDebug>

namespace Corbomite {

KilnPlugin::KilnPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent)
{
}

KilnPlugin::~KilnPlugin() = default;

void KilnPlugin::onLoad(PluginContext *ctx)
{
    if (!ctx)
        return;

    // The runner is built here, once. A view that made its own would be a second
    // path to the same process, and the permission check would be in two places.
    if (auto *spawner = ctx->process())
        m_runner = std::make_unique<KilnRunner>(spawner, this);

    auto *commands = ctx->commands();
    if (!commands)
        return;

    // No Obsidian alias for any of these: there is no Obsidian command to be a second
    // name for, and `addCommand` prefixes with the plugin id, which is what a
    // Corbomite-native command should be called.
    Command open;
    open.id = QStringLiteral("open");
    open.name = i18n("Kiln: open the panel");
    open.icon = QStringLiteral("document-export");
    open.callback = [ctx] {
        if (auto *workspace = ctx->workspace())
            workspace->revealDockView(QStringLiteral("kiln"));
    };
    commands->addCommand(open);

    // Both commands raise the panel first. A run whose report appears in a panel the
    // user cannot see is a run they will do again.
    const auto withPanel = [this, ctx](void (KilnView::*action)()) {
        if (auto *workspace = ctx->workspace())
            workspace->revealDockView(QStringLiteral("kiln"));
        if (m_view)
            (m_view.data()->*action)();
    };

    Command check;
    check.id = QStringLiteral("check");
    check.name = i18n("Kiln: check the projection");
    check.icon = QStringLiteral("dialog-ok-apply");
    check.callback = [withPanel] { withPanel(&KilnView::runCheck); };
    commands->addCommand(check);

    Command project;
    project.id = QStringLiteral("project");
    project.name = i18n("Kiln: write the pages");
    project.icon = QStringLiteral("document-export");
    project.callback = [withPanel] { withPanel(&KilnView::runProject); };
    commands->addCommand(project);
}

void KilnPlugin::onExternalSettingsChange()
{
    // The panel writes the file it reads, so this also fires for the panel's own
    // autosave. KilnView::reloadFromStoreIfChanged compares before acting.
    if (m_view)
        m_view->reloadFromStoreIfChanged();
}

QObject *KilnPlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx)
        return nullptr;
    if (!ctx->vault()) {
        qWarning() << "KilnPlugin: vault.read missing; panel skipped";
        return nullptr;
    }
    if (!m_runner) {
        qWarning() << "KilnPlugin: process permission missing; panel skipped";
        return nullptr;
    }

    // reinterpret_cast, as every bundled plugin does here: `MainWindow` is only
    // forward-declared in a plugin's translation unit, so qobject_cast has no
    // complete type to test against. The host always hands over its own window.
    m_view = new KilnView(ctx, m_runner.get(), reinterpret_cast<QWidget *>(mainWindow));
    return m_view;
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(KilnPluginFactory, "metadata.json",
    registerPlugin<Corbomite::KilnPlugin>();)

#include "KilnPlugin.moc"
