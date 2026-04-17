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

class TestVaultCreate : public QObject
{
    Q_OBJECT
private slots:
    void createCreatesFile();
    void createCreatesParentFolderIfNeeded();
    void createEmitsCreated();
    void createFolderCreatesDir();
    void createRejectsExisting();
};

void TestVaultCreate::createCreatesFile()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.create(QStringLiteral("new.md"), QByteArray("body"));
    QVERIFY(tf);
    QCOMPARE(tf->path, QStringLiteral("new.md"));
    QFile f(dir.path() + "/new.md");
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("body"));
}

void TestVaultCreate::createCreatesParentFolderIfNeeded()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.create(QStringLiteral("sub/deeper/new.md"),
                             QByteArray("body"));
    QVERIFY(tf);
    QVERIFY(QFileInfo::exists(dir.path() + "/sub/deeper/new.md"));
}

void TestVaultCreate::createEmitsCreated()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::created);
    vault.create(QStringLiteral("x.md"), QByteArray("y"));
    QCOMPARE(spy.count(), 1);
}

void TestVaultCreate::createFolderCreatesDir()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.createFolder(QStringLiteral("sub"));
    QVERIFY(tf);
    QVERIFY(QFileInfo(dir.path() + "/sub").isDir());
}

void TestVaultCreate::createRejectsExisting()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QCOMPARE(vault.create(QStringLiteral("a.md"), QByteArray("y")),
             static_cast<Corbomite::TFile *>(nullptr));
}

QTEST_MAIN(TestVaultCreate)
#include "tst_vault_create.moc"
