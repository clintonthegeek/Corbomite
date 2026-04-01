// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTextDocument>
#include "corbomite/core/RegexRenderEngine.h"

class TestRenderEngine : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testRenderProducesDocument()
    {
        Corbomite::RegexRenderEngine engine;
        auto result = engine.render(QStringLiteral("# Hello\n\nWorld"));
        QVERIFY(result != nullptr);
        QVERIFY(result->toQTextDocument() != nullptr);
        QVERIFY(result->toQTextDocument()->toPlainText().contains(QStringLiteral("Hello")));
        QVERIFY(result->toQTextDocument()->toPlainText().contains(QStringLiteral("World")));
    }

    void testEmptyMarkdown()
    {
        Corbomite::RegexRenderEngine engine;
        auto result = engine.render(QString());
        QVERIFY(result != nullptr);
        QVERIFY(result->toQTextDocument() != nullptr);
    }

    void testProfileAffectsOutput()
    {
        Corbomite::RegexRenderEngine engine;

        engine.setProfile(Corbomite::RenderProfile::readingMode());
        auto reading = engine.render(QStringLiteral("# Test"));
        QString readingHtml = reading->toQTextDocument()->toHtml();

        engine.setProfile(Corbomite::RenderProfile::canvasCard());
        auto canvas = engine.render(QStringLiteral("# Test"));
        QString canvasHtml = canvas->toQTextDocument()->toHtml();

        // They should produce different results (different font sizes at minimum)
        QVERIFY(readingHtml != canvasHtml);
    }

    void testRenderWithSubpath()
    {
        Corbomite::RegexRenderEngine engine;
        QString md = QStringLiteral("# Title\n\nIntro\n\n## Section\n\nSection content");
        Corbomite::RenderOptions opts;
        opts.subpath = QStringLiteral("#Section");
        auto result = engine.render(md, opts);
        QVERIFY(result != nullptr);
        QString text = result->toQTextDocument()->toPlainText();
        QVERIFY(text.contains(QStringLiteral("Section content")));
        QVERIFY(!text.contains(QStringLiteral("Intro")));
    }

    void testOptionsOverrideProfile()
    {
        Corbomite::RegexRenderEngine engine;
        engine.setProfile(Corbomite::RenderProfile::readingMode()); // 16pt font

        Corbomite::RenderOptions opts;
        opts.baseFontSizePt = 20;

        auto result = engine.render(QStringLiteral("Hello"), opts);
        QString html = result->toQTextDocument()->toHtml();
        QVERIFY(html.contains(QStringLiteral("20")));
    }
};

QTEST_MAIN(TestRenderEngine)
#include "tst_renderengine.moc"
