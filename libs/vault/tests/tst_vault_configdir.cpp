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
    void setConfigDirRejectsIllegalChars_data();
    void setConfigDirRejectsIllegalChars();
    void invalidSetRevertsRatherThanKeepingPrevious();
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

// validateConfigDir tests the whole name against the platform's illegal-char
// set (`app/own/1129-UD.js:105-108` calling `HD`), so a custom config dir
// cannot smuggle in a separator or a colon.
void TestVaultConfigDir::setConfigDirRejectsIllegalChars_data()
{
    QTest::addColumn<QString>("candidate");

    QTest::newRow("colon")     << QStringLiteral(".ob:s");
    QTest::newRow("backslash") << QStringLiteral(".ob\\s");
}

void TestVaultConfigDir::setConfigDirRejectsIllegalChars()
{
    QFETCH(QString, candidate);
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.setConfigDir(candidate);
    QCOMPARE(v.configDir(), QStringLiteral(".obsidian"));
}

// Obsidian's setConfigDir assigns unconditionally after the validity test, so
// an invalid value resets to `.obsidian` instead of leaving the old one in
// place.
void TestVaultConfigDir::invalidSetRevertsRatherThanKeepingPrevious()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.setConfigDir(QStringLiteral(".custom"));
    QCOMPARE(v.configDir(), QStringLiteral(".custom"));
    v.setConfigDir(QStringLiteral("nope"));
    QCOMPARE(v.configDir(), QStringLiteral(".obsidian"));
}

QTEST_MAIN(TestVaultConfigDir)
#include "tst_vault_configdir.moc"
