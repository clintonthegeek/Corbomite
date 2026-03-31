// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QCoreApplication>
#include "markdownhighlighter.h"

class TestObsidianHighlighting : public QObject {
    Q_OBJECT

    // Helper: get format at a character position in first block
    QTextCharFormat formatAt(QTextDocument *doc, int pos, int block = 0) {
        auto tb = doc->findBlockByNumber(block);
        auto *layout = tb.layout();
        auto ranges = layout->formats();
        for (const auto &range : ranges) {
            if (pos >= range.start && pos < range.start + range.length) {
                return range.format;
            }
        }
        return {};
    }

    // Helper: check if position has a specific foreground color
    bool hasForeground(QTextDocument *doc, int pos, const QColor &color, int block = 0) {
        auto fmt = formatAt(doc, pos, block);
        return fmt.foreground().color() == color;
    }

    // Obsidian format colors (must match initTextFormats)
    const QColor wikiLinkColor{123, 108, 217};
    const QColor tagColor{224, 108, 117};
    const QColor calloutColor{209, 154, 102};
    const QColor commentColor{92, 99, 112};
    const QColor maskedColor{204, 204, 204};
    const QColor highlightBg{255, 243, 176};

    QTextDocument *createDoc(const QString &text) {
        auto *doc = new QTextDocument(this);
        auto *hl = new MarkdownHighlighter(doc);
        doc->setPlainText(text);
        QCoreApplication::processEvents();
        hl->rehighlight();
        QCoreApplication::processEvents();
        return doc;
    }

private Q_SLOTS:
    void testWikiLinkBasic()
    {
        auto *doc = createDoc(QStringLiteral("See [[My Note]] here"));
        // "[[" at pos 4-5 should be masked
        QVERIFY(hasForeground(doc, 4, maskedColor));
        // "My Note" at pos 6-12 should be wikilink color
        QVERIFY(hasForeground(doc, 6, wikiLinkColor));
        // "]]" at pos 13-14 should be masked
        QVERIFY(hasForeground(doc, 13, maskedColor));
    }

    void testWikiLinkWithAlias()
    {
        auto *doc = createDoc(QStringLiteral("[[Note|Display Text]]"));
        // "Note" at pos 2-5 should be wikilink
        QVERIFY(hasForeground(doc, 2, wikiLinkColor));
        // "|" at pos 6 should be masked
        QVERIFY(hasForeground(doc, 6, maskedColor));
        // "Display Text" at pos 7-18 should be wikilink
        QVERIFY(hasForeground(doc, 7, wikiLinkColor));
    }

    void testEmbed()
    {
        auto *doc = createDoc(QStringLiteral("![[image.png]]"));
        // Content should have embed color (same as wikilink)
        QVERIFY(hasForeground(doc, 3, wikiLinkColor));
    }

    void testTag()
    {
        auto *doc = createDoc(QStringLiteral("Hello #project tag"));
        // "#project" at pos 6-13 should be tag color
        QVERIFY(hasForeground(doc, 6, tagColor));
    }

    void testNestedTag()
    {
        auto *doc = createDoc(QStringLiteral("#parent/child/deep"));
        QVERIFY(hasForeground(doc, 0, tagColor));
    }

    void testHeadingNotTag()
    {
        auto *doc = createDoc(QStringLiteral("# Heading"));
        // "#" should NOT be tag color — it's a heading
        QVERIFY(!hasForeground(doc, 0, tagColor));
    }

    void testCallout()
    {
        auto *doc = createDoc(QStringLiteral("> [!warning] Be careful"));
        // "warning" should be callout color
        // Find where "warning" starts — "> [!" is 4 chars
        QVERIFY(hasForeground(doc, 4, calloutColor));
    }

    void testHighlightSyntax()
    {
        auto *doc = createDoc(QStringLiteral("Some ==highlighted== text"));
        // "==" delimiters should be masked
        QVERIFY(hasForeground(doc, 5, maskedColor));  // opening ==
        // "highlighted" should have highlight background
        auto fmt = formatAt(doc, 7);
        QCOMPARE(fmt.background().color(), highlightBg);
    }

    void testComment()
    {
        auto *doc = createDoc(QStringLiteral("Visible %%hidden%% visible"));
        // "%%" delimiters should be masked
        QVERIFY(hasForeground(doc, 8, maskedColor));
        // "hidden" should be comment color
        QVERIFY(hasForeground(doc, 10, commentColor));
    }

    void testBlockRef()
    {
        auto *doc = createDoc(QStringLiteral("Some text ^my-block-id"));
        // "^my-block-id" should be block ref color
        QVERIFY(hasForeground(doc, 10, commentColor));  // Same gray as blockref
    }

    void testWikiLinkNotInCode()
    {
        auto *doc = createDoc(QStringLiteral("`[[not a link]]`"));
        // Inside code span — should NOT be wikilink color
        QVERIFY(!hasForeground(doc, 3, wikiLinkColor));
    }

    void testTagNotInCode()
    {
        auto *doc = createDoc(QStringLiteral("`#not-a-tag`"));
        QVERIFY(!hasForeground(doc, 1, tagColor));
    }
};

QTEST_MAIN(TestObsidianHighlighting)
#include "tst_obsidian_highlighting.moc"
