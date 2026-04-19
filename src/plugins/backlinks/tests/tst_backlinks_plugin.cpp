// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>

#include <KPluginMetaData>

#include "../BacklinksPlugin.h"
#include "../BacklinksView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
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
    void registersOpenCommandAndDispatchesRevealDockView();
};

static PluginMetaData makeMeta()
{
    return PluginMetaData(KPluginMetaData{});
}

static PluginMetaData makeMetaWithId(const QString &id)
{
    QJsonObject full;
    full.insert(QStringLiteral("KPlugin"),
        QJsonObject{{QStringLiteral("Id"), id},
                    {QStringLiteral("Name"), id}});
    return PluginMetaData(KPluginMetaData(full, id));
}

void TestBacklinksPlugin::createsViewWhenMetadataReadGranted()
{
    BacklinksPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    LinkResolver resolver;
    MetadataCache cache(resolver);
    ctx.setCoreServices(nullptr, nullptr, &cache, nullptr, nullptr,
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

void TestBacklinksPlugin::registersOpenCommandAndDispatchesRevealDockView()
{
    // CommandRegistry/Workspace must outlive PluginContext — CommandRegistrar's
    // destructor calls CommandRegistry::removeCommand during PluginContext
    // teardown, so the registry must still be alive.
    LinkResolver resolver;
    MetadataCache cache(resolver);
    CommandRegistry commands;
    ViewRegistry views;
    Workspace workspace(&views);

    BacklinksPlugin plugin;
    PluginContext ctx(makeMetaWithId(QStringLiteral("corbomite-backlinks")),
        {QStringLiteral("metadata.read"), QStringLiteral("workspace"),
         QStringLiteral("ui.commands")});
    ctx.setCoreServices(nullptr, nullptr, &cache, nullptr, &workspace,
                         &commands, nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    auto *cmd = commands.findCommand(QStringLiteral("corbomite-backlinks:open"));
    QVERIFY(cmd != nullptr);

    QSignalSpy spy(&workspace, &Workspace::revealDockViewRequested);
    QVERIFY(commands.executeById(QStringLiteral("corbomite-backlinks:open")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("backlinks"));
}

QTEST_MAIN(TestBacklinksPlugin)
#include "tst_backlinks_plugin.moc"
