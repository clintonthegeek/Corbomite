# Handoff → Graffodil devs: a destructor that touches freed items, an undocumented raw-pointer contract, and no theming surface

**From:** Corbomite (downstream consumer — `libs/canvas` derives from Graffodil)
**Date:** 2026-10-02
**Corbomite HEAD:** `57e79fad`
**Graffodil pin:** `011b342fa8c9eabae1b4f70d5bb6ebe0d2e8ba75`
**Severity (our view):** **P0** for §1 (use-after-free on scene teardown),
**P1** for §2–§3 (lifetime contract and use-after-free/ghost edges),
**P1** for §4 (theming — the reason we ended up with our own palette) and §5
(scale). §6–§8 are P1/P2. Nothing here blocks your roadmap.

**Our relationship to this library:** ten of `libs/canvas`'s headers derive from
Graffodil — `CanvasScene`, `CanvasNodeItem`, `EdgeItem`, `GroupItem`,
`CanvasResizeTool`, `CanvasDuplicateDragTool`, `CanvasEdgeGestureTool`,
`ReconnectEdgeTool`, `CanvasAlignmentStrategy`, `CanvasBezierStrategy` — and our
graph view consumes it directly. We are a heavy consumer, so we are being greedy.

**One thing we got wrong first, and want to record.** During a dead-code audit
we nearly deleted `SpatialGrid` as unused. It is live — a member of
`BatchNodeItem`. The lesson ("a symbol whose only references are in a header you
did not read is not dead") is in our punch list, and §5 below is partly a
consequence of having almost repeated that mistake.

---

## 0. How we verified §1

§1's ordering claim is Qt's documented child-QObject destruction order, and we
confirmed the two facts it rests on directly: `BatchRenderer` is constructed as
a child of the scene (`BatchRenderer.cpp:23`, `QObject(scene)`), and
`setBatchMode(false)` re-shows items (`:74-79`). We have **not** reproduced a
crash, because it needs a teardown ordering we could not force in a short
harness. We are flagging it as reasoned-from-ordering rather than reproduced, and
would welcome being shown we have it wrong.

---

## 1. (P0) `BatchRenderer`'s destructor touches items the scene has already destroyed

`src/batch/src/BatchRenderer.cpp:23, 29-31`

```cpp
BatchRenderer::BatchRenderer(GraphScene *scene) : QObject(scene), d(new Private) { ... }
BatchRenderer::~BatchRenderer() {
    setBatchMode(false);     // :30  re-shows every item
    delete d;
}
```

`BatchRenderer` is a **child QObject of the scene**, so the destruction order is:

1. `~BatchRenderer` body — runs `setBatchMode(false)`
2. `~QGraphicsScene` body — **destroys all items**
3. `~QObject` body — destroys children, i.e. `~BatchRenderer` proper

`setBatchMode(false)` therefore runs against a destroyed item set, calling the
**virtual** `node->graphicsItem()` on freed `IGraphNode` objects and
`setVisible(true)` on freed `QGraphicsItem`s.

**Separately**, `synchronize()` has no null guards at all (`:127-128`):

```cpp
QPointF srcPos = edge->sourceNode()->graphicsItem()->pos();
QPointF tgtPos = edge->targetNode()->graphicsItem()->pos();
```

Every other call site in the library defensively uses `IGraphEdge::sourceNodeId()`,
which `IGraphEdge.h:22-26` documents as null-safe ("Returns an empty string if the
endpoint is null"). These two are the outliers.

**What we would like:** store a `QPointer<GraphScene>` and early-return in the
destructor if it is null (the scene owns us and has already cleaned up); add
`if (!edge->sourceNode() || !edge->targetNode()) continue;` plus a
`graphicsItem()` null check in `synchronize()`.

**Test:** create a `GraphScene` + `BatchRenderer`, enter batch mode, delete the
scene, assert clean under ASAN. That is the whole test.

---

## 2. (P1) `GraphEdgeItem` holds raw `IGraphNode *` endpoints with no detach API

`src/core/src/GraphEdgeItem.h:121-122`
```cpp
IGraphNode *m_source;
IGraphNode *m_target;
```

`GraphScene::removeNode` unregisters connected edges (`GraphScene.cpp:41-47`) but
**never clears the edges' endpoints**. Two outcomes:

- The consumer frees a node before its edges → `adjust()` dereferences freed
  memory on the next drag frame.
- The node survives → `adjust()`'s early return (`GraphEdgeItem.cpp:241-242`)
  leaves the **stale path rendered**, with no invalidation. A ghost edge frozen
  at the dead node's last position.

```cpp
if (!m_strategy || !m_source || !m_target)
    return;                 // :241  returns BEFORE prepareGeometryChange()
```

**Why it matters to us:** `libs/canvas` owns node and edge lifetime across vault
reloads and file-watcher rebuilds, so the "node freed first" ordering is a path
we can reach.

**What we would like:** add `void setEndpoints(IGraphNode *src, IGraphNode *tgt, …)`
and have `GraphScene::removeNode`/`removeEdge` null them; make `adjust()`'s null
path call `setPath({})` + `prepareGeometryChange()` so the edge visibly
disappears instead of ghosting; document the ownership order in
`GraphEdgeItem.h`.

---

## 3. (P1) The same raw-pointer exposure survives in the tools

| Site | Note |
|---|---|
| `src/tools/src/CreateEdgeTool.cpp:199-201` | `m_sourceNode->…` inside `updatePreview`, called from **every** `mouseMoveEvent` |
| `src/tools/src/CreateEdgeTool.h` `m_sourceNode` | raw |
| `src/tools/src/SelectMoveTool.cpp:165-186` | `m_dragNode`, plus `QHash<IGraphNode*, QPointF> m_dragStartPositions` keyed on raw pointers |

**We are not raising this as a new argument.** Your own
`docs/bugs/anchorhighlight-dangling-node-pointer-crash.md` fixed one instance of
exactly this class and the report's chosen remedy was the `QPointer` guard
pattern, which `AnchorHighlight.cpp:19` demonstrates. **Three siblings of that
same bug appear to have been left in place**, and `AnchorHighlight.h:16-18`'s
comment ("consumers can guard their references with `QPointer` and detect
scene-driven deletion") now implies a safety the rest of the library does not
provide.

We also note the other half of that report appears unfinished: it chose to
"document the requirement" that `IGraphNode` implementations derive from
`QGraphicsObject`, but `IGraphNode.h` (80 lines, read in full) never mentions it.
A consumer implementing `IGraphNode` on a bare `QGraphicsItem` gets a silently
inert `showAnchors()` with no diagnostic.

**What we would like:** convert the three sites above to `QPointer`, resolved
from `graphicsItem()` at assignment time; add the `QGraphicsObject` requirement
to `IGraphNode.h`; and `qCWarning` in `showAnchors` when the guard fails to
resolve.

---

## 4. (P1) There is no theming surface at all — five hardcoded colours

| Location | Role |
|---|---|
| `GraphEdgeItem.cpp:353` | selection `QColor(0, 120, 215)` |
| `CreateEdgeTool.cpp:96` | preview line |
| `SelectMoveTool.cpp:145-146, 199` | rubber band, guides |
| `AnchorHighlight.cpp:77` | accent |
| `GraphMinimap.cpp:56-57, 89-90` | dark chrome |
| `ConnectorItem.cpp:12-13` | grey |
| **`BatchRenderer.cpp:112`** | `nd.color = QColor(100, 150, 200)` — assigned **unconditionally** |

That last one is the sharpest: a consumer that themes individual nodes in normal
mode gets uniform blue in batch mode, with **no API to prevent it**.

**Direct evidence this bit us.** Corbomite's canvas dark-mode work (shipped
2026-10-02) had to build a 14-role `CanvasPalette`
(`libs/canvas/include/corbomite/canvas/CanvasPalette.h`), whose header says:

> *Canvas had **zero** theme awareness: ~15 hardcoded literals.*

And our `ReconnectEdgeTool.cpp:78` **re-hardcodes Graffodil's exact
`QColor(0, 120, 215)`** because Graffodil's own connect-preview line is not
themeable. We are reimplementing your colour in our tool because we have no seam.

**What we would like:** a `struct Graffodil::Palette` with the roles above,
`GraphScene::setPalette()` and `GraphEdgeItem::setSelectionColor()`, threaded to
every site. For `BatchRenderer`, an optional
`std::function<QColor(const IGraphNode *)>` colour provider so batch mode can
honour per-node theming.

**We can send our `CanvasPalette` as the worked design** if that is useful — it is
a concrete role list that maps one-to-one onto the sites above, and it is already
reviewed on our side.

---

## 5. (P1) No interaction path is sub-linear, despite shipping `Spatial` for exactly that

Every mouse-move does an O(items × nodes) scan plus an `anchors()` allocation per
node:

- `CreateEdgeTool.cpp:51-63` (`findNodeAt`) and `:175-183`
- `SelectMoveTool.cpp:22-46`
- `CompositeTool.cpp:58-75` (`isNearAnchor`, via `updateHoverCursor` at `:82`)
- `GraphScene.cpp:634-640`

`CreateEdgeTool.cpp:54-61` in particular:

```cpp
for (auto *item : items)
    for (auto *node : m_scene->nodes())        // QHash::values() — fresh QList, heap alloc
        if (node->graphicsItem() == item || node->graphicsItem()->isAncestorOf(item))
```

and the fallback at `:175-183` iterates all nodes × all anchors, calling
`node->anchors()` — which **constructs a fresh `QList<Anchor>` per node per
mouse-move**. `CompositeTool::isNearAnchor` has an AABB pre-reject (`:64-67`) but
still calls `sceneBoundingRect()` and `anchors()` per candidate, plus a
`QLineF(...).length()` (a `std::hypot`) per anchor. `CompositeTool::mouseMoveEvent:185`
broadcasts to **all** sub-tools, compounding it.

**`Graffodil::Spatial` (QuadTree + SpatialGrid) has zero consumers inside
`Graffodil::Core`.** The index exists and is not wired to any path that needs it.

**What we would like:** build a QuadTree/`SpatialGrid` over node bounding rects
in `GraphScene`, invalidated on `changed`/`nodeAdded`/`nodeRemoved`, and route
all five call sites through one `GraphScene::nodesAt(scenePos, radius)`. Cache
`anchors()` per node, invalidated on `setGeometry`.

---

## 6. (P1) Sugiyama layering is a single pass, so any cycle silently corrupts it

`src/sugiyama/src/SugiyamaLayout.cpp:437-439, 478-481, 486-494`

```cpp
QList<QString> topoOrder = topologicalSort(allTasks, successors, predecessors);
if (topoOrder.isEmpty()) topoOrder = allTasks;      // :438  handles all-or-nothing only
```

Kahn's algorithm returns a **partial** order when a cycle exists. The code only
handles the case where the order is completely empty. A single self-loop or 2-cycle
leaves `topoOrder` non-empty but **incomplete**; the missing nodes are appended at
`:478-481` in arbitrary non-topological order. The longest-path pass at `:486-494`
then runs **once**, and `if (layer.contains(pred))` (`:489`) silently skips
predecessors that appear later in the list — yielding a **too-small layer**.

Edges then get `span <= 0`, dummy vertices are never inserted for them, and long
edges are routed with no waypoints at all.

**Why it matters to us:** canvas files and dependency graphs legitimately contain
cycles, and ours do.

**What we would like:** relax layer assignment to a fixed point
(`do { changed = false; … } while (changed);`). **Test:** one self-loop plus a
3-node chain, asserting the chain's layering is unchanged by the loop.

---

## 7. (P1/P2) Correctness and lifecycle items

| Item | Location | Note |
|---|---|---|
| `edgeActionAt` hit-tests in **item-local** coordinates and copies every path | `GraphScene.cpp:613-625` | `pathItem->path()` returns **by value** — a full `QPainterPath` copy per edge per action per press — and `pointAtPercent` yields item-local coords while `scenePos` is scene coords. The moment an edge item carries a transform (rotation for label-follow at `GraphEdgeItem.cpp:171-175`, or any layout that `setPos`es the edge) the affordance is unclickable. The sibling `nodeSubItemAt` (`:634`) correctly uses scene coords — **the two hit-testers disagree.** Fix: `mapToScene(...)`, and hoist the path copy out of the inner loop. |
| `QHash` iteration → non-deterministic hit-test winner | `GraphScene.cpp:610, 634`, `CreateEdgeTool.cpp:56`, `SelectMoveTool.cpp:26` | `m_edges`/`m_nodes` are `QHash` (`GraphScene.h:293-294`); `nodes()`/`edges()` return `QHash::values()` in unspecified order. With overlapping nodes, which one claims a hit is unspecified and can change between runs. Suggest iterating `items(scenePos)` (already topmost-first) and testing membership. |
| `GraphMinimap` never resets `m_dragging` | `GraphMinimap.cpp:97` | The header overrides only `paintEvent`, `mousePressEvent`, `mouseMoveEvent`, `eventFilter` — **there is no `mouseReleaseEvent`.** After one click, every subsequent hover-move re-centres the view with no button held. A `mouseReleaseEvent` + `hideEvent` (and `leaveEvent`) would fix it. |
| `GraphMinimap` divides by zero-width rects | `GraphMinimap.cpp:66-68, 134-136` | `effectiveSceneRect` returns `{}` only for a *null* rect, so a non-null zero-width rect yields `scaleX = inf`, `qMin(inf,inf) = inf`, and `painter.scale(inf, inf)`. Guard `width() <= 0 \|\| height() <= 0`. |
| `GraphScene::clearGraph()` emits mid-teardown | `GraphScene.cpp:186-197` | `edgeRemoved`/`nodeRemoved` fire while `m_edges`/`m_nodes` are only partly cleared (`m_edges.clear()` at `:190` comes *after* the edge loop). A slot that re-enters corrupts the registries. Snapshot and clear first, then emit. |
| `LayoutResult::edgeWaypoints` keyed by `(sourceId, targetId)` | `SugiyamaLayout.cpp:790-798`, delivered `GraphScene.cpp:413` | Direction-sensitive and first-match, so two distinct A→B edges receive the **same** merged waypoint list and route identically. Self-loops unhandled. Keying on edge id would fix it. |
| `countCrossings` is O(E²), up to 25× per layout | `SugiyamaLayout.cpp:131-138, 654, 701` | `maxSweeps = 24`. Barth–Jünger–Mutzel makes it O(E log V); minimally, bucket `edgePairs` by `uPos` and skip equal-`uPos` pairs, which can never cross. |
| `SelectMoveTool` destroys and rebuilds the guide overlay every drag frame | `SelectMoveTool.cpp:189-202` | `removeItem` + `delete` the `QGraphicsItemGroup`, then `new` + N `QGraphicsLineItem`s + `addItem`, per mouse-move. Keep the group and toggle visibility. |
| `CreateEdgeTool` leaks its preview line | `CreateEdgeTool.cpp:21-31` | Cleans `m_highlight`, never `m_previewLine`. A tool destroyed mid-gesture leaves an invisible-but-present dashed item in the scene forever. |
| `GraphEdgeItem::boundingRect()` ignores pen width | `GraphEdgeItem.cpp:301-312` | `setHighlighted()` (`:181-185`) and the selected path (`:352-355`) widen the pen by 1.0–1.5px **without** `prepareGeometryChange()`, so the widened stroke can paint outside the bounding rect and clip. |
| `IGraphNode::anchorToward()` returns a silent `(0,0)` on an empty list | `IGraphNode.cpp:8-20` | Edges route to the scene origin. `anchor()` (`:28-33`) *does* warn — but at per-frame rate, from `adjust()`. Warn once on the empty list; rate-limit the other. |
| `setEdgesAboveNodes()` cannot distinguish a consumer-set z=0 from the Qt default | `GraphScene.cpp:101-112` | It re-assigns any item at z `0.0` or `-1.0`, and Qt's default z **is** `0.0`. `GraphScene.h:23-25` claims the scene "only assigns these to items still at the Qt default of 0", which is unachievable in practice. |
| `QuadTree::insert()` holds a `QuadNode&` across `subdivide()` | `QuadTree.cpp:76, 96` | `subdivide()` (`:64-69`) does four `m_nodes.append()`. Safe only incidentally — `qn` is not read afterwards — and `m_nodes.reserve(5 * positions.size())` at `:38` is insufficient for deep trees, so realloc **will** happen. Return child indices from `subdivide()` and drop the reference. |
| `QuadTree::computeRepulsion` uses `bounds.width()` as "the" region size | `QuadTree.cpp:161` | Root bounds are `(maxX-minX)+2 × (maxY-minY)+2` — independent extents. For a wide graph the Barnes-Hut criterion treats regions as much larger than they are and silently coarsens the approximation. Use `qMax(width, height)`. |
| `IGraphNode::graphicsItem()` is pure virtual with no null contract | `IGraphNode.h:64` | `GraphScene.cpp:23-26` and `:88-91` deref it immediately, and `QGraphicsScene::addItem(nullptr)` is itself UB. Guard + document. |
| `SugiyamaLayout::compute()` / `ForceLayout` bypass the null-safe accessor | `SugiyamaLayout.cpp:407-408`, `ForceLayout.cpp:104-105` | Both deref `sourceNode()`/`targetNode()` directly where `sourceNodeId()` exists precisely to be null-safe. |
| `AnchorHighlight` uses `dynamic_cast` where the report specified `qobject_cast` | `AnchorHighlight.cpp:19` | Functionally equivalent, slower, and misses Qt's metaobject path. `CreateEdgeTool.cpp:54` correctly uses `qobject_cast`. |
| `GraphScene::nodeBoundingRect()` is consumed as item-local in two places | `IGraphNode.h:42-44`, `GraphScene.cpp:388`, `GroupItem.h:37` | Works only because `addGroup` never moves the item (`:284-287`). A consumer reparenting a `GroupItem` gets double-transformed bounds. Worth documenting the invariant on `setBounds`. |
| `edgePoint()` copies the path twice | `GraphEdgeItem.cpp:266-272` | One copy into a local. |

---

## 8. Build, test and packaging

1. **Zero Graffodil tests run in Corbomite.** `GRAFFODIL_BUILD_TESTS` defaults to
   `${PROJECT_IS_TOP_LEVEL}` (`:22`), which is FALSE as a subdirectory, so
   `ctest -N` in our tree matches **none** of your 16 cases. We tried enabling it
   and **it does not configure**: your `tests/CMakeLists.txt` uses hardcoded
   target names and `tst_forcelayout` + `tst_quadtree` collide with our own
   `libs/forcegraph/tests/`, so `add_executable` fails on the duplicate.

   **What we would like:** either a target-name prefix (e.g. `graffodil_` on
   those two), or a `GRAFFODIL_TEST_PREFIX` variable so consumers can
   disambiguate without patching. We would rather have a knob than patch your
   tree on every re-pin. For reference, your tests also carry **no**
   `ENVIRONMENT` property, so they will abort on a headless box — they need
   `QT_QPA_PLATFORM=offscreen` in the same change.

2. **No usable install.** `src/core/CMakeLists.txt:74-81` does `install(TARGETS)`,
   but `target_include_directories` has only `$<BUILD_INTERFACE:>` (no
   `INSTALL_INTERFACE`), there is no `install(FILES … include/graffodil/*.h)`,
   and no `install(EXPORT)`. A packaged Graffodil is a `.so` nobody can link.
   `docs/specs/cmake-install-export.md` exists but is unexecuted — we mention it
   only because it suggests you already know.

3. **No `LABELS` or `ENVIRONMENT` on the 16 tests** (`:15 add_test` calls, zero
   `set_tests_properties`). Preventive rather than a live defect — nothing
   times out today — but our canonical run excludes perf tests by name regex, so
   a future Graffodil timing test would not be excluded.

---

## 9. One question rather than a request

`Corbomite/libs/forcegraph/` looks like a Corbomite-side extraction of
Graffodil's force + spatial extensions — its tests include `tst_forcelayout.cpp`
and `tst_quadtree.cpp`, which are the same names as two of yours. **We have not
investigated this properly**, so we are flagging it rather than asking anything:
if it *is* a fork, we would value your view on whether folding it back upstream
is worth your time, because we are currently paying maintenance for it. No
action needed from you unless you already have a view.

---

## 10. Suggested triage order

1. **§1** — smallest diff with the largest payoff; one ASAN test proves it.
2. **§2** + **§3** — one issue, because your own bug report is the precedent and
   this is the natural follow-up.
3. **§4** — an API request rather than a bug; we can send `CanvasPalette` as the
   design so the ask arrives with an answer attached.
4. **§5** — the `Spatial` index is already written; this is wiring.
5. **§6**, §7, §8 as capacity allows.

On §1 and §2 in particular: we would genuinely rather be shown wrong than carry
workarounds. If Graffodil's intended ownership contract is that consumers must
tear down items before the scene, then §2 is our bug and we would like to know —
but we could not find that contract documented anywhere, which is most of why we
are asking.