// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasDocument.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QSet>
#include <QUuid>

namespace Canvas {

namespace {

// Keys this version of Corbomite consumes when loading. Anything outside the
// set is preserved verbatim into extraData / extraTopLevel so a plugin- or
// future-Obsidian-written file survives a round-trip unchanged.
const QSet<QString> &knownNodeKeys()
{
    static const QSet<QString> k = {
        QStringLiteral("id"),         QStringLiteral("type"),
        QStringLiteral("x"),          QStringLiteral("y"),
        QStringLiteral("width"),      QStringLiteral("height"),
        QStringLiteral("color"),      QStringLiteral("text"),
        QStringLiteral("file"),       QStringLiteral("subpath"),
        QStringLiteral("url"),        QStringLiteral("label"),
        QStringLiteral("background"), QStringLiteral("backgroundStyle"),
    };
    return k;
}

const QSet<QString> &knownEdgeKeys()
{
    static const QSet<QString> k = {
        QStringLiteral("id"),       QStringLiteral("fromNode"),
        QStringLiteral("toNode"),   QStringLiteral("fromSide"),
        QStringLiteral("toSide"),   QStringLiteral("fromEnd"),
        QStringLiteral("toEnd"),    QStringLiteral("color"),
        QStringLiteral("label"),
    };
    return k;
}

QJsonObject captureExtras(const QJsonObject &src, const QSet<QString> &known)
{
    QJsonObject extras;
    for (auto it = src.begin(); it != src.end(); ++it) {
        if (!known.contains(it.key()))
            extras.insert(it.key(), it.value());
    }
    return extras;
}

void mergeExtras(QJsonObject &dst, const QJsonObject &extras)
{
    // Append extras after the modelled keys. Obsidian's actual emission order
    // puts unknownData first; preserving JSON object key order isn't load-bearing
    // (the parsers we target are not order-sensitive), so emitting after keeps
    // diffs against our own previous output stable.
    for (auto it = extras.begin(); it != extras.end(); ++it)
        dst.insert(it.key(), it.value());
}

} // namespace

CanvasDocument::CanvasDocument(QObject *parent)
    : QObject(parent)
{
}

bool CanvasDocument::loadFromJson(const QJsonObject &json)
{
    m_nodes.clear();
    m_edges.clear();
    m_extraTopLevel = captureExtras(
        json, {QStringLiteral("nodes"), QStringLiteral("edges")});

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
        node.extraData = captureExtras(obj, knownNodeKeys());

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
        edge.extraData = captureExtras(obj, knownEdgeKeys());

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

        mergeExtras(obj, node.extraData);

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

        mergeExtras(obj, edge.extraData);

        edgesArray.append(obj);
    }

    json[QStringLiteral("nodes")] = nodesArray;
    json[QStringLiteral("edges")] = edgesArray;
    mergeExtras(json, m_extraTopLevel);
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

void CanvasDocument::updateEdge(const CanvasEdge &edge)
{
    if (!m_edges.contains(edge.id)) return;
    m_edges[edge.id] = edge;
    m_modified = true;
    Q_EMIT edgeChanged(edge.id);
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
