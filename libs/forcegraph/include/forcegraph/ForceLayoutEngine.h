// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QHash>
#include <QPointF>
#include <QVector>
#include "GraphTypes.h"

class QTimer;

namespace ForceGraph {
class ForceLayoutEngine : public QObject {
    Q_OBJECT
public:
    explicit ForceLayoutEngine(QObject *parent = nullptr);
    void setNodes(const QVector<GraphNode> &nodes);
    void setEdges(const QVector<GraphEdge> &edges);
    void clear();
    void start();
    void stop();
    void step();
    bool isRunning() const;
    bool isStable() const;
    void pinNode(const QString &id, QPointF position);
    void unpinNode(const QString &id);
    void setCenterForce(double force);
    void setRepelForce(double force);
    void setLinkForce(double force);
    void setLinkDistance(double distance);
    void setDamping(double damping);

    // Getters, added for Cluster O Phase O6.T3. The engine had setters only,
    // which made it impossible to assert that a GraphControlsPanel and the
    // engine it drives actually agree — the O6.T3 test needed exactly that
    // check, and "assume they match because both start at defaults" is not a
    // falsifiable claim once a panel is created lazily.
    double centerForce() const { return m_centerForce; }
    double repelForce() const { return m_repelForce; }
    double linkForce() const { return m_linkForce; }
    double linkDistance() const { return m_linkDistance; }
    void randomizePositions();
    QVector<GraphNode> nodes() const;
    int nodeCount() const;
    int edgeCount() const;
Q_SIGNALS:
    void positionsUpdated(const QHash<QString, QPointF> &positions);
    void simulationStarted();
    void simulationStopped();
    void simulationStable();
private:
    void buildNodeIndex();
    void buildAdjacency();
    void computeDegrees();
    void bfsInitialPlacement();
    double estimateCanvasArea() const;
    void randomizePositionsIfNeeded();

    QVector<GraphNode> m_nodes;
    QVector<GraphEdge> m_edges;
    QHash<QString, int> m_nodeIndex;
    QHash<QString, QVector<QString>> m_adjacency;
    QHash<QString, int> m_degree;
    QVector<QPointF> m_displacements;
    QVector<QPointF> m_previousForces;
    double m_theta = 0.8;
    double m_centerForce = 0.01;
    double m_repelForce = 1500.0;
    double m_linkForce = 0.05;
    double m_linkDistance = 100.0;
    double m_damping = 0.85; // kept for backward compat (no-op with adaptive speed)
    double m_globalSpeed = 1.0;
    double m_energy = 0.0;
    double m_prevEnergy = 0.0;
    int m_iteration = 0;
    int m_stableCount = 0;
    int m_energyDecreaseCount = 0;
    bool m_running = false;
    bool m_stable = false;
    ::QTimer *m_timer = nullptr;
};
} // namespace ForceGraph
