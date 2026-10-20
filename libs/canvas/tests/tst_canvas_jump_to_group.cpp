// SPDX-License-Identifier: GPL-3.0-or-later
//
// "Canvas: Jump to group".
//
//   app/own/2028-n9.js:667-677  the canvas:jump-to-group command
//   app/own/2028-n9.js:289-314  l9, the suggest modal: items are the
//                               document's `type == "group"` nodes, each
//                               shown as `label || "Untitled group"`
//   app/own/2018-k7.js:568-578  zoomToBbox: fit with a 1.1 margin, clamp
//                               log2 scale to [-4, 1], centre on the bbox
//
// The 1.1 margin factor and the clamp are the whole point of pinning the
// scale: a plain fitInView would satisfy "the group is on screen" while
// getting both numbers wrong.

#include <QTest>

#include <KLocalizedString>
#include <QTransform>

#include "canvas/CanvasDocument.h"
#include "canvas/CanvasScene.h"
#include "canvas/CanvasView.h"

#include <cmath>

using namespace Canvas;

namespace {

CanvasNode group(const QString &id, const QString &label, int x, int y, int w, int h)
{
    CanvasNode n;
    n.id = id;
    n.type = NodeType::Group;
    n.label = label;
    n.x = x;
    n.y = y;
    n.width = w;
    n.height = h;
    return n;
}

} // namespace

class TestCanvasJumpToGroup : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("corbomite");
    }

    // Only groups, in document order, with Obsidian's untitled fallback.
    void groupJumpTargetsAreGroupsInOrderWithUntitledFallback()
    {
        CanvasDocument doc;
        CanvasScene scene;
        scene.setDocument(&doc);

        doc.addNode(group(QStringLiteral("g1"), QStringLiteral("Alpha"), 0, 0, 100, 100));
        CanvasNode text;
        text.id = QStringLiteral("t1");
        text.type = NodeType::Text;
        text.text = QStringLiteral("not a group");
        doc.addNode(text);
        doc.addNode(group(QStringLiteral("g2"), QString(), -500, 20, 200, 80));
        doc.addNode(group(QStringLiteral("g3"), QStringLiteral("Beta"), 40, -60, 60, 60));

        const auto targets = scene.groupJumpTargets();
        QCOMPARE(targets.size(), 3);
        QCOMPARE(targets[0].nodeId, QStringLiteral("g1"));
        QCOMPARE(targets[0].label, QStringLiteral("Alpha"));
        QCOMPARE(targets[1].nodeId, QStringLiteral("g2"));
        QCOMPARE(targets[1].label, QStringLiteral("Untitled group"));
        QCOMPARE(targets[2].nodeId, QStringLiteral("g3"));
        QCOMPARE(targets[2].label, QStringLiteral("Beta"));

        QCOMPARE(targets[0].bounds, QRectF(0, 0, 100, 100));
        QCOMPARE(targets[2].bounds, QRectF(40, -60, 60, 60));
    }

    void groupJumpTargetsAreEmptyWithoutGroups()
    {
        CanvasDocument doc;
        CanvasScene scene;
        scene.setDocument(&doc);
        CanvasNode text;
        text.id = QStringLiteral("t1");
        text.type = NodeType::Text;
        doc.addNode(text);
        QVERIFY(scene.groupJumpTargets().isEmpty());
    }

    // Obsidian's scale: min(W / (1.1 * w), H / (1.1 * h)), clamped, where W
    // and H are the canvas viewport's CSS pixels (app/own/2018-k7.js:569-573).
    // That is viewport MINUS scrollbar, so derive the expectation from the
    // view rather than from the 800x600 it was resized to.
    void zoomToBboxUsesObsidianMarginAndClamp()
    {
        CanvasView view;
        view.resize(800, 600);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY(view.viewport()->width() > 0);
        QVERIFY(view.viewport()->height() > 0);
        const double vw = view.viewport()->width();
        const double vh = view.viewport()->height();

        // Width-limited: 800 / (1.1 * 400) == 1.818…, height allows more.
        view.zoomToBbox(QRectF(0, 0, 400, 100));
        const double widthLimited = vw / (1.1 * 400.0);
        QVERIFY2(std::abs(view.transform().m11() - widthLimited) < 1e-9,
                 qPrintable(QStringLiteral("scale: got %1 want %2")
                                .arg(view.transform().m11()).arg(widthLimited)));

        // Height-limited instead: a tall, narrow bbox.
        view.zoomToBbox(QRectF(0, 0, 100, 300));
        const double heightLimited = vh / (1.1 * 300.0);
        QVERIFY(std::abs(view.transform().m11() - heightLimited) < 1e-9);
    }

    // A group far larger than the viewport would fit to a scale below the
    // floor, and one far smaller to a scale above the ceiling; both clamp to
    // Obsidian's log2 [-4, 1] — the same clamp requestFrame() re-applies
    // every frame upstream (app/own/2018-k7.js:821).
    void zoomToBboxClampsBothEnds()
    {
        CanvasView view;
        view.resize(800, 600);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        view.zoomToBbox(QRectF(0, 0, 100000, 100000));
        QVERIFY2(std::abs(view.transform().m11() - std::pow(2.0, -4.0)) < 1e-12,
                 qPrintable(QStringLiteral("huge bbox scale: got %1 want %2")
                                .arg(view.transform().m11())
                                .arg(std::pow(2.0, -4.0))));

        view.zoomToBbox(QRectF(0, 0, 1, 1));
        QVERIFY2(std::abs(view.transform().m11() - 2.0) < 1e-12,
                 qPrintable(QStringLiteral("tiny bbox scale: got %1 want 2")
                                .arg(view.transform().m11())));
    }

    // ...and it centres on the bbox, which is the whole point of the
    // command: a group anywhere on an infinite canvas must land in the
    // middle of the viewport.
    void zoomToBboxCentresOnTheBox()
    {
        CanvasView view;
        view.resize(800, 600);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const QRectF target(-1240.5, 907.25, 400, 200);

        view.zoomToBbox(target);
        QTest::qWait(1);   // let the deferred post-layout re-centre land

        const QPointF centre = view.mapToScene(view.viewport()->rect().center());
        const double scale = view.transform().m11();
        // Scrollbars move in device pixels, so the centre lands within one
        // of them -- the same tolerance CanvasView::setViewState() documents.
        const double tol = 1.5 / scale;
        QVERIFY2(std::abs(centre.x() - target.center().x()) < tol,
                 qPrintable(QStringLiteral("x: got %1 want %2").arg(centre.x())
                                .arg(target.center().x())));
        QVERIFY2(std::abs(centre.y() - target.center().y()) < tol,
                 qPrintable(QStringLiteral("y: got %1 want %2").arg(centre.y())
                                .arg(target.center().y())));
    }

    void zoomToBboxIgnoresDegenerateInput()
    {
        CanvasView view;
        view.resize(800, 600);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.resetTransform();
        const double before = view.transform().m11();

        view.zoomToBbox(QRectF());
        view.zoomToBbox(QRectF(10, 10, 0, 50));
        view.zoomToBbox(QRectF(10, 10, 50, 0));
        QCOMPARE(view.transform().m11(), before);
    }
};

QTEST_MAIN(TestCanvasJumpToGroup)
#include "tst_canvas_jump_to_group.moc"