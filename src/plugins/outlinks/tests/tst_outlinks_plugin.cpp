// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>

#include <KPluginMetaData>

#include "../OutlinksPlugin.h"
#include "../OutlinksView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
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
    void registersOpenCommandAndDispatchesRevealDockView();
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

void TestOutlinksPlugin::createsViewWhenMetadataReadGranted()
{
    OutlinksPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    LinkResolver resolver;
    MetadataCache cache(resolver);
    ctx.setCoreServices(nullptr, nullptr, &cache, nullptr, nullptr,
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

void TestOutlinksPlugin::registersOpenCommandAndDispatchesRevealDockView()
{
    LinkResolver resolver;
    MetadataCache cache(resolver);
    CommandRegistry commands;
    ViewRegistry views;
    Workspace workspace(&views);

    OutlinksPlugin plugin;
    PluginContext ctx(makeMetaWithId(QStringLiteral("corbomite-outlinks")),
        {QStringLiteral("metadata.read"), QStringLiteral("workspace"),
         QStringLiteral("ui.commands")});
    ctx.setCoreServices(nullptr, nullptr, &cache, nullptr, &workspace,
                         &commands, nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QVERIFY(commands.findCommand(QStringLiteral("corbomite-outlinks:open")));

    QSignalSpy spy(&workspace, &Workspace::revealDockViewRequested);
    QVERIFY(commands.executeById(QStringLiteral("corbomite-outlinks:open")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("outlinks"));
}

QTEST_MAIN(TestOutlinksPlugin)
#include "tst_outlinks_plugin.moc"
