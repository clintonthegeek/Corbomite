// SPDX-License-Identifier: GPL-3.0-or-later
// CompletionController — spec §6. Headless: a FakeLeaf MarkdownView
// supplies caretRect/cursorPosition; a real NoteDocument supplies the
// text + edit path; a stub '@' suggester isolates controller logic.
#include "CompletionController.h"
#include "CompletionPopup.h"
#include "CompletionDelegate.h"
#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkdownView.h>
#include <markoff/core/MarkoffDocument.h>

#include <QFontMetrics>
#include <QObject>
#include <QStandardItemModel>
#include <QTest>
#include <QWidget>

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

    // ---- Task 11 behavior (keyboard navigation via the scoped app filter) ----

    void keys_navigateAndAcceptViaAppFilter()
    {
        Rig rig(QStringLiteral("hello @"));
        rig.placeCursor(1, 8);                       // right after '@' (col = len+1)
        QTRY_VERIFY(rig.ctl.isActive());
        QCOMPARE(rig.ctl.popup()->visibleRowCount(), 2);   // empty query → both

        // Keys are sent to the LEAF (the focused editor in production);
        // the controller's app-level filter must intercept them.
        // CompletionPopup pre-highlights row 0 (apple) on show, so a single
        // Key_Down lands on row 1 (banana).
        QTest::keyClick(&rig.leaf, Qt::Key_Down);    // second row: banana
        QTest::keyClick(&rig.leaf, Qt::Key_Return);
        QTRY_COMPARE(QString::fromUtf8(rig.doc->markoff()->serializeForSave()),
                     QStringLiteral("hello @banana!\n"));
        QVERIFY(!rig.ctl.isActive());
    }

    void escape_dismissesWithoutEdit()
    {
        Rig rig(QStringLiteral("hello @"));
        rig.placeCursor(1, 8);
        QTRY_VERIFY(rig.ctl.isActive());
        QTest::keyClick(&rig.leaf, Qt::Key_Escape);
        QTRY_VERIFY(!rig.ctl.isActive());
        QCOMPARE(QString::fromUtf8(rig.doc->markoff()->serializeForSave()),
                 QStringLiteral("hello @\n"));
    }

    // Regression: the popup must size its height to the number of visible
    // rows. Before this was fixed it rendered one row tall regardless, so
    // multi-candidate sets (e.g. Beta vs sub/Beta) were invisible and
    // arrow-navigation moved an unseen selection.
    void popupHeight_growsWithRowCount()
    {
        Rig two(QStringLiteral("hello @"));      // empty query → apple + banana
        two.placeCursor(1, 8);
        QTRY_VERIFY(two.ctl.isActive());
        QCOMPARE(two.ctl.popup()->visibleRowCount(), 2);
        const int h2 = two.ctl.popup()->sizeHint().height();

        Rig one(QStringLiteral("hello @ap"));     // fuzzy 'ap' → apple only
        one.placeCursor(1, 10);
        QTRY_VERIFY(one.ctl.isActive());
        QCOMPARE(one.ctl.popup()->visibleRowCount(), 1);
        const int h1 = one.ctl.popup()->sizeHint().height();

        QVERIFY2(h2 > h1,
                 qPrintable(QStringLiteral("two-row popup (%1px) must be taller "
                                           "than one-row (%2px)").arg(h2).arg(h1)));
    }

    // Regression: the popup must widen to fit the dim detail column so a
    // path like a deeply-nested note isn't needlessly elided. Before this,
    // the detail was capped at half the row and the popup at a fixed width.
    void popupWidth_fitsDetailWithoutEliding()
    {
        QWidget host;
        host.resize(500, 400);
        QStandardItemModel model;
        auto *item = new QStandardItem(QStringLiteral("Beta"));
        const QString detail = QStringLiteral("deeply/nested/folder/Beta.md");
        item->setData(detail, Qt::UserRole + 1);
        item->setData(detail, Qt::UserRole + 2);
        model.appendRow(item);

        CompletionPopup popup(&model, &host);
        popup.setFilterText(QString());
        const int natural = CompletionDelegate::rowNaturalWidth(
            QFontMetrics(popup.font()), QStringLiteral("Beta"), detail);
        QVERIFY2(popup.sizeHint().width() >= natural,
                 qPrintable(QStringLiteral("popup width %1px must fit the row's "
                                           "natural width %2px")
                                .arg(popup.sizeHint().width()).arg(natural)));
    }
};

QTEST_MAIN(CompletionControllerTest)
#include "tst_completion_controller.moc"
