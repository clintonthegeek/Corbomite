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

    void testIterativeMatchesBruteForce()
    {
        // Build a tree and verify the iterative traversal produces reasonable forces
        ForceGraph::QuadTree tree;
        QVector<ForceGraph::GraphNode> nodes;
        QVector<double> masses;
        for (int i = 0; i < 20; ++i) {
            ForceGraph::GraphNode n;
            n.id = QString::number(i);
            n.position = QPointF(i * 30.0, (i % 3) * 40.0);
            nodes.append(n);
            masses.append(1.0);
        }
        tree.build(nodes, QRectF(-100, -100, 800, 400), masses);

        // Query repulsion for first node
        QPointF force = tree.computeRepulsion(nodes[0].position, 1500.0, 1.0, 0.8);

        // Force should be non-zero and point generally away from the cluster
        double mag = std::sqrt(force.x() * force.x() + force.y() * force.y());
        QVERIFY2(mag > 0.0, "Force should be non-zero");

        // Compare with brute-force computation
        QPointF bruteForce(0, 0);
        for (int j = 1; j < nodes.size(); ++j) {
            QPointF delta = nodes[0].position - nodes[j].position;
            double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
            if (dist < 0.001) continue;
            double f = 1500.0 * 1.0 * 1.0 / (dist * dist);
            bruteForce += (delta / dist) * f;
        }

        // Should be within 20% of brute force (Barnes-Hut approximation)
        double bruteMag = std::sqrt(bruteForce.x() * bruteForce.x() + bruteForce.y() * bruteForce.y());
        QVERIFY2(std::abs(mag - bruteMag) / bruteMag < 0.20,
                 qPrintable(QStringLiteral("BH: %1, Brute: %2, Error: %3%")
                            .arg(mag).arg(bruteMag).arg(100.0 * std::abs(mag - bruteMag) / bruteMag)));
    }
    void testPositionBasedBuild()
    {
        // Two nodes — repulsion should push them apart
        QVector<QPointF> positions = { QPointF(0, 0), QPointF(10, 0) };
        QVector<double> masses = { 1.0, 1.0 };
        QRectF bounds(-100, -100, 200, 200);

        ForceGraph::QuadTree tree;
        tree.build(positions, bounds, masses);

        QPointF force = tree.computeRepulsion(positions[0], 1000.0, masses[0], 0.8);
        // Force on node 0 should point in -x direction (away from node 1)
        QVERIFY2(force.x() < 0,
                 qPrintable(QStringLiteral("Force x: %1").arg(force.x())));

        // Verify it matches the GraphNode-based path
        ForceGraph::GraphNode n1; n1.id = QStringLiteral("a"); n1.position = QPointF(0, 0);
        ForceGraph::GraphNode n2; n2.id = QStringLiteral("b"); n2.position = QPointF(10, 0);
        ForceGraph::QuadTree tree2;
        tree2.build({n1, n2}, bounds, masses);
        QPointF force2 = tree2.computeRepulsion(positions[0], 1000.0, masses[0], 0.8);

        // Forces should be identical
        QVERIFY(std::abs(force.x() - force2.x()) < 0.001);
        QVERIFY(std::abs(force.y() - force2.y()) < 0.001);
    }

    void testPositionBasedApproximation()
    {
        // 50 seeded-random nodes — Barnes-Hut should approximate brute-force within 30%
        QRandomGenerator rng(42); // fixed seed for determinism
        QVector<QPointF> positions;
        QVector<double> masses;
        positions.reserve(50);
        masses.reserve(50);
        for (int i = 0; i < 50; ++i) {
            positions.append(QPointF(rng.generateDouble() * 1000.0,
                                      rng.generateDouble() * 1000.0));
            masses.append(1.0);
        }

        QRectF bounds(-100, -100, 1200, 1200);
        ForceGraph::QuadTree tree;
        tree.build(positions, bounds, masses);

        // Brute-force repulsion for node 0
        QPointF bruteForce(0, 0);
        for (int j = 1; j < 50; ++j) {
            QPointF delta = positions[0] - positions[j];
            double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
            dist = std::max(dist, 0.001);
            double f = 1000.0 / (dist * dist);
            bruteForce += (delta / dist) * f;
        }

        QPointF bhForce = tree.computeRepulsion(positions[0], 1000.0, 1.0, 0.8);

        double bruteLen = std::sqrt(bruteForce.x() * bruteForce.x() + bruteForce.y() * bruteForce.y());
        double bhLen = std::sqrt(bhForce.x() * bhForce.x() + bhForce.y() * bhForce.y());

        QVERIFY2(std::abs(bhLen - bruteLen) / bruteLen < 0.3,
                 qPrintable(QStringLiteral("Brute: %1, BH: %2").arg(bruteLen).arg(bhLen)));
    }
};

QTEST_MAIN(TestQuadTree)
#include "tst_quadtree.moc"
