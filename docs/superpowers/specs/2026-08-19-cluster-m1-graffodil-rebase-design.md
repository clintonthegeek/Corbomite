# Cluster M Phase M1 — Graffodil rebase design (migration contract)

**Date:** 2026-08-19. **Status:** Accepted design; dispatchable.
**Parent plan:** [`../plans/2026-08-19-cluster-m-canvas-authoring-parity.md`](../plans/2026-08-19-cluster-m-canvas-authoring-parity.md).
**Graffodil pin:** `libs/graffodil` @ `v0.2.3` (`dd7667de`). Headers under
`libs/graffodil/src/core/include/graffodil/`.

This spec is the **complete migration contract** for rebasing `libs/canvas`
onto `Graffodil::Core`. It was written after reading both API surfaces in
full; an executing agent should not need to re-derive any mapping decision.
When code contradicts this spec, verify against the two sources of truth
(current `libs/canvas` behavior; Graffodil v0.2.3 headers) and note the
divergence in the plan file — do not silently improvise.

## 0. Prime directive

M1 is **feature-frozen**: the goal is the substrate swap with the current
feature set intact. The only three *intended* behavior changes are listed in
§7. Everything else must look and act identical before/after (same menus,
same inline editing, same export output, same undo granularity, same
`.canvas` bytes on save).

## 1. What Graffodil provides vs. what stays consumer-owned

| Concern | After M1 | Notes |
|---|---|---|
| Scene registry, node/edge maps, adjacency index | `Graffodil::GraphScene` | replaces `m_textCardItems`/`m_fileCardItems`/`m_groupItems`/`m_edgeItems` hashes |
| Select / marquee / multi-move / delete-key / drag signals | `Graffodil::SelectMoveTool` (inside `DefaultGraphTool`) | fixes file-card unselectability structurally |
| Pan (middle-drag) + zoom (Ctrl+wheel, about cursor) | `Graffodil::PanZoomTool` (inside `DefaultGraphTool`) | replaces `CanvasView`'s hand-rolled pan/zoom handlers |
| Resize (8 zones, min-size clamp) | **consumer** `CanvasResizeTool : GraphTool`, routed via CompositeTool predicate | Graffodil has no resize tool; §4.3 |
| Edge path/arrow/label/hit-shape rendering | `Graffodil::GraphEdgeItem` subclass | stock `BezierPathStrategy` == current curvature math exactly (`min(dist*0.4, 80)`); Obsidian-exact strategy deferred to M3 |
| `.canvas` JSON model, undo commands, id generation | **consumer, unchanged** | `CanvasDocument`, `CanvasCommands`, `CanvasTypes` untouched in M1 |
| Node painting, inline-edit proxies, context menus, export, color mapping | **consumer, unchanged** | stays on `CanvasScene` / item classes |
| Graffodil `GroupItem`/`GroupStyle` | **NOT used** | see §3.3 — canvas groups are real persisted nodes, not decorative backdrops. Do not map them onto Graffodil's decorative GroupItem. |
| Graffodil layout engines, minimap, ConnectorItem, waypoints | **NOT used in M1** | available later; ignore |

## 2. Anchor model (the one identity that makes everything line up)

Canvas `Side` ↔ Graffodil anchor id is a **string identity**:
`sideToString(Side)` already produces exactly `"top"/"right"/"bottom"/"left"`,
which are Graffodil's `AnchorIds::{Top,Right,Bottom,Left}` conventions and
what `Graffodil::compassAnchors(rect)` emits. Therefore:

- `IGraphNode::anchors()` on every canvas node returns
  `Graffodil::compassAnchors(sceneBoundingRect())` — nothing more.
- Edge endpoints always carry a **non-empty** anchor id
  (`sideToString(edge.fromSide)`). Never pass an empty id: empty = Graffodil
  "swivel" mode, which would violate `.canvas` invariant 2 (sides are
  concrete once loaded — `pickSideToward()` self-heal already guarantees
  this at parse time).
- Converting back: `sideFromString(anchorId)` when writing tool results into
  `CanvasEdge`.

## 3. Class-by-class mapping

### 3.1 `CanvasNodeItem` (NEW) — common node base

```cpp
// libs/canvas/include/canvas/CanvasNodeItem.h
class CanvasNodeItem : public QGraphicsObject, public Graffodil::IGraphNode {
    Q_OBJECT
public:
    explicit CanvasNodeItem(const CanvasNode &data, QGraphicsItem *parent = nullptr);
    // IGraphNode
    QString nodeId() const override;                      // m_data.id
    QList<Graffodil::Anchor> anchors() const override;    // compassAnchors(sceneBoundingRect())
    QRectF nodeBoundingRect() const override;             // sceneBoundingRect()
    QGraphicsItem *graphicsItem() override { return this; }
    void setGeometry(const QRectF &rect) override;        // setPos + update m_data w/h + prepareGeometryChange
    // Shared canvas surface (replaces ConnectableItem + the 3 duplicated enums)
    virtual void setNodeData(const CanvasNode &data);
    CanvasNode nodeData() const;
    enum ResizeMode { NoResize = 0, TopLeft, Top, TopRight, Right,
                      BottomRight, Bottom, BottomLeft, Left };
    ResizeMode resizeModeAtPos(const QPointF &localPos) const; // one impl, kResizeZone as today
Q_SIGNALS:
    void editRequested();          // double-click; subclass-specific meaning
protected:
    CanvasNode m_data;
};
```

- `TextCardItem`, `FileCardItem`, `GroupItem` all inherit `CanvasNodeItem`;
  their **paint code, displayTitle, rendered-document members, and
  double-click handlers are unchanged**. Delete the three per-class
  `ResizeMode` enum copies and the three `resizeModeAtPos` bodies; delete
  `connectionPoint(Side)` (superseded by `anchors()`); delete
  `ConnectableItem.h` entirely.
- The per-item `positionChanged` → adjust-edges lambdas in the scene are
  **deleted**: `GraphScene` adjusts edges via its adjacency index. Keep an
  `itemChange(ItemPositionHasChanged)` → `scene()->adjustEdgesForNode(nodeId())`
  call in `CanvasNodeItem` **only if** verification task V2 (§6) shows
  GraphScene doesn't self-subscribe to position changes.
- `GroupItem` keeps its `itemChange` move-children logic (`m_movingChildren`
  guard, center-containment test) **verbatim** in M1. M4 replaces it.

### 3.2 `CanvasEdgeItem` (rename of `EdgeItem`) — GraphEdgeItem subclass

```cpp
class CanvasEdgeItem : public Graffodil::GraphEdgeItem {
public:
    CanvasEdgeItem(CanvasNodeItem *from, CanvasNodeItem *to, const CanvasEdge &data);
    QString edgeId() const override;      // m_data.id — overrides GraphEdgeItem's auto-id
    void setEdgeData(const CanvasEdge &data);
    CanvasEdge edgeData() const;
private:
    CanvasEdge m_data;
};
```

Constructor wiring:
- base ctor: `(from, sideToString(data.fromSide), to, sideToString(data.toSide),
  std::make_unique<Graffodil::BezierPathStrategy>())`.
- Termini: `setTerminus(ArrowEnd::Target, …TriangleTerminus…)` when
  `toEnd == Arrow`, else `NoTerminus`; same for Source/`fromEnd`. (`reverse()`
  exists upstream for M3's direction menu; unused in M1.)
- Pen: current color logic (`colorFromCanvasColor`, fallback `#969696`, width 2.0)
  via `setPen`.
- Label: `setLabel(data.label)` — Graffodil 6c renders at t=0.5 with a
  background, replacing `EdgeItem::paint`'s hand-drawn label. Style it with
  `EdgeLabelStyle` to approximate today's look (9pt, `#505050` on
  `rgba(255,255,255,220)`, padding ~4). Delete the old `paint()` override
  entirely — base class handles path + termini + label.
- `setHitWidth(24.0)` — matches Obsidian's fat hit path (current code had
  none; this is intended-change #3, §7).
- `setEdgeData`: update pen/label/termini/anchor ids
  (`setSourceAnchorId` / `setTargetAnchorId`), then `adjust()`.

### 3.3 Groups are nodes, not Graffodil groups

Obsidian group nodes have their **own persisted geometry** (`x/y/w/h` in the
file), are resizable, connectable, and z-ordered by area. Graffodil's
`GroupItem` is a decorative backdrop whose bounds *follow* its members and
whose `shape()` is empty (never hit-testable). These are different concepts.
Canvas `GroupItem` therefore stays a `CanvasNodeItem` (§3.1). Any agent
tempted to call `GraphScene::setGroupMembers` for canvas groups is wrong.

### 3.4 `CanvasScene : Graffodil::GraphScene`

Keeps its name, header location, and **every public method the app layer
calls** (`setDocument`, `document`, `setRenderEngine`, `setFileResolver`,
`setFileSaver`, `undoStack`, `renderToImage`, `renderToSvg`,
`addTextCardItem`, `addFileCardItem`, `addGroupItemToScene`,
`addEdgeItemToScene`, `remove*`, `textCardItem`/`fileCardItem`/`groupItem`/
`edgeItem`/`connectableItem` — the last returning `CanvasNodeItem*` now).
Internal changes:

- The four `QHash` item maps are **deleted**. Lookups delegate to GraphScene
  (verify lookup API in V1, §6; if GraphScene lacks by-id node lookup,
  keep ONE `QHash<QString, CanvasNodeItem*>` — but check first).
- `add*Item` bodies become: construct item → `GraphScene::addNode(item)` (or
  `addEdge`) → connect `editRequested` → render content (unchanged logic).
- `remove*Item` → `GraphScene::removeNode(id)` / `removeEdge(id)`
  (ownership semantics per V1).
- Mouse/key overrides: **delete** `CanvasScene`'s
  `mousePressEvent`/`mouseMoveEvent`/`mouseReleaseEvent`/`keyPressEvent`
  overrides *except* the edit-proxy guard, which becomes a thin pre-check
  that then calls the **base** `GraphScene::` handler (GraphScene routes to
  the active tool and gives focused items key priority). Do not re-implement
  tool dispatch.
- `contextMenuEvent` stays as-is (menus unchanged; i18n sweep applies —
  wrap every user-visible string in `i18n()`, file gains
  `#include <KLocalizedString>`).
- Inline-edit machinery (`beginInlineEdit`/`beginFileCardEdit`/
  `beginGroupLabelEdit` + finishers) unchanged.
- Export methods unchanged (hide-edges loop now iterates GraphScene edges).

### 3.5 Tool assembly (in `CanvasScene` ctor)

```cpp
m_defaultTool = new Graffodil::DefaultGraphTool(this);
m_resizeTool  = new CanvasResizeTool(this);           // consumer, §4.3
m_defaultTool->addMouseRoute(m_resizeTool, [this](QGraphicsSceneMouseEvent *ev) {
    return ev->button() == Qt::LeftButton && resizeHit(ev->scenePos()) != 0;
});   // MUST be added BEFORE DefaultGraphTool's own routes → see V3
setActiveTool(m_defaultTool);
```

`resizeHit(scenePos)`: top-most `CanvasNodeItem` at pos, **selected only**
(today's behavior: resize zones only act on the pressed item after it is
selected; preserve: only offer resize when the item under the cursor is
already selected, matching current UX closely enough — note any divergence
during live eyeball), mapped to local, `resizeModeAtPos != NoResize`.

Old `CanvasTool.{h,cpp}` (`CanvasTool`, `SelectMoveTool`, `CreateCardTool`,
`CreateEdgeTool`) are **deleted files**. `CanvasScene::setActiveTool/activeTool`
change signature to `Graffodil::GraphTool*` (grep consumers: only tests/scene).

### 3.6 Undo wiring (intent-signal discipline — tools never touch the model)

| Graffodil signal | Handler in `CanvasScene` | Command pushed |
|---|---|---|
| `SelectMoveTool::dragBegan(nodes)` | snapshot `{nodeId → pos}` | — |
| `SelectMoveTool::dragEnded(nodes)` | diff vs snapshot | `CmdMoveCards` (one command, all moved nodes) |
| `SelectMoveTool::deleteRequested(nodes, edges)` | build compound (port the existing Delete-key body out of the old tool) | parent `QUndoCommand` + `CmdRemoveEdge`×n + `CmdRemoveCard`×n |
| `SelectMoveTool::nodeMoved` | nothing (live position is applied by the tool; doc write happens in `CmdMoveCards::redo`) | — |
| `CanvasResizeTool::resizeCommitted(nodeId, oldRect, newRect)` | direct | `CmdResizeCard` |
| `SelectMoveTool::reverseRequested` | **ignore in M1** (R-key reverse is new behavior; M3 wires it through an edge-direction command) | — |

`CmdMoveCards::redo()` sets document positions, which round-trips back into
item positions via `nodeChanged` — same as today. Verify no double-apply
jitter on drag-release (the tool already moved the items; redo() writes the
same values — idempotent, as today).

### 3.7 `CanvasView` slims down

Delete: `m_panning` middle-drag handlers, `wheelEvent` (bare-wheel zoom).
Keep: `zoomToFit/zoomIn/zoomOut` methods, `keyPressEvent`
(Ctrl+Z/Y, +/−, Home), `drawBackground` grid, `cardDoubleClicked` forward.
Add: nothing. Wheel/middle-drag now reach the scene → `DefaultGraphTool`.
(Check `QGraphicsView` forwards wheel to the scene by default — it does; do
NOT set `dragMode`/`transformationAnchor` interactions that swallow middle
button.)

## 4. New consumer classes

### 4.3 `CanvasResizeTool : Graffodil::GraphTool`

Port of `SelectMoveTool::DragMode::Resize` (CanvasTool.cpp:140–260) verbatim:
same 8-mode geometry math, same `kMinSize = 40.0` clamp, same
qRound-into-`setNodeData` live application, emitting
`resizeCommitted(nodeId, oldRect, newRect)` on release instead of pushing the
command itself. ~120 LOC. Signal-only; no document access.

## 5. Test plan

Must stay green (behavioral contract): `tst_canvasdocument` (untouched code),
`tst_canvas_export`, `tst_canvasscene` all 11 slots — these call scene API
(`addTextCardItem`, `addEdgeItemToScene`, …) whose signatures we keep.
Where a test constructs old tool classes directly, rewrite against the new
seam, preserving the *assertion*, not the mechanism.

New tests (in `tst_canvasscene` unless noted):
- `testFileCardSelectableAndMovable` — press-drag on a `FileCardItem` via
  scene events moves it and pushes `CmdMoveCards` (the gap-#1 regression test;
  MUST fail against pre-M1 code).
- `testRubberBandSelectsAllNodeKinds` — marquee over text+file+group selects
  all three.
- `testDeleteSelectionSingleUndoStep` — delete of mixed selection = one undo
  entry restoring everything (ports the existing guarantee to the new wiring).
- `testResizeToolCommitsUndo` — resize gesture → `CmdResizeCard`, min-size
  clamp respected.
- `testEdgeFollowsNodeMove` — replaces the positionChanged-lambda coverage:
  move node via document, edge path updates (adjacency-index path).
- `testEdgeIdPreserved` — `CanvasEdgeItem::edgeId()` returns the document id,
  not a Graffodil auto-id (guards the §3.2 override).
- Any test touching Graffodil signals with `QSignalSpy`:
  `qRegisterMetaType<Graffodil::IGraphNode*>("IGraphNode*")` in `initTestCase`
  (upstream-documented wart).

Gate: full offscreen suite (313 baseline) + **live eyeball against both
reference vaults** (`~/Documents/business-model-canvas-for-obsidian`,
`~/Documents/design-research-vault`) — open, select/move/resize each node
kind, edit text card, edit group label, export PNG+SVG, undo/redo each,
save and diff the `.canvas` (must be byte-identical for a pure open+move+
undo+save cycle). Project memory: canvas interaction fixes are NOT done at
offscreen-green.

## 6. Verification tasks (do these FIRST — cheap, prevents mid-port surprises)

- **V1 — GraphScene ownership/lookup:** read `libs/graffodil/src/core/src/GraphScene.cpp`:
  does `removeNode(id)` delete the graphicsItem or return it? Is there a
  by-id node getter? Does `addNode` call `QGraphicsScene::addItem` itself?
  Adjust §3.4 mechanics to match (spec assumption: scene adds/removes items;
  deletion semantics unknown).
- **V2 — edge auto-adjust:** does GraphScene subscribe to node position
  changes itself, or must consumers call `adjustEdgesForNode`? (Its "changed"
  signal hookup suggests groups only.) Determines the `itemChange` hook in §3.1.
- **V3 — CompositeTool route order:** confirm `DefaultGraphTool`'s ctor
  routes leave room for a prepended consumer route (read
  `CompositeTool::addMouseRoute` ordering semantics + `DefaultGraphTool.cpp`).
  If first-match-wins and DefaultGraphTool pre-registered its own routes,
  a route added *after* construction is tried *last* — in that case build a
  bespoke `CompositeTool` from parts (`SelectMoveTool`+`PanZoomTool` are
  constructible standalone; `DefaultGraphTool` is only a convenience).
- **V4 — GraphScene key routing to focused editor:** confirm inline-edit
  `QTextEdit` proxy keeps receiving keys (SelectMoveTool's focused-editor
  contract says yes; verify Delete inside the editor does not delete nodes).

## 7. Intended behavior changes (the only three; everything else frozen)

1. **File cards become selectable/movable/resizable** (bug fix; was gap #1).
2. **Wheel = scroll/pan, Ctrl+wheel = zoom-about-cursor** (was: bare wheel
   zoomed, no wheel scroll). This is both Graffodil's default and Obsidian's
   actual behavior — adopting it during the swap avoids porting a defect.
   Keep `+`/`−`/`Home` keys working.
3. **Edges gain a 24px-wide invisible hit zone** (was: exact path stroke).
   Prerequisite for M3's edge context-menu/reconnect ergonomics; matches
   Obsidian's `.canvas-interaction-path`.

Accepted micro-divergences (do not chase): Ctrl+click now *toggles*
selection (Graffodil) where Shift+click previously toggled (Shift now
strictly adds) — Obsidian's shift is additive/XOR, close enough for M1;
exact reconciliation happens in M4 with the rest of §9.5.

## 6a. Verification answers (M1.0, 2026-08-19)

Read in full: `GraphScene.cpp/.h`, `CompositeTool.cpp/.h`, `DefaultGraphTool.cpp/.h`,
`SelectMoveTool.cpp/.h`, plus `IGraphNode.h`, `IGraphEdge.h`, `GraphEdgeItem.{h,cpp}`,
`GraphTool.h`, `Anchors.h`, `EdgePathStrategy.h`, `TerminusStyle.h`, `PanZoomTool.h`,
`Types.h`, `GroupStyle.h`.

- **V1 (ownership/lookup):** `removeNode(id)` calls `removeItem(node->graphicsItem())`
  only — it does **not** `delete` the item; ownership stays 100% with the consumer
  (confirmed by the header doc: "The scene never deletes items"). Same for
  `removeEdge`. By-id lookup exists: `nodeForId(id)` / `edgeForId(id)`. `addNode`/
  `addEdge` call `QGraphicsScene::addItem()` themselves. **Decision: do NOT keep the
  four consumer-side `QHash` maps.** `CanvasScene::textCardItem/fileCardItem/
  groupItem/edgeItem/connectableItem` become one-line wrappers around
  `nodeForId()`/`edgeForId()` + a single `dynamic_cast`/`qgraphicsitem_cast` to the
  requested subtype (this is a *typed accessor*, not the banned "dynamic_cast
  dispatch in interaction logic" pattern from Appendix B item 4 — internal tool/scene
  logic stays type-blind through `CanvasNodeItem`/`IGraphNode`).
- **V2 (edge auto-adjust):** `GraphScene` does **not** self-subscribe to node
  position changes for edges (only groups hook `QGraphicsScene::changed` — see
  `addGroup`/`refreshGroupsFromSceneChange`). `SelectMoveTool::mouseMoveEvent`
  explicitly calls `m_scene->adjustEdgesForNode(n->nodeId())` per dragged node
  during an interactive drag, so live dragging is covered without any node-level
  hook. But **programmatic** position writes (undo/redo replaying `CmdMoveCards`/
  `CmdResizeCard` via `CanvasDocument::nodeChanged` → `CanvasNodeItem::setNodeData`
  → `setPos`) are NOT covered by the tool and would leave edges stale after
  undo/redo or a document-driven move. **Decision: add the `itemChange
  (ItemPositionHasChanged)` hook in `CanvasNodeItem`** (spec §3.1's conditional
  hook) calling `scene()->adjustEdgesForNode(nodeId())` when the item's scene is a
  `Graffodil::GraphScene`. This double-fires during interactive drag (tool call +
  itemChange call) but `adjustEdgesForNode`/`GraphEdgeItem::adjust()` is idempotent
  (recomputes from current anchor positions), so redundant calls are harmless and
  cheap for canvas-scale node counts.
- **V3 (CompositeTool route order):** `CompositeTool::addMouseRoute` **appends**
  (`m_mouseRoutes.append`); only `addAnchorRoute` prepends, and only for its own
  anchor-hit-radius route. `DefaultGraphTool`'s constructor registers `m_select`'s
  4 matchers (incl. plain `Left+NoModifier`) and `m_panZoom`'s middle-button
  matcher **at construction time**, before any consumer code runs. Consequently a
  `CanvasResizeTool` route added via `addMouseRoute` *after* constructing a
  `DefaultGraphTool` would always lose to `m_select`'s plain-left-button route
  (first-match-wins, and `m_select`'s route matches every left-press including
  ones that land on a resize handle). **Decision, per the spec's own fallback
  instruction: do NOT use `Graffodil::DefaultGraphTool`.** Build a bespoke
  `Graffodil::CompositeTool` in `CanvasScene`'s constructor, owning our own
  `Graffodil::SelectMoveTool` and `Graffodil::PanZoomTool` (both constructible
  standalone), and register routes in this order: (1) `CanvasResizeTool` gated by
  the `resizeHit()` predicate, (2) `SelectMoveTool` for
  Left+{NoModifier,Shift,Control,Meta}, (3) `PanZoomTool` for Middle (pan) +
  wheel (Ctrl+wheel zoom, matching `DefaultGraphTool`'s own bindings). This is a
  **divergence from spec §3.5's literal code sample** (which shows constructing
  `Graffodil::DefaultGraphTool` then calling `addMouseRoute` on it) — the
  underlying intent (resize routed ahead of select, same key/wheel bindings as
  `DefaultGraphTool` ships) is preserved exactly, only the assembly mechanism
  changes to a hand-built `CompositeTool`.
- **V4 (focused-editor key routing):** Confirmed both layers guard on
  `focusItem()`: `GraphScene::keyPressEvent`/`keyReleaseEvent` route to
  `focusItem()` first when non-null (before ever reaching `m_activeTool`), and
  `SelectMoveTool::keyPressEvent` independently no-ops when
  `m_scene->focusItem()` is set (belt-and-suspenders per its own doc comment).
  So the inline-edit `QTextEdit` proxy (added via `addWidget()` + `setFocus()`)
  keeps receiving all keys, including Delete/Backspace, once it holds scene
  focus — the base `GraphScene::keyPressEvent` we now delegate to after our
  edit-proxy pre-check already has this contract; no additional guard needed in
  `CanvasScene` beyond the existing "click outside proxy finishes editing" logic.

## 8. File disposition summary

| File | Fate |
|---|---|
| `include/canvas/ConnectableItem.h` | **delete** |
| `include/canvas/CanvasTool.h`, `src/CanvasTool.cpp` | **delete** (resize logic moves to `CanvasResizeTool`) |
| `include/canvas/CanvasNodeItem.h`, `src/CanvasNodeItem.cpp` | **new** |
| `include/canvas/CanvasResizeTool.h`, `src/CanvasResizeTool.cpp` | **new** |
| `include/canvas/EdgeItem.h`, `src/EdgeItem.cpp` | **rewrite** as `CanvasEdgeItem` (~60% smaller) |
| `TextCardItem`, `FileCardItem`, `GroupItem` | **modify**: rebase onto `CanvasNodeItem`, delete dup enums/`connectionPoint`/`resizeModeAtPos` |
| `CanvasScene.{h,cpp}` | **modify**: base class swap, delete item maps + mouse/key dispatch, add signal→command wiring, i18n sweep |
| `CanvasView.cpp` | **modify**: delete pan/wheel handlers |
| `CanvasDocument`, `CanvasTypes`, `CanvasCommands` | **untouched** |
| `libs/canvas/CMakeLists.txt` | link `graffodil-core`; add include path |
| `tests/tst_canvasscene.cpp` | extend per §5 |

Link note: the target name is `graffodil-core` (aliased `Graffodil::Core`);
prefer the `Graffodil::Core` alias — it matches the future exported name so a
later `find_package` switch is build-system-only.
