# Graph Rendering Optimization — Design Specification

## Overview

The force-directed graph view handles layout computation well (Barnes-Hut, multilevel coarsening), but **rendering** is the bottleneck at 10K nodes. The batch paint system iterates all items every frame even when zoomed in showing 5% of them. This spec addresses rendering-side optimizations in four stages, ordered by impact/effort ratio.

**Benchmark target:** 10K nodes, smooth interaction (pan/zoom at 30+ fps), usable overview.

**Tech stack constraint:** QGraphicsView + QPainter (CPU). No GPU/OpenGL — that's a future nuclear option, not needed yet.

---

## Stage 1: Viewport Culling in Batch Paint

**Impact:** Massive when zoomed in. **Effort:** Small.

### Problem

`BatchNodeItem::paint()` iterates all 10K nodes every frame. `BatchEdgeItem::paint()` iterates all edges (often 20-40K). When zoomed to show 5% of the scene, 95% of this work is wasted.

### Solution

Cull items outside the visible rectangle before drawing.

#### BatchNodeItem changes

```cpp
void BatchNodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    // ... existing LOD check ...

    // Get visible area in scene coordinates
    QRectF visibleRect = painter->worldTransform().inverted().mapRect(
        QRectF(painter->viewport()));

    // Expand by max node radius to catch nodes whose center is just outside
    double maxR = /* track max radius */ * m_sizeScale;
    visibleRect.adjust(-maxR, -maxR, maxR, maxR);

    // Only process nodes inside visibleRect
    for (const auto &node : m_nodes) {
        if (!visibleRect.contains(node.position))
            continue;
        // ... existing color grouping / drawing ...
    }

    // Labels: same culling
    for (const auto &node : m_nodes) {
        if (!visibleRect.contains(node.position))
            continue;
        // ... existing label drawing ...
    }
}
```

#### BatchEdgeItem changes

```cpp
void BatchEdgeItem::paint(QPainter *painter, ...)
{
    QRectF visibleRect = painter->worldTransform().inverted().mapRect(
        QRectF(painter->viewport()));
    visibleRect.adjust(-50, -50, 50, 50); // margin for long edges crossing viewport

    for (const auto &edge : m_edges) {
        // Skip edges where both endpoints are outside the visible rect
        // on the SAME side (conservative: only skip clear misses)
        if (!edgeIntersectsRect(edge.line, visibleRect))
            continue;
        // ... existing partitioning ...
    }
}
```

Edge intersection test — fast conservative check:

```cpp
static bool edgeIntersectsRect(const QLineF &line, const QRectF &rect)
{
    // If either endpoint is inside, it's visible
    if (rect.contains(line.p1()) || rect.contains(line.p2()))
        return true;

    // Both outside: check if they're on the same side (definitely invisible)
    // Only cull the obvious cases — a few false positives are fine
    if (line.p1().x() < rect.left() && line.p2().x() < rect.left()) return false;
    if (line.p1().x() > rect.right() && line.p2().x() > rect.right()) return false;
    if (line.p1().y() < rect.top() && line.p2().y() < rect.top()) return false;
    if (line.p1().y() > rect.bottom() && line.p2().y() > rect.bottom()) return false;

    return true; // Might cross the rect — draw it
}
```

#### Track max radius

Add `m_maxRadius` to `BatchNodeItem`, computed in `setNodes()`:

```cpp
void BatchNodeItem::setNodes(const QVector<NodeData> &nodes)
{
    m_nodes = nodes;
    m_maxRadius = 0;
    for (const auto &n : nodes)
        m_maxRadius = std::max(m_maxRadius, n.radius);
    update();
}
```

### Expected impact

At full zoom-out: no change (everything visible). At typical interaction zoom (showing ~500 of 10K nodes): ~20x fewer items processed per frame.

---

## Stage 2: Aggressive LOD Thresholds

**Impact:** Large at overview zoom. **Effort:** Small.

### Problem

At overview zoom (lod < 0.1), edges are sub-pixel lines consuming draw calls for zero visual contribution. Arrows are drawn at medium zoom where they're too small to see. Text rendering (font metrics + word wrap) is the most expensive per-node operation.

### Changes

#### 2a. Hide edges at very low LOD

In `BatchEdgeItem::paint()`, add early-out:

```cpp
double lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
    painter->worldTransform());
if (lod < 0.03)
    return; // Edges invisible at this zoom
```

#### 2b. Skip short screen-space edges

Edges where both endpoints are within a few pixels of each other contribute nothing:

```cpp
// In the edge iteration loop:
QPointF screenP1 = painter->worldTransform().map(edge.line.p1());
QPointF screenP2 = painter->worldTransform().map(edge.line.p2());
double screenDist = QLineF(screenP1, screenP2).length();
if (screenDist < 2.0)
    continue; // Sub-pixel edge, skip
```

#### 2c. Auto-hide arrows at low LOD

Currently arrows are controlled by a user toggle. Add automatic suppression:

```cpp
// In BatchEdgeItem::paint(), arrow section:
bool drawArrows = m_showArrows && lod > 0.5;
```

#### 2d. Text greeking at borderline LOD

When text is too small to read but the label region is visible, draw a gray rectangle approximating the text block instead of calling `drawText()`:

```cpp
if (fontSize < 3.0 && fontSize >= 0.5) {
    // Greek: gray rectangle approximating text
    double greekW = std::min(w, node.label.size() * fontSize * 0.6);
    painter->fillRect(
        QRectF(node.position.x() - greekW / 2.0, node.position.y() + r + gap,
               greekW, fontSize * 1.2),
        QColor(120, 120, 120, 60));
    continue; // Skip drawText
}
```

### LOD threshold summary

| LOD range | Nodes | Edges | Labels | Arrows |
|-----------|-------|-------|--------|--------|
| < 0.03 | Skip entirely | Skip entirely | None | No |
| 0.03 - 0.05 | 2x2 pixel rect | Skip | None | No |
| 0.05 - 0.3 | Filled rect | Thin lines, cull short | None | No |
| 0.3 - 0.5 | Ellipse | Lines, cull short | None | No |
| 0.5 - threshold | Ellipse + border | Lines | None | Yes (if enabled) |
| threshold - 2x | Full detail | Lines | Greeked or degree-filtered | Yes |
| > 2x threshold | Full detail | Lines | Full text | Yes |

---

## Stage 3: Edge Hiding During Pan/Zoom

**Impact:** Large for interaction feel. **Effort:** Small.

### Problem

Edges typically outnumber nodes 2-4x. During rapid pan/zoom, the user is navigating — they don't need edge detail. Hiding edges during interaction frees half the rendering budget.

### Design

Add interaction state tracking to `ForceGraphView`:

```cpp
// ForceGraphView.h
bool m_interacting = false;
QTimer *m_interactionTimer = nullptr; // fires ~150ms after last input

// ForceGraphView.cpp
void ForceGraphView::beginInteraction()
{
    if (!m_interacting && nodeCount > EDGE_HIDE_THRESHOLD) {
        m_interacting = true;
        m_scene->setEdgesVisible(false);
    }
    m_interactionTimer->start(150); // reset on each input
}

void ForceGraphView::endInteraction()
{
    m_interacting = false;
    m_scene->setEdgesVisible(true);
}
```

Call `beginInteraction()` from:
- `wheelEvent()`
- `mouseMoveEvent()` (during pan only)

The timer fires `endInteraction()` 150ms after the last pan/zoom event, restoring edges smoothly.

#### Threshold

Only hide edges when the graph is large enough to matter:

```cpp
static constexpr int EDGE_HIDE_THRESHOLD = 2000; // nodes
```

Below 2000 nodes, edges stay visible during interaction.

#### Scene support

```cpp
void ForceGraphScene::setEdgesVisible(bool visible)
{
    if (m_batchMode) {
        m_batchEdges->setVisible(visible);
    } else {
        for (auto *edge : std::as_const(m_edgeItems))
            edge->setVisible(visible);
    }
}
```

---

## Stage 4: Spatial Hash Grid for Batch Data

**Impact:** Large at all zoom levels for 10K+. **Effort:** Medium.

### Problem

Even with viewport culling (Stage 1), we still iterate all nodes to check `visibleRect.contains()`. At 10K nodes this is 10K comparisons per frame — fast but not free, especially during simulation where paint() is called 60 times/second.

### Design

Partition node data into a fixed-size grid. Query only cells intersecting the viewport.

```cpp
class SpatialGrid {
public:
    void build(const QVector<BatchNodeItem::NodeData> &nodes, const QRectF &bounds);
    QVector<int> query(const QRectF &rect) const; // returns indices into original array

private:
    static constexpr int GRID_SIZE = 64; // 64x64 cells
    QRectF m_bounds;
    double m_cellW, m_cellH;
    QVector<QVector<int>> m_cells; // GRID_SIZE * GRID_SIZE cells
};
```

#### Build (called in setNodes / syncBatchData)

```cpp
void SpatialGrid::build(const QVector<BatchNodeItem::NodeData> &nodes, const QRectF &bounds)
{
    m_bounds = bounds;
    m_cellW = bounds.width() / GRID_SIZE;
    m_cellH = bounds.height() / GRID_SIZE;
    m_cells.resize(GRID_SIZE * GRID_SIZE);
    for (auto &cell : m_cells) cell.clear();

    for (int i = 0; i < nodes.size(); ++i) {
        int cx = std::clamp(int((nodes[i].position.x() - bounds.left()) / m_cellW), 0, GRID_SIZE - 1);
        int cy = std::clamp(int((nodes[i].position.y() - bounds.top()) / m_cellH), 0, GRID_SIZE - 1);
        m_cells[cy * GRID_SIZE + cx].append(i);
    }
}
```

#### Query (called in paint)

```cpp
QVector<int> SpatialGrid::query(const QRectF &rect) const
{
    int x0 = std::clamp(int((rect.left() - m_bounds.left()) / m_cellW), 0, GRID_SIZE - 1);
    int y0 = std::clamp(int((rect.top() - m_bounds.top()) / m_cellH), 0, GRID_SIZE - 1);
    int x1 = std::clamp(int((rect.right() - m_bounds.left()) / m_cellW), 0, GRID_SIZE - 1);
    int y1 = std::clamp(int((rect.bottom() - m_bounds.top()) / m_cellH), 0, GRID_SIZE - 1);

    QVector<int> result;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            result.append(m_cells[y * GRID_SIZE + x]);
    return result;
}
```

#### Integration

`BatchNodeItem` owns a `SpatialGrid`. Rebuilt in `setNodes()`. In `paint()`:

```cpp
auto indices = m_grid.query(visibleRect);
for (int idx : indices) {
    const auto &node = m_nodes[idx];
    // ... draw ...
}
```

#### Edge spatial grid

Edges are indexed by their midpoint. An edge's midpoint falling outside the viewport doesn't mean the edge is invisible (long edges cross the viewport), so the edge grid uses a larger query margin. Alternatively, edges whose both endpoints fall in far-off cells are skipped — reusing the conservative `edgeIntersectsRect` from Stage 1 as a secondary filter.

For edges, a simpler approach: store edge indices in grid cells for *both* endpoints. Query the viewport cells, deduplicate indices with a `QBitArray`, then draw.

```cpp
class EdgeSpatialGrid {
    // Same grid structure but each edge is inserted into cells for both endpoints
    void build(const QVector<BatchEdgeItem::EdgeData> &edges, const QRectF &bounds);
    void query(const QRectF &rect, QBitArray &visible) const; // sets bits for visible edges
};
```

### Expected impact

Overview zoom: ~same (all cells visited). Zoomed to 10% of scene: iterate ~1% of cells = ~100 nodes instead of 10K. During simulation with 60fps paint: saves ~0.5ms per frame at 10K nodes.

---

## Stage 5: Cosmetic Pens & Minor QPainter Wins

**Impact:** Small but free. **Effort:** Trivial.

### Cosmetic pens for edges

Non-cosmetic pens scale with the transform, requiring QPainter to compute scaled width per line. Cosmetic pens are always 1 device pixel — faster to rasterize:

```cpp
QPen pen(QColor(150, 150, 150, 60), 1.0);
pen.setCosmetic(true);
painter->setPen(pen);
```

Use cosmetic pens for edges in batch mode (where exact scene-space width doesn't matter much). Keep non-cosmetic for individual mode where the user expects edges to scale with zoom.

### Exposed rect clipping

Tell QPainter to clip to the exposed area. One line in each batch paint():

```cpp
painter->setClipRect(option->exposedRect);
```

This is redundant with viewport culling but costs nothing and catches edge cases where QPainter rasterizes geometry outside the viewport.

Note: `option->exposedRect` is only populated when `QGraphicsItem::ItemUsesExtendedStyleOption` flag is set. Add this flag to both batch items in their constructors:

```cpp
BatchNodeItem::BatchNodeItem(QGraphicsItem *parent) : QGraphicsItem(parent)
{
    setFlag(ItemUsesExtendedStyleOption); // populates option->exposedRect
    setZValue(1);
}
```

### Pre-sort edges by dimmed state

Currently `BatchEdgeItem::paint()` partitions edges into normal/dimmed vectors every frame. Instead, keep edges pre-sorted so dimmed edges are at the end:

```cpp
void BatchEdgeItem::setEdges(const QVector<EdgeData> &edges)
{
    m_edges = edges;
    // Partition: normal edges first, dimmed at end
    m_normalCount = std::partition(m_edges.begin(), m_edges.end(),
        [](const EdgeData &e) { return !e.dimmed; }) - m_edges.begin();
    update();
}
```

Then in paint():

```cpp
// Draw normal edges (0..m_normalCount)
QVector<QLineF> normalLines;
for (int i = 0; i < m_normalCount; ++i)
    normalLines.append(m_edges[i].line);
painter->drawLines(normalLines);

// Draw dimmed edges (m_normalCount..end)
QVector<QLineF> dimmedLines;
for (int i = m_normalCount; i < m_edges.size(); ++i)
    dimmedLines.append(m_edges[i].line);
painter->drawLines(dimmedLines);
```

This avoids allocating two vectors and branching per edge in paint().

---

## What This Does NOT Include

- GPU/OpenGL rendering (future, only if CPU rendering can't handle 10K)
- Community detection / cluster aggregation (separate feature, not a rendering optimization)
- Progressive/budget rendering with frame deadline (complex, evaluate after stages 1-4)
- Edge bundling (visual feature, not performance)
- QOpenGLWidget as viewport (research shows minimal gains, breaks text rendering)

---

## Implementation Order

Each stage is independent and shippable:

1. **Viewport culling** — highest impact single change, touches BatchNodeItem + BatchEdgeItem paint()
2. **Aggressive LOD** — touches BatchNodeItem + BatchEdgeItem paint(), adds LOD thresholds
3. **Edge hiding during interaction** — touches ForceGraphView + ForceGraphScene, new timer
4. **Spatial hash grid** — new SpatialGrid class, integrates into batch items
5. **Cosmetic pens & minor wins** — scattered small changes across batch items

Stage 1+2 can be done in one pass since they both modify the same paint() methods. Stage 3 is independent. Stage 4 builds on Stage 1's viewport rect computation. Stage 5 is sprinkled throughout.

---

## Implementation Progress

- [x] **Stage 1+2: Viewport culling + aggressive LOD** — done. BatchNodeItem and BatchEdgeItem paint() rewritten with visible rect culling, LOD edge hiding (lod<0.03), sub-pixel edge skipping, auto arrow suppression (lod<0.5), text greeking, pre-sorted edge partitioning.
- [x] **Stage 3: Edge hiding during interaction** — done. ForceGraphView hides edges during pan/zoom for graphs >2000 nodes, restores after 150ms idle via QTimer.
- [x] **Stage 4: Spatial hash grid** — done. SpatialGrid (header-only, 64x64 grid) with prefix-sum packed storage. BatchNodeItem queries grid for O(visible) iteration instead of O(all).
- [x] **Stage 5: Cosmetic pens & minor wins** — done. Cosmetic pens on batch edges, exposed rect clipping via ItemUsesExtendedStyleOption + setClipRect in both batch items.
- [x] **Build & test** — all graph tests pass (tst_graphdatabuilder, tst_quadtree, tst_forcelayout).
