// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphEdge.h"
#include "forcegraph/ForceGraphNode.h"
namespace ForceGraph {
ForceGraphEdge::ForceGraphEdge(ForceGraphNode *s, ForceGraphNode *t, QGraphicsItem *p)
    : QGraphicsLineItem(p), m_source(s), m_target(t) {}
void ForceGraphEdge::adjust() {}
ForceGraphNode *ForceGraphEdge::sourceNode() const { return m_source; }
ForceGraphNode *ForceGraphEdge::targetNode() const { return m_target; }
void ForceGraphEdge::setDimmed(bool d) { m_dimmed = d; }
} // namespace ForceGraph
