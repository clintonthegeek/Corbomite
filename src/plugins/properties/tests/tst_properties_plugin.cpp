// SPDX-License-Identifier: GPL-3.0-or-later
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../PropertiesPlugin.h"
#include "../PropertiesView.h"
#include "../../../sidebar/PropertyRow.h"

#include <markoff/parser/Document.h>
#include <markoff/parser/YamlValue.h>

#include "corbomite/core/Command.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/models/PropertyType.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/TFile.h"
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
    void addThenWritePersistsTypedKeysInOrder();
    void deleteRemovesKeyFromDisk();
    void renameChangesKeyPreservingPositionAndValue();
    void moveReordersPersistedKeys();
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

void TestPropertiesPlugin::addThenWritePersistsTypedKeysInOrder()
{
    using namespace Corbomite;
    FileSystemAdapter fs; QTemporaryDir dir; Vault vault(&fs); vault.load(dir.path());
    LinkResolver resolver; MetadataCache cache(resolver); FileManager fm(&vault, &cache);
    TFile *note = vault.create(QStringLiteral("note.md"), "---\nexisting: 1\n---\nbody\n");
    QVERIFY(note);

    PropertiesPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);
    plugin.load(&ctx);
    auto *view = qobject_cast<PropertiesView *>(plugin.createView(nullptr));
    QVERIFY(view);
    view->setActiveFileForTest(QStringLiteral("note.md"));

    view->addProperty(QStringLiteral("title"), PropertyType::Text);
    view->setRowValueForTest(QStringLiteral("title"), QStringLiteral("Hello"));
    view->flushPendingWrite();

    auto doc = Markoff::Document::fromMarkdown(
        QString::fromUtf8(vault.read(note)));
    QVERIFY(doc->parsedFrontmatter().keys().contains(QStringLiteral("title")));
    QCOMPARE(doc->parsedFrontmatter().get(QStringLiteral("title")).asString(),
             QStringLiteral("Hello"));
    delete view;
}

void TestPropertiesPlugin::deleteRemovesKeyFromDisk()
{
    using namespace Corbomite;
    FileSystemAdapter fs; QTemporaryDir dir; Vault vault(&fs); vault.load(dir.path());
    LinkResolver resolver; MetadataCache cache(resolver); FileManager fm(&vault, &cache);
    TFile *note = vault.create(QStringLiteral("n.md"), "---\nkeep: 1\ndrop: 2\n---\nx\n");
    QVERIFY(note);

    PropertiesPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);
    plugin.load(&ctx);
    auto *view = qobject_cast<PropertiesView *>(plugin.createView(nullptr));
    QVERIFY(view);
    view->setActiveFileForTest(QStringLiteral("n.md"));
    view->addProperty(QStringLiteral("keep"), PropertyType::Text);
    view->setRowValueForTest(QStringLiteral("keep"), QStringLiteral("1"));
    view->addProperty(QStringLiteral("drop"), PropertyType::Text);
    view->setRowValueForTest(QStringLiteral("drop"), QStringLiteral("2"));

    view->deleteProperty(QStringLiteral("drop"));
    view->flushPendingWrite();

    auto doc = Markoff::Document::fromMarkdown(
        QString::fromUtf8(vault.read(note)));
    QVERIFY(!doc->parsedFrontmatter().keys().contains(QStringLiteral("drop")));
    QVERIFY(doc->parsedFrontmatter().keys().contains(QStringLiteral("keep")));
    delete view;
}

void TestPropertiesPlugin::renameChangesKeyPreservingPositionAndValue()
{
    using namespace Corbomite;
    FileSystemAdapter fs; QTemporaryDir dir; Vault vault(&fs); vault.load(dir.path());
    LinkResolver resolver; MetadataCache cache(resolver); FileManager fm(&vault, &cache);
    TFile *note = vault.create(QStringLiteral("n.md"), "---\na: x\nb: y\n---\nbody\n");
    QVERIFY(note);

    PropertiesPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);
    plugin.load(&ctx);
    auto *view = qobject_cast<PropertiesView *>(plugin.createView(nullptr));
    QVERIFY(view);
    view->setActiveFileForTest(QStringLiteral("n.md"));
    view->addProperty(QStringLiteral("a"), PropertyType::Text);
    view->setRowValueForTest(QStringLiteral("a"), QStringLiteral("x"));
    view->addProperty(QStringLiteral("b"), PropertyType::Text);
    view->setRowValueForTest(QStringLiteral("b"), QStringLiteral("y"));

    QVERIFY(view->renameProperty(QStringLiteral("a"), QStringLiteral("alpha")));
    QVERIFY(!view->renameProperty(QStringLiteral("alpha"), QStringLiteral("b"))); // dup rejected
    view->flushPendingWrite();

    auto doc = Markoff::Document::fromMarkdown(
        QString::fromUtf8(vault.read(note)));
    QCOMPARE(doc->parsedFrontmatter().keys(),
             QStringList({QStringLiteral("alpha"), QStringLiteral("b")}));
    QCOMPARE(doc->parsedFrontmatter().get(QStringLiteral("alpha")).asString(),
             QStringLiteral("x"));
    delete view;
}

void TestPropertiesPlugin::moveReordersPersistedKeys()
{
    using namespace Corbomite;
    FileSystemAdapter fs; QTemporaryDir dir; Vault vault(&fs); vault.load(dir.path());
    LinkResolver resolver; MetadataCache cache(resolver); FileManager fm(&vault, &cache);
    TFile *note = vault.create(QStringLiteral("n.md"),
                               "---\none: 1\ntwo: 2\nthree: 3\n---\nb\n");
    QVERIFY(note);

    PropertiesPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);
    plugin.load(&ctx);
    auto *view = qobject_cast<PropertiesView *>(plugin.createView(nullptr));
    QVERIFY(view);
    view->setActiveFileForTest(QStringLiteral("n.md"));
    const QStringList keys{QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("three")};
    for (const QString &k : keys) {
        view->addProperty(k, PropertyType::Text);
        view->setRowValueForTest(k, QStringLiteral("v"));
    }

    view->moveProperty(2, 0);   // move "three" to the front
    view->flushPendingWrite();

    auto doc = Markoff::Document::fromMarkdown(
        QString::fromUtf8(vault.read(note)));
    QCOMPARE(doc->parsedFrontmatter().keys(),
             QStringList({QStringLiteral("three"), QStringLiteral("one"),
                          QStringLiteral("two")}));
    delete view;
}

QTEST_MAIN(TestPropertiesPlugin)
#include "tst_properties_plugin.moc"
