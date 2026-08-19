// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestFileManagerNewFile : public QObject
{
    Q_OBJECT
private slots:
    void createNewMarkdownFileAtRoot();
    void collisionFreeNaming();
    void getNewFileParentUsesHint();
    void createNewFolderCollisionFree();
};

void TestFileManagerNewFile::createNewMarkdownFileAtRoot()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *f = fm.createNewMarkdownFile(v.getRoot(), QStringLiteral("Note"));
    QVERIFY(f);
    QCOMPARE(f->extension, QStringLiteral("md"));
}

void TestFileManagerNewFile::collisionFreeNaming()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *a = fm.createNewMarkdownFile(v.getRoot(), QStringLiteral("Note"));
    auto *b = fm.createNewMarkdownFile(v.getRoot(), QStringLiteral("Note"));
    QVERIFY(a && b);
    QVERIFY(a->path != b->path);
    // Obsidian's collision numbering starts at " 1", not " 2": the first
    // collision on "Note.md" should yield "Note 1.md".
    QCOMPARE(b->path, QStringLiteral("Note 1.md"));

    // A collision against an existing file that differs only by case must
    // still be detected (vault-portable, matches Vault::create's own
    // case-insensitive collision check) rather than silently returning the
    // bare candidate.
    auto *other = fm.createNewMarkdownFile(v.getRoot(), QStringLiteral("Other"));
    QVERIFY(other);
    QCOMPARE(other->path, QStringLiteral("Other.md"));
    auto *c = fm.createNewMarkdownFile(v.getRoot(), QStringLiteral("other"));
    QVERIFY(c);
    QCOMPARE(c->path, QStringLiteral("other 1.md"));
}

void TestFileManagerNewFile::getNewFileParentUsesHint()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    v.createFolder(QStringLiteral("sub"));
    Corbomite::FileManager fm(&v, nullptr);

    auto *parent = fm.getNewFileParent(QStringLiteral("sub/x.md"));
    QVERIFY(parent);
    QCOMPARE(parent->path, QStringLiteral("sub"));
}

void TestFileManagerNewFile::createNewFolderCollisionFree()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *a = fm.createNewFolder(v.getRoot());
    auto *b = fm.createNewFolder(v.getRoot());
    QVERIFY(a && b);
    QVERIFY(a->path != b->path);
}

QTEST_MAIN(TestFileManagerNewFile)
#include "tst_file_manager_newfile.moc"
