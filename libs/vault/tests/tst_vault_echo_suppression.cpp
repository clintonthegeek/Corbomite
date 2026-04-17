// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultEchoSuppression : public QObject
{
    Q_OBJECT
private slots:
    void selfWriteDoesNotDoubleEmit();
    void externalWriteAfterSelfWriteEmits();
};

void TestVaultEchoSuppression::selfWriteDoesNotDoubleEmit()
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

    vault.modify(tf, QByteArray("two"));
    QTest::qWait(300);
    // Exactly one emission — the direct modify() call — NOT two (which
    // would indicate the watcher's echo made it through).
    QCOMPARE(spy.count(), 1);
}

void TestVaultEchoSuppression::externalWriteAfterSelfWriteEmits()
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
    vault.modify(tf, QByteArray("two"));

    QTest::qWait(1200);  // past the 1s self-write ledger window
    QSignalSpy spy(&vault, &Corbomite::Vault::modified);

    QFile g(dir.path() + "/a.md");
    QVERIFY(g.open(QIODevice::WriteOnly));
    g.write("three");
    g.close();

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
}

QTEST_MAIN(TestVaultEchoSuppression)
#include "tst_vault_echo_suppression.moc"
