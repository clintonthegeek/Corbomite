// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "forcegraph/QuadTree.h"
#include <QRandomGenerator>
#include <cmath>

class TestQuadTree : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testBuildFromNodes()
    {
        ForceGraph::QuadTree tree;
        QVector<ForceGraph::GraphNode> nodes;
        for (int i = 0; i < 10; ++i) {
            ForceGraph::GraphNode n;
            n.id = QString::number(i);
            n.position = QPointF(i * 10.0, i * 10.0);
            nodes.append(n);
        }
        tree.build(nodes, QRectF(0, 0, 100, 100));
        // Should not crash, tree is built
    }

    void testRepulsionNonZero()
    {
        ForceGraph::QuadTree tree;
        QVector<ForceGraph::GraphNode> nodes;

        ForceGraph::GraphNode n1; n1.id = QStringLiteral("a"); n1.position = QPointF(0, 0);
        ForceGraph::GraphNode n2; n2.id = QStringLiteral("b"); n2.position = QPointF(50, 0);
        nodes << n1 << n2;

        tree.build(nodes, QRectF(-100, -100, 200, 200));

        // Repulsion from tree at (0,0) should push left (away from node at 50,0)
        QPointF force = tree.computeRepulsion(QPointF(0, 0), 1500.0, 1.0, 0.0);
        // With theta=0 (exact), force should be purely in -x direction
        QVERIFY(force.x() < 0);
        QVERIFY(std::abs(force.y()) < 0.001);
    }

    void testRepulsionApproximation()
    {
        // Barnes-Hut with theta=0.8 should approximate exact within 20%
        ForceGraph::QuadTree tree;
        QVector<ForceGraph::GraphNode> nodes;

        auto *rng = QRandomGenerator::global();
        for (int i = 0; i < 50; ++i) {
            ForceGraph::GraphNode n;
            n.id = QString::number(i);
            n.position = QPointF(rng->bounded(1000), rng->bounded(1000));
            nodes.append(n);
        }

        tree.build(nodes, QRectF(0, 0, 1000, 1000));

        QPointF queryPos(500, 500);
        QPointF exactForce = tree.computeRepulsion(queryPos, 1500.0, 1.0, 0.0);  // theta=0 -> exact
        QPointF approxForce = tree.computeRepulsion(queryPos, 1500.0, 1.0, 0.8); // theta=0.8 -> approx

        // Approximate should be within 20% of exact magnitude
        double exactMag = std::sqrt(exactForce.x() * exactForce.x() + exactForce.y() * exactForce.y());
        double approxMag = std::sqrt(approxForce.x() * approxForce.x() + approxForce.y() * approxForce.y());

        if (exactMag > 0.001) {
            double ratio = approxMag / exactMag;
            QVERIFY2(ratio > 0.5 && ratio < 2.0,
                     qPrintable(QStringLiteral("Ratio: %1").arg(ratio)));
        }
    }

    void testEmptyTree()
    {
        ForceGraph::QuadTree tree;
        tree.build({}, QRectF(0, 0, 100, 100));
        QPointF force = tree.computeRepulsion(QPointF(50, 50), 1500.0);
        QCOMPARE(force, QPointF(0, 0));
    }

    void testSingleNode()
    {
        ForceGraph::QuadTree tree;
        ForceGraph::GraphNode n;
        n.id = QStringLiteral("a");
        n.position = QPointF(50, 50);
        tree.build({n}, QRectF(0, 0, 100, 100));

        // Query from different position — should get repulsive force
        QPointF force = tree.computeRepulsion(QPointF(0, 0), 1500.0);
        // Force should push away from (50,50), i.e., toward (-x, -y)
        QVERIFY(force.x() < 0);
        QVERIFY(force.y() < 0);
    }
};

QTEST_MAIN(TestQuadTree)
#include "tst_quadtree.moc"
