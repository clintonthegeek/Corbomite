// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster O Phase O1 — ActionContextController.
//
// This is the phase's acceptance gate (tst_action_context_no_silent_noop)
// plus the two other tests the plan names explicitly:
//   - O1.T2: inPlaceViewTypeChange_refreshesActionState
//   - O1.T4: saveAction_savesCanvas
//
// Driven through a real MainWindow + vault, same pattern as
// tst_mainwindow_link_navigation.cpp. Runs under QT_QPA_PLATFORM=offscreen.

#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTest>

#include <KAboutData>
#include <KActionCollection>
#include <KLocalizedString>

#include <QAction>

#include "app/ActionContextController.h"
#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "canvas/CanvasFileView.h"
#include "canvas/CanvasViewTab.h"
#include "corbomite/bases/BasesView.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "editor/MarkdownView.h"
#include "editor/NoteEditorWidget.h"

#include <canvas/CanvasCommands.h>
#include <canvas/CanvasDocument.h>
#include <canvas/CanvasScene.h>

using namespace Corbomite;

namespace {

void createFile(const QString &path, const QString &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(content.toUtf8());
}

const QString kCanvasSeed = QStringLiteral(
    "{\"nodes\":[{\"id\":\"n1\",\"type\":\"text\",\"x\":0,\"y\":0,"
    "\"width\":250,\"height\":60,\"text\":\"hi\"}],\"edges\":[]}");

const QString kBaseSeed = QStringLiteral(
    "views:\n  - type: table\n    name: All\n");

} // namespace

class TstActionContext : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("action-context test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    // -----------------------------------------------------------------
    // Phase gate: for every ENABLED action in the collection, across
    // several contexts (no vault, markdown tab, canvas tab, bases tab),
    // the controller must report a real handler. This is the O1
    // acceptance gate named in the plan.
    // -----------------------------------------------------------------
    void noSilentNoop()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Note.md"), QStringLiteral("# Note\n"));
        createFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);
        createFile(tmp.path() + QStringLiteral("/Base.base"), kBaseSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        auto *ctx = mw.actionContext();
        QVERIFY(ctx);

        // Context 1: no vault open at all (welcome screen).
        assertNoSilentNoop(&mw, QStringLiteral("no-vault"));

        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        // Context 2: markdown tab focused.
        mw.onNoteActivated(QStringLiteral("Note.md"));
        QTest::qWait(200);
        assertNoSilentNoop(&mw, QStringLiteral("markdown"));

        // Context 3: canvas tab focused.
        mw.onNoteActivated(QStringLiteral("Canvas.canvas"));
        QTest::qWait(200);
        assertNoSilentNoop(&mw, QStringLiteral("canvas"));

        // Context 4: bases tab focused.
        mw.onNoteActivated(QStringLiteral("Base.base"));
        QTest::qWait(200);
        assertNoSilentNoop(&mw, QStringLiteral("bases"));
    }

    // -----------------------------------------------------------------
    // O1.T2 — an in-place view-type swap (markdown leaf navigate()'d to a
    // .canvas viewState, active-leaf POINTER unchanged) must refresh
    // action state: format_bold disabled, editor-mode radio cleared.
    // Reproduces report §4.2 (Workspace::setActiveLeaf early-returns on
    // an unchanged leaf, so activeLeafChanged never fires for this case).
    // -----------------------------------------------------------------
    void inPlaceViewTypeChange_refreshesActionState()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Source.md"),
                   QStringLiteral("# Source\n[[Target.canvas]]\n"));
        createFile(tmp.path() + QStringLiteral("/Target.canvas"), kCanvasSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        mw.onNoteActivated(QStringLiteral("Source.md"));
        QTest::qWait(200);

        auto *ws = mw.findChild<Workspace *>();
        QVERIFY(ws);
        auto *leaf = ws->activeLeaf();
        QVERIFY(leaf);
        auto *mv = qobject_cast<MarkdownView *>(leaf->view());
        QVERIFY2(mv, "active leaf must be a MarkdownView after opening Source.md");
        auto *editor = mv->editorWidget();
        QVERIFY(editor);

        auto *ac = mw.actionCollection();
        QVERIFY(ac->action(QStringLiteral("format_bold"))->isEnabled());

        // Plain-click wikilink navigation — NAVIGATES THE SAME LEAF IN
        // PLACE via WorkspaceLeaf::navigate(), which is exactly the path
        // report §4.2 describes: activeLeafChanged does not fire because
        // the active leaf pointer itself never changes.
        const int leafCountBefore = ws->allLeaves().size();
        Q_EMIT editor->linkActivated(QStringLiteral("Target.canvas"), /*openInNewTab=*/false);
        QTest::qWait(200);

        QCOMPARE(ws->allLeaves().size(), leafCountBefore);
        QVERIFY2(qobject_cast<CanvasFileView *>(ws->activeLeaf()->view()) != nullptr,
                 "leaf must now host a CanvasFileView (in-place swap)");

        QVERIFY2(!ac->action(QStringLiteral("format_bold"))->isEnabled(),
                  "format_bold must be disabled after swapping to a canvas view in place");
        QVERIFY2(!ac->action(QStringLiteral("view_editing_mode"))->isChecked(),
                  "editor-mode radio must not stay checked on a non-markdown tab");
        QVERIFY2(!ac->action(QStringLiteral("view_editing_mode"))->isEnabled(),
                  "editor-mode radio must be disabled on a non-markdown tab");
    }

    // -----------------------------------------------------------------
    // O1.T4 — Ctrl+S (file_save) must actually save a canvas tab, not
    // just markdown/text. Previously CanvasFileView (a bare FileView, not
    // a TextFileView) was invisible to saveCurrentNote()'s fallback path.
    // -----------------------------------------------------------------
    void saveAction_savesCanvas()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString canvasPath = tmp.path() + QStringLiteral("/Canvas.canvas");
        createFile(canvasPath, kCanvasSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        mw.onNoteActivated(QStringLiteral("Canvas.canvas"));
        QTest::qWait(200);

        auto *ws = mw.findChild<Workspace *>();
        QVERIFY(ws);
        auto *cv = qobject_cast<CanvasFileView *>(ws->activeLeaf()->view());
        QVERIFY2(cv, "active leaf must be a CanvasFileView after opening Canvas.canvas");
        auto *tab = cv->canvasWidget();
        QVERIFY(tab);
        auto *scene = tab->canvasScene();
        QVERIFY(scene);

        // file_save must be enabled on a canvas tab (O1.T4's enablement half).
        auto *saveAction = mw.actionCollection()->action(QStringLiteral("file_save"));
        QVERIFY(saveAction);
        QVERIFY2(saveAction->isEnabled(), "file_save must be enabled on a canvas tab");

        // Mutate the node's position (real undoable op) and confirm the
        // tab reports modified before save.
        auto *doc = scene->document();
        QVERIFY(doc);
        QHash<QString, QPointF> oldPos{{QStringLiteral("n1"), QPointF(0, 0)}};
        QHash<QString, QPointF> newPos{{QStringLiteral("n1"), QPointF(500, 500)}};
        scene->undoStack()->push(new Canvas::CmdMoveCards(doc, oldPos, newPos));
        QVERIFY2(tab->isModified(), "moving a node must mark the canvas modified");

        saveAction->trigger();
        QTest::qWait(200);

        QVERIFY2(!tab->isModified(), "file_save must actually save the canvas (Ctrl+S was a no-op before O1.T4)");

        // Confirm the NEW position actually reached disk.
        QFile f(canvasPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto json = QJsonDocument::fromJson(f.readAll()).object();
        const auto nodes = json.value(QStringLiteral("nodes")).toArray();
        QCOMPARE(nodes.size(), 1);
        const auto n1 = nodes.at(0).toObject();
        QCOMPARE(n1.value(QStringLiteral("x")).toInt(), 500);
        QCOMPARE(n1.value(QStringLiteral("y")).toInt(), 500);
    }

    // -----------------------------------------------------------------
    // O2.T4 — Reading mode must disable format verbs by routing through
    // MarkdownView::canEdit() (== activeLeaf()->hasEditing()), not just
    // stop responding to keystrokes.
    // -----------------------------------------------------------------
    void readingMode_disablesFormatVerbs()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Note.md"), QStringLiteral("# Note\n"));

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        mw.onNoteActivated(QStringLiteral("Note.md"));
        QTest::qWait(200);

        auto *ws = mw.findChild<Workspace *>();
        QVERIFY(ws);
        auto *mv = qobject_cast<MarkdownView *>(ws->activeLeaf()->view());
        QVERIFY(mv);
        auto *ac = mw.actionCollection();

        QVERIFY2(ac->action(QStringLiteral("format_bold"))->isEnabled(),
                  "format_bold must be enabled in LivePreview mode");

        mv->editorWidget()->setViewMode(NoteEditorWidget::ViewMode::Reading);
        QTest::qWait(200);

        QVERIFY2(!mv->canEdit(), "canEdit() must be false in Reading mode");
        QVERIFY2(!ac->action(QStringLiteral("format_bold"))->isEnabled(),
                  "format_bold must be disabled in Reading mode");
        QVERIFY2(!ac->action(QStringLiteral("insert_table"))->isEnabled(),
                  "Insert > Table must be disabled in read-only Reading mode (O1.T6)");
    }

    // -----------------------------------------------------------------
    // O2.T5 — vault-open is a window-level Tier-B capability: with no
    // vault open at all, vault-scoped actions must be disabled.
    // -----------------------------------------------------------------
    void noVault_disablesVaultActions()
    {
        CorbomiteApp app;
        MainWindow mw(&app);
        auto *ac = mw.actionCollection();

        QVERIFY2(!ac->action(QStringLiteral("file_close_vault"))->isEnabled(),
                  "file_close_vault must be disabled with no vault open");
        QVERIFY2(!ac->action(QStringLiteral("file_new_note"))->isEnabled(),
                  "file_new_note must be disabled with no vault open");
        QVERIFY2(!ac->action(QStringLiteral("search_vault"))->isEnabled(),
                  "search_vault must be disabled with no vault open");
        QVERIFY2(!ac->action(QStringLiteral("graph_view"))->isEnabled(),
                  "graph_view must be disabled with no vault open");
    }

private:
    static void assertNoSilentNoop(MainWindow *mw, const QString &contextLabel)
    {
        auto *ctx = mw->actionContext();
        for (QAction *action : mw->actionCollection()->actions()) {
            if (!action->isEnabled()) continue;
            const QString id = action->objectName();
            if (id.isEmpty()) continue;
            QVERIFY2(ctx->hasHandlerForCurrentContext(id),
                     qPrintable(QStringLiteral(
                         "action '%1' is ENABLED in context '%2' but the "
                         "controller reports no handler for it — silent "
                         "no-op").arg(id, contextLabel)));
        }
    }
};

QTEST_MAIN(TstActionContext)
#include "tst_action_context.moc"
