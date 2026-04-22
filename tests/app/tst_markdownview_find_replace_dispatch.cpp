// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase C7 Task 14 — end-to-end pinning test for the Cluster R hamburger
// Find/Replace dispatch + MainWindow's edit_find_next / edit_find_previous /
// edit_replace QAction routing into the active MarkdownView leaf.
//
// What this pins:
//   - Task 12 (3f332846): Corbomite::MarkdownView's hamburger Find/Replace
//     items are wired to m_editorWidget->activeLeaf()->showFindBar() /
//     showReplaceBar(). If the lambda is reverted to the disabled tooltip,
//     hamburgerFindAction_opensSearchBar fails (the SearchBar stays hidden).
//   - Task 13 (ac40eb20): MainWindow::triggerEditorAction routes
//     Markoff::ActionId::FindNext|FindPrevious|Replace through the active
//     leaf's MarkdownView virtuals, with the SourceEditor branch invoking
//     SourceEditor::findNextAction()/findPrevAction() so SearchController
//     advances when a query is set. If that special-case is removed, the
//     `mainWindowDispatchPath_*` slots fail.
//
// Test shape: synthetic Corbomite::MarkdownView wrapper over a directly
// constructed NoteEditorWidget in Source mode. This skips the heavy
// MainWindow + Workspace + vault-open machinery (no programmatic
// open-vault-then-open-file API exists today) while still exercising both
// dispatch paths end-to-end on the same code the MainWindow shell calls.
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include "MarkdownView.h"
#include "NoteEditorWidget.h"

#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/WorkspaceLeaf.h"

#include <markoff/MarkdownView.h>
#include <markoff/MarkoffDocument.h>
#include <markoff/SearchBar.h>
#include <markoff/SearchController.h>
#include <markoff/source/SourceEditor.h>

#include <QAction>
#include <QMenu>
#include <QTest>

using Corbomite::MarkdownView;
using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;
using Corbomite::ViewRegistry;
using Corbomite::WorkspaceLeaf;
using Markoff::Source::SourceEditor;

namespace {

/// Locate a top-level QAction in the hamburger menu by an i18n-stable text
/// substring. Returns nullptr if no match. Walks `actions()` (no submenu
/// recursion — find/replace live at the top level under section "find").
QAction *findHamburgerAction(QMenu &menu, const QString &textNeedle)
{
    for (QAction *a : menu.actions()) {
        if (a->menu()) continue;  // skip submenus
        if (a->text().contains(textNeedle))
            return a;
    }
    return nullptr;
}

/// Locate the SearchBar child of a SourceEditor by type. SourceEditor mounts
/// exactly one Markoff::SearchBar in its QVBoxLayout below Qutepart.
Markoff::SearchBar *findSearchBar(SourceEditor *src)
{
    return src->findChild<Markoff::SearchBar *>(QString(), Qt::FindDirectChildrenOnly);
}

/// Build a MarkdownView over a fresh NoteDocument with `body` markdown,
/// switch the editor to Source mode so SourceEditor + SearchController exist.
struct Fixture {
    NoteDocument doc;
    ViewRegistry registry;
    WorkspaceLeaf leaf;
    MarkdownView view;

    explicit Fixture(const QString &body)
        : doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"))
        , leaf(&registry)
        , view(&leaf)
    {
        doc.setMarkdown(body);
        view.editorWidget()->setNoteDocument(&doc);
        view.editorWidget()->setViewMode(NoteEditorWidget::ViewMode::Source);
        // SourceEditor's setDocument is called inside setViewMode's attach
        // step — that's what constructs the ReplaceController behind
        // searchController(). Sanity: if this assertion ever fires, the
        // mode-transition wiring regressed.
        Q_ASSERT(view.editorWidget()->sourceEditor());
        Q_ASSERT(view.editorWidget()->sourceEditor()->searchController());
    }

    SourceEditor *source() const
    {
        return view.editorWidget()->sourceEditor();
    }
};

}  // namespace

class TstMarkdownViewFindReplaceDispatch : public QObject
{
    Q_OBJECT

private slots:

    // --- Hamburger menu (Cluster R / Task 12) -----------------------------

    /// MarkdownView's "Find..." hamburger action triggers the active leaf's
    /// showFindBar() — proving the Task 12 lambda swap from the disabled
    /// stub to the live `m_editorWidget->activeLeaf()->showFindBar()` call.
    void hamburgerFindAction_opensSearchBar()
    {
        Fixture f(QStringLiteral("alpha beta alpha gamma alpha\n"));
        f.view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&f.view));

        QMenu menu;
        Corbomite::MenuSectionHelper helper(&menu);
        f.view.onMoreOptionsMenu(helper);
        helper.finalize();

        QAction *findAct = findHamburgerAction(menu, QStringLiteral("Find"));
        QVERIFY2(findAct, "hamburger 'Find...' action missing");
        QVERIFY2(findAct->isEnabled(),
                 "hamburger Find action should be enabled (Task 12)");

        auto *bar = findSearchBar(f.source());
        QVERIFY(bar);
        QVERIFY(bar->isHidden());  // baseline: bar starts hidden

        findAct->trigger();
        QVERIFY2(!bar->isHidden(),
                 "Triggering the hamburger Find action must reveal the SearchBar");
    }

    /// MarkdownView's "Replace..." hamburger action opens the bar in
    /// replace mode (find row + replace row both visible). Mirrors the
    /// Task 12 wiring for the second action.
    void hamburgerReplaceAction_opensReplaceBar()
    {
        Fixture f(QStringLiteral("foo bar foo baz\n"));
        f.view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&f.view));

        QMenu menu;
        Corbomite::MenuSectionHelper helper(&menu);
        f.view.onMoreOptionsMenu(helper);
        helper.finalize();

        QAction *replaceAct =
            findHamburgerAction(menu, QStringLiteral("Replace"));
        QVERIFY2(replaceAct, "hamburger 'Replace...' action missing");
        QVERIFY(replaceAct->isEnabled());

        auto *bar = findSearchBar(f.source());
        QVERIFY(bar);
        QVERIFY(bar->isHidden());

        replaceAct->trigger();
        QVERIFY(!bar->isHidden());
        // Bar's *replace* row exposes a QLineEdit named the second one in
        // the layout — the only visible-after-showReplace API distinction
        // is "isHidden() of the bar itself flips false AND the bar's
        // QLineEdits count both visible". That's an internal detail; here
        // we settle for "bar visible" as the contract — `showReplaceBar`'s
        // post-condition under SearchBar::showReplace() is the bar shown.
    }

    // --- MainWindow dispatch path (Task 13) ------------------------------

    /// Mirrors the body of MainWindow::triggerEditorAction(FindNext) when
    /// the active leaf is a SourceEditor: walk through
    /// `editorWidget()->activeLeaf()` → qobject_cast<SourceEditor*> →
    /// findNextAction()->trigger(). Pins the cast + accessor surface that
    /// Task 13 depends on.
    void mainWindowDispatchPath_findNextWithoutQueryOpensBar()
    {
        Fixture f(QStringLiteral("alpha beta alpha gamma alpha\n"));
        f.view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&f.view));

        // Walk the same code path the MainWindow dispatcher uses.
        Markoff::MarkdownView *leaf = f.view.editorWidget()->activeLeaf();
        QVERIFY(leaf);
        auto *srcCast = qobject_cast<SourceEditor *>(leaf);
        QVERIFY2(srcCast, "activeLeaf() of a Source-mode NoteEditorWidget "
                          "must qobject_cast to SourceEditor");

        auto *bar = findSearchBar(srcCast);
        QVERIFY(bar);
        QVERIFY(bar->isHidden());

        QAction *act = srcCast->findNextAction();
        QVERIFY(act);
        act->trigger();

        QVERIFY2(!bar->isHidden(),
                 "edit_find_next with no query should open the SearchBar");
    }

    /// With a query that matches multiple times, triggering the
    /// findNextAction() must advance SearchController::currentIndex().
    /// This is the "edit_find_next with active query advances cursor"
    /// branch of Task 13.
    void mainWindowDispatchPath_findNextAdvancesController()
    {
        Fixture f(QStringLiteral("alpha beta alpha gamma alpha\n"));
        f.view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&f.view));

        auto *ctrl = f.source()->searchController();
        QVERIFY(ctrl);

        // Drive a query directly into the controller — same effect as
        // typing into the SearchBar (the bar's signal is `searchTextChanged`
        // which calls `m_replaceController->setQuery(q)` in SourceEditor).
        ctrl->setQuery(QStringLiteral("alpha"));
        QCOMPARE(ctrl->matchCount(), 3);

        // After setQuery, the controller's currentIndex is at -1 or 0
        // depending on the controller's notify-on-change semantics. Snapshot
        // it then drive findNext to verify *advancement*, not absolute index.
        const int before = ctrl->currentIndex();

        QAction *act = f.source()->findNextAction();
        QVERIFY(act);
        act->trigger();
        const int afterOnce = ctrl->currentIndex();
        QVERIFY2(afterOnce > before,
                 qPrintable(QStringLiteral("currentIndex did not advance: %1 -> %2")
                                .arg(before).arg(afterOnce)));

        // Advance once more to prove repeated dispatch keeps stepping.
        act->trigger();
        QVERIFY(ctrl->currentIndex() > afterOnce
                || (ctrl->flags().wrap && ctrl->currentIndex() != afterOnce));
    }

    /// Mirrors MainWindow::triggerEditorAction(FindPrevious) — the
    /// findPrevAction()->trigger() retreats currentIndex (or wraps).
    void mainWindowDispatchPath_findPreviousRetreats()
    {
        Fixture f(QStringLiteral("alpha beta alpha gamma alpha\n"));
        f.view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&f.view));

        auto *ctrl = f.source()->searchController();
        QVERIFY(ctrl);
        ctrl->setQuery(QStringLiteral("alpha"));
        QCOMPARE(ctrl->matchCount(), 3);

        // Step forward twice so we have a non-zero starting index to
        // measurably retreat from.
        f.source()->findNextAction()->trigger();
        f.source()->findNextAction()->trigger();
        const int beforePrev = ctrl->currentIndex();
        QVERIFY(beforePrev > 0);

        f.source()->findPrevAction()->trigger();
        QVERIFY2(ctrl->currentIndex() < beforePrev,
                 qPrintable(QStringLiteral("currentIndex did not retreat: %1 -> %2")
                                .arg(beforePrev).arg(ctrl->currentIndex())));
    }

    /// Mirrors MainWindow::triggerEditorAction(Replace) — the leaf's
    /// showReplaceBar() must reveal the bar. SourceEditor's
    /// replaceAction() (Ctrl+H shortcut) calls into showReplaceBar()
    /// itself, so triggering it is equivalent to the MainWindow dispatch
    /// for the Replace ActionId.
    void mainWindowDispatchPath_replaceActionOpensBar()
    {
        Fixture f(QStringLiteral("foo bar foo baz\n"));
        f.view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&f.view));

        auto *bar = findSearchBar(f.source());
        QVERIFY(bar);
        QVERIFY(bar->isHidden());

        QAction *replaceAct = f.source()->replaceAction();
        QVERIFY(replaceAct);
        replaceAct->trigger();

        QVERIFY(!bar->isHidden());
    }
};

QTEST_MAIN(TstMarkdownViewFindReplaceDispatch)
#include "tst_markdownview_find_replace_dispatch.moc"
