// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <cmath>
#include "forcegraph/ForceLayoutEngine.h"

class TestForceLayout : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testTwoNodesConverge()
    {
        ForceGraph::ForceLayoutEngine engine;

        ForceGraph::GraphNode n1; n1.id = QStringLiteral("a"); n1.position = QPointF(0, 0);
        ForceGraph::GraphNode n2; n2.id = QStringLiteral("b"); n2.position = QPointF(200, 0);
        engine.setNodes({n1, n2});

        ForceGraph::GraphEdge e; e.sourceId = QStringLiteral("a"); e.targetId = QStringLiteral("b");
        engine.setEdges({e});
        engine.setLinkDistance(100.0);

        // Run many iterations
        for (int i = 0; i < 200; ++i) engine.step();

        auto nodes = engine.nodes();
        double dist = std::sqrt(
            std::pow(nodes[0].position.x() - nodes[1].position.x(), 2) +
            std::pow(nodes[0].position.y() - nodes[1].position.y(), 2));

        // Should converge near linkDistance (within 30%)
        QVERIFY2(dist > 70 && dist < 130,
                 qPrintable(QStringLiteral("Distance: %1").arg(dist)));
    }

    void testRepulsionSpreadsNodes()
    {
        ForceGraph::ForceLayoutEngine engine;

        QVector<ForceGraph::GraphNode> nodes;
        for (int i = 0; i < 5; ++i) {
            ForceGraph::GraphNode n;
            n.id = QString::number(i);
            n.position = QPointF(0, 0); // All at origin
            nodes.append(n);
        }
        engine.setNodes(nodes);
        // No edges — pure repulsion

        for (int i = 0; i < 100; ++i) engine.step();

        // Nodes should have spread out from origin
        auto result = engine.nodes();
        double maxDist = 0;
        for (const auto &n : result) {
            double d = std::sqrt(n.position.x() * n.position.x() + n.position.y() * n.position.y());
            maxDist = std::max(maxDist, d);
        }
        QVERIFY2(maxDist > 10, qPrintable(QStringLiteral("Max distance: %1").arg(maxDist)));
    }

    void testConvergenceDetection()
    {
        ForceGraph::ForceLayoutEngine engine;

        ForceGraph::GraphNode n1; n1.id = QStringLiteral("a"); n1.position = QPointF(0, 0);
        ForceGraph::GraphNode n2; n2.id = QStringLiteral("b"); n2.position = QPointF(100, 0);
        engine.setNodes({n1, n2});

        ForceGraph::GraphEdge e; e.sourceId = QStringLiteral("a"); e.targetId = QStringLiteral("b");
        engine.setEdges({e});

        QVERIFY(!engine.isStable());

        for (int i = 0; i < 500; ++i) engine.step();

        QVERIFY(engine.isStable());
    }

    void testPinnedNodeStays()
    {
        ForceGraph::ForceLayoutEngine engine;

        ForceGraph::GraphNode n1; n1.id = QStringLiteral("a"); n1.position = QPointF(0, 0);
        ForceGraph::GraphNode n2; n2.id = QStringLiteral("b"); n2.position = QPointF(200, 0);
        engine.setNodes({n1, n2});

        engine.pinNode(QStringLiteral("a"), QPointF(0, 0));

        for (int i = 0; i < 100; ++i) engine.step();

        auto nodes = engine.nodes();
        // Pinned node should not have moved
        QCOMPARE(nodes[0].position, QPointF(0, 0));
        // Unpinned node may have moved
    }

    void testEmptyGraph()
    {
        ForceGraph::ForceLayoutEngine engine;
        engine.step(); // Should not crash
        QCOMPARE(engine.nodeCount(), 0);
    }

    void testSingleNode()
    {
        ForceGraph::ForceLayoutEngine engine;
        ForceGraph::GraphNode n; n.id = QStringLiteral("a"); n.position = QPointF(50, 50);
        engine.setNodes({n});

        for (int i = 0; i < 50; ++i) engine.step();

        // Single node with center force should drift toward origin
        auto nodes = engine.nodes();
        double dist = std::sqrt(nodes[0].position.x() * nodes[0].position.x() +
                                nodes[0].position.y() * nodes[0].position.y());
        QVERIFY(dist < 50); // Closer to origin than start
    }

    void testParameterEffects()
    {
        // Higher repel force should produce wider spread
        auto runWithRepel = [](double repel) -> double {
            ForceGraph::ForceLayoutEngine engine;
            engine.setRepelForce(repel);

            QVector<ForceGraph::GraphNode> nodes;
            for (int i = 0; i < 5; ++i) {
                ForceGraph::GraphNode n;
                n.id = QString::number(i);
                n.position = QPointF(i * 10, 0);
                nodes.append(n);
            }
            engine.setNodes(nodes);

            for (int i = 0; i < 200; ++i) engine.step();

            auto result = engine.nodes();
            double maxDist = 0;
            for (const auto &n : result) {
                double d = std::sqrt(n.position.x() * n.position.x() + n.position.y() * n.position.y());
                maxDist = std::max(maxDist, d);
            }
            return maxDist;
        };

        double spreadLow = runWithRepel(500.0);
        double spreadHigh = runWithRepel(3000.0);
        QVERIFY2(spreadHigh > spreadLow,
                 qPrintable(QStringLiteral("Low: %1, High: %2").arg(spreadLow).arg(spreadHigh)));
    }
};

QTEST_MAIN(TestForceLayout)
#include "tst_forcelayout.moc"
