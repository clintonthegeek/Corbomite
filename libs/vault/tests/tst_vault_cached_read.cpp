// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultCachedRead : public QObject
{
    Q_OBJECT
private slots:
    void firstCallReadsFromDisk();
    void secondCallSkipsAdapter();
    void modifiedEventEvictsCache();
};

void TestVaultCachedRead::firstCallReadsFromDisk()
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
    QCOMPARE(vault.cachedRead(tf), QByteArray("x"));
}

void TestVaultCachedRead::secondCallSkipsAdapter()
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
    vault.cachedRead(tf);
    QFile(dir.path() + "/a.md").remove();
    QCOMPARE(vault.cachedRead(tf), QByteArray("x"));
}

void TestVaultCachedRead::modifiedEventEvictsCache()
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
    vault.cachedRead(tf);

    QTest::qSleep(1100);
    QFile g(dir.path() + "/a.md");
    QVERIFY(g.open(QIODevice::WriteOnly));
    g.write("y");
    g.close();

    QTRY_COMPARE_WITH_TIMEOUT(vault.cachedRead(tf), QByteArray("y"), 5000);
}

QTEST_MAIN(TestVaultCachedRead)
#include "tst_vault_cached_read.moc"
