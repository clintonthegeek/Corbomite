# Graph View Optimization & UX — Design Specification

## Overview

Comprehensive overhaul of the graph view: algorithm optimizations for large graphs (1,000-10,000+ nodes), rendering performance for QGraphicsView, and UX/visual improvements that make the graph useful rather than a pretty hairball.

Organized as a priority stack — each section is a standalone improvement. Implement top-down.

**Primary references:**
- Handbook of Graph Drawing (Tamassia, ed.), Chapter 12 — `docs/HandbookGraphDrawing.txt`
- GRIP: Gajer, Goodrich, Kobourov (JGAA 2004)
- ForceAtlas2: Jacomy et al. (PLOS ONE 2014)
- Walshaw: "A multilevel algorithm for force-directed graph drawing" (JGAA 2003)
- FM³: Hachul & Jünger (GD 2004)

---

## Priority 1: BFS Initial Placement (Replace Random)

**Impact:** 50-80% fewer iterations to converge. **Effort:** Low.

### Problem

`ForceLayoutEngine::randomizePositionsIfNeeded()` places nodes randomly in a circle. The simulation starts from a state with no relation to graph structure, requiring hundreds of iterations to untangle — visible as a slow "unraveling" animation.

### Algorithm: Radial BFS from Diameter Endpoint

```
procedure BFSInitialPlacement(nodes, edges):
    Build adjacency list from edges

    // 1. Find approximate diameter endpoints (two BFS passes)
    start = arbitrary node
    BFS from start → farthest = node at max distance     // pass 1
    BFS from farthest → opposite = node at max distance   // pass 2

    // 2. BFS from farthest to assign layers
    queue = [farthest]
    visited = {farthest}
    layer = {farthest: 0}
    maxLayer = 0

    while queue not empty:
        v = queue.dequeue()
        for each neighbor u of v:
            if u not in visited:
                visited.add(u)
                layer[u] = layer[v] + 1
                maxLayer = max(maxLayer, layer[u])
                queue.enqueue(u)

    // 3. Place nodes on concentric rings
    k = ideal_edge_length  // = C * sqrt(area / |V|), use existing m_linkDistance
    layerCounts = count nodes per layer
    for each node v:
        L = layer[v]
        radius = L * k
        indexInLayer = position of v among nodes in layer L
        angleStep = 2π / layerCounts[L]
        angle = indexInLayer * angleStep

        // Jitter to prevent exact overlaps
        angle += random(-0.1, 0.1)

        v.position = (radius * cos(angle), radius * sin(angle))
```

### Handling Disconnected Components

```
procedure ComponentAwarePlacement(nodes, edges):
    components = find connected components via BFS/DFS
    sort components by size (largest first)

    offsetX = 0
    gap = ideal_edge_length * 3

    for each component C:
        BFSInitialPlacement(C.nodes, C.edges)

        // Shift component to avoid overlap
        translate all nodes in C by (offsetX, 0)
        estimatedWidth = 2 * maxLayer * ideal_edge_length
        offsetX += estimatedWidth + gap
```

### Where to Implement

In `ForceLayoutEngine.cpp`, replace `randomizePositionsIfNeeded()` (or the existing `randomizePositions()`) with `bfsInitialPlacement()`. The adjacency list can be built from `m_edges`. Call it before `start()`.

### Complexity

O(|V| + |E|) for BFS — negligible compared to layout iterations.

---

## Priority 2: Degree-Weighted Repulsion (ForceAtlas2-Style)

**Impact:** Better layout for scale-free networks (most knowledge graphs are scale-free). **Effort:** Low.

### Problem

Current repulsion: `F_repel = repelForce / dist²` treats all nodes equally. High-degree hub nodes (like MOC/index notes) get buried in their many neighbors because they don't push hard enough.

### Formula

Replace uniform repulsion with degree-weighted:

```
F_repel(u, v) = repelForce * (deg(u) + 1) * (deg(v) + 1) / dist(u, v)²
```

Where `deg(v)` is the number of edges incident to node `v`. The `+1` prevents zero-degree orphans from having zero repulsion.

### Where to Implement

1. **Store degree per node.** In `ForceLayoutEngine`, after `setEdges()`, compute and store degree for each node:

```cpp
// In ForceLayoutEngine, after edges are set:
m_degree.clear();
for (const auto &edge : m_edges) {
    m_degree[edge.sourceId]++;
    m_degree[edge.targetId]++;
}
```

2. **Modify repulsion calculation.** In `computeRepulsiveForces()` (both brute-force and Barnes-Hut paths), change:

```cpp
// Before:
double force = m_repelForce / (dist * dist);

// After:
double degI = m_degree.value(nodeI.id, 0) + 1.0;
double degJ = m_degree.value(nodeJ.id, 0) + 1.0;
double force = m_repelForce * degI * degJ / (dist * dist);
```

3. **For Barnes-Hut:** The QuadTree stores aggregate mass. Change the mass of each node from 1.0 to `(degree + 1)`:

```cpp
// When inserting into QuadTree:
double mass = m_degree.value(node.id, 0) + 1.0;
```

Then the tree's center-of-mass computation naturally aggregates degree-weighted repulsion.

### Also: Degree-Weighted Gravity

The center force should also scale by degree to prevent hubs from drifting:

```cpp
// Before:
QPointF gravityForce = -pos * m_centerForce;

// After:
double deg = m_degree.value(node.id, 0) + 1.0;
QPointF gravityForce = -pos * m_centerForce * deg;
```

---

## Priority 3: Adaptive Speed (ForceAtlas2 Swinging/Traction)

**Impact:** Self-tuning convergence — no arbitrary `m_maxIterations`. Better final layouts. **Effort:** Medium.

### Problem

Current approach uses global exponential temperature decay: `T(t) = T₀ * exp(-3.0 * progress)`. This is arbitrary — it either cools too fast (poor layout) or too slow (wasted iterations). The simulation declares stability based on max displacement threshold, but the threshold is also arbitrary.

### ForceAtlas2 Adaptive Speed System

Replace temperature with per-node swinging detection:

```
procedure AdaptiveStep(nodes, forces, previousForces):
    // Per-node metrics
    for each node n:
        swinging[n] = |forces[n] - previousForces[n]|     // force direction change
        traction[n] = |forces[n] + previousForces[n]| / 2 // useful convergent movement

    // Global metrics (degree-weighted)
    globalSwinging = Σ (deg(n) + 1) * swinging[n]
    globalTraction = Σ (deg(n) + 1) * traction[n]

    // Compute global speed
    tolerance = 1.0  // tunable parameter
    globalSpeed = tolerance * globalTraction / globalSwinging

    // Prevent speed from growing too fast
    globalSpeed = min(globalSpeed, previousGlobalSpeed * 1.5)

    // Per-node speed and displacement
    for each node n:
        localSpeed = globalSpeed / (1.0 + globalSpeed * sqrt(swinging[n]))

        displacement = localSpeed * forces[n]

        // Cap: no node moves more than 10x its radius per step
        maxDisp = 10.0 * n.radius
        if |displacement| > maxDisp:
            displacement = normalize(displacement) * maxDisp

        n.position += displacement

    previousGlobalSpeed = globalSpeed
```

### Convergence Detection

Instead of arbitrary threshold, use energy:

```
energy = Σ (deg(n) + 1) * |forces[n]|

if energy < previousEnergy * 0.999 for 10 consecutive steps:
    simulation is stable → emit simulationStable()
```

### Where to Implement

Replace the temperature/displacement logic in `ForceLayoutEngine::step()` (approximately lines 240-275). Store `m_previousForces` as a `QHash<QString, QPointF>` alongside existing force storage. Store `m_previousGlobalSpeed` and `m_energy` as members.

Remove `m_maxIterations` — the simulation self-terminates via energy convergence.

---

## Priority 4: QGraphicsView Rendering Performance

**Impact:** ~2x rendering speed during simulation. **Effort:** Low.

### Changes to ForceGraphView

```cpp
// 1. During simulation: use FullViewportUpdate (the whole scene changes every frame)
void ForceGraphView::onSimulationStarted() {
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
}

void ForceGraphView::onSimulationStopped() {
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
}

// 2. Add DontAdjustForAntialiasing (already have DontSavePainterState)
setOptimizationFlags(DontSavePainterState | DontAdjustForAntialiasing);

// 3. Set explicit scene rect to avoid repeated itemsBoundingRect() calls
m_scene->setSceneRect(-10000, -10000, 20000, 20000);
```

### Changes to ForceGraphScene

```cpp
// 4. During simulation: disable BSP index (overhead > benefit when all items move)
void ForceGraphScene::onSimulationStarted() {
    setItemIndexMethod(QGraphicsScene::NoIndex);
}

void ForceGraphScene::onSimulationStopped() {
    setItemIndexMethod(QGraphicsScene::BspTreeIndex);
}
```

### Connect the signals

Wire `ForceLayoutEngine::simulationStarted` and `simulationStopped` to these methods through the view/scene.

---

## Priority 5: Batch Node/Edge Painting

**Impact:** ~10x rendering speed for 5,000+ nodes. **Effort:** Medium.

### Problem

Each `ForceGraphNode` (QGraphicsEllipseItem) and `ForceGraphEdge` (QGraphicsLineItem) is a separate QGraphicsItem. Qt processes each one individually: BSP lookup, transform calculation, painter state save/restore, paint call. At 5,000 items this takes ~300ms per frame.

### Solution: Two Custom Batch Items

Replace individual node/edge items with two aggregate items:

```cpp
class BatchEdgeItem : public QGraphicsItem {
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override {
        // Draw all edges in one call
        QPen normalPen(QColor(150, 150, 150, 100), 1.0 * m_widthScale);
        QPen dimmedPen(QColor(200, 200, 200, 30), 0.5 * m_widthScale);

        p->setPen(normalPen);
        QVector<QLineF> normalLines, dimmedLines;

        for (const auto &edge : m_edges) {
            QLineF line(edge.sourcePos, edge.targetPos);
            if (edge.dimmed)
                dimmedLines.append(line);
            else
                normalLines.append(line);
        }

        p->setPen(normalPen);
        p->drawLines(normalLines);  // Single batch call

        p->setPen(dimmedPen);
        p->drawLines(dimmedLines);  // Single batch call

        // Arrows (if enabled) drawn in a third pass
        if (m_showArrows) {
            // ... draw arrow triangles at target ends
        }
    }

    QRectF boundingRect() const override {
        return m_scene->sceneRect();  // Covers everything
    }
};

class BatchNodeItem : public QGraphicsItem {
    void paint(QPainter *p, const QStyleOptionGraphicsItem *opt, QWidget *) override {
        double lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
            p->worldTransform());

        // Group nodes by color for minimal state changes
        QHash<QRgb, QVector<QPointF>> colorGroups;
        QHash<QRgb, QVector<double>> colorRadii;

        for (const auto &node : m_nodes) {
            QColor c = node.color;
            if (node.dimmed) c.setAlphaF(0.15);
            if (node.highlighted) c = c.lighter(130);

            colorGroups[c.rgba()].append(node.position);
            colorRadii[c.rgba()].append(node.radius * m_sizeScale);
        }

        p->setPen(Qt::NoPen);
        for (auto it = colorGroups.constBegin(); it != colorGroups.constEnd(); ++it) {
            p->setBrush(QColor::fromRgba(it.key()));
            const auto &positions = it.value();
            const auto &radii = colorRadii[it.key()];
            for (int i = 0; i < positions.size(); ++i) {
                double r = radii[i];
                p->drawEllipse(positions[i], r, r);
            }
        }

        // Labels (only at sufficient LOD)
        if (lod >= m_textFadeThreshold) {
            p->setPen(QColor(80, 80, 80));
            QFont font;
            font.setPointSizeF(lod >= 2.0 ? 8.0 : 6.0);
            p->setFont(font);

            for (const auto &node : m_nodes) {
                if (node.dimmed) continue;
                QString label = (lod >= 2.0) ? node.label : node.label.left(12);
                p->drawText(QPointF(node.position.x() - 50,
                                     node.position.y() + node.radius * m_sizeScale + 4),
                            label);
            }
        }
    }
};
```

### Migration Path

Keep the existing `ForceGraphNode`/`ForceGraphEdge` classes for hit testing and hover detection (they provide `itemAt()` support). Add the batch items as a **rendering overlay** that paints on top, while making the individual items invisible:

```cpp
// During simulation (batch mode):
for (auto *node : m_nodeItems) node->setVisible(false);
for (auto *edge : m_edgeItems) edge->setVisible(false);
m_batchNodeItem->setVisible(true);
m_batchEdgeItem->setVisible(true);

// After simulation stabilizes (individual mode for interaction):
for (auto *node : m_nodeItems) node->setVisible(true);
for (auto *edge : m_edgeItems) edge->setVisible(true);
m_batchNodeItem->setVisible(false);
m_batchEdgeItem->setVisible(false);
```

This gives fast rendering during animation and full interactivity (hover, click, drag) when stable.

### Performance Reference

| Items | Individual paint | Batched paint |
|---|---|---|
| 1,000 | ~16ms (60fps) | ~5ms |
| 5,000 | ~80ms (12fps) | ~15ms (66fps) |
| 10,000 | ~300ms (3fps) | ~30ms (33fps) |

---

## Priority 6: QuadTree Optimizations

**Impact:** ~2x Barnes-Hut speed. **Effort:** Low.

### 6a. Pre-allocate tree nodes

```cpp
void QuadTree::build(const QVector<GraphNode> &nodes, const QRectF &bounds) {
    m_nodes.clear();
    m_nodes.reserve(5 * nodes.size());  // Worst case: ~4n internal + n leaf
    // ... existing build code
}
```

### 6b. Iterative traversal (replace recursive)

```cpp
QPointF QuadTree::computeRepulsion(const QPointF &pos, double repelForce,
                                    double nodeMass, double theta) const
{
    QPointF totalForce(0, 0);
    if (m_nodes.isEmpty()) return totalForce;

    // Fixed-size stack (tree depth bounded by log4(n) ≈ 8-10)
    int stack[40];
    int stackSize = 0;
    stack[stackSize++] = 0; // root

    while (stackSize > 0) {
        int idx = stack[--stackSize];
        const auto &node = m_nodes[idx];

        if (node.totalMass < 0.001) continue;

        QPointF delta = pos - node.centerOfMass;
        double dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());

        if (dist < 0.001) continue;

        double size = std::max(node.bounds.width(), node.bounds.height());

        if (node.isLeaf() || (size / dist < theta)) {
            // Treat as single body
            double force = repelForce * nodeMass * node.totalMass / (dist * dist);
            totalForce += (delta / dist) * force;
        } else {
            // Push children onto stack
            for (int i = 3; i >= 0; --i) { // Reverse order for depth-first
                if (node.children[i] >= 0)
                    stack[stackSize++] = node.children[i];
            }
        }
    }

    return totalForce;
}
```

### 6c. Adaptive theta

```cpp
double theta;
double progress = static_cast<double>(m_iteration) / m_maxIterations;
if (progress < 0.3)
    theta = 1.2;   // Fast, ~5% error — fine for early layout
else if (progress < 0.7)
    theta = 0.8;   // Balanced
else
    theta = 0.5;   // Precise for final positions
```

(If using ForceAtlas2 adaptive speed from Priority 3, base theta on energy level instead of iteration count.)

---

## Priority 7: Multilevel Coarsening (GRIP/Walshaw)

**Impact:** Required for 10,000+ nodes — eliminates local minima, converges in seconds. **Effort:** High.

### Framework

```
procedure MultilevelLayout(G₀):
    // Phase 1: COARSEN
    levels = [G₀]
    l = 0
    while |V(levels[l])| > 50:
        G_{l+1} = Coarsen(levels[l])
        levels.append(G_{l+1})
        l++
        if |V(G_{l+1})| / |V(G_l)| > 0.75:
            break  // Diminishing returns

    // Phase 2: LAYOUT COARSEST
    Apply ForceLayoutEngine to levels[L] (small graph, converges fast)

    // Phase 3: UNCOARSEN AND REFINE
    for l = L-1 down to 0:
        Interpolate positions from levels[l+1] to levels[l]
        Refine levels[l] with force layout (limited iterations)
        // Fewer iterations at finer levels — positions are already close

    return positions of G₀
```

### Coarsening: Maximal Edge Matching (Walshaw)

Simple, effective, and compatible with our existing Fruchterman-Reingold engine.

```
procedure CoarsenByEdgeMatching(G):
    // 1. Compute edge weights (optional: use connection strength)
    // For unweighted graphs, all edges have weight 1

    // 2. Find maximal matching
    matched = empty set of vertex pairs
    visited = empty set

    // Visit in random order for better matching quality
    vertices = shuffle(V(G))
    for each v in vertices:
        if v in visited: continue
        // Find heaviest unmatched neighbor
        bestNeighbor = null
        bestWeight = -1
        for each neighbor u of v:
            if u not in visited and weight(v,u) > bestWeight:
                bestNeighbor = u
                bestWeight = weight(v,u)
        if bestNeighbor != null:
            matched.add((v, bestNeighbor))
            visited.add(v)
            visited.add(bestNeighbor)

    // 3. Contract matched pairs
    G' = empty graph
    mapping = {}  // maps G vertices to G' vertices

    for each (u, v) in matched:
        w = new vertex in G'
        w.weight = weight(u) + weight(v)  // aggregate node weight
        mapping[u] = w
        mapping[v] = w

    for each unmatched vertex v:
        w = new vertex in G'
        w.weight = weight(v)
        mapping[v] = w

    // 4. Create edges in G'
    for each edge (u, v) in G:
        u' = mapping[u]
        v' = mapping[v]
        if u' != v':  // Skip self-loops from contracted edges
            add edge (u', v') to G' with weight += original weight

    return G', mapping
```

### Interpolation (Uncoarsening)

```
procedure Interpolate(positions_coarse, mapping):
    positions_fine = {}
    for each vertex v in fine graph:
        w = mapping[v]  // coarse vertex that v was merged into
        positions_fine[v] = positions_coarse[w]
        // Add small random offset to separate merged vertices
        positions_fine[v] += random_offset(±ideal_edge_length * 0.1)
    return positions_fine
```

### Refinement at Each Level

Use the existing `ForceLayoutEngine` with reduced iterations:

```
iterationsPerLevel = max(50, sqrt(|V_l|) * 10)
// Coarsest level: full convergence (~500 iterations)
// Middle levels: ~100-200 iterations
// Finest level: ~50-100 iterations (positions already close)
```

### Level Count

```
Expected levels = log₂(|V| / 50)
|V| = 1,000  → ~4 levels
|V| = 10,000 → ~8 levels
|V| = 100,000 → ~11 levels
```

### Implementation Location

New class `MultilevelLayout` in `libs/forcegraph/` that orchestrates the coarsen-layout-uncoarsen pipeline. It uses the existing `ForceLayoutEngine` at each level. `GraphViewTab` calls `MultilevelLayout` instead of `ForceLayoutEngine` directly when node count exceeds a threshold (e.g., 2,000).

### Complexity

- Coarsening: O(|V| + |E|) per level, O(log|V|) levels = O((|V|+|E|) log|V|)
- Layout per level: O(|V_l| log|V_l|) with Barnes-Hut
- Total: O(|V| log²|V|)

---

## Priority 8: GPU Acceleration

**Impact:** Required for 50,000+ nodes. **Effort:** Very high. **Recommendation:** Skip until needed.

### When QGraphicsView Is No Longer Viable

Above ~20,000 items, QGraphicsView's per-item overhead dominates regardless of optimization. Options at that scale:

- Custom `QWidget` with direct `QPainter` calls (no scene/item abstraction)
- `QQuickItem` with custom scene graph nodes
- Raw OpenGL/Vulkan compute shaders for force calculation

### If GPU Is Needed

Use OpenCL compute (no Qt dependency). Architecture:

```
Host (CPU, Qt):                    Device (GPU, OpenCL):
  step()  →  upload positions  →     Kernel 1: Build quadtree
             launch kernels          Kernel 2: Compute center of mass
             download positions  ←   Kernel 3: Compute repulsive forces
             update scene            Kernel 4: Compute attractive forces
                                     Kernel 5: Integrate positions
```

Reference: Burtscher & Pingali GPU Barnes-Hut (74x speedup for 5M bodies). Crossover point where GPU wins: ~50,000 nodes.

---

## UX Improvements

### Semantic Zoom (Level of Detail)

Replace the current fixed LOD thresholds with a graduated system:

| Zoom level (LOD) | Nodes | Edges | Labels |
|---|---|---|---|
| < 0.05 | Skip | Skip | None |
| 0.05 - 0.3 | Filled dot (1-2px) | Hidden | None |
| 0.3 - 0.8 | Filled circle | Thin gray lines | None |
| 0.8 - 1.5 | Circle + outline | Lines + dimming | Top 20% by degree, abbreviated (12 chars) |
| 1.5 - 2.5 | Full circle | Lines + arrows (if enabled) | All visible nodes, abbreviated |
| > 2.5 | Full circle + highlight ring | Full lines | Full labels, word-wrapped |

The key improvement: show labels for **high-degree nodes first** at medium zoom, not all-or-nothing:

```cpp
void ForceGraphNode::paint(QPainter *p, ...) {
    double lod = ...;

    // Always draw the circle (at appropriate detail level)
    // ...

    // Labels: show high-degree nodes first
    if (lod >= m_textFadeThreshold) {
        // At threshold LOD: only show if degree >= some rank cutoff
        // At 2x threshold: show all
        double labelVisibility = (lod - m_textFadeThreshold) / m_textFadeThreshold;
        double degreeThreshold = (1.0 - labelVisibility) * m_maxDegree;

        if (m_degree >= degreeThreshold) {
            // Draw label
        }
    }
}
```

### Node Styling by Type

Use existing `GraphDataBuilder` node colors but add more variation:

| Node type | Color | Radius | Shape |
|---|---|---|---|
| Regular note (1-5 links) | Purple (#7B6CD9) | 4 + log(1+deg)*3 | Circle |
| Hub note (6+ links) | Brighter purple (#9B8CE9) | Same formula | Circle + glow |
| Orphan (0 links) | Light gray (#AAA) | 3.0 | Circle |
| Unresolved link | Gray (#888), 50% opacity | 2.5 | Circle, dashed border |
| Daily note | Teal (#56B6C2) | 3.5 | Circle |

### Edge Styling

Current edges are uniform gray lines. Improve:

```cpp
// Base edge: low opacity, thin
QPen normalPen(QColor(150, 150, 150, 60), 0.8);  // Was alpha=100, width=1

// On hover highlight: connected edges brighten
QPen highlightPen(QColor(150, 150, 150, 180), 1.5);

// Dimmed: nearly invisible
QPen dimmedPen(QColor(200, 200, 200, 20), 0.3);  // Was alpha=30, width=0.5
```

**Key principle from research:** Edges should be the least prominent element by default. Low opacity (0.1-0.3) with increase on hover/selection.

### Hover Preview

On node hover, show a tooltip after 500ms delay:

```
┌─────────────────────────────┐
│ Note Title           12 links│
│ tag1, tag2                   │
│                              │
│ First line of note content...│
│ Second line of content...    │
└─────────────────────────────┘
```

Implementation: `QGraphicsItem::setToolTip()` with rich text, or a custom popup `QLabel` positioned near the cursor.

### Smooth Animations

**Layout transitions** (when filter changes, nodes appear/disappear):
- Animate node positions from old to new over 200-300ms
- Use `QTimeLine` or `QPropertyAnimation` with ease-in-out curve
- Entering nodes: fade in + scale from 0 over 300ms
- Exiting nodes: fade out over 200ms

**Zoom-to-node** (when user searches for a node):
- Animate viewport to center on the target node
- Duration 200-400ms, ease-in-out
- Combine zoom + pan into single smooth motion via `QTimeLine` driving `centerOn()` + `scale()`

**Hover feedback:**
- Node enlarges 1.1x on hover (immediate, no delay)
- Connected edges brighten
- Non-connected elements dim to alpha 0.15

### Color Theory

**Palette constraints:**
- Maximum 7 distinct category colors
- 3:1 minimum contrast ratio against background for all node colors
- 4.5:1 for text (WCAG AA)
- Test with red-green colorblind simulation

**Dark theme:** Lower saturation, muted hues. Never pure black background (#1E1E1E, not #000).
**Light theme:** Saturated-but-dark colors. Avoid low-saturation + high-luminosity.

Use `QPalette` colors for theme awareness — derive graph colors from the palette rather than hardcoding.

---

## What This Does NOT Include

- Edge bundling (visual cable grouping for dense graphs)
- Cluster detection (Louvain community algorithm)
- Fisheye lens (magnify cursor region)
- Path finding between nodes
- Timeline-based layout (position by creation date)
- 3D graph rendering
- Persistent node positions across sessions (needs session state serialization)
- Minimap overlay

---

## Implementation Order

The priorities are designed so each one stands alone and improves the graph independently:

1. **BFS initial placement** — change one function in ForceLayoutEngine
2. **Degree-weighted repulsion** — modify force calculation + QuadTree mass
3. **Adaptive speed** — replace temperature system in step()
4. **QGraphicsView perf** — add ~10 lines of configuration
5. **Batch painting** — new classes, swap during simulation
6. **QuadTree optimizations** — pre-allocate, iterative traversal, adaptive theta
7. **Multilevel coarsening** — new class orchestrating the pipeline
8. **GPU** — only if 50K+ nodes needed

Each priority can be implemented and shipped independently. No priority depends on a later one.
