// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTextDocument>

#include "markoff/Document.h"
#include "markoff/Renderer.h"
#include "markoff/RenderSettings.h"

class TestRenderer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testRenderReturnsDocument();
    void testRenderHeading();
    void testRenderBold();
    void testRenderCodeBlock();
    void testRenderSettingsAffectOutput();
    void testRenderEmptyDocument();
    void testRenderCallout();
    void testRenderHighlight();
    void testRenderCommentHidden();
    void testRenderTag();
    void testRenderWikilink();
    void testRenderTable();
};

void TestRenderer::testRenderReturnsDocument()
{
    auto doc = Markoff::Document::fromMarkdown(QStringLiteral("Hello"));
    QVERIFY(doc != nullptr);

    Markoff::Renderer renderer;
    auto textDoc = renderer.renderToTextDocument(*doc);

    QVERIFY(textDoc != nullptr);
    QVERIFY(!textDoc->isEmpty());
}

void TestRenderer::testRenderHeading()
{
    auto doc = Markoff::Document::fromMarkdown(QStringLiteral("# Title"));
    QVERIFY(doc != nullptr);

    Markoff::Renderer renderer;
    auto textDoc = renderer.renderToTextDocument(*doc);

    QVERIFY(textDoc != nullptr);
    const QString html = textDoc->toHtml();
    QVERIFY(html.contains(QStringLiteral("Title")));
}

void TestRenderer::testRenderBold()
{
    auto doc = Markoff::Document::fromMarkdown(QStringLiteral("**bold text**"));
    QVERIFY(doc != nullptr);

    Markoff::Renderer renderer;
    auto textDoc = renderer.renderToTextDocument(*doc);

    QVERIFY(textDoc != nullptr);
    const QString html = textDoc->toHtml();
    QVERIFY(html.contains(QStringLiteral("bold text")));
    // Qt renders bold as font-weight in its HTML output
    QVERIFY(html.contains(QStringLiteral("font-weight")) || html.contains(QStringLiteral("<b>")));
}

void TestRenderer::testRenderCodeBlock()
{
    auto doc = Markoff::Document::fromMarkdown(QStringLiteral("```cpp\nint x = 42;\n```"));
    QVERIFY(doc != nullptr);

    Markoff::Renderer renderer;
    auto textDoc = renderer.renderToTextDocument(*doc);

    QVERIFY(textDoc != nullptr);
    const QString html = textDoc->toHtml();
    // Syntax highlighting may split the text into spans, so check parts
    QVERIFY(html.contains(QStringLiteral("int")));
    QVERIFY(html.contains(QStringLiteral("42")));
}

void TestRenderer::testRenderSettingsAffectOutput()
{
    auto doc = Markoff::Document::fromMarkdown(QStringLiteral("Hello world"));
    QVERIFY(doc != nullptr);

    Markoff::RenderSettings settings;
    settings.baseFontSizePt = 20;

    Markoff::Renderer renderer;
    renderer.setSettings(settings);
    auto textDoc = renderer.renderToTextDocument(*doc);

    QVERIFY(textDoc != nullptr);
    const QString html = textDoc->toHtml();
    QVERIFY(html.contains(QStringLiteral("20")));
}

void TestRenderer::testRenderEmptyDocument()
{
    auto doc = Markoff::Document::fromMarkdown(QString());
    QVERIFY(doc != nullptr);

    Markoff::Renderer renderer;
    auto textDoc = renderer.renderToTextDocument(*doc);

    QVERIFY(textDoc != nullptr);
    QVERIFY(textDoc->isEmpty());
}

void TestRenderer::testRenderCallout()
{
    auto doc = Markoff::Document::fromMarkdown(QStringLiteral("> [!warning] Be careful\n> This is important."));
    Markoff::Renderer renderer;
    auto textDoc = renderer.renderToTextDocument(*doc);
    const QString html = textDoc->toHtml();
    // Callout should have the title rendered
    QVERIFY(html.contains(QStringLiteral("Be careful")));
    // Content should be present
    QVERIFY(html.contains(QStringLiteral("important")));
}

void TestRenderer::testRenderHighlight()
{
    auto doc = Markoff::Document::fromMarkdown(QStringLiteral("This has ==highlighted text== in it."));
    Markoff::Renderer renderer;
    auto textDoc = renderer.renderToTextDocument(*doc);
    const QString html = textDoc->toHtml();
    QVERIFY(html.contains(QStringLiteral("highlighted text")));
    // Should not contain the == delimiters in rendered output
    QVERIFY(!html.contains(QStringLiteral("==")));
}

void TestRenderer::testRenderCommentHidden()
{
    auto doc = Markoff::Document::fromMarkdown(QStringLiteral("Visible text %%hidden comment%% more visible."));
    Markoff::Renderer renderer;
    auto textDoc = renderer.renderToTextDocument(*doc);
    const QString html = textDoc->toHtml();
    QVERIFY(html.contains(QStringLiteral("Visible text")));
    QVERIFY(html.contains(QStringLiteral("more visible")));
    // Comment text should NOT appear
    QVERIFY(!html.contains(QStringLiteral("hidden comment")));
}

void TestRenderer::testRenderTag()
{
    auto doc = Markoff::Document::fromMarkdown(QStringLiteral("Check out #my-tag here."));
    Markoff::Renderer renderer;
    auto textDoc = renderer.renderToTextDocument(*doc);
    const QString html = textDoc->toHtml();
    QVERIFY(html.contains(QStringLiteral("my-tag")));
}

void TestRenderer::testRenderWikilink()
{
    auto doc = Markoff::Document::fromMarkdown(QStringLiteral("Link to [[My Note]] here."));
    Markoff::Renderer renderer;
    auto textDoc = renderer.renderToTextDocument(*doc);
    const QString html = textDoc->toHtml();
    QVERIFY(html.contains(QStringLiteral("My Note")));
    QVERIFY(html.contains(QStringLiteral("wikilink:")));
}

void TestRenderer::testRenderTable()
{
    auto doc = Markoff::Document::fromMarkdown(QStringLiteral("| A | B |\n|---|---|\n| 1 | 2 |"));
    Markoff::Renderer renderer;
    auto textDoc = renderer.renderToTextDocument(*doc);
    const QString html = textDoc->toHtml();
    QVERIFY(html.contains(QStringLiteral("A")));
    QVERIFY(html.contains(QStringLiteral("B")));
    QVERIFY(html.contains(QStringLiteral("1")));
    QVERIFY(html.contains(QStringLiteral("2")));
}

QTEST_MAIN(TestRenderer)
#include "tst_renderer.moc"
