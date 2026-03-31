// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceLayoutEngine.h"
namespace ForceGraph {
ForceLayoutEngine::ForceLayoutEngine(QObject *parent) : QObject(parent) {}
void ForceLayoutEngine::setNodes(const QVector<GraphNode> &n) { m_nodes = n; }
void ForceLayoutEngine::setEdges(const QVector<GraphEdge> &e) { m_edges = e; }
void ForceLayoutEngine::clear() { m_nodes.clear(); m_edges.clear(); }
void ForceLayoutEngine::start() {}
void ForceLayoutEngine::stop() {}
void ForceLayoutEngine::step() {}
bool ForceLayoutEngine::isRunning() const { return m_running; }
bool ForceLayoutEngine::isStable() const { return m_stable; }
void ForceLayoutEngine::pinNode(const QString &, QPointF) {}
void ForceLayoutEngine::unpinNode(const QString &) {}
void ForceLayoutEngine::setCenterForce(double f) { m_centerForce = f; }
void ForceLayoutEngine::setRepelForce(double f) { m_repelForce = f; }
void ForceLayoutEngine::setLinkForce(double f) { m_linkForce = f; }
void ForceLayoutEngine::setLinkDistance(double d) { m_linkDistance = d; }
void ForceLayoutEngine::setDamping(double d) { m_damping = d; }
QVector<GraphNode> ForceLayoutEngine::nodes() const { return m_nodes; }
int ForceLayoutEngine::nodeCount() const { return m_nodes.size(); }
int ForceLayoutEngine::edgeCount() const { return m_edges.size(); }
} // namespace ForceGraph
