// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>

#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"

class TestVaultConfigJson : public QObject
{
    Q_OBJECT
private slots:
    void writeReadRoundTrip();
    void writeCreatesConfigDir();
    void deleteRemovesFile();
    void readMissingReturnsNull();
    void writesObsidianStyleTwoSpaceIndent();
};

void TestVaultConfigJson::writeReadRoundTrip()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());

    QJsonObject obj{{"k", 1}};
    QVERIFY(v.writeConfigJson(QStringLiteral("test"), obj));
    const auto loaded = v.readConfigJson(QStringLiteral("test")).toObject();
    QCOMPARE(loaded.value("k").toInt(), 1);
}

void TestVaultConfigJson::writeCreatesConfigDir()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    v.writeConfigJson(QStringLiteral("x"), QJsonObject{});
    QVERIFY(QFileInfo(dir.path() + "/.obsidian/x.json").exists());
}

void TestVaultConfigJson::deleteRemovesFile()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    v.writeConfigJson(QStringLiteral("x"), QJsonObject{});
    QVERIFY(v.deleteConfigJson(QStringLiteral("x")));
    QVERIFY(!QFileInfo(dir.path() + "/.obsidian/x.json").exists());
}

void TestVaultConfigJson::readMissingReturnsNull()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    QVERIFY(v.readConfigJson(QStringLiteral("missing")).isNull());
}

// Regression: Vault::writeConfigJson must emit Obsidian's exact wire format —
// 2-space indent, no trailing newline — to stay byte-compatible with
// .obsidian/*.json files Obsidian writes (and with VaultConfig::writeJson,
// which already emits this shape via serializeObsidianStyle). Bookmarks +
// other plugin paths route through Vault::writeConfigJson and were emitting
// 4-space-indented JSON with a trailing newline.
void TestVaultConfigJson::writesObsidianStyleTwoSpaceIndent()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());

    QJsonObject obj{
        {"a", 1},
        {"nested", QJsonObject{{"b", 2}}}
    };
    QVERIFY(v.writeConfigJson(QStringLiteral("x"), obj));

    QFile f(dir.path() + "/.obsidian/x.json");
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray bytes = f.readAll();

    QVERIFY2(!bytes.endsWith('\n'),
             "Obsidian writes JSON without a trailing newline");
    QVERIFY2(bytes.contains("\n  \""),
             "Top-level keys must be 2-space indented (Obsidian default)");
    // Byte-exact agreement with the canonical helper — Vault::writeConfigJson
    // and VaultConfig::writeJson must produce identical output for the same
    // input so plugins routing through either path don't churn the diff.
    QCOMPARE(bytes, Corbomite::VaultConfig::serializeObsidianStyle(obj));
}

QTEST_MAIN(TestVaultConfigJson)
#include "tst_vault_config_json.moc"
