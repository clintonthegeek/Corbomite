// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster V Task 2.8 — introspect MainWindow's KActionCollection and
// assert that every action wired in Phase 2+3 (Cluster V §3.4 menu tree)
// is present with a valid QAction. Guards against rc/action-id drift
// when the corbomiteui.rc.in menu tree is edited.
#include <QtTest/QtTest>
#include <QStandardPaths>

#include <KActionCollection>

#include <corbomite/core/Command.h>
#include <corbomite/core/Workspace.h>

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"

#include <markoff/EditorContext.h>

using Corbomite::CorbomiteApp;
using Corbomite::MainWindow;

class TstMainWindowActionWiring : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void everyExpectedActionExists()
    {
        CorbomiteApp app;
        MainWindow w(&app);

        auto *ac = w.actionCollection();
        QVERIFY(ac != nullptr);

        const QStringList expected = {
            // Edit
            QStringLiteral("edit_find"),
            QStringLiteral("edit_find_next"),
            QStringLiteral("edit_find_previous"),
            QStringLiteral("edit_replace"),

            // View (zoom + modes + fold)
            QStringLiteral("view_zoom_in"),
            QStringLiteral("view_zoom_out"),
            QStringLiteral("view_zoom_reset"),
            QStringLiteral("editor_toggle_mode"),
            QStringLiteral("view_source_mode"),
            QStringLiteral("view_editing_mode"),
            QStringLiteral("view_reading_mode"),
            QStringLiteral("fold_all"),
            QStringLiteral("unfold_all"),
            QStringLiteral("toggle_fold"),

            // Format
            QStringLiteral("format_bold"),
            QStringLiteral("format_italic"),
            QStringLiteral("format_strikethrough"),
            QStringLiteral("format_inline_code"),
            QStringLiteral("insert_link"),
            QStringLiteral("insert_wiki_link"),
            QStringLiteral("insert_image"),
            QStringLiteral("insert_code_block"),
            QStringLiteral("insert_block_quote"),
            QStringLiteral("insert_horizontal_rule"),

            // Heading
            QStringLiteral("heading_1"),
            QStringLiteral("heading_2"),
            QStringLiteral("heading_3"),
            QStringLiteral("heading_4"),
            QStringLiteral("heading_5"),
            QStringLiteral("heading_6"),
            QStringLiteral("heading_increase"),
            QStringLiteral("heading_decrease"),

            // Insert
            QStringLiteral("insert_table"),
            QStringLiteral("insert_callout"),
            QStringLiteral("toggle_checkbox"),

            // Table
            QStringLiteral("table_row_above"),
            QStringLiteral("table_row_below"),
            QStringLiteral("table_col_left"),
            QStringLiteral("table_col_right"),
            QStringLiteral("table_delete_row"),
            QStringLiteral("table_delete_col"),

            // Workspace
            QStringLiteral("split_right"),
            QStringLiteral("split_down"),
            QStringLiteral("tab_undo_close"),
        };
        for (const auto &id : expected)
            QVERIFY2(ac->action(id) != nullptr, qPrintable(id));
    }

    void headingGroupIsCheckableRadio()
    {
        CorbomiteApp app;
        MainWindow w(&app);
        auto *ac = w.actionCollection();

        for (int level = 1; level <= 6; ++level) {
            auto *act = ac->action(QStringLiteral("heading_%1").arg(level));
            QVERIFY(act);
            QVERIFY2(act->isCheckable(),
                     qPrintable(QStringLiteral("heading_%1 not checkable").arg(level)));
        }
    }

    void editorModeGroupIsCheckableRadio()
    {
        CorbomiteApp app;
        MainWindow w(&app);
        auto *ac = w.actionCollection();

        for (const QString &id : {QStringLiteral("view_source_mode"),
                                   QStringLiteral("view_editing_mode"),
                                   QStringLiteral("view_reading_mode")}) {
            auto *act = ac->action(id);
            QVERIFY(act);
            QVERIFY2(act->isCheckable(), qPrintable(id));
        }
    }

    void onEditorContextChangedUpdatesFormatToolbar()
    {
        // Phase C6 — verifies the new MainWindow::onEditorContextChanged
        // slot drives Format toolbar check state from an EditorContext
        // snapshot. We invoke the slot directly rather than drive it via
        // a real Markoff::Editor — that surface is exercised by the
        // live-preview e2e path; here we only need the adapter wiring.
        CorbomiteApp app;
        MainWindow w(&app);
        auto *ac = w.actionCollection();

        auto *boldAct = ac->action(QStringLiteral("format_bold"));
        auto *italicAct = ac->action(QStringLiteral("format_italic"));
        auto *strikeAct = ac->action(QStringLiteral("format_strikethrough"));
        auto *codeAct = ac->action(QStringLiteral("format_inline_code"));
        QVERIFY(boldAct && italicAct && strikeAct && codeAct);

        // Baseline: no snapshot has fired, so these start unchecked.
        QVERIFY(!boldAct->isChecked());
        QVERIFY(!italicAct->isChecked());

        // Synthesise a cursor-inside-bold-italic snapshot.
        Markoff::EditorContext ctx;
        ctx.inBold = true;
        ctx.inItalic = true;
        w.onEditorContextChanged(ctx);

        QVERIFY(boldAct->isChecked());
        QVERIFY(italicAct->isChecked());
        QVERIFY(!strikeAct->isChecked());
        QVERIFY(!codeAct->isChecked());

        // Move out of the run — check state should clear.
        Markoff::EditorContext plain;
        w.onEditorContextChanged(plain);
        QVERIFY(!boldAct->isChecked());
        QVERIFY(!italicAct->isChecked());

        // Heading snapshot flips the matching H-N radio item.
        Markoff::EditorContext hctx;
        hctx.blockKind = Markoff::EditorContext::BlockKind::Heading;
        hctx.headingLevel = 3;
        w.onEditorContextChanged(hctx);
        auto *h3 = ac->action(QStringLiteral("heading_3"));
        QVERIFY(h3 && h3->isChecked());
        for (int i = 1; i <= 6; ++i) {
            if (i == 3) continue;
            auto *hi = ac->action(QStringLiteral("heading_%1").arg(i));
            QVERIFY(hi && !hi->isChecked());
        }
        // toggle_fold gates on blockKind == Heading.
        auto *toggleFold = ac->action(QStringLiteral("toggle_fold"));
        QVERIFY(toggleFold && toggleFold->isEnabled());
    }

    // Bug #1 (filed 2026-04-24, commit 9fb2fe47) — per-view hamburger
    // menu "Split right" / "Split down" entries dispatch via the
    // CommandRegistry, but split_right / split_down were only wired
    // as KActionCollection actions in setupActions(), so executeById
    // returned false silently. Fix registers commands that delegate
    // to the KAction's trigger() (preserving the vault-open gate).
    void hamburgerSplitDispatchesCreateNewLeaf()
    {
        CorbomiteApp app;
        MainWindow w(&app);
        QVERIFY(app.openVault(
            QStringLiteral("/home/clinton/dev/Corbomite/testvaults/DevVault")));
        QTest::qWait(100);

        auto *cmds = w.commandRegistry();
        QVERIFY(cmds);
        QVERIFY2(cmds->findCommand(QStringLiteral("split_right")),
                 "split_right must be registered so hamburger dispatch works");
        QVERIFY2(cmds->findCommand(QStringLiteral("split_down")),
                 "split_down must be registered so hamburger dispatch works");

        auto *ws = w.findChild<Corbomite::Workspace*>();
        QVERIFY(ws);

        const int before = ws->allLeaves().size();
        QVERIFY(cmds->executeById(QStringLiteral("split_right")));
        QTest::qWait(50);
        QCOMPARE(ws->allLeaves().size(), before + 1);

        QVERIFY(cmds->executeById(QStringLiteral("split_down")));
        QTest::qWait(50);
        QCOMPARE(ws->allLeaves().size(), before + 2);
    }

    void editorActionsAreDisabledWithoutActiveMarkdownView()
    {
        // With no vault open, active leaf has no MarkdownView. Editor
        // actions should all start disabled so the menubar doesn't
        // advertise unreachable commands.
        CorbomiteApp app;
        MainWindow w(&app);
        auto *ac = w.actionCollection();

        for (const QString &id : {QStringLiteral("format_bold"),
                                   QStringLiteral("heading_1"),
                                   QStringLiteral("insert_table"),
                                   QStringLiteral("fold_all")}) {
            auto *act = ac->action(id);
            QVERIFY(act);
            QVERIFY2(!act->isEnabled(),
                     qPrintable(QStringLiteral("%1 should be disabled with no markdown view").arg(id)));
        }
    }
};

QTEST_MAIN(TstMainWindowActionWiring)
#include "tst_mainwindow_action_wiring.moc"
