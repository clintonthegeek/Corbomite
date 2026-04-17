// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultProcess : public QObject
{
    Q_OBJECT
private slots:
    void processMutatesAtomically();
    void processPassesCurrentContent();
    void processNullFileReturnsFalse();
    void processNullMutatorReturnsFalse();
};

void TestVaultProcess::processMutatesAtomically()
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
    QVERIFY(vault.process(tf, [](const QByteArray &cur) {
        return cur + QByteArray(" -> mutated");
    }));

    QFile g(dir.path() + "/a.md");
    QVERIFY(g.open(QIODevice::ReadOnly));
    QCOMPARE(g.readAll(), QByteArray("one -> mutated"));
}

void TestVaultProcess::processPassesCurrentContent()
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
    QByteArray seen;
    vault.process(tf, [&seen](const QByteArray &cur) {
        seen = cur;
        return cur;
    });
    QCOMPARE(seen, QByteArray("hello"));
}

void TestVaultProcess::processNullFileReturnsFalse()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    QCOMPARE(vault.process(nullptr, [](const QByteArray &b) { return b; }),
             false);
}

void TestVaultProcess::processNullMutatorReturnsFalse()
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
    QCOMPARE(vault.process(tf, {}), false);
}

QTEST_MAIN(TestVaultProcess)
#include "tst_vault_process.moc"
