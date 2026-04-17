// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include <KPluginMetaData>

#include "../BacklinksPlugin.h"
#include "../BacklinksView.h"

#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/PluginContext.h"

using namespace Corbomite;

class TestBacklinksPlugin : public QObject
{
    Q_OBJECT
private slots:
    void createsViewWhenMetadataReadGranted();
    void returnsNullWhenMetadataReadMissing();
};

static PluginMetaData makeMeta()
{
    return PluginMetaData(KPluginMetaData{});
}

void TestBacklinksPlugin::createsViewWhenMetadataReadGranted()
{
    BacklinksPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    LinkResolver resolver;
    MetadataCache cache(resolver);
    ctx.setCoreServices(nullptr, nullptr, &cache, nullptr,
                         nullptr, nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view != nullptr);
    QVERIFY(qobject_cast<BacklinksView *>(view) != nullptr);
    delete view;
}

void TestBacklinksPlugin::returnsNullWhenMetadataReadMissing()
{
    BacklinksPlugin plugin;
    PluginContext ctx(makeMeta(), {});
    plugin.load(&ctx);
    QObject *view = plugin.createView(nullptr);
    QCOMPARE(view, nullptr);
}

QTEST_MAIN(TestBacklinksPlugin)
#include "tst_backlinks_plugin.moc"
