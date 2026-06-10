// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 1 (contract v2) — NoteEditorWidget ephemeral state round-trips
// through the MarkdownView base: cursor as 1-based flat-visual-line
// CursorPos, scroll as 0.0–1.0 fraction. Replaces the pre-port tests that
// drove the retired leaf-specific scrollPosition/cursorLine APIs.
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"
#include "corbomite/storage/EphemeralState.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkdownView.h>

#include <QObject>
#include <QStringList>
#include <QTest>

#include <cmath>

using Corbomite::EphemeralState;
using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;

namespace {
QString makeParagraphs(int count)
{
    QStringList blocks;
    blocks.reserve(count);
    for (int i = 0; i < count; ++i)
        blocks.append(QStringLiteral("Paragraph %1 line A.").arg(i));
    return blocks.join(QStringLiteral("\n\n"));
}
} // namespace

class NoteEditorWidgetEphemeralTest : public QObject {
    Q_OBJECT

    void roundTripInMode(NoteEditorWidget::ViewMode mode)
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.setViewMode(mode);

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(makeParagraphs(60));
        widget.setNoteDocument(&doc);

        auto *leaf = widget.activeLeaf();
        QVERIFY(leaf);
        leaf->setCursorPosition({7, 3});
        leaf->setScrollPositionVisualLine(0.5f);
        QTRY_COMPARE(leaf->cursorPosition().line, 7);

        const EphemeralState saved = widget.saveEphemeralState();
        QCOMPARE(saved.cursor.line, 7);
        QVERIFY2(std::abs(saved.scroll - 0.5f) <= 0.1f,
                 qPrintable(QStringLiteral("scroll drift: %1").arg(saved.scroll)));

        leaf->setCursorPosition({1, 1});
        leaf->setScrollPositionVisualLine(0.0f);
        QTest::qWait(20);
        widget.restoreEphemeralState(saved);

        QTRY_COMPARE(leaf->cursorPosition().line, 7);
        QVERIFY(leaf->scrollPositionVisualLine() > 0.3f);
    }

private Q_SLOTS:
    void sourceModeRoundTrip()  { roundTripInMode(NoteEditorWidget::ViewMode::Source); }
    void liveModeRoundTrip()    { roundTripInMode(NoteEditorWidget::ViewMode::LivePreview); }
    void readingModeRoundTrip() { roundTripInMode(NoteEditorWidget::ViewMode::Reading); }

    void cursorSurvivesModeSwitch()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.setViewMode(NoteEditorWidget::ViewMode::Source);

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(makeParagraphs(60));
        widget.setNoteDocument(&doc);

        widget.activeLeaf()->setCursorPosition({9, 1});
        QTest::qWait(20);
        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);
        QTest::qWait(20);

        // setViewMode captures the outgoing leaf's state and restores it on
        // the incoming leaf — line position carries across the mode switch.
        QCOMPARE(widget.activeLeaf()->cursorPosition().line, 9);
    }

    void goToLineAllModes()
    {
        const NoteEditorWidget::ViewMode modes[] = {
            NoteEditorWidget::ViewMode::Source,
            NoteEditorWidget::ViewMode::LivePreview,
            NoteEditorWidget::ViewMode::Reading};
        for (auto mode : modes) {
            NoteEditorWidget widget;
            widget.resize(600, 240);
            widget.show();
            QVERIFY(QTest::qWaitForWindowExposed(&widget));
            widget.setViewMode(mode);

            NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
            doc.setMarkdown(makeParagraphs(60));
            widget.setNoteDocument(&doc);

            QVERIFY2(widget.goToLine(5),
                     "goToLine must succeed in every mode under contract v2");
            QTRY_COMPARE(widget.activeLeaf()->cursorPosition().line, 5);
        }
    }
};

QTEST_MAIN(NoteEditorWidgetEphemeralTest)
#include "tst_note_editor_widget_ephemeral.moc"
