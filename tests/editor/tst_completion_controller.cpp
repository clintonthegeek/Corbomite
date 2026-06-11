// SPDX-License-Identifier: GPL-3.0-or-later
// CompletionController — spec §6. Headless: a FakeLeaf MarkdownView
// supplies caretRect/cursorPosition; a real NoteDocument supplies the
// text + edit path; a stub '@' suggester isolates controller logic.
#include "CompletionController.h"
#include "CompletionPopup.h"
#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkdownView.h>
#include <markoff/core/MarkoffDocument.h>

#include <QObject>
#include <QTest>

using namespace Corbomite;

namespace {

class FakeLeaf : public Markoff::MarkdownView {
public:
    QRect caretRect() const override { return m_caret; }
    bool hasCursor()  const override { return true; }
    bool hasEditing() const override { return !isReadOnly(); }
    Markoff::CursorPos cursorPosition() const override { return m_pos; }
    void setCursorPosition(Markoff::CursorPos p) override
    {
        m_pos = p;
        Q_EMIT cursorPositionChanged(p.line, p.column);
    }
    QRect m_caret{20, 20, 2, 14};
    Markoff::CursorPos m_pos{1, 1};
};

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
        set.items.append({QStringLiteral("apple"),  QStringLiteral("apple!"),  {}});
        set.items.append({QStringLiteral("banana"), QStringLiteral("banana!"), {}});
        return set;
    }
};

struct Rig {
    EditorSuggestManager manager;
    AtSuggest suggest;
    FakeLeaf leaf;
    std::unique_ptr<NoteDocument> doc;
    CompletionController ctl;

    explicit Rig(const QString &markdown)
    {
        manager.registerSuggest(&suggest);
        doc = std::make_unique<NoteDocument>(QStringLiteral("/tmp/v"), QStringLiteral("n.md"));
        doc->setMarkdown(markdown);
        leaf.setDocument(doc->markoff());
        leaf.resize(400, 300);
        leaf.show();
        ctl.setManager(&manager);
        ctl.setLeaf(&leaf);
        ctl.setNoteDocument(doc.get());
    }

    void placeCursor(int line, int col) { leaf.setCursorPosition({line, col}); }
};

} // namespace

class CompletionControllerTest : public QObject {
    Q_OBJECT
private Q_SLOTS:

    void triggerShowsPopup_universeListed()
    {
        Rig rig(QStringLiteral("hello @ap"));
        rig.placeCursor(1, 10);
        QTRY_VERIFY(rig.ctl.isActive());
        QVERIFY(rig.ctl.popup());
        QCOMPARE(rig.ctl.popup()->visibleRowCount(), 1);   // fuzzy 'ap' → apple only
    }

    void noTrigger_dismisses()
    {
        Rig rig(QStringLiteral("hello @a"));
        rig.placeCursor(1, 9);
        QTRY_VERIFY(rig.ctl.isActive());
        rig.placeCursor(1, 3);
        QTRY_VERIFY(!rig.ctl.isActive());
    }

    void readOnly_neverTriggers()
    {
        Rig rig(QStringLiteral("hello @a"));
        rig.leaf.setReadOnly(true);
        rig.placeCursor(1, 9);
        QTest::qWait(30);
        QVERIFY(!rig.ctl.isActive());
    }

    void invalidCaretRect_suppresses()
    {
        Rig rig(QStringLiteral("hello @a"));
        rig.leaf.m_caret = QRect();
        rig.placeCursor(1, 9);
        QTest::qWait(30);
        QVERIFY(!rig.ctl.isActive());
    }

    void docDestroyed_whilePopupOpen_dismissesWithoutCrash()
    {
        Rig rig(QStringLiteral("hello @a"));
        rig.placeCursor(1, 9);
        QTRY_VERIFY(rig.ctl.isActive());
        rig.doc.reset();
        QTRY_VERIFY(!rig.ctl.isActive());
        rig.placeCursor(1, 2);
        QTest::qWait(20);
    }

    // ---- Task 10 behavior (accept path) — committed RED ----

    void accept_replacesRange_movesCaret_isOneUndoStep()
    {
        Rig rig(QStringLiteral("hello @ap"));
        rig.placeCursor(1, 10);
        QTRY_VERIFY(rig.ctl.isActive());
        rig.ctl.popup()->selectNext();
        QVERIFY(rig.ctl.popup()->acceptCurrent());
        QTRY_COMPARE(QString::fromUtf8(rig.doc->markoff()->serializeForSave()),
                     QStringLiteral("hello @apple!\n"));
        QCOMPARE(rig.leaf.m_pos.column, 14);
        QVERIFY(!rig.ctl.isActive());
        rig.doc->markoff()->undoD2();
        QTRY_COMPARE(QString::fromUtf8(rig.doc->markoff()->serializeForSave()),
                     QStringLiteral("hello @ap\n"));
    }

    void acceptWithStaleTrigger_abortsSilently()
    {
        Rig rig(QStringLiteral("hello @a"));
        rig.placeCursor(1, 9);
        QTRY_VERIFY(rig.ctl.isActive());
        auto *popup = rig.ctl.popup();
        popup->selectNext();
        rig.placeCursor(1, 3);
        QTRY_VERIFY(!rig.ctl.isActive());
        QCOMPARE(QString::fromUtf8(rig.doc->markoff()->serializeForSave()),
                 QStringLiteral("hello @a\n"));
        Q_UNUSED(popup)
    }
};

QTEST_MAIN(CompletionControllerTest)
#include "tst_completion_controller.moc"
