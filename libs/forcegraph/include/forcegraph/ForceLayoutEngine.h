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
    double estimateCanvasArea() const;
    void randomizePositionsIfNeeded();

    QVector<GraphNode> m_nodes;
    QVector<GraphEdge> m_edges;
    QHash<QString, int> m_nodeIndex;
    QVector<QPointF> m_displacements;
    QVector<QPointF> m_prevDisplacements;
    QVector<double> m_vertexTemperatures;
    double m_centerForce = 0.01;
    double m_repelForce = 1500.0;
    double m_linkForce = 0.05;
    double m_linkDistance = 100.0;
    double m_damping = 0.85;
    double m_temperature = 0.0;
    double m_initialTemperature = 0.0;
    int m_iteration = 0;
    int m_maxIterations = 500;
    int m_stableCount = 0;
    bool m_running = false;
    bool m_stable = false;
    ::QTimer *m_timer = nullptr;
};
} // namespace ForceGraph
