// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster O Phase O4 — the CanvasViewActions provider. Named test from the
// plan's "Tests:" line: each toggle reaches
// CanvasAlignmentStrategy/CanvasView. Driven through a real MainWindow +
// vault, same pattern as tst_view_actions_provider.cpp /
// tst_toolbar_policy.cpp. Runs under QT_QPA_PLATFORM=offscreen.

#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
#include "canvas/CanvasViewActions.h"
#include "canvas/CanvasViewTab.h"
#include "corbomite/core/View.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomitesettings.h"

#include <canvas/CanvasAlignmentStrategy.h>
#include <canvas/CanvasScene.h>
#include <canvas/CanvasView.h>
#include <canvas/TextCardItem.h>

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

} // namespace

class TstCanvasViewActions : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("canvas-view-actions test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    void cleanup()
    {
        // Don't leak a non-default setting into the next test slot / run.
        CorbomiteSettings::self()->setSnapToGrid(true);
        CorbomiteSettings::self()->setSnapToObjects(true);
        CorbomiteSettings::self()->setShowGrid(true);
        CorbomiteSettings::self()->save();
    }

    // O4.T2/T5 — providers are constructed eagerly, same discipline as
    // MarkdownViewActions (O3.T2 — the Hotkeys page needs every type's
    // shortcuts even with no matching tab open).
    void eagerlyConstructed_beforeAnyTabOpen()
    {
        CorbomiteApp app;
        MainWindow mw(&app);

        auto *provider = mw.canvasViewActions();
        QVERIFY2(provider, "CanvasViewActions must exist at MainWindow construction");
        QCOMPARE(provider->viewType(), QStringLiteral("canvas"));

        auto *pac = provider->actionCollection();
        QVERIFY(pac);
        QVERIFY2(pac->action(QStringLiteral("canvas_snap_grid")) != nullptr,
                  "provider's collection must be populated before any tab is open");
        QVERIFY2(pac->action(QStringLiteral("canvas_zoom_to_fit")) != nullptr, "");

        QVERIFY2(mw.actionContext()->currentProvider() == nullptr,
                  "provider must not be installed with nothing focused");
    }

    // Tier A — focusing a canvas tab installs the canvas provider (swap
    // away from markdown, not an uninstall-to-null — canvas has its own
    // provider as of O4).
    void focusingCanvasTab_installsCanvasProvider()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Note.md"), QStringLiteral("# Note\n"));
        createFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        auto *provider = mw.canvasViewActions();
        QVERIFY(provider);

        mw.onNoteActivated(QStringLiteral("Canvas.canvas"));
        QTest::qWait(200);
        QCOMPARE(mw.actionContext()->currentProvider(), static_cast<ViewActions *>(provider));

        auto *pac = provider->actionCollection();
        QVERIFY2(pac->action(QStringLiteral("canvas_snap_grid"))->isEnabled(),
                  "installed + bound to a canvas view -> canvas_snap_grid enabled");

        mw.onNoteActivated(QStringLiteral("Note.md"));
        QTest::qWait(200);
        QVERIFY2(!pac->action(QStringLiteral("canvas_snap_grid"))->isEnabled(),
                  "uninstalled (focus moved to markdown) -> canvas actions disabled");
    }

    // O4.T2/T3/T4 — each toggle actually reaches
    // CanvasAlignmentStrategy/CanvasView, not just the QAction's own
    // checked state.
    void toggles_reachAlignmentStrategyAndView()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        mw.onNoteActivated(QStringLiteral("Canvas.canvas"));
        QTest::qWait(200);

        auto *cv = qobject_cast<CanvasFileView *>(mw.actionContext()->activeCanvasView());
        QVERIFY(cv);
        auto *tab = cv->canvasWidget();
        QVERIFY(tab);
        auto *align = tab->canvasScene()->alignmentStrategy();
        QVERIFY(align);
        auto *view = tab->canvasView();
        QVERIFY(view);

        QVERIFY2(align->snapToGridEnabled(), "kcfg default is true");
        QVERIFY2(align->snapToObjectsEnabled(), "kcfg default is true");
        QVERIFY2(view->gridVisible(), "kcfg default is true");

        auto *pac = mw.canvasViewActions()->actionCollection();
        pac->action(QStringLiteral("canvas_snap_grid"))->trigger();
        QVERIFY2(!align->snapToGridEnabled(), "toggling the action must reach the real CanvasAlignmentStrategy");
        QVERIFY2(!CorbomiteSettings::self()->snapToGrid(), "and persist to kcfg");

        pac->action(QStringLiteral("canvas_snap_objects"))->trigger();
        QVERIFY2(!align->snapToObjectsEnabled(), "");

        pac->action(QStringLiteral("canvas_show_grid"))->trigger();
        QVERIFY2(!view->gridVisible(), "toggling the action must reach the real CanvasView");
    }

    // O4.T5 — Tier B: zoom-to-selection needs a selection.
    void zoomToSelection_disabledUntilSomethingSelected()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        mw.onNoteActivated(QStringLiteral("Canvas.canvas"));
        QTest::qWait(200);

        auto *pac = mw.canvasViewActions()->actionCollection();
        auto *zoomSel = pac->action(QStringLiteral("canvas_zoom_to_selection"));
        QVERIFY(zoomSel);
        QVERIFY2(!zoomSel->isEnabled(), "nothing selected -> disabled");

        auto *cv = qobject_cast<CanvasFileView *>(mw.actionContext()->activeCanvasView());
        QVERIFY(cv);
        auto *scene = cv->canvasWidget()->canvasScene();
        auto *item = scene->textCardItem(QStringLiteral("n1"));
        QVERIFY(item);
        item->setSelected(true);
        QTest::qWait(50);

        QVERIFY2(zoomSel->isEnabled(), "something selected -> enabled");
    }
};

QTEST_MAIN(TstCanvasViewActions)
#include "tst_canvas_view_actions.moc"
