// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

// Phase 3 Task 3.3 will turn on the real tests once Vault::modify lands.
// The echo-suppression ledger (stampSelfWrite / consumeSelfWrite) is private
// API and not test-accessible until a public mutation entry point exists.

class TestVaultEchoSuppression : public QObject
{
    Q_OBJECT
private slots:
    void selfWriteDoesNotDoubleEmit();
    void externalWriteAfterSelfWriteEmits();
};

void TestVaultEchoSuppression::selfWriteDoesNotDoubleEmit()
{
    QSKIP("Phase 3 Task 3.3 turns this on (needs Vault::modify)", QTest::SkipAll);
}

void TestVaultEchoSuppression::externalWriteAfterSelfWriteEmits()
{
    QSKIP("Phase 3 Task 3.3 turns this on (needs Vault::modify)", QTest::SkipAll);
}

QTEST_MAIN(TestVaultEchoSuppression)
#include "tst_vault_echo_suppression.moc"
