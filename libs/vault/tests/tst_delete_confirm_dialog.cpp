// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <qglobal.h>

#include <KConfigGroup>
#include <KSharedConfig>

#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "dialogs/DeleteConfirmDialog.h"

class TestDeleteConfirmDialog : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void testCancelIsDefaultButton();
    void testSystemTrashOptionTextShown();
    void testVaultTrashOptionTextShown();
    void testPermanentOptionTextShown();
    void testDontAskAgainPersists();
    void testFolderDialogHasNoDontAskAgain();
};

namespace {

void writeFile(const QString &path, const QByteArray &body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(body);
}

void setTrashOption(const QString &v)
{
    KConfigGroup grp(KSharedConfig::openConfig(), QStringLiteral("Files"));
    grp.writeEntry(QStringLiteral("TrashOption"), v);
    grp.writeEntry(QStringLiteral("PromptDelete"), true);
    grp.sync();
}

}  // namespace

void TestDeleteConfirmDialog::initTestCase()
{
    // XDG_CONFIG_HOME redirection happens in main() before QApplication
    // constructs — any later redirection would miss the path Qt/KConfig
    // has already cached. Sanity check that we're not pointing at the
    // user's real config.
    QVERIFY(qgetenv("XDG_CONFIG_HOME").contains("delete-confirm-test"));
}

void TestDeleteConfirmDialog::init()
{
    // Reset config between tests.
    KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Files"));
    KSharedConfig::openConfig()->sync();
    setTrashOption(QStringLiteral("system"));
}

void TestDeleteConfirmDialog::testCancelIsDefaultButton()
{
    QTemporaryDir tmp;
    writeFile(tmp.path() + QStringLiteral("/foo.md"), "hi");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("foo.md"));
    QVERIFY(file);

    Corbomite::DeleteConfirmDialog dlg(file, &vault, nullptr);
    auto *bb = dlg.findChild<QDialogButtonBox *>();
    QVERIFY(bb);
    auto *cancel = bb->button(QDialogButtonBox::Cancel);
    QVERIFY(cancel);
    QVERIFY(cancel->isDefault());
}

void TestDeleteConfirmDialog::testSystemTrashOptionTextShown()
{
    setTrashOption(QStringLiteral("system"));

    QTemporaryDir tmp;
    writeFile(tmp.path() + QStringLiteral("/foo.md"), "hi");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("foo.md"));
    QVERIFY(file);

    Corbomite::DeleteConfirmDialog dlg(file, &vault, nullptr);
    QVERIFY(dlg.bodyText().contains(QStringLiteral("system trash"),
                                    Qt::CaseInsensitive));
}

void TestDeleteConfirmDialog::testVaultTrashOptionTextShown()
{
    setTrashOption(QStringLiteral("vault"));

    QTemporaryDir tmp;
    writeFile(tmp.path() + QStringLiteral("/foo.md"), "hi");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("foo.md"));
    QVERIFY(file);

    Corbomite::DeleteConfirmDialog dlg(file, &vault, nullptr);
    QVERIFY(dlg.bodyText().contains(QStringLiteral("trash folder"),
                                    Qt::CaseInsensitive));
}

void TestDeleteConfirmDialog::testPermanentOptionTextShown()
{
    setTrashOption(QStringLiteral("permanent"));

    QTemporaryDir tmp;
    writeFile(tmp.path() + QStringLiteral("/foo.md"), "hi");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("foo.md"));
    QVERIFY(file);

    Corbomite::DeleteConfirmDialog dlg(file, &vault, nullptr);
    QVERIFY(dlg.bodyText().contains(QStringLiteral("permanently deleted"),
                                    Qt::CaseInsensitive));
}

void TestDeleteConfirmDialog::testDontAskAgainPersists()
{
    QTemporaryDir tmp;
    writeFile(tmp.path() + QStringLiteral("/foo.md"), "hi");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("foo.md"));
    QVERIFY(file);

    Corbomite::DeleteConfirmDialog dlg(file, &vault, nullptr);
    dlg.setDontAskAgain(true);
    dlg.accept();

    KConfigGroup grp(KSharedConfig::openConfig(), QStringLiteral("Files"));
    QCOMPARE(grp.readEntry(QStringLiteral("PromptDelete"), true), false);
}

void TestDeleteConfirmDialog::testFolderDialogHasNoDontAskAgain()
{
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("sub"));

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto *folder = vault.getAbstractFileByPath(QStringLiteral("sub"));
    QVERIFY(folder);

    Corbomite::DeleteConfirmDialog dlg(folder, &vault, nullptr);
    // The folder path hides the "Don't ask again" checkbox — setting it
    // is a no-op and accepting must not flip PromptDelete.
    KConfigGroup grp(KSharedConfig::openConfig(), QStringLiteral("Files"));
    grp.writeEntry(QStringLiteral("PromptDelete"), true);
    grp.sync();

    dlg.setDontAskAgain(true);  // no-op for folders
    dlg.accept();

    QCOMPARE(grp.readEntry(QStringLiteral("PromptDelete"), false), true);
}

// Custom main so XDG_CONFIG_HOME is redirected before QApplication
// constructs — otherwise QStandardPaths caches the path and KSharedConfig
// ends up touching the real user config file.
int main(int argc, char *argv[])
{
    QTemporaryDir xdg(QDir::tempPath()
                      + QLatin1String("/delete-confirm-test-XXXXXX"));
    xdg.setAutoRemove(true);
    qputenv("XDG_CONFIG_HOME", xdg.path().toUtf8());
    qputenv("XDG_DATA_HOME",
            QString(xdg.path() + QLatin1String("/data")).toUtf8());

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tst_delete_confirm_dialog"));
    TestDeleteConfirmDialog t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_delete_confirm_dialog.moc"
