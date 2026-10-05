// SPDX-License-Identifier: GPL-3.0-or-later
//
// VaultConfig multi-step write sequences must not leave partial state.
//
// Both cases here are ports of Obsidian shapes that fail the same way:
//   * `Vault::saveConfig` (app/own/1129-UD.js:132-155) awaits writeConfigJson
//     ("app") and then writeConfigJson ("appearance") and lets a rejection on
//     the second stand. app.json then holds the new values and appearance.json
//     the old ones — a config split across two files, with configTs never
//     advanced and no record anywhere that it is split.
//   * the legacy core-plugins migration retires `core-plugins-migration.json`
//     after writing the modern file. That file is the only remaining record of
//     the intended state: the legacy array it merges into is presence-encoded
//     (every id true), so dropping the migration file after a failed write
//     silently re-enables every core plugin it carried.

#include <QTest>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>

#include "corbomite/core/DataAdapter.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"

namespace {

/// FileSystemAdapter that refuses writes to any path whose file name matches
/// `failOn`, and can be armed once that refusal has been consumed.
class FlakyAdapter : public Corbomite::FileSystemAdapter
{
public:
    QString failOn;
    bool armed = false;
    mutable int writeAttempts = 0;

    bool writeBinary(const QString &absolutePath,
                     const QByteArray &content,
                     const Corbomite::WriteHints &hints = {}) override
    {
        ++writeAttempts;
        if (armed && absolutePath.endsWith(failOn)) return false;
        return Corbomite::FileSystemAdapter::writeBinary(absolutePath, content,
                                                         hints);
    }
};

QByteArray readFile(const QTemporaryDir &dir, const char *name)
{
    QFile f(dir.path() + "/.obsidian/" + name);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

bool fileExists(const QTemporaryDir &dir, const char *name)
{
    return QFileInfo::exists(dir.path() + "/.obsidian/" + name);
}

} // namespace

class TstVaultConfigAtomicity : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void saveConfig_secondWriteFailure_restoresFirstFile();
    void saveConfig_secondWriteFailure_removesFirstFileItCreated();
    void saveConfig_bothWritesSucceed_persistsBoth();
    void readCorePlugins_failedMigrationWrite_keepsMigrationFile();
};

void TstVaultConfigAtomicity::saveConfig_secondWriteFailure_restoresFirstFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FlakyAdapter fs;
    Corbomite::VaultConfig cfg(&fs, dir.path());

    QJsonObject seed;
    seed.insert(QStringLiteral("alwaysUpdateLinks"), true);
    seed.insert(QStringLiteral("theme"), QStringLiteral("obsidian"));
    QVERIFY(cfg.writeJson(QStringLiteral("app.json"), seed));
    const QByteArray appBefore = readFile(dir, "app.json");
    QVERIFY(!appBefore.isEmpty());

    QJsonObject merged;
    merged.insert(QStringLiteral("alwaysUpdateLinks"), false);
    merged.insert(QStringLiteral("theme"), QStringLiteral("moonstone"));
    merged.insert(QStringLiteral("promptDelete"), false);

    fs.failOn = QStringLiteral("appearance.json");
    fs.armed = true;
    QVERIFY(!cfg.saveConfig(merged));

    // app.json must be exactly as it was: the caller is told the save failed,
    // and a later successful save must not find a half-applied config.
    QCOMPARE(readFile(dir, "app.json"), appBefore);

    // And with the obstruction gone the same save lands in both files.
    fs.armed = false;
    QVERIFY(cfg.saveConfig(merged));
    const QJsonObject app = cfg.readAppJson().value_or(QJsonObject{});
    const QJsonObject appearance = cfg.readAppearanceJson().value_or(QJsonObject{});
    QCOMPARE(app.value(QStringLiteral("alwaysUpdateLinks")).toBool(), false);
    QCOMPARE(app.value(QStringLiteral("promptDelete")).toBool(), false);
    QCOMPARE(appearance.value(QStringLiteral("theme")).toString(),
             QStringLiteral("moonstone"));
}

void TstVaultConfigAtomicity::saveConfig_secondWriteFailure_removesFirstFileItCreated()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FlakyAdapter fs;
    Corbomite::VaultConfig cfg(&fs, dir.path());
    QVERIFY(cfg.ensureConfigDir());
    QVERIFY(!fileExists(dir, "app.json"));

    QJsonObject merged;
    merged.insert(QStringLiteral("promptDelete"), false);

    fs.failOn = QStringLiteral("appearance.json");
    fs.armed = true;
    QVERIFY(!cfg.saveConfig(merged));

    // app.json did not exist before this call, so the rollback has to remove
    // it rather than write empty bytes — a zero-length app.json would read back
    // as a present-but-empty config and mask the failure.
    QVERIFY2(!fileExists(dir, "app.json"),
             "a failed saveConfig left the app.json it had just created behind");
}

void TstVaultConfigAtomicity::saveConfig_bothWritesSucceed_persistsBoth()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FlakyAdapter fs;
    Corbomite::VaultConfig cfg(&fs, dir.path());

    QJsonObject merged;
    merged.insert(QStringLiteral("useMarkdownLinks"), true);
    merged.insert(QStringLiteral("baseFontSize"), 17);
    QVERIFY(cfg.saveConfig(merged));

    const QJsonObject app = cfg.readAppJson().value_or(QJsonObject{});
    const QJsonObject appearance = cfg.readAppearanceJson().value_or(QJsonObject{});
    QCOMPARE(app.value(QStringLiteral("useMarkdownLinks")).toBool(), true);
    QCOMPARE(appearance.value(QStringLiteral("baseFontSize")).toInt(), 17);
}

void TstVaultConfigAtomicity::readCorePlugins_failedMigrationWrite_keepsMigrationFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FlakyAdapter fs;
    Corbomite::VaultConfig cfg(&fs, dir.path());
    QVERIFY(cfg.ensureConfigDir());

    // Legacy array (presence-encoded → everything true) plus a migration file
    // that turns one of them off.
    QJsonArray legacy;
    legacy.append(QStringLiteral("file-explorer"));
    legacy.append(QStringLiteral("global-search"));
    QVERIFY(cfg.writeCommunityPlugins({}));          // creates .obsidian/
    {
        QFile f(dir.path() + "/.obsidian/core-plugins.json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(legacy).toJson(QJsonDocument::Compact));
    }
    {
        QJsonObject mig;
        QJsonObject globalSearch;
        globalSearch.insert(QStringLiteral("enabled"), false);
        mig.insert(QStringLiteral("global-search"), globalSearch);
        QFile f(dir.path() + "/.obsidian/core-plugins-migration.json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(mig).toJson(QJsonDocument::Compact));
    }

    fs.failOn = QStringLiteral("core-plugins.json");
    fs.armed = true;
    const auto cp = cfg.readCorePlugins();
    QVERIFY(cp.has_value());
    QCOMPARE(cp->raw.value(QStringLiteral("global-search")).toBool(), false);

    QVERIFY2(fileExists(dir, "core-plugins-migration.json"),
             "the migration file is the only record of the disabled state; "
             "deleting it after a failed write silently re-enables the plugin");

    // Once the write can land, the migration retires and the modern file wins.
    fs.armed = false;
    const auto again = cfg.readCorePlugins();
    QVERIFY(again.has_value());
    QCOMPARE(again->raw.value(QStringLiteral("global-search")).toBool(), false);
    QVERIFY(!fileExists(dir, "core-plugins-migration.json"));
}

QTEST_MAIN(TstVaultConfigAtomicity)
#include "tst_vaultconfig_atomicity.moc"