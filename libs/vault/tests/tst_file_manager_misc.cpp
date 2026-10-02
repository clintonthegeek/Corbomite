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
    void generateMarkdownLinkHonoursAppJson();
    void generateMarkdownLinkRelativeToSource();
    void generateMarkdownLinkAbsoluteFormat();
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

void TestFileManagerMisc::generateMarkdownLinkHonoursAppJson()
{
    QTemporaryDir dir;
    QDir(dir.path()).mkpath(QStringLiteral(".obsidian"));
    QDir(dir.path()).mkpath(QStringLiteral("notes"));
    writeFile(dir.path() + "/.obsidian/app.json",
              R"({"useMarkdownLinks":true,"newLinkFormat":"relative"})");
    writeFile(dir.path() + "/notes/Foo.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *foo = v.getFileByPath(QStringLiteral("notes/Foo.md"));
    QVERIFY(foo);
    // Obsidian-shaped Markdown link, not the wiki-link Corbomite always emitted.
    QCOMPARE(fm.generateMarkdownLink(foo, QStringLiteral("notes/Bar.md")),
             QStringLiteral("[Foo](Foo.md)"));
}

void TestFileManagerMisc::generateMarkdownLinkRelativeToSource()
{
    QTemporaryDir dir;
    QDir(dir.path()).mkpath(QStringLiteral(".obsidian"));
    QDir(dir.path()).mkpath(QStringLiteral("notes"));
    writeFile(dir.path() + "/.obsidian/app.json",
              R"({"useMarkdownLinks":true,"newLinkFormat":"relative"})");
    writeFile(dir.path() + "/notes/Foo.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *foo = v.getFileByPath(QStringLiteral("notes/Foo.md"));
    QVERIFY(foo);
    // Source lives in a different folder -> the path stays vault-relative.
    QCOMPARE(fm.generateMarkdownLink(foo, QStringLiteral("other/Bar.md")),
             QStringLiteral("[Foo](notes/Foo.md)"));
}

void TestFileManagerMisc::generateMarkdownLinkAbsoluteFormat()
{
    QTemporaryDir dir;
    QDir(dir.path()).mkpath(QStringLiteral(".obsidian"));
    QDir(dir.path()).mkpath(QStringLiteral("notes"));
    writeFile(dir.path() + "/.obsidian/app.json",
              R"({"useMarkdownLinks":true,"newLinkFormat":"absolute"})");
    writeFile(dir.path() + "/notes/Foo.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *foo = v.getFileByPath(QStringLiteral("notes/Foo.md"));
    QVERIFY(foo);
    QCOMPARE(fm.generateMarkdownLink(foo, QStringLiteral("notes/Bar.md")),
             QStringLiteral("[Foo](notes/Foo.md)"));
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
