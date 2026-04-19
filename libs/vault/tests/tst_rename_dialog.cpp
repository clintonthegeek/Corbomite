// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QLineEdit>

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "dialogs/RenameDialog.h"

class TestRenameDialog : public QObject
{
    Q_OBJECT
private slots:
    void testBasenameIsPreselected();
    void testInvalidInputBlocksSave();
    void testValidSaveReturnsNewName();
    void testSameNameIsValid();
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

void TestRenameDialog::testBasenameIsPreselected()
{
    QTemporaryDir tmp;
    writeFile(tmp.path() + QStringLiteral("/foo.md"), "hi");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("foo.md"));
    QVERIFY(file);

    Corbomite::RenameDialog dlg(file, &vault, nullptr);
    auto *edit = dlg.findChild<QLineEdit *>();
    QVERIFY(edit);
    QCOMPARE(edit->text(), QStringLiteral("foo.md"));
    QCOMPARE(edit->selectedText(), QStringLiteral("foo"));
}

void TestRenameDialog::testInvalidInputBlocksSave()
{
    QTemporaryDir tmp;
    writeFile(tmp.path() + QStringLiteral("/foo.md"), "hi");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("foo.md"));
    QVERIFY(file);

    Corbomite::RenameDialog dlg(file, &vault, nullptr);
    auto *edit = dlg.findChild<QLineEdit *>();
    QVERIFY(edit);
    edit->setText(QStringLiteral("bad:name.md"));
    QVERIFY(!dlg.isSaveEnabled());
}

void TestRenameDialog::testValidSaveReturnsNewName()
{
    QTemporaryDir tmp;
    writeFile(tmp.path() + QStringLiteral("/foo.md"), "hi");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("foo.md"));
    QVERIFY(file);

    Corbomite::RenameDialog dlg(file, &vault, nullptr);
    auto *edit = dlg.findChild<QLineEdit *>();
    QVERIFY(edit);
    edit->setText(QStringLiteral("bar.md"));
    QVERIFY(dlg.isSaveEnabled());
    dlg.accept();
    QCOMPARE(dlg.proposedNewName(), QStringLiteral("bar.md"));
}

void TestRenameDialog::testSameNameIsValid()
{
    // Renaming a file to its own name is not a collision — Save should
    // stay enabled (though the FileManager caller short-circuits out
    // without touching disk).
    QTemporaryDir tmp;
    writeFile(tmp.path() + QStringLiteral("/foo.md"), "hi");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("foo.md"));
    QVERIFY(file);

    Corbomite::RenameDialog dlg(file, &vault, nullptr);
    QVERIFY(dlg.isSaveEnabled());
}

QTEST_MAIN(TestRenameDialog)
#include "tst_rename_dialog.moc"
