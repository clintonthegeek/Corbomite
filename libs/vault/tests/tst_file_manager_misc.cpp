// SPDX-License-Identifier: GPL-3.0-or-later
//
// Covers the remaining FileManager methods:
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
    void attachmentNameStripsLinkSyntax_data();
    void attachmentNameStripsLinkSyntax();
    void attachmentNameTruncatedAt250Units();
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

// Obsidian runs an attachment basename through `DD`/`xD`
// (app/own/1120-DD.js:2, app/own/1118-SD.js:3) before the collision check:
// `: # | ^ \ CR LF`, the pair `%%` and the pairs `[[` / `]]` each become a
// space, then whitespace runs collapse. The set exists to strip link syntax
// back out of a pasted linktext; `%%` is comment syntax and `|` is the wiki
// alias separator, so `[[Note|alias]].png` must land as `Note alias .png`'s
// stem, not as a filename full of brackets and pipes.
void TestFileManagerMisc::attachmentNameStripsLinkSyntax_data()
{
    QTest::addColumn<QString>("linktext");
    QTest::addColumn<QString>("expected");

    QTest::newRow("wiki link")   << "[[Note]]"                 << QStringLiteral("Note");
    QTest::newRow("alias")       << "[[Note|alias]].png"       << QStringLiteral("Note alias .png");
    QTest::newRow("comment")     << "note%%comment.png"        << QStringLiteral("note comment.png");
    QTest::newRow("drive letter")<< "C:shots.png"              << QStringLiteral("C shots.png");
    QTest::newRow("caret pipe")  << "a^b|c.png"                << QStringLiteral("a b c.png");
    QTest::newRow("collapsed")   << "a:::b.png"                << QStringLiteral("a b.png");
    QTest::newRow("half open")   << "[Note].png"               << QStringLiteral("[Note].png");
    QTest::newRow("plain")       << "image.png"                << QStringLiteral("image.png");
}

void TestFileManagerMisc::attachmentNameStripsLinkSyntax()
{
    QFETCH(QString, linktext);
    QFETCH(QString, expected);

    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    QCOMPARE(fm.getAvailablePathForAttachment(linktext), expected);
}

// `DD(...).slice(0, 250)` (app/own/1129-UD.js:575) — 250 UTF-16 code units,
// applied after the sanitise pass, and before any collision suffix.
void TestFileManagerMisc::attachmentNameTruncatedAt250Units()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    const QString got = fm.getAvailablePathForAttachment(
        QString(300, QLatin1Char('a')) + QStringLiteral(".png"));
    QCOMPARE(got.size(), 250);
}

void TestFileManagerMisc::generateWikiLinkForMarkdown()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/Foo.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *f = v.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(f);
    QCOMPARE(fm.generateMarkdownLink(f, QStringLiteral("Bar.md")),
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
