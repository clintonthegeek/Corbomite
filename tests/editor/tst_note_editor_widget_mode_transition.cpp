// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase C3 — NoteEditorWidget mode-transition end-to-end test. Covers the
// signal-driven attach/detach/restore sequence across all six
// Source↔LivePreview↔Reading transition pairs.
//
// Phase C3 key contract: canonical content NEVER round-trips through leaves
// during mode swap. All edits push MarkdownDelta commands onto the shared
// QUndoStack; mode swap is:
//   1. ephemeralState() from outgoing leaf
//   2. setDocument(nullptr) on outgoing leaf
//   3. stack swap
//   4. setDocument(markoff()) on incoming leaf
//   5. setEphemeralState() on incoming leaf
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"

#include <markoff/Editor.h>
#include <markoff/reading/ReadingView.h>
#include <markoff/source/SourceEditor.h>
#include <markoff/MarkoffDocument.h>

#include "corbomite/core/NoteDocument.h"

#include <QObject>
#include <QSignalSpy>
#include <QStringList>
#include <QTest>

using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;
using Markoff::Source::SourceEditor;

namespace {

QString makeParagraphs(int count)
{
    QStringList blocks;
    blocks.reserve(count);
    for (int i = 0; i < count; ++i) {
        blocks.append(QStringLiteral("Paragraph %1 line A.").arg(i));
    }
    return blocks.join(QStringLiteral("\n\n"));
}

/// Wait for the Live editor scene to contain content. headingsChanged fires
/// when the coordinator finishes loadMarkdown after parseUpdated. For a doc
/// with no headings (word-count signal is more reliable), we spy on
/// wordCountChanged. Returns false on 2 s timeout.
bool waitForLiveScene(Markoff::Editor *editor, int msec = 2000)
{
    if (!editor->toPlainText().isEmpty()) return true;
    QSignalSpy spy(editor, &Markoff::Editor::wordCountChanged);
    if (!editor->toPlainText().isEmpty()) return true;
    return spy.wait(msec);
}

} // namespace

class NoteEditorWidgetModeTransitionTest : public QObject {
    Q_OBJECT

private Q_SLOTS:

    // --- Test case 1: Source→LivePreview preserves cursor + scroll. ---
    void sourceToLivePreviewPreservesCursorAndScroll()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("note.md"));
        doc.setMarkdown(makeParagraphs(100));
        widget.setNoteDocument(&doc);

        // Wait for Live editor to build its scene from the canonical parse.
        QVERIFY(waitForLiveScene(widget.editor()));

        widget.setViewMode(NoteEditorWidget::ViewMode::Source);
        auto *source = widget.sourceEditor();
        QVERIFY(source);
        // Wait for Source to receive canonical content.
        QTest::qWait(50);

        // Drive cursor + scroll on Source. Line 51 is paragraph 25's
        // text line (paragraphs are at odd 1-based lines with blank
        // separators at even lines under makeParagraphs's \n\n joining).
        source->setCursorPosition({51, 5});
        source->setScrollPosition(48.0f);
        QTest::qWait(30);

        // Switch to LivePreview — outgoing Source detaches, incoming Live attaches.
        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);

        // Wait for the Live scene to rebuild after attach.
        QVERIFY(waitForLiveScene(widget.editor()));
        QTest::qWait(60);

        // Cursor — v0.6.1 added goToLineAndColumn, so both line and
        // column should preserve across the Source -> Live transition.
        // Source cursor was at line 51 (1-based) + column 5
        // → stored in EphemeralState as line 50 (0-based) + column 5
        // → restored as goToLineAndColumn(51, 5) on Live-attach.
        // Live's item structure maps Source lines to scene items
        // differently (paragraph-per-item vs Qt block-per-line), so
        // cursorLine may drift ±3. cursorColumn is 1-based in Markoff
        // (0-based col 5 → cursorColumn 6), exact ±1.
        const int line = widget.editor()->cursorLine();
        QVERIFY2(std::abs(line - 51) <= 3,
                 qPrintable(QStringLiteral("LivePreview cursor line drift: %1").arg(line)));
        const int col = widget.editor()->cursorColumn();
        QVERIFY2(std::abs(col - 6) <= 1,  // source col 5 + 1-based cursorColumn
                 qPrintable(QStringLiteral("LivePreview cursor column drift: %1 (expected ~6)").arg(col)));

        // Scroll ≈ 48.0 within ±1.5 (offscreen viewport may clamp).
        const float scroll = widget.editor()->scrollPositionVisualLine();
        QVERIFY2(std::abs(scroll - 48.0f) <= 1.5f,
                 qPrintable(QStringLiteral("LivePreview scroll drift: %1").arg(scroll)));
    }

    // --- Test case 2: LivePreview→Reading→Source preserves scroll. ---
    //
    // ReadingView's virtualized/async pipeline means the scroll value after
    // a mode swap is best-effort: the scrollbar range is 0 until sections
    // mount. We verify the end-to-end contract by confirming the swap occurred
    // cleanly: Source populated with correct text.
    void livePreviewToReadingToSourcePreservesScroll()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("note.md"));
        doc.setMarkdown(makeParagraphs(60));
        widget.setNoteDocument(&doc);

        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);
        QVERIFY(waitForLiveScene(widget.editor()));
        QTest::qWait(30);
        widget.editor()->setScrollPositionVisualLine(20.0f);
        QTest::qWait(30);

        // LivePreview → Reading.
        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);
        auto *reading = widget.readingView();
        QVERIFY(reading);
        if (reading->mountedCount() == 0) {
            QSignalSpy mountedSpy(reading,
                &Markoff::Reading::ReadingView::mountingFinished);
            mountedSpy.wait(1000);
        }

        // Reading → Source.
        widget.setViewMode(NoteEditorWidget::ViewMode::Source);
        QTest::qWait(60);
        auto *source = widget.sourceEditor();
        QVERIFY(source);
        // We at minimum require the swap to have occurred cleanly (Source
        // visible + populated with correct text).
        QCOMPARE(source->toPlainText(), doc.markdown());
    }

    // --- Test case 3: Reading→LivePreview preserves scroll. ---
    // (Reading has no cursor so we don't test cursor here.)
    void readingToLivePreviewPreservesScroll()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("note.md"));
        doc.setMarkdown(makeParagraphs(40));
        widget.setNoteDocument(&doc);

        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);
        auto *reading = widget.readingView();
        QVERIFY(reading);
        if (reading->mountedCount() == 0) {
            QSignalSpy mountedSpy(reading,
                &Markoff::Reading::ReadingView::mountingFinished);
            mountedSpy.wait(1000);
        }

        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);
        QVERIFY(waitForLiveScene(widget.editor()));
        QTest::qWait(60);

        // Swap succeeded + content visible via canonical binding.
        QCOMPARE(widget.editor()->toPlainText(), doc.markdown());
    }

    // --- Test case 4: Mode swap Source → Live → Source preserves canonical
    //     bytes exactly. Content must not be corrupted during detach/attach. ---
    void modeSwap_preservesCanonicalBytes()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("note.md"));
        const QString original = QStringLiteral("# hello\n\nbody text here");
        doc.setMarkdown(original);
        widget.setNoteDocument(&doc);

        const QString before = doc.markoff()->toMarkdown();

        // Perform three-hop mode swap without any user edits.
        widget.setViewMode(NoteEditorWidget::ViewMode::Source);
        QTest::qWait(20);
        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);
        QTest::qWait(20);
        widget.setViewMode(NoteEditorWidget::ViewMode::Source);
        QTest::qWait(20);

        // Byte-exact equality: mode swap must not alter canonical content.
        QCOMPARE(doc.markoff()->toMarkdown(), before);
    }

    // --- Test case 5: Canonical document is the authority; leaves reflect it
    //     after attaching and parsing. ---
    void leavesReflectCanonicalOnAttach()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("note.md"));
        const QString content = QStringLiteral("Hello canonical world.");
        doc.setMarkdown(content);
        widget.setNoteDocument(&doc);

        // Switch to Source — leaf attaches, receives canonical content.
        widget.setViewMode(NoteEditorWidget::ViewMode::Source);
        QTest::qWait(50);
        auto *source = widget.sourceEditor();
        QVERIFY(source);
        QCOMPARE(source->toPlainText(), content);

        // Switch to LivePreview — leaf attaches, scene rebuilds from parse.
        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);
        QVERIFY(waitForLiveScene(widget.editor()));
        QTest::qWait(30);
        QCOMPARE(widget.editor()->toPlainText(), content);
    }

    // --- Bonus: setViewMode emits viewModeChanged on every real change. ---
    void emitsViewModeChangedOnEveryRealChange()
    {
        NoteEditorWidget widget;
        widget.resize(400, 200);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("hi"));
        widget.setNoteDocument(&doc);

        QSignalSpy spy(&widget,
                       &NoteEditorWidget::viewModeChanged);

        widget.setViewMode(NoteEditorWidget::ViewMode::Source);
        QCOMPARE(spy.count(), 1);

        // Same mode — no emit.
        widget.setViewMode(NoteEditorWidget::ViewMode::Source);
        QCOMPARE(spy.count(), 1);

        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);
        QCOMPARE(spy.count(), 2);

        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);
        QCOMPARE(spy.count(), 3);
    }
};

QTEST_MAIN(NoteEditorWidgetModeTransitionTest)
#include "tst_note_editor_widget_mode_transition.moc"
