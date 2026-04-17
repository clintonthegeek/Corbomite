// SPDX-License-Identifier: GPL-3.0-or-later
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../FileExplorerPlugin.h"
#include "../FileExplorerView.h"

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

class TestFileExplorerPlugin : public QObject
{
    Q_OBJECT
private slots:
    void createsViewWhenVaultGranted();
    void returnsNullWhenVaultMissing();
    void sessionStateRoundTripsEmptyWhenNoFolders();
};

static PluginMetaData makeMeta() { return PluginMetaData(KPluginMetaData{}); }

void TestFileExplorerPlugin::createsViewWhenVaultGranted()
{
    FileExplorerPlugin plugin;
    FileSystemAdapter fs;
    QTemporaryDir dir;
    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);
    FileManager fm(&vault, &cache);

    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("vault.events"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                         nullptr, nullptr);
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view != nullptr);
    QVERIFY(qobject_cast<FileExplorerView *>(view) != nullptr);
    delete view;
}

void TestFileExplorerPlugin::returnsNullWhenVaultMissing()
{
    FileExplorerPlugin plugin;
    PluginContext ctx(makeMeta(), {QStringLiteral("vault.read")});
    plugin.load(&ctx);
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

void TestFileExplorerPlugin::sessionStateRoundTripsEmptyWhenNoFolders()
{
    // saveSessionState returns empty object when nothing is expanded.
    FileExplorerPlugin plugin;
    FileSystemAdapter fs;
    QTemporaryDir dir;
    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);
    FileManager fm(&vault, &cache);

    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("vault.events")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                         nullptr, nullptr);
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view);
    const QJsonObject state = plugin.saveSessionState(view);
    QVERIFY(state.isEmpty());
    delete view;
}

QTEST_MAIN(TestFileExplorerPlugin)
#include "tst_fileexplorer_plugin.moc"
