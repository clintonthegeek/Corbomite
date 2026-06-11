// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 2 (road-to-dogfood) — status-bar word count honesty.
//
// NoteEditorWidget caches the active document's word count and ships it on
// the cursorInfoChanged(line, column, wordCount) signal that MainWindow's
// status bar consumes. Before this wiring m_cachedWordCount was initialised
// to 0 and never updated, so the status bar permanently read "Words: 0".
//
// This test pins the contract: attaching a document seeds the count, and
// every subsequent edit (NoteDocument::textChanged) re-emits a fresh count
// matching NoteDocument::wordCount(). Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"
#include "corbomite/core/NoteDocument.h"

#include <QObject>
#include <QSignalSpy>
#include <QTest>

using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;

class NoteEditorWidgetWordCountTest : public QObject {
    Q_OBJECT

private Q_SLOTS:

    void attachSeedsWordCount()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("alpha beta gamma delta"));
        QCOMPARE(doc.wordCount(), 4);

        QSignalSpy spy(&widget, &NoteEditorWidget::cursorInfoChanged);
        widget.setNoteDocument(&doc);

        // Attaching the document seeds the status-bar count immediately,
        // without waiting for a cursor move.
        QTRY_VERIFY(!spy.isEmpty());
        QCOMPARE(spy.last().at(2).toInt(), 4);
    }

    void editUpdatesWordCount()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("one two three"));
        widget.setNoteDocument(&doc);

        QSignalSpy spy(&widget, &NoteEditorWidget::cursorInfoChanged);

        // A text change must drive a fresh count onto the status bar even if
        // the caret never moves.
        doc.setMarkdown(QStringLiteral("uno dos tres cuatro cinco"));

        QTRY_VERIFY(!spy.isEmpty());
        QCOMPARE(spy.last().at(2).toInt(), 5);
    }
};

QTEST_MAIN(NoteEditorWidgetWordCountTest)
#include "tst_note_editor_widget_word_count.moc"
