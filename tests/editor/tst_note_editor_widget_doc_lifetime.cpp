// SPDX-License-Identifier: GPL-3.0-or-later
//
// Regression: a NoteDocument freed while a live NoteEditorWidget is attached
// must not crash. Repro of the 2026-06-10 first-run SIGSEGV: opening a recent
// vault double-fired vaultRequested (WelcomeScreen wired both itemActivated
// AND itemDoubleClicked), so a second onVaultOpened tore the vault down
// (Vault::unload → teardownTree → qDeleteAll(m_docs)) while the first restore's
// live editor was still attached. The deferred QML initial-focus seed
// (LiveView.qml onCountChanged → requestTextCaretAtRow(0,0)) then dereferenced
// the freed MarkoffDocument.
//
// The trigger is fixed in WelcomeScreen (single activation signal); the
// robustness root cause is fixed in markoff-live (LiveListModelBinding +
// EditorWidget retire-on-destroy). This test pins the robustness fix at the
// Corbomite integration level: deleting the owning NoteDocument out from under
// a shown live editor leaves the leaf cleanly detached and survives subsequent
// event processing. Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkdownView.h>
#include <markoff/live/EditorWidget.h>
#include <markoff/live/LiveListModelBinding.h>

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

    void documentFreedUnderLiveLeaf_doesNotCrash()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);

        // Heap-owned so we can free it out from under the live editor exactly
        // as Vault teardown does.
        auto *doc = new NoteDocument(QStringLiteral("/tmp/vault"),
                                     QStringLiteral("note.md"));
        doc->setMarkdown(makeParagraphs(40));
        widget.setNoteDocument(doc);

        // Let the live leaf attach and the QML scene populate (the seed path).
        auto *editor = widget.editor();
        QVERIFY(editor);
        QTRY_COMPARE(editor->binding()->document(), doc->markoff());

        // The document is destroyed while the live editor is still attached.
        delete doc;

        // Retire-on-destroy: the live binding and the base both dropped the
        // dangling pointer synchronously.
        QCOMPARE(editor->binding()->document(), nullptr);
        QCOMPARE(editor->document(), nullptr);

        // Drain the event loop so any pending QML polish / initial-focus seed
        // fires. Before the fix this reached flushPendingD2Changed on freed
        // memory → SIGSEGV.
        QTest::qWait(60);

        // Base accessors stay safe after the document is gone.
        QCOMPARE(editor->cursorPosition().line, 1);
        QCOMPARE(editor->binding()->document(), nullptr);
    }
};

QTEST_MAIN(NoteEditorWidgetDocLifetimeTest)
#include "tst_note_editor_widget_doc_lifetime.moc"
