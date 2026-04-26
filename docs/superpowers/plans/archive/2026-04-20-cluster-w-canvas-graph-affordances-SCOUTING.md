# Cluster W — Canvas & Graph affordances (SCOUTING)

**Type:** Scouting doc (not yet dispatchable). Expand to a full plan once Cluster M (internal-plugin feature audits) lands or once the user confirms scope.

**Motivation.** A 2026-04-20 dead-code audit confirmed that `libs/canvas/`, `libs/forcegraph/`, `src/canvas/`, and `src/graph/` (plus the embed bridges in `libs/jkqtmathtext/` and `libs/mmdr/`) carry a substantial inventory of features that work internally but have no UI affordance — node types defined but not creatable, tools implemented but not switchable, force-layout parameters tunable in code only, embed renderers registered but never dispatched. Same pattern as the editor stack ([Cluster V](2026-04-20-cluster-v-editor-workspace-ui-surfacing-SCOUTING.md)) but in the canvas/graph subsystem.

This cluster is intentionally split from Cluster V because canvas + graph need design work, not just menu wiring. Toolbars, node-type pickers, and physics-tuning UI are non-trivial UX decisions.

---

## Audit references

- 2026-04-20 audit findings (canvas/graph/embeds agent transcript) — concrete `file:line` punch list reproduced below.
- [`../../obsidian-audit/domains/canvas-bundle.md`](../../obsidian-audit/domains/canvas-bundle.md) — Obsidian canvas spec; node types (text/file/group/link), edge types, snap-to-grid, multi-select, resize.
- [`../../obsidian-audit/domains/graph-bundle.md`](../../obsidian-audit/domains/graph-bundle.md) — Obsidian graph spec; force-layout sliders, group-by-tag, color-by-folder, depth slider, filters.
- [`../../obsidian-audit/domains/rendering-bundle.md`](../../obsidian-audit/domains/rendering-bundle.md) §Embeds — embed-type detection + dispatch (PDF / audio / video / images).
- [`../../cluster-retros/cluster-j.md`](../../cluster-retros/cluster-j.md) — embed registry + post-processor + code-block registry shipped here.

---

## Scope (rough phasing — revisit on plan expansion)

### Phase 1 — Canvas node-creation completeness

`libs/canvas/src/CanvasScene.cpp:363-364` — `NodeType::Link` enum exists but `onNodeAdded` has an empty `case`. Link nodes load from JSON, vanish on reload. `populateFromDocument` only handles Text / File / Group.

- Implement Link-node rendering (graphics item + JSON round-trip).
- `CanvasScene.cpp:871-896` context menu offers **New Text Card** + **New Group** but no **New File** or **New Link**. Add both. **New File** opens a file picker (or accepts drop); **New Link** opens a wikilink picker (reuse `WikiLinkSuggest` from Cluster H).
- Drag-and-drop from File Explorer onto canvas should create a File node at drop position. Hook `CanvasView::dropEvent` (currently rejects MIME `application/x-corbomite-tfile`).

### Phase 2 — Canvas tool palette

`libs/canvas/include/canvas/CanvasTool.h:40-103` defines `SelectMoveTool`, `CreateCardTool`, `CreateEdgeTool`. `CanvasScene::setActiveTool()` exists but no UI ever calls it — only `SelectMoveTool` is ever active.

- Add a **canvas tool palette** as a left-edge toolbar in `CanvasFileView` (Select / Card / Edge / pan-zoom). Bind `setActiveTool` on click.
- Keyboard shortcuts: V (select), T (text card), E (edge), Space (pan).
- Tool cursor changes per active tool (`CanvasView::updateCursor()`).

### Phase 3 — Canvas resize, snap-to-grid, multi-select feedback

- `SelectMoveTool` supports `DragMode::Resize` internally (`CanvasTool.h:60`) but no resize handles are drawn on selected nodes. Add 8 resize handles per node when single-selected (corner + edge midpoints).
- `CanvasView.cpp:16` defines `kGridSize = 20.0` and the grid is rendered, but nothing snaps to it. Add **View > Snap to grid** toggle (canvas-scoped setting persisted to `.canvas` JSON unknown-keys); when on, drag/resize/create snaps positions to `kGridSize` multiples.
- Rubber-band selection works (`CanvasTool.cpp`) but no visual highlight on selected nodes. Add a 2 px highlight outline + selection-bounding-rect when ≥2 selected. `selectionChanged` signal currently has no consumer; route to the canvas hamburger to enable group/colour/link/delete actions.

### Phase 4 — Canvas group operations

Building on Phase 3 multi-select feedback:

- **Group selected** (Ctrl+G) — wrap selected nodes in a `NodeType::Group` with auto-fit bounding rect.
- **Ungroup** (Ctrl+Shift+G).
- **Set color** submenu (6 preset colors + custom — match Obsidian).
- **Align horizontally / vertically / distribute** — bonus, defer if scope creeps.
- All routed through canvas hamburger (Cluster R menu substrate) + canvas context menu.

### Phase 5 — Force-graph physics + filters

`libs/forcegraph/include/forcegraph/ForceLayoutEngine.h:26-30` — public methods `setCenterForce`, `setRepelForce`, `setLinkForce`, `setLinkDistance` exist with no UI; `pinNode` / `unpinNode` exist with no pin affordance.

- Add a **graph control panel** (collapsible right-edge or settings popover on the graph hamburger) with sliders for the four force parameters. Live-apply via `engine->setRepelForce(value)` etc.
- Persist settings per-vault (Obsidian stores in `.obsidian/graph.json`; round-trip with unknown-key preservation per Cluster S pattern).
- Pin/unpin: right-click a graph node → **Pin** / **Unpin** (toggle); pinned nodes shown with a different border.
- Filter UI (existing or new): tag filter, folder filter, depth slider for local graph (LocalGraphView already implements depth — surface it).

### Phase 6 — Graph signal wiring

`src/plugins/graph-view/GraphViewTab.h:8-11` — three signals emitted, never connected:

- `openNoteInNewTabRequested(relativePath)` → connect to `WorkspaceController::openFileInLeaf(LeafSpawn::NewTab)`.
- `revealInNavigationRequested(relativePath)` → connect to `FileExplorerPlugin::revealPath(path)` (add the slot if missing).
- `deleteNoteRequested(relativePath)` → route through `FileManager::promptForDeletion`.

Trigger via right-click on a graph node (currently node click does only `noteActivated`).

### Phase 7 — Embed registry consolidation + media stubs

`libs/readingview/src/EmbedRenderer.cpp:388-403` — factories registered for `.pdf`, `.mp3`, `.wav`, `.mp4`, `.webm` return bare text placeholders (e.g. `[PDF preview not yet available — document.pdf]`). Real renderers are not in scope here, but two consolidations are:

- **Mermaid double-path** (`ReadingView.cpp:127-134` registry vs `SectionLayout.cpp:903` direct): `SectionLayout` calls `MermaidRenderer::renderSvg` directly instead of dispatching through `codeBlockProcessorRegistry`. Same FFI from two paths. Route block dispatch through the registry; delete the direct call site. (Already listed in Cluster V Phase 4 — coordinate so it isn't done twice.)
- **PDF / audio / video real renderers**: out of scope, but capture as backlog. PDF: Poppler (`~/src/kde/src/poppler` already checked out per CLAUDE.md). Audio/video: `QMediaPlayer` from `QtMultimedia`. Decide whether these become internal renderers or wait for plugin-API expansion.

### Phase 8 — Audit pass

After Phases 1–7:

- Re-run dead-code grep over `libs/canvas/`, `libs/forcegraph/`, `src/canvas/`, `src/graph/`, `src/plugins/graph-view/`, `src/plugins/local-graph/`. Wire or delete the residue.

---

## Primitive inventory

**Reused:**

- `Workspace::splitLeaf` / Cluster R menu helpers / `WikiLinkSuggest`.
- `MermaidRenderer` (FFI bridge in `libs/mmdr/`) — already works.
- `JKQTMathText` — already works for inline math; block-mode lift is in Cluster V Phase 4.
- `SQLiteIndex` filter queries for graph tag/folder filters.
- `.canvas` / `.obsidian/graph.json` unknown-key preservation pattern from Cluster S.

**New primitives:**

- Canvas tool palette widget (small left toolbar in `CanvasFileView`).
- Resize handles graphics item (8 squares around bounding rect).
- Snap-to-grid math + persistence flag.
- Group / ungroup operations on `CanvasDocument`.
- Color-tint submenu helper.
- Graph control panel widget (sliders + persisted settings).
- Pin overlay graphics for graph nodes.

---

## Blockers / prerequisites

1. **Cluster M (internal-plugin feature audits — Graph + Canvas)** is the natural design-input source. Either land Cluster M first, or fold its scope into this cluster's planning step.
2. **Cluster P (Graffodil adoption)** is a parallel internal refactor that may replace `libs/forcegraph` and `libs/canvas` substrates entirely. If Graffodil is imminent, defer Phase 5 + Phase 3's resize handles until after the migration to avoid throw-away work. If Graffodil is months out, ship this cluster on the current substrate.
3. **Decision: PDF / audio / video real renderers in scope or out?** Phase 7 currently treats them as out-of-scope; revisit on plan expansion.

---

## Out of scope (deferred)

- **PDF / audio / video real embed renderers** — Phase 7 captures the placeholder problem; real implementations are backlog.
- **Drag-from-graph onto canvas / canvas-on-canvas embedding** — Obsidian doesn't ship these either.
- **Plugin-API embed registration** — `codeBlockProcessorRegistry()` is exposed publicly but no plugin consumes it. Defer to plugin-development milestone (same reasoning as Cluster V's deferred Tier-5 plugin API).

---

## Estimate (rough)

8–12 days once expanded. Most expensive phases: Phase 2 (tool palette UX design), Phase 3 (resize handles + snap math), Phase 5 (control panel + persistence). Phases 1, 4, 6, 7, 8 are mostly wiring.

---

## Expansion triggers

Expand to a full plan when:

1. Cluster M lands (or is folded in).
2. Cluster P (Graffodil) sequencing decision is made — defer Cluster W phases that touch the substrate, or proceed on hand-rolled.
3. PDF / audio / video scope decision is made.
4. The user confirms Cluster W is the next canvas/graph-side cluster.

Until then this scouting doc captures the audit's punch list.
