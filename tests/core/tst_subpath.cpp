// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/MarkdownRenderEngine.h"

class TestSubpath : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testEmptySubpathReturnsFullDocument()
    {
        QString md = QStringLiteral("# Title\n\nSome content\n\n## Section\n\nMore content");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QString());
        QCOMPARE(result, md);
    }

    void testHeadingSubpath()
    {
        QString md = QStringLiteral("# Title\n\nIntro\n\n## Section A\n\nContent A\n\n## Section B\n\nContent B");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#Section A"));
        QVERIFY(result.contains(QStringLiteral("## Section A")));
        QVERIFY(result.contains(QStringLiteral("Content A")));
        QVERIFY(!result.contains(QStringLiteral("Section B")));
        QVERIFY(!result.contains(QStringLiteral("Intro")));
    }

    void testHeadingSubpathIncludesSubsections()
    {
        QString md = QStringLiteral("# Title\n\n## Parent\n\nParent content\n\n### Child\n\nChild content\n\n## Sibling\n\nSibling content");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#Parent"));
        QVERIFY(result.contains(QStringLiteral("## Parent")));
        QVERIFY(result.contains(QStringLiteral("### Child")));
        QVERIFY(result.contains(QStringLiteral("Child content")));
        QVERIFY(!result.contains(QStringLiteral("Sibling")));
    }

    void testHeadingSubpathAtEOF()
    {
        QString md = QStringLiteral("# Title\n\n## Last Section\n\nFinal content");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#Last Section"));
        QVERIFY(result.contains(QStringLiteral("## Last Section")));
        QVERIFY(result.contains(QStringLiteral("Final content")));
    }

    void testHeadingCaseInsensitive()
    {
        QString md = QStringLiteral("## My Heading\n\nContent here");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#my heading"));
        QVERIFY(result.contains(QStringLiteral("## My Heading")));
        QVERIFY(result.contains(QStringLiteral("Content here")));
    }

    void testNonexistentHeading()
    {
        QString md = QStringLiteral("# Title\n\nContent");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#Nonexistent"));
        QVERIFY(result.isEmpty());
    }

    void testBlockIdSubpath()
    {
        QString md = QStringLiteral("First paragraph.\n\nThis is the target paragraph with some\nimportant content. ^my-block\n\nThird paragraph.");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#^my-block"));
        QVERIFY(result.contains(QStringLiteral("target paragraph")));
        QVERIFY(result.contains(QStringLiteral("important content")));
        QVERIFY(!result.contains(QStringLiteral("^my-block")));
        QVERIFY(!result.contains(QStringLiteral("First paragraph")));
        QVERIFY(!result.contains(QStringLiteral("Third paragraph")));
    }

    void testNonexistentBlockId()
    {
        QString md = QStringLiteral("# Title\n\nContent");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#^nonexistent"));
        QVERIFY(result.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestSubpath)
#include "tst_subpath.moc"
