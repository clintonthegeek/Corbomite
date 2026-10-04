// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QJsonObject>
#include <QSet>
#include <QString>

namespace Canvas {

// Unknown: a `type` this build does not model. Upstream's dispatch ends in
// `if ("group" !== u.type) continue` (app/own/2018-k7.js:424) and getData
// rebuilds the array from the live registry, so Obsidian *deletes* such a node
// on the next save — we keep it instead (see CanvasNode::rawData).
enum class NodeType { Text, File, Link, Group, Unknown };
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

    // Unknown JSON fields on this node preserved verbatim across load→save
    // (Obsidian spreads ...unknownData; we must too).
    QJsonObject extraData;

    // Set only for NodeType::Unknown: the `type` string exactly as read, and
    // the whole source object. toJson() emits rawData verbatim instead of
    // modelling the node, so a type a newer Obsidian (or a plugin) introduced
    // survives a Corbomite save. Note that Qt sorted rawData's keys when the
    // file was parsed, so they come back alphabetically rather than in file
    // order — the keys and values are exact, the sequence is not.
    QString rawType;
    QJsonObject rawData;

    // Optional keys this node carried in the source file. Upstream's per-class
    // omit-lists move such keys into unknownData rather than deleting them, so
    // a key already holding the value getData() would have generated survives
    // the round trip: "color":"", "label":"", "subpath":"",
    // "backgroundStyle":"cover" all stay. Only these are tracked; required
    // fields are written unconditionally.
    QSet<QString> explicitKeys;
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

    // Unknown JSON fields on this edge preserved verbatim across load→save.
    QJsonObject extraData;

    // True while at least one endpoint id does not resolve to a node. Upstream
    // imports an edge only `if (o.has(y) && o.has(k))`
    // (app/own/2018-k7.js:442) and rebuilds the array from the live registry,
    // so a dangling edge is deleted on the next save. While set, rawData is
    // emitted verbatim and none of the modelled fields are written.
    bool unresolved = false;
    QJsonObject rawData;

    // See CanvasNode::explicitKeys — same rule for fromEnd/toEnd/color/label.
    QSet<QString> explicitKeys;
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
