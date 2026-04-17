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
