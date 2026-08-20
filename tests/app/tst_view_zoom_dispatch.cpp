// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster O Phase O1.T3 — re-light the View::zoomIn/Out/Reset polymorphic
// dispatch. Report §4.1: the virtuals existed, MarkdownView's overrides
// were stale empty TODOs, and MainWindow bypassed them entirely — so
// canvas/graph (both of which have a real zoom) had no zoom action at
// all. One slot per view type, asserting the virtual is actually reached.
//
// Graph coverage lives alongside GraphView's own plugin test suite
// (src/plugins/graph-view/tests/tst_graphview_plugin.cpp) since GraphView
// is a separate plugin target this test binary doesn't link against.
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTransform>

#include <KAboutData>
#include <KLocalizedString>

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "canvas/CanvasFileView.h"
#include "canvas/CanvasViewTab.h"
#include "corbomite/core/View.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "editor/MarkdownView.h"
#include "editor/NoteEditorWidget.h"

#include <canvas/CanvasView.h>
#include <markoff/core/MarkdownView.h>

using namespace Corbomite;

namespace {

void createFile(const QString &path, const QString &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(content.toUtf8());
}

const QString kCanvasSeed = QStringLiteral("{\"nodes\":[],\"edges\":[]}");

} // namespace

class TstViewZoomDispatch : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("view-zoom-dispatch test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    void markdownView_zoomReachesFontScale()
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
        auto *leaf = mv->editorWidget()->activeLeaf();
        QVERIFY(leaf);

        const qreal base = leaf->fontScale();
        QCOMPARE(base, qreal(1.0));

        // Dispatch through the base View* pointer — this is exactly what
        // MainWindow::onZoomIn/Out/Reset now do (report §4.1's fix).
        View *view = mv;
        view->zoomIn();
        QVERIFY2(leaf->fontScale() > base, "View::zoomIn() must reach the Markoff leaf's fontScale");

        const qreal afterIn = leaf->fontScale();
        view->zoomOut();
        QVERIFY2(leaf->fontScale() < afterIn, "View::zoomOut() must reach the Markoff leaf's fontScale");

        view->zoomReset();
        QCOMPARE(leaf->fontScale(), qreal(1.0));
    }

    void canvasFileView_zoomReachesViewportTransform()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

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
        auto *canvasView = cv->canvasWidget()->canvasView();
        QVERIFY(canvasView);

        const QTransform identity;
        QCOMPARE(canvasView->transform(), identity);

        View *view = cv;
        view->zoomIn();
        QVERIFY2(canvasView->transform() != identity,
                  "View::zoomIn() must reach the CanvasView viewport transform");

        const QTransform afterIn = canvasView->transform();
        view->zoomOut();
        QVERIFY2(canvasView->transform() != afterIn,
                  "View::zoomOut() must reach the CanvasView viewport transform");

        view->zoomReset();
        QCOMPARE(canvasView->transform(), identity);
    }
};

QTEST_MAIN(TstViewZoomDispatch)
#include "tst_view_zoom_dispatch.moc"
