// SPDX-License-Identifier: GPL-3.0-or-later
//
// `.obsidian/app.json` "Files & Links" settings are actually consumed.
//
// Obsidian stores these per-vault (obsidian-audit/domains/settings.md §
// ".obsidian/app.json": `newFileLocation`, `newFileFolderPath`,
// `attachmentFolderPath`, `useMarkdownLinks`, `newLinkFormat`,
// `alwaysUpdateLinks`, `trashOption`, `promptDelete`). Corbomite read none of
// them — `trashOption`/`promptDelete` came from the app's KConfig only, and
// `useMarkdownLinks`/`newLinkFormat` were an explicit TODO in
// `FileManager::generateMarkdownLink` ("config honouring is a follow-up").
// So a vault authored in Obsidian silently lost all of them.
//
// The precedence rule under test, and the reason the fallback parameter exists
// at all: **vault app.json wins; the Corbomite app-level setting is the
// fallback; an absent OR UNRECOGNISED value falls back rather than guessing.**
// The unrecognised case matters most for `trashOption` — silently escalating to
// a permanent delete because a vault used a future vocabulary would destroy
// data the user could otherwise recover.

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using Corbomite::FileSystemAdapter;
using Corbomite::VaultConfig;

class TestAppJsonSettings : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void readsTrashMode_data();
    void readsTrashMode();
    void unknownTrashMode_fallsBack_data();
    void unknownTrashMode_fallsBack();
    void absentTrashOption_fallsBack();
    void readsPromptDelete();
    void readsAlwaysUpdateLinks();
    void readsUseMarkdownLinks();
    void readsNewLinkFormat();

    void nonBooleanValue_doesNotOverrideFallback();
    void nonStringValue_doesNotOverrideFallback();
    void missingAppJson_fallsBack();

private:
    void writeAppJson(const QByteArray &json);
    /// A VaultConfig bound to the temp vault, built on demand because
    /// VaultConfig has no default constructor (it needs adapter + root).
    VaultConfig config();

    QTemporaryDir m_dir;
    FileSystemAdapter m_fs;
};

void TestAppJsonSettings::init()
{
    QVERIFY(m_dir.isValid());
    QDir(m_dir.path()).mkpath(QStringLiteral(".obsidian"));
}

void TestAppJsonSettings::cleanup()
{
    QFile::remove(m_dir.path() + QStringLiteral("/.obsidian/app.json"));
}

VaultConfig TestAppJsonSettings::config() { return VaultConfig(&m_fs, m_dir.path()); }

void TestAppJsonSettings::writeAppJson(const QByteArray &json)
{
    QFile f(m_dir.path() + QStringLiteral("/.obsidian/app.json"));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(f.write(json), static_cast<qint64>(json.size()));
}

void TestAppJsonSettings::readsTrashMode_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<int>("expected");   // 0 system, 1 vault, 2 permanent

    QTest::newRow("system")    << QByteArray(R"({"trashOption":"system"})")   << 0;
    QTest::newRow("vault")     << QByteArray(R"({"trashOption":"vault"})")    << 1;
    QTest::newRow("permanent") << QByteArray(R"({"trashOption":"permanent"})") << 2;
}

void TestAppJsonSettings::readsTrashMode()
{
    QFETCH(QByteArray, json);
    QFETCH(int, expected);

    writeAppJson(json);
    const VaultConfig cfg = config();

    const VaultConfig::TrashMode fallback = VaultConfig::TrashMode::SystemTrash;
    QCOMPARE(int(cfg.appTrashMode(fallback)), expected);
}

void TestAppJsonSettings::unknownTrashMode_fallsBack_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<int>("fallback");

    QTest::newRow("future-vocabulary")
        << QByteArray(R"({"trashOption":"someFutureMode"})")
        << int(VaultConfig::TrashMode::Permanent);
    QTest::newRow("empty-string")
        << QByteArray(R"({"trashOption":""})")
        << int(VaultConfig::TrashMode::Permanent);
    QTest::newRow("wrong-type")
        << QByteArray(R"({"trashOption":true})")
        << int(VaultConfig::TrashMode::Permanent);
}

void TestAppJsonSettings::unknownTrashMode_fallsBack()
{
    QFETCH(QByteArray, json);
    QFETCH(int, fallback);

    writeAppJson(json);
    const VaultConfig cfg = config();

    QCOMPARE(int(cfg.appTrashMode(VaultConfig::TrashMode(fallback))),
             fallback);
}

void TestAppJsonSettings::absentTrashOption_fallsBack()
{
    writeAppJson(R"({"promptDelete":false})");
    const VaultConfig cfg = config();

    QCOMPARE(int(cfg.appTrashMode(VaultConfig::TrashMode::VaultTrash)),
             int(VaultConfig::TrashMode::VaultTrash));
}

void TestAppJsonSettings::readsPromptDelete()
{
    writeAppJson(R"({"promptDelete":false})");
    const VaultConfig cfg = config();

    QVERIFY2(!cfg.appPromptDelete(/*fallback=*/true),
             "vault app.json's promptDelete:false must win over the fallback");
    QCOMPARE(cfg.appPromptDelete(/*fallback=*/false), false);
}

void TestAppJsonSettings::readsAlwaysUpdateLinks()
{
    writeAppJson(R"({"alwaysUpdateLinks":false})");
    const VaultConfig cfg = config();

    QVERIFY2(!cfg.appAlwaysUpdateLinks(/*fallback=*/true),
             "vault app.json's alwaysUpdateLinks:false must be honoured — it "
             "gates link rewriting on rename");
}

void TestAppJsonSettings::readsUseMarkdownLinks()
{
    writeAppJson(R"({"useMarkdownLinks":true,"newLinkFormat":"absolute"})");
    const VaultConfig cfg = config();

    QVERIFY(cfg.appUseMarkdownLinks(/*fallback=*/false));
    QCOMPARE(cfg.appNewLinkFormat(), QStringLiteral("absolute"));
}

void TestAppJsonSettings::readsNewLinkFormat()
{
    writeAppJson(R"({"newLinkFormat":"relative"})");
    const VaultConfig cfg = config();

    QCOMPARE(cfg.appNewLinkFormat(), QStringLiteral("relative"));
}

// A hand-edited vault can put a number where a boolean belongs. That must not
// be silently coerced — it means "unspecified", so the fallback wins.
void TestAppJsonSettings::nonBooleanValue_doesNotOverrideFallback()
{
    writeAppJson(R"({"promptDelete":1,"alwaysUpdateLinks":"no"})");
    const VaultConfig cfg = config();

    QCOMPARE(cfg.appPromptDelete(true), true);
    QCOMPARE(cfg.appAlwaysUpdateLinks(true), true);
}

void TestAppJsonSettings::nonStringValue_doesNotOverrideFallback()
{
    writeAppJson(R"({"newLinkFormat":7})");
    const VaultConfig cfg = config();

    QVERIFY(cfg.appNewLinkFormat().isEmpty());
}

// No app.json at all — a plain folder of notes. Everything falls back.
void TestAppJsonSettings::missingAppJson_fallsBack()
{
    const VaultConfig cfg = config();

    QCOMPARE(int(cfg.appTrashMode(VaultConfig::TrashMode::VaultTrash)),
             int(VaultConfig::TrashMode::VaultTrash));
    QCOMPARE(cfg.appPromptDelete(false), false);
    QCOMPARE(cfg.appAlwaysUpdateLinks(false), false);
    QCOMPARE(cfg.appUseMarkdownLinks(false), false);
    QVERIFY(cfg.appNewLinkFormat().isEmpty());
}

QTEST_MAIN(TestAppJsonSettings)
#include "tst_appjson_settings.moc"