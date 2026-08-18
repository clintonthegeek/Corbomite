// SPDX-License-Identifier: GPL-3.0-or-later
//
// Regression: a NoteDocument freed while a live NoteEditorWidget is attached
// must not crash. Originally reproduced the 2026-06-10 first-run SIGSEGV
// against the QML Live leaf; retargeted to the canvas LivePreview leaf after
// Cluster K Phase 5 (QML retirement). The contract is leaf-agnostic:
// MarkdownView must retire its document pointer on destroy so later event
// processing cannot touch freed memory.
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkdownView.h>

#include <QObject>
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

class NoteEditorWidgetDocLifetimeTest : public QObject {
    Q_OBJECT

private Q_SLOTS:

    void documentFreedUnderLivePreviewLeaf_doesNotCrash()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);

        auto *doc = new NoteDocument(QStringLiteral("/tmp/vault"),
                                     QStringLiteral("note.md"));
        doc->setMarkdown(makeParagraphs(40));
        widget.setNoteDocument(doc);

        auto *leaf = widget.activeLeaf();
        QVERIFY(leaf);
        QTRY_COMPARE(leaf->document(), doc->markoff());

        delete doc;

        QCOMPARE(leaf->document(), nullptr);

        QTest::qWait(60);
        QVERIFY(true);
    }
};

QTEST_MAIN(NoteEditorWidgetDocLifetimeTest)
#include "tst_note_editor_widget_doc_lifetime.moc"
