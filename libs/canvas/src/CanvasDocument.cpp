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

// V5 angular-sector picker (canvas.md §3 invariant 5 + §8 invariant 10):
// given the node's own dimensions and the vector from its center to the other
// endpoint's center, return the face the edge emerges from. Aspect-aware: the
// boundary between "horizontal face" and "vertical face" is the node's own
// corner diagonal, i.e. atan2(h/2, w/2), not a fixed 45°.
//
// Do NOT conflate with A3 (live drag-snap nearest-face) — that one is in the
// scene/tool layer, not here.
Side pickSideToward(int thisW, int thisH, double dx, double dy)
{
    const double absDx = dx < 0 ? -dx : dx;
    const double absDy = dy < 0 ? -dy : dy;
    // |dy|/|dx| > thisH/thisW  ⇔  |dy|*thisW > |dx|*thisH   (no divide-by-zero)
    const bool vertical = absDy * thisW > absDx * thisH;
    if (vertical)
        return dy > 0 ? Side::Bottom : Side::Top;
    return dx > 0 ? Side::Right : Side::Left;
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
    m_nodeOrder.clear();
    m_edgeOrder.clear();
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

        // Obsidian Math.round()s geometry on every setData (canvas.md §3
        // invariant 3); QJsonValue::toInt() truncates, so fractional values
        // would drift by ≤1px every cross-app save. Round explicitly.
        node.x      = qRound(obj[QStringLiteral("x")].toDouble());
        node.y      = qRound(obj[QStringLiteral("y")].toDouble());
        node.width  = qRound(obj[QStringLiteral("width")].toDouble(250));
        node.height = qRound(obj[QStringLiteral("height")].toDouble(60));
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

        if (!m_nodes.contains(node.id))
            m_nodeOrder.append(node.id);
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

        const QString rawFromSide = obj[QStringLiteral("fromSide")].toString();
        const QString rawToSide   = obj[QStringLiteral("toSide")].toString();
        edge.fromSide = sideFromString(rawFromSide);
        edge.toSide   = sideFromString(rawToSide);

        // V5 self-heal: absent sides resolve to the angular-sector pick now and
        // are persisted in-memory so the next save bakes a concrete value in
        // — otherwise we'd ping-pong with Obsidian on every cross-app edit.
        if (rawFromSide.isEmpty() || rawToSide.isEmpty()) {
            const auto fromIt = m_nodes.constFind(edge.fromNode);
            const auto toIt   = m_nodes.constFind(edge.toNode);
            if (fromIt != m_nodes.constEnd() && toIt != m_nodes.constEnd()) {
                const double fromCx = fromIt->x + fromIt->width  / 2.0;
                const double fromCy = fromIt->y + fromIt->height / 2.0;
                const double toCx   = toIt->x   + toIt->width    / 2.0;
                const double toCy   = toIt->y   + toIt->height   / 2.0;
                if (rawFromSide.isEmpty())
                    edge.fromSide = pickSideToward(fromIt->width, fromIt->height,
                                                   toCx - fromCx, toCy - fromCy);
                if (rawToSide.isEmpty())
                    edge.toSide = pickSideToward(toIt->width, toIt->height,
                                                 fromCx - toCx, fromCy - toCy);
            }
        }

        edge.fromEnd = endTypeFromString(obj[QStringLiteral("fromEnd")].toString());
        edge.toEnd = endTypeFromString(
            obj.contains(QStringLiteral("toEnd"))
                ? obj[QStringLiteral("toEnd")].toString()
                : QStringLiteral("arrow"));
        edge.color = obj[QStringLiteral("color")].toString();
        edge.label = obj[QStringLiteral("label")].toString();
        edge.extraData = captureExtras(obj, knownEdgeKeys());

        if (!m_edges.contains(edge.id))
            m_edgeOrder.append(edge.id);
        m_edges.insert(edge.id, edge);
    }

    m_modified = false;
    return true;
}

QJsonObject CanvasDocument::toJson() const
{
    QJsonObject json;

    QJsonArray nodesArray;
    for (const auto &id : m_nodeOrder) {
        const auto &node = m_nodes[id];
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
        // Obsidian's default is "cover" and omits it from JSON; emit only when
        // it diverges from the default. See canvas.md §3 invariant 2.
        if (node.type == NodeType::Group
            && !node.backgroundStyle.isEmpty()
            && node.backgroundStyle != QLatin1String("cover"))
            obj[QStringLiteral("backgroundStyle")] = node.backgroundStyle;

        mergeExtras(obj, node.extraData);

        nodesArray.append(obj);
    }

    QJsonArray edgesArray;
    for (const auto &id : m_edgeOrder) {
        const auto &edge = m_edges[id];
        QJsonObject obj;
        obj[QStringLiteral("id")] = edge.id;
        obj[QStringLiteral("fromNode")] = edge.fromNode;
        obj[QStringLiteral("toNode")] = edge.toNode;

        // Sides are always written: invariant 2 (every edge resolves to a
        // concrete side post-load via V5 self-heal), so by the time we save
        // there is no "absent side" case to represent.
        obj[QStringLiteral("fromSide")] = sideToString(edge.fromSide);
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
    if (!m_nodes.contains(node.id))
        m_nodeOrder.append(node.id);
    m_nodes.insert(node.id, node);
    m_modified = true;
    Q_EMIT nodeAdded(node.id);
    Q_EMIT modificationChanged(true);
}

void CanvasDocument::removeNode(const QString &id)
{
    m_nodes.remove(id);
    m_nodeOrder.removeAll(id);
    // Remove connected edges
    QStringList edgesToRemove;
    for (const auto &edge : m_edges) {
        if (edge.fromNode == id || edge.toNode == id) {
            edgesToRemove.append(edge.id);
        }
    }
    for (const auto &edgeId : edgesToRemove) {
        m_edges.remove(edgeId);
        m_edgeOrder.removeAll(edgeId);
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
    QVector<CanvasNode> result;
    result.reserve(m_nodeOrder.size());
    for (const auto &id : m_nodeOrder)
        result.append(m_nodes.value(id));
    return result;
}

bool CanvasDocument::hasNode(const QString &id) const
{
    return m_nodes.contains(id);
}

void CanvasDocument::addEdge(const CanvasEdge &edge)
{
    if (!m_edges.contains(edge.id))
        m_edgeOrder.append(edge.id);
    m_edges.insert(edge.id, edge);
    m_modified = true;
    Q_EMIT edgeAdded(edge.id);
    Q_EMIT modificationChanged(true);
}

void CanvasDocument::removeEdge(const QString &id)
{
    m_edges.remove(id);
    m_edgeOrder.removeAll(id);
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
    QVector<CanvasEdge> result;
    result.reserve(m_edgeOrder.size());
    for (const auto &id : m_edgeOrder)
        result.append(m_edges.value(id));
    return result;
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
