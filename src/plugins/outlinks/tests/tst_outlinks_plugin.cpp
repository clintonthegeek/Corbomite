// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include <KPluginMetaData>

#include "../OutlinksPlugin.h"
#include "../OutlinksView.h"

#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/PluginContext.h"

using namespace Corbomite;

class TestOutlinksPlugin : public QObject
{
    Q_OBJECT
private slots:
    void createsViewWhenMetadataReadGranted();
    void returnsNullWhenMetadataReadMissing();
};

static PluginMetaData makeMeta() { return PluginMetaData(KPluginMetaData{}); }

void TestOutlinksPlugin::createsViewWhenMetadataReadGranted()
{
    OutlinksPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    LinkResolver resolver;
    MetadataCache cache(resolver);
    ctx.setCoreServices(nullptr, nullptr, &cache, nullptr,
                         nullptr, nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view != nullptr);
    QVERIFY(qobject_cast<OutlinksView *>(view) != nullptr);
    delete view;
}

void TestOutlinksPlugin::returnsNullWhenMetadataReadMissing()
{
    OutlinksPlugin plugin;
    PluginContext ctx(makeMeta(), {});
    plugin.load(&ctx);
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

QTEST_MAIN(TestOutlinksPlugin)
#include "tst_outlinks_plugin.moc"
