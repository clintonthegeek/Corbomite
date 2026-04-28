// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultRead : public QObject
{
    Q_OBJECT
private slots:
    void readReturnsBody();
    void readBinaryReturnsBytes();
    void readRawBypassesTree();
    void readMissingReturnsEmpty();
    void readStripsLeadingUtf8Bom();
    void readBinaryPreservesUtf8Bom();
    void readRawStripsLeadingUtf8Bom();
};

void TestVaultRead::readReturnsBody()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("hello");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    QCOMPARE(vault.read(tf), QByteArray("hello"));
}

void TestVaultRead::readBinaryReturnsBytes()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.bin");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("\x01\x02\x03", 3);
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.bin"));
    QVERIFY(tf);
    QCOMPARE(vault.readBinary(tf), QByteArray("\x01\x02\x03", 3));
}

void TestVaultRead::readRawBypassesTree()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("raw");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QCOMPARE(vault.readRaw(QStringLiteral("a.md")), QByteArray("raw"));
}

void TestVaultRead::readMissingReturnsEmpty()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    QCOMPARE(vault.read(nullptr), QByteArray());
}

void TestVaultRead::readStripsLeadingUtf8Bom()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/bom.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("\xEF\xBB\xBF# Title\n", 11);
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("bom.md"));
    QVERIFY(tf);
    QCOMPARE(vault.read(tf), QByteArray("# Title\n"));
}

void TestVaultRead::readBinaryPreservesUtf8Bom()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/bom.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("\xEF\xBB\xBF# Title\n", 11);
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("bom.md"));
    QVERIFY(tf);
    QCOMPARE(vault.readBinary(tf), QByteArray("\xEF\xBB\xBF# Title\n", 11));
}

void TestVaultRead::readRawStripsLeadingUtf8Bom()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/bom.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("\xEF\xBB\xBFhello", 8);
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QCOMPARE(vault.readRaw(QStringLiteral("bom.md")), QByteArray("hello"));
}

QTEST_MAIN(TestVaultRead)
#include "tst_vault_read.moc"
