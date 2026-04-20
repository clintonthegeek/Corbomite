// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster V Task 2.8 — introspect MainWindow's KActionCollection and
// assert that every action wired in Phase 2+3 (Cluster V §3.4 menu tree)
// is present with a valid QAction. Guards against rc/action-id drift
// when the corbomiteui.rc.in menu tree is edited.
#include <QtTest/QtTest>
#include <QStandardPaths>

#include <KActionCollection>

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"

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
