// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../GraphControlsPanel.h"
#include "../GraphViewPlugin.h"

#include "corbomite/core/ViewRegistry.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

class TestGraphViewPlugin : public QObject
{
    Q_OBJECT
private slots:
    void registersGraphViewTypeOnLoad();
    void createViewReturnsControlsPanel();
    void skipsViewRegistrationWithoutUiViewsPermission();
};

static PluginMetaData makeMeta() { return PluginMetaData(KPluginMetaData{}); }

void TestGraphViewPlugin::registersGraphViewTypeOnLoad()
{
    ViewRegistry registry;
    // Seed a host ViewRegistry; plugin's ctx->views() registrar forwards here.
    FileSystemAdapter fs;
    QTemporaryDir dir;
    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);
    SQLiteIndex index;

    GraphViewPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("metadata.read"),
         QStringLiteral("workspace"), QStringLiteral("ui.views")});
    ctx.setCoreServices(&vault, nullptr, &cache, nullptr, nullptr,
                         &registry, nullptr, nullptr);
    ctx.setSearchIndex(&index);
    plugin.load(&ctx);

    QCOMPARE(registry.getTypeByExtension(QStringLiteral("md")), QString());
    QVERIFY(bool(registry.getViewCreatorByType(QStringLiteral("graph"))));
}

void TestGraphViewPlugin::createViewReturnsControlsPanel()
{
    GraphViewPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("ui.views")});
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view);
    QVERIFY(qobject_cast<GraphControlsPanel *>(view) != nullptr);
    // Second call returns the same instance (shared controls panel).
    QCOMPARE(plugin.createView(nullptr), view);
    delete view;
}

void TestGraphViewPlugin::skipsViewRegistrationWithoutUiViewsPermission()
{
    ViewRegistry registry;
    GraphViewPlugin plugin;
    PluginContext ctx(makeMeta(), {QStringLiteral("vault.read")});
    ctx.setCoreServices(nullptr, nullptr, nullptr, nullptr, nullptr,
                         &registry, nullptr, nullptr);
    plugin.load(&ctx);
    QVERIFY(!bool(registry.getViewCreatorByType(QStringLiteral("graph"))));
}

QTEST_MAIN(TestGraphViewPlugin)
#include "tst_graphview_plugin.moc"
