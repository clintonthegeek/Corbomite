// SPDX-License-Identifier: GPL-3.0-or-later
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../LocalGraphPlugin.h"
#include "../LocalGraphView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

class TestLocalGraphPlugin : public QObject
{
    Q_OBJECT
private slots:
    void createsViewWhenVaultAndMetadataGranted();
    void returnsNullWhenVaultMissing();
    void registersOpenLocalCommandAndDispatchesRevealDockView();
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

void TestLocalGraphPlugin::createsViewWhenVaultAndMetadataGranted()
{
    LocalGraphPlugin plugin;
    FileSystemAdapter fs;
    QTemporaryDir dir;
    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);
    SQLiteIndex index;

    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("metadata.read"),
         QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, nullptr, &cache, &index, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view != nullptr);
    QVERIFY(qobject_cast<LocalGraphView *>(view) != nullptr);
    delete view;
}

void TestLocalGraphPlugin::returnsNullWhenVaultMissing()
{
    LocalGraphPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("metadata.read")});
    // No setCoreServices → vault() / metadataCache() / search() all null.
    plugin.load(&ctx);
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

void TestLocalGraphPlugin::registersOpenLocalCommandAndDispatchesRevealDockView()
{
    CommandRegistry commands;
    ViewRegistry views;
    Workspace workspace(&views);

    LocalGraphPlugin plugin;
    PluginContext ctx(makeMetaWithId(QStringLiteral("corbomite-local-graph")),
        {QStringLiteral("metadata.read"), QStringLiteral("workspace"),
         QStringLiteral("ui.commands")});
    ctx.setCoreServices(nullptr, nullptr, nullptr, nullptr, &workspace,
                         &commands, nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QVERIFY(commands.findCommand(QStringLiteral("corbomite-local-graph:open-local")));

    QSignalSpy spy(&workspace, &Workspace::revealDockViewRequested);
    QVERIFY(commands.executeById(QStringLiteral("corbomite-local-graph:open-local")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("local-graph"));
}

QTEST_MAIN(TestLocalGraphPlugin)
#include "tst_localgraph_plugin.moc"
