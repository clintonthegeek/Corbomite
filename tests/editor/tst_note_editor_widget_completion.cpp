// SPDX-License-Identifier: GPL-3.0-or-later
// Integration: CompletionController against the REAL Live leaf — real QML
// caretRect, real document propagation. Drives the document + base-contract
// cursor like tst_note_editor_widget_ephemeral (keyboard-level QML typing is
// covered upstream by markoff's harness; not re-tested here).
#include "NoteEditorWidget.h"
#include "CompletionController.h"
#include "CompletionPopup.h"
#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkdownView.h>
#include <markoff/core/MarkoffDocument.h>

#include <QApplication>
#include <QObject>
#include <QQuickWidget>
#include <QTest>

using namespace Corbomite;

namespace {
class AtSuggest : public EditorSuggest {
public:
    std::optional<EditorSuggestTriggerInfo> onTrigger(int cursorPos,
                                                        const QString &lineText,
                                                        NoteDocument *) override
    {
        int i = cursorPos - 1;
        while (i >= 0 && !lineText.at(i).isSpace()) {
            if (lineText.at(i) == QLatin1Char('@')) {
                EditorSuggestTriggerInfo info;
                info.start = i + 1;
                info.end = cursorPos;
                info.query = lineText.mid(info.start, info.end - info.start);
                return info;
            }
            --i;
        }
        return std::nullopt;
    }
    EditorSuggestionSet getSuggestions(const EditorSuggestTriggerInfo &ctx) override
    {
        EditorSuggestionSet set;
        set.filter = ctx.query;
        set.items.append({QStringLiteral("zebra"), QStringLiteral("zebra!"), {}});
        return set;
    }
};
} // namespace

class NoteEditorWidgetCompletionTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void liveLeaf_triggerInsertUndo_endToEnd()
    {
        EditorSuggestManager manager;
        AtSuggest suggest;
        manager.registerSuggest(&suggest);

        NoteEditorWidget widget;
        widget.setEditorSuggestManager(&manager);
        widget.resize(700, 400);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        // The live leaf's caretRect() is a read-only query over the focused
        // QML TextEdit (the window's activeFocusItem). That inner TextEdit only
        // becomes the activeFocusItem when the hosting QQuickWidget already has
        // keyboard focus at the moment the QML initial-focus seed runs (the
        // seed's takeFocus() -> forceActiveFocus() promotes the TextEdit, not
        // the delegate root). So focus the QQuickWidget BEFORE attaching the
        // document — exactly the ordering a real session produces.
        widget.activateWindow();
        QApplication::setActiveWindow(&widget);
        auto *quick = widget.findChild<QQuickWidget *>();
        QVERIFY(quick);
        quick->setFocus(Qt::OtherFocusReason);

        NoteDocument doc(QStringLiteral("/tmp/v"), QStringLiteral("n.md"));
        doc.setMarkdown(QStringLiteral("hello @z\n\nsecond paragraph"));
        widget.setNoteDocument(&doc);

        auto *leaf = widget.activeLeaf();
        QVERIFY(leaf);

        // Re-issue the cursor request until the QML seed has promoted the inner
        // TextEdit to activeFocusItem (focus propagation into the QQuickWidget
        // scene can lag the seed by a few event-loop turns under offscreen).
        // Each re-issue routes through requestTextCaretAtRow -> takeFocus ->
        // forceActiveFocus, so once focus has settled caretRect() goes valid.
        bool caretValid = false;
        for (int i = 0; i < 60 && !caretValid; ++i) {
            quick->setFocus(Qt::OtherFocusReason);
            leaf->setCursorPosition({1, 9});
            QTest::qWait(30);
            caretValid = leaf->caretRect().isValid();
        }
        QVERIFY2(caretValid, "live caretRect never became valid");

        QTRY_VERIFY(widget.findChild<CompletionPopup *>() != nullptr);
        auto *popup = widget.findChild<CompletionPopup *>();
        QTRY_COMPARE(popup->visibleRowCount(), 1);

        QTest::keyClick(QApplication::focusWidget() ? QApplication::focusWidget()
                                                    : static_cast<QWidget *>(&widget),
                        Qt::Key_Down);
        QTest::keyClick(QApplication::focusWidget() ? QApplication::focusWidget()
                                                    : static_cast<QWidget *>(&widget),
                        Qt::Key_Return);
        QTRY_VERIFY(QString::fromUtf8(doc.markoff()->serializeForSave())
                        .startsWith(QStringLiteral("hello @zebra!")));

        doc.markoff()->undoD2();
        QTRY_VERIFY(QString::fromUtf8(doc.markoff()->serializeForSave())
                        .startsWith(QStringLiteral("hello @z\n")));
    }
};

QTEST_MAIN(NoteEditorWidgetCompletionTest)
#include "tst_note_editor_widget_completion.moc"
