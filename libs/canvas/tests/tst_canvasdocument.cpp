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

    void testUpdateEdge()
    {
        Canvas::CanvasDocument doc;
        doc.loadFromJson(sampleJson());

        auto edge = doc.edge(QStringLiteral("e1"));
        QCOMPARE(edge.label, QStringLiteral("relates to"));

        edge.label = QStringLiteral("connects");
        doc.updateEdge(edge);

        auto updated = doc.edge(QStringLiteral("e1"));
        QCOMPARE(updated.label, QStringLiteral("connects"));
    }

    void testUpdateEdgeEmitsSignal()
    {
        Canvas::CanvasDocument doc;
        doc.loadFromJson(sampleJson());

        QSignalSpy spy(&doc, &Canvas::CanvasDocument::edgeChanged);
        auto edge = doc.edge(QStringLiteral("e1"));
        edge.label = QStringLiteral("test");
        doc.updateEdge(edge);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("e1"));
    }

    void testUnknownNodeFieldsPreservedOnRoundTrip()
    {
        // Obsidian / plugins / newer versions can write fields we don't model.
        // Round-trip must preserve them on each node (Obsidian uses ...unknownData
        // rest-spread; we must do the same).
        auto json = QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"n1","type":"text","x":0,"y":0,"width":100,"height":50,
                 "text":"hi",
                 "styleAttributes":{"a":1,"b":"two"},
                 "futureField":42}
            ],
            "edges": []
        })").object();

        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(json));
        auto out = doc.toJson();
        auto outNode = out[QStringLiteral("nodes")].toArray().at(0).toObject();
        QVERIFY(outNode.contains(QStringLiteral("styleAttributes")));
        QCOMPARE(outNode[QStringLiteral("styleAttributes")].toObject()[QStringLiteral("a")].toInt(), 1);
        QCOMPARE(outNode[QStringLiteral("styleAttributes")].toObject()[QStringLiteral("b")].toString(),
                 QStringLiteral("two"));
        QVERIFY(outNode.contains(QStringLiteral("futureField")));
        QCOMPARE(outNode[QStringLiteral("futureField")].toInt(), 42);
    }

    void testUnknownEdgeFieldsPreservedOnRoundTrip()
    {
        auto json = QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"a","type":"text","x":0,"y":0,"width":100,"height":50},
                {"id":"b","type":"text","x":200,"y":0,"width":100,"height":50}
            ],
            "edges": [
                {"id":"e","fromNode":"a","toNode":"b","fromSide":"right","toSide":"left",
                 "weight":3,"meta":{"flag":true}}
            ]
        })").object();

        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(json));
        auto out = doc.toJson();
        auto outEdge = out[QStringLiteral("edges")].toArray().at(0).toObject();
        QCOMPARE(outEdge[QStringLiteral("weight")].toInt(), 3);
        QCOMPARE(outEdge[QStringLiteral("meta")].toObject()[QStringLiteral("flag")].toBool(), true);
    }

    void testUnknownTopLevelFieldsPreservedOnRoundTrip()
    {
        auto json = QJsonDocument::fromJson(R"({
            "nodes": [],
            "edges": [],
            "metadata": {"version": "1.2"},
            "customExt": [1,2,3]
        })").object();

        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(json));
        auto out = doc.toJson();
        QCOMPARE(out[QStringLiteral("metadata")].toObject()[QStringLiteral("version")].toString(),
                 QStringLiteral("1.2"));
        QCOMPARE(out[QStringLiteral("customExt")].toArray().size(), 3);
    }

    void testDefaultValuesOmittedOnWrite()
    {
        // Obsidian omits default values on write to keep cross-app diffs small:
        //   color/label/subpath empty → omitted
        //   fromEnd == "none"        → omitted
        //   toEnd   == "arrow"       → omitted
        //   backgroundStyle == "cover" → omitted
        // See docs/obsidian-audit/domains/canvas.md §3 invariant 2.
        Canvas::CanvasDocument doc;

        Canvas::CanvasNode group;
        group.id = QStringLiteral("g");
        group.type = Canvas::NodeType::Group;
        group.backgroundStyle = QStringLiteral("cover"); // Obsidian default
        doc.addNode(group);

        Canvas::CanvasNode file;
        file.id = QStringLiteral("f");
        file.type = Canvas::NodeType::File;
        file.file = QStringLiteral("notes/x.md");
        // subpath, color empty
        doc.addNode(file);

        Canvas::CanvasNode text;
        text.id = QStringLiteral("t");
        text.type = Canvas::NodeType::Text;
        text.text = QStringLiteral("hi");
        // color empty
        doc.addNode(text);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("e");
        edge.fromNode = QStringLiteral("t");
        edge.toNode = QStringLiteral("f");
        // fromEnd default None, toEnd default Arrow, color/label empty
        doc.addEdge(edge);

        auto json = doc.toJson();

        // Find each emitted object by id.
        QHash<QString, QJsonObject> outNodes;
        for (auto v : json[QStringLiteral("nodes")].toArray()) {
            auto o = v.toObject();
            outNodes.insert(o[QStringLiteral("id")].toString(), o);
        }
        auto outEdge = json[QStringLiteral("edges")].toArray().at(0).toObject();

        QVERIFY2(!outNodes[QStringLiteral("g")].contains(QStringLiteral("backgroundStyle")),
                 "Group node should omit backgroundStyle when it equals the Obsidian default 'cover'");
        QVERIFY(!outNodes[QStringLiteral("f")].contains(QStringLiteral("subpath")));
        QVERIFY(!outNodes[QStringLiteral("f")].contains(QStringLiteral("color")));
        QVERIFY(!outNodes[QStringLiteral("t")].contains(QStringLiteral("color")));
        QVERIFY(!outEdge.contains(QStringLiteral("fromEnd")));
        QVERIFY(!outEdge.contains(QStringLiteral("toEnd")));
        QVERIFY(!outEdge.contains(QStringLiteral("color")));
        QVERIFY(!outEdge.contains(QStringLiteral("label")));
    }

    void testNonDefaultBackgroundStyleEmitted()
    {
        // Inverse of the cover-omission rule: anything else must still be written.
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode g;
        g.id = QStringLiteral("g");
        g.type = Canvas::NodeType::Group;
        g.backgroundStyle = QStringLiteral("ratio");
        doc.addNode(g);

        auto out = doc.toJson()[QStringLiteral("nodes")].toArray().at(0).toObject();
        QCOMPARE(out[QStringLiteral("backgroundStyle")].toString(), QStringLiteral("ratio"));
    }

    void testMissingFromSideSelfHealsHorizontal()
    {
        // toNode lies due-right of fromNode → fromSide must resolve to "right".
        // V5 angular-sector picker; see canvas.md §3 invariant 5 + §8 invariant 10.
        auto json = QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"a","type":"text","x":0,"y":0,"width":100,"height":100},
                {"id":"b","type":"text","x":300,"y":0,"width":100,"height":100}
            ],
            "edges": [
                {"id":"e","fromNode":"a","toNode":"b","toSide":"left"}
            ]
        })").object();
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(json));
        QCOMPARE(doc.edge(QStringLiteral("e")).fromSide, Canvas::Side::Right);
    }

    void testMissingToSideSelfHealsHorizontal()
    {
        // toSide must point back toward fromNode (left), not the
        // sideFromString default of "right".
        auto json = QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"a","type":"text","x":0,"y":0,"width":100,"height":100},
                {"id":"b","type":"text","x":300,"y":0,"width":100,"height":100}
            ],
            "edges": [
                {"id":"e","fromNode":"a","toNode":"b","fromSide":"right"}
            ]
        })").object();
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(json));
        QCOMPARE(doc.edge(QStringLiteral("e")).toSide, Canvas::Side::Left);
    }

    void testMissingSidesSelfHealVertical()
    {
        auto json = QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"a","type":"text","x":0,"y":0,"width":100,"height":100},
                {"id":"b","type":"text","x":0,"y":300,"width":100,"height":100}
            ],
            "edges": [
                {"id":"e","fromNode":"a","toNode":"b"}
            ]
        })").object();
        Canvas::CanvasDocument doc;
        doc.loadFromJson(json);
        QCOMPARE(doc.edge(QStringLiteral("e")).fromSide, Canvas::Side::Bottom);
        QCOMPARE(doc.edge(QStringLiteral("e")).toSide,   Canvas::Side::Top);
    }

    void testHealedSidesPersistOnRoundTrip()
    {
        // The computed sides must be written back into JSON; otherwise the next
        // save still looks side-less to Obsidian and the diff ping-pongs.
        auto json = QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"a","type":"text","x":0,"y":0,"width":100,"height":100},
                {"id":"b","type":"text","x":300,"y":0,"width":100,"height":100}
            ],
            "edges": [
                {"id":"e","fromNode":"a","toNode":"b"}
            ]
        })").object();
        Canvas::CanvasDocument doc;
        doc.loadFromJson(json);
        auto out = doc.toJson()[QStringLiteral("edges")].toArray().at(0).toObject();
        QCOMPARE(out[QStringLiteral("fromSide")].toString(), QStringLiteral("right"));
        QCOMPARE(out[QStringLiteral("toSide")].toString(),   QStringLiteral("left"));
    }

    void testAspectRatioInformsAngularSector()
    {
        // Wide 200x100 nodes; offset (300, 200) center-to-center.
        // corner angle = atan2(50,100) ≈ 26.6°; offset angle ≈ 33.7° → vertical face.
        // dy > 0 → fromSide=Bottom; the reverse vector at toNode → toSide=Top.
        auto json = QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"a","type":"text","x":0,"y":0,"width":200,"height":100},
                {"id":"b","type":"text","x":200,"y":150,"width":200,"height":100}
            ],
            "edges": [
                {"id":"e","fromNode":"a","toNode":"b"}
            ]
        })").object();
        Canvas::CanvasDocument doc;
        doc.loadFromJson(json);
        QCOMPARE(doc.edge(QStringLiteral("e")).fromSide, Canvas::Side::Bottom);
        QCOMPARE(doc.edge(QStringLiteral("e")).toSide,   Canvas::Side::Top);
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
