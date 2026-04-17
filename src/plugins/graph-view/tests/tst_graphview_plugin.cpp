// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include <KPluginMetaData>

#include "../GraphViewPlugin.h"
#include "corbomite/vault/PluginContext.h"

using namespace Corbomite;

class TestGraphViewPlugin : public QObject
{
    Q_OBJECT
private slots:
    void shellPluginCreateViewReturnsNull();
    void lifecycleLoadsAndUnloads();
};

static PluginMetaData makeMeta() { return PluginMetaData(KPluginMetaData{}); }

void TestGraphViewPlugin::shellPluginCreateViewReturnsNull()
{
    // Cluster Q Task 20 shipped GraphView as a shell plugin — the actual
    // View class still lives in CorbomiteApp. createView falls through to
    // the Plugin base default (nullptr). Main-area view-type registration
    // via ViewRegistrar is a tracked follow-up.
    GraphViewPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("ui.views"), QStringLiteral("workspace")});
    plugin.load(&ctx);
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

void TestGraphViewPlugin::lifecycleLoadsAndUnloads()
{
    GraphViewPlugin plugin;
    PluginContext ctx(makeMeta(), {});
    plugin.load(&ctx);
    QVERIFY(plugin.isLoaded());
    plugin.unload();
    QVERIFY(!plugin.isLoaded());
}

QTEST_MAIN(TestGraphViewPlugin)
#include "tst_graphview_plugin.moc"
