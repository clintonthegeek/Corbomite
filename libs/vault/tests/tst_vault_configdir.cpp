// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultConfigDir : public QObject
{
    Q_OBJECT
private slots:
    void defaultsToDotObsidian();
    void setConfigDirAcceptsValid();
    void setConfigDirRejectsInvalid();
};

void TestVaultConfigDir::defaultsToDotObsidian()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    QCOMPARE(v.configDir(), QStringLiteral(".obsidian"));
}

void TestVaultConfigDir::setConfigDirAcceptsValid()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.setConfigDir(QStringLiteral(".custom"));
    QCOMPARE(v.configDir(), QStringLiteral(".custom"));
}

void TestVaultConfigDir::setConfigDirRejectsInvalid()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.setConfigDir(QStringLiteral("notleadingdot"));
    QCOMPARE(v.configDir(), QStringLiteral(".obsidian"));
    v.setConfigDir(QStringLiteral("."));
    QCOMPARE(v.configDir(), QStringLiteral(".obsidian"));
}

QTEST_MAIN(TestVaultConfigDir)
#include "tst_vault_configdir.moc"
