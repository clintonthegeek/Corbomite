// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsRectItem>

#include <graffodil/IGraphNode.h>
#include <graffodil/Anchors.h>
#include "canvas/CanvasAlignmentStrategy.h"

namespace {

/// Minimal IGraphNode for testing — mirrors Graffodil::Test::TestNode
/// (libs/graffodil/tests/TestHelpers.h, not a public/installed header so
/// not reused directly here).
class AlignTestNode : public QGraphicsRectItem, public Graffodil::IGraphNode {
public:
    AlignTestNode(const QString &id, const QRectF &rect)
        : QGraphicsRectItem(0, 0, rect.width(), rect.height())
    {
        setPos(rect.topLeft());
        m_id = id;
        // QGraphicsRectItem's default cosmetic pen pads sceneBoundingRect()
        // by half a pixel beyond rect() — disable it so scene-rect edges
        // line up exactly with the geometry the test asserts against.
        setPen(Qt::NoPen);
    }

    QString nodeId() const override { return m_id; }
    QList<Graffodil::Anchor> anchors() const override
    {
        return Graffodil::compassAnchors(mapToScene(rect()).boundingRect());
    }
    QRectF nodeBoundingRect() const override { return mapToScene(rect()).boundingRect(); }
    QGraphicsItem *graphicsItem() override { return this; }

private:
    QString m_id;
};

/// Test seam: overrides currentModifiers() to inject fake Alt/Shift state,
/// since QGuiApplication::keyboardModifiers() reflects genuine OS-level
/// modifier state that an offscreen unit test cannot fake by driving
/// events (no precedent for faking it elsewhere in this codebase — the
/// M2.5 Alt-drag-duplicate code reads modifiers off a real QGraphicsSceneMouseEvent,
/// not the global getter).
class FakeModifierStrategy : public Canvas::CanvasAlignmentStrategy {
public:
    void setFakeModifiers(Qt::KeyboardModifiers mods) { m_fake = mods; }

protected:
    Qt::KeyboardModifiers currentModifiers() const override { return m_fake; }

private:
    Qt::KeyboardModifiers m_fake = Qt::NoModifier;
};

} // namespace

class TestCanvasAlignment : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Appendix A ladder: 20 at scale>=1, 40 at >=0.5, 80 at >=0.25, else 160.
    void testGridSnapPitchByZoom()
    {
        QCOMPARE(Canvas::CanvasAlignmentStrategy::gridSpacingForScale(1.5), 20.0);
        QCOMPARE(Canvas::CanvasAlignmentStrategy::gridSpacingForScale(1.0), 20.0);
        QCOMPARE(Canvas::CanvasAlignmentStrategy::gridSpacingForScale(0.7), 40.0);
        QCOMPARE(Canvas::CanvasAlignmentStrategy::gridSpacingForScale(0.5), 40.0);
        QCOMPARE(Canvas::CanvasAlignmentStrategy::gridSpacingForScale(0.3), 80.0);
        QCOMPARE(Canvas::CanvasAlignmentStrategy::gridSpacingForScale(0.25), 80.0);
        QCOMPARE(Canvas::CanvasAlignmentStrategy::gridSpacingForScale(0.1), 160.0);
    }

    void testObjectSnapCornerAndCenter()
    {
        QGraphicsScene scene;
        QGraphicsView view(&scene);

        AlignTestNode primary(QStringLiteral("primary"), QRectF(0, 0, 100, 60));
        AlignTestNode other(QStringLiteral("other"), QRectF(300, 0, 100, 60));
        scene.addItem(&primary);
        scene.addItem(&other);

        Canvas::CanvasAlignmentStrategy strategy;

        // Drag primary so its right edge (proposed right = proposed.x()+100)
        // lands 5 units off other's left edge (300) -> within 15/scale=15
        // tolerance at scale 1.0, should snap primary's right edge exactly
        // onto other's left edge (proposed.x() == 200).
        const QPointF proposed(195, 0);
        auto result = strategy.align(&primary, proposed, {&primary, &other});

        QCOMPARE(result.position.x(), 200.0);
        QVERIFY(!result.guides.isEmpty());
    }

    void testToleranceScalesInverseZoom()
    {
        QGraphicsScene scene;
        QGraphicsView view(&scene);

        AlignTestNode primary(QStringLiteral("primary"), QRectF(0, 0, 100, 60));
        AlignTestNode other(QStringLiteral("other"), QRectF(300, 0, 100, 60));
        scene.addItem(&primary);
        scene.addItem(&other);

        Canvas::CanvasAlignmentStrategy strategy;
        // Isolate object-snap tolerance behavior from grid snap (which
        // could coincidentally land on the same coordinate).
        strategy.setSnapToGridEnabled(false);

        // At scale 2.0, tolerance = 15/2 = 7.5 scene units. Put primary's
        // right edge 7 units short of other's left edge (300) -> within
        // tolerance -> snaps.
        view.resetTransform();
        view.scale(2.0, 2.0);
        const QPointF justInside(193, 0); // right edge at 293, diff 7 <= 7.5
        auto insideResult = strategy.align(&primary, justInside, {&primary, &other});
        QCOMPARE(insideResult.position.x(), 200.0);

        // 8 units short -> outside 7.5 tolerance -> no object snap (grid
        // snap may still move it, but not to the object-snap coordinate).
        const QPointF justOutside(192, 0); // right edge at 292, diff 8 > 7.5
        auto outsideResult = strategy.align(&primary, justOutside, {&primary, &other});
        QVERIFY(outsideResult.position.x() != 200.0);
    }

    void testAltDisablesSnap()
    {
        QGraphicsScene scene;
        QGraphicsView view(&scene);

        AlignTestNode primary(QStringLiteral("primary"), QRectF(0, 0, 100, 60));
        AlignTestNode other(QStringLiteral("other"), QRectF(300, 0, 100, 60));
        scene.addItem(&primary);
        scene.addItem(&other);

        FakeModifierStrategy strategy;
        strategy.setFakeModifiers(Qt::AltModifier);

        const QPointF proposed(195, 0); // would otherwise object-snap to 200
        auto result = strategy.align(&primary, proposed, {&primary, &other});

        QCOMPARE(result.position, proposed);
        QVERIFY(result.guides.isEmpty());
    }

    // Regression test for a real live bug: dragging a card into a snap
    // caused the drag to get permanently stuck, with no way to pull it
    // back out. Root cause: SelectMoveTool derives each frame's `proposed`
    // from the primary's CURRENT (possibly already-snapped) pos(), so once
    // a frame snapped pos() onto a candidate, every subsequent frame's
    // `proposed` was computed relative to that pinned point -- and since
    // real per-frame mouse deltas are almost always smaller than the snap
    // tolerance, the search kept re-snapping right back to zero offset
    // forever, regardless of how far the user actually dragged. This test
    // drives align() the same way SelectMoveTool actually does: apply
    // result.position back to the primary's pos() before the next call,
    // and feed small per-frame deltas (well under the snap tolerance)
    // that nonetheless accumulate, over many frames, to a total
    // displacement well past it.
    void testSnapReleasesAfterSufficientCumulativeMovement()
    {
        QGraphicsScene scene;
        QGraphicsView view(&scene);

        AlignTestNode primary(QStringLiteral("primary"), QRectF(0, 0, 100, 60));
        AlignTestNode other(QStringLiteral("other"), QRectF(300, 0, 100, 60));
        scene.addItem(&primary);
        scene.addItem(&other);

        Canvas::CanvasAlignmentStrategy strategy;
        strategy.endDrag(); // fresh-drag state, mirroring CanvasScene::onDragBegan

        // Frame 1: land exactly on the snap candidate (right edge -> 200).
        QPointF result = strategy.align(&primary, QPointF(200, 0), {&primary, &other}).position;
        QCOMPARE(result.x(), 200.0);
        primary.setPos(result); // SelectMoveTool applies this before the next frame

        // Many subsequent frames, each nudging by only 3 units (well under
        // the 15-unit tolerance) but consistently in the same direction --
        // 3 * 10 = 30 units of true total movement, which must eventually
        // escape a 15-unit tolerance zone. Before the fix, every single
        // frame re-snapped back to x()==200 and primary.pos() never moved.
        bool escaped = false;
        for (int i = 0; i < 10; ++i) {
            const QPointF proposed = primary.pos() + QPointF(3, 0);
            result = strategy.align(&primary, proposed, {&primary, &other}).position;
            primary.setPos(result);
            if (result.x() > 205.0) { // clearly off the snap candidate
                escaped = true;
                break;
            }
        }

        QVERIFY2(escaped, "drag never escaped the snap point -- stuck-forever regression");
    }

    void testShiftLocksDominantAxisPerFrame()
    {
        QGraphicsScene scene;
        QGraphicsView view(&scene);

        AlignTestNode primary(QStringLiteral("primary"), QRectF(0, 0, 100, 60));
        scene.addItem(&primary);

        FakeModifierStrategy strategy;
        strategy.setFakeModifiers(Qt::ShiftModifier);
        strategy.setSnapToGridEnabled(false);
        strategy.setSnapToObjectsEnabled(false);

        // primary currently at (0,0); frame delta (10, 2) -> x dominant ->
        // y locked back to primary's current y (0).
        const QPointF proposed(10, 2);
        auto result = strategy.align(&primary, proposed, {&primary});
        QCOMPARE(result.position.x(), 10.0);
        QCOMPARE(result.position.y(), 0.0);
    }
};

QTEST_MAIN(TestCanvasAlignment)
#include "tst_canvas_alignment.moc"
