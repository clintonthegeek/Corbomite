// SPDX-License-Identifier: GPL-3.0-or-later
//
// `.canvas` disk contract: byte parity with Obsidian 1.13.7's `$d`/`Zd`
// printer (app/own/0563-$d.js, app/own/0564-Zd.js) and the round-trip-loss
// rules from
// docs/obsidian-audit/addenda/2026-10-03-canvas-disk-contract-and-viewport-settings.md.
//
// Byte fixtures and their goldens live in canvas_diskcontract_data.h.
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include "canvas/CanvasDocument.h"
#include "canvas/CanvasJsonPrinter.h"
#include "canvas_diskcontract_data.h"

class TestCanvasDiskContract : public QObject {
    Q_OBJECT

    static QString writeFixture(const QByteArray &contents, const QString &name)
    {
        static QTemporaryDir *dir = new QTemporaryDir;
        const QString path = dir->path() + QLatin1Char('/') + name;
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(contents);
        return path;
    }

private Q_SLOTS:

    // ---------------------------------------------------------------- task 3

    void emptyCanvasMatchesObsidianBytes()
    {
        Canvas::CanvasDocument doc;
        QCOMPARE(doc.serializeObsidianStyle(),
                 QByteArray("{\n\t\"nodes\":[],\n\t\"edges\":[]\n}"));
        // $d is Zd(...).join("\n") — no trailing newline.
        QVERIFY(!doc.serializeObsidianStyle().endsWith('\n'));
    }

    void fullyModelledCanvasMatchesObsidianBytes()
    {
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromFile(writeFixture(canvasModelledFixture(), "modelled.canvas")));
        QCOMPARE(doc.serializeObsidianStyle(), canvasModelledGoldenBytes());
        QCOMPARE(doc.serializeObsidianStyle().endsWith('\n'), false);
    }

    void edgeKeyOrderPutsSidesBesideTheirOwnNode()
    {
        // getData() emits id, fromNode, fromSide, toNode, toSide — fromSide
        // before toNode, not the toNode-then-fromSide order this used to
        // write. Asserted on the bytes, because a QJsonObject cannot show key
        // order at all: Qt sorts it.
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"a","type":"text","x":0,"y":0,"width":10,"height":10,"text":""},
                {"id":"b","type":"text","x":90,"y":0,"width":10,"height":10,"text":""}
            ],
            "edges": [{"id":"e","fromNode":"a","fromSide":"right","toNode":"b","toSide":"left"}]
        })").object()));
        QVERIFY(doc.serializeObsidianStyle().contains(
            "\t\t{\"id\":\"e\",\"fromNode\":\"a\",\"fromSide\":\"right\","
            "\"toNode\":\"b\",\"toSide\":\"left\"}\n"));
    }

    void explicitDefaultValuesSurviveRoundTrip()
    {
        // Upstream's omit-lists move these keys into unknownData rather than
        // deleting them, so a file that already states a default keeps stating
        // it (addendum F5).
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"g","type":"group","x":0,"y":0,"width":10,"height":10,
                 "label":"","backgroundStyle":"cover"},
                {"id":"f","type":"file","x":0,"y":0,"width":10,"height":10,
                 "file":"n.md","subpath":""},
                {"id":"t","type":"text","x":0,"y":0,"width":10,"height":10,"text":"","color":""}
            ],
            "edges": [{"id":"e","fromNode":"t","toNode":"f","fromSide":"right","toSide":"left",
                       "fromEnd":"none","toEnd":"arrow","color":""}]
        })").object()));

        QHash<QString, QJsonObject> nodes;
        for (const auto &v : doc.toJson()[QStringLiteral("nodes")].toArray())
            nodes.insert(v.toObject()[QStringLiteral("id")].toString(), v.toObject());
        const auto edge = doc.toJson()[QStringLiteral("edges")].toArray().at(0).toObject();

        QCOMPARE(nodes[QStringLiteral("g")][QStringLiteral("backgroundStyle")].toString(),
                 QStringLiteral("cover"));
        QCOMPARE(nodes[QStringLiteral("g")][QStringLiteral("label")].toString(), QString());
        QVERIFY(nodes[QStringLiteral("g")].contains(QStringLiteral("label")));
        QCOMPARE(nodes[QStringLiteral("f")][QStringLiteral("subpath")].toString(), QString());
        QVERIFY(nodes[QStringLiteral("f")].contains(QStringLiteral("subpath")));
        QVERIFY(nodes[QStringLiteral("t")].contains(QStringLiteral("color")));
        QCOMPARE(edge[QStringLiteral("fromEnd")].toString(), QStringLiteral("none"));
        QCOMPARE(edge[QStringLiteral("toEnd")].toString(), QStringLiteral("arrow"));
        QVERIFY(edge.contains(QStringLiteral("color")));
        QCOMPARE(edge[QStringLiteral("color")].toString(), QString());
    }

    void orphanBackgroundStyleSurvivesWithoutBackground()
    {
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(R"({
            "nodes": [{"id":"g","type":"group","x":0,"y":0,"width":10,"height":10,
                       "backgroundStyle":"repeat"}],
            "edges": []
        })").object()));
        const auto g = doc.toJson()[QStringLiteral("nodes")].toArray().at(0).toObject();
        QCOMPARE(g[QStringLiteral("backgroundStyle")].toString(), QStringLiteral("repeat"));
    }

    void requiredFieldsAreAlwaysWritten()
    {
        // getData() assigns text / file / url unconditionally, so an empty card,
        // an unresolved file node and a bare link node each keep their key.
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"t","type":"text","x":0,"y":0,"width":10,"height":10,"text":""},
                {"id":"f","type":"file","x":0,"y":0,"width":10,"height":10,"file":""},
                {"id":"l","type":"link","x":0,"y":0,"width":10,"height":10,"url":""}
            ],
            "edges": []
        })").object()));
        QHash<QString, QJsonObject> nodes;
        for (const auto &v : doc.toJson()[QStringLiteral("nodes")].toArray())
            nodes.insert(v.toObject()[QStringLiteral("id")].toString(), v.toObject());
        QVERIFY(nodes[QStringLiteral("t")].contains(QStringLiteral("text")));
        QVERIFY(nodes[QStringLiteral("f")].contains(QStringLiteral("file")));
        QVERIFY(nodes[QStringLiteral("l")].contains(QStringLiteral("url")));
    }

    void nonStringRequiredFieldIsNotAdopted()
    {
        // String.isString(n) && setText(n) (app/own/2022-W7.js:372) — a
        // non-string text is ignored and the unconditional re-assignment then
        // writes "" over it.
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(
            R"({"nodes":[{"id":"t","type":"text","x":0,"y":0,"width":10,"height":10,"text":42}],
                "edges":[]})").object()));
        const auto t = doc.toJson()[QStringLiteral("nodes")].toArray().at(0).toObject();
        QCOMPARE(t[QStringLiteral("text")].toString(), QString());
        QCOMPARE(doc.node(QStringLiteral("t")).text, QString());
    }

    // ---------------------------------------------------------------- task 1

    void unknownNodeTypeSurvivesFileRoundTrip()
    {
        const QString path = writeFixture(canvasP0Fixture(), "p0.canvas");
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromFile(path));

        QCOMPARE(doc.nodes().size(), 3);
        const auto tbl = doc.node(QStringLiteral("tbl"));
        QCOMPARE(tbl.type, Canvas::NodeType::Unknown);
        QCOMPARE(tbl.rawType, QStringLiteral("table"));
        QCOMPARE(tbl.rawData[QStringLiteral("type")].toString(), QStringLiteral("table"));
        QCOMPARE(tbl.rawData[QStringLiteral("rows")].toInt(), 3);
        QCOMPARE(tbl.rawData[QStringLiteral("collapsed")].toBool(), false);

        const QString out = path + QStringLiteral(".out");
        QVERIFY(doc.saveToFile(out));

        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), canvasP0GoldenBytes());
    }

    void unknownNodeTypeIsNotRewrittenAsATextCard()
    {
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromFile(writeFixture(canvasP0Fixture(), "coerce.canvas")));
        const auto bytes = doc.serializeObsidianStyle();
        QVERIFY2(bytes.contains("\"type\":\"table\""),
                 "the unmodelled node type must survive verbatim");
        QVERIFY(!bytes.contains("\"type\":\"text\",\"text\":\"\""));
    }

    void edgeTouchingUnknownNodeIsKeptAndStaysResolvable()
    {
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromFile(writeFixture(canvasP0Fixture(), "edge.canvas")));
        const auto kept = doc.edge(QStringLiteral("e-kept"));
        QCOMPARE(kept.fromNode, QStringLiteral("tbl"));
        QCOMPARE(kept.toNode, QStringLiteral("txt"));
        QCOMPARE(kept.label, QStringLiteral("wire"));
        QVERIFY(!kept.unresolved);
        QCOMPARE(doc.edges().size(), 2);
    }

    void unresolvedEdgeSurvivesFileRoundTrip()
    {
        // Upstream imports an edge only when both endpoints resolve
        // (app/own/2018-k7.js:442) and then drops it from getData.
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromFile(writeFixture(canvasP0Fixture(), "dangle.canvas")));
        const auto dangle = doc.edge(QStringLiteral("e-dangle"));
        QVERIFY(dangle.unresolved);
        QCOMPARE(dangle.toNode, QStringLiteral("ghost"));
        QCOMPARE(dangle.rawData[QStringLiteral("label")].toString(), QStringLiteral("orphan"));
        QVERIFY(doc.serializeObsidianStyle().contains("\"toNode\":\"ghost\""));
    }

    void missingFromSideIsNotHealedOnAnUnresolvedEdge()
    {
        // Healing needs both endpoints' geometry. Guessing a side for a wire we
        // cannot draw would bake a value into a payload we promised to keep.
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(R"({
            "nodes": [{"id":"a","type":"text","x":0,"y":0,"width":10,"height":10,"text":""}],
            "edges": [{"id":"e","fromNode":"a","toNode":"ghost"}]
        })").object()));
        QVERIFY(doc.edge(QStringLiteral("e")).unresolved);
        QVERIFY(!doc.serializeObsidianStyle().contains("\"fromSide\""));
    }

    void unresolvedEdgeBecomesModelledWhenItsEndpointArrives()
    {
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(R"({
            "nodes": [{"id":"a","type":"text","x":0,"y":0,"width":10,"height":10,"text":""}],
            "edges": [{"id":"e","fromNode":"a","toNode":"ghost","label":"w"}]
        })").object()));
        QVERIFY(doc.edge(QStringLiteral("e")).unresolved);

        Canvas::CanvasNode ghost;
        ghost.id = QStringLiteral("ghost");
        ghost.type = Canvas::NodeType::Text;
        ghost.x = 300; ghost.y = 0; ghost.width = 10; ghost.height = 10;
        ghost.text = QStringLiteral("here");
        doc.addNode(ghost);

        const auto edge = doc.edge(QStringLiteral("e"));
        QVERIFY(!edge.unresolved);
        QCOMPARE(edge.label, QStringLiteral("w"));
        QCOMPARE(edge.fromSide, Canvas::Side::Right);
        QCOMPARE(edge.toSide, Canvas::Side::Left);
    }

    void saveLoadSaveIsByteStable()
    {
        const QString path = writeFixture(canvasP0Fixture(), "stable.canvas");
        Canvas::CanvasDocument first;
        QVERIFY(first.loadFromFile(path));
        const QString out1 = path + QStringLiteral(".1");
        QVERIFY(first.saveToFile(out1));

        Canvas::CanvasDocument second;
        QVERIFY(second.loadFromFile(out1));
        const QString out2 = path + QStringLiteral(".2");
        QVERIFY(second.saveToFile(out2));

        QFile a(out1), b(out2);
        QVERIFY(a.open(QIODevice::ReadOnly));
        QVERIFY(b.open(QIODevice::ReadOnly));
        QCOMPARE(b.readAll(), a.readAll());
    }

    // ---------------------------------------------------------------- task 2

    void nodesArePaintOrderAndEdgesKeepInsertionOrder()
    {
        // Upstream sorts `nodes` by zIndex and leaves `edges` in Map order
        // (app/own/2018-k7.js:362,367-370), and zIndex is reconstructed as a
        // running maximum over file order (:429) — so array position is paint
        // order. Ids chosen so an id-sort or a QHash walk would reorder them.
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"z-bottom","type":"text","x":0,"y":0,"width":10,"height":10,"text":""},
                {"id":"a-middle","type":"text","x":100,"y":0,"width":10,"height":10,"text":""},
                {"id":"m-top","type":"text","x":200,"y":0,"width":10,"height":10,"text":""}
            ],
            "edges": [
                {"id":"e-last","fromNode":"z-bottom","fromSide":"right","toNode":"a-middle","toSide":"left"},
                {"id":"e-early","fromNode":"a-middle","fromSide":"right","toNode":"m-top","toSide":"left"}
            ]
        })").object()));

        const QStringList nodeIds = {QStringLiteral("z-bottom"), QStringLiteral("a-middle"),
                                     QStringLiteral("m-top")};
        QStringList listed;
        for (const auto &n : doc.nodes())
            listed << n.id;
        QCOMPARE(listed, nodeIds);

        QStringList edgeIds;
        for (const auto &e : doc.edges())
            edgeIds << e.id;
        QCOMPARE(edgeIds, (QStringList{QStringLiteral("e-last"), QStringLiteral("e-early")}));

        const auto json = doc.toJson();
        QStringList written;
        for (const auto &v : json[QStringLiteral("nodes")].toArray())
            written << v.toObject()[QStringLiteral("id")].toString();
        QCOMPARE(written, nodeIds);
        written.clear();
        for (const auto &v : json[QStringLiteral("edges")].toArray())
            written << v.toObject()[QStringLiteral("id")].toString();
        QCOMPARE(written, (QStringList{QStringLiteral("e-last"), QStringLiteral("e-early")}));
    }

    void newNodeLandsOnTopOfThePaintOrder()
    {
        // A freshly created node takes the next zIndex counter value, so it
        // appends last in the array — the front of the paint order.
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"a","type":"text","x":0,"y":0,"width":10,"height":10,"text":""},
                {"id":"b","type":"text","x":100,"y":0,"width":10,"height":10,"text":""}
            ],
            "edges": []
        })").object()));

        Canvas::CanvasNode added;
        added.id = QStringLiteral("c");
        added.type = Canvas::NodeType::Text;
        added.text = QStringLiteral("new");
        doc.addNode(added);

        QStringList listed;
        for (const auto &v : doc.toJson()[QStringLiteral("nodes")].toArray())
            listed << v.toObject()[QStringLiteral("id")].toString();
        QCOMPARE(listed, (QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));

        doc.removeNode(QStringLiteral("a"));
        listed.clear();
        for (const auto &v : doc.toJson()[QStringLiteral("nodes")].toArray())
            listed << v.toObject()[QStringLiteral("id")].toString();
        QCOMPARE(listed, (QStringList{QStringLiteral("b"), QStringLiteral("c")}));
    }

    // ---------------------------------------------------------------- task 4

    void rejectedGeometryKeepsObsidianConstructorDefaults()
    {
        // isNaN(n) || ... (app/own/2020-O7.js:194) leaves the previous value
        // alone, and the constructor default is x=0 y=0 width=100 height=100
        // (:165) — not the 250x60 new-card size.
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(R"({
            "nodes": [
                {"id":"a","type":"text","text":"x"},
                {"id":"b","type":"text","x":[],"y":{},"width":"abc","height":true}
            ],
            "edges": []
        })").object()));
        const auto a = doc.node(QStringLiteral("a"));
        QCOMPARE(a.x, 0);
        QCOMPARE(a.y, 0);
        QCOMPARE(a.width, 100);
        QCOMPARE(a.height, 100);
        const auto b = doc.node(QStringLiteral("b"));
        QCOMPARE(b.x, 0);
        QCOMPARE(b.y, 0);
        QCOMPARE(b.width, 100);
        QCOMPARE(b.height, 100);
    }

    void coercedGeometryFollowsMathRound()
    {
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(R"({
            "nodes": [{"id":"a","type":"text","x":"12.6","y":null,"width":"","height":-0.5}],
            "edges": []
        })").object()));
        const auto a = doc.node(QStringLiteral("a"));
        QCOMPARE(a.x, 13);      // Math.round("12.6")
        QCOMPARE(a.y, 0);       // Math.round(null)
        QCOMPARE(a.width, 0);   // Number("") === 0
        QCOMPARE(a.height, 0);  // Math.round(-0.5) === -0, not qRound's -1
    }

    void negativeHalvesRoundTowardPositiveInfinity()
    {
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(R"({
            "nodes": [{"id":"a","type":"text","x":-1.5,"y":-2.5,"width":-0.5,"height":-0.6}],
            "edges": []
        })").object()));
        const auto a = doc.node(QStringLiteral("a"));
        QCOMPARE(a.x, -1);
        QCOMPARE(a.y, -2);
        QCOMPARE(a.width, 0);
        QCOMPARE(a.height, -1);
    }

    void colorTripletIsNormalisedWithoutClamping()
    {
        QCOMPARE(Canvas::normalizeCanvasColor(QStringLiteral("255,0,0")), QStringLiteral("#ff0000"));
        QCOMPARE(Canvas::normalizeCanvasColor(QStringLiteral(" 12 , 34 , 56 ")),
                 QStringLiteral("#0c2238"));
        // zb() is a bare toString(16), so an out-of-range component produces a
        // longer hex string rather than being clamped.
        QCOMPARE(Canvas::normalizeCanvasColor(QStringLiteral("300,0,0")),
                 QStringLiteral("#12c0000"));
        QCOMPARE(Canvas::normalizeCanvasColor(QStringLiteral("1,2,3")), QStringLiteral("#010203"));
        // Presets, hex and non-triplets pass straight through.
        QCOMPARE(Canvas::normalizeCanvasColor(QStringLiteral("4")), QStringLiteral("4"));
        QCOMPARE(Canvas::normalizeCanvasColor(QStringLiteral("#AbCdEf")), QStringLiteral("#AbCdEf"));
        QCOMPARE(Canvas::normalizeCanvasColor(QStringLiteral("")), QString());
        QCOMPARE(Canvas::normalizeCanvasColor(QStringLiteral("1.5,0,0")), QStringLiteral("1.5,0,0"));
        QCOMPARE(Canvas::normalizeCanvasColor(QStringLiteral("1,2,3,4")), QStringLiteral("1,2,3,4"));
    }

    void colourIsNormalisedOnReadAndRewritten()
    {
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromJson(QJsonDocument::fromJson(R"({
            "nodes": [{"id":"a","type":"text","x":0,"y":0,"width":10,"height":10,"text":"",
                       "color":"255,0,0"}],
            "edges": [{"id":"e","fromNode":"a","toNode":"a","fromSide":"right","toSide":"left",
                       "color":"0,128,255"}]
        })").object()));
        QCOMPARE(doc.node(QStringLiteral("a")).color, QStringLiteral("#ff0000"));
        QCOMPARE(doc.edge(QStringLiteral("e")).color, QStringLiteral("#0080ff"));
        const auto json = doc.toJson();
        QCOMPARE(json[QStringLiteral("nodes")].toArray().at(0).toObject()
                     [QStringLiteral("color")].toString(),
                 QStringLiteral("#ff0000"));
        QCOMPARE(json[QStringLiteral("edges")].toArray().at(0).toObject()
                     [QStringLiteral("color")].toString(),
                 QStringLiteral("#0080ff"));
    }

    // ---------------------------------------------------------- printer unit

    void printerMatchesZdCollapsingRules()
    {
        // Direct exercise of OrderedJson::print(): an all-primitive container
        // collapses to one line, a null counts as non-primitive (typeof null ===
        // "object", app/own/0562-Qd.js:2) so it forces the container open.
        Canvas::OrderedJson obj = Canvas::OrderedJson::object();
        obj.set(QStringLiteral("a"), 1);
        obj.set(QStringLiteral("b"), QStringLiteral("two"));

        Canvas::OrderedJson nested = Canvas::OrderedJson::object();
        nested.set(QStringLiteral("k"), true);
        obj.setObject(QStringLiteral("c"), nested);

        QCOMPARE(obj.print(), QByteArray("{\n\t\"a\":1,\n\t\"b\":\"two\",\n\t\"c\":{\"k\":true}\n}"));

        Canvas::OrderedJson empty = Canvas::OrderedJson::object();
        empty.set(QStringLiteral("n"), QJsonValue(QJsonValue::Null));
        QCOMPARE(empty.print(), QByteArray("{\n\t\"n\":null\n}"));
    }

    void printerKeepsAnAllPrimitiveArrayOnOneLine()
    {
        Canvas::OrderedJson arr = Canvas::OrderedJson::array();
        arr.append(1);
        arr.append(2);
        Canvas::OrderedJson root = Canvas::OrderedJson::object();
        root.setObject(QStringLiteral("xs"), arr);
        QCOMPARE(root.print(), QByteArray("{\n\t\"xs\":[1,2]\n}"));
    }

    void toJsonAndSerializeAgreeOnContent()
    {
        Canvas::CanvasDocument doc;
        QVERIFY(doc.loadFromFile(writeFixture(canvasP0Fixture(), "agree.canvas")));
        const QJsonObject json = doc.toJson();
        QCOMPARE(json[QStringLiteral("nodes")].toArray().size(), 3);
        QCOMPARE(json[QStringLiteral("edges")].toArray().size(), 2);
        QCOMPARE(json[QStringLiteral("metadata")].toObject()[QStringLiteral("version")].toString(),
                 QStringLiteral("1.2"));
        QCOMPARE(QJsonDocument(json).toJson(QJsonDocument::Compact),
                 QJsonDocument(QJsonDocument::fromJson(canvasP0GoldenBytes()).object())
                     .toJson(QJsonDocument::Compact));
    }
};

QTEST_MAIN(TestCanvasDiskContract)
#include "tst_canvas_diskcontract.moc"