# libcanvas — Implementation Plan (Phase 4a)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a standalone, reusable canvas editing library with JSON Canvas 1.0 format support, text cards, groups, edges, pan/zoom, resize, undo/redo, and context menus.

**Architecture:** Two layers — CanvasDocument (data model + JSON serialization, Qt6::Core only) and CanvasView/Scene (QGraphicsView-based interactive editor, Qt6::Widgets). Tool state machine pattern from PlanStan. Resize detection from Kdenlive. Undo commands from Umbrello pattern.

**Tech Stack:** C++20, Qt6::Core (data layer), Qt6::Widgets (view layer), QGraphicsView framework, QUndoStack

**Spec:** `docs/superpowers/specs/2026-03-31-canvas-library-design.md`

**Patterns borrowed from:**
- PlanStan graph lib (`~/dev/PlanStan/libs/graph/`) — tool state machine, node anchors, scene dispatch
- Kdenlive titler (`~/src/kde/src/kdenlive/src/titler/graphicsscenerectmove.cpp`) — resize handle detection
- Umbrello UML editor (`~/src/kde/src/umbrello/umbrello/cmds/`) — QUndoCommand pattern

---

### Task 1: Project Scaffold + Data Types + CanvasDocument

**Files:**
- Create: `libs/canvas/CMakeLists.txt`
- Create: `libs/canvas/README.md`
- Create: `libs/canvas/include/canvas/CanvasTypes.h`
- Create: `libs/canvas/include/canvas/CanvasDocument.h`
- Create: `libs/canvas/src/CanvasDocument.cpp`
- Create: `libs/canvas/tests/CMakeLists.txt`
- Create: `libs/canvas/tests/tst_canvasdocument.cpp`
- Create: stubs for all other library files
- Modify: `CMakeLists.txt` (root — add subdirectory)
- Modify: `src/CMakeLists.txt` (link canvas library)

This is the foundation task — data model, JSON serialization, and all stubs so the library compiles.

- [ ] **Step 1: Create directory structure**

```bash
mkdir -p libs/canvas/{include/canvas,src,tests}
```

- [ ] **Step 2: Create CanvasTypes.h**

`libs/canvas/include/canvas/CanvasTypes.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QString>

namespace Canvas {

enum class NodeType { Text, File, Link, Group };
enum class Side { Top, Right, Bottom, Left };
enum class EndType { None, Arrow };

struct CanvasNode {
    QString id;
    NodeType type = NodeType::Text;
    int x = 0, y = 0;
    int width = 250, height = 60;
    QString color;

    // Type-specific
    QString text;              // Text nodes
    QString file;              // File nodes (Phase 4b)
    QString subpath;           // File nodes (Phase 4b)
    QString url;               // Link nodes (Phase 4b)
    QString label;             // Group nodes
    QString background;        // Group nodes
    QString backgroundStyle;   // Group nodes: "cover", "ratio", "repeat"
};

struct CanvasEdge {
    QString id;
    QString fromNode;
    QString toNode;
    Side fromSide = Side::Right;
    Side toSide = Side::Left;
    EndType fromEnd = EndType::None;
    EndType toEnd = EndType::Arrow;
    QString color;
    QString label;
};

// Color mapping from JSON Canvas spec
inline QColor colorFromCanvasColor(const QString &c)
{
    if (c == QLatin1String("1")) return QColor(233, 49, 71);     // Red
    if (c == QLatin1String("2")) return QColor(236, 117, 0);     // Orange
    if (c == QLatin1String("3")) return QColor(224, 172, 0);     // Yellow
    if (c == QLatin1String("4")) return QColor(8, 185, 78);      // Green
    if (c == QLatin1String("5")) return QColor(0, 191, 188);     // Cyan
    if (c == QLatin1String("6")) return QColor(120, 82, 238);    // Purple
    if (c.startsWith(QLatin1Char('#'))) return QColor(c);
    return QColor();
}

inline QString sideToString(Side s)
{
    switch (s) {
    case Side::Top: return QStringLiteral("top");
    case Side::Right: return QStringLiteral("right");
    case Side::Bottom: return QStringLiteral("bottom");
    case Side::Left: return QStringLiteral("left");
    }
    return QStringLiteral("right");
}

inline Side sideFromString(const QString &s)
{
    if (s == QLatin1String("top")) return Side::Top;
    if (s == QLatin1String("bottom")) return Side::Bottom;
    if (s == QLatin1String("left")) return Side::Left;
    return Side::Right;
}

inline QString endTypeToString(EndType e)
{
    return e == EndType::Arrow ? QStringLiteral("arrow") : QStringLiteral("none");
}

inline EndType endTypeFromString(const QString &s)
{
    return s == QLatin1String("arrow") ? EndType::Arrow : EndType::None;
}

} // namespace Canvas
```

- [ ] **Step 3: Create CanvasDocument.h**

`libs/canvas/include/canvas/CanvasDocument.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QHash>
#include <QVector>
#include <QJsonObject>
#include "CanvasTypes.h"

namespace Canvas {

class CanvasDocument : public QObject {
    Q_OBJECT

public:
    explicit CanvasDocument(QObject *parent = nullptr);

    // Serialization
    bool loadFromJson(const QJsonObject &json);
    QJsonObject toJson() const;
    bool loadFromFile(const QString &filePath);
    bool saveToFile(const QString &filePath);

    // Node operations
    void addNode(const CanvasNode &node);
    void removeNode(const QString &id);
    void updateNode(const CanvasNode &node);
    CanvasNode node(const QString &id) const;
    QVector<CanvasNode> nodes() const;
    bool hasNode(const QString &id) const;

    // Edge operations
    void addEdge(const CanvasEdge &edge);
    void removeEdge(const QString &id);
    CanvasEdge edge(const QString &id) const;
    QVector<CanvasEdge> edges() const;
    QVector<CanvasEdge> edgesForNode(const QString &nodeId) const;

    // State
    bool isModified() const;
    void setModified(bool modified);

    // ID generation
    static QString generateId();

Q_SIGNALS:
    void nodeAdded(const QString &id);
    void nodeRemoved(const QString &id);
    void nodeChanged(const QString &id);
    void edgeAdded(const QString &id);
    void edgeRemoved(const QString &id);
    void modificationChanged(bool modified);

private:
    QHash<QString, CanvasNode> m_nodes;
    QHash<QString, CanvasEdge> m_edges;
    bool m_modified = false;
};

} // namespace Canvas
```

- [ ] **Step 4: Implement CanvasDocument.cpp**

`libs/canvas/src/CanvasDocument.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasDocument.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QUuid>

namespace Canvas {

CanvasDocument::CanvasDocument(QObject *parent)
    : QObject(parent)
{
}

bool CanvasDocument::loadFromJson(const QJsonObject &json)
{
    m_nodes.clear();
    m_edges.clear();

    // Parse nodes
    auto nodesArray = json[QStringLiteral("nodes")].toArray();
    for (const auto &val : nodesArray) {
        auto obj = val.toObject();
        CanvasNode node;
        node.id = obj[QStringLiteral("id")].toString();

        QString typeStr = obj[QStringLiteral("type")].toString();
        if (typeStr == QLatin1String("text")) node.type = NodeType::Text;
        else if (typeStr == QLatin1String("file")) node.type = NodeType::File;
        else if (typeStr == QLatin1String("link")) node.type = NodeType::Link;
        else if (typeStr == QLatin1String("group")) node.type = NodeType::Group;

        node.x = obj[QStringLiteral("x")].toInt();
        node.y = obj[QStringLiteral("y")].toInt();
        node.width = obj[QStringLiteral("width")].toInt(250);
        node.height = obj[QStringLiteral("height")].toInt(60);
        node.color = obj[QStringLiteral("color")].toString();

        // Type-specific fields
        node.text = obj[QStringLiteral("text")].toString();
        node.file = obj[QStringLiteral("file")].toString();
        node.subpath = obj[QStringLiteral("subpath")].toString();
        node.url = obj[QStringLiteral("url")].toString();
        node.label = obj[QStringLiteral("label")].toString();
        node.background = obj[QStringLiteral("background")].toString();
        node.backgroundStyle = obj[QStringLiteral("backgroundStyle")].toString();

        m_nodes.insert(node.id, node);
    }

    // Parse edges
    auto edgesArray = json[QStringLiteral("edges")].toArray();
    for (const auto &val : edgesArray) {
        auto obj = val.toObject();
        CanvasEdge edge;
        edge.id = obj[QStringLiteral("id")].toString();
        edge.fromNode = obj[QStringLiteral("fromNode")].toString();
        edge.toNode = obj[QStringLiteral("toNode")].toString();
        edge.fromSide = sideFromString(obj[QStringLiteral("fromSide")].toString());
        edge.toSide = sideFromString(obj[QStringLiteral("toSide")].toString());
        edge.fromEnd = endTypeFromString(obj[QStringLiteral("fromEnd")].toString());
        edge.toEnd = endTypeFromString(
            obj.contains(QStringLiteral("toEnd"))
                ? obj[QStringLiteral("toEnd")].toString()
                : QStringLiteral("arrow"));
        edge.color = obj[QStringLiteral("color")].toString();
        edge.label = obj[QStringLiteral("label")].toString();

        m_edges.insert(edge.id, edge);
    }

    m_modified = false;
    return true;
}

QJsonObject CanvasDocument::toJson() const
{
    QJsonObject json;

    QJsonArray nodesArray;
    for (const auto &node : m_nodes) {
        QJsonObject obj;
        obj[QStringLiteral("id")] = node.id;

        switch (node.type) {
        case NodeType::Text: obj[QStringLiteral("type")] = QStringLiteral("text"); break;
        case NodeType::File: obj[QStringLiteral("type")] = QStringLiteral("file"); break;
        case NodeType::Link: obj[QStringLiteral("type")] = QStringLiteral("link"); break;
        case NodeType::Group: obj[QStringLiteral("type")] = QStringLiteral("group"); break;
        }

        obj[QStringLiteral("x")] = node.x;
        obj[QStringLiteral("y")] = node.y;
        obj[QStringLiteral("width")] = node.width;
        obj[QStringLiteral("height")] = node.height;

        if (!node.color.isEmpty()) obj[QStringLiteral("color")] = node.color;

        // Type-specific
        if (node.type == NodeType::Text && !node.text.isEmpty())
            obj[QStringLiteral("text")] = node.text;
        if (node.type == NodeType::File && !node.file.isEmpty())
            obj[QStringLiteral("file")] = node.file;
        if (node.type == NodeType::File && !node.subpath.isEmpty())
            obj[QStringLiteral("subpath")] = node.subpath;
        if (node.type == NodeType::Link && !node.url.isEmpty())
            obj[QStringLiteral("url")] = node.url;
        if (node.type == NodeType::Group && !node.label.isEmpty())
            obj[QStringLiteral("label")] = node.label;
        if (node.type == NodeType::Group && !node.background.isEmpty())
            obj[QStringLiteral("background")] = node.background;
        if (node.type == NodeType::Group && !node.backgroundStyle.isEmpty())
            obj[QStringLiteral("backgroundStyle")] = node.backgroundStyle;

        nodesArray.append(obj);
    }

    QJsonArray edgesArray;
    for (const auto &edge : m_edges) {
        QJsonObject obj;
        obj[QStringLiteral("id")] = edge.id;
        obj[QStringLiteral("fromNode")] = edge.fromNode;
        obj[QStringLiteral("toNode")] = edge.toNode;

        if (edge.fromSide != Side::Right)
            obj[QStringLiteral("fromSide")] = sideToString(edge.fromSide);
        else
            obj[QStringLiteral("fromSide")] = sideToString(edge.fromSide);

        if (edge.toSide != Side::Left)
            obj[QStringLiteral("toSide")] = sideToString(edge.toSide);
        else
            obj[QStringLiteral("toSide")] = sideToString(edge.toSide);

        if (edge.fromEnd != EndType::None)
            obj[QStringLiteral("fromEnd")] = endTypeToString(edge.fromEnd);
        if (edge.toEnd != EndType::Arrow)
            obj[QStringLiteral("toEnd")] = endTypeToString(edge.toEnd);

        if (!edge.color.isEmpty()) obj[QStringLiteral("color")] = edge.color;
        if (!edge.label.isEmpty()) obj[QStringLiteral("label")] = edge.label;

        edgesArray.append(obj);
    }

    json[QStringLiteral("nodes")] = nodesArray;
    json[QStringLiteral("edges")] = edgesArray;
    return json;
}

bool CanvasDocument::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;
    return loadFromJson(doc.object());
}

bool CanvasDocument::saveToFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    m_modified = false;
    Q_EMIT modificationChanged(false);
    return true;
}

void CanvasDocument::addNode(const CanvasNode &node)
{
    m_nodes.insert(node.id, node);
    m_modified = true;
    Q_EMIT nodeAdded(node.id);
    Q_EMIT modificationChanged(true);
}

void CanvasDocument::removeNode(const QString &id)
{
    m_nodes.remove(id);
    // Remove connected edges
    QStringList edgesToRemove;
    for (const auto &edge : m_edges) {
        if (edge.fromNode == id || edge.toNode == id) {
            edgesToRemove.append(edge.id);
        }
    }
    for (const auto &edgeId : edgesToRemove) {
        m_edges.remove(edgeId);
        Q_EMIT edgeRemoved(edgeId);
    }
    m_modified = true;
    Q_EMIT nodeRemoved(id);
    Q_EMIT modificationChanged(true);
}

void CanvasDocument::updateNode(const CanvasNode &node)
{
    if (!m_nodes.contains(node.id)) return;
    m_nodes[node.id] = node;
    m_modified = true;
    Q_EMIT nodeChanged(node.id);
    Q_EMIT modificationChanged(true);
}

CanvasNode CanvasDocument::node(const QString &id) const
{
    return m_nodes.value(id);
}

QVector<CanvasNode> CanvasDocument::nodes() const
{
    return QVector<CanvasNode>(m_nodes.cbegin(), m_nodes.cend());
}

bool CanvasDocument::hasNode(const QString &id) const
{
    return m_nodes.contains(id);
}

void CanvasDocument::addEdge(const CanvasEdge &edge)
{
    m_edges.insert(edge.id, edge);
    m_modified = true;
    Q_EMIT edgeAdded(edge.id);
    Q_EMIT modificationChanged(true);
}

void CanvasDocument::removeEdge(const QString &id)
{
    m_edges.remove(id);
    m_modified = true;
    Q_EMIT edgeRemoved(id);
    Q_EMIT modificationChanged(true);
}

CanvasEdge CanvasDocument::edge(const QString &id) const
{
    return m_edges.value(id);
}

QVector<CanvasEdge> CanvasDocument::edges() const
{
    return QVector<CanvasEdge>(m_edges.cbegin(), m_edges.cend());
}

QVector<CanvasEdge> CanvasDocument::edgesForNode(const QString &nodeId) const
{
    QVector<CanvasEdge> result;
    for (const auto &edge : m_edges) {
        if (edge.fromNode == nodeId || edge.toNode == nodeId) {
            result.append(edge);
        }
    }
    return result;
}

bool CanvasDocument::isModified() const
{
    return m_modified;
}

void CanvasDocument::setModified(bool modified)
{
    if (m_modified != modified) {
        m_modified = modified;
        Q_EMIT modificationChanged(modified);
    }
}

QString CanvasDocument::generateId()
{
    // Match Obsidian's ID format: 16-char hex string
    return QUuid::createUuid().toString(QUuid::Id128).left(16);
}

} // namespace Canvas
```

- [ ] **Step 5: Write CanvasDocument tests**

`libs/canvas/tests/tst_canvasdocument.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
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
```

- [ ] **Step 6: Create CMakeLists and stubs for all view layer files**

`libs/canvas/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(canvas VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)

add_library(canvas STATIC
    src/CanvasDocument.cpp
    src/CanvasView.cpp
    src/CanvasScene.cpp
    src/CanvasTool.cpp
    src/TextCardItem.cpp
    src/GroupItem.cpp
    src/EdgeItem.cpp
    include/canvas/CanvasDocument.h
    include/canvas/CanvasView.h
    include/canvas/CanvasScene.h
    include/canvas/CanvasTool.h
    include/canvas/TextCardItem.h
    include/canvas/GroupItem.h
    include/canvas/EdgeItem.h
)
set_target_properties(canvas PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(canvas
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(canvas PUBLIC Qt6::Core Qt6::Widgets)

if(NOT PROJECT_IS_TOP_LEVEL)
    # In-tree build
else()
    enable_testing()
    add_subdirectory(tests)
endif()
```

Create stub headers and .cpp files for all view layer classes (CanvasView, CanvasScene, CanvasTool, TextCardItem, GroupItem, EdgeItem) with minimal declarations and empty implementations. The implementer should read the spec for the class declarations and create stubs following the same pattern used in the forcegraph library scaffold (Task 1 of the forcegraph plan).

`libs/canvas/tests/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(canvas_tests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_canvasdocument tst_canvasdocument.cpp)
add_test(NAME tst_canvasdocument COMMAND tst_canvasdocument)
target_link_libraries(tst_canvasdocument PRIVATE Qt6::Test canvas)
set_tests_properties(tst_canvasdocument PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

`libs/canvas/README.md`:
```markdown
# libcanvas

A standalone Qt6/C++ canvas editing library compatible with JSON Canvas 1.0 (jsoncanvas.org).

## Features

- JSON Canvas 1.0 format read/write
- Text cards with inline markdown editing
- Groups with labeled containment
- Edges with directional arrows and labels
- QGraphicsView-based interactive editor
- Pan, zoom, resize, undo/redo
- Tool state machine (select, create card, create edge)

## License

GPL-3.0-or-later
```

Add to root `CMakeLists.txt` after forcegraph:
```cmake
add_subdirectory(libs/canvas)
```

Add to `src/CMakeLists.txt` link libraries: `canvas`

Add to root `CMakeLists.txt` test subdirectories:
```cmake
add_subdirectory(libs/canvas/tests)
```

- [ ] **Step 7: Build and run tests**

```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R tst_canvasdocument --output-on-failure
```

Expected: All CanvasDocument tests pass.

- [ ] **Step 8: Commit**

```bash
git add libs/canvas/ CMakeLists.txt src/CMakeLists.txt
git commit -m "feat: scaffold libcanvas with CanvasDocument + JSON Canvas 1.0 serialization

Data model with full JSON Canvas spec support (text, group, file, link
node types; edges with sides, ends, labels, colors). 13 unit tests
covering serialization, round-trip, signals, color encoding.
View layer stubs for incremental implementation."
```

---

### Task 2: TextCardItem + EdgeItem (rendering)

**Files:**
- Modify: `libs/canvas/include/canvas/TextCardItem.h`
- Modify: `libs/canvas/src/TextCardItem.cpp`
- Modify: `libs/canvas/include/canvas/EdgeItem.h`
- Modify: `libs/canvas/src/EdgeItem.cpp`

The implementer should read the spec for the full class declarations. Key implementation points:

**TextCardItem:**
- `QGraphicsObject` subclass with `boundingRect()` returning the card rect
- `paint()`: draws rounded rect (8px radius), optional color stripe at top (20px), text via `QTextDocument::drawContents()`
- `ItemIsMovable`, `ItemIsSelectable`, `ItemSendsGeometryChanges` flags
- `connectionPoint(Side)`: returns center of the specified edge for edge attachment
- `resizeModeAtPos(QPointF)`: Kdenlive-style detection (~8px border zone for resize handles
- Cursor feedback: `SizeFDiagCursor` on corners, `SizeVerCursor`/`SizeHorCursor` on edges

**EdgeItem:**
- `QGraphicsPathItem` subclass
- `adjust()`: computes path from `source->connectionPoint(fromSide)` to `target->connectionPoint(toSide)`
- Arrow rendering: small triangle at endpoint with `EndType::Arrow`
- Label: rendered at midpoint via `QGraphicsTextItem` child or direct `paint()`
- Z-value below cards (0 vs 1)

- [ ] **Step 1-5: Implement, build, commit**

The implementer should write the full implementations (~150 lines each), build, and commit.

---

### Task 3: GroupItem

**Files:**
- Modify: `libs/canvas/include/canvas/GroupItem.h`
- Modify: `libs/canvas/src/GroupItem.cpp`

**GroupItem:**
- `QGraphicsObject` subclass
- `paint()`: semi-transparent colored rect (alpha 0.1), darker border, label text at top-left
- `ItemIsMovable`, `ItemIsSelectable`, `ItemSendsGeometryChanges`
- Z-value -1 (behind cards and edges)
- `containedItems()`: returns all items whose center point is within the group's rect
- `itemChange(ItemPositionHasChanged)`: moves all contained items by the same delta
- Resize handles (same pattern as TextCardItem)

- [ ] **Step 1-4: Implement, build, commit**

---

### Task 4: CanvasTool (tool state machine)

**Files:**
- Modify: `libs/canvas/include/canvas/CanvasTool.h`
- Modify: `libs/canvas/src/CanvasTool.cpp`

Three tools in one file:

**CanvasTool (abstract base):**
```cpp
class CanvasTool : public QObject {
public:
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *) = 0;
    virtual void activate() {}
    virtual void deactivate() {}
};
```

**SelectMoveTool:**
- Click empty: deselect all
- Click item: select (with Shift for multi-select)
- Drag item: move (push CmdMoveCards undo command on release)
- Drag on resize handle: resize (push CmdResizeCard on release)
- Delete key: remove selected items (push CmdRemoveCard/CmdRemoveEdge)
- Arrow keys: nudge selected items by 1px (10px with Shift)

**CreateCardTool:**
- Click on empty space: create new TextCardItem at click position, switch back to SelectMoveTool

**CreateEdgeTool:**
- Click on card edge zone: start edge from that card
- Drag to another card: show preview line
- Release on target card: create edge, switch back to SelectMoveTool
- Release on empty space: cancel

- [ ] **Step 1-4: Implement, build, commit**

---

### Task 5: CanvasScene + CanvasView (interactive editor)

**Files:**
- Modify: `libs/canvas/include/canvas/CanvasScene.h`
- Modify: `libs/canvas/src/CanvasScene.cpp`
- Modify: `libs/canvas/include/canvas/CanvasView.h`
- Modify: `libs/canvas/src/CanvasView.cpp`

**CanvasScene:**
- `setDocument(doc)`: creates TextCardItem/GroupItem for each node, EdgeItem for each edge
- Mouse events delegate to `m_activeTool`
- Context menus (right-click empty → New Text Card/New Group; right-click card → Color/Delete/Duplicate)
- `QUndoStack` for undo/redo
- Connects to `CanvasDocument` signals to sync items with data

**CanvasView:**
- Constructor: create CanvasScene, set render hints, drag mode
- `wheelEvent()`: zoom with `scale()` centered on cursor
- `keyPressEvent()`: Ctrl+Z undo, Ctrl+Y/Ctrl+Shift+Z redo, Home → zoomToFit, +/- zoom, Delete → remove selection
- `drawBackground()`: dotted grid pattern
- Pan via middle-mouse or click+drag on empty space (via SelectMoveTool)

- [ ] **Step 1-4: Implement, build, commit**

---

### Task 6: Undo/Redo Commands

**Files:**
- Create: `libs/canvas/include/canvas/CanvasCommands.h`
- Create: `libs/canvas/src/CanvasCommands.cpp`
- Modify: `libs/canvas/CMakeLists.txt`

Undo commands using Umbrello's QUndoCommand pattern:

```cpp
class CmdMoveCards : public QUndoCommand { ... };
class CmdResizeCard : public QUndoCommand { ... };
class CmdAddCard : public QUndoCommand { ... };
class CmdRemoveCard : public QUndoCommand { ... };
class CmdAddEdge : public QUndoCommand { ... };
class CmdRemoveEdge : public QUndoCommand { ... };
class CmdEditText : public QUndoCommand { ... };
class CmdChangeColor : public QUndoCommand { ... };
```

Each command stores old/new state, applies on redo, restores on undo.

- [ ] **Step 1-4: Implement, build, commit**

---

### Task 7: Scene Tests + Integration

**Files:**
- Create: `libs/canvas/tests/tst_canvasscene.cpp`
- Modify: `libs/canvas/tests/CMakeLists.txt`

**Tests (need QApplication, offscreen):**

```cpp
void testAddTextCard()
{
    Canvas::CanvasDocument doc;
    Canvas::CanvasScene scene;
    scene.setDocument(&doc);

    Canvas::CanvasNode node;
    node.id = Canvas::CanvasDocument::generateId();
    node.type = Canvas::NodeType::Text;
    node.text = "Test";
    node.x = 100; node.y = 100;
    node.width = 200; node.height = 80;
    doc.addNode(node);

    QVERIFY(scene.items().size() > 0);
}

void testMoveCardUpdatesDocument() { ... }
void testDeleteCardRemovesEdges() { ... }
void testGroupContainment() { ... }
void testUndoMoveCard() { ... }
void testEdgeAdjustsOnCardMove() { ... }
```

- [ ] **Step 1-4: Implement, build, commit**

---

Self-review:

1. **Spec coverage:** CanvasTypes ✓. CanvasDocument with JSON Canvas 1.0 ✓ (13 tests). TextCardItem with resize ✓. GroupItem with containment ✓. EdgeItem with arrows+labels ✓. Tool state machine (SelectMove, CreateCard, CreateEdge) ✓. CanvasScene with document sync ✓. CanvasView with pan/zoom/grid ✓. Undo commands ✓. Context menus ✓. Color palette ✓.

2. **Placeholder scan:** Task 1 has complete code (data layer). Tasks 2-6 describe implementations algorithmically since each is ~150-250 lines of graphics code. The spec has exact class declarations, method signatures, and interaction descriptions. Tasks 2-6 are MORE detailed than our forcegraph plan Tasks 4-5 which the implementers handled well.

3. **Type consistency:** `CanvasNode`/`CanvasEdge` used throughout. `NodeType::Text/Group`, `Side::Top/Right/Bottom/Left`, `EndType::None/Arrow` consistent. `connectionPoint(Side)` on TextCardItem used by EdgeItem's `adjust()`. `CanvasDocument` signals (`nodeAdded`, etc.) consumed by `CanvasScene`. `QUndoStack` owned by CanvasScene.
