// SPDX-License-Identifier: GPL-3.0-or-later
// StyledRenderEngine wraps Markoff::Styled::DocumentRenderer into the
// MarkdownRenderEngine/RenderedDocument abstraction. Offscreen.

#include "corbomite/core/StyledRenderEngine.h"
#include "corbomite/core/RenderedDocument.h"

#include <QTextDocument>
#include <QTest>

using Corbomite::StyledRenderEngine;

class StyledRenderEngineTest : public QObject {
    Q_OBJECT
private Q_SLOTS:

    void render_producesNonEmptyDocumentWithContent()
    {
        StyledRenderEngine engine;
        auto rendered = engine.render(
            QStringLiteral("# Title\n\nbody text here"));
        QVERIFY(rendered != nullptr);
        QTextDocument *doc = rendered->toQTextDocument();
        QVERIFY(doc != nullptr);
        // Styled keeps delimiters visible; plain text contains the content.
        QVERIFY(doc->toPlainText().contains(QStringLiteral("body text here")));
        QVERIFY(doc->toPlainText().contains(QStringLiteral("Title")));
    }

    void render_emptyMarkdown_isSafe()
    {
        StyledRenderEngine engine;
        auto rendered = engine.render(QString());
        QVERIFY(rendered != nullptr);
        QVERIFY(rendered->toQTextDocument() != nullptr);
    }

    void render_subpath_extractsSection()
    {
        StyledRenderEngine engine;
        Corbomite::RenderOptions opts;
        opts.subpath = QStringLiteral("#Second");
        auto rendered = engine.render(
            QStringLiteral("# First\n\nalpha\n\n# Second\n\nbravo"), opts);
        const QString text = rendered->toQTextDocument()->toPlainText();
        QVERIFY(text.contains(QStringLiteral("bravo")));
        QVERIFY(!text.contains(QStringLiteral("alpha")));
    }
};

QTEST_MAIN(StyledRenderEngineTest)
#include "tst_styled_render_engine.moc"
