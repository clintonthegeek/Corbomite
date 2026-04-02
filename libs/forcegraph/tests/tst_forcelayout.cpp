// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSet>
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
    void testBFSPlacementProducesLayeredLayout()
    {
        // Build a linear chain: 0 — 1 — 2 — 3 — 4
        ForceGraph::ForceLayoutEngine engine;

        QVector<ForceGraph::GraphNode> nodes;
        for (int i = 0; i < 5; ++i) {
            ForceGraph::GraphNode n;
            n.id = QString::number(i);
            n.position = QPointF(0, 0); // All at origin
            nodes.append(n);
        }
        engine.setNodes(nodes);

        QVector<ForceGraph::GraphEdge> edges;
        for (int i = 0; i < 4; ++i) {
            ForceGraph::GraphEdge e;
            e.sourceId = QString::number(i);
            e.targetId = QString::number(i + 1);
            edges.append(e);
        }
        engine.setEdges(edges);

        // Run a few steps to trigger bfsInitialPlacement
        engine.step();

        auto result = engine.nodes();

        // Nodes at different BFS depths should be at different distances from the first node
        // After BFS placement, they should be at different radii
        QSet<int> uniqueDistanceBuckets;
        for (const auto &n : result) {
            double dist = std::sqrt(n.position.x() * n.position.x() + n.position.y() * n.position.y());
            // Bucket by rough distance (multiples of linkDistance/2)
            int bucket = static_cast<int>(dist / 50.0);
            uniqueDistanceBuckets.insert(bucket);
        }
        // A chain of 5 nodes should produce at least 3 distinct distance buckets
        QVERIFY2(uniqueDistanceBuckets.size() >= 3,
                 qPrintable(QStringLiteral("Only %1 distance buckets for chain of 5").arg(uniqueDistanceBuckets.size())));
    }

    void testDegreeWeightedRepulsionSpreadsHubs()
    {
        ForceGraph::ForceLayoutEngine engine;

        // Star graph: hub "h" connected to a, b, c, d, e
        QVector<ForceGraph::GraphNode> nodes;
        ForceGraph::GraphNode hub; hub.id = QStringLiteral("h"); hub.position = QPointF(0, 0);
        nodes.append(hub);
        for (const auto &id : {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"),
                               QStringLiteral("d"), QStringLiteral("e")}) {
            ForceGraph::GraphNode n;
            n.id = id;
            n.position = QPointF(0, 0);
            nodes.append(n);
        }
        engine.setNodes(nodes);

        QVector<ForceGraph::GraphEdge> edges;
        for (const auto &id : {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"),
                               QStringLiteral("d"), QStringLiteral("e")}) {
            edges.append({QStringLiteral("h"), id});
        }
        engine.setEdges(edges);

        for (int i = 0; i < 200; ++i) engine.step();

        auto result = engine.nodes();
        // Hub (degree 5) should be near center due to gravity
        // Leaves should be spread around it
        double avgLeafDist = 0;
        for (int i = 1; i < result.size(); ++i) {
            double d = std::sqrt(
                std::pow(result[i].position.x() - result[0].position.x(), 2) +
                std::pow(result[i].position.y() - result[0].position.y(), 2));
            avgLeafDist += d;
        }
        avgLeafDist /= 5.0;

        // Leaves should be spread out from hub
        QVERIFY2(avgLeafDist > 30,
                 qPrintable(QStringLiteral("Avg leaf dist from hub: %1").arg(avgLeafDist)));
    }

    void testBFSPlacementDisconnectedComponents()
    {
        // Two disconnected pairs: {a-b} and {c-d}
        ForceGraph::ForceLayoutEngine engine;

        QVector<ForceGraph::GraphNode> nodes;
        for (const auto &id : {QStringLiteral("a"), QStringLiteral("b"),
                               QStringLiteral("c"), QStringLiteral("d")}) {
            ForceGraph::GraphNode n;
            n.id = id;
            n.position = QPointF(0, 0);
            nodes.append(n);
        }
        engine.setNodes(nodes);

        QVector<ForceGraph::GraphEdge> edges;
        ForceGraph::GraphEdge e1; e1.sourceId = QStringLiteral("a"); e1.targetId = QStringLiteral("b");
        ForceGraph::GraphEdge e2; e2.sourceId = QStringLiteral("c"); e2.targetId = QStringLiteral("d");
        edges << e1 << e2;
        engine.setEdges(edges);

        // Run one step to trigger BFS placement
        engine.step();

        auto result = engine.nodes();

        // Find center of each component
        QPointF center1, center2;
        for (const auto &n : result) {
            if (n.id == QStringLiteral("a") || n.id == QStringLiteral("b")) {
                center1 += n.position;
            } else {
                center2 += n.position;
            }
        }
        center1 /= 2.0;
        center2 /= 2.0;

        // Components should be placed apart (offset by at least gap = 3 * linkDistance)
        double separation = std::sqrt(
            std::pow(center1.x() - center2.x(), 2) +
            std::pow(center1.y() - center2.y(), 2));
        QVERIFY2(separation > 100.0,
                 qPrintable(QStringLiteral("Component separation: %1").arg(separation)));
    }
};

QTEST_MAIN(TestForceLayout)
#include "tst_forcelayout.moc"
