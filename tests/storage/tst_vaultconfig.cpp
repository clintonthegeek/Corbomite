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
};

QTEST_APPLESS_MAIN(TestVaultConfig)
#include "tst_vaultconfig.moc"
