// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../OutlinePlugin.h"
#include "../OutlineView.h"

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
};

static PluginMetaData makeMeta() { return PluginMetaData(KPluginMetaData{}); }

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

QTEST_MAIN(TestOutlinePlugin)
#include "tst_outline_plugin.moc"
