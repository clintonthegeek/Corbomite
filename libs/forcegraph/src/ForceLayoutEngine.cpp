// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceLayoutEngine.h"
#include "forcegraph/QuadTree.h"

#include <QQueue>
#include <QRandomGenerator>
#include <QSet>
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
    buildAdjacency();
    computeDegrees();
}

void ForceLayoutEngine::clear()
{
    m_nodes.clear();
    m_edges.clear();
    m_nodeIndex.clear();
    m_adjacency.clear();
    m_degree.clear();
    m_displacements.clear();
    m_previousForces.clear();
    m_iteration = 0;
    m_stableCount = 0;
    m_energyDecreaseCount = 0;
    m_stable = false;
    m_globalSpeed = 1.0;
    m_energy = 0.0;
    m_prevEnergy = 0.0;
}

void ForceLayoutEngine::buildNodeIndex()
{
    m_nodeIndex.clear();
    for (int i = 0; i < m_nodes.size(); ++i) {
        m_nodeIndex[m_nodes[i].id] = i;
    }
}

void ForceLayoutEngine::buildAdjacency()
{
    m_adjacency.clear();
    // Pre-insert all node IDs so disconnected nodes appear in the adjacency map
    for (const auto &node : m_nodes) {
        m_adjacency[node.id]; // default-construct empty vector
    }
    for (const auto &edge : m_edges) {
        m_adjacency[edge.sourceId].append(edge.targetId);
        m_adjacency[edge.targetId].append(edge.sourceId);
    }
}

void ForceLayoutEngine::computeDegrees()
{
    m_degree.clear();
    for (const auto &edge : m_edges) {
        m_degree[edge.sourceId]++;
        m_degree[edge.targetId]++;
    }
}

void ForceLayoutEngine::bfsInitialPlacement()
{
    if (m_nodes.size() <= 1) return;

    // Only run if all nodes are at origin (first layout)
    bool allAtOrigin = true;
    for (const auto &node : m_nodes) {
        if (std::abs(node.position.x()) > EPSILON || std::abs(node.position.y()) > EPSILON) {
            allAtOrigin = false;
            break;
        }
    }
    if (!allAtOrigin) return;

    // Find connected components via BFS
    QVector<QVector<QString>> components;
    QSet<QString> visited;

    for (const auto &node : m_nodes) {
        if (visited.contains(node.id)) continue;

        QVector<QString> component;
        QQueue<QString> queue;
        queue.enqueue(node.id);
        visited.insert(node.id);

        while (!queue.isEmpty()) {
            QString current = queue.dequeue();
            component.append(current);

            const auto &neighbors = m_adjacency.value(current);
            for (const auto &neighbor : neighbors) {
                if (!visited.contains(neighbor)) {
                    visited.insert(neighbor);
                    queue.enqueue(neighbor);
                }
            }
        }
        components.append(component);
    }

    // Sort components by size descending (largest first)
    std::sort(components.begin(), components.end(),
              [](const QVector<QString> &a, const QVector<QString> &b) {
                  return a.size() > b.size();
              });

    double offsetX = 0.0;
    // Small gap — center force pulls components together during simulation.
    // We WANT disconnected components to overlap initially so the layout
    // looks like one organic graph (matching Obsidian's behavior).
    double gap = m_linkDistance * 0.5;

    for (const auto &component : components) {
        if (component.size() == 1) {
            // Isolated node: place at offset
            int idx = m_nodeIndex.value(component[0], -1);
            if (idx >= 0 && !m_nodes[idx].pinned) {
                m_nodes[idx].position = QPointF(offsetX, 0.0);
            }
            offsetX += gap;
            continue;
        }

        // Convert component to QSet for O(1) membership tests
        QSet<QString> componentSet(component.begin(), component.end());

        // Two-pass BFS to find approximate diameter endpoints
        // Pass 1: BFS from arbitrary node to find farthest
        auto bfsFarthest = [&](const QString &startId) -> QString {
            QHash<QString, int> dist;
            QQueue<QString> q;
            q.enqueue(startId);
            dist[startId] = 0;
            QString farthest = startId;
            int maxDist = 0;

            while (!q.isEmpty()) {
                QString current = q.dequeue();
                const auto &neighbors = m_adjacency.value(current);
                for (const auto &neighbor : neighbors) {
                    if (!dist.contains(neighbor) && componentSet.contains(neighbor)) {
                        dist[neighbor] = dist[current] + 1;
                        if (dist[neighbor] > maxDist) {
                            maxDist = dist[neighbor];
                            farthest = neighbor;
                        }
                        q.enqueue(neighbor);
                    }
                }
            }
            return farthest;
        };

        QString endpoint1 = bfsFarthest(component[0]);
        QString endpoint2 = bfsFarthest(endpoint1);
        Q_UNUSED(endpoint2); // endpoint2 is the other end of the diameter

        // BFS from endpoint1 to assign layers
        QHash<QString, int> layer;
        QQueue<QString> queue;
        queue.enqueue(endpoint1);
        layer[endpoint1] = 0;
        int maxLayer = 0;

        while (!queue.isEmpty()) {
            QString current = queue.dequeue();
            const auto &neighbors = m_adjacency.value(current);
            for (const auto &neighbor : neighbors) {
                if (!layer.contains(neighbor) && componentSet.contains(neighbor)) {
                    layer[neighbor] = layer[current] + 1;
                    maxLayer = std::max(maxLayer, layer[neighbor]);
                    queue.enqueue(neighbor);
                }
            }
        }

        // Count nodes per layer for angular distribution
        QHash<int, int> layerCounts;
        QHash<int, int> layerCurrentIndex;
        for (const auto &nodeId : component) {
            int l = layer.value(nodeId, 0);
            layerCounts[l]++;
            layerCurrentIndex[l] = 0;
        }

        // Place nodes on concentric rings
        auto *rng = QRandomGenerator::global();
        double componentMaxRadius = 0.0;

        for (const auto &nodeId : component) {
            int idx = m_nodeIndex.value(nodeId, -1);
            if (idx < 0) continue;
            if (m_nodes[idx].pinned) continue;

            int l = layer.value(nodeId, 0);
            double radius = l * m_linkDistance;
            componentMaxRadius = std::max(componentMaxRadius, radius);

            int count = layerCounts[l];
            int index = layerCurrentIndex[l]++;

            double angle;
            if (count == 1) {
                angle = 0.0;
            } else {
                double angleStep = 2.0 * M_PI / count;
                angle = index * angleStep;
            }
            // Small jitter to prevent exact overlaps
            angle += (rng->generateDouble() - 0.5) * 0.2;

            m_nodes[idx].position = QPointF(
                offsetX + radius * std::cos(angle),
                radius * std::sin(angle)
            );
        }

        offsetX += 2.0 * componentMaxRadius + gap;
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
    m_energyDecreaseCount = 0;
    m_iteration = 0;

    bfsInitialPlacement();

    int n = m_nodes.size();
    m_displacements.resize(n);
    m_previousForces.resize(n);
    for (int i = 0; i < n; ++i) {
        m_displacements[i] = QPointF(0, 0);
        m_previousForces[i] = QPointF(0, 0);
    }
    m_globalSpeed = 1.0;
    m_energy = 0.0;
    m_prevEnergy = 0.0;

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
        bfsInitialPlacement();
        m_displacements.resize(n);
        m_previousForces.resize(n);
        for (int i = 0; i < n; ++i) {
            m_displacements[i] = QPointF(0, 0);
            m_previousForces[i] = QPointF(0, 0);
        }
        m_globalSpeed = 1.0;
        m_energy = 0.0;
        m_prevEnergy = 0.0;
        m_energyDecreaseCount = 0;
    }

    // 1. Compute canvas area and optimal spacing
    double canvasArea = estimateCanvasArea();
    double k = m_linkDistance; // Target spacing for connected nodes

    // 2. Reset current displacements (previous forces saved at end of step)
    for (int i = 0; i < n; ++i) {
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

        // Build quadtree with degree-weighted masses
        QVector<double> masses(n);
        for (int i = 0; i < n; ++i) {
            masses[i] = m_degree.value(m_nodes[i].id, 0) + 1.0;
        }

        QuadTree quadTree;
        quadTree.build(m_nodes, bounds, masses);

        for (int i = 0; i < n; ++i) {
            double nodeMass = m_degree.value(m_nodes[i].id, 0) + 1.0;
            QPointF repulsion = quadTree.computeRepulsion(
                m_nodes[i].position, m_repelForce, nodeMass, m_theta);
            m_displacements[i] += repulsion;
        }
    } else {
        // Naive O(n^2) repulsion with degree weighting (sqrt for gentler curve)
        for (int i = 0; i < n; ++i) {
            double degI = m_degree.value(m_nodes[i].id, 0) + 1.0;
            for (int j = i + 1; j < n; ++j) {
                QPointF delta = m_nodes[i].position - m_nodes[j].position;
                double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
                dist = std::max(dist, EPSILON);

                double degJ = m_degree.value(m_nodes[j].id, 0) + 1.0;
                double force = m_repelForce * std::sqrt(degI * degJ) / (dist * dist);

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

    // 5. Apply center force (pull toward origin), weighted by degree
    for (int i = 0; i < n; ++i) {
        double deg = m_degree.value(m_nodes[i].id, 0) + 1.0;
        m_displacements[i] -= m_nodes[i].position * m_centerForce * deg;
    }

    // 6. ForceAtlas2 Adaptive Speed

    // Compute per-node swinging and traction (degree-weighted)
    double globalSwinging = 0.0;
    double globalTraction = 0.0;

    for (int i = 0; i < n; ++i) {
        double deg = m_degree.value(m_nodes[i].id, 0) + 1.0;

        // Swinging = change in force direction
        double swingX = m_displacements[i].x() - m_previousForces[i].x();
        double swingY = m_displacements[i].y() - m_previousForces[i].y();
        double swinging = std::sqrt(swingX * swingX + swingY * swingY);

        // Traction = useful convergent movement
        double tractX = (m_displacements[i].x() + m_previousForces[i].x()) / 2.0;
        double tractY = (m_displacements[i].y() + m_previousForces[i].y()) / 2.0;
        double traction = std::sqrt(tractX * tractX + tractY * tractY);

        globalSwinging += deg * swinging;
        globalTraction += deg * traction;
    }

    // Compute global speed
    double tolerance = 1.0;
    if (globalSwinging > EPSILON) {
        m_globalSpeed = tolerance * globalTraction / globalSwinging;
    }
    // Prevent speed from growing too fast
    m_globalSpeed = std::min(m_globalSpeed, m_globalSpeed * 1.5);
    m_globalSpeed = std::max(m_globalSpeed, EPSILON);

    // 7. Apply displacement with per-node speed
    double maxDisplacement = 0.0;
    m_energy = 0.0;

    for (int i = 0; i < n; ++i) {
        if (m_nodes[i].pinned) continue;

        double deg = m_degree.value(m_nodes[i].id, 0) + 1.0;

        // Per-node swinging
        double swingX = m_displacements[i].x() - m_previousForces[i].x();
        double swingY = m_displacements[i].y() - m_previousForces[i].y();
        double swinging = std::sqrt(swingX * swingX + swingY * swingY);

        // Per-node speed
        double localSpeed = m_globalSpeed / (1.0 + m_globalSpeed * std::sqrt(swinging));

        // Compute displacement
        double dx = m_displacements[i].x() * localSpeed;
        double dy = m_displacements[i].y() * localSpeed;
        double mag = std::sqrt(dx * dx + dy * dy);

        // Cap displacement at 10x node radius
        double maxDisp = std::max(10.0 * m_nodes[i].radius, 10.0);
        if (mag > maxDisp) {
            double scale = maxDisp / mag;
            dx *= scale;
            dy *= scale;
            mag = maxDisp;
        }

        m_nodes[i].position += QPointF(dx, dy);
        maxDisplacement = std::max(maxDisplacement, mag);
        m_energy += deg * std::sqrt(m_displacements[i].x() * m_displacements[i].x() +
                                     m_displacements[i].y() * m_displacements[i].y());
    }

    // Save forces for next iteration's swinging calculation
    m_previousForces = m_displacements;

    // 8. Convergence detection (energy-based + displacement-based fallback)

    // Energy-based convergence
    if (m_prevEnergy > EPSILON && m_energy < m_prevEnergy * 0.999) {
        ++m_energyDecreaseCount;
        if (m_energyDecreaseCount >= 10 && !m_stable) {
            m_stable = true;
            Q_EMIT simulationStable();
            if (m_running) {
                stop();
            }
        }
    } else {
        m_energyDecreaseCount = 0;
    }

    // Displacement-based fallback
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

    // Adaptive theta: use higher theta (faster, less accurate) when energy is high,
    // lower theta (slower, more accurate) when energy is low and layout is settling
    if (m_prevEnergy > 0 && m_energy > 0) {
        double energyRatio = m_energy / m_prevEnergy;
        if (energyRatio > 0.95) {
            m_theta = 1.2;   // Still lots of movement — be fast
        } else if (energyRatio > 0.5) {
            m_theta = 0.8;   // Moderate — balanced
        } else {
            m_theta = 0.5;   // Settling — be precise
        }
    }

    m_prevEnergy = m_energy;

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
