// tests/dialogs/tst_merge_file_dialog.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster F-a P2.4 — MergeFileDialog: the source is excluded, filtering works,
// append/prepend round-trips, and the dialog only *chooses* (Cancel is a true
// no-op — it never touches files).
#include <QTest>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>

#include "dialogs/MergeFileDialog.h"

using namespace Corbomite;

class TestMergeFileDialog : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void excludesSourceAndFilters()
    {
        MergeFileDialog dlg(QStringLiteral("A.md"),
                            {QStringLiteral("A.md"), QStringLiteral("B.md"),
                             QStringLiteral("notes/C.md")});
        auto *list = dlg.findChild<QListWidget *>(QStringLiteral("mergeList"));
        QVERIFY(list);
        // Source is never offered.
        for (int i = 0; i < list->count(); ++i)
            QVERIFY(list->item(i)->text() != QStringLiteral("A.md"));
        QCOMPARE(list->count(), 2);

        auto *filter = dlg.findChild<QLineEdit *>(QStringLiteral("mergeFilter"));
        QVERIFY(filter);
        filter->setText(QStringLiteral("C"));
        QCOMPARE(list->count(), 1);
        QCOMPARE(list->item(0)->text(), QStringLiteral("notes/C.md"));
    }

    void choosingTargetAdvancesAndDefaultsToAppend()
    {
        MergeFileDialog dlg(QStringLiteral("A.md"),
                            {QStringLiteral("A.md"), QStringLiteral("B.md")});
        auto *list = dlg.findChild<QListWidget *>(QStringLiteral("mergeList"));
        auto *next = dlg.findChild<QPushButton *>(QStringLiteral("mergeNext"));
        auto *merge = dlg.findChild<QPushButton *>(QStringLiteral("mergeConfirm"));
        auto *append = dlg.findChild<QRadioButton *>(QStringLiteral("mergeAppend"));
        auto *prepend = dlg.findChild<QRadioButton *>(QStringLiteral("mergePrepend"));
        QVERIFY(list && next && merge && append && prepend);

        QVERIFY(!merge->isEnabled());  // nothing chosen yet
        list->setCurrentRow(0);
        next->click();
        QVERIFY(merge->isEnabled());
        QCOMPARE(dlg.selectedTarget(), QStringLiteral("B.md"));
        QVERIFY(append->isChecked());
        QVERIFY(!dlg.prependSelected());

        merge->click();
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }

    void prependTogglesAndCancelSelectsNothing()
    {
        MergeFileDialog dlg(QStringLiteral("A.md"),
                            {QStringLiteral("B.md"), QStringLiteral("C.md")});
        auto *list = dlg.findChild<QListWidget *>(QStringLiteral("mergeList"));
        auto *next = dlg.findChild<QPushButton *>(QStringLiteral("mergeNext"));
        auto *prepend = dlg.findChild<QRadioButton *>(QStringLiteral("mergePrepend"));

        // Cancel before choosing anything: no target, rejected.
        auto *cancel = dlg.findChild<QPushButton *>();
        Q_UNUSED(cancel);
        dlg.reject();
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Rejected));
        QVERIFY(dlg.selectedTarget().isEmpty());

        // Re-run choosing prepend.
        list->setCurrentRow(0);
        next->click();
        prepend->setChecked(true);
        QVERIFY(dlg.prependSelected());
    }
};

QTEST_MAIN(TestMergeFileDialog)
#include "tst_merge_file_dialog.moc"
