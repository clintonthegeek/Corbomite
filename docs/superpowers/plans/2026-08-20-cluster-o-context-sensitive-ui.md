# Cluster O — Context-sensitive menu bar, toolbar & sidebar

> **Created 2026-08-20**, user-directed, during Cluster M Phase M4 live-testing.
> Stub plan; needs a brainstorm + full plan expansion before dispatch. Not
> started — no code written yet.

## Problem

`MainWindow`'s menu bar, toolbar, and right-hand sidebar are today one
static, editor-oriented surface, always fully present regardless of which
kind of tab is focused. This was tolerable while Corbomite had essentially
one leaf kind (the markdown editor, in its three flavors). It stopped being
tolerable once Canvas, Bases, and the graph view became real, separately
focused document types with their own action vocabularies:

- **Canvas** has no exposed KActions at all for its own settings —
  snap-to-grid, snap-to-objects, grid visibility — despite the underlying
  toggles existing since Cluster M Phase M4.1 (`CanvasAlignmentStrategy::
  setSnapToGridEnabled`/`setSnapToObjectsEnabled`). Punch-list:
  `[ui-bundle][canvas][P2][cluster-o]`. (Grid *rendering* itself was a
  separate, standalone fix — not part of this cluster's scope; see
  decisions-archive 2026-08-20.)
- **Editor-mode toggles** (Live/Source/Reading) stay prominent even when a
  Canvas, Bases, or graph tab is focused — options for a document type that
  isn't open and can't be acted on.
- **Reading mode**, itself read-only, still shows editing affordances it
  can't act on (should hide/disable them, not just no-op them).
- **The right-hand sidebar** has no concept of a per-view-type panel (e.g. a
  graph-control panel that should only appear while a graph view is
  focused).
- **Bases** presumably has the same gap as Canvas — no evidence yet either
  way, needs auditing as part of scoping this cluster.

Net effect, in the user's own words during the Phase M4 live-test: **canvas
view is still rather unusable** as a document type in its current UI
context — the chrome around it doesn't know canvas exists.

## Goal

Menu bar, toolbar, and sidebar entries become dynamic per the currently
focused tab's document type, with context-sensitive options: a canvas
toolbar (grid/snap toggles, and whatever else Cluster M's later phases turn
up) appears only while a canvas tab is focused and disappears otherwise; a
graph-control sidebar panel appears only while a graph view is focused;
editor-mode (Live/Source/Reading) toggles and markdown-editing actions hide
or disable themselves when the focused tab isn't a markdown document, or is
read-only (Reading).

## Scope (tentative — needs a brainstorm pass, not committed)

- Audit every current `MainWindow` menu/toolbar/sidebar action for which
  document type(s) it's actually meaningful for (markdown-editor-only,
  canvas-only, bases-only, graph-only, universal).
- Design the activation mechanism: something keyed off the active
  `WorkspaceLeaf`'s content type, presumably reacting to the same
  active-leaf-changed signal `MainWindow::activeCanvasView()`/
  `activeBasesView()`-style helpers already key off (Cluster L's
  `activeLeafChanged` wiring is a likely existing hook to reuse rather than
  inventing a second one).
- Decide the mechanism for registering a document-type's action group:
  likely something the internal-plugin/leaf-type registration path can
  hang a "these KActions/toolbar entries/sidebar panel belong to leaf-type
  X" declaration off, rather than `MainWindow` hardcoding a big if/else
  ladder per type.
- Canvas's specific action set (at minimum): snap-to-grid toggle,
  snap-to-objects toggle, grid-visibility toggle (the grid itself now
  renders as of 2026-08-20, zoom-adaptive, `CanvasScene::drawBackground` —
  this cluster only needs to add the show/hide KAction, not build the grid).
- Graph view's control sidebar panel, Bases' own action set (needs
  auditing — unclear yet what if anything is currently exposed), Reading
  mode's disable-not-hide policy for editing actions.

## Out of scope (explicitly, pending the brainstorm)

- Building any one-off per-view menu/toolbar patch ahead of this cluster
  (e.g. a standalone canvas-snap-toggle action) — punch-listed items that
  are clearly this cluster's shape should wait for the mechanism rather
  than accreting more static, ungated actions that this cluster would then
  have to retrofit.

## Relationship to other clusters

- **Cluster M** (canvas authoring parity) is what surfaced this gap and
  continues to be blocked by it in practice — Canvas is functionally
  capable (M0-M4 landed) but its settings are inaccessible without this
  cluster, and the grid-visualization item is a direct dependency.
- **Cluster N** (rich clipboard) is unrelated, concurrent, isolated on
  `feature/rich-clipboard`.
- Likely touches the same `MainWindow` decomposition territory as
  [`../specs/2026-06-10-mainwindow-decomposition-design.md`](../specs/2026-06-10-mainwindow-decomposition-design.md)
  — read that spec before scoping, it may already have relevant seams.

## Next step

Brainstorm + audit session: full inventory of current menu/toolbar/sidebar
actions tagged by which document type(s) they apply to, survey of the
active-leaf-change signal(s) already available to hook into, and a design
decision on the action-group-registration mechanism, before writing a full
phased plan.
