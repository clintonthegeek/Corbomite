// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"

using namespace Corbomite;

namespace {

void writeRaw(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(bytes);
}

QByteArray readRaw(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

} // namespace

class TestVaultConfig : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void ensureConfigDirCreatesDotObsidian()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());

        QVERIFY(cfg.ensureConfigDir());
        QVERIFY(fs.exists(tmp.path() + QStringLiteral("/.obsidian")));
    }

    void readMissingReturnsNullopt()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(!cfg.readAppJson().has_value());
        QVERIFY(!cfg.readHotkeys().has_value());
    }

    // --- Unknown-key preservation (the load-bearing invariant) ---

    void unknownKeysRoundTripThroughAppJson()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        cfg.ensureConfigDir();

        // Simulate an Obsidian-written app.json with both known + unknown keys
        // (including a nested object from a hypothetical plugin).
        const QByteArray original = R"({
  "alwaysUpdateLinks": true,
  "newLinkFormat": "shortest",
  "_futurePluginKey": {
    "nestedArray": [1, 2, 3],
    "flag": false
  },
  "someNumber": 42,
  "userIgnoreFilters": ["^tmp/", "/^scratch/"]
})";
        writeRaw(cfg.configFilePath(QStringLiteral("app.json")), original);

        auto loaded = cfg.readAppJson();
        QVERIFY(loaded.has_value());

        // Do not mutate; write straight back.
        QVERIFY(cfg.writeAppJson(*loaded));

        auto reloaded = cfg.readAppJson();
        QVERIFY(reloaded.has_value());

        // Every original key survives.
        QVERIFY(reloaded->contains(QStringLiteral("alwaysUpdateLinks")));
        QVERIFY(reloaded->contains(QStringLiteral("newLinkFormat")));
        QVERIFY(reloaded->contains(QStringLiteral("_futurePluginKey")));
        QVERIFY(reloaded->contains(QStringLiteral("someNumber")));
        QVERIFY(reloaded->contains(QStringLiteral("userIgnoreFilters")));

        // Values match.
        QCOMPARE(reloaded->value(QStringLiteral("alwaysUpdateLinks")).toBool(), true);
        QCOMPARE(reloaded->value(QStringLiteral("newLinkFormat")).toString(),
                 QStringLiteral("shortest"));
        QCOMPARE(reloaded->value(QStringLiteral("someNumber")).toInt(), 42);

        // Nested unknown-plugin structure intact.
        const auto nested = reloaded->value(QStringLiteral("_futurePluginKey")).toObject();
        QVERIFY(nested.contains(QStringLiteral("nestedArray")));
        const auto arr = nested.value(QStringLiteral("nestedArray")).toArray();
        QCOMPARE(arr.size(), 3);
        QCOMPARE(arr[0].toInt(), 1);
        QCOMPARE(nested.value(QStringLiteral("flag")).toBool(), false);
    }

    void mutateOneKeyPreservesOthers()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        cfg.ensureConfigDir();

        QJsonObject obj;
        obj.insert(QStringLiteral("known1"), 1);
        obj.insert(QStringLiteral("_pluginA"), QJsonObject{{QStringLiteral("x"), 7}});
        obj.insert(QStringLiteral("known2"), QStringLiteral("hello"));
        QVERIFY(cfg.writeAppJson(obj));

        auto loaded = cfg.readAppJson();
        QVERIFY(loaded.has_value());
        loaded->insert(QStringLiteral("known1"), 999);
        QVERIFY(cfg.writeAppJson(*loaded));

        auto after = cfg.readAppJson();
        QVERIFY(after.has_value());
        QCOMPARE(after->value(QStringLiteral("known1")).toInt(), 999);
        QCOMPARE(after->value(QStringLiteral("known2")).toString(), QStringLiteral("hello"));
        QVERIFY(after->contains(QStringLiteral("_pluginA")));
        QCOMPARE(after->value(QStringLiteral("_pluginA")).toObject()
                    .value(QStringLiteral("x")).toInt(), 7);
    }

    // --- Serialisation format: 2-space indent, no trailing newline ---

    void outputIs2SpaceIndentNoTrailingNewline()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        cfg.ensureConfigDir();

        QJsonObject obj;
        obj.insert(QStringLiteral("a"), 1);
        obj.insert(QStringLiteral("b"), QJsonObject{{QStringLiteral("c"), 2}});
        QVERIFY(cfg.writeAppJson(obj));

        const QByteArray bytes = readRaw(cfg.configFilePath(QStringLiteral("app.json")));
        QVERIFY(!bytes.endsWith('\n'));
        // Nested child indents should be 4 spaces (2 * 2).
        QVERIFY(bytes.contains("\n    \"c\":"));
        // Top-level indent 2 spaces.
        QVERIFY(bytes.contains("\n  \"a\":"));
    }

    // --- community-plugins.json: array-of-string shape ---

    void communityPluginsArrayRoundTrip()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        cfg.ensureConfigDir();

        const QStringList ids{QStringLiteral("dataview"), QStringLiteral("templater")};
        QVERIFY(cfg.writeCommunityPlugins(ids));

        auto loaded = cfg.readCommunityPlugins();
        QVERIFY(loaded.has_value());
        QCOMPARE(*loaded, ids);
    }

    // --- core-plugins.json: legacy array→object migration ---

    void corePluginsModernObjectFormat()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        cfg.ensureConfigDir();

        const QByteArray modern = R"({
  "file-explorer": true,
  "global-search": true,
  "graph": false
})";
        writeRaw(cfg.configFilePath(QStringLiteral("core-plugins.json")), modern);

        const auto cp = cfg.readCorePlugins();
        QVERIFY(cp.has_value());
        QCOMPARE(cp->raw.value(QStringLiteral("file-explorer")).toBool(), true);
        QCOMPARE(cp->raw.value(QStringLiteral("graph")).toBool(), false);
    }

    void corePluginsLegacyArrayMigratesToObjectAndDeletesMigrationFile()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        cfg.ensureConfigDir();

        // Legacy array of enabled IDs.
        writeRaw(cfg.configFilePath(QStringLiteral("core-plugins.json")),
                 R"(["file-explorer", "graph"])");
        // Migration sidecar with disabled entries.
        writeRaw(cfg.configFilePath(QStringLiteral("core-plugins-migration.json")),
                 R"({
  "tag-pane": { "enabled": false },
  "graph":    { "enabled": true }
})");

        const auto cp = cfg.readCorePlugins();
        QVERIFY(cp.has_value());

        // Array entries → true.
        QCOMPARE(cp->raw.value(QStringLiteral("file-explorer")).toBool(), true);
        // Migration merged: tag-pane present, disabled.
        QVERIFY(cp->raw.contains(QStringLiteral("tag-pane")));
        QCOMPARE(cp->raw.value(QStringLiteral("tag-pane")).toBool(), false);
        // Conflict: migration's "graph: enabled=true" agrees with array.
        QCOMPARE(cp->raw.value(QStringLiteral("graph")).toBool(), true);

        // Migration file deleted after successful merge.
        QVERIFY(!fs.exists(cfg.configFilePath(
            QStringLiteral("core-plugins-migration.json"))));

        // On second read, the file is now object-format; re-read works
        // without re-triggering the migration path.
        const auto cp2 = cfg.readCorePlugins();
        QVERIFY(cp2.has_value());
        QCOMPARE(cp2->raw.value(QStringLiteral("file-explorer")).toBool(), true);
    }

    // --- userIgnoreFilters convenience ---

    void userIgnoreFiltersExtracted()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        cfg.ensureConfigDir();

        QJsonObject app;
        app.insert(QStringLiteral("userIgnoreFilters"),
                   QJsonArray{QStringLiteral("^tmp/"), QStringLiteral("/scratch/")});
        cfg.writeAppJson(app);

        const auto filters = cfg.userIgnoreFilters();
        QCOMPARE(filters.size(), 2);
        QVERIFY(filters.contains(QStringLiteral("^tmp/")));
    }

    void userIgnoreFiltersEmptyWhenAppJsonMissing()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.userIgnoreFilters().isEmpty());
    }

    // --- daily-notes.json + templates.json typed accessors ---

    void testReadDailyNotesJsonMissingFileReturnsNullopt()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(!cfg.readDailyNotesJson().has_value());
    }

    void testRoundTripDailyNotesJsonPreservesUnknownKeys()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        cfg.ensureConfigDir();

        QJsonObject obj;
        obj.insert(QStringLiteral("format"), QStringLiteral("YYYY-MM-DD"));
        obj.insert(QStringLiteral("folder"), QStringLiteral("Daily"));
        obj.insert(QStringLiteral("unknown_future_key"),
                   QStringLiteral("should_survive"));
        QVERIFY(cfg.writeDailyNotesJson(obj));

        auto loaded = cfg.readDailyNotesJson();
        QVERIFY(loaded.has_value());
        QVERIFY(loaded->contains(QStringLiteral("format")));
        QVERIFY(loaded->contains(QStringLiteral("folder")));
        QVERIFY(loaded->contains(QStringLiteral("unknown_future_key")));
        QCOMPARE(loaded->value(QStringLiteral("format")).toString(),
                 QStringLiteral("YYYY-MM-DD"));
        QCOMPARE(loaded->value(QStringLiteral("folder")).toString(),
                 QStringLiteral("Daily"));
        QCOMPARE(loaded->value(QStringLiteral("unknown_future_key")).toString(),
                 QStringLiteral("should_survive"));
    }

    void testReadTemplatesJsonMissingFileReturnsNullopt()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(!cfg.readTemplatesJson().has_value());
    }

    void testRoundTripTemplatesJsonPreservesUnknownKeys()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        cfg.ensureConfigDir();

        QJsonObject obj;
        obj.insert(QStringLiteral("folder"), QStringLiteral("templates"));
        obj.insert(QStringLiteral("date_format"), QStringLiteral("YYYY-MM-DD"));
        obj.insert(QStringLiteral("time_format"), QStringLiteral("HH:mm"));
        obj.insert(QStringLiteral("_experimental_flag"), true);
        QVERIFY(cfg.writeTemplatesJson(obj));

        auto loaded = cfg.readTemplatesJson();
        QVERIFY(loaded.has_value());
        QVERIFY(loaded->contains(QStringLiteral("folder")));
        QVERIFY(loaded->contains(QStringLiteral("date_format")));
        QVERIFY(loaded->contains(QStringLiteral("time_format")));
        QVERIFY(loaded->contains(QStringLiteral("_experimental_flag")));
        QCOMPARE(loaded->value(QStringLiteral("folder")).toString(),
                 QStringLiteral("templates"));
        QCOMPARE(loaded->value(QStringLiteral("date_format")).toString(),
                 QStringLiteral("YYYY-MM-DD"));
        QCOMPARE(loaded->value(QStringLiteral("time_format")).toString(),
                 QStringLiteral("HH:mm"));
        QCOMPARE(loaded->value(QStringLiteral("_experimental_flag")).toBool(),
                 true);
    }

    // --- Malformed input handling ---

    void malformedJsonReturnsNullopt()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        cfg.ensureConfigDir();
        writeRaw(cfg.configFilePath(QStringLiteral("app.json")),
                 QByteArray("{not json"));
        QVERIFY(!cfg.readAppJson().has_value());
    }

    // --- mergeJson: round-trip with unknown-key preservation ---

    void testMergeJsonPreservesUnknownKeys()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileSystemAdapter fs;
        VaultConfig vc(&fs, dir.path());
        QVERIFY(vc.ensureConfigDir());

        QJsonObject existing;
        existing.insert(QStringLiteral("theme"), QStringLiteral("light"));
        existing.insert(QStringLiteral("obsidianMystery"), 42);
        QVERIFY(vc.writeJson(QStringLiteral("appearance.json"), existing));

        QJsonObject updates;
        updates.insert(QStringLiteral("theme"), QStringLiteral("dark"));
        QVERIFY(vc.mergeJson(QStringLiteral("appearance.json"), updates));

        const auto result = vc.readJson(QStringLiteral("appearance.json"));
        QVERIFY(result.has_value());
        QCOMPARE(result->value(QStringLiteral("theme")).toString(),
                 QStringLiteral("dark"));
        QCOMPARE(result->value(QStringLiteral("obsidianMystery")).toInt(), 42);
    }

    void testMergeJsonCreatesFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileSystemAdapter fs;
        VaultConfig vc(&fs, dir.path());
        QVERIFY(vc.ensureConfigDir());

        QJsonObject updates;
        updates.insert(QStringLiteral("folder"), QStringLiteral("Daily"));
        QVERIFY(vc.mergeJson(QStringLiteral("daily-notes.json"), updates));

        const auto result = vc.readJson(QStringLiteral("daily-notes.json"));
        QVERIFY(result.has_value());
        QCOMPARE(result->size(), 1);
        QCOMPARE(result->value(QStringLiteral("folder")).toString(),
                 QStringLiteral("Daily"));
    }

    void testMergeJsonOverwritesKnownKeys()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileSystemAdapter fs;
        VaultConfig vc(&fs, dir.path());
        QVERIFY(vc.ensureConfigDir());

        QJsonObject existing;
        existing.insert(QStringLiteral("k1"), 1);
        existing.insert(QStringLiteral("k2"), 2);
        QVERIFY(vc.writeJson(QStringLiteral("templates.json"), existing));

        QJsonObject updates;
        updates.insert(QStringLiteral("k1"), 99);
        QVERIFY(vc.mergeJson(QStringLiteral("templates.json"), updates));

        const auto result = vc.readJson(QStringLiteral("templates.json"));
        QVERIFY(result.has_value());
        QCOMPARE(result->value(QStringLiteral("k1")).toInt(), 99);
        QCOMPARE(result->value(QStringLiteral("k2")).toInt(), 2);
    }
    // Regression: appearance.json `theme` value vocabulary must match
    // Obsidian. Corbomite stores its KConfig theme as
    // "system"/"light"/"dark"; Obsidian writes "obsidian"/"moonstone"/""
    // (empty string = follow system). Without translation, a vault edited
    // by both tools sees the appearance.json `theme` value mean different
    // things in each.
    void obsidianAppearanceTheme_mapsCorbomiteTokens()
    {
        QCOMPARE(VaultConfig::obsidianAppearanceTheme(QStringLiteral("light")),
                 QStringLiteral("moonstone"));
        QCOMPARE(VaultConfig::obsidianAppearanceTheme(QStringLiteral("dark")),
                 QStringLiteral("obsidian"));
        // "system" → empty (Obsidian's "follow OS" sentinel).
        QCOMPARE(VaultConfig::obsidianAppearanceTheme(QStringLiteral("system")),
                 QString());
        // Empty input → empty (follow OS).
        QCOMPARE(VaultConfig::obsidianAppearanceTheme(QString()),
                 QString());
    }

    void obsidianAppearanceTheme_passesThroughCustomCssThemes()
    {
        // Custom CSS-theme names that aren't one of Corbomite's three
        // tokens must pass through verbatim — Obsidian uses arbitrary
        // theme-name strings here when the user picked a community theme.
        QCOMPARE(VaultConfig::obsidianAppearanceTheme(
                     QStringLiteral("Catppuccin")),
                 QStringLiteral("Catppuccin"));
    }
};

QTEST_APPLESS_MAIN(TestVaultConfig)
#include "tst_vaultconfig.moc"
