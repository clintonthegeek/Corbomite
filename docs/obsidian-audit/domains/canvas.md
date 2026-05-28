# `obsidian/canvas` — JSON-Canvas infinite-whiteboard core plugin

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/app.pretty.js` (NOT a `tree/obsidian/<domain>/` directory — see artifact note)
**Audit date:** 2026-05-27 (audit-expansion pass; fulfils the placeholder promised in [`../addenda/2026-04-19-canvas-export-as-image.md`](../addenda/2026-04-19-canvas-export-as-image.md))
**Approx. source span:** `app.pretty.js:186300–196260` (canvas controller, nodes, edges, index, view, plugin) + CSS at `app.css:16400–17293`
**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> N/A — Canvas was never assigned a Pass-1 routing-table section. It is name-dropped in `domains/views.md` (`.canvas` → `CanvasView`) and `VAULT-FORMAT.md` (`.canvas` JSON schema) only. This document is a Pass-2-equivalent audit produced out of band.

**De-minifier artifact note:** Unlike the other 15 domain docs, the canvas feature is **not** present in the `renamed/obsidian/tree/` split — the de-obfuscator only carved named public-API declarations and the `obsidian/<domain>/` namespaces; the canvas implementation is internal (no public API symbols) so it survives only in the monolithic beautified bundle `renamed/app.pretty.js`. All identifiers are minified single/double-letter+digit symbols (`y5`, `S5`, `G5`, …) that **will change between Obsidian builds** — cite them by *role*, not by letter. There is no per-file source-line comment to bound; the span above was located by landmark grep (`canvas-node`, `fromNode`, `registerViewType`, `this.id="canvas"`). The `renderSuggestion` blocks at `195973–196260` are the **command-palette** plugin, not canvas (one agent's slice overlapped it) — excluded here.

**Symbol→role map (this build only, for re-tracing):**

| Symbol | Role |
|---|---|
| `y5` | Canvas controller (the scene/model: nodes, edges, selection, viewport, save) |
| `S5` | Abstract base node |
| `x5` | Content-bearing node base (mount/unmount/blocker/placeholder) |
| `A5` | Editable-child base (text + file share) |
| `I5` / `P5` / `F5` / `R5` | Text / File / Link / Group node |
| `T5` | Dummy "ghost" node used during drag-to-connect / duplicate-drag |
| `M5` | Node interaction-layer overlay (resizers + connection points) |
| `z3` | Multi-select selection box (its own 8 resizers) |
| `G5` / `K5` | Edge / Edge-label element |
| `e8` / `t8` / `n8` | Per-canvas link **index** / metadataCache link-updater / per-file local-data store |
| `i8` / `r8` | Canvas core **plugin** / canvas **view** (`extends TextFileView`) |
| `cc(16)` | 16-hex-char id generator; `dc()` tab-indent JSON pretty-printer |
| `H5`/`z5`/`q5` | Face-midpoint anchor / 7px inset / outward-normal offset |
| `V5` / `A3` | Angular auto-side picker (load-time, sticky) / nearest-face drag-snap (live) |
| `j5` | Cubic-bezier edge path builder |

---

## 1. Public API surface

Canvas exposes **no public JS API** (no `module.exports`, not in the 502 public declarations). Its only outward surfaces are the *view type*, *commands*, *workspace events*, and *registry registrations* (Sections 4–7, 10). Architecturally there are seven internal classes worth naming:

- **Canvas controller `y5`** (`app.pretty.js:188317`) — the model+scene: owns `nodes`/`edges` maps, `selection` Set, the viewport transform, undo `history`, and the `requestSave`/`requestFrame` loops. Owns DOM `wrapperEl` → `canvasEl` (`.canvas`) → `edgeContainerEl` (SVG). Instantiated once per canvas view; lives for the view's lifetime.
- **Node hierarchy** `S5` → `x5` → `A5` → {`I5` text, `P5` file}; `F5` link and `R5` group derive directly (`F5` from `x5`, `R5` from `S5`). Each node owns a 0×0 `.canvas-node` anchor with an absolutely-positioned `.canvas-node-container`.
- **Edge `G5`** (`app.pretty.js:194071`) — owns the dual SVG path (visible + hit), arrowhead `<g>`, and optional label `K5`.
- **Index `e8`** (`194731`), **link-updater `t8`** (registered as `metadataCache.linkUpdaters["canvas"]`), **view `r8`** (`extends TextFileView`), **plugin `i8`** (`this.id="canvas"`, `195041`).

Method groups are documented by role in Sections 8–10 and the feature catalogue (Section 9) rather than enumerated per-class — the classes have 20–60 methods each.

---

## 2. Data structures

### `CanvasData` (the whole `.canvas` file)

```typescript
{
  nodes: CanvasNode[];
  edges: CanvasEdge[];
  // ...any unknown top-level keys are PRESERVED on round-trip
}
```
`getData()` spreads the last-parsed object and overwrites only `nodes`/`edges` (`app.pretty.js:188838`), so foreign top-level keys survive.

### `CanvasNode` (common base, from `S5.getData` `191892`)

```typescript
{
  ...unknownData;          // unrecognized keys captured + re-emitted (191917 / 191901)
  id: string;              // 16 lowercase hex chars (cc(16))
  x: number; y: number;    // integer-rounded (191920)
  width: number; height: number;  // integer-rounded; default 100×100
  color?: string;          // omitted when falsy. "1".."6" preset index OR "#rrggbb" OR "r,g,b"
  type: "text"|"file"|"link"|"group";  // appended by subclass — appears AFTER geometry on disk
  // + type-specific fields below
}
```
- `type:"text"` → `text: string` (markdown source; always written).
- `type:"file"` → `file: string` (vault-relative path), `subpath?: string` (`#heading`/`#^block`/`#page=N`; omitted when empty).
- `type:"link"` → `url: string`.
- `type:"group"` → `label?` (if truthy), `background?` (image path), `backgroundStyle?: "ratio"|"repeat"` (omitted when default `"cover"`).

### `CanvasEdge` (`G5.getData` `194111`)

```typescript
{
  ...unknownData;          // preserved (194119 / 194143)
  id: string;              // 16 hex
  fromNode: string; fromSide: "top"|"right"|"bottom"|"left";
  toNode: string;   toSide: "top"|"right"|"bottom"|"left";
  fromEnd?: "none"|"arrow";  // omitted when default "none"
  toEnd?:   "none"|"arrow";  // omitted when default "arrow"
  color?: string;  label?: string;  // omitted when falsy
}
```
Field-emission order is fixed: `…unknownData, id, fromNode, fromSide, toNode, toSide, [fromEnd], [toEnd], [color], [label]`.

### Ephemeral state (NOT in the `.canvas` file)

- **Viewport** `{x, y, zoom}` — `zoom` is **log2(scale)**. Persisted in workspace leaf state (`r8.getState`/`canvas.getState` `189810/189819`), travels with `workspace.json`, never the document.
- **Read-only flag** — per-file machine-local store keyed `"canvas-"+path` in `app` localStorage (`n8` `195014`); `saveLocalData` writes `{readonly:true}` or `null` (`195642`).

---

## 3. On-disk contracts

**This section is load-bearing for Corbomite compatibility.** Corbomite's `libs/canvas/CanvasDocument` currently **fails three of these** (see Section 11).

1. **Round-trip unknown fields.** Nodes, edges, AND top-level keys all preserve unrecognized data (`unknownData` rest-spread). A canvas written by a plugin/newer-Obsidian must survive a Corbomite save unchanged. *Corbomite currently drops them* (`CanvasTypes` structs model only known fields).
2. **Omit defaults, don't write them.** `color`/`label`/`subpath` omitted when empty; `fromEnd` omitted when `"none"`; `toEnd` omitted when `"arrow"`; `backgroundStyle` omitted when `"cover"`. Writing defaults is *valid JSON* but produces noisy VCS diffs against Obsidian-written files.
3. **Integer geometry.** `x/y/width/height` are rounded to integers on every `setData`.
4. **`file` is a vault-relative path string**, not a UID/link.
5. **Missing edge side self-heals.** On import, an absent `fromSide`/`toSide` is computed by `V5` (angular sector) and **written back** (`188950`), so the next save bakes a concrete side in. Corbomite must do the same or it will keep re-emitting side-less edges that Obsidian then rewrites — a diff ping-pong.
6. **Serializer is bespoke.** `dc()` (`34743`) is a tab-indent pretty-printer that collapses all-primitive arrays/objects onto one line. The output is standard JSON (safe to parse with any parser) but is **not** byte-identical to `JSON.stringify(x,null,"\t")`. Byte-for-byte parity (to avoid churn) would require reimplementing `fc()` (`34747`); functional parity does not.
7. **id format** — 16 lowercase hex chars, `[0-9a-f]{16}`, no collision check beyond map keys.

**The canvas is treated as a plain text file on disk** — the view extends `TextFileView` and rides the standard debounced-save / external-change lifecycle (Section 8).

---

## 4. Events emitted

Canvas fires **workspace events** (for plugin extension), not an `Events`-derived emitter of its own:

| Event | Fired at | Payload (inferred) |
|---|---|---|
| `canvas:node-menu` | `192188` | `(menu, node)` — extend a node's context menu |
| `canvas:edge-menu` | `194416` | `(menu, edge)` |
| `canvas:selection-menu` | `190729` | `(menu, canvas)` — multi-select menu |
| `canvas:node-connection-drop-menu` | `192516` | `(menu, …)` — menu shown when an edge is dropped on empty space |
| `file-menu` | `193199` | with source tag `"canvas-menu"` (file nodes) |
| `files-menu` | `190737` | when >1 file node selected |

It also calls `app.metadataCache.queueFileForLinkResolution(file)` after indexing (`194817`) — the hook that injects canvas links into the global link graph.

---

## 5. Events consumed

- **Vault `create`/`modify`/`rename`/`delete`** — the index `e8` watches these (`194646`) to re-process canvas files; the plugin's `requestProcessRename` (`195048`) rewrites embedded `file`/`background` paths when a referenced file is renamed.
- **`metadataCache` rename of headings/blocks** — via the `linkUpdaters["canvas"]` contract (`renameSubpath` `194926`) to fix `subpath`s on file nodes.
- **`TextFileView.onModify`** — external change to the open `.canvas` triggers re-import (Section 8).

---

## 6. Commands registered

Registered in plugin `init` (`195160–195242`) via `registerGlobalCommand`:

| id | Name | Effect |
|---|---|---|
| `canvas:new-file` | Create new canvas | New `.canvas` in active folder, open with rename eState; also a ribbon item |
| `canvas:export-as-image` | Export as image | Desktop-only; `canvas.generateHDImage()` (Section 9.7) |
| `canvas:jump-to-group` | Jump to group | FuzzySuggest of group nodes → `zoomToBbox` |
| `canvas:convert-to-file` | Convert to file | Enabled when exactly one text node selected → `convertToFile()` |

No zoom/lock/undo *commands* — those are on-canvas controls/menu toggles only.

---

## 7. Registries owned / joined

Canvas does not own a registry; it **registers into three app registries** (`195303–195340`):

1. `metadataCache.linkUpdaters["canvas"] = t8` — supplies `iterateReferences`, `iterateReferencesForFile`, `applyUpdates`, `renameSubpath`. This is how canvas participates in backlinks/outgoing-links/unresolved-links and link-rewrite-on-rename.
2. `embedRegistry.registerExtension("canvas", …)` — lets a `.canvas` be embedded inside a note (renders the static minimap thumbnail, Section 9.6).
3. `viewRegistry` via `registerViewType` (`195157`) — `.canvas` → canvas view.

---

## 8. Invariants

Corbomite must uphold these for compatibility:

1. **`.canvas` contains only `nodes`+`edges` (+preserved unknown keys).** No viewport, no UI state. Pan/zoom/readonly live out-of-band.
2. **Every edge resolves to a concrete `fromSide`/`toSide` once loaded** (auto-picked + written back if absent). The four sides are the *only* anchor positions — edges attach at the **midpoint of a node face**, never an arbitrary boundary point.
3. **Default edge = tail `none` + head `arrow`** pointing at the target.
4. **Group membership is purely geometric, recomputed at grab time** (`getContainingNodes` = full-containment test). There is no stored parent pointer; dragging a group moves whatever its rectangle currently encloses.
5. **Groups sit behind everything** (`zIndex = -width*height`, `193947`) — larger groups further back.
6. **A text card is a full markdown editor** (`MarkdownEditor`/`MarkdownPreviewView`), not a text widget. Frontmatter in a card renders literally (a card has no `TFile` to parse YAML against).
7. **A file node is a full transclusion** dispatched through the same `embedRegistry.getEmbedCreator` path as inline `![[…]]` embeds, depth-capped at ≤5.
8. **Content is virtualized by zoom**, not viewport-by-default: below a `zoomBreakpoint`, node content is torn out of the DOM and replaced by a lightweight placeholder (`192577`). (The controller additionally viewport-virtualizes attachment to ≤10 new nodes/frame, `189758`.)
9. **Save is debounced ~2000ms** (`TextFileView`), force-saved on unload; external modify re-imports by reconciling node/edge **ids** (no 3-way merge — `isPlaintext=false`), so unsaved local edits can be lost on external change.
10. **Two distinct "auto-side" algorithms** exist and must not be conflated: `V5` angular-sector for load-time missing-side resolution (sticky, no live re-swivel); `A3` nearest-face-within-snap-distance for live endpoint dragging.

---

## 9. Observable user features

The full feature catalogue with implementation detail — this is the "detailed feature list" deliverable.

### 9.1 Node types & content rendering
- **Text card** (`I5`): child is the full `MarkdownEditor` (`O5`/`CZ`) with dual CodeMirror-edit ↔ rendered-preview modes. Double-click → edit; blur → preview. Auto-fit height via double-click on bottom/right resizer (measures `scrollHeight`). Folds saved per `canvasFile#^nodeId`. **Gets for free** (because it's the real renderer): embedded images, transclusions `![[Note#h]]` (recursive, depth≤5), MathJax, Mermaid, callouts, tables, footnotes, interactive task checkboxes, syntax-highlighted code, clickable wikilinks/tags with hover-preview, and **community-plugin markdown post-processors** (Dataview etc.).
- **File node** (`P5`): full transclusion via `embedRegistry`. Markdown → live editable embed with inline title + properties editor; `subpath` narrows to a heading/block/`#page=N`. Images/video/audio → native `media-embed`, auto-setting `aspectRatio` from natural dimensions. PDF → `pdfViewer` with page toolbar. `.base` → bases embed. Missing file → "create/swap/remove" placeholder that auto-resolves on vault create/rename.
- **Link node** (`F5`): Electron `<webview>` (desktop, dedicated session partition, `allowpopups`) or sandboxed `<iframe class="canvas-link">` (web/mobile). URL transform `Az` (e.g. YouTube embed rewrite). `alwaysKeepLoaded` (never virtualized). Menu: change URL / open in browser / copy URL / reload.
- **Group node** (`R5`): contenteditable label with auto-contrast foreground (luminance test); optional background image with `cover`/`ratio`/`repeat` styles.

### 9.2 Node chrome
8 resize handles (4 edges + 4 corners) + 4 connection points (on the edge handles), hosted on a shared `M5` interaction-layer overlay re-targeted onto the active node. Edge handles bottom/right have dblclick-auto-fit. Node label visibility = `never`/`hover`/always. Color = 6 theme presets (`mod-canvas-color-1..6`) or custom hex (`<input type=color>` → inline `--canvas-color`).

### 9.3 Edges
Single **cubic bezier**: control points pushed perpendicular to each attach face by `clamp(distance/2, 70, 150)` world-units (`j5` `194042`). Anchor = face midpoint (`H5`); path inset 7px from face (`z5`), straight `L` segment fills the gap when no arrowhead, else the 13×10.4px triangle arrow (`polygon 0,0 6.5,10.4 -6.5,10.4`) is placed at the face rotated per side. Dual SVG path: visible 2px + invisible 24px hit-path. Label `K5` at bezier `t=0.5`. Drag an endpoint to reconnect (nearest-face snap `A3`); drop on empty space deletes the edge. Direction submenu: none/unidirectional/bidirectional.

### 9.4 Creation flows
Card-menu drag (3 draggable buttons: text/note/media), double-click empty → text, paste (image→file, canvas-JSON→clone+reID, URL→link, text→text), drag-drop (file/files/folder→grid of file nodes, URL→link), Mod+marquee (behavior per `defaultModDragBehavior`), context-menu create, edge-drag-to-empty → create node at drop, Alt/Ctrl-drag → duplicate. Defaults: text node 250×60, file node 400×400, embeddable URL 640×360.

### 9.5 Selection / move / snapping
Single Set of selected nodes+edges. Marquee on empty drag (groups require full containment); shift = additive/XOR. Move with shift-axis-lock and edge-auto-pan (60Hz when cursor nears wrapper edge). **Two snap modes** (both default ON, persisted via `saveOptions`): `snapToGrid` (zoom-dependent spacing 20/40/80/160) and `snapToObjects` (aligns corners + centers of other nodes within 15px/scale, drawing accent alignment guides in a `.canvas-snaps` SVG). No-snap modifier: Ctrl(mac)/Alt. Arrow-key nudge = gridSpacing (×5 with shift). Resize reuses the same align logic + aspect-lock on shift.

### 9.6 Viewport
World transform applied **inline** to `.canvas`: `translate(w/2,h/2) scale(2^zoom) translate(-x,-y)`. Zoom clamped log2 `[-4,1]` → scale `[0.0625, 2]`. Eased rAF loop (`1 - 0.984^dt`). Pan = middle/right-drag, space-drag, wheel (axis-swap on shift); zoom = Ctrl/Space+wheel or trackpad pinch, about cursor. Zoom-to-fit (Shift+1), zoom-to-selection (Shift+2), smart-zoom. On-canvas control cluster (gear/lock, zoom ±/reset/fit, undo/redo, help). **Minimap is NOT a live navigator** — `canvas-minimap` is a static SVG thumbnail used only for embedding a canvas inside a note.

### 9.7 Export as image
Desktop-only (`generateHDImage`). Modal: frame (whole canvas / current viewport), resolution slider (log2, capped at a 16284px raster limit), show-logo toggle (Obsidian watermark, default ON), privacy-mode (garble text). **PNG only — no SVG, no transparent background.** Viewport mode = single Electron `webContents.capturePage`; whole-canvas = tiled capture loop compositing into an offscreen `<canvas>` → `toBlob` → write. (Qt substitute: `QGraphicsScene::render` to `QImage`/`QSvgGenerator` — strictly easier; the tiling dance is an Electron limitation we don't have.)

### 9.8 Read-only / lock
Per-file local flag; adds `mod-readonly`, gates all editing/creation/delete/drag and label focus.

### 9.9 Link participation
Canvas text-node links + file/group embeds are indexed and fed to the global metadata graph (backlinks, unresolved links) and rewritten on rename (Section 7).

---

## 10. Extension surfaces exposed

- Workspace events `canvas:{node,edge,selection,node-connection-drop}-menu` + `file-menu`/`files-menu` (Section 4) — plugins extend canvas menus.
- `embedRegistry` extension for `.canvas` — plugins/embeds can render canvases.
- File/link/group nodes route through the standard `embedRegistry` embed creators — any embed type a plugin registers (e.g. custom code-block/file types) appears inside file nodes automatically.
- Markdown post-processors run inside text cards (cards are real `MarkdownPreviewView`s).

---

## 11. Corbomite mapping & migration strategy

### 11.1 Concept mapping (current state)

Corbomite's canvas (`libs/canvas/`, `src/canvas/`) is a competent QGraphicsView clone that covers the *mechanics* but diverges from Obsidian on fidelity and the disk contract.

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `.canvas` JSON r/w | `CanvasDocument` (`libs/canvas/.../CanvasDocument.cpp`) | **Partial** | **Drops unknown fields**; **writes defaults** (e.g. `toEnd:"arrow"`); does **not** self-heal/persist missing edge sides. P0 for interop. |
| 16-hex id | `CanvasDocument::generateId` | OK | Matches format. |
| Controller `y5` | `CanvasScene` (`QGraphicsScene`) | OK-ish | Hand-rolled item maps + tools. |
| Node base `S5`/`x5` | `TextCardItem`/`FileCardItem`/`GroupItem` (`QGraphicsObject`) | Partial | No shared base; no zoom-virtualization; no interaction-layer split. |
| Text card = full markdown editor | `TextCardItem` + `MarkdownRenderEngine` (regex/Markoff) | **Weak** | Renders a *snapshot* doc; **no transclusion, math, mermaid, callouts, interactive checkboxes, hover-preview, plugin post-processors, in-place CodeMirror edit**. Strips frontmatter (Obsidian does NOT). |
| File node = full transclusion | `FileCardItem` (renders resolved file via engine) | **Weak** | No `embedRegistry`-style dispatch; no native image/PDF/video; no subpath narrowing UI; no properties editor. |
| Link node (webview/iframe) | **Missing** | **None** | `NodeType::Link` modelled but no UI item. |
| Edge bezier | `EdgeItem` (`clamp(dist*0.4, 80)`) | Close | Curvature constant differs (Obsidian `clamp(dist/2, 70, 150)`); anchors at face midpoint = compatible. |
| Auto edge side | **Missing** | **None** | Sides are taken from JSON only; absent sides aren't computed → diff ping-pong with Obsidian. |
| `snapToGrid`/`snapToObjects` + guides | **Missing** | **None** | Grid drawn but not snapped; no object alignment guides. |
| Group geometric membership | `GroupItem::containedItems` (center-in-bounds) | Partial | Obsidian uses *full containment*; Corbomite uses center test. |
| Link index → metadataCache | **Missing** | **None** | Canvas links/embeds don't feed backlinks/unresolved-links; no rename rewrite. Wire to existing `MetadataCache`/`SQLiteIndex`. |
| Viewport persistence | **Missing** | **None** | No pan/zoom save (Obsidian: workspace leaf state). |
| Read-only mode | **Missing** | **None** | — |
| Export image | `CanvasScene::renderToImage/Svg` + modal (Cluster R) | **Ahead** | Corbomite has PNG **and** SVG + transparent-bg; Obsidian PNG-only. Keep ours. |
| Minimap | **Missing** | N/A | Obsidian has none live either — net-new if wanted. |
| Commands | partial (`new-file`, export) | Partial | `jump-to-group`, `convert-to-file` absent. |

**Priority interop fixes (independent of any rebase), highest first:** (1) unknown-field passthrough on node/edge/top-level; (2) omit-defaults on write; (3) compute+persist missing edge sides (`V5`); (4) integer geometry rounding. These four are the difference between "lossless against an Obsidian/kepano vault" and "silently corrupts canvases."

### 11.2 Graffodil rebase — library vs consumer split

`/home/clinton/dev/Graffodil` (`Graffodil::Core`, v0.1.0) is a Qt6 QGraphicsView graph-scene framework purpose-built to absorb exactly this kind of editor (its design doc names Canvas as the first intended consumer). Mapping Obsidian's feature set against Graffodil's API, here is what belongs **in the library** vs **in Corbomite `libs/canvas/`**.

**Goes into / is already provided by Graffodil::Core (generic graph-editor substrate):**
- **Scene/registry + tool dispatch** → `GraphScene` (replaces `CanvasScene`'s hand-rolled item maps).
- **Pan/zoom** → `PanZoomTool` (Graffodil applies the world transform; matches Obsidian's single-transform model). *Library gap to confirm:* Obsidian's eased rAF camera animation + zoom-to-cursor/pinch math — verify `PanZoomTool` eases and zooms about cursor; if not, that's a Graffodil enhancement, not Corbomite code.
- **Selection + marquee + multi-move + delete + duplicate** → `SelectMoveTool` (signals `nodeMoved`/`deleteRequested`/`dragBegan`/`dragEnded`).
- **Edge creation + endpoint reconnect** → `CreateEdgeTool` (anchor snapping, preview, `edgeRequested` signal).
- **Bezier edge rendering** → `BezierPathStrategy` + `TriangleTerminus`/`NoTerminus`. *Library work:* Graffodil's anchor model is general 2D anchors; Obsidian uses **exactly 4 face-midpoint sides**. Constrain via `compassAnchors(rect)` (Graffodil already ships `AnchorIds::{Top,Right,Bottom,Left}`). Reproduce the curvature `clamp(dist/2,70,150)`, 7px inset, and arrow geometry inside a Corbomite `EdgePathStrategy` subclass *or* upstream as a "CanvasBezier" preset.
- **Group backdrops** → `GroupStyle` + `GroupItem` (decorative, tracks members, never eats clicks — matches Obsidian's pointer-events:none group).
- **Snap-to-grid / snap-to-objects with alignment guides** → implement `IAlignmentStrategy` (Graffodil's hook). The *algorithm* (corners+centers, 15px tol, guide lines) is Corbomite's to write, but it plugs into Graffodil's `SelectMoveTool::setAlignmentStrategy` — no Graffodil change needed.
- **Minimap** → `GraphMinimap` (Graffodil ships a draggable-viewport minimap — *better* than Obsidian's static thumbnail, available for free if we want it).
- **Rich-node paint caching / virtualization** → `IRenderableCache` mixin (Graffodil composites cached pixmaps in batch mode). This is the hook for Obsidian's zoom-breakpoint placeholder swap.

**Stays in Corbomite `libs/canvas/` (Obsidian-specific / app-coupled):**
- **`.canvas` JSON model + serializer** (`CanvasDocument`, `CanvasTypes`) — Graffodil deliberately owns no serialization. All of Section 3's disk contract lives here.
- **Node visuals & content** — `TextCardItem`/`FileCardItem`/`GroupItem`/(new)`LinkCardItem` implement `IGraphNode` and paint themselves. The hard part — rendering full markdown / transclusions / media / webview inside a card — is 100% Corbomite. Graffodil only positions and wires them.
- **Markdown/embed rendering pipeline** — binding to Markoff / a `QWebEngineView`-per-card / embed dispatch. (See 11.3 — this is the dominant cost and is unaffected by the rebase.)
- **Undo/redo** — `CanvasCommands` (`QUndoStack`) hooks Graffodil tool *signals* (which emit intent, never mutate). Stays Corbomite.
- **The link index → `MetadataCache` wiring**, rename rewriting, commands (`jump-to-group`, `convert-to-file`), read-only mode, export modal — all Corbomite/app surface.
- **Resize handles** — Obsidian's 8 handles + 4 connection points. Graffodil's anchors cover connection points; the resize-handle visuals/interaction are node-level Corbomite paint (per the Graffodil integration-feedback doc, resize handles were explicitly left to consumers).

**Net:** the rebase removes the scene-registry, tool state machine, pan/zoom, bezier math, group backdrop, and minimap from `libs/canvas/` (~1.3–1.8k LOC) and **keeps** the data model, node content rendering, undo commands, and app wiring. It does **not** simplify the genuinely hard part (card content fidelity). Treat Graffodil as de-risking the *plumbing*, not the *rendering*. Caveat from the scouting work: Graffodil is v0.1.0 with a recent Side→Anchor refactor — confirm API stability before committing, and the existing scouting doc (`docs/superpowers/plans/2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md`) sequences Canvas first.

### 11.3 What Obsidian gets "for free" from HTML/CSS — and the Qt tradeoffs

Obsidian's canvas is cheap to build because Chromium does the heavy lifting. A QGraphicsView reimplementation pays for each of these explicitly. The honest tradeoff ledger:

| Obsidian gets free from DOM/CSS | Mechanism (`app.css`) | Qt/QGraphicsView reality | Tradeoff |
|---|---|---|---|
| **Full markdown in every card** (transclusion, math, mermaid, callouts, code highlighting, interactive checkboxes, hover-preview, plugin post-processors) | A card *is* a `MarkdownPreviewView` DOM subtree | Either embed a `QWebEngineView`/Markoff widget **per card** (heavy: N web contexts) or re-implement the whole render pipeline inside `QGraphicsItem::paint` | **The dominant cost.** This is *the* reason a from-scratch Qt canvas looks impoverished next to Obsidian. Decide deliberately: widget-per-card (fidelity, RAM/perf cost, mandatory virtualization) vs. static rendered snapshot (what Corbomite does today — no interactivity, no live embeds). |
| **GPU-composited pan/zoom** | One inline `transform` on `.canvas`; Chromium auto-layers, no `will-change` needed | `QGraphicsView` transforms are CPU-rasterized unless you opt into `QOpenGLWidget` viewport; smooth zoom of many rich items is not free | Use a GL viewport + cache `QGraphicsItem::ItemCoordinateCache`; budget for it. |
| **Per-node layout/paint isolation** | `contain: strict` on every `.canvas-node-container` | No direct equivalent; `QGraphicsItem` caching + `setFlag(ItemClipsChildrenToShape)` approximate it | Manual; correctness-sensitive. |
| **Chrome that stays screen-constant under zoom** | `--zoom-multiplier` CSS var consumed by `calc()` in label/resizer/edge-width rules | Must counter-scale handles/labels/edge pens per-frame in code (`QGraphicsItem::ItemIgnoresTransformations` for some, manual for stroke widths) | Per-frame work; `ItemIgnoresTransformations` helps for handles/labels. |
| **Fat invisible hit-target on thin edges** | `.canvas-interaction-path { pointer-events:stroke; stroke-width:24px }` over a 2px visible path | `QGraphicsPathItem::shape()` widened via `QPainterPathStroker` | Easy to replicate — already partially done. |
| **Declarative input routing** | layered `pointer-events:none`→`initial`; `.canvas-node-content-blocker` overlay swallows clicks unless focused | Explicit event filtering / `acceptedMouseButtons` / a transparent blocker item per node | More code, but the blocker pattern ports directly. |
| **In-place editing** | `contenteditable` labels + CodeMirror cards | `QGraphicsProxyWidget` hosting `QLineEdit`/editor (Corbomite already does this for labels/text) | Works; proxy widgets are clunky at scale and don't zoom crisply. |
| **One-token theming** (6 presets + every opacity from one `--canvas-color` rgb triplet via `rgb()`/`rgba()`) | `.mod-canvas-color-N` → `--canvas-color` | Map preset index → `QColor`, derive alphas in code (Corbomite's `colorFromCanvasColor` already does the 6 presets) | Minor; already handled. |
| **Native media/PDF/webview embeds** | `<img>/<video>/<iframe>/<webview>` inside node content | `QMovie`/`QMediaPlayer`/`QPdfView`/`QWebEngineView` per node | Each is a real widget; heavy; mandates virtualization. Link/web nodes specifically need `QWebEngineView` (a hard dep we may not want). |
| **Animated camera** | `transition: transform 500ms cubic-bezier(…)` | `QVariantAnimation` driving the view transform | Easy. |
| **Export** | Electron `capturePage` (PNG-only, tiled, watermark) | `QGraphicsScene::render()` → `QImage`/`QSvgGenerator` | **Qt wins** — Corbomite already does PNG+SVG+transparent-bg in one pass; no tiling. |

**Strategic implication:** the QGraphicsView substrate (whether hand-rolled or Graffodil) is *fine* for the whiteboard mechanics — positioning, edges, selection, snapping, export are all comfortably reproducible (several already exceed Obsidian, e.g. SVG export and a real draggable minimap). The fidelity cliff is entirely **card content**. The choice that defines the project is per-card render strategy: (a) `QWebEngineView`/full live embed per card → near-parity, heavyweight, requires Obsidian-style zoom-breakpoint virtualization (Section 8 invariant 8) and likely a Chromium dependency Corbomite may otherwise avoid; or (b) Markoff-rendered static documents → lightweight, native, but permanently behind on transclusions/math/mermaid/interactivity. This decision should be made explicitly (brainstorm) before any Graffodil rebase, because it dwarfs the plumbing question.

---

## 12. Markoff gap confirmations / discoveries

Canvas touches the rendering path via text/file node content:

- **Confirmed:** Obsidian renders card content through the *same* `MarkdownPreviewView` + post-processor pipeline as the main editor — there is no canvas-specific renderer. Any Markoff capability gap (transclusion, math, mermaid, callouts, interactive checkboxes, hover-preview) is therefore inherited 1:1 by cards. Canvas does not add rendering features; it *hosts* the editor.
- **Discovery:** cards use a `linktext = canvasFile#^nodeId` convention to persist per-card fold state — a Markoff/editor integration point if live-editing cards is ever pursued.
- **Discovery:** file-node embeds use the embed depth-guard (≤5) — relevant if Corbomite implements transclusion to avoid recursion blowups.

---

## 13. Open questions

1. **Viewport off-screen culling vs zoom-breakpoint:** confirmed zoom-breakpoint virtualization (content mount/unmount) and ≤10-attach/frame throttle; whether nodes panned fully off-screen are *detached* (vs just not painted) wasn't pinned in the node slice — controller `virtualize` (`189758`) suggests yes. Confirm before designing Corbomite's virtualization.
2. **`Az` URL-transform rules** (YouTube etc.) for link nodes — not traced; needed only if we build link nodes.
3. **`backgroundStyle` full enum** — `"cover"` (default, omitted) confirmed in code; `"ratio"`/`"repeat"` confirmed via the menu (`187747–187769`). No fourth value seen.
4. **Byte-exact serializer parity** — do we care? Only if minimizing diffs against Obsidian-written canvases in shared vaults. Decision needed; reimplementing `fc()` is the cost.
5. **Color normalization `H3`/`Vk`/`Fk`** internals (rgb-triplet ↔ hex) not read; replicate behavior from the CSS triplet convention.

---

## 14. Recommended Pass 3 synthesis input

- **Disk contract (Section 3) → GAP-ANALYSIS P2.18** ("Canvas schema compat") is now fully specified: the four interop fixes in 11.1 are the concrete deliverables. Promote P2.18 from "validate + round-trip test" to "implement unknown-field passthrough, default-omission, side self-heal, integer rounding; add an Obsidian-vault round-trip golden test."
- **Link index (Section 7) → Cluster M** should include wiring canvas references into `MetadataCache`/`SQLiteIndex` (backlinks/unresolved) + rename rewriting — currently entirely absent.
- **Graffodil split (11.2) → Cluster P scouting doc** can be upgraded with this concrete library/consumer boundary.
- **Card-render strategy decision (11.3) is a prerequisite brainstorm** — flag it as blocking any canvas feature push beyond interop fixes.

---

## 15. Cross-domain references

- `embedRegistry` / embed creators — owned by **rendering** domain (`domains/rendering.md`); canvas file nodes consume it.
- `metadataCache.linkUpdaters` / `queueFileForLinkResolution` — **metadata** domain (`domains/metadata.md`).
- `TextFileView` / `ItemView` / view registry — **views** domain (`domains/views.md`); canvas view extends `TextFileView`.
- `MarkdownEditor` / `MarkdownPreviewView` — **editor** / **editor-markdown** domains.
- `dragManager.handleDrop` — **ui-bundle** domain.
- `.canvas` schema cross-listed in `VAULT-FORMAT.md §Canvas`.
- Export-as-image menu preamble: `../addenda/2026-04-19-canvas-export-as-image.md` (this doc supersedes its "deeper behaviour out of scope" caveat).
