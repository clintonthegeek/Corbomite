// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsLineItem>
namespace ForceGraph {
class ForceGraphNode;
class ForceGraphEdge : public QGraphicsLineItem {
public:
    ForceGraphEdge(ForceGraphNode *source, ForceGraphNode *target, QGraphicsItem *parent = nullptr);
    void adjust();
    ForceGraphNode *sourceNode() const;
    ForceGraphNode *targetNode() const;
    void setDimmed(bool dimmed);
private:
    ForceGraphNode *m_source;
    ForceGraphNode *m_target;
    bool m_dimmed = false;
};
} // namespace ForceGraph
