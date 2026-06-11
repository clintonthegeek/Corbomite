// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 2 (road-to-dogfood) — template-at-cursor.
//
// NoteEditorWidget::insertAtCursor inserts text at the active leaf's caret as
// one undo-integrated D2 edit, replacing the old "append at end-of-document"
// behaviour that template insertion used. The caret-marker form positions the
// caret after the insert. Source mode is used for deterministic caret reads.
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkdownView.h>

#include <QObject>
#include <QTest>

using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;

namespace {
void attachInSource(NoteEditorWidget &widget, NoteDocument &doc)
{
    widget.resize(600, 240);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    widget.setViewMode(NoteEditorWidget::ViewMode::Source);
    widget.setNoteDocument(&doc);
    QTRY_VERIFY(widget.activeLeaf() != nullptr);
    QTRY_COMPARE(widget.activeLeaf()->document(), doc.markoff());
}
} // namespace

class NoteEditorWidgetInsertAtCursorTest : public QObject {
    Q_OBJECT

private Q_SLOTS:

    void insertsAtCaretNotEnd()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("Alpha\n\nGamma"));
        attachInSource(widget, doc);

        // Caret in the middle of "Alpha" (after "Al").
        widget.activeLeaf()->setCursorPosition({1, 3});
        QTRY_COMPARE(widget.activeLeaf()->cursorPosition().column, 3);

        QVERIFY(widget.insertAtCursor(QStringLiteral("INS")));

        // Lands mid-block, NOT appended at the end.
        QTRY_COMPARE(doc.markdown().trimmed(),
                     QStringLiteral("AlINSpha\n\nGamma"));
    }

    void insertsIntoEmptyDocument()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        attachInSource(widget, doc);

        QVERIFY(widget.insertAtCursor(QStringLiteral("Hello world")));
        QTRY_COMPARE(doc.markdown().trimmed(), QStringLiteral("Hello world"));
    }

    void caretMarkerStrippedFromContent()
    {
        // The marker must never survive into the document. (The caret column
        // it produces is pure LineResolve::caretAfterFlatInsert arithmetic,
        // unit-tested in tst_line_resolve; applying it to a live leaf is
        // subject to the same attach-window cursor timing the rest of the
        // codebase handles, so it is not asserted here.)
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        attachInSource(widget, doc);

        QVERIFY(widget.insertAtCursor(QStringLiteral("before<CUR>after"),
                                      QStringLiteral("<CUR>")));
        QTRY_COMPARE(doc.markdown().trimmed(), QStringLiteral("beforeafter"));
    }

    void insertsMultiBlockMidBlock()
    {
        // A structural (newline-bearing) insert in the middle of a block
        // splits it into multiple blocks at the caret.
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("Hello"));
        attachInSource(widget, doc);

        // Caret inside "Hello", after "Hel" (col 4).
        widget.activeLeaf()->setCursorPosition({1, 4});
        QTRY_COMPARE(widget.activeLeaf()->cursorPosition().column, 4);

        QVERIFY(widget.insertAtCursor(QStringLiteral("X\n\nY")));
        QTRY_COMPARE(doc.markdown().trimmed(), QStringLiteral("HelX\n\nYlo"));
    }
};

QTEST_MAIN(NoteEditorWidgetInsertAtCursorTest)
#include "tst_note_editor_widget_insert_at_cursor.moc"
