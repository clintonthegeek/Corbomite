// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../LocalGraphPlugin.h"
#include "../LocalGraphView.h"

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
};

static PluginMetaData makeMeta() { return PluginMetaData(KPluginMetaData{}); }

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
    ctx.setCoreServices(&vault, nullptr, &cache, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    ctx.setSearchIndex(&index);
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
    // No setCoreServices → vaultRaw() / metadata() / searchIndex() all null.
    plugin.load(&ctx);
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

QTEST_MAIN(TestLocalGraphPlugin)
#include "tst_localgraph_plugin.moc"
