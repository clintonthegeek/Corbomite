// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster M M5.3 — canvas viewport persistence.
//
// The contract under test is Obsidian's, not ours, so every field name and
// unit here is Obsidian's verbatim. Source of truth, all in the restored
// 1.13.7 corpus:
//
//   app/own/2028-n9.js:850   CanvasView.getState() -> state.viewState = canvas.getState()
//   app/own/2018-k7.js:968   canvas.getState() -> { x: tx, y: ty, zoom: tZoom }
//   app/own/2018-k7.js:963   canvas.setState() guards all three with Number.isNumber
//   app/own/2018-k7.js:841   transform = translate(W/2,H/2) scale(2^zoom) translate(-x,-y)
//   app/own/2018-k7.js:821   tZoom = clamp(tZoom, -4, 1)   => zoom is log2 of scale
//
// So: `zoom` is log2 of the scale factor, and `x`/`y` are the scene point
// drawn at the VIEWPORT CENTRE — not the top-left corner.
//
// The field lives at ViewState.state.viewState, which WorkspaceSerializer
// reads via WorkspaceLeaf::serialize() and writes via setViewState(). Those
// two are exactly the seams this test drives, so it needs no libs/core
// internals.
//
// Refutes the Cluster M M5.3 plan's original wording ("into the canvas leaf's
// ephemeral state"): Obsidian's canvas getEphemeralState() returns {} and
// eState is never serialised at all. See
// docs/obsidian-audit/addenda/2026-10-03-canvas-viewport-ephemeral-state-resolution.md
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

#include <corbomite/canvas/CanvasView.h>

#include <cmath>

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

/// A canvas leaf as Obsidian 1.13.7 writes it. `eState` is absent because
/// Obsidian never writes one — its WorkspaceLeaf.serialize writes only
/// id/type/dimension?/pinned?/state/group?, and Obsidian's canvas
/// getEphemeralState() returns {} regardless. obsidianAuthoredLeafIsReadBack-
/// Verbatim() asserts we do not start writing one.
const QByteArray kObsidianCanvasLeaf = QByteArrayLiteral(R"({
  "id": "9f2c1ab4de705536",
  "type": "leaf",
  "state": {
    "type": "canvas",
    "state": {
      "file": "Canvas.canvas",
      "viewState": { "x": -1180.5, "y": 640.25, "zoom": -1.5849625007211563 }
    },
    "icon": "palette",
    "title": "Canvas"
  }
})");

/// Obsidian restores x/y exactly (a continuous CSS transform on the canvas
/// element). QGraphicsView pans by scrollbar value and a scroll unit is one
/// device pixel, so a restore lands on the rounding step of that grid —
/// measured worst case 0.5 device pixels across the zoom range and targets
/// spanning the world. Assert with 3x that as slack; expressed in scene
/// units, `devicePx / scale`.
double xyTolerance(double logZoom, double devicePx = 1.5)
{
    return devicePx / std::pow(2.0, logZoom);
}

} // namespace

class TstCanvasViewState : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("canvas view-state test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    // The falsifiable core: a raw CanvasView honours Obsidian's units and
    // anchor. Independent of any workspace plumbing, so a regression here
    // localises to CanvasView rather than the leaf/serializer path.
    void canvasViewViewStateRoundTripsObsidianUnitsAndAnchor()
    {
        Corbomite::Canvas::CanvasView view;
        view.resize(800, 600);

        const double logZoom = -1.0;               // scale 0.5
        const double sceneX = -1180.5, sceneY = 640.25;
        const double tol = xyTolerance(logZoom);
        view.setViewState(QJsonObject{
            {QStringLiteral("x"), sceneX},
            {QStringLiteral("y"), sceneY},
            {QStringLiteral("zoom"), logZoom},
        });
        QTest::qWait(1);   // let the deferred post-layout re-centre land

        // zoom is log2 of scale, not the scale itself: 2^-1 == 0.5. If it
        // were stored as a raw factor this would be 1/0.5 == 2.
        QVERIFY(std::abs(view.transform().m11() - 0.5) < 1e-12);

        // x/y are the scene point at the viewport CENTRE.
        const QPointF centreScene = view.mapToScene(view.viewport()->rect().center());
        QVERIFY2(std::abs(centreScene.x() - sceneX) < tol,
                 qPrintable(QStringLiteral("x: got %1 want %2 tol %3")
                                .arg(centreScene.x()).arg(sceneX).arg(tol)));
        QVERIFY2(std::abs(centreScene.y() - sceneY) < tol,
                 qPrintable(QStringLiteral("y: got %1 want %2 tol %3")
                                .arg(centreScene.y()).arg(sceneY).arg(tol)));

        // ...and specifically NOT the top-left corner, which sits half a
        // viewport away in scene units. Pin the offset so a switch to a
        // corner-anchored reading cannot pass.
        const QPointF topLeftScene = view.mapToScene(QPoint(0, 0));
        const double halfW = view.viewport()->width() / 2.0 / 0.5;
        const double halfH = view.viewport()->height() / 2.0 / 0.5;
        const double cornerTol = xyTolerance(logZoom, 2.5);
        QVERIFY(std::abs(topLeftScene.x() - (sceneX - halfW)) < cornerTol);
        QVERIFY(std::abs(topLeftScene.y() - (sceneY - halfH)) < cornerTol);
        QVERIFY(std::abs(topLeftScene.x() - sceneX) > halfW / 2.0);

        // Re-emitting is lossless for an in-range value.
        const QJsonObject out = view.viewState();
        QCOMPARE(out.keys(), (QStringList{QStringLiteral("x"),
                                          QStringLiteral("y"),
                                          QStringLiteral("zoom")}));
        QVERIFY(std::abs(out.value(QStringLiteral("x")).toDouble() - centreScene.x()) < 1e-9);
        QVERIFY(std::abs(out.value(QStringLiteral("y")).toDouble() - centreScene.y()) < 1e-9);
        QVERIFY(std::abs(out.value(QStringLiteral("zoom")).toDouble() - logZoom) < 1e-12);
    }

    // Obsidian clamps log2 zoom to [-4, 1] on the very first animation frame
    // and accepts the out-of-range value on load rather than rejecting it
    // (app/own/2018-k7.js:821 vs :966).
    void outOfRangeZoomIsClampedNotRejected()
    {
        Corbomite::Canvas::CanvasView view;
        view.resize(400, 400);

        view.setViewState(QJsonObject{
            {QStringLiteral("x"), 0.0},
            {QStringLiteral("y"), 0.0},
            {QStringLiteral("zoom"), -40.0},
        });
        QVERIFY(std::abs(view.transform().m11() - std::pow(2.0, -4.0)) < 1e-12);

        view.setViewState(QJsonObject{
            {QStringLiteral("x"), 0.0},
            {QStringLiteral("y"), 0.0},
            {QStringLiteral("zoom"), 12.0},
        });
        QVERIFY(std::abs(view.transform().m11() - 2.0) < 1e-12);
    }

    // The Number.isNumber(x) && (y) && (zoom) guard at 2018-k7.js:966 is
    // all-or-nothing: there is no merge, so a partial payload must leave the
    // viewport completely untouched rather than nudging one axis.
    void partialOrNonNumericViewStateIsIgnoredWholesale()
    {
        Corbomite::Canvas::CanvasView view;
        view.resize(400, 400);
        view.setViewState(QJsonObject{
            {QStringLiteral("x"), 12.0},
            {QStringLiteral("y"), -34.0},
            {QStringLiteral("zoom"), 0.0},
        });
        const QJsonObject before = view.viewState();

        const QJsonArray rejected{
            QJsonObject{{QStringLiteral("x"), 999.0},
                        {QStringLiteral("y"), 999.0}},                       // no zoom
            QJsonObject{{QStringLiteral("x"), 999.0}},                       // no y, no zoom
            QJsonObject{{QStringLiteral("x"), 999.0},
                        {QStringLiteral("y"), 999.0},
                        {QStringLiteral("zoom"), QStringLiteral("nope")}},    // zoom a string
            QJsonObject{{QStringLiteral("x"), QStringLiteral("999")},
                        {QStringLiteral("y"), 999.0},
                        {QStringLiteral("zoom"), 0.0}},                      // x a string
        };
        for (const auto &bad : rejected) {
            view.setViewState(bad.toObject());
            const QJsonObject after = view.viewState();
            QVERIFY2(std::abs(after.value(QStringLiteral("x")).toDouble()
                              - before.value(QStringLiteral("x")).toDouble()) < 1e-9
                         && std::abs(after.value(QStringLiteral("y")).toDouble()
                                     - before.value(QStringLiteral("y")).toDouble()) < 1e-9
                         && std::abs(after.value(QStringLiteral("zoom")).toDouble()
                                     - before.value(QStringLiteral("zoom")).toDouble()) < 1e-12,
                     "a non-conforming viewState must not move the viewport at all");
        }
    }

    // End to end through WorkspaceLeaf -- the exact object
    // WorkspaceSerializer::buildLeafNode reads (wl->serialize()) and
    // applyLeafPayload writes (wl->setViewState()). A real Obsidian-written
    // canvas leaf must open at the right viewport, and serializing again
    // must reproduce the fixture's viewState.
    void obsidianAuthoredLeafIsReadBackVerbatim()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        auto *ws = mw.findChild<Workspace *>();
        QVERIFY(ws);

        // Exactly what WorkspaceSerializer::applyLeafPayload does with an
        // Obsidian-authored leaf: hand the leaf's `state` object to the leaf.
        const QJsonObject obsidianLeaf =
            QJsonDocument::fromJson(kObsidianCanvasLeaf).object();
        auto *leaf = ws->activeLeaf();
        QVERIFY(leaf);
        leaf->setViewState(obsidianLeaf.value(QStringLiteral("state")).toObject());
        QTest::qWait(1);

        auto *cv = qobject_cast<CanvasFileView *>(leaf->view());
        QVERIFY2(cv, "an Obsidian canvas leaf must instantiate CanvasFileView");
        QVERIFY(cv->canvasWidget());
        auto *canvasView = cv->canvasWidget()->canvasView();
        QVERIFY(canvasView);

        const QJsonObject expected =
            obsidianLeaf.value(QStringLiteral("state"))
                .toObject().value(QStringLiteral("state"))
                .toObject().value(QStringLiteral("viewState"))
                .toObject();

        const double wantZoom = expected.value(QStringLiteral("zoom")).toDouble();
        const double tol = xyTolerance(wantZoom);

        // log2 of the scale factor -- not the raw factor.
        const double wantScale = std::pow(2.0, wantZoom);
        QVERIFY2(std::abs(canvasView->transform().m11() - wantScale) < 1e-12,
                 "zoom must be restored as log2(scale)");

        // x/y land at the viewport centre, in scene units.
        const QPointF centre = canvasView->mapToScene(canvasView->viewport()->rect().center());
        QVERIFY2(std::abs(centre.x() - expected.value(QStringLiteral("x")).toDouble()) < tol,
                 qPrintable(QStringLiteral("x: got %1 want %2 tol %3")
                                .arg(centre.x())
                                .arg(expected.value(QStringLiteral("x")).toDouble())
                                .arg(tol)));
        QVERIFY2(std::abs(centre.y() - expected.value(QStringLiteral("y")).toDouble()) < tol,
                 qPrintable(QStringLiteral("y: got %1 want %2 tol %3")
                                .arg(centre.y())
                                .arg(expected.value(QStringLiteral("y")).toDouble())
                                .arg(tol)));

        // Round trip back out through the serializer's read seam. Compared
        // against what we actually restored, so this asserts idempotence
        // rather than re-asserting the tolerance.
        const QJsonObject outState = leaf->serialize().value(QStringLiteral("state")).toObject();
        const QJsonObject outViewState =
            outState.value(QStringLiteral("state")).toObject().value(QStringLiteral("viewState")).toObject();

        QCOMPARE(outViewState.keys(), expected.keys());
        QVERIFY2(std::abs(outViewState.value(QStringLiteral("x")).toDouble()
                          - centre.x()) < 1e-6, "x must round-trip");
        QVERIFY2(std::abs(outViewState.value(QStringLiteral("y")).toDouble()
                          - centre.y()) < 1e-6, "y must round-trip");
        QVERIFY2(std::abs(outViewState.value(QStringLiteral("zoom")).toDouble()
                          - wantZoom) < 1e-12, "zoom must round-trip");

        // The viewport lives in `state`, never in an `eState` key -- the whole
        // point of the addendum. Anything else makes Corbomite write a key
        // Obsidian never writes.
        for (auto it = outState.begin(); it != outState.end(); ++it)
            QVERIFY2(it.key() != QLatin1String("eState"),
                     "Corbomite must not write eState into a workspace.json leaf");
    }

    // A canvas leaf Corbomite authored itself must restore on reopen: pan +
    // zoom, serialize, restore into a fresh leaf, same viewport.
    void corbomiteAuthoredViewportSurvivesReopen()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        auto *ws = mw.findChild<Workspace *>();
        QVERIFY(ws);

        const double logZoom = -2.5;
        const double sceneX = 4242.5, sceneY = -777.25;
        const double tol = xyTolerance(logZoom);
        QJsonObject written;
        {
            auto *leaf = ws->activeLeaf();
            QVERIFY(leaf);
            leaf->setViewState(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("canvas")},
                {QStringLiteral("state"), QJsonObject{{QStringLiteral("file"), QStringLiteral("Canvas.canvas")}}},
            });
            auto *cv = qobject_cast<CanvasFileView *>(leaf->view());
            QVERIFY(cv);
            auto *canvasView = cv->canvasWidget()->canvasView();
            QVERIFY(canvasView);

            canvasView->setViewState(QJsonObject{
                {QStringLiteral("x"), sceneX},
                {QStringLiteral("y"), sceneY},
                {QStringLiteral("zoom"), logZoom},
            });
            QTest::qWait(1);

            written = leaf->serialize().value(QStringLiteral("state")).toObject();
            // Prove the leaf is actually recording a viewport: a reopen that
            // restores "nothing" would otherwise pass vacuously.
            const QJsonObject vs =
                written.value(QStringLiteral("state")).toObject().value(QStringLiteral("viewState")).toObject();
            QVERIFY(!vs.isEmpty());
        }

        // Reopen: fresh leaf, the Obsidian-shaped ViewState we just wrote.
        auto *leaf2 = ws->activeLeaf();
        QVERIFY(leaf2);
        leaf2->setViewState(written);
        QTest::qWait(1);
        auto *cv2 = qobject_cast<CanvasFileView *>(leaf2->view());
        QVERIFY(cv2);
        auto *view2 = cv2->canvasWidget()->canvasView();
        QVERIFY(view2);

        const QJsonObject viewState = view2->viewState();
        QVERIFY2(std::abs(viewState.value(QStringLiteral("x")).toDouble() - sceneX) < tol,
                 qPrintable(QStringLiteral("x: got %1 want %2 tol %3")
                                .arg(viewState.value(QStringLiteral("x")).toDouble())
                                .arg(sceneX).arg(tol)));
        QVERIFY2(std::abs(viewState.value(QStringLiteral("y")).toDouble() - sceneY) < tol,
                 qPrintable(QStringLiteral("y: got %1 want %2 tol %3")
                                .arg(viewState.value(QStringLiteral("y")).toDouble())
                                .arg(sceneY).arg(tol)));
        QVERIFY2(std::abs(viewState.value(QStringLiteral("zoom")).toDouble() - logZoom) < 1e-12,
                 "zoom must survive a reopen");
    }

    // A canvas leaf with NO viewState (what every pre-M5.3 workspace.json
    // contains) must still open, and Corbomite must then always emit a
    // complete viewState.
    void absentViewStateStillOpensAndThenEmitsOne()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        auto *ws = mw.findChild<Workspace *>();
        QVERIFY(ws);
        auto *leaf = ws->activeLeaf();
        QVERIFY(leaf);

        leaf->setViewState(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("canvas")},
            {QStringLiteral("state"), QJsonObject{{QStringLiteral("file"), QStringLiteral("Canvas.canvas")}}},
        });
        auto *cv = qobject_cast<CanvasFileView *>(leaf->view());
        QVERIFY(cv);
        QVERIFY(cv->canvasWidget());
        QVERIFY(cv->canvasWidget()->canvasView());

        const QJsonObject outState =
            leaf->serialize().value(QStringLiteral("state")).toObject();
        const QJsonObject viewState =
            outState.value(QStringLiteral("state")).toObject().value(QStringLiteral("viewState")).toObject();
        QVERIFY2(viewState.contains(QStringLiteral("x"))
                     && viewState.contains(QStringLiteral("y"))
                     && viewState.contains(QStringLiteral("zoom")),
                 "a canvas leaf must always emit a complete viewState");
        QVERIFY(std::isfinite(viewState.value(QStringLiteral("zoom")).toDouble()));
        QVERIFY(std::isfinite(viewState.value(QStringLiteral("x")).toDouble()));
        QVERIFY(std::isfinite(viewState.value(QStringLiteral("y")).toDouble()));
    }
};

QTEST_MAIN(TstCanvasViewState)
#include "tst_canvas_view_state.moc"