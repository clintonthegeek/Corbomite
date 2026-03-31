// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include "canvas/CanvasDocument.h"

class TestCanvasDocument : public QObject {
    Q_OBJECT

    QJsonObject sampleJson()
    {
        return QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"n1","type":"text","x":0,"y":0,"width":250,"height":60,"text":"Hello **world**"},
                {"id":"n2","type":"text","x":300,"y":0,"width":200,"height":80,"color":"1"},
                {"id":"g1","type":"group","x":-50,"y":-50,"width":600,"height":200,"label":"My Group"}
            ],
            "edges": [
                {"id":"e1","fromNode":"n1","toNode":"n2","fromSide":"right","toSide":"left","label":"relates to"}
            ]
        })").object();
    }

private Q_SLOTS:
    void testLoadJson()
    {
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(sampleJson()));
        QCOMPARE(doc.nodes().size(), 3);
        QCOMPARE(doc.edges().size(), 1);
    }

    void testTextNodeFields()
    {
        Canvas::CanvasDocument doc;
        doc.loadFromJson(sampleJson());
        auto node = doc.node(QStringLiteral("n1"));
        QCOMPARE(node.type, Canvas::NodeType::Text);
        QCOMPARE(node.x, 0);
        QCOMPARE(node.y, 0);
        QCOMPARE(node.width, 250);
        QCOMPARE(node.height, 60);
        QCOMPARE(node.text, QStringLiteral("Hello **world**"));
    }

    void testGroupNodeFields()
    {
        Canvas::CanvasDocument doc;
        doc.loadFromJson(sampleJson());
        auto node = doc.node(QStringLiteral("g1"));
        QCOMPARE(node.type, Canvas::NodeType::Group);
        QCOMPARE(node.label, QStringLiteral("My Group"));
    }

    void testEdgeFields()
    {
        Canvas::CanvasDocument doc;
        doc.loadFromJson(sampleJson());
        auto edge = doc.edge(QStringLiteral("e1"));
        QCOMPARE(edge.fromNode, QStringLiteral("n1"));
        QCOMPARE(edge.toNode, QStringLiteral("n2"));
        QCOMPARE(edge.fromSide, Canvas::Side::Right);
        QCOMPARE(edge.toSide, Canvas::Side::Left);
        QCOMPARE(edge.toEnd, Canvas::EndType::Arrow);
        QCOMPARE(edge.label, QStringLiteral("relates to"));
    }

    void testRoundTrip()
    {
        Canvas::CanvasDocument doc1;
        doc1.loadFromJson(sampleJson());
        QJsonObject json = doc1.toJson();

        Canvas::CanvasDocument doc2;
        doc2.loadFromJson(json);
        QCOMPARE(doc2.nodes().size(), doc1.nodes().size());
        QCOMPARE(doc2.edges().size(), doc1.edges().size());

        auto n1 = doc2.node(QStringLiteral("n1"));
        QCOMPARE(n1.text, QStringLiteral("Hello **world**"));
    }

    void testEmptyDocument()
    {
        Canvas::CanvasDocument doc;
        auto json = doc.toJson();
        QVERIFY(json.contains(QStringLiteral("nodes")));
        QVERIFY(json.contains(QStringLiteral("edges")));
        QCOMPARE(json[QStringLiteral("nodes")].toArray().size(), 0);
        QCOMPARE(json[QStringLiteral("edges")].toArray().size(), 0);
    }

    void testAddNodeSignal()
    {
        Canvas::CanvasDocument doc;
        QSignalSpy spy(&doc, &Canvas::CanvasDocument::nodeAdded);

        Canvas::CanvasNode node;
        node.id = QStringLiteral("test");
        node.type = Canvas::NodeType::Text;
        doc.addNode(node);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("test"));
        QVERIFY(doc.hasNode(QStringLiteral("test")));
    }

    void testRemoveNodeRemovesEdges()
    {
        Canvas::CanvasDocument doc;
        doc.loadFromJson(sampleJson());
        QCOMPARE(doc.edges().size(), 1);

        doc.removeNode(QStringLiteral("n1"));
        QVERIFY(!doc.hasNode(QStringLiteral("n1")));
        QCOMPARE(doc.edges().size(), 0); // Edge connected to n1 removed
    }

    void testColorEncoding()
    {
        QCOMPARE(Canvas::colorFromCanvasColor(QStringLiteral("1")), QColor(233, 49, 71));
        QCOMPARE(Canvas::colorFromCanvasColor(QStringLiteral("6")), QColor(120, 82, 238));
        QCOMPARE(Canvas::colorFromCanvasColor(QStringLiteral("#FF0000")), QColor(255, 0, 0));
        QVERIFY(!Canvas::colorFromCanvasColor(QString()).isValid());
    }

    void testGenerateId()
    {
        QString id1 = Canvas::CanvasDocument::generateId();
        QString id2 = Canvas::CanvasDocument::generateId();
        QCOMPARE(id1.length(), 16);
        QVERIFY(id1 != id2);
    }

    void testModifiedState()
    {
        Canvas::CanvasDocument doc;
        QVERIFY(!doc.isModified());

        Canvas::CanvasNode node;
        node.id = QStringLiteral("test");
        doc.addNode(node);
        QVERIFY(doc.isModified());

        doc.setModified(false);
        QVERIFY(!doc.isModified());
    }

    void testFileRoundTrip()
    {
        QTemporaryDir tmp;
        QString path = tmp.path() + "/test.canvas";

        Canvas::CanvasDocument doc1;
        doc1.loadFromJson(sampleJson());
        QVERIFY(doc1.saveToFile(path));

        Canvas::CanvasDocument doc2;
        QVERIFY(doc2.loadFromFile(path));
        QCOMPARE(doc2.nodes().size(), 3);
        QCOMPARE(doc2.edges().size(), 1);
    }

    void testEdgesForNode()
    {
        Canvas::CanvasDocument doc;
        doc.loadFromJson(sampleJson());

        auto edges = doc.edgesForNode(QStringLiteral("n1"));
        QCOMPARE(edges.size(), 1);
        QCOMPARE(edges.at(0).id, QStringLiteral("e1"));

        auto noEdges = doc.edgesForNode(QStringLiteral("g1"));
        QCOMPARE(noEdges.size(), 0);
    }

    void testEdgeDefaultToEnd()
    {
        // When toEnd is not specified in JSON, default should be "arrow"
        auto json = QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"a","type":"text","x":0,"y":0,"width":100,"height":50},
                {"id":"b","type":"text","x":200,"y":0,"width":100,"height":50}
            ],
            "edges": [
                {"id":"e","fromNode":"a","toNode":"b","fromSide":"right","toSide":"left"}
            ]
        })").object();

        Canvas::CanvasDocument doc;
        doc.loadFromJson(json);
        auto edge = doc.edge(QStringLiteral("e"));
        QCOMPARE(edge.toEnd, Canvas::EndType::Arrow);
        QCOMPARE(edge.fromEnd, Canvas::EndType::None);
    }
};

QTEST_MAIN(TestCanvasDocument)
#include "tst_canvasdocument.moc"
