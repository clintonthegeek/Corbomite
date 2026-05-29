// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reading mode is a read-only Markoff::Styled::Editor leaf (no QML).
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"

#include <markoff/styled/Editor.h>
#include <markoff/core/MarkdownView.h>
#include <markoff/core/MarkoffDocument.h>

#include "corbomite/core/NoteDocument.h"

#include <QSignalSpy>
#include <QTest>
#include <QTextEdit>

using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;

class ReadingStyledLeafTest : public QObject {
    Q_OBJECT
private Q_SLOTS:

    void readingMode_constructsReadOnlyStyledLeaf()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("# Title\n\nbody text here"));
        widget.setNoteDocument(&doc);

        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);

        auto *leaf = widget.activeLeaf();
        QVERIFY(leaf);
        auto *styled = qobject_cast<Markoff::Styled::Editor *>(leaf);
        QVERIFY2(styled, "Reading mode leaf must be a Markoff::Styled::Editor");
        QVERIFY2(styled->isReadOnly(), "Reading leaf must be read-only");
    }

    void readingLeaf_reflectsCanonicalContent()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("# Title\n\nbody text here"));
        widget.setNoteDocument(&doc);

        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);
        QTest::qWait(50);  // styled applies formats on d2DocumentChanged

        auto *styled =
            qobject_cast<Markoff::Styled::Editor *>(widget.activeLeaf());
        QVERIFY(styled);
        // Styled keeps delimiters visible, so plain text contains the body.
        QVERIFY2(styled->textEdit()->toPlainText().contains(
                     QStringLiteral("body text here")),
                 "Reading leaf must show canonical content");
    }

    void switchingAwayFromReading_stillEmitsAndSwaps()
    {
        NoteEditorWidget widget;
        widget.resize(400, 200);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("hi"));
        widget.setNoteDocument(&doc);

        QSignalSpy spy(&widget, &NoteEditorWidget::viewModeChanged);
        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);
        QCOMPARE(spy.count(), 1);
        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);
        QCOMPARE(spy.count(), 2);
        QVERIFY(qobject_cast<Markoff::Styled::Editor *>(widget.activeLeaf())
                == nullptr);  // back on the Live leaf
    }
};

QTEST_MAIN(ReadingStyledLeafTest)
#include "tst_reading_styled_leaf.moc"
