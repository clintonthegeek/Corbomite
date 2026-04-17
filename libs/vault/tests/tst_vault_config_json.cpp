// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>

#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultConfigJson : public QObject
{
    Q_OBJECT
private slots:
    void writeReadRoundTrip();
    void writeCreatesConfigDir();
    void deleteRemovesFile();
    void readMissingReturnsNull();
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

QTEST_MAIN(TestVaultConfigJson)
#include "tst_vault_config_json.moc"
