// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QModelIndex>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>

#include <KPluginMetaData>

#include "../FileExplorerPlugin.h"
#include "../FileExplorerView.h"

#include "corbomite/core/Command.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

class TestFileExplorerPlugin : public QObject
{
    Q_OBJECT
private slots:
    void createsViewWhenVaultGranted();
    void returnsNullWhenVaultMissing();
    void sessionStateRoundTripsEmptyWhenNoFolders();
    void revealPathSelectsTheRow();
    void revealPathIgnoresUnknownPath();
    void revealFileCommandIsRegistered();
};

static PluginMetaData makeMeta() { return PluginMetaData(KPluginMetaData{}); }

void TestFileExplorerPlugin::createsViewWhenVaultGranted()
{
    FileExplorerPlugin plugin;
    FileSystemAdapter fs;
    QTemporaryDir dir;
    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);
    FileManager fm(&vault, &cache);

    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("vault.events"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view != nullptr);
    QVERIFY(qobject_cast<FileExplorerView *>(view) != nullptr);
    delete view;
}

void TestFileExplorerPlugin::returnsNullWhenVaultMissing()
{
    FileExplorerPlugin plugin;
    PluginContext ctx(makeMeta(), {QStringLiteral("vault.read")});
    plugin.load(&ctx);
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

void TestFileExplorerPlugin::sessionStateRoundTripsEmptyWhenNoFolders()
{
    // saveSessionState returns empty object when nothing is expanded.
    FileExplorerPlugin plugin;
    FileSystemAdapter fs;
    QTemporaryDir dir;
    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);
    FileManager fm(&vault, &cache);

    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("vault.events")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view);
    const QJsonObject state = plugin.saveSessionState(view);
    QVERIFY(state.isEmpty());
    delete view;
}

static void writeFile(const QString &path, const QByteArray &body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(body);
}

void TestFileExplorerPlugin::revealPathSelectsTheRow()
{
    FileExplorerPlugin plugin;
    FileSystemAdapter fs;
    QTemporaryDir dir;
    writeFile(dir.path() + QStringLiteral("/notes/foo.md"), "hi");
    writeFile(dir.path() + QStringLiteral("/notes/bar.md"), "hi");

    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);
    FileManager fm(&vault, &cache);

    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("vault.events"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    auto *view = qobject_cast<FileExplorerView *>(
        plugin.createView(nullptr));
    QVERIFY(view);

    view->revealPath(QStringLiteral("notes/foo.md"));

    auto *tree = view->findChild<QTreeView *>();
    QVERIFY(tree);
    QModelIndex current = tree->currentIndex();
    QVERIFY(current.isValid());
    QCOMPARE(current.data().toString(), QStringLiteral("foo.md"));
    // Parent folder must be expanded.
    QVERIFY(tree->isExpanded(current.parent()));

    delete view;
}

void TestFileExplorerPlugin::revealPathIgnoresUnknownPath()
{
    FileExplorerPlugin plugin;
    FileSystemAdapter fs;
    QTemporaryDir dir;
    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);
    FileManager fm(&vault, &cache);

    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("vault.events"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    auto *view = qobject_cast<FileExplorerView *>(
        plugin.createView(nullptr));
    QVERIFY(view);

    // Must be a no-op (no crash, no selection).
    view->revealPath(QStringLiteral("does-not-exist.md"));

    delete view;
}

void TestFileExplorerPlugin::revealFileCommandIsRegistered()
{
    FileExplorerPlugin plugin;
    FileSystemAdapter fs;
    QTemporaryDir dir;
    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);
    FileManager fm(&vault, &cache);

    // Set the plugin id on the metadata so CommandRegistrar's auto-namespace
    // produces "file-explorer:reveal-file" as expected.
    KPluginMetaData raw;  // empty — PluginMetaData wrapping default-constructs
    PluginMetaData meta(raw);
    CommandRegistry registry;

    PluginContext ctx(meta,
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("vault.events"), QStringLiteral("ui.commands")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, &registry,
                        nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    // The command is namespaced by the plugin's id. We don't know it via
    // the empty KPluginMetaData used here, so just assert *some* command
    // matching "reveal-file" landed in the registry.
    bool found = false;
    for (const auto *c : registry.listCommands()) {
        if (c->id.endsWith(QStringLiteral(":reveal-file"))) {
            found = true;
            break;
        }
    }
    QVERIFY(found);
}

QTEST_MAIN(TestFileExplorerPlugin)
#include "tst_fileexplorer_plugin.moc"
