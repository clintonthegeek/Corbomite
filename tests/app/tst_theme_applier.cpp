// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster V Task 1.7 — smoke test the KColorScheme plumbing MainWindow's
// applyTheme() relies on. KColorSchemeManager is a process-wide singleton
// and palette changes are asynchronous, so this test covers only the
// preconditions: the manager instance is reachable, its model is valid,
// and the Breeze scheme IDs applyTheme() uses (BreezeLight / BreezeDark)
// resolve to a valid model index on this system.
#include <QtTest/QtTest>
#include <QApplication>
#include <KColorSchemeManager>

class TstThemeApplier : public QObject {
    Q_OBJECT
private slots:
    void managerInstanceIsAvailable()
    {
        auto *mgr = KColorSchemeManager::instance();
        QVERIFY(mgr != nullptr);
        QVERIFY(mgr->model() != nullptr);
    }

    void breezeSchemesResolvable()
    {
        auto *mgr = KColorSchemeManager::instance();
        const QModelIndex light =
            mgr->indexForSchemeId(QStringLiteral("BreezeLight"));
        const QModelIndex dark =
            mgr->indexForSchemeId(QStringLiteral("BreezeDark"));
        // At least one Breeze variant must be installed for applyTheme()
        // to have a meaningful target. Some minimal distros ship only one.
        QVERIFY(light.isValid() || dark.isValid());
    }

    void systemThemeUnsetsScheme()
    {
        // The "system" branch calls activateScheme(QModelIndex()) to hand
        // control back to the OS. Verify the call doesn't crash.
        auto *mgr = KColorSchemeManager::instance();
        mgr->activateScheme(QModelIndex());
    }
};

QTEST_MAIN(TstThemeApplier)
#include "tst_theme_applier.moc"
