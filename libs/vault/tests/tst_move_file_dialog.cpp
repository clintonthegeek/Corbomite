// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QLineEdit>
#include <QListWidget>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "dialogs/MoveFileDialog.h"

class TestMoveFileDialog : public QObject
{
    Q_OBJECT
private slots:
    void testFolderListExcludesSourceParent();
    void testFilterNarrowsList();
    void testRootIsIncluded();
    void testSelectionReturnsPath();
};

namespace {
void writeFile(const QString &path, const QByteArray &body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(body);
}
}  // namespace

void TestMoveFileDialog::testFolderListExcludesSourceParent()
{
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("archive/2024"));
    QDir(tmp.path()).mkpath(QStringLiteral("notes"));
    writeFile(tmp.path() + QStringLiteral("/notes/foo.md"), "hi");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("notes/foo.md"));
    QVERIFY(file);

    Corbomite::MoveFileDialog dlg(file, &vault, nullptr);
    const auto list = dlg.availableFolderPaths();

    QVERIFY(list.contains(QStringLiteral("/")));
    QVERIFY(list.contains(QStringLiteral("archive")));
    QVERIFY(list.contains(QStringLiteral("archive/2024")));
    // Source's current parent excluded.
    QVERIFY(!list.contains(QStringLiteral("notes")));
}

void TestMoveFileDialog::testFilterNarrowsList()
{
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("archive"));
    QDir(tmp.path()).mkpath(QStringLiteral("daily"));
    QDir(tmp.path()).mkpath(QStringLiteral("notes"));
    writeFile(tmp.path() + QStringLiteral("/src.md"), "");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("src.md"));
    QVERIFY(file);

    Corbomite::MoveFileDialog dlg(file, &vault, nullptr);
    dlg.setFilterText(QStringLiteral("arch"));
    auto *listWidget = dlg.findChild<QListWidget *>();
    QVERIFY(listWidget);
    QCOMPARE(listWidget->count(), 1);
    QCOMPARE(listWidget->item(0)->text(), QStringLiteral("archive"));
}

void TestMoveFileDialog::testRootIsIncluded()
{
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("sub"));
    writeFile(tmp.path() + QStringLiteral("/sub/src.md"), "");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("sub/src.md"));
    QVERIFY(file);

    Corbomite::MoveFileDialog dlg(file, &vault, nullptr);
    QVERIFY(dlg.availableFolderPaths().contains(QStringLiteral("/")));
}

void TestMoveFileDialog::testSelectionReturnsPath()
{
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("archive"));
    writeFile(tmp.path() + QStringLiteral("/src.md"), "");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("src.md"));
    QVERIFY(file);

    Corbomite::MoveFileDialog dlg(file, &vault, nullptr);
    auto *listWidget = dlg.findChild<QListWidget *>();
    QVERIFY(listWidget);

    // Find the "archive" row and make it the current selection, then
    // activate it to set the result.
    for (int i = 0; i < listWidget->count(); ++i) {
        if (listWidget->item(i)->text() == QStringLiteral("archive")) {
            listWidget->setCurrentRow(i);
            emit listWidget->itemActivated(listWidget->item(i));
            break;
        }
    }

    QCOMPARE(dlg.selectedFolderPath(), QStringLiteral("archive"));
}

QTEST_MAIN(TestMoveFileDialog)
#include "tst_move_file_dialog.moc"
