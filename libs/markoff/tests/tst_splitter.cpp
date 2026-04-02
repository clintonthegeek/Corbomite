// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QFile>
#include <QTextStream>
#include "MarkdownSplitter.h"
#include "TreeSitterParser.h"

#ifndef SHOWCASE_PATH
#define SHOWCASE_PATH "/home/clinton/dev/Corbomite/libs/markoff/tests/showcase.md"
#endif

using namespace Markoff;

class TestSplitter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testNoBlocks();
    void testSingleTable();
    void testSingleCodeBlock();
    void testTableBetweenText();
    void testMultipleBlocks();
    void testEmptyDocument();
    void testBlockAtStart();
    void testBlockAtEnd();
    void testShowcaseFile();
};

void TestSplitter::testNoBlocks()
{
    TreeSitterParser parser;
    auto segments = MarkdownSplitter::split(
        QStringLiteral("# Hello\n\nSome text here."), parser);
    QCOMPARE(segments.size(), 1);
    QCOMPARE(segments[0].type, MarkdownSegment::Text);
    QVERIFY(segments[0].text.contains(QStringLiteral("Hello")));
}

void TestSplitter::testSingleTable()
{
    TreeSitterParser parser;
    auto segments = MarkdownSplitter::split(
        QStringLiteral("| A | B |\n|---|---|\n| 1 | 2 |"), parser);
    QCOMPARE(segments.size(), 1);
    QCOMPARE(segments[0].type, MarkdownSegment::Table);
    QVERIFY(segments[0].text.contains(QStringLiteral("| A | B |")));
}

void TestSplitter::testSingleCodeBlock()
{
    TreeSitterParser parser;
    auto segments = MarkdownSplitter::split(
        QStringLiteral("```python\nprint('hi')\n```"), parser);
    QCOMPARE(segments.size(), 1);
    QCOMPARE(segments[0].type, MarkdownSegment::FencedCodeBlock);
    QVERIFY(segments[0].text.contains(QStringLiteral("python")));
}

void TestSplitter::testTableBetweenText()
{
    TreeSitterParser parser;
    QString md = QStringLiteral(
        "# Title\n\nBefore table.\n\n"
        "| A | B |\n|---|---|\n| 1 | 2 |\n\n"
        "After table.");
    auto segments = MarkdownSplitter::split(md, parser);
    QCOMPARE(segments.size(), 3);
    QCOMPARE(segments[0].type, MarkdownSegment::Text);
    QVERIFY(segments[0].text.contains(QStringLiteral("Title")));
    QCOMPARE(segments[1].type, MarkdownSegment::Table);
    QVERIFY(segments[1].text.contains(QStringLiteral("| A | B |")));
    QCOMPARE(segments[2].type, MarkdownSegment::Text);
    QVERIFY(segments[2].text.contains(QStringLiteral("After table")));
}

void TestSplitter::testMultipleBlocks()
{
    TreeSitterParser parser;
    QString md = QStringLiteral(
        "Intro\n\n"
        "| A |\n|---|\n| 1 |\n\n"
        "Middle\n\n"
        "```js\nconsole.log('x')\n```\n\n"
        "End");
    auto segments = MarkdownSplitter::split(md, parser);
    QCOMPARE(segments.size(), 5);
    QCOMPARE(segments[0].type, MarkdownSegment::Text);
    QCOMPARE(segments[1].type, MarkdownSegment::Table);
    QCOMPARE(segments[2].type, MarkdownSegment::Text);
    QCOMPARE(segments[3].type, MarkdownSegment::FencedCodeBlock);
    QCOMPARE(segments[4].type, MarkdownSegment::Text);
}

void TestSplitter::testEmptyDocument()
{
    TreeSitterParser parser;
    auto segments = MarkdownSplitter::split(QString(), parser);
    QCOMPARE(segments.size(), 1);
    QCOMPARE(segments[0].type, MarkdownSegment::Text);
}

void TestSplitter::testBlockAtStart()
{
    TreeSitterParser parser;
    QString md = QStringLiteral("| A |\n|---|\n| 1 |\n\nText after.");
    auto segments = MarkdownSplitter::split(md, parser);
    QVERIFY(segments.size() >= 2);
    QCOMPARE(segments[0].type, MarkdownSegment::Table);
    QCOMPARE(segments.last().type, MarkdownSegment::Text);
    QVERIFY(segments.last().text.contains(QStringLiteral("Text after")));
}

void TestSplitter::testBlockAtEnd()
{
    TreeSitterParser parser;
    QString md = QStringLiteral("Text before.\n\n| A |\n|---|\n| 1 |");
    auto segments = MarkdownSplitter::split(md, parser);
    QVERIFY(segments.size() >= 2);
    QCOMPARE(segments[0].type, MarkdownSegment::Text);
    QVERIFY(segments[0].text.contains(QStringLiteral("Text before")));
    QCOMPARE(segments.last().type, MarkdownSegment::Table);
}

void TestSplitter::testShowcaseFile()
{
    TreeSitterParser parser;
    QFile f(QStringLiteral(SHOWCASE_PATH));
    QVERIFY2(f.open(QIODevice::ReadOnly), "Cannot open showcase.md");
    QString md = QTextStream(&f).readAll();
    auto segments = MarkdownSplitter::split(md, parser);

    // showcase.md has 3 code blocks, 2 tables, and text between them
    // So we expect at least 8 segments
    for (int i = 0; i < segments.size(); ++i) {
        QString typeName = segments[i].type == MarkdownSegment::Text ? QStringLiteral("Text")
            : segments[i].type == MarkdownSegment::Table ? QStringLiteral("Table")
            : QStringLiteral("CodeBlock");
        QString preview = segments[i].text.left(50).replace(QLatin1Char('\n'), QStringLiteral("\\n"));
        qDebug() << i << typeName << "len:" << segments[i].text.size() << preview;
    }
    QVERIFY2(segments.size() >= 8,
             qPrintable(QStringLiteral("Expected >=8 segments, got %1").arg(segments.size())));

    // Last segment should contain "Frontmatter" (near end of file)
    QVERIFY(segments.last().text.contains(QStringLiteral("Frontmatter")));
}

QTEST_MAIN(TestSplitter)
#include "tst_splitter.moc"
