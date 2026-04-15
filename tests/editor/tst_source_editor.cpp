// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 2 test suite for Corbomite::SourceEditor (qutepart-corbomite fork).
//
// Covers:
//   - cursor round-trip through (line, column)
//   - scroll integer + fractional round-trip via visual-line float
//   - reflow stability (resize + re-read scroll position)
//   - textChanged fires once on setPlainText
//   - cursorPositionChanged fires on setCursorPosition
//
// EphemeralState-fixture round-trip is not tested here: that struct does not
// yet exist in libs/storage/ as of 2026-04-15 — it lands in Cluster E Phase 1.
//
// Runs headless via QT_QPA_PLATFORM=offscreen.

#include "SourceEditor.h"

#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTest>

using Corbomite::SourceEditor;

namespace {
QString makeLines(int count)
{
    QStringList lines;
    lines.reserve(count);
    for (int i = 0; i < count; ++i) {
        lines.append(QStringLiteral("line %1").arg(i));
    }
    return lines.join(QLatin1Char('\n'));
}
} // namespace

class SourceEditorTest : public QObject {
    Q_OBJECT

private slots:
    void cursorRoundTrip() {
        SourceEditor editor;
        editor.setPlainText(QStringLiteral("line1\nline2\nline3"));

        editor.setCursorPosition({1, 3});
        const auto got = editor.cursorPosition();
        QCOMPARE(got.line, 1);
        QCOMPARE(got.column, 3);
    }

    void scrollIntegerRoundTrip() {
        SourceEditor editor;
        editor.setPlainText(makeLines(50));
        editor.resize(400, 200);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));

        editor.setScrollPosition(5.0f);
        const float got = editor.scrollPosition();
        QVERIFY2(std::abs(got - 5.0f) < 0.01f,
                 qPrintable(QStringLiteral("integer scroll round-trip off: got %1").arg(got)));
    }

    void scrollFractionalRoundTrip() {
        // QPlainTextEdit's vertical scrollbar is visual-line granular —
        // sub-line fractional positioning is impossible through the public
        // API. The Phase 2 contract is: fractional `setScrollPosition` is
        // rounded to the nearest visual line, and `scrollPosition` reads
        // back that rounded line (with any residual `topLineFracture` Qt
        // happens to carry). Tolerance here is ±0.55 to accept either side
        // of the round for 5.5 → {5, 6}. The FoldCalculator / KSyntaxHighlighting
        // rework in Phase 4 is the natural spot to revisit sub-line fidelity.
        SourceEditor editor;
        editor.setPlainText(makeLines(50));
        editor.resize(400, 200);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));

        editor.setScrollPosition(5.5f);
        const float got = editor.scrollPosition();
        QVERIFY2(std::abs(got - 5.5f) <= 0.55f,
                 qPrintable(QStringLiteral("fractional scroll round-trip off: got %1").arg(got)));
    }

    void scrollReflowStability() {
        SourceEditor editor;
        editor.setPlainText(makeLines(50));
        editor.resize(600, 300);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));

        editor.setScrollPosition(5.5f);
        // Force a reflow by halving width.
        editor.resize(300, 300);
        QTest::qWait(20);

        const float got = editor.scrollPosition();
        // ±0.5 visual-line tolerance per the plan's spec; we accept equality
        // at 0.5 because the scrollbar rounds fractional line requests
        // (see scrollFractionalRoundTrip for the same reason).
        QVERIFY2(std::abs(got - 5.5f) <= 0.55f,
                 qPrintable(QStringLiteral("reflow stability failed: got %1").arg(got)));
    }

    void textChangedFiresOnceOnSetPlainText() {
        SourceEditor editor;
        QSignalSpy spy(&editor, &SourceEditor::textChanged);
        editor.setPlainText(QStringLiteral("hello"));
        QCOMPARE(spy.count(), 1);
    }

    void cursorPositionChangedFires() {
        SourceEditor editor;
        editor.setPlainText(QStringLiteral("line1\nline2\nline3"));
        QSignalSpy spy(&editor, &SourceEditor::cursorPositionChanged);
        editor.setCursorPosition({2, 2});
        QVERIFY(spy.count() >= 1);
        const auto args = spy.takeLast();
        const auto pos = args.at(0).value<SourceEditor::CursorPos>();
        QCOMPARE(pos.line, 2);
        QCOMPARE(pos.column, 2);
    }

    void foldedHeadingsScaffoldRoundTrip() {
        // Phase-2 scaffold: set / get round-trips; real fold logic lands in
        // Phase 4 / 7. This test merely locks in the scaffold contract.
        SourceEditor editor;
        QVERIFY(editor.foldedHeadings().isEmpty());
        editor.setFoldedHeadings({3, 7, 11});
        QCOMPARE(editor.foldedHeadings(), QVector<int>({3, 7, 11}));
    }

    void readOnlyRoundTrip() {
        SourceEditor editor;
        QVERIFY(!editor.isReadOnly());
        editor.setReadOnly(true);
        QVERIFY(editor.isReadOnly());
        editor.setReadOnly(false);
        QVERIFY(!editor.isReadOnly());
    }
};

QTEST_MAIN(SourceEditorTest)
#include "tst_source_editor.moc"
