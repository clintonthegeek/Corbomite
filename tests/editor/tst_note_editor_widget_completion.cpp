// SPDX-License-Identifier: GPL-3.0-or-later
// Integration: CompletionController against the canvas LivePreview leaf —
// real caretRect, real document propagation. Keyboard-level typing is
// covered upstream by markoff's harness; not re-tested here.
#include "NoteEditorWidget.h"
#include "CompletionController.h"
#include "CompletionPopup.h"
#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/canvas/EditorWidget.h>
#include <markoff/core/MarkdownView.h>
#include <markoff/core/MarkoffDocument.h>

#include <QApplication>
#include <QObject>
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
    void canvasLeaf_triggerInsertUndo_endToEnd()
    {
        EditorSuggestManager manager;
        AtSuggest suggest;
        manager.registerSuggest(&suggest);

        NoteEditorWidget widget;
        widget.setEditorSuggestManager(&manager);
        widget.resize(700, 400);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        widget.activateWindow();
        QApplication::setActiveWindow(&widget);
        auto *canvas = widget.canvasEditor();
        QVERIFY(canvas);
        canvas->setFocus(Qt::OtherFocusReason);

        NoteDocument doc(QStringLiteral("/tmp/v"), QStringLiteral("n.md"));
        doc.setMarkdown(QStringLiteral("hello @z\n\nsecond paragraph"));
        widget.setNoteDocument(&doc);

        auto *leaf = widget.activeLeaf();
        QVERIFY(leaf);

        bool caretValid = false;
        for (int i = 0; i < 60 && !caretValid; ++i) {
            canvas->setFocus(Qt::OtherFocusReason);
            leaf->setCursorPosition({1, 9});
            QTest::qWait(30);
            caretValid = leaf->caretRect().isValid();
        }
        QVERIFY2(caretValid, "canvas caretRect never became valid");

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
