// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../PropertiesPlugin.h"
#include "../PropertiesView.h"

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

class TestPropertiesPlugin : public QObject
{
    Q_OBJECT
private slots:
    void createsViewWhenMetadataAndWriteGranted();
    void returnsNullWhenVaultWriteMissing();
};

static PluginMetaData makeMeta() { return PluginMetaData(KPluginMetaData{}); }

void TestPropertiesPlugin::createsViewWhenMetadataAndWriteGranted()
{
    PropertiesPlugin plugin;
    FileSystemAdapter fs;
    QTemporaryDir dir;
    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);
    FileManager fm(&vault, &cache);

    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view != nullptr);
    QVERIFY(qobject_cast<PropertiesView *>(view) != nullptr);
    delete view;
}

void TestPropertiesPlugin::returnsNullWhenVaultWriteMissing()
{
    PropertiesPlugin plugin;
    LinkResolver resolver;
    MetadataCache cache(resolver);
    PluginContext ctx(makeMeta(), {QStringLiteral("metadata.read")});
    ctx.setCoreServices(nullptr, nullptr, &cache, nullptr, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    plugin.load(&ctx);
    // No FileManager (missing vault.read/write) → createView returns null.
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

QTEST_MAIN(TestPropertiesPlugin)
#include "tst_properties_plugin.moc"
