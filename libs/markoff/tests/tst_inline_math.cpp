// SPDX-License-Identifier: GPL-3.0-or-later
#include <QObject>
#include <QTest>
#include <QChar>
#include <QGraphicsScene>

#include "MarkdownTextItem.h"
#include "MarkdownHighlighter.h"
#include "TreeSitterParser.h"

using namespace Markoff;

class TestInlineMath : public QObject {
    Q_OBJECT

private slots:
    void roundTripPlainText();
    void substitutesInlineMath();
    void substitutesDisplayMath();
    void preservesAdjacentText();
    void allMarkdownReturnsCanonicalSource();
    void noSubstitutionWithoutHighlighter();
    void parserDistinguishesInlineFromDisplayMath();

private:
    /// Build a text item with a highlighter, parsing the source so the
    /// span map is set, then trigger math substitution.
    MarkdownTextItem *makeItem(const QString &source);
    TreeSitterParser m_parser;
    QGraphicsScene m_scene;  // owns text items in tests so they get cleaned up
};

MarkdownTextItem *TestInlineMath::makeItem(const QString &source)
{
    auto *item = new MarkdownTextItem;
    m_scene.addItem(item);

    auto *hl = new MarkdownHighlighter(item->document());
    hl->setMode(MarkdownHighlighter::Mode::LivePreview);

    if (!m_parser.parse(source)) {
        qFatal("parser failed to accept test source: %s", qPrintable(source));
    }
    hl->setSpanMap(m_parser.buildSpanMap());

    item->setPlainText(source);
    hl->rehighlight();
    item->refreshMathSubstitution();
    return item;
}

void TestInlineMath::roundTripPlainText()
{
    // No math at all → allMarkdown() returns the source unchanged.
    auto *item = makeItem(QStringLiteral("plain text with no math"));
    QCOMPARE(item->allMarkdown(), QStringLiteral("plain text with no math"));
}

void TestInlineMath::substitutesInlineMath()
{
    auto *item = makeItem(QStringLiteral("before $x^2$ after"));

    const QString docText = item->document()->toPlainText();
    // The document should now contain the U+FFFC glyph in place of the math.
    QVERIFY2(docText.contains(QChar::ObjectReplacementCharacter),
             "expected substituted glyph in document text");
    // And the literal source should not appear in the displayed document.
    QVERIFY2(!docText.contains(QStringLiteral("$x^2$")),
             "raw math source should not be in displayed document");

    // allMarkdown() must reconstruct the canonical source.
    QCOMPARE(item->allMarkdown(), QStringLiteral("before $x^2$ after"));
}

void TestInlineMath::substitutesDisplayMath()
{
    // Display math uses $$...$$ delimiters; substitution should preserve that.
    auto *item = makeItem(QStringLiteral("Pre $$\\sum x_i$$ post"));

    const QString docText = item->document()->toPlainText();
    QVERIFY(docText.contains(QChar::ObjectReplacementCharacter));
    QVERIFY(!docText.contains(QStringLiteral("$$")));

    QCOMPARE(item->allMarkdown(), QStringLiteral("Pre $$\\sum x_i$$ post"));
}

void TestInlineMath::preservesAdjacentText()
{
    // Multiple math regions on a single line round-trip correctly.
    const QString src = QStringLiteral("a $x$ b $y^2$ c");
    auto *item = makeItem(src);
    QCOMPARE(item->allMarkdown(), src);
}

void TestInlineMath::allMarkdownReturnsCanonicalSource()
{
    // Stripping returns the document to canonical source form.
    auto *item = makeItem(QStringLiteral("eq: $a+b$"));
    QVERIFY(item->document()->toPlainText().contains(QChar::ObjectReplacementCharacter));

    item->stripMathSubstitution();
    const QString docText = item->document()->toPlainText();
    QVERIFY(!docText.contains(QChar::ObjectReplacementCharacter));
    QCOMPARE(docText, QStringLiteral("eq: $a+b$"));
    // allMarkdown() agrees.
    QCOMPARE(item->allMarkdown(), QStringLiteral("eq: $a+b$"));
}

void TestInlineMath::noSubstitutionWithoutHighlighter()
{
    // A bare item without a highlighter should not crash on
    // refreshMathSubstitution() — it's a no-op.
    auto *item = new MarkdownTextItem;
    m_scene.addItem(item);
    item->setPlainText(QStringLiteral("$x^2$"));
    item->refreshMathSubstitution();
    QCOMPARE(item->allMarkdown(), QStringLiteral("$x^2$"));
}

void TestInlineMath::parserDistinguishesInlineFromDisplayMath()
{
    // Regression: tree-sitter-markdown emits the same node type for both
    // inline `$...$` and display `$$...$$`. TreeSitterParser distinguishes
    // them by inspecting the source bytes for `$$` at the node start.
    auto check = [&](const QString &src, bool expectDisplay) {
        QVERIFY(m_parser.parse(src));
        const auto spans = m_parser.buildSpanMap();
        bool sawMath = false, sawDisplay = false;
        for (const auto &s : spans) {
            if (s.math) sawMath = true;
            if (s.mathDisplay) sawDisplay = true;
        }
        QVERIFY2(sawMath, qPrintable(QStringLiteral("no math span for: ") + src));
        if (expectDisplay) {
            QVERIFY2(sawDisplay, qPrintable(QStringLiteral("expected display flag for: ") + src));
        } else {
            QVERIFY2(!sawDisplay, qPrintable(QStringLiteral("did not expect display flag for: ") + src));
        }
    };
    check(QStringLiteral("inline $x^2$ here"), false);
    check(QStringLiteral("display $$\\sum x_i$$ here"), true);
    check(QStringLiteral("$a$"), false);
    check(QStringLiteral("$$a$$"), true);
}

QTEST_MAIN(TestInlineMath)
#include "tst_inline_math.moc"
