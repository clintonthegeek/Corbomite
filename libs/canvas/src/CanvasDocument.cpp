// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/canvas/CanvasDocument.h"
#include <QFile>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace Corbomite::Canvas {

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

// Node/edge keys that are only written when they carry a non-default value.
// Everything else — id, x, y, width, height, type, text, file, url, fromNode,
// toNode, fromSide, toSide — is written unconditionally, matching getData().
const QSet<QString> &optionalNodeKeys()
{
    static const QSet<QString> k = {
        QStringLiteral("color"),      QStringLiteral("label"),
        QStringLiteral("subpath"),    QStringLiteral("background"),
        QStringLiteral("backgroundStyle"),
    };
    return k;
}

const QSet<QString> &optionalEdgeKeys()
{
    static const QSet<QString> k = {
        QStringLiteral("fromEnd"), QStringLiteral("toEnd"),
        QStringLiteral("color"),    QStringLiteral("label"),
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

// Math.round() with JS coercion (app/own/2020-O7.js:194): a number, a numeric
// string and null all convert; anything else is NaN and is *rejected*, i.e.
// the caller's previous value survives. Halves round toward +Infinity, which
// is where Math.round differs from qRound's away-from-zero (-0.5 → 0 here,
// -1 there).
int readGeometry(const QJsonValue &v, int fallback)
{
    double d = 0.0;
    if (v.isDouble()) {
        d = v.toDouble();
    } else if (v.isNull()) {
        d = 0.0;
    } else if (v.isString()) {
        // Number("") and Number("  ") are both 0, unlike QString::toDouble().
        const QString trimmed = v.toString().trimmed();
        if (trimmed.isEmpty())
            return 0;
        bool ok = false;
        d = trimmed.toDouble(&ok);
        if (!ok)
            return fallback;
    } else {
        return fallback;
    }
    if (!std::isfinite(d))
        return fallback;
    return int(std::floor(d + 0.5));
}

QString stringOrEmpty(const QJsonObject &obj, const QString &key)
{
    const QJsonValue v = obj.value(key);
    return v.isString() ? v.toString() : QString();
}

// Obsidian's colour field goes through Z8 only after a truthiness test, so a
// non-string is not a colour at all.
QString readColor(const QJsonObject &obj, const QString &key)
{
    return normalizeCanvasColor(stringOrEmpty(obj, key));
}

} // namespace

QString normalizeCanvasColor(const QString &raw)
{
    static const QRegularExpression triplet(
        QStringLiteral("^\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*$"));
    const QRegularExpressionMatch m = triplet.match(raw);
    if (!m.hasMatch())
        return raw;
    QString hex = QStringLiteral("#");
    for (int i = 1; i <= 3; ++i) {
        bool ok = false;
        const qlonglong component = m.captured(i).toLongLong(&ok);
        if (!ok)
            return raw;
        QString digits = QString::number(component, 16);
        if (digits.size() == 1)
            digits.prepend(QLatin1Char('0'));
        hex += digits;
    }
    return hex;
}

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

CanvasDocument::CanvasDocument(QObject *parent)
    : QObject(parent)
{
}

void CanvasDocument::adoptEdge(const QJsonObject &obj, CanvasEdge &edge) const
{
    edge.id = obj[QStringLiteral("id")].toString();
    edge.fromNode = obj[QStringLiteral("fromNode")].toString();
    edge.toNode = obj[QStringLiteral("toNode")].toString();

    const QString rawFromSide = obj[QStringLiteral("fromSide")].toString();
    const QString rawToSide   = obj[QStringLiteral("toSide")].toString();
    edge.fromSide = sideFromString(rawFromSide);
    edge.toSide   = sideFromString(rawToSide);

    // Absent sides resolve to the angular-sector pick now and are persisted
    // in-memory so the next save bakes a concrete value in — otherwise we'd
    // ping-pong with Obsidian on every cross-app edit. Upstream heals at
    // import ($7, app/own/2018-k7.js:448) and again in edge setData
    // (app/own/2028-n9.js:58).
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

    // i9 = "none", r9 = "arrow" (app/own/2028-n9.js:8-9); the fallbacks mirror
    // `i = i || i9` / `a = a || r9` in edge setData (:58) — a falsy fromEnd is
    // "none" and a falsy toEnd is "arrow".
    edge.fromEnd = endTypeFromString(stringOrEmpty(obj, QStringLiteral("fromEnd")));
    const QString rawToEnd = stringOrEmpty(obj, QStringLiteral("toEnd"));
    edge.toEnd = endTypeFromString(
        rawToEnd.isEmpty() ? QStringLiteral("arrow") : rawToEnd);
    edge.color = readColor(obj, QStringLiteral("color"));
    edge.label = stringOrEmpty(obj, QStringLiteral("label"));
    edge.extraData = captureExtras(obj, knownEdgeKeys());
    edge.explicitKeys.clear();
    for (const QString &key : optionalEdgeKeys()) {
        if (obj.contains(key))
            edge.explicitKeys.insert(key);
    }
}

void CanvasDocument::refreshUnresolvedEdges()
{
    for (const QString &id : std::as_const(m_edgeOrder)) {
        auto it = m_edges.find(id);
        if (it == m_edges.end() || !it->unresolved)
            continue;
        if (!m_nodes.contains(it->fromNode) || !m_nodes.contains(it->toNode))
            continue;
        adoptEdge(it->rawData, *it);
        it->unresolved = false;
        it->rawData = QJsonObject();
    }
}

bool CanvasDocument::loadFromJson(const QJsonObject &json)
{
    m_nodes.clear();
    m_edges.clear();
    m_nodeOrder.clear();
    m_edgeOrder.clear();
    m_nodeZIndex.clear();
    m_extraTopLevel = captureExtras(
        json, {QStringLiteral("nodes"), QStringLiteral("edges")});

    // Parse nodes
    auto nodesArray = json[QStringLiteral("nodes")].toArray();
    for (const auto &val : nodesArray) {
        auto obj = val.toObject();
        CanvasNode node;
        node.id = obj[QStringLiteral("id")].toString();

        // Obsidian Math.round()s geometry on every setData and rejects a NaN
        // result, keeping the constructor default of x=0 y=0 w=100 h=100
        // (app/own/2020-O7.js:165,194). Corbomite's struct default of 250x60
        // is the "new card" size and is deliberately not the parse fallback.
        node.width  = 100;
        node.height = 100;
        node.x      = readGeometry(obj.value(QStringLiteral("x")), node.x);
        node.y      = readGeometry(obj.value(QStringLiteral("y")), node.y);
        node.width  = readGeometry(obj.value(QStringLiteral("width")), node.width);
        node.height = readGeometry(obj.value(QStringLiteral("height")), node.height);
        node.color = readColor(obj, QStringLiteral("color"));

        const QString typeStr = stringOrEmpty(obj, QStringLiteral("type"));
        if (typeStr == QLatin1String("text")) {
            node.type = NodeType::Text;
            node.text = stringOrEmpty(obj, QStringLiteral("text"));
        } else if (typeStr == QLatin1String("file")) {
            node.type = NodeType::File;
            node.file = stringOrEmpty(obj, QStringLiteral("file"));
            node.subpath = stringOrEmpty(obj, QStringLiteral("subpath"));
        } else if (typeStr == QLatin1String("link")) {
            node.type = NodeType::Link;
            node.url = stringOrEmpty(obj, QStringLiteral("url"));
        } else if (typeStr == QLatin1String("group")) {
            node.type = NodeType::Group;
            node.label = stringOrEmpty(obj, QStringLiteral("label"));
            node.background = stringOrEmpty(obj, QStringLiteral("background"));
            node.backgroundStyle = stringOrEmpty(obj, QStringLiteral("backgroundStyle"));
        } else {
            // Unrecognised (or absent) type. Upstream skips it entirely
            // (app/own/2018-k7.js:424) and then omits it from getData, so the
            // node — and every edge touching it — is gone after one save.
            node.type = NodeType::Unknown;
            node.rawType = typeStr;
            node.rawData = obj;
        }
        node.extraData = captureExtras(obj, knownNodeKeys());
        node.explicitKeys.clear();
        for (const QString &key : optionalNodeKeys()) {
            if (obj.contains(key))
                node.explicitKeys.insert(key);
        }

        if (!m_nodes.contains(node.id))
            m_nodeOrder.append(node.id);
        m_nodeZIndex.insert(node.id, m_nodeOrder.size() - 1);
        m_nodes.insert(node.id, node);
    }

    // Parse edges
    auto edgesArray = json[QStringLiteral("edges")].toArray();
    for (const auto &val : edgesArray) {
        auto obj = val.toObject();
        CanvasEdge edge;
        edge.fromNode = obj[QStringLiteral("fromNode")].toString();
        edge.toNode = obj[QStringLiteral("toNode")].toString();

        // Both endpoints must resolve or upstream never imports the edge
        // (app/own/2018-k7.js:442) and getData then drops it. Keep the source
        // object instead so a hand-edited, plugin-authored or
        // not-yet-loaded canvas does not lose its wires.
        if (!m_nodes.contains(edge.fromNode) || !m_nodes.contains(edge.toNode)) {
            edge.id = obj[QStringLiteral("id")].toString();
            edge.unresolved = true;
            edge.rawData = obj;
        } else {
            adoptEdge(obj, edge);
        }

        if (!m_edges.contains(edge.id))
            m_edgeOrder.append(edge.id);
        m_edges.insert(edge.id, edge);
    }

    refreshUnresolvedEdges();

    m_modified = false;
    return true;
}

QByteArray CanvasDocument::serializeObsidianStyle() const
{
    return toOrderedJson().print();
}

QJsonObject CanvasDocument::toJson() const
{
    return toOrderedJson().toQJsonValue().toObject();
}

OrderedJson CanvasDocument::toOrderedJson() const
{
    OrderedJson root = OrderedJson::object();

    // Upstream spreads the loaded file's own top-level keys and overwrites
    // nodes/edges in place, so in practice "nodes" and "edges" lead.
    OrderedJson nodes = OrderedJson::array();
    QVector<QString> paintOrder = m_nodeOrder;
    std::stable_sort(paintOrder.begin(), paintOrder.end(),
                     [this](const QString &a, const QString &b) {
                         return m_nodeZIndex.value(a) < m_nodeZIndex.value(b);
                     });
    for (const auto &id : paintOrder) {
        const auto &node = m_nodes[id];
        if (node.type == NodeType::Unknown) {
            nodes.append(QJsonValue(node.rawData));
            continue;
        }
        OrderedJson obj = OrderedJson::object();
        // ...unknownData first, then the modelled keys in getData()'s order
        // (app/own/2020-O7.js:167-183 + the subclass appends).
        for (auto it = node.extraData.constBegin(); it != node.extraData.constEnd(); ++it)
            obj.set(it.key(), it.value());
        obj.set(QStringLiteral("id"), node.id);
        obj.set(QStringLiteral("x"), node.x);
        obj.set(QStringLiteral("y"), node.y);
        obj.set(QStringLiteral("width"), node.width);
        obj.set(QStringLiteral("height"), node.height);
        if (!node.color.isEmpty() || node.explicitKeys.contains(QStringLiteral("color")))
            obj.set(QStringLiteral("color"), node.color);

        switch (node.type) {
        case NodeType::Text:
            obj.set(QStringLiteral("type"), QStringLiteral("text"));
            obj.set(QStringLiteral("text"), node.text);
            break;
        case NodeType::File:
            obj.set(QStringLiteral("type"), QStringLiteral("file"));
            obj.set(QStringLiteral("file"), node.file);
            if (!node.subpath.isEmpty() || node.explicitKeys.contains(QStringLiteral("subpath")))
                obj.set(QStringLiteral("subpath"), node.subpath);
            break;
        case NodeType::Link:
            obj.set(QStringLiteral("type"), QStringLiteral("link"));
            obj.set(QStringLiteral("url"), node.url);
            break;
        case NodeType::Group: {
            obj.set(QStringLiteral("type"), QStringLiteral("group"));
            if (!node.label.isEmpty() || node.explicitKeys.contains(QStringLiteral("label")))
                obj.set(QStringLiteral("label"), node.label);
            const bool hasBackground = !node.background.isEmpty();
            if (hasBackground
                || node.explicitKeys.contains(QStringLiteral("background")))
                obj.set(QStringLiteral("background"), node.background);
            // getData() writes backgroundStyle only alongside a background and
            // only when it is not "cover" (app/own/2022-W7.js:617) — but the
            // key stays in unknownData either way, so an explicit one survives.
            const bool styleIsDefault = node.backgroundStyle.isEmpty()
                || node.backgroundStyle == QLatin1String("cover");
            if ((hasBackground && !styleIsDefault)
                || node.explicitKeys.contains(QStringLiteral("backgroundStyle")))
                obj.set(QStringLiteral("backgroundStyle"), node.backgroundStyle);
            break;
        }
        case NodeType::Unknown:
            break;
        }
        nodes.appendObject(obj);
    }
    root.setObject(QStringLiteral("nodes"), nodes);

    // Edges are not sorted upstream — Map insertion order survives the save.
    OrderedJson edges = OrderedJson::array();
    for (const auto &id : m_edgeOrder) {
        const auto &edge = m_edges[id];
        if (edge.unresolved) {
            edges.append(QJsonValue(edge.rawData));
            continue;
        }
        OrderedJson obj = OrderedJson::object();
        for (auto it = edge.extraData.constBegin(); it != edge.extraData.constEnd(); ++it)
            obj.set(it.key(), it.value());
        obj.set(QStringLiteral("id"), edge.id);
        obj.set(QStringLiteral("fromNode"), edge.fromNode);
        obj.set(QStringLiteral("fromSide"), sideToString(edge.fromSide));
        obj.set(QStringLiteral("toNode"), edge.toNode);
        obj.set(QStringLiteral("toSide"), sideToString(edge.toSide));
        if (edge.fromEnd != EndType::None
            || edge.explicitKeys.contains(QStringLiteral("fromEnd")))
            obj.set(QStringLiteral("fromEnd"), endTypeToString(edge.fromEnd));
        if (edge.toEnd != EndType::Arrow
            || edge.explicitKeys.contains(QStringLiteral("toEnd")))
            obj.set(QStringLiteral("toEnd"), endTypeToString(edge.toEnd));
        if (!edge.color.isEmpty() || edge.explicitKeys.contains(QStringLiteral("color")))
            obj.set(QStringLiteral("color"), edge.color);
        if (!edge.label.isEmpty() || edge.explicitKeys.contains(QStringLiteral("label")))
            obj.set(QStringLiteral("label"), edge.label);
        edges.appendObject(obj);
    }
    root.setObject(QStringLiteral("edges"), edges);

    for (auto it = m_extraTopLevel.constBegin(); it != m_extraTopLevel.constEnd(); ++it)
        root.set(it.key(), it.value());
    return root;
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
    file.write(serializeObsidianStyle());
    m_modified = false;
    Q_EMIT modificationChanged(false);
    return true;
}

void CanvasDocument::addNode(const CanvasNode &node)
{
    if (!m_nodes.contains(node.id))
        m_nodeOrder.append(node.id);
    m_nodeZIndex.insert(node.id, m_nodeOrder.size() - 1);
    m_nodes.insert(node.id, node);
    refreshUnresolvedEdges();
    m_modified = true;
    Q_EMIT nodeAdded(node.id);
    Q_EMIT modificationChanged(true);
}

void CanvasDocument::removeNode(const QString &id)
{
    m_nodes.remove(id);
    m_nodeOrder.removeAll(id);
    m_nodeZIndex.remove(id);
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
    QVector<QString> paintOrder = m_nodeOrder;
    std::stable_sort(paintOrder.begin(), paintOrder.end(),
                     [this](const QString &a, const QString &b) {
                         return m_nodeZIndex.value(a) < m_nodeZIndex.value(b);
                     });
    for (const auto &id : paintOrder)
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

} // namespace Corbomite::Canvas