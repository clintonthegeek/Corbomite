// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultModify : public QObject
{
    Q_OBJECT
private slots:
    void modifyWritesAndEmits();
    void modifyInvalidatesCache();
    void appendAppendsBody();
    void modifyOnNullReturnsFalse();
};

void TestVaultModify::modifyWritesAndEmits()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("one");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    QSignalSpy spy(&vault, &Corbomite::Vault::modified);

    QVERIFY(vault.modify(tf, QByteArray("two")));
    QFile g(dir.path() + "/a.md");
    QVERIFY(g.open(QIODevice::ReadOnly));
    QCOMPARE(g.readAll(), QByteArray("two"));

    QCOMPARE(spy.count(), 1);
}

void TestVaultModify::modifyInvalidatesCache()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("one");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    QCOMPARE(vault.cachedRead(tf), QByteArray("one"));

    QVERIFY(vault.modify(tf, QByteArray("two")));
    QCOMPARE(vault.cachedRead(tf), QByteArray("two"));
}

void TestVaultModify::appendAppendsBody()
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
    QVERIFY(vault.append(tf, QByteArray("y")));
    QFile g(dir.path() + "/a.md");
    QVERIFY(g.open(QIODevice::ReadOnly));
    QCOMPARE(g.readAll(), QByteArray("xy"));
}

void TestVaultModify::modifyOnNullReturnsFalse()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    QCOMPARE(vault.modify(nullptr, QByteArray("x")), false);
}

QTEST_MAIN(TestVaultModify)
#include "tst_vault_modify.moc"
