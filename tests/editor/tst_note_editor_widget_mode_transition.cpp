// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 1 (contract v2) — NoteEditorWidget signal-driven mode-transition
// glue, rewritten against the MarkdownView base contract. Covers:
//   - cursor movement on the active leaf surfacing as cursorInfoChanged,
//   - canonical bytes preserved byte-exact across detach/attach swaps,
//   - leaves reflecting canonical content on attach,
//   - viewModeChanged emitted on every real change.
// The pre-port cursor/scroll-preservation slots (which drove the retired
// Markoff::Editor / ReadingView / setScrollPosition APIs) are dropped here;
// cursor/scroll round-trip across modes is covered by
// tst_note_editor_widget_ephemeral under contract v2.
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"

#include <markoff/source/Editor.h>
#include <markoff/core/MarkdownView.h>
#include <markoff/core/MarkoffDocument.h>

#include "corbomite/core/NoteDocument.h"

#include <QObject>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QStringList>
#include <QTest>

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

class NoteEditorWidgetModeTransitionTest : public QObject {
    Q_OBJECT

private Q_SLOTS:

    // Phase 1 (contract v2): leaf cursor movement must surface as
    // cursorInfoChanged(line, column, wordCount) for the statusbar.
    void cursorMovesEmitCursorInfo()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("alpha\n\nbravo\n\ncharlie"));
        widget.setNoteDocument(&doc);
        widget.setViewMode(NoteEditorWidget::ViewMode::Source);

        QSignalSpy spy(&widget, &NoteEditorWidget::cursorInfoChanged);
        widget.activeLeaf()->setCursorPosition({3, 1});
        QTest::qWait(20);

        QVERIFY2(!spy.isEmpty(), "no cursorInfoChanged after cursor move");
        const auto args = spy.last();
        QCOMPARE(args.at(0).toInt(), 3);   // 1-based flat visual line
    }

    // Mode swap Source → Live → Source must not corrupt canonical bytes.
    void modeSwapPreservesCanonicalBytes()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("# hello\n\nbody text here"));
        widget.setNoteDocument(&doc);

        const QByteArray before = doc.markoff()->serializeForSave();

        widget.setViewMode(NoteEditorWidget::ViewMode::Source);
        QTest::qWait(20);
        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);
        QTest::qWait(20);
        widget.setViewMode(NoteEditorWidget::ViewMode::Source);
        QTest::qWait(20);

        QCOMPARE(doc.markoff()->serializeForSave(), before);
    }

    // Canonical document is the authority; the Source leaf reflects it on
    // attach (flat-view text == canonical markdown for plain paragraphs).
    void leavesReflectCanonicalOnAttach()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        const QString content = QStringLiteral("Hello canonical world.");
        doc.setMarkdown(content);
        widget.setNoteDocument(&doc);

        widget.setViewMode(NoteEditorWidget::ViewMode::Source);
        QTest::qWait(50);
        auto *source = widget.sourceEditor();
        QVERIFY(source);
        QCOMPARE(source->plainTextEdit()->toPlainText(), content);
    }

    // setViewMode emits viewModeChanged on every real change, never on a
    // no-op re-selection of the current mode.
    void emitsViewModeChangedOnEveryRealChange()
    {
        NoteEditorWidget widget;
        widget.resize(400, 200);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("hi"));
        widget.setNoteDocument(&doc);

        QSignalSpy spy(&widget, &NoteEditorWidget::viewModeChanged);

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
