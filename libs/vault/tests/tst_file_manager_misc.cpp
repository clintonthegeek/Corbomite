// SPDX-License-Identifier: GPL-3.0-or-later
//
// Covers the remaining FileManager methods (Task 5.5 scope):
//   getAvailablePathForAttachment / generateMarkdownLink / insertIntoFile
//
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestFileManagerMisc : public QObject
{
    Q_OBJECT
private slots:
    void attachmentPathPicksUnoccupied();
    void attachmentPathSameFolderAsSource();
    void generateWikiLinkForMarkdown();
    void generateWikiLinkWithSubpathAndDisplay();
    void insertAppends();
    void insertPrepends();
};

namespace {
void writeFile(const QString &p, const QByteArray &b)
{
    QFile f(p);
    f.open(QIODevice::WriteOnly);
    f.write(b);
    f.close();
}
QByteArray readFileAll(const QString &p)
{
    QFile f(p);
    f.open(QIODevice::ReadOnly);
    return f.readAll();
}
}

void TestFileManagerMisc::attachmentPathPicksUnoccupied()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/Pasted.png", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    const QString got = fm.getAvailablePathForAttachment(QStringLiteral("Pasted.png"));
    QVERIFY(!got.isEmpty());
    QVERIFY(got != QStringLiteral("Pasted.png"));
}

void TestFileManagerMisc::attachmentPathSameFolderAsSource()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    v.createFolder(QStringLiteral("notes"));
    Corbomite::FileManager fm(&v, nullptr);

    const QString got = fm.getAvailablePathForAttachment(
        QStringLiteral("img.png"), QStringLiteral("notes/host.md"));
    QCOMPARE(got, QStringLiteral("notes/img.png"));
}

void TestFileManagerMisc::generateWikiLinkForMarkdown()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/Foo.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *foo = v.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    QCOMPARE(fm.generateMarkdownLink(foo, QStringLiteral("")),
             QStringLiteral("[[Foo]]"));
}

void TestFileManagerMisc::generateWikiLinkWithSubpathAndDisplay()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/Foo.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *foo = v.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    QCOMPARE(fm.generateMarkdownLink(foo, QString(), QStringLiteral("#Heading"),
                                      QStringLiteral("Alias")),
             QStringLiteral("[[Foo#Heading|Alias]]"));
}

void TestFileManagerMisc::insertAppends()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "one");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *tf = v.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(fm.insertIntoFile(tf, QByteArray(" two"),
                              Corbomite::FileManager::InsertMode::Append));
    QCOMPARE(readFileAll(dir.path() + "/a.md"), QByteArray("one two"));
}

void TestFileManagerMisc::insertPrepends()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "one");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *tf = v.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(fm.insertIntoFile(tf, QByteArray("pre "),
                              Corbomite::FileManager::InsertMode::Prepend));
    QCOMPARE(readFileAll(dir.path() + "/a.md"), QByteArray("pre one"));
}

QTEST_MAIN(TestFileManagerMisc)
#include "tst_file_manager_misc.moc"
