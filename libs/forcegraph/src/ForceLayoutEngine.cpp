// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceLayoutEngine.h"
#include "forcegraph/QuadTree.h"

#include <QRandomGenerator>
#include <QTimer>
#include <cmath>

namespace ForceGraph {

static constexpr double EPSILON = 0.001;
static constexpr int BARNES_HUT_THRESHOLD = 100;
static constexpr int STABLE_ITERATIONS_REQUIRED = 5;
static constexpr double MIN_CANVAS_AREA = 10000.0;
static constexpr double TIMER_INTERVAL_MS = 33;

ForceLayoutEngine::ForceLayoutEngine(QObject *parent)
    : QObject(parent)
{
}

void ForceLayoutEngine::setNodes(const QVector<GraphNode> &nodes)
{
    m_nodes = nodes;
    buildNodeIndex();
}

void ForceLayoutEngine::setEdges(const QVector<GraphEdge> &edges)
{
    m_edges = edges;
}

void ForceLayoutEngine::clear()
{
    m_nodes.clear();
    m_edges.clear();
    m_nodeIndex.clear();
    m_displacements.clear();
    m_prevDisplacements.clear();
    m_vertexTemperatures.clear();
    m_iteration = 0;
    m_stableCount = 0;
    m_stable = false;
    m_temperature = 0.0;
    m_initialTemperature = 0.0;
}

void ForceLayoutEngine::buildNodeIndex()
{
    m_nodeIndex.clear();
    for (int i = 0; i < m_nodes.size(); ++i) {
        m_nodeIndex[m_nodes[i].id] = i;
    }
}

double ForceLayoutEngine::estimateCanvasArea() const
{
    if (m_nodes.size() < 2) return MIN_CANVAS_AREA;

    double minX = m_nodes[0].position.x();
    double maxX = minX;
    double minY = m_nodes[0].position.y();
    double maxY = minY;

    for (const auto &node : m_nodes) {
        minX = std::min(minX, node.position.x());
        maxX = std::max(maxX, node.position.x());
        minY = std::min(minY, node.position.y());
        maxY = std::max(maxY, node.position.y());
    }

    double width = std::max(maxX - minX, 100.0);
    double height = std::max(maxY - minY, 100.0);

    // Multiply by 4 to give room for expansion
    return std::max(width * height * 4.0, MIN_CANVAS_AREA);
}

void ForceLayoutEngine::randomizePositionsIfNeeded()
{
    bool allAtOrigin = true;
    for (const auto &node : m_nodes) {
        if (std::abs(node.position.x()) > EPSILON || std::abs(node.position.y()) > EPSILON) {
            allAtOrigin = false;
            break;
        }
    }

    if (allAtOrigin && m_nodes.size() > 1) {
        double radius = std::sqrt(static_cast<double>(m_nodes.size())) * 50.0;
        auto *rng = QRandomGenerator::global();
        for (auto &node : m_nodes) {
            double angle = rng->generateDouble() * 2.0 * M_PI;
            double r = rng->generateDouble() * radius;
            node.position = QPointF(r * std::cos(angle), r * std::sin(angle));
        }
    }
}

void ForceLayoutEngine::start()
{
    if (m_running) return;

    m_running = true;
    m_stable = false;
    m_stableCount = 0;
    m_iteration = 0;

    // Scale iterations with graph size — large graphs need more time to reach equilibrium
    m_maxIterations = std::max(500, static_cast<int>(std::sqrt(static_cast<double>(m_nodes.size())) * 100.0));

    randomizePositionsIfNeeded();

    double area = estimateCanvasArea();
    m_initialTemperature = std::sqrt(area) / 10.0;
    m_temperature = m_initialTemperature;

    int n = m_nodes.size();
    m_displacements.resize(n);
    m_prevDisplacements.resize(n);
    m_vertexTemperatures.resize(n);
    for (int i = 0; i < n; ++i) {
        m_displacements[i] = QPointF(0, 0);
        m_prevDisplacements[i] = QPointF(0, 0);
        m_vertexTemperatures[i] = m_initialTemperature;
    }

    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &ForceLayoutEngine::step);
    }
    // Scale timer interval based on node count to avoid freezing the GUI
    int interval = static_cast<int>(TIMER_INTERVAL_MS);
    if (n > 2000) interval = 200;
    else if (n > 500) interval = 100;
    else if (n > 200) interval = 50;
    m_timer->start(interval);

    Q_EMIT simulationStarted();
}

void ForceLayoutEngine::stop()
{
    if (!m_running) return;

    m_running = false;
    if (m_timer) {
        m_timer->stop();
    }

    Q_EMIT simulationStopped();
}

void ForceLayoutEngine::step()
{
    const int n = m_nodes.size();
    if (n == 0) return;

    // --- Initialize per-step state on first call (if not started via start()) ---
    if (m_displacements.size() != n) {
        randomizePositionsIfNeeded();
        double area = estimateCanvasArea();
        m_initialTemperature = std::sqrt(area) / 10.0;
        m_temperature = m_initialTemperature;
        m_displacements.resize(n);
        m_prevDisplacements.resize(n);
        m_vertexTemperatures.resize(n);
        for (int i = 0; i < n; ++i) {
            m_displacements[i] = QPointF(0, 0);
            m_prevDisplacements[i] = QPointF(0, 0);
            m_vertexTemperatures[i] = m_initialTemperature;
        }
    }

    // 1. Compute canvas area and optimal spacing
    double canvasArea = estimateCanvasArea();
    double k = m_linkDistance; // Target spacing for connected nodes

    // 2. Save previous displacements and reset current
    for (int i = 0; i < n; ++i) {
        m_prevDisplacements[i] = m_displacements[i];
        m_displacements[i] = QPointF(0, 0);
    }

    // 3. Compute repulsive forces
    // Uses f_r = repelForce / d^2, consistent between naive and Barnes-Hut paths
    if (n > BARNES_HUT_THRESHOLD) {
        // Barnes-Hut: build quadtree and query repulsion
        double minX = m_nodes[0].position.x();
        double maxX = minX;
        double minY = m_nodes[0].position.y();
        double maxY = minY;
        for (const auto &node : m_nodes) {
            minX = std::min(minX, node.position.x());
            maxX = std::max(maxX, node.position.x());
            minY = std::min(minY, node.position.y());
            maxY = std::max(maxY, node.position.y());
        }
        double margin = std::max(maxX - minX, maxY - minY) * 0.1 + 1.0;
        QRectF bounds(minX - margin, minY - margin,
                      (maxX - minX) + 2 * margin, (maxY - minY) + 2 * margin);

        QuadTree quadTree;
        quadTree.build(m_nodes, bounds);

        for (int i = 0; i < n; ++i) {
            QPointF repulsion = quadTree.computeRepulsion(
                m_nodes[i].position, m_repelForce, 0.8);
            m_displacements[i] += repulsion;
        }
    } else {
        // Naive O(n^2) repulsion: f_r = repelForce / d^2
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF delta = m_nodes[i].position - m_nodes[j].position;
                double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
                dist = std::max(dist, EPSILON);

                double force = m_repelForce / (dist * dist);

                QPointF normalized(delta.x() / dist, delta.y() / dist);
                m_displacements[i] += normalized * force;
                m_displacements[j] -= normalized * force;
            }
        }
    }

    // 4. Compute attractive forces along edges
    // Spring-like: f_a = (d - linkDistance) * linkForce, pulling toward target distance
    for (const auto &edge : m_edges) {
        auto srcIt = m_nodeIndex.find(edge.sourceId);
        auto tgtIt = m_nodeIndex.find(edge.targetId);
        if (srcIt == m_nodeIndex.end() || tgtIt == m_nodeIndex.end()) continue;

        int srcIdx = srcIt.value();
        int tgtIdx = tgtIt.value();

        QPointF delta = m_nodes[srcIdx].position - m_nodes[tgtIdx].position;
        double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
        dist = std::max(dist, EPSILON);

        double force = (dist - k) * m_linkForce;

        QPointF normalized(delta.x() / dist, delta.y() / dist);
        m_displacements[srcIdx] -= normalized * force;
        m_displacements[tgtIdx] += normalized * force;
    }

    // 5. Apply center force (pull toward origin)
    for (int i = 0; i < n; ++i) {
        m_displacements[i] -= m_nodes[i].position * m_centerForce;
    }

    // 6. Oscillation detection (Frick et al.) — per-vertex local temperature
    // Exponential cooling — slower decay gives nodes more time to spread
    double progress = static_cast<double>(m_iteration) / static_cast<double>(m_maxIterations);
    double globalTemp = m_initialTemperature * std::exp(-3.0 * progress);
    // This keeps temperature at ~5% of initial at iteration = maxIterations
    // vs linear which reaches 0 — exponential gives more movement in middle iterations
    globalTemp = std::max(globalTemp, 0.01);

    for (int i = 0; i < n; ++i) {
        double dot = m_displacements[i].x() * m_prevDisplacements[i].x()
                   + m_displacements[i].y() * m_prevDisplacements[i].y();

        if (dot < 0) {
            // Oscillating — reduce local temperature
            m_vertexTemperatures[i] *= 0.9;
        } else {
            // Converging — allow slight increase, capped by global temp
            m_vertexTemperatures[i] = std::min(m_vertexTemperatures[i] * 1.1, globalTemp);
        }
    }

    // 7. Limit displacement by temperature and apply damping; track max displacement
    double maxDisplacement = 0.0;

    for (int i = 0; i < n; ++i) {
        double mag = std::sqrt(m_displacements[i].x() * m_displacements[i].x()
                             + m_displacements[i].y() * m_displacements[i].y());

        if (mag < EPSILON) continue;

        double limitedMag = std::min(mag, m_vertexTemperatures[i]);
        double scale = (limitedMag / mag) * m_damping;

        m_displacements[i] *= scale;

        double finalMag = limitedMag * m_damping;
        maxDisplacement = std::max(maxDisplacement, finalMag);
    }

    // 8. Update positions (skip pinned nodes)
    for (int i = 0; i < n; ++i) {
        if (m_nodes[i].pinned) continue;
        m_nodes[i].position += m_displacements[i];
    }

    // 9. Check convergence
    double convergenceThreshold = canvasArea * 0.0001;
    if (maxDisplacement < convergenceThreshold) {
        ++m_stableCount;
        if (m_stableCount >= STABLE_ITERATIONS_REQUIRED && !m_stable) {
            m_stable = true;
            Q_EMIT simulationStable();
            if (m_running) {
                stop();
            }
        }
    } else {
        m_stableCount = 0;
    }

    // 10. Emit positionsUpdated
    QHash<QString, QPointF> positions;
    positions.reserve(n);
    for (int i = 0; i < n; ++i) {
        positions[m_nodes[i].id] = m_nodes[i].position;
    }
    Q_EMIT positionsUpdated(positions);

    // 11. Increment iteration counter
    ++m_iteration;
}

bool ForceLayoutEngine::isRunning() const
{
    return m_running;
}

bool ForceLayoutEngine::isStable() const
{
    return m_stable;
}

void ForceLayoutEngine::pinNode(const QString &id, QPointF position)
{
    auto it = m_nodeIndex.find(id);
    if (it == m_nodeIndex.end()) return;

    int idx = it.value();
    m_nodes[idx].pinned = true;
    m_nodes[idx].position = position;
}

void ForceLayoutEngine::unpinNode(const QString &id)
{
    auto it = m_nodeIndex.find(id);
    if (it == m_nodeIndex.end()) return;

    m_nodes[it.value()].pinned = false;
}

void ForceLayoutEngine::randomizePositions()
{
    if (m_nodes.isEmpty()) return;

    double radius = std::sqrt(static_cast<double>(m_nodes.size())) * 50.0;
    auto *rng = QRandomGenerator::global();
    for (auto &node : m_nodes) {
        if (node.pinned) continue;
        double angle = rng->generateDouble() * 2.0 * M_PI;
        double r = rng->generateDouble() * radius;
        node.position = QPointF(r * std::cos(angle), r * std::sin(angle));
    }
}

void ForceLayoutEngine::setCenterForce(double f) { m_centerForce = f; }
void ForceLayoutEngine::setRepelForce(double f) { m_repelForce = f; }
void ForceLayoutEngine::setLinkForce(double f) { m_linkForce = f; }
void ForceLayoutEngine::setLinkDistance(double d) { m_linkDistance = d; }
void ForceLayoutEngine::setDamping(double d) { m_damping = d; }

QVector<GraphNode> ForceLayoutEngine::nodes() const { return m_nodes; }
int ForceLayoutEngine::nodeCount() const { return m_nodes.size(); }
int ForceLayoutEngine::edgeCount() const { return m_edges.size(); }

} // namespace ForceGraph
