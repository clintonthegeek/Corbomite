// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultRenameRemove : public QObject
{
    Q_OBJECT
private slots:
    void renameUpdatesPathAndFireSignal();
    void removeDeletesFileAndFireSignal();
    void removeTombstonesHandle();
    void copyDuplicatesFile();
};

void TestVaultRenameRemove::renameUpdatesPathAndFireSignal()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    QSignalSpy spy(&vault, &Corbomite::Vault::renamed);

    QVERIFY(vault.rename(tf, QStringLiteral("b.md")));
    QCOMPARE(tf->path, QStringLiteral("b.md"));
    QCOMPARE(spy.count(), 1);
    QVERIFY(vault.getFileByPath(QStringLiteral("b.md")));
    QVERIFY(!vault.getFileByPath(QStringLiteral("a.md")));
}

void TestVaultRenameRemove::removeDeletesFileAndFireSignal()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    QSignalSpy spy(&vault, &Corbomite::Vault::deletedFile);

    QVERIFY(vault.remove(tf));
    QCOMPARE(spy.count(), 1);
    QVERIFY(!QFileInfo::exists(dir.path() + "/a.md"));
}

void TestVaultRenameRemove::removeTombstonesHandle()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    vault.remove(tf);
    QCOMPARE(tf->deleted, true);
}

void TestVaultRenameRemove::copyDuplicatesFile()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    QVERIFY(vault.copy(tf, QStringLiteral("b.md")));
    QVERIFY(QFileInfo::exists(dir.path() + "/b.md"));
    QVERIFY(QFileInfo::exists(dir.path() + "/a.md"));
}

QTEST_MAIN(TestVaultRenameRemove)
#include "tst_vault_rename_remove.moc"
