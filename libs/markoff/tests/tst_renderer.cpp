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
    QVERIFY(html.contains(QStringLiteral("int x = 42")));
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

QTEST_MAIN(TestRenderer)
#include "tst_renderer.moc"
