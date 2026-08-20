// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../GraphControlsPanel.h"
#include "../GraphView.h"
#include "../GraphViewPlugin.h"
#include "../GraphViewTab.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <forcegraph/ForceGraphView.h>

#include <QTransform>

using namespace Corbomite;

class TestGraphViewPlugin : public QObject
{
    Q_OBJECT
private slots:
    void registersGraphViewTypeOnLoad();
    void createViewReturnsControlsPanel();
    void skipsViewRegistrationWithoutUiViewsPermission();
    void registersCopyScreenshotCommandWhenUiCommandsGranted();
    void zoomDispatchesToViewportTransform();
    void capabilities();
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

// Cluster O Phase O1.T3 — View::zoomIn/Out/Reset must reach the real
// ForceGraphView viewport transform (previously the base View no-ops were
// never overridden, so graph had no zoom action at all — report §4.1).
void TestGraphViewPlugin::zoomDispatchesToViewportTransform()
{
    // GraphView::onOpen() only constructs its GraphViewTab once both a
    // search and vault proxy are set (empty-permission stubs are enough —
    // buildGraph()'s query surface gates on permissions and returns empty
    // rather than dereferencing the null backends).
    SearchProxy search(nullptr, {}, QStringLiteral("test"));
    VaultProxy vault(nullptr, {}, QStringLiteral("test"));

    // `host` must be declared before `view`: View::open() reparents `view`
    // under `host`, and locals destruct in reverse declaration order. With
    // `view` declared first, `host` would destruct first and (as `view`'s
    // new Qt-ownership parent) delete it — then `view`'s own stack
    // destructor would run a second time on already-freed memory.
    QWidget host;
    GraphView view(nullptr);
    view.setSearch(&search);
    view.setVault(&vault);
    view.open(&host);

    auto *tab = view.graphWidget();
    QVERIFY(tab);
    auto *gv = tab->graphView();
    QVERIFY(gv);

    const QTransform identity;
    QCOMPARE(gv->transform(), identity);

    View &base = view;
    base.zoomIn();
    QVERIFY2(gv->transform() != identity,
             "View::zoomIn() must reach the ForceGraphView viewport transform");

    const QTransform afterIn = gv->transform();
    base.zoomOut();
    QVERIFY2(gv->transform() != afterIn,
             "View::zoomOut() must reach the ForceGraphView viewport transform");

    base.zoomReset();
    QCOMPARE(gv->transform(), identity);
}

// Cluster O Phase O2.T1 — Tier-B capability surface. GraphView adds no
// overrides beyond the zoom virtuals above: canZoom() stays the base
// default (true, matches the real zoom), everything else is a correct
// "no" for graph today.
void TestGraphViewPlugin::capabilities()
{
    GraphView view(nullptr);
    View &base = view;
    QVERIFY2(base.canZoom(), "graph must answer canZoom() true (base default)");
    QVERIFY(!base.canEdit());
    QVERIFY(!base.canSave());
    QVERIFY(!base.canFind());
    QVERIFY(!base.hasSelection());
    QVERIFY(!base.canUndo());
    QVERIFY(!base.canRedo());
}

QTEST_MAIN(TestGraphViewPlugin)
#include "tst_graphview_plugin.moc"
