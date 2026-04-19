// SPDX-License-Identifier: GPL-3.0-or-later
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../OutlinePlugin.h"
#include "../OutlineView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

class TestOutlinePlugin : public QObject
{
    Q_OBJECT
private slots:
    void createsViewWhenVaultReadGranted();
    void returnsNullWhenVaultReadMissing();
    void registersOpenCommandAndDispatchesRevealDockView();
};

static PluginMetaData makeMeta() { return PluginMetaData(KPluginMetaData{}); }

static PluginMetaData makeMetaWithId(const QString &id)
{
    QJsonObject full;
    full.insert(QStringLiteral("KPlugin"),
        QJsonObject{{QStringLiteral("Id"), id},
                    {QStringLiteral("Name"), id}});
    return PluginMetaData(KPluginMetaData(full, id));
}

void TestOutlinePlugin::createsViewWhenVaultReadGranted()
{
    OutlinePlugin plugin;
    FileSystemAdapter fs;
    QTemporaryDir dir;
    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);

    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("metadata.read"),
         QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, nullptr, &cache, nullptr, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view != nullptr);
    QVERIFY(qobject_cast<OutlineView *>(view) != nullptr);
    delete view;
}

void TestOutlinePlugin::returnsNullWhenVaultReadMissing()
{
    OutlinePlugin plugin;
    PluginContext ctx(makeMeta(), {});
    plugin.load(&ctx);
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

void TestOutlinePlugin::registersOpenCommandAndDispatchesRevealDockView()
{
    CommandRegistry commands;
    ViewRegistry views;
    Workspace workspace(&views);

    OutlinePlugin plugin;
    PluginContext ctx(makeMetaWithId(QStringLiteral("corbomite-outline")),
        {QStringLiteral("metadata.read"), QStringLiteral("workspace"),
         QStringLiteral("ui.commands")});
    ctx.setCoreServices(nullptr, nullptr, nullptr, nullptr, &workspace,
                         &commands, nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QVERIFY(commands.findCommand(QStringLiteral("corbomite-outline:open")));

    QSignalSpy spy(&workspace, &Workspace::revealDockViewRequested);
    QVERIFY(commands.executeById(QStringLiteral("corbomite-outline:open")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("outline"));
}

QTEST_MAIN(TestOutlinePlugin)
#include "tst_outline_plugin.moc"
