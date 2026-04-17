// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSet>
#include <QStringList>
#include "corbomite/vault/PluginPermissionGrantDialog.h"

class TestPluginPermissionDialog : public QObject
{
    Q_OBJECT
private slots:
    void grantAllReturnsFullSet();
    void uncheckDropsFromSet();
    void cancelLeavesNoGrantsAndFlagsCancelled();
    void describeReturnsHumanReadable();
    void unknownPermissionFallsBackToToken();
};

void TestPluginPermissionDialog::grantAllReturnsFullSet()
{
    const QStringList requested{
        QStringLiteral("vault.read"),
        QStringLiteral("network")
    };
    Corbomite::PluginPermissionGrantDialog dlg(QStringLiteral("Test Plugin"),
                                                QStringLiteral("A test"),
                                                requested);
    const QSet<QString> granted = dlg.grantedIfAccepted();
    QCOMPARE(granted.size(), 2);
    QVERIFY(granted.contains(QStringLiteral("vault.read")));
    QVERIFY(granted.contains(QStringLiteral("network")));
}

void TestPluginPermissionDialog::uncheckDropsFromSet()
{
    const QStringList requested{
        QStringLiteral("vault.read"),
        QStringLiteral("network")
    };
    Corbomite::PluginPermissionGrantDialog dlg(QStringLiteral("Test"),
                                                QStringLiteral(""),
                                                requested);
    dlg.setCheckedForTest(QStringLiteral("network"), false);
    const QSet<QString> granted = dlg.grantedIfAccepted();
    QCOMPARE(granted.size(), 1);
    QVERIFY(granted.contains(QStringLiteral("vault.read")));
    QVERIFY(!granted.contains(QStringLiteral("network")));
}

void TestPluginPermissionDialog::cancelLeavesNoGrantsAndFlagsCancelled()
{
    const QStringList requested{QStringLiteral("vault.read")};
    Corbomite::PluginPermissionGrantDialog dlg(QStringLiteral("Test"),
                                                QStringLiteral(""),
                                                requested);
    QVERIFY(!dlg.wasCancelled()); // not cancelled until cancelForTest fires
    dlg.cancelForTest();
    QVERIFY(dlg.wasCancelled());
    QVERIFY(dlg.grantedIfAccepted().isEmpty());
}

void TestPluginPermissionDialog::describeReturnsHumanReadable()
{
    using D = Corbomite::PluginPermissionGrantDialog;
    QVERIFY(!D::describe(QStringLiteral("vault.read")).isEmpty());
    QVERIFY(D::describe(QStringLiteral("vault.read"))
                != QStringLiteral("vault.read"));
    QVERIFY(!D::describe(QStringLiteral("network")).isEmpty());
}

void TestPluginPermissionDialog::unknownPermissionFallsBackToToken()
{
    using D = Corbomite::PluginPermissionGrantDialog;
    const QString token = QStringLiteral("never.heard.of.it");
    QCOMPARE(D::describe(token), token);
}

QTEST_MAIN(TestPluginPermissionDialog)
#include "tst_plugin_permission_dialog.moc"
