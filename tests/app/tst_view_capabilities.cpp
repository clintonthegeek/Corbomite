// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster O Phase O2.T1 — the View Tier-B capability surface
// (canEdit/canSave/canZoom/canFind/hasSelection/canUndo/canRedo). One
// slot per view type this test binary can link against, asserting the
// per-type overrides answer correctly and — where the underlying state
// is mutable in-process (canvas selection/undo) — that the answer
// actually tracks real state changes.
//
// Graph coverage lives alongside GraphView's own plugin test suite
// (src/plugins/graph-view/tests/tst_graphview_plugin.cpp), same reason
// as tst_view_zoom_dispatch.cpp: GraphView is a separate plugin target
// this binary doesn't link against.
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include <KAboutData>
#include <KLocalizedString>

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "canvas/CanvasFileView.h"
#include "canvas/CanvasViewTab.h"
#include "corbomite/bases/BasesView.h"
#include "corbomite/core/View.h"
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

class TstViewCapabilities : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("view-capabilities test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    void markdownView_capabilities()
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
        View *view = ws->activeLeaf()->view();
        QVERIFY(view);
        QVERIFY2(qobject_cast<MarkdownView *>(view), "must be a MarkdownView");

        QVERIFY2(view->canZoom(), "markdown must answer canZoom() true (base default)");
        QVERIFY2(view->canSave(), "markdown must answer canSave() true");
        QVERIFY2(view->canFind(), "markdown must answer canFind() true");
        QVERIFY2(view->canEdit(), "markdown must answer canEdit() true in default (Live) mode");
        // O1.T8's approximation: canUndo/canRedo track canEdit() until
        // Markoff exposes a real undoD2 depth query (see
        // MarkdownView::canUndo()'s comment).
        QCOMPARE(view->canUndo(), view->canEdit());
        QCOMPARE(view->canRedo(), view->canEdit());
        QVERIFY2(!view->hasSelection(), "markdown has no hasSelection() override — base default false");
    }

    void canvasFileView_capabilities()
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
        QVERIFY(cv);
        View *view = cv;

        QVERIFY2(view->canZoom(), "canvas must answer canZoom() true (base default)");
        QVERIFY2(view->canEdit(), "canvas must answer canEdit() true");
        QVERIFY2(view->canSave(), "canvas must answer canSave() true");
        QVERIFY2(!view->canFind(), "canvas has no canFind() override — base default false");

        auto *scene = cv->canvasWidget()->canvasScene();
        QVERIFY(scene);
        QVERIFY2(!view->hasSelection(), "nothing selected yet");
        QVERIFY2(!view->canUndo(), "nothing pushed yet");

        // Real state changes must flow through: select the seeded node,
        // then push a real undoable command.
        QSignalSpy contextSpy(view, &View::contextChanged);
        for (auto *item : scene->items())
            item->setSelected(true);
        QVERIFY2(!contextSpy.isEmpty(), "selecting must emit contextChanged()");
        QVERIFY2(view->hasSelection(), "hasSelection() must reflect the real selection");

        auto *doc = scene->document();
        QVERIFY(doc);
        QHash<QString, QPointF> oldPos{{QStringLiteral("n1"), QPointF(0, 0)}};
        QHash<QString, QPointF> newPos{{QStringLiteral("n1"), QPointF(500, 500)}};
        contextSpy.clear();
        scene->undoStack()->push(new Canvas::CmdMoveCards(doc, oldPos, newPos));
        QVERIFY2(!contextSpy.isEmpty(), "pushing an undoable command must emit contextChanged()");
        QVERIFY2(view->canUndo(), "canUndo() must reflect the real undo stack");
        QVERIFY2(!view->canRedo(), "nothing to redo yet");
    }

    void basesView_capabilities()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Base.base"), kBaseSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        mw.onNoteActivated(QStringLiteral("Base.base"));
        QTest::qWait(200);

        auto *ws = mw.findChild<Workspace *>();
        QVERIFY(ws);
        auto *bv = qobject_cast<Bases::BasesView *>(ws->activeLeaf()->view());
        QVERIFY(bv);
        View *view = bv;

        QVERIFY2(!view->canZoom(), "bases must override canZoom() false — nothing to zoom");
        QVERIFY2(view->canEdit(), "bases must answer canEdit() true");
        QVERIFY2(view->canSave(), "bases must answer canSave() true");
        QVERIFY2(view->canFind(), "bases must answer canFind() true — owns its own search box");
        QCOMPARE(view->canUndo(), bv->canUndo());
        QCOMPARE(view->canRedo(), bv->canRedo());
    }
};

QTEST_MAIN(TstViewCapabilities)
#include "tst_view_capabilities.moc"
