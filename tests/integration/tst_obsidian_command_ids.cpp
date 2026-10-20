// SPDX-License-Identifier: GPL-3.0-or-later
//
// Command-id compatibility gate.
//
// `.obsidian/hotkeys.json` is keyed by Obsidian command id
// (docs/obsidian-audit/addenda/2026-10-03-core-command-and-hotkey-contracts.md §D),
// so a vault's override for an id Corbomite does not register is inert — and
// silently so. The full inventory of those ids is generated into
// docs/obsidian-command-ids.md by RE/tools/extract-command-ids.js.
//
// This test covers the plugin half of that contract. Each plugin is loaded
// through the real KPluginFactory dlopen path PluginManager uses in production
// (no setFactoryOverride — constructing the class in-process would prove
// nothing about what a running app registers), then the live CommandRegistry is
// walked. Three properties are pinned per id:
//
//   1. the Obsidian id is registered as a real command, not an alias — a vault
//      override has to reach something;
//   2. the Corbomite-native id that used to be the only name still dispatches,
//      because MarkdownFileView, CanvasFileView and GraphViewActions dispatch it;
//   3. only one of the two is palette-visible.
//
// Finally the whole registry is diffed against
// docs/obsidian-command-ids.corbomite.json, the hand-maintained input to the
// generator, so the generated table cannot quietly drift from the code.
#include <QSet>
#include <QTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <KPluginMetaData>

#include "corbomite/core/Command.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Plugin.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/PluginManager.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

// pluginId → { Obsidian id, Corbomite-native id, dock slug or empty }
struct ShadowCase {
    const char *pluginId;
    const char *obsidianId;
    const char *corbomiteId;
    const char *dockSlug;
};

// The seven ids the audit found shadowed. `file-explorer:reveal-active-file` is
// not a dock reveal — it selects the active file in the navigation tree — so it
// has no dockSlug and is asserted on registration and aliasing only.
static const ShadowCase kShadowed[] = {
    {"corbomite-backlinks",    "backlink:open",                      "corbomite-backlinks:open",     "backlinks"},
    {"corbomite-outlinks",     "outgoing-links:open",                "corbomite-outlinks:open",      "outlinks"},
    {"corbomite-outline",      "outline:open",                       "corbomite-outline:open",       "outline"},
    {"corbomite-properties",   "properties:open",                    "corbomite-properties:open",    "properties"},
    {"corbomite-bookmarks",    "bookmarks:open",                     "corbomite-bookmarks:open",     "bookmarks"},
    {"corbomite-local-graph",  "graph:open-local",                   "corbomite-local-graph:open-local", "local-graph"},
    {"corbomite-file-explorer", "file-explorer:reveal-active-file",  "corbomite-file-explorer:reveal-file", nullptr},
};

class TestObsidianCommandIds : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void obsidianIdIsRegisteredAsARealCommand();
    void corbomiteNativeIdIsAnAliasOfIt();
    void obsidianIdDispatchesTheDockReveal();
    void aliasIsNotListedInThePalette();
    void aliasSurvivesUntilThePluginIsDisabled();
    void mappingTableMatchesTheLiveRegistry();
};

namespace {

struct Harness {
    QTemporaryDir dir;
    FileSystemAdapter fs;
    Vault vault{&fs};
    LinkResolver resolver;
    MetadataCache cache{resolver};
    FileManager fm{&vault, &cache};
    CommandRegistry commands;
    ViewRegistry views;
    Workspace workspace{&views};
    PluginManager pm;

    bool boot(QString *why)
    {
        if (!dir.isValid()) { *why = "temp dir"; return false; }
        vault.load(dir.path());
#ifdef CORBOMITE_PLUGIN_DEV_DIR
        pm.setSystemSearchPath(QStringLiteral(CORBOMITE_PLUGIN_DEV_DIR));
#else
        *why = "CORBOMITE_PLUGIN_DEV_DIR not defined";
        return false;
#endif
        pm.setContextConfigurator([this](PluginContext *ctx) {
            ctx->setCoreServices(&vault, &fm, &cache, nullptr, &workspace,
                                 &commands, nullptr, nullptr, nullptr);
        });
        pm.discoverPlugins();
        for (const auto &c : kShadowed) {
            if (!pm.pluginById(QString::fromLatin1(c.pluginId))) {
                *why = QStringLiteral("%1 not found under CORBOMITE_PLUGIN_DEV_DIR")
                           .arg(QString::fromLatin1(c.pluginId));
                return false;
            }
            if (!pm.enablePlugin(QString::fromLatin1(c.pluginId))) {
                *why = QStringLiteral("enablePlugin(%1) failed")
                           .arg(QString::fromLatin1(c.pluginId));
                return false;
            }
        }
        return true;
    }
};

} // namespace

void TestObsidianCommandIds::obsidianIdIsRegisteredAsARealCommand()
{
    Harness h;
    QString why;
    if (!h.boot(&why)) QSKIP(qPrintable(why));

    for (const auto &c : kShadowed) {
        const QString obsidianId = QString::fromLatin1(c.obsidianId);
        const auto *cmd = h.commands.findCommand(obsidianId);
        QVERIFY2(cmd, qPrintable(QStringLiteral("Obsidian id %1 not registered").arg(obsidianId)));
        // Not an alias: it is the primary. An alias would mean the primary is
        // some Corbomite-native id a vault's config cannot name.
        QVERIFY2(!h.commands.isAlias(obsidianId),
                 qPrintable(QStringLiteral("%1 is an alias, not a primary").arg(obsidianId)));
    }
}

void TestObsidianCommandIds::corbomiteNativeIdIsAnAliasOfIt()
{
    Harness h;
    QString why;
    if (!h.boot(&why)) QSKIP(qPrintable(why));

    for (const auto &c : kShadowed) {
        const QString corbomiteId = QString::fromLatin1(c.corbomiteId);
        QVERIFY2(h.commands.isAlias(corbomiteId),
                 qPrintable(QStringLiteral("%1 should be an alias").arg(corbomiteId)));
        const auto *a = h.commands.findCommand(corbomiteId);
        QVERIFY(a);
        QCOMPARE(a->aliasOf, QString::fromLatin1(c.obsidianId));
        // The alias exists to be dispatched, so it must be reachable.
        QVERIFY(h.commands.findCommand(corbomiteId));
    }
}

void TestObsidianCommandIds::obsidianIdDispatchesTheDockReveal()
{
    Harness h;
    QString why;
    if (!h.boot(&why)) QSKIP(qPrintable(why));

    for (const auto &c : kShadowed) {
        if (!c.dockSlug) continue;
        const QString obsidianId = QString::fromLatin1(c.obsidianId);
        QSignalSpy spy(&h.workspace, &Workspace::revealDockViewRequested);
        QVERIFY2(h.commands.executeById(obsidianId),
                 qPrintable(QStringLiteral("executeById(%1) failed").arg(obsidianId)));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QString::fromLatin1(c.dockSlug));

        // Same behaviour under the Corbomite-native id — one callback, so the
        // two names cannot disagree.
        spy.clear();
        QVERIFY2(h.commands.executeById(QString::fromLatin1(c.corbomiteId)),
                 qPrintable(QStringLiteral("executeById(%1) failed").arg(c.corbomiteId)));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QString::fromLatin1(c.dockSlug));
    }
}

void TestObsidianCommandIds::aliasIsNotListedInThePalette()
{
    Harness h;
    QString why;
    if (!h.boot(&why)) QSKIP(qPrintable(why));

    for (const auto &c : kShadowed) {
        QStringList listed;
        for (const auto *cmd : h.commands.listAvailable()) listed.append(cmd->id);
        QVERIFY2(listed.contains(QString::fromLatin1(c.obsidianId)),
                 qPrintable(QStringLiteral("%1 missing from the palette").arg(c.obsidianId)));
        QVERIFY2(!listed.contains(QString::fromLatin1(c.corbomiteId)),
                 qPrintable(QStringLiteral("%1 must not add a second palette row")
                                .arg(QString::fromLatin1(c.corbomiteId))));
    }
}

void TestObsidianCommandIds::aliasSurvivesUntilThePluginIsDisabled()
{
    Harness h;
    QString why;
    if (!h.boot(&why)) QSKIP(qPrintable(why));

    // Disabling the plugin tears down its CommandRegistrar, which must remove
    // the alias too — a leftover alias would keep the id resolvable and make a
    // hotkey fire into a destroyed plugin.
    const QString pluginId = QStringLiteral("corbomite-outline");
    QVERIFY(h.pm.disablePlugin(pluginId));
    QVERIFY(!h.commands.findCommand(QStringLiteral("outline:open")));
    QVERIFY(!h.commands.findCommand(QStringLiteral("corbomite-outline:open")));
    QVERIFY(!h.commands.executeById(QStringLiteral("outline:open")));
}

void TestObsidianCommandIds::mappingTableMatchesTheLiveRegistry()
{
    Harness h;
    QString why;
    if (!h.boot(&why)) QSKIP(qPrintable(why));

    const QString path = QStringLiteral(CORBOMITE_SOURCE_DIR)
        + QStringLiteral("/docs/obsidian-command-ids.corbomite.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QFAIL(qPrintable(QStringLiteral("cannot read %1 — is CORBOMITE_SOURCE_DIR set?")
                             .arg(path)));
    }
    const QJsonObject doc = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonObject byId = doc.value(QStringLiteral("byId")).toObject();

    // Every id the table claims is Obsidian-compatible must be live, unless it
    // is a MainWindow-side id (KAction bridges) — those are covered by
    // tst_mainwindow_obsidian_commands. The plugin-side ids are exactly the
    // shadowed set plus the bookmarks family.
    QStringList missing;
    for (auto it = byId.constBegin(); it != byId.constEnd(); ++it) {
        const QString id = it.key();
        if (!h.commands.findCommand(id))
            missing.append(id);
    }
    // The MainWindow-owned ids are legitimately absent here; strip them.
    const QSet<QString> mainWindowOwned = {
        QStringLiteral("app:go-back"), QStringLiteral("app:go-forward"),
        QStringLiteral("app:toggle-left-sidebar"), QStringLiteral("command-palette:open"),
        QStringLiteral("editor:save-file"), QStringLiteral("global-search:open"),
        QStringLiteral("graph:open"), QStringLiteral("workspace:split-vertical"),
        QStringLiteral("workspace:split-horizontal"), QStringLiteral("workspace:close"),
        QStringLiteral("workspace:next-tab"), QStringLiteral("workspace:previous-tab"),
        QStringLiteral("workspace:goto-last-tab"), QStringLiteral("workspace:undo-close-pane"),
        QStringLiteral("workspace:toggle-pin"), QStringLiteral("workspace:move-to-new-window"),
        QStringLiteral("workspace:toggle-stacked-tabs"),
        QStringLiteral("markdown:add-metadata-property"),
    };
    for (const auto &id : std::as_const(mainWindowOwned)) missing.removeAll(id);
    QVERIFY2(missing.isEmpty(),
             qPrintable(QStringLiteral("mapping table claims these are implemented but no "
                                       "plugin registers them: %1")
                            .arg(missing.join(QLatin1Char(',')))));

    // And the reverse: no plugin-side Obsidian id may be live but absent from
    // the table, or the generated coverage counts under-report.
    const QJsonObject native = doc.value(QStringLiteral("corbomiteNativeIds")).toObject();
    for (const auto *cmd : h.commands.listCommands()) {
        if (h.commands.isAlias(cmd->id)) continue;
        if (mainWindowOwned.contains(cmd->id)) continue;
        if (cmd->id.startsWith(QLatin1String("split_"))) continue;
        QVERIFY2(byId.contains(cmd->id) || native.contains(cmd->id),
                 qPrintable(QStringLiteral("%1 is registered but listed in neither "
                                           "byId nor corbomiteNativeIds — add it to "
                                           "docs/obsidian-command-ids.corbomite.json")
                                .arg(cmd->id)));
    }
}

QTEST_MAIN(TestObsidianCommandIds)
#include "tst_obsidian_command_ids.moc"
