// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 3a end-to-end test: feeds a 500-line synthetic markdown fixture
// through ReadingView::setPlainText() and asserts the scene is non-empty
// and every section got a mounted QGraphicsItem.

#include "corbomite/readingview/ReadingSection.h"
#include "corbomite/readingview/ReadingView.h"

#include <QGraphicsScene>
#include <QTest>

using namespace Corbomite::ReadingView;

class TestReadingViewEndToEnd : public QObject
{
    Q_OBJECT

private slots:
    void fiveHundredLineNote();
    void scrollApiIsNotIdentity();
};

static QString buildFixture()
{
    QString md;
    md += QStringLiteral("---\ntitle: Demo\n---\n");
    for (int i = 0; i < 30; ++i) {
        md += QStringLiteral("\n# Heading %1\n\n").arg(i);
        md += QStringLiteral(
            "This is **bold** and *italic* and `code` in a paragraph.\n");
        md += QStringLiteral("A second line with a [link](http://x) and a "
                              "[[WikiNote]] reference.\n\n");
        md += QStringLiteral("## Subheading %1\n\n").arg(i);
        md += QStringLiteral("- item one\n- item two\n- item three\n\n");
        md += QStringLiteral("1. first\n2. second\n\n");
        md += QStringLiteral("> a blockquote line\n\n");
        md += QStringLiteral("---\n\n");
        md += QStringLiteral("```python\n"
                             "def f(x):\n"
                             "    return x * 2\n"
                             "```\n\n");
        // New Phase 3b content types.
        md += QStringLiteral("| L | C | R |\n|:-|:-:|-:|\n"
                              "| a | b | c |\n| d | e | f |\n\n");
        md += QStringLiteral("Some math: $x=1$ in a sentence.\n\n");
        md += QStringLiteral("$$\ny = 2\n$$\n\n");
        md += QStringLiteral("![alt-%1](missing-%1.png)\n\n").arg(i);
        md += QStringLiteral("```mermaid\ngraph TD;\nA-->B;\n```\n\n");
    }
    return md;
}

void TestReadingViewEndToEnd::fiveHundredLineNote()
{
    ReadingView rv;
    const QString md = buildFixture();
    // Guard: the fixture should comfortably exceed 500 lines.
    QVERIFY(md.count(QLatin1Char('\n')) >= 500);

    rv.setPlainText(md);
    auto *s = rv.scene();
    QVERIFY(s != nullptr);

    const QRectF rect = s->sceneRect();
    QVERIFY(rect.width() > 0);
    QVERIFY(rect.height() > 0);

    QVERIFY(!rv.sections().isEmpty());
    for (const auto &sec : rv.sections()) {
        QVERIFY(sec->graphicsItem() != nullptr);
    }
}

void TestReadingViewEndToEnd::scrollApiIsNotIdentity()
{
    ReadingView rv;
    rv.setPlainText(buildFixture());
    rv.resize(800, 600);
    // Ensure the scrollbars know their range.
    rv.show();
    QTest::qWait(20);

    const float initial = rv.scrollPositionVisualLine();
    rv.setScrollPositionVisualLine(25.0f);
    QTest::qWait(10);
    const float after = rv.scrollPositionVisualLine();
    // The API should actually move; we don't require exact equality
    // because pixel rounding + viewport clipping alter the reached value.
    QVERIFY(after > initial);
}

QTEST_MAIN(TestReadingViewEndToEnd)
#include "tst_readingview_end_to_end.moc"
