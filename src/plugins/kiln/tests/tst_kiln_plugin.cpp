// SPDX-License-Identifier: GPL-3.0-or-later
#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../KilnPlugin.h"
#include "../KilnView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

class TestKilnPlugin : public QObject
{
    Q_OBJECT
private slots:
    void createsThePanelWhenItHasVaultReadAndProcess();
    void skipsThePanelWithoutVaultRead();
    void skipsThePanelWithoutTheProcessPermission();
    void registersItsCommandsUnderItsOwnNamespace();
    void openCommandRevealsThePanel();
};

/// A loaded plugin with a real vault, the core services, and whatever permissions the
/// case asks for.
///
/// The declaration order here is load-bearing. `PluginContext` owns the proxies, and a
/// proxy's destructor unregisters itself from the registry it was built on — so every
/// registry a plugin touches has to outlive the context. Declare one of these after
/// `ctx` and teardown is a use-after-free in `CommandRegistrar::~CommandRegistrar`,
/// which is what this harness did the first time.
struct Harness
{
    QTemporaryDir dir;
    FileSystemAdapter fs;
    Vault vault{&fs};

    CommandRegistry commands;
    ViewRegistry views;
    Workspace workspace{&views};
    PluginContext ctx;

    explicit Harness(const QStringList &permissions)
        : ctx(makeMeta(), permissionSet(permissions))
    {
        vault.load(dir.path());
        ctx.setCoreServices(&vault, nullptr, nullptr, nullptr, &workspace, &commands, nullptr,
                            nullptr, nullptr);
    }

    static PluginMetaData makeMeta()
    {
        QJsonObject full;
        full.insert(QStringLiteral("KPlugin"),
                    QJsonObject{{QStringLiteral("Id"), QStringLiteral("corbomite-kiln")},
                                {QStringLiteral("Name"), QStringLiteral("Kiln")}});
        return PluginMetaData(KPluginMetaData(full, QStringLiteral("corbomite-kiln")));
    }

    static QSet<QString> permissionSet(const QStringList &permissions)
    {
        QSet<QString> set;
        for (const QString &permission : permissions)
            set.insert(permission);
        return set;
    }
};

void TestKilnPlugin::createsThePanelWhenItHasVaultReadAndProcess()
{
    Harness harness({QStringLiteral("vault.read"), QStringLiteral("config"),
                     QStringLiteral("process"), QStringLiteral("ui.commands")});
    KilnPlugin plugin;
    plugin.load(&harness.ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view != nullptr);
    QVERIFY(qobject_cast<KilnView *>(view) != nullptr);
    delete view;
}

void TestKilnPlugin::skipsThePanelWithoutVaultRead()
{
    Harness harness({QStringLiteral("process"), QStringLiteral("config")});
    KilnPlugin plugin;
    plugin.load(&harness.ctx);
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

void TestKilnPlugin::skipsThePanelWithoutTheProcessPermission()
{
    // A panel that cannot run anything is a panel that lies about being able to.
    Harness harness({QStringLiteral("vault.read"), QStringLiteral("config")});
    KilnPlugin plugin;
    plugin.load(&harness.ctx);
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

void TestKilnPlugin::registersItsCommandsUnderItsOwnNamespace()
{
    // `addCommand`, not `addCommandRaw`: there is no Obsidian command for these to be
    // a second name for, so they get the plugin's own prefix.
    Harness harness({QStringLiteral("vault.read"), QStringLiteral("ui.commands"),
                     QStringLiteral("workspace")});
    KilnPlugin plugin;
    plugin.load(&harness.ctx);

    QVERIFY(harness.commands.findCommand(QStringLiteral("corbomite-kiln:open")));
    QVERIFY(harness.commands.findCommand(QStringLiteral("corbomite-kiln:check")));
    QVERIFY(harness.commands.findCommand(QStringLiteral("corbomite-kiln:project")));
    QVERIFY(harness.commands.isAvailable(QStringLiteral("corbomite-kiln:check")));
}

void TestKilnPlugin::openCommandRevealsThePanel()
{
    Harness harness({QStringLiteral("vault.read"), QStringLiteral("ui.commands"),
                     QStringLiteral("workspace")});
    KilnPlugin plugin;
    plugin.load(&harness.ctx);

    QSignalSpy spy(&harness.workspace, &Workspace::revealDockViewRequested);
    QVERIFY(harness.commands.executeById(QStringLiteral("corbomite-kiln:open")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("kiln"));
}

QTEST_MAIN(TestKilnPlugin)
#include "tst_kiln_plugin.moc"
