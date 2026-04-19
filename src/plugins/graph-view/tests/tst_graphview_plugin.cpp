// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../GraphControlsPanel.h"
#include "../GraphViewPlugin.h"

#include "corbomite/core/Command.h"
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
    void registersCopyScreenshotCommandWhenUiCommandsGranted();
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
    ctx.setCoreServices(&vault, nullptr, &cache, &index, nullptr, nullptr,
                         &registry, nullptr, nullptr);
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
    ctx.setCoreServices(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                         &registry, nullptr, nullptr);
    plugin.load(&ctx);
    QVERIFY(!bool(registry.getViewCreatorByType(QStringLiteral("graph"))));
}

void TestGraphViewPlugin::registersCopyScreenshotCommandWhenUiCommandsGranted()
{
    // CommandRegistry must outlive PluginContext (CommandRegistrar's
    // destructor calls removeCommand on the registry during teardown).
    CommandRegistry commands;
    ViewRegistry registry;

    GraphViewPlugin plugin;
    PluginContext ctx(makeMetaWithId(QStringLiteral("corbomite-graph-view")),
        {QStringLiteral("ui.views"), QStringLiteral("ui.commands")});
    ctx.setCoreServices(nullptr, nullptr, nullptr, nullptr, nullptr,
                         &commands, &registry, nullptr, nullptr);
    plugin.load(&ctx);

    QVERIFY(commands.findCommand(
        QStringLiteral("corbomite-graph-view:copy-screenshot")) != nullptr);
}

QTEST_MAIN(TestGraphViewPlugin)
#include "tst_graphview_plugin.moc"
