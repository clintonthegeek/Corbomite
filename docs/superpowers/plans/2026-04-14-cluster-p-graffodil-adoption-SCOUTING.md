# Cluster P — Graffodil adoption (SCOUTING)

> **Living-status note:** This is a *scouting document*, not a plan. Captures the strategic rationale + migration sketch for porting Corbomite's `libs/forcegraph/` and `libs/canvas/` onto Graffodil (`~/dev/Graffodil/`). Live status is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md). When expanded to a full plan, rename to drop `-SCOUTING` suffix and update `INDEX.md`.

**Scouting written:** 2026-04-14.
**Expand to full plan when:** (a) Graffodil's API stabilises for 2–3 weeks of Corbomite's post-A/B work (current v0.1.0, actively evolving — recent `Side`→`Anchor` refactor on 2026-04-13 is the kind of upstream change we'd hit mid-migration), and (b) Cluster A has landed (vault path normalisation affects ForceGraph's node-ID scheme).

**Covers:** an internal architectural refactor — not on the Obsidian-parity roadmap. Consolidates ~3,500 LOC of Corbomite graph/canvas code into a shared library, enabling cross-pollination with PlanStan and reducing Corbomite's maintenance surface.

## Strategic rationale

Graffodil (`~/dev/Graffodil/`, v0.1.0, 40+ source files, 12/12 tests passing) is a QGraphicsView-based graph scene framework explicitly designed to subsume five graph-drawing contexts across PlanStan and Corbomite. The design doc (`~/dev/Graffodil/docs/graffodil-design.md` §Boundary Map) names Corbomite's `ForceGraphNode`/`GraphDataBuilder`/`GraphViewTab` and `TextCardItem`/`FileCardItem`/`CanvasDocument` as planned consumers of `Graffodil::Core + Force + Batch` and `Graffodil::Core` respectively. **The library was built with our migration as a first-class use case.**

The proven implementations Graffodil already contains, transplanted from Corbomite:
- `ForceLayout` + `MultilevelLayout` + `QuadTree` (from `libs/forcegraph/`) → now `Graffodil::Force`
- `BatchRenderer` + `SpatialGrid` (from `libs/forcegraph/`'s batch path) → now `Graffodil::Batch`

And from PlanStan + cross-pollination:
- `SugiyamaLayout` (800 LOC layered layout, PERT-style) — **new capability for Corbomite**
- `CircularLayout` — new capability
- Richer tool framework: `SelectMoveTool`, `CreateEdgeTool`, `PanZoomTool`, `CompositeTool`, `DefaultGraphTool`, `AnchorHighlight`, `GraphMinimap`
- Pluggable `EdgePathStrategy` (Direct, Bezier, Orthogonal, Polyline) + `TerminusStyle` (Triangle, Diamond, Circle, Bar, Open, None) system — **materially richer than what Corbomite has today**

The forward benefit isn't just code-size reduction. It's that **every future improvement to arrowheads, anchors, tool behaviour, and batch rendering is shared with PlanStan** at zero additional maintenance cost. Corbomite gets Sugiyama layout "for free" if we ever want to render a PERT-style view of our force graph or a layered file-dependency visualisation.

## Subsumption map (what Graffodil replaces in Corbomite)

**From `libs/forcegraph/` (~3,283 LOC):**

| Corbomite file | Replacement |
|---|---|
| `ForceLayoutEngine.cpp` | `Graffodil::ForceLayout` (already a transplant) |
| `MultilevelLayout.cpp` | `Graffodil::MultilevelLayout` |
| `QuadTree.cpp` | `Graffodil::QuadTree` |
| `BatchNodeItem.cpp`, `BatchEdgeItem.cpp` | `Graffodil::BatchRenderer` + internal batch items |
| Most of `ForceGraphScene.cpp` | `Graffodil::GraphScene` (plus Corbomite's selection/focus policies on top) |
| `ForceGraphEdge.cpp` (line + arrowhead drawing) | `Graffodil::GraphEdgeItem` + `DirectPathStrategy` + `TriangleTerminus` |
| `ForceGraphNode.cpp` | **Kept** — adapts to implement `Graffodil::IGraphNode` |
| `ForceGraphView.cpp` | **Kept** — QGraphicsView with Corbomite-specific chrome (minimap, search filter, zoom-to-fit) |
| `GraphDataBuilder` | **Kept** — Corbomite-specific (converts VaultModel + MetadataCache into nodes/edges) |

Estimated net code change: ~1,500 LOC deleted, ~700 LOC rewritten to use Graffodil interfaces. `libs/forcegraph/` shrinks to ~1,100 LOC of Corbomite-specific adaptation code.

**From `libs/canvas/` (~3,851 LOC):**

| Corbomite file | Replacement |
|---|---|
| `CanvasTool.cpp` base | `Graffodil::GraphTool` base |
| Canvas edge-creation tool | `Graffodil::CreateEdgeTool` |
| Canvas pan/zoom | `Graffodil::PanZoomTool` |
| Anchor-highlight code | `Graffodil::AnchorHighlight` |
| Most of `CanvasScene.cpp` plumbing | `Graffodil::GraphScene` |
| `EdgeItem.cpp` (cubic bezier + arrows) | `Graffodil::GraphEdgeItem` + `BezierPathStrategy` + configurable terminus |
| `CanvasView.cpp` | **Kept** — Canvas-specific chrome |
| `TextCardItem.cpp`, `FileCardItem.cpp`, `GroupItem.cpp` | **Kept** — adapt to implement `Graffodil::IGraphNode`; Corbomite-specific (markdown rendering via RenderEngine, file embedding, group behaviour) |
| `CanvasDocument.cpp` (JSON Canvas 1.0 format) | **Kept** — bespoke on-disk format; not Graffodil's concern |
| `CanvasCommands.cpp` (undo/redo) | **Kept** — Qt undo stack integration; Graffodil tools emit signals, Corbomite issues undo commands |
| `CreateCardTool.cpp` | **Kept** — canvas-specific (which card type to create?) |

Estimated net code change: ~500 LOC deleted, ~1,200 LOC rewritten to use Graffodil interfaces. `libs/canvas/` shrinks to ~2,150 LOC of Corbomite-specific content (cards, documents, undo).

**Total: ~3,500 LOC of Corbomite code consolidated into a shared library.** Plus material new capabilities (Sugiyama, richer edge-path strategies, AnchorHighlight, GraphMinimap, pluggable TerminusStyle).

## KDE / GPL3-compatible prior art

**Local KDE source convention:** as with A–O, `~/src/kde/src/<repo>` is the grep-first location.

| Target | Local path | Note |
|---|---|---|
| **Graffodil itself** | `~/dev/Graffodil/` (not under `kde/src/` but local) | Primary reference. `docs/graffodil-design.md` (756 lines) has the full API spec; `docs/research/03-corbomite-forcegraph.md` and `04-corbomite-canvas.md` are our own inventories that drove Graffodil's design |
| Batch-rendering large QGraphicsScene | `~/src/kde/src/kdevelop/kdevplatform/` — grep for `QGraphicsScene` tuning patterns in any code-flow-graph visualisations | Secondary reference; Graffodil's `BatchRenderer` is the primary solution |
| Group / container items | `~/src/kde/src/krita/` (if present — verify) or Inkscape source (external GPL) | Inkscape's SPG model is the richest open-source group semantics reference; Canvas's GroupItem may want to inform or be informed by it |

## Expansion triggers & sequencing

Per `graffodil-design.md` §Migration Path, the prescribed order is:

1. **Canvas first** (lightest existing consumer, already well-isolated).
2. **ForceGraph second** (largest migration, validates performance at 10k+ nodes via Batch module).

That's the order we adopt. Benefits: Canvas migration catches integration bugs with our undo-stack and document-format concerns before ForceGraph's batch-rendering migration adds perf-test complexity.

**Expansion trigger conditions:**
- Graffodil API stable for 2–3 weeks (current active evolution is exactly what would hurt us mid-migration). Observable via `git log` on `~/dev/Graffodil/`.
- Cluster A landed (vault path normalisation affects ForceGraph's node-ID scheme — today node IDs are relative file paths; post-A they'll resolve through `LinkResolver`).
- No hard blocker from B–O; Cluster P is *parallelisable* with most of the parity roadmap.

## Work breakdown sketch (for planning, not prescriptive)

### Phase 1 — Canvas migration (smaller, de-risks framework)
1.1. Add `~/dev/Graffodil/` as a dependency in Corbomite's top-level `CMakeLists.txt` (add_subdirectory or find_package). Build must stay green.
1.2. `TextCardItem`, `FileCardItem`, `GroupItem` implement `Graffodil::IGraphNode`. `EdgeItem` replaced by `Graffodil::GraphEdgeItem` with a `BezierPathStrategy` instance.
1.3. `CanvasScene` becomes a `Graffodil::GraphScene` subclass (or owner); ad-hoc tool-dispatch replaced with `GraphScene::setActiveTool(...)`.
1.4. Canvas-specific tools (SelectMove, PanZoom, CreateEdge, AnchorHighlight) swapped for Graffodil's. `CreateCardTool` stays (Canvas-specific UX).
1.5. `CanvasView` keeps its chrome; consumes `GraphScene` underneath.
1.6. Undo/redo (`CanvasCommands`) wires via Graffodil tool signals — e.g. `SelectMoveTool::nodeMoved(node, old, new)` → Corbomite creates an undo command.
1.7. Delete old canvas tool/edge code. Run existing canvas tests; port or delete obsolete.
1.8. Smoke test: open an existing `.canvas` file, verify identical visual rendering + behaviour.

### Phase 2 — ForceGraph migration (larger, validates Batch)
2.1. `ForceGraphNode` adapts to `Graffodil::IGraphNode`. `ForceGraphEdge` replaced by `GraphEdgeItem` + `DirectPathStrategy`.
2.2. `ForceGraphScene` becomes a `Graffodil::GraphScene` subclass with a `Graffodil::BatchRenderer` attached.
2.3. `ForceLayoutEngine` replaced by `Graffodil::ForceLayout`. Same force constants, same multilevel path.
2.4. `MultilevelLayout` replaced by `Graffodil::MultilevelLayout`.
2.5. `QuadTree` deleted; Graffodil provides the same algorithm.
2.6. `BatchEdgeItem` / `BatchNodeItem` deleted; Graffodil's `BatchRenderer` handles both.
2.7. `ForceGraphView` keeps chrome (search filter, zoom-to-node animation, minimap). Consumes `GraphScene` underneath.
2.8. `GraphDataBuilder` unchanged in algorithm but outputs Graffodil-shaped node/edge IDs (string paths — Graffodil already uses `QString` IDs).
2.9. Benchmark: 10k-node vault render must match or beat current performance. If regression, investigate upstream with Graffodil.
2.10. Delete obsolete tests (`tst_quadtree`, `tst_forcelayout`, `tst_benchmark_layout` in Corbomite — Graffodil has equivalents). Keep `tst_graphdatabuilder` (Corbomite-specific).

### Phase 3 — Upstream contributions (feedback to Graffodil)
Write a `docs/graffodil-feedback.md` in Graffodil itself documenting any rough edges we hit. Submit patches back for:
3.1. Any Canvas-specific need that generalises (e.g. resizable-node mixin if we surface one).
3.2. Any performance issue found in the 10k-node path.
3.3. Clarifications to `graffodil-design.md` where our migration revealed ambiguity.
3.4. Qt Quick rendering exploration? Not urgent, but could unlock 100k-node graph performance. Note as a future epic.

## Recommendations to expand Graffodil (informed by Corbomite's use cases)

Full filtered analysis written as a feedback doc *to Graffodil*: **`~/dev/Graffodil/docs/corbomite-integration-feedback.md`**. The filtering discipline: upstream only what's *generally useful* to Graffodil's five named consumers (PERT, Gantt, sync topology, ForceGraph, Canvas); keep Corbomite-specific whiteboard/document-canvas features internal to avoid overloading Graffodil and violating its "core stays lightweight, don't dictate node appearance" non-goals.

**Upstream to Graffodil (3 items):**

1. **`IRenderableCache` mixin.** Optional interface for rich-content nodes the `BatchRenderer` can cache. Drivers: Canvas `TextCardItem`, `FileCardItem`; ForceGraph `BatchNodeItem`. General need (PERT tasks with content, sync-topology device icons also benefit). ~1 day upstream work; backwards-compatible via `dynamic_cast`.

2. **Undo-integration signal contract — doc + audit.** Document every mutation signal each provided tool emits; audit for gaps (edge redirect, anchor change, multi-select-drag aggregation, tool-activation). Possibly add per-user-action-boundary signals (`dragBegan`/`dragEnded`) alongside existing per-atomic-mutation signals. Driver: `CanvasCommands` migration needs complete coverage. Universal need.

3. **Alignment snap-guide extensibility — verify first.** Graffodil's `SelectMoveTool` claims "optional alignment snap guides"; confirm whether it matches Canvas's alignment-line-overlay UX and whether the consumer can customise alignment-target queries + guide rendering. If already flexible → docs only. If rigid → small extension-point addition.

Plus **long-term awareness** (not near-term): Qt Quick rendering fallback for ≥100k-node scenes. Flag to preserve API shape compatibility.

**Kept internal to Corbomite (3 items that would overload Graffodil):**

1. **`GroupItem` / container nodes.** Only Canvas-shaped consumers have groups (PERT doesn't, Force doesn't, sync topology doesn't). Upstreaming `IGraphGroup` pollutes Core for one consumer class. Corbomite implements grouping above Graffodil: `GroupItem` is a Corbomite `QGraphicsItem` that implements `IGraphNode` (Graffodil sees it as a plain node), and `CanvasScene` intercepts move events to propagate to children.

2. **Resizable-node decorator / 8-point resize handles.** Only whiteboard cards are user-resizable; other graph views have content-sized or fixed-size nodes. Corbomite implements resize handles directly in `TextCardItem`/`FileCardItem`, as today.

3. **Obstacle-aware bezier routing.** Implement as a Corbomite-local `ObstacleAwareBezierStrategy : public Graffodil::EdgePathStrategy` — Graffodil's extension point already covers this. Reconsider upstreaming only if the implementation proves clean and another consumer asks.

This filtering means Cluster P's upstream work is modest (~2–3 days of patches + docs) while still unblocking both migrations cleanly.

## Risks

1. **Graffodil is v0.1.0 and actively evolving.** Recent `Side`→`Anchor` refactor (2026-04-13) is exactly the kind of upstream change that would force rework mid-migration. **Mitigation: pin to a commit + observe 2–3 weeks of stability before expanding this scouting doc.** Graffodil's git log becomes an expansion-trigger input.

2. **Performance must not regress at 10k+ nodes.** Graffodil claims the same algorithm, but wrapping in interfaces + registry lookups can cost cache-friendly tight loops. Benchmark gates Phase 2 sub-step 2.9. Have a rollback plan (keep `libs/forcegraph/` around under a CMake flag during migration).

3. **Canvas document format compat is load-bearing.** `.canvas` files must round-trip byte-identically. `CanvasDocument` stays in Corbomite; verify no coupling leaks into Graffodil-side data representation during migration.

4. **Test infrastructure replacement.** Corbomite's forcegraph tests (`tst_quadtree`, `tst_forcelayout`, `tst_benchmark_layout`) are load-bearing. Deleting requires confidence that Graffodil's equivalents cover the same ground. Audit test coverage before deletion, not after.

5. **Scope creep into Markoff.** TextCardItem uses a markdown render engine (currently `MarkdownRenderEngine` for canvas cards). Graffodil shouldn't touch Markoff. Keep the boundary clean during migration.

## Do NOT do

- **Start now.** Graffodil is evolving. Wait for API stability.
- **Migrate ForceGraph first.** The design doc prescribes Canvas first for good reasons. Honour it.
- **Expand this scouting doc into a full plan before observing Graffodil git-log for 2 weeks of stability.** A `Side`→`Anchor`-scale refactor mid-Corbomite-migration is expensive.
- **Absorb Graffodil into Corbomite's tree (as a vendored copy).** Keep it as a sibling-directory external dep, so improvements flow back to PlanStan too.

## Sequencing notes

- **Not blocked by A/B/C/D/E** — Cluster P is an internal refactor parallelisable with the parity roadmap.
- **Beneficial ordering:** after Cluster A (so ForceGraph's node IDs resolve via the correct `LinkResolver`) and after Cluster I (so graph data builds from fine-grained MetadataCache events).
- **Unblocks:** cleaner future work on both graph view and canvas. Also enables sharing any future improvements with PlanStan (arrowhead styles, edge routing, tool UX).

## Open questions when expanding

1. Does Graffodil install cleanly as a sibling CMake project into Corbomite, or should we `add_subdirectory(../Graffodil)`? Both work; prefer `add_subdirectory` during co-development, install-target once both codebases stabilise.
2. Does `Graffodil::GraphMinimap` (transplanted from PlanStan) drop in for Corbomite's existing minimap behaviour, or is there Corbomite-specific chrome to preserve?
3. How much of Canvas's tool-state machine actually generalises to `GraphTool`? Likely ~90%, but the document-format-specific logic in `CreateCardTool` (card colour selection, default text) stays in Corbomite.
4. Undo granularity: Graffodil's `SelectMoveTool::nodeMoved(old, new)` emits per drag. Does Canvas's current CanvasCommands expect per-atomic-move or per-drag-sequence? Check before migration; may need Graffodil to expose both granularities (begin-drag + end-drag signals in addition to nodeMoved).
5. The recent commit `91bd585 feat(core): GraphScene::edgeBetweenUndirected` suggests undirected-edge awareness. Canvas edges are directional. Confirm no behavioural change.
