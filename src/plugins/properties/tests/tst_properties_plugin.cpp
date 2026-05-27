// SPDX-License-Identifier: GPL-3.0-or-later
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../PropertiesPlugin.h"
#include "../PropertiesView.h"
#include "../../../sidebar/PropertyRow.h"

#include <markoff/parser/YamlValue.h>

#include "corbomite/core/Command.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

class TestPropertiesPlugin : public QObject
{
    Q_OBJECT
private slots:
    void createsViewWhenMetadataAndWriteGranted();
    void returnsNullWhenVaultWriteMissing();
    void registersOpenCommandAndDispatchesRevealDockView();
    void classifiesComplexValuesAsNonEditable();
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

void TestPropertiesPlugin::createsViewWhenMetadataAndWriteGranted()
{
    PropertiesPlugin plugin;
    FileSystemAdapter fs;
    QTemporaryDir dir;
    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);
    FileManager fm(&vault, &cache);

    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view != nullptr);
    QVERIFY(qobject_cast<PropertiesView *>(view) != nullptr);
    delete view;
}

void TestPropertiesPlugin::returnsNullWhenVaultWriteMissing()
{
    PropertiesPlugin plugin;
    LinkResolver resolver;
    MetadataCache cache(resolver);
    PluginContext ctx(makeMeta(), {QStringLiteral("metadata.read")});
    ctx.setCoreServices(nullptr, nullptr, &cache, nullptr, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    plugin.load(&ctx);
    // No FileManager (missing vault.read/write) → createView returns null.
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

void TestPropertiesPlugin::registersOpenCommandAndDispatchesRevealDockView()
{
    CommandRegistry commands;
    ViewRegistry views;
    Workspace workspace(&views);

    PropertiesPlugin plugin;
    PluginContext ctx(makeMetaWithId(QStringLiteral("corbomite-properties")),
        {QStringLiteral("metadata.read"), QStringLiteral("workspace"),
         QStringLiteral("ui.commands")});
    ctx.setCoreServices(nullptr, nullptr, nullptr, nullptr, &workspace,
                         &commands, nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QVERIFY(commands.findCommand(QStringLiteral("corbomite-properties:open")));

    QSignalSpy spy(&workspace, &Workspace::revealDockViewRequested);
    QVERIFY(commands.executeById(QStringLiteral("corbomite-properties:open")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("properties"));
}

void TestPropertiesPlugin::classifiesComplexValuesAsNonEditable()
{
    using namespace Corbomite;
    using YV = Markoff::YamlValue;
    QString e;
    YV scalars = YV::parse(QStringLiteral(
        "s: text\nn: 3\nb: true\nlist:\n  - a\n  - b\n"), &e);
    QVERIFY(isEditableFrontmatterValue(scalars.get(QStringLiteral("s"))));
    QVERIFY(isEditableFrontmatterValue(scalars.get(QStringLiteral("n"))));
    QVERIFY(isEditableFrontmatterValue(scalars.get(QStringLiteral("b"))));
    QVERIFY(isEditableFrontmatterValue(scalars.get(QStringLiteral("list")))); // scalar list

    YV complex = YV::parse(QStringLiteral(
        "m:\n  k: v\nlistofmaps:\n  - x: 1\n"), &e);
    QVERIFY(!isEditableFrontmatterValue(complex.get(QStringLiteral("m"))));          // map
    QVERIFY(!isEditableFrontmatterValue(complex.get(QStringLiteral("listofmaps")))); // seq of maps
}

QTEST_MAIN(TestPropertiesPlugin)
#include "tst_properties_plugin.moc"
