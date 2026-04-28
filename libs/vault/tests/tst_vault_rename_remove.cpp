// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

#include "corbomite/core/NoteDocument.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultRenameRemove : public QObject
{
    Q_OBJECT
private slots:
    void renameUpdatesPathAndFireSignal();
    void removeDeletesFileAndFireSignal();
    void removeTombstonesHandle();
    void copyDuplicatesFile();
    void renameFolderUpdatesDescendantPaths();
    void renameFolderEmitsRenamedForEachDescendant();
    void renameNotifiesOpenNoteDocument();
    void renameRekeysNoteDocumentCache();
    void renameFolderNotifiesDescendantNoteDocuments();
};

void TestVaultRenameRemove::renameUpdatesPathAndFireSignal()
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
    QSignalSpy spy(&vault, &Corbomite::Vault::renamed);

    QVERIFY(vault.rename(tf, QStringLiteral("b.md")));
    QCOMPARE(tf->path, QStringLiteral("b.md"));
    QCOMPARE(spy.count(), 1);
    QVERIFY(vault.getFileByPath(QStringLiteral("b.md")));
    QVERIFY(!vault.getFileByPath(QStringLiteral("a.md")));
}

void TestVaultRenameRemove::removeDeletesFileAndFireSignal()
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
    QSignalSpy spy(&vault, &Corbomite::Vault::deletedFile);

    QVERIFY(vault.remove(tf));
    QCOMPARE(spy.count(), 1);
    QVERIFY(!QFileInfo::exists(dir.path() + "/a.md"));
}

void TestVaultRenameRemove::removeTombstonesHandle()
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
    vault.remove(tf);
    QCOMPARE(tf->deleted, true);
}

void TestVaultRenameRemove::copyDuplicatesFile()
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
    QVERIFY(vault.copy(tf, QStringLiteral("b.md")));
    QVERIFY(QFileInfo::exists(dir.path() + "/b.md"));
    QVERIFY(QFileInfo::exists(dir.path() + "/a.md"));
}

// Regression: renaming a folder must also update every descendant's path in
// m_fileMap (and on the TFile/TFolder nodes themselves). Previously only
// the folder itself moved, leaving descendants with stale paths — lookups
// by new path returned nullptr and lookups by old path returned the moved
// node, both of which are wrong.
void TestVaultRenameRemove::renameFolderUpdatesDescendantPaths()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("alpha/sub")));
    {
        QFile f1(dir.path() + "/alpha/note.md");
        QVERIFY(f1.open(QIODevice::WriteOnly));
        f1.write("x");
    }
    {
        QFile f2(dir.path() + "/alpha/sub/deep.md");
        QVERIFY(f2.open(QIODevice::WriteOnly));
        f2.write("y");
    }

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *folder = vault.getFolderByPath(QStringLiteral("alpha"));
    QVERIFY(folder);
    QVERIFY(vault.rename(folder, QStringLiteral("beta")));

    QVERIFY2(vault.getFileByPath(QStringLiteral("beta/note.md")),
             "direct descendant must be reachable at new path");
    QVERIFY2(vault.getFileByPath(QStringLiteral("beta/sub/deep.md")),
             "transitive descendant must be reachable at new path");
    QVERIFY2(!vault.getFileByPath(QStringLiteral("alpha/note.md")),
             "old descendant path must no longer resolve");
    QVERIFY2(!vault.getFileByPath(QStringLiteral("alpha/sub/deep.md")),
             "old transitive descendant path must no longer resolve");
    QVERIFY2(vault.getFolderByPath(QStringLiteral("beta/sub")),
             "sub-folder must be reachable at new path");
}

void TestVaultRenameRemove::renameFolderEmitsRenamedForEachDescendant()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("alpha")));
    {
        QFile f1(dir.path() + "/alpha/one.md");
        QVERIFY(f1.open(QIODevice::WriteOnly));
        f1.write("x");
    }
    {
        QFile f2(dir.path() + "/alpha/two.md");
        QVERIFY(f2.open(QIODevice::WriteOnly));
        f2.write("y");
    }

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *folder = vault.getFolderByPath(QStringLiteral("alpha"));
    QVERIFY(folder);
    QSignalSpy spy(&vault, &Corbomite::Vault::renamed);
    QVERIFY(vault.rename(folder, QStringLiteral("beta")));

    // Folder itself + 2 descendants = 3 emissions. Plugins (notably link-
    // updaters) listen on renamed and miss descendant moves otherwise.
    QCOMPARE(spy.count(), 3);
}

void TestVaultRenameRemove::renameNotifiesOpenNoteDocument()
{
    // Audit: views.md §"Top suspected bugs" — title not refreshed on
    // external rename. Vault::rename must update an open NoteDocument's
    // relativePath and emit pathChanged so FileView subclasses can refresh
    // their tab caption from name().
    QTemporaryDir dir;
    QFile f(dir.path() + "/old.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *doc = vault.openDocument(QStringLiteral("old.md"));
    QVERIFY(doc);
    QCOMPARE(doc->name(), QStringLiteral("old"));
    QSignalSpy pathSpy(doc, &Corbomite::NoteDocument::pathChanged);

    auto *tf = vault.getFileByPath(QStringLiteral("old.md"));
    QVERIFY(tf);
    QVERIFY(vault.rename(tf, QStringLiteral("new.md")));

    QCOMPARE(pathSpy.count(), 1);
    QCOMPARE(pathSpy.at(0).at(0).toString(), QStringLiteral("old.md"));
    QCOMPARE(doc->relativePath(), QStringLiteral("new.md"));
    QCOMPARE(doc->name(), QStringLiteral("new"));
}

void TestVaultRenameRemove::renameRekeysNoteDocumentCache()
{
    // After rename, cachedDocument(newPath) must resolve to the same
    // NoteDocument and cachedDocument(oldPath) must resolve to nullptr —
    // otherwise reopening the file in the same vault session creates a
    // duplicate doc with a stale path.
    QTemporaryDir dir;
    QFile f(dir.path() + "/foo.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *doc = vault.openDocument(QStringLiteral("foo.md"));
    QVERIFY(doc);
    auto *tf = vault.getFileByPath(QStringLiteral("foo.md"));
    QVERIFY(vault.rename(tf, QStringLiteral("bar.md")));

    QCOMPARE(vault.cachedDocument(QStringLiteral("bar.md")), doc);
    QCOMPARE(vault.cachedDocument(QStringLiteral("foo.md")),
             static_cast<Corbomite::NoteDocument *>(nullptr));
}

void TestVaultRenameRemove::renameFolderNotifiesDescendantNoteDocuments()
{
    // Folder rename must propagate setRelativePath to every cached
    // NoteDocument under the renamed prefix; otherwise descendants keep a
    // stale relativePath and views holding them show the wrong name.
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("alpha")));
    {
        QFile f(dir.path() + "/alpha/note.md");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
    }

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *doc = vault.openDocument(QStringLiteral("alpha/note.md"));
    QVERIFY(doc);
    QSignalSpy spy(doc, &Corbomite::NoteDocument::pathChanged);

    auto *folder = vault.getFolderByPath(QStringLiteral("alpha"));
    QVERIFY(folder);
    QVERIFY(vault.rename(folder, QStringLiteral("beta")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(doc->relativePath(), QStringLiteral("beta/note.md"));
    QCOMPARE(vault.cachedDocument(QStringLiteral("beta/note.md")), doc);
}

QTEST_MAIN(TestVaultRenameRemove)
#include "tst_vault_rename_remove.moc"
