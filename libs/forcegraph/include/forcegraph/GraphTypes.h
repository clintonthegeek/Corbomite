// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QPointF>
#include <QString>

namespace ForceGraph {

struct GraphNode {
    QString id;
    QString label;
    double radius = 5.0;
    QColor color = QColor(123, 108, 217);
    QPointF position;
    bool pinned = false;
};

struct GraphEdge {
    QString sourceId;
    QString targetId;
};

} // namespace ForceGraph
