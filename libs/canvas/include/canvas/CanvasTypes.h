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
