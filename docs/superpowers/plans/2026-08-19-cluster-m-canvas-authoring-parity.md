# Cluster M — Canvas authoring parity (Graffodil rebase first)

**Opened:** 2026-08-19. **Type:** Full plan. **Track:** strategic cluster.
**Decision of record:** user chose **"Graffodil rebase first"** over extending the
hand-rolled `CanvasScene` (2026-08-19 session) — the substrate migration is Phase M1,
and all authoring features build on Graffodil's tool/anchor/snap plumbing.

## Goal

Corbomite opens Obsidian-authored `.canvas` files faithfully (interop bundle shipped
2026-05-28; order-preservation + vault-root path fixes 2026-08-19) but gives the user
almost no way to **author** one. Close the authoring gap against Obsidian's canvas
feature catalogue (`docs/obsidian-audit/domains/canvas.md` §9) so a user can build
canvases like `business-model-canvas-for-obsidian` / `design-research-vault` from
scratch: create text/file/group nodes fluidly, draw and reconnect edges, snap and
align, and have canvas links participate in the vault graph.

**Explicit non-goals (deferred, decision-gated — see §Deferred):** per-card content
fidelity strategy (audit §11.3), link nodes (webview dependency), byte-exact
serializer parity.

## Where we are (verified against code 2026-08-19)

Working today (`libs/canvas`, ~3.8k LOC + `src/canvas/` app shell):
- `.canvas` round-trip is **lossless and diff-stable** (unknown-field passthrough,
  default omission, V5 side self-heal, integer rounding, node/edge order preserved).
- `SelectMoveTool`: click-select, shift-toggle, marquee, move, edge/corner resize
  (`resizeModeAtPos`), Delete (single compound undo), arrow nudge (1px/10px), Ctrl+A.
- Context menus: Edit / Duplicate / Delete / Color (6 presets) on cards; Edit Label /
  Reverse Direction / Delete on edges; New Text Card / New Group on empty space.
- Inline editing via `QGraphicsProxyWidget` (`QTextEdit`) for text cards, file cards,
  group labels; `QUndoStack` with 8 command classes; PNG **and** SVG export (ahead of
  Obsidian); middle-drag pan, wheel zoom, Home = zoom-to-fit.

Broken / missing (the authoring cliff):
1. **File cards are not selectable or movable.** `SelectMoveTool::mousePressEvent`
   dynamic_casts only `TextCardItem`/`GroupItem`; a click on a `FileCardItem` falls
   through to rubber-band. The user's reference canvases are ~all file cards — they
   literally cannot be rearranged. (Same per-type-cast disease in
   `CreateEdgeTool::findNearCard`, `CanvasTool.cpp:478`.)
2. **No edge-creation affordance.** `CreateEdgeTool` (press/drag/release + preview
   line + undo command) is complete but **dead code** — never instantiated
   (`CanvasScene.cpp` only creates `SelectMoveTool`). No hover connection points.
3. **No creation flows** beyond the two context-menu entries: no double-click-empty →
   text card, no drag-drop files → file nodes, no paste (text/URL/canvas-JSON/image),
   no Alt-drag duplicate, no card-menu toolbar, no "New file card" at all.
4. **No snapping** — grid is drawn but not snapped-to; no object alignment guides.
5. **No edge endpoint reconnect**, no direction submenu (only Reverse), drop-on-empty
   does nothing.
6. Group semantics diverge: center-test membership (Obsidian: full containment,
   recomputed at grab), no `zIndex = -area` stacking.
7. **No viewport persistence** (Obsidian: `{x, y, zoom(log2)}` in workspace leaf
   eState), no read-only lock, no `jump-to-group`/`convert-to-file` commands.
8. **Canvas links don't feed the metadata graph** — no backlinks/unresolved-links
   participation, no rename rewrite of `file`/`background`/`subpath`.
9. i18n convention violated throughout canvas menus (`QStringLiteral` for
   user-visible strings).

## Audit references

- `docs/obsidian-audit/domains/canvas.md` — §9.2–9.6 (chrome/edges/creation/
  selection/viewport: the feature spec for M2–M5), §8 invariants 2–5 & 10 (edge
  anchors, defaults, group semantics, V5-vs-A3 side algorithms), §7 + §9.9 (link
  index, M6), §11.2 (the Graffodil library/consumer boundary M1 executes), §13 open
  questions 1–2.
- `docs/superpowers/plans/archive/2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md`
  (archived; use as background only — Graffodil has moved from v0.1.0 to v0.2.2 since).
- `~/dev/Graffodil/docs/ROADMAP.md` — phases 1–5 + 6c (edge labels) + 6f
  (graph-editor affordances) complete; **6d (CMake install/export) still open** and
  named as the consumer-migration gate.
- `~/dev/Graffodil/docs/corbomite-integration-feedback.md` — our own 2026-04-14
  requirements doc; 6f was built from it.

## Target classes

| Area | Classes |
|---|---|
| Rebase (M1) | `Canvas::CanvasScene` → subclass `Graffodil::GraphScene`; `TextCardItem`/`FileCardItem`/`GroupItem` implement `Graffodil::IGraphNode` (+`IRenderableCache`); `EdgeItem` → `Graffodil::GraphEdgeItem` + a `CanvasBezierStrategy : EdgePathStrategy`; tools → `Graffodil::{SelectMoveTool, CreateEdgeTool, PanZoomTool, DefaultGraphTool}`; `CanvasView` slims to a shell |
| Kept as-is | `CanvasDocument`/`CanvasTypes` (all of §3 disk contract), `CanvasCommands` (`QUndoStack` — now driven by Graffodil tool intent-signals), export paths, `CanvasViewTab`/`CanvasFileView` |
| App side (M5/M6) | `WorkspaceSerializer` (eState viewport), `SessionManager` tier-3 (read-only flag), `MetadataCache`/`SQLiteIndex`/`LinkResolver` + `FileManager` rename rewriting (M6), `MainWindow` commands |

## KDE/Qt prior art

- Graffodil itself is the prior art for scene/tools/anchors/minimap (built for this).
- KDE `kdiagram`/`calligra` flow layers for snap-guide rendering conventions;
  `QGraphicsItem::ItemIgnoresTransformations` for zoom-constant chrome (audit §11.3
  table); `QVariantAnimation` for eased camera.

## Work breakdown

### Phase M0 — Graffodil readiness gate (cross-repo, small) — **CLOSED 2026-08-19**

> **Resolution:** Graffodil replied same day
> (`~/dev/Graffodil/docs/handoff/2026-08-19-reply-to-corbomite-canvas-adoption.md`):
> submodule consumption blessed (6d not required; Graffodil's own `origin` IS
> GitHub, no mirror-lag risk); pin **`v0.2.3`** (`dd7667de`), tagged same day with
> the M3 gesture gap already fixed upstream — `CreateEdgeTool` supports
> press-drag-release (use `setDragOnly(true)` for Obsidian-strict cancel) and a new
> `edgeDroppedOnEmpty(source, anchorId, scenePos)` intent signal covers
> release-on-empty (tool resets before emitting; self-loop is a silent cancel, not
> a drop). `IAlignmentStrategy` confirmed consumer-side. No eased camera upstream —
> Corbomite drives `QVariantAnimation` over `applyZoom()` in M5 as planned.
> Executed: submodule added at `libs/graffodil` (GitHub URL, checked out
> `v0.2.3`), `add_subdirectory` wired after markoff-family (demo/tests auto-OFF
> via `PROJECT_IS_TOP_LEVEL`), dev preset builds all six `libgraffodil-*.a`,
> full offscreen suite **313/313**. Test-side note for M1+: spying on
> `edgeDroppedOnEmpty`/`edgeRequested` needs
> `qRegisterMetaType<IGraphNode*>("IGraphNode*")`.
- Write a handoff doc (`docs/handoff/2026-08-19-to-graffodil-for-canvas-adoption.md`)
  asking Graffodil to land **6d (CMake install/export)** or bless submodule
  `add_subdirectory` consumption; per `~/dev/CLAUDE.md` symlink rule the consumption
  mechanism is a **git submodule** (Codeberg-primary Graffodil; mirror question same
  as markoff-family precedent) unless 6d's installed-package route is preferred.
- Verify in Graffodil: `PanZoomTool` zooms about cursor + eases (audit §11.2 flag);
  anchor model supports exactly-4 face-midpoint compass anchors; `CreateEdgeTool`
  supports live nearest-face snap (A3 semantics) distinct from load-time V5.
  File Graffodil issues/specs for any gap — **Graffodil-side work happens in
  Graffodil's repo under its own roadmap**, not here.
- Exit: pinned Graffodil revision building inside Corbomite's dev preset.

### Phase M1 — Substrate rebase, feature-frozen
Port `libs/canvas` onto Graffodil::Core with **zero intended behavior change**; the
existing test suite (`tst_canvasdocument`, `tst_canvasscene`, `tst_canvas_export`,
`tst_canvasviewtab_vaultroot_paths`) plus a new golden round-trip fixture is the gate.
- `CanvasScene : Graffodil::GraphScene`; item maps/tool state machine deleted
  (~1.3–1.8k LOC per audit §11.2 estimate).
- Node items implement `IGraphNode`; **fix gap #1 for free** by routing selection/
  move/resize through Graffodil's type-blind node protocol (file cards become
  first-class citizens).
- `CanvasBezierStrategy`: `clamp(dist/2, 70, 150)` control offset, 7px face inset,
  13×10.4 arrow polygon, face-midpoint anchors (`AnchorIds::{Top,Right,Bottom,Left}`).
- Undo: Graffodil tools emit intent (`nodeMoved`, `edgeRequested`, `deleteRequested`)
  → existing `Cmd*` classes push to the stack; tools never mutate `CanvasDocument`.
- i18n sweep of all canvas user-visible strings while touching the menus (gap #9).
- Exit: current feature set intact live (eyeball against both reference vaults),
  suite green, export unchanged.

### Phase M2 — Node creation flows (audit §9.4)
- Double-click empty → new text card in edit mode (`mouseDoubleClickEventBackground`
  hook); Obsidian defaults: text 250×60, file 400×400.
- Context menu: New text card / **New file card** (fuzzy vault-file picker) / New
  group; "New canvas" command + file-explorer context entry (`canvas:new-file`
  parity, opens with rename state).
- Drag-drop: file(s) from FileExplorer or OS → file node(s) at drop point (grid
  layout for multiple); text drop → text card.
- Paste: plain text → text card; canvas-JSON subgraph → clone + re-ID (16-hex);
  copy of selection produces that JSON (enables cross-canvas copy with Obsidian).
- Alt/Ctrl-drag duplicate.
- Exit: user can build the business-model canvas layout from an empty file without
  touching JSON.

### Phase M3 — Edge authoring (audit §9.3, §8 inv. 2/3/10)
- Hover chrome: 4 face-midpoint connection points appear on node hover
  (`AnchorHighlight`); drag from a point runs `CreateEdgeTool` with live preview.
- Works against **all** node kinds (text/file/group).
- Endpoint reconnect: drag an existing edge end to re-anchor (A3 nearest-face
  snap); drop on empty space deletes the edge (Obsidian semantics) — via undo cmd.
- Edge context menu: direction none/unidirectional/bidirectional (supersedes bare
  "Reverse"); label editing stays (Graffodil 6c can take over rendering at t=0.5).
- Stretch (Obsidian: edge-drag to empty → node-create menu): defer if noisy.
- Exit: draw, label, redirect, reconnect, and delete edges entirely with the mouse;
  files written are Obsidian-clean (concrete sides, default omission).

### Phase M4 — Move/snap/selection polish (audit §9.5, §9.2)
- `IAlignmentStrategy` impl: snapToGrid (zoom-dependent spacing 20/40/80/160) +
  snapToObjects (corners+centers of nearby nodes, 15px/scale tolerance, accent
  guide lines); both default ON, persisted; no-snap modifier Alt.
- Shift axis-lock on move; shift aspect-lock on resize; arrow-key nudge =
  gridSpacing (×5 with shift) replacing today's 1px/10px.
- Group semantics: full-containment membership recomputed at grab time; dragging a
  group moves enclosed nodes; `zIndex = -width*height`.
- Visible 8-handle resize chrome (zoom-constant via `ItemIgnoresTransformations`),
  cursor feedback.
- Exit: cards align as effortlessly as in Obsidian; a saved file's geometry stays
  integer + stable.

### Phase M5 — Viewport, commands, persistence (audit §9.6, §6, §2-ephemeral)
- Eased camera (`QVariantAnimation`), zoom clamp log2 [-4, 1], zoom about cursor;
  Shift+1 zoom-to-fit / Shift+2 zoom-to-selection; on-canvas control cluster
  (lock, zoom ±/reset/fit, undo/redo).
- Viewport `{x, y, zoom(log2)}` persisted in workspace leaf eState — **Obsidian
  schema-at-rest fidelity per Cluster L0 doctrine** (this travels in the shared
  `workspace.json`; coordinate with `WorkspaceSerializer` tiers).
- Read-only lock: machine-local (Obsidian uses localStorage) → tier-3 app-data
  store, keyed by path; gates all editing.
- Commands: `jump-to-group` (fuzzy suggest → zoomToBbox), `convert-to-file`
  (single selected text node → new note + file node swap).
- Exit: reopening a canvas restores the view; lock works; both commands in palette.

### Phase M6 — Vault integration (audit §7, §9.9)
- Index canvas text-node wikilinks + file-node paths into
  `MetadataCache`/`SQLiteIndex` → backlinks/outgoing/unresolved parity.
- Rename rewrite: vault file rename updates `file`/`background` fields and
  `subpath`s in canvases (FileManager link-rewrite path); canvas rename updates
  inbound links.
- Missing-file placeholder card (create/swap/remove) that self-resolves on vault
  create/rename events.
- Exit: a canvas behaves like a first-class vault citizen in search/backlinks.

## Explore-agent dispatch prompts

- *M0:* "Read ~/dev/Graffodil/src/core/{GraphScene,SelectMoveTool,CreateEdgeTool,PanZoomTool,Anchors,AnchorHighlight,IAlignmentStrategy,GroupItem}.h and docs/graffodil-design.md. Report: exact zoom/easing behavior of PanZoomTool; how anchors are declared per-node and whether a 4-compass-anchor restriction is native; CreateEdgeTool's snap + preview contract and its signals; how consumers feed undo; what 6f (v0.2.2 affordances) added. Cite headers by path:line."
- *M1:* "Map every behavior in Corbomite libs/canvas/src/{CanvasScene,CanvasTool}.cpp (selection, marquee, resize modes, inline-edit proxies, context menus, key handling) to its Graffodil::Core equivalent or mark it consumer-retained. Output a two-column migration table."
- *M6:* "Trace how Corbomite indexes markdown wikilinks end-to-end (MetadataCache → SQLiteIndex → LinkResolver → backlinks UI) and where FileManager rewrites links on rename; identify the seams a canvas link source must plug into."

## Definition of done

- All six phase exit criteria met; each phase closes with the standard ritual
  (offscreen suite green **plus live eyeball** — project memory: canvas/editor
  interaction fixes must be re-tested live before closing).
- Golden round-trip test: open → author (every M2/M3/M4 gesture) → save produces
  Obsidian-clean JSON (verified against a real Obsidian re-open at least once).
- `PARITY-MATRIX.md` §5 canvas rows updated; punch-list `[canvas][NEEDS-DESIGN]`
  edge-creation item closed pointing here.

## Blocks / enables

- **Blocked by:** ~~Graffodil 6d (or submodule-consumption sign-off)~~ — resolved 2026-08-19, see M0 resolution above. No upstream blockers remain; M1 is dispatchable.
- **Interacts with:** Cluster L5 soak (M5's eState work touches
  `WorkspaceSerializer`; sequence after L5 resumes/closes or coordinate).
- **Enables:** Graffodil migration precedent for `libs/forcegraph` (same substrate);
  card-fidelity brainstorm (§Deferred) lands on a stable authoring base.

## Deferred (needs its own brainstorm before any phase picks it up)

1. **Card content fidelity (audit §11.3)** — the audit's webview-vs-snapshot fork now
   has a **third option that didn't exist when it was written**: embed
   `Markoff::Canvas::EditorWidget` (since 2026-08-18 the sole LivePreview engine, a
   plain QWidget) per card via proxy for editing, with `StyledRenderEngine` snapshots
   (+`IRenderableCache`) for display. Likely the native sweet spot; needs perf
   validation (N editors) and a zoom-virtualization design (audit §8 inv. 8).
2. **Link nodes** — `NodeType::Link` parses/round-trips but has no item; rendering a
   live web page needs `QWebEngineView` (heavy new dep). A static "URL card" interim
   (favicon/title + open-in-browser) could ship without the dep — decide in M2 review.
3. **Byte-exact `dc()` serializer parity** (audit §13 Q4) — only matters for diff-noise
   in shared vaults; current output is parse-compatible.
