// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "canvas/CanvasBezierStrategy.h"

class TestCanvasBezierStrategy : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Appendix A: control offset = clamp(dist/2, 70, 150) along
    // outwardDirection; path is inset 7px from each face with a straight
    // stub filling the remainder.
    void testBezierMatchesObsidianConstants()
    {
        Graffodil::Anchor source;
        source.id = QStringLiteral("right");
        source.scenePos = QPointF(100, 0);
        source.outwardDirection = QPointF(1, 0);

        Graffodil::Anchor target;
        target.id = QStringLiteral("left");
        target.scenePos = QPointF(300, 0);
        target.outwardDirection = QPointF(-1, 0);

        // dist = 200 -> clamp(100, 70, 150) = 100 (within range, unclamped).
        Canvas::CanvasBezierStrategy strategy;
        const QPainterPath path = strategy.computePath(source, target);

        // moveTo(source), lineTo(sourceInset), cubicTo(ctrl1, ctrl2, targetInset),
        // lineTo(target) -> 5 elements: MoveTo, LineTo, CurveTo + 2 CurveToData.
        QCOMPARE(path.elementCount(), 6);

        const auto e0 = path.elementAt(0); // moveTo source
        QVERIFY(qFuzzyCompare(e0.x, 100.0));
        QVERIFY(qFuzzyCompare(e0.y, 0.0));
        QCOMPARE(e0.type, QPainterPath::MoveToElement);

        const auto e1 = path.elementAt(1); // lineTo sourceInset (100+7, 0)
        QVERIFY(qFuzzyCompare(e1.x, 107.0));
        QVERIFY(qFuzzyCompare(e1.y, 0.0));
        QCOMPARE(e1.type, QPainterPath::LineToElement);

        // cubicTo: element 2 = ctrl1, element 3 = ctrl2, element 4 = targetInset.
        const auto ctrl1 = path.elementAt(2);
        QVERIFY(qFuzzyCompare(ctrl1.x, 200.0)); // 100 + 1*100
        QVERIFY(qFuzzyCompare(ctrl1.y, 0.0));

        const auto ctrl2 = path.elementAt(3);
        QVERIFY(qFuzzyCompare(ctrl2.x, 200.0)); // 300 + (-1)*100
        QVERIFY(qFuzzyCompare(ctrl2.y, 0.0));

        const auto targetInset = path.elementAt(4);
        QVERIFY(qFuzzyCompare(targetInset.x, 293.0)); // 300 + (-1)*7
        QVERIFY(qFuzzyCompare(targetInset.y, 0.0));

        const auto e5 = path.elementAt(5); // lineTo target face
        QVERIFY(qFuzzyCompare(e5.x, 300.0));
        QVERIFY(qFuzzyCompare(e5.y, 0.0));
        QCOMPARE(e5.type, QPainterPath::LineToElement);
    }

    // Curvature clamps to 70 below dist=140, and 150 above dist=300.
    void testCurvatureClampsToRange()
    {
        Graffodil::Anchor source;
        source.scenePos = QPointF(0, 0);
        source.outwardDirection = QPointF(1, 0);

        Graffodil::Anchor tooClose;
        tooClose.scenePos = QPointF(40, 0); // dist=40 -> dist/2=20 -> clamp to 70
        tooClose.outwardDirection = QPointF(-1, 0);

        Canvas::CanvasBezierStrategy strategy;
        const QPainterPath closePath = strategy.computePath(source, tooClose);
        const auto ctrl1 = closePath.elementAt(2);
        QVERIFY(qFuzzyCompare(ctrl1.x, 70.0)); // 0 + 1*70

        Graffodil::Anchor farAway;
        farAway.scenePos = QPointF(1000, 0); // dist=1000 -> dist/2=500 -> clamp to 150
        farAway.outwardDirection = QPointF(-1, 0);
        const QPainterPath farPath = strategy.computePath(source, farAway);
        const auto ctrl1Far = farPath.elementAt(2);
        QVERIFY(qFuzzyCompare(ctrl1Far.x, 150.0)); // 0 + 1*150
    }
};

QTEST_MAIN(TestCanvasBezierStrategy)
#include "tst_canvasbezierstrategy.moc"
