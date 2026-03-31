// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/MarkdownRenderer.h"

class TestMarkdownRenderer : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testHeading()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("# Title"));
        QVERIFY(html.contains(QStringLiteral("<h1>Title</h1>")));
    }

    void testHeadingLevels()
    {
        Corbomite::MarkdownRenderer r;
        QVERIFY(r.renderToHtml(QStringLiteral("## Sub")).contains(QStringLiteral("<h2>")));
        QVERIFY(r.renderToHtml(QStringLiteral("### Sub3")).contains(QStringLiteral("<h3>")));
    }

    void testBold()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("This is **bold** text"));
        QVERIFY(html.contains(QStringLiteral("<strong>bold</strong>")));
    }

    void testItalic()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("This is *italic* text"));
        QVERIFY(html.contains(QStringLiteral("<em>italic</em>")));
    }

    void testLink()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("[Click](https://example.com)"));
        QVERIFY(html.contains(QStringLiteral("<a href=\"https://example.com\">Click</a>")));
    }

    void testInlineCode()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("Use `printf()` here"));
        QVERIFY(html.contains(QStringLiteral("<code>printf()</code>")));
    }

    void testCodeBlock()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("```cpp\nint x = 1;\n```"));
        QVERIFY(html.contains(QStringLiteral("<pre><code")));
        QVERIFY(html.contains(QStringLiteral("int x = 1;")));
    }

    void testWikiLink()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("See [[My Note]] here"));
        QVERIFY(html.contains(QStringLiteral("class=\"internal-link\"")));
        QVERIFY(html.contains(QStringLiteral("My Note")));
    }

    void testHighlight()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("This is ==important== text"));
        QVERIFY(html.contains(QStringLiteral("<mark>important</mark>")));
    }

    void testCommentStripped()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("Visible %%hidden%% text"));
        QVERIFY(!html.contains(QStringLiteral("hidden")));
        QVERIFY(html.contains(QStringLiteral("Visible")));
        QVERIFY(html.contains(QStringLiteral("text")));
    }

    void testCallout()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("> [!warning] Be careful\n> Content here"));
        QVERIFY(html.contains(QStringLiteral("callout")));
        QVERIFY(html.contains(QStringLiteral("warning")));
    }

    void testTag()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("Hello #project tag"));
        QVERIFY(html.contains(QStringLiteral("class=\"tag\"")));
        QVERIFY(html.contains(QStringLiteral("#project")));
    }

    void testUnorderedList()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("- item one\n- item two"));
        QVERIFY(html.contains(QStringLiteral("<ul>")));
        QVERIFY(html.contains(QStringLiteral("<li>")));
    }

    void testBlockquote()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("> quoted text"));
        QVERIFY(html.contains(QStringLiteral("<blockquote>")));
    }

    void testHorizontalRule()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("---"));
        QVERIFY(html.contains(QStringLiteral("<hr")));
    }

    void testCheckbox()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("- [ ] todo\n- [x] done"));
        QVERIFY(html.contains(QStringLiteral("checkbox")));
    }
};

QTEST_MAIN(TestMarkdownRenderer)
#include "tst_markdownrenderer.moc"
