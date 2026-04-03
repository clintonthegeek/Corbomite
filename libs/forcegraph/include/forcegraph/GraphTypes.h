// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QPointF>
#include <QString>

namespace ForceGraph {

enum class NodeType {
    Regular,    // Normal note with links
    Hub,        // High-degree note (6+ links)
    Orphan,     // Zero connections
    Unresolved, // Link target that doesn't exist as a file
    DailyNote,  // Daily/journal note (YYYY-MM-DD pattern)
};

struct GraphNode {
    QString id;
    QString label;
    QString tooltip;
    double radius = 5.0;
    QColor color = QColor(123, 108, 217);
    QPointF position;
    int degree = 0;
    NodeType type = NodeType::Regular;
    bool pinned = false;
};

struct GraphEdge {
    QString sourceId;
    QString targetId;
};

} // namespace ForceGraph
