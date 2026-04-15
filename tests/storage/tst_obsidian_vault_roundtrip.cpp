// SPDX-License-Identifier: GPL-3.0-or-later
// End-to-end round-trip test for Cluster B Phase 6 DoD.
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"
#include "corbomite/storage/WorkspaceState.h"

using namespace Corbomite;

namespace {

void writeText(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(bytes);
}

void writeJson(const QString &path, const QJsonObject &obj)
{
    writeText(path, QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void writeJsonArray(const QString &path, const QJsonArray &arr)
{
    writeText(path, QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

} // namespace

class TestObsidianVaultRoundTrip : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void fullObsidianVaultRoundTrip()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        // --- 1. Seed the simulated .obsidian/ directory ---
        // (Built programmatically to dodge moc's raw-string parsing quirks.)

        QJsonObject app;
        app.insert(QStringLiteral("alwaysUpdateLinks"), true);
        app.insert(QStringLiteral("newLinkFormat"), QStringLiteral("shortest"));
        app.insert(QStringLiteral("userIgnoreFilters"),
                   QJsonArray{QStringLiteral("^tmp/")});
        app.insert(QStringLiteral("livePreview"), false);
        QJsonObject futureKey;
        QJsonObject nested;
        nested.insert(QStringLiteral("flag"), true);
        nested.insert(QStringLiteral("list"), QJsonArray{1, 2});
        futureKey.insert(QStringLiteral("nested"), nested);
        app.insert(QStringLiteral("_futureObsidianKey"), futureKey);
        app.insert(QStringLiteral("_dataviewPluginKey"),
                   QStringLiteral("persisted-value"));
        writeJson(cfg.configFilePath(QStringLiteral("app.json")), app);

        QJsonObject appearance;
        appearance.insert(QStringLiteral("theme"), QStringLiteral("obsidian"));
        appearance.insert(QStringLiteral("baseFontSize"), 16);
        QJsonObject pluginTheme;
        pluginTheme.insert(QStringLiteral("accent"), QStringLiteral("#ff00aa"));
        appearance.insert(QStringLiteral("_pluginCustomTheme"), pluginTheme);
        writeJson(cfg.configFilePath(QStringLiteral("appearance.json")), appearance);

        QJsonObject corePlugins;
        corePlugins.insert(QStringLiteral("file-explorer"), true);
        corePlugins.insert(QStringLiteral("global-search"), true);
        corePlugins.insert(QStringLiteral("graph"), false);
        corePlugins.insert(QStringLiteral("backlink"), true);
        writeJson(cfg.configFilePath(QStringLiteral("core-plugins.json")), corePlugins);

        writeJsonArray(cfg.configFilePath(QStringLiteral("community-plugins.json")),
                       QJsonArray{QStringLiteral("dataview"),
                                  QStringLiteral("templater"),
                                  QStringLiteral("breadcrumbs")});

        QJsonObject hotkeys;
        QJsonObject hotkeyBinding;
        hotkeyBinding.insert(QStringLiteral("modifiers"),
                             QJsonArray{QStringLiteral("Mod")});
        hotkeyBinding.insert(QStringLiteral("key"), QStringLiteral("p"));
        hotkeys.insert(QStringLiteral("workspace:split-vertical"),
                       QJsonArray{hotkeyBinding});
        hotkeys.insert(QStringLiteral("_futureHotkey"), QStringLiteral("unknown"));
        writeJson(cfg.configFilePath(QStringLiteral("hotkeys.json")), hotkeys);

        // workspace.json — full 5-variant tree.
        QJsonObject leaf;
        leaf.insert(QStringLiteral("id"), QStringLiteral("leaf-note"));
        leaf.insert(QStringLiteral("type"), QStringLiteral("leaf"));
        QJsonObject leafState;
        leafState.insert(QStringLiteral("type"), QStringLiteral("markdown"));
        QJsonObject leafStateInner;
        leafStateInner.insert(QStringLiteral("file"), QStringLiteral("note.md"));
        leafStateInner.insert(QStringLiteral("mode"), QStringLiteral("source"));
        leafState.insert(QStringLiteral("state"), leafStateInner);
        leafState.insert(QStringLiteral("_leafStateUnknown"), true);
        leaf.insert(QStringLiteral("state"), leafState);

        QJsonObject tabsNode;
        tabsNode.insert(QStringLiteral("id"), QStringLiteral("tabs-1"));
        tabsNode.insert(QStringLiteral("type"), QStringLiteral("tabs"));
        tabsNode.insert(QStringLiteral("children"), QJsonArray{leaf});
        tabsNode.insert(QStringLiteral("currentTab"), 0);

        QJsonObject mainNode;
        mainNode.insert(QStringLiteral("id"), QStringLiteral("root-main"));
        mainNode.insert(QStringLiteral("type"), QStringLiteral("split"));
        mainNode.insert(QStringLiteral("direction"), QStringLiteral("vertical"));
        mainNode.insert(QStringLiteral("_mainUnknownKey"), 42);
        mainNode.insert(QStringLiteral("children"), QJsonArray{tabsNode});

        QJsonObject workspace;
        workspace.insert(QStringLiteral("main"), mainNode);
        workspace.insert(QStringLiteral("active"), QStringLiteral("leaf-note"));
        workspace.insert(QStringLiteral("lastOpenFiles"),
                         QJsonArray{QStringLiteral("note.md"),
                                    QStringLiteral("other.md")});
        workspace.insert(QStringLiteral("_futureRootKey"), QStringLiteral("preserved"));
        writeJson(cfg.configFilePath(QStringLiteral("workspace.json")), workspace);

        // Plugin data.json.
        QJsonObject dataviewData;
        dataviewData.insert(QStringLiteral("enableJavaScriptQueries"), true);
        dataviewData.insert(QStringLiteral("refreshInterval"), 2500);
        dataviewData.insert(QStringLiteral("_newerPluginVersionKey"),
                            QJsonObject{{QStringLiteral("a"), 1}});
        writeJson(cfg.configDir() + QStringLiteral("/plugins/dataview/data.json"),
                  dataviewData);

        // --- 2. Read everything back via the libs ---

        auto app2 = cfg.readAppJson();
        QVERIFY(app2.has_value());
        auto appearance2 = cfg.readAppearanceJson();
        QVERIFY(appearance2.has_value());
        auto corePlugins2 = cfg.readCorePlugins();
        QVERIFY(corePlugins2.has_value());
        auto commPlugins2 = cfg.readCommunityPlugins();
        QVERIFY(commPlugins2.has_value());
        auto hotkeys2 = cfg.readHotkeys();
        QVERIFY(hotkeys2.has_value());
        auto ws2 = WorkspaceState::load(
            &fs, cfg.configFilePath(QStringLiteral("workspace.json")));
        QVERIFY(ws2.has_value());

        // --- 3. Known + unknown keys both loaded at every depth ---

        QCOMPARE(app2->value(QStringLiteral("newLinkFormat")).toString(),
                 QStringLiteral("shortest"));
        QCOMPARE(cfg.userIgnoreFilters().size(), 1);
        const auto futureRoot = app2->value(
            QStringLiteral("_futureObsidianKey")).toObject();
        const auto nestedLoaded = futureRoot.value(
            QStringLiteral("nested")).toObject();
        const auto listLoaded = nestedLoaded.value(
            QStringLiteral("list")).toArray();
        QCOMPARE(listLoaded.size(), 2);

        QCOMPARE(ws2->main().value(
            QStringLiteral("_mainUnknownKey")).toInt(), 42);
        const auto loadedTabs = WorkspaceState::children(ws2->main()).at(0).toObject();
        const auto loadedLeaf = WorkspaceState::children(loadedTabs).at(0).toObject();
        const auto loadedLeafState = loadedLeaf.value(
            QStringLiteral("state")).toObject();
        QCOMPARE(loadedLeafState.value(
            QStringLiteral("_leafStateUnknown")).toBool(), true);

        // --- 4. Save everything back unchanged → reload → verify survival ---

        QVERIFY(cfg.writeAppJson(*app2));
        QVERIFY(cfg.writeAppearanceJson(*appearance2));
        QVERIFY(cfg.writeCorePlugins(*corePlugins2));
        QVERIFY(cfg.writeCommunityPlugins(*commPlugins2));
        QVERIFY(cfg.writeHotkeys(*hotkeys2));
        QVERIFY(ws2->save(&fs,
            cfg.configFilePath(QStringLiteral("workspace.json"))));

        auto app3 = cfg.readAppJson();
        auto appearance3 = cfg.readAppearanceJson();
        auto corePlugins3 = cfg.readCorePlugins();
        auto commPlugins3 = cfg.readCommunityPlugins();
        auto hotkeys3 = cfg.readHotkeys();
        auto ws3 = WorkspaceState::load(
            &fs, cfg.configFilePath(QStringLiteral("workspace.json")));

        // Every key at every depth still present.
        QVERIFY(app3->contains(QStringLiteral("alwaysUpdateLinks")));
        QVERIFY(app3->contains(QStringLiteral("_futureObsidianKey")));
        QVERIFY(app3->contains(QStringLiteral("_dataviewPluginKey")));
        QVERIFY(app3->value(QStringLiteral("_futureObsidianKey")).toObject()
                    .value(QStringLiteral("nested")).toObject()
                    .value(QStringLiteral("list")).isArray());

        QVERIFY(appearance3->contains(QStringLiteral("theme")));
        QVERIFY(appearance3->contains(QStringLiteral("_pluginCustomTheme")));

        QCOMPARE(corePlugins3->raw.value(QStringLiteral("graph")).toBool(), false);

        QCOMPARE(*commPlugins3,
                 (QStringList{QStringLiteral("dataview"),
                              QStringLiteral("templater"),
                              QStringLiteral("breadcrumbs")}));

        QVERIFY(hotkeys3->contains(QStringLiteral("_futureHotkey")));

        QCOMPARE(ws3->main().value(QStringLiteral("_mainUnknownKey")).toInt(), 42);
        QCOMPARE(ws3->raw().value(QStringLiteral("_futureRootKey")).toString(),
                 QStringLiteral("preserved"));
        QCOMPARE(ws3->lastOpenFiles().size(), 2);

        // --- 5. No .case-probe-* detritus in .obsidian/ ---
        const auto entries = fs.list(cfg.configDir());
        for (const auto &e : entries) {
            QVERIFY2(!e.startsWith(QStringLiteral(".case-probe-")),
                     qPrintable(e));
        }
    }
};

QTEST_APPLESS_MAIN(TestObsidianVaultRoundTrip)
#include "tst_obsidian_vault_roundtrip.moc"
