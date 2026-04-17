// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include <KPluginMetaData>

#include "../SearchPlugin.h"
#include "../SearchView.h"

#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/vault/PluginContext.h"

using namespace Corbomite;

class TestSearchPlugin : public QObject
{
    Q_OBJECT
private slots:
    void createsViewWhenMetadataReadGranted();
    void returnsNullWhenIndexMissing();
    void focusOverrideDispatchesToSearchInput();
};

static PluginMetaData makeMeta() { return PluginMetaData(KPluginMetaData{}); }

void TestSearchPlugin::createsViewWhenMetadataReadGranted()
{
    SearchPlugin plugin;
    LinkResolver resolver;
    MetadataCache cache(resolver);
    SQLiteIndex index;

    PluginContext ctx(makeMeta(),
        {QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    ctx.setCoreServices(nullptr, nullptr, &cache, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    ctx.setSearchIndex(&index);
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view != nullptr);
    QVERIFY(qobject_cast<SearchView *>(view) != nullptr);
    delete view;
}

void TestSearchPlugin::returnsNullWhenIndexMissing()
{
    SearchPlugin plugin;
    LinkResolver resolver;
    MetadataCache cache(resolver);
    PluginContext ctx(makeMeta(), {QStringLiteral("metadata.read")});
    ctx.setCoreServices(nullptr, nullptr, &cache, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    // No setSearchIndex — searchIndex() returns nullptr.
    plugin.load(&ctx);
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

void TestSearchPlugin::focusOverrideDispatchesToSearchInput()
{
    // Non-SearchView input falls through to Plugin::focus default.
    // Passing nullptr must be a safe no-op (default ignores non-widget).
    SearchPlugin plugin;
    plugin.focus(nullptr);
    QObject notAView;
    plugin.focus(&notAView);
    // If we had a SearchView here, focus() would land on its QLineEdit
    // via focusSearchInput(). Covered end-to-end by tst_e2e_gui.
}

QTEST_MAIN(TestSearchPlugin)
#include "tst_search_plugin.moc"
