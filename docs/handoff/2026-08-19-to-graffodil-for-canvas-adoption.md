# Handoff: Corbomite → Graffodil — Canvas adoption is starting (Cluster M)

**Date:** 2026-08-19. **From:** Corbomite (Cluster M, [plan](../superpowers/plans/2026-08-19-cluster-m-canvas-authoring-parity.md)).
**To:** Graffodil maintainers/sessions. **Reply expected:** yes — the two decision
items below gate Corbomite's Phase M0.

## Context

Corbomite is rebasing `libs/canvas/` (the `.canvas` whiteboard) onto
`Graffodil::Core`, per the boundary mapped in
`~/dev/Corbomite/docs/obsidian-audit/domains/canvas.md` §11.2 and your own
`docs/corbomite-integration-feedback.md`. This is the consumer migration your
ROADMAP sequences "after Phases 1–5 + 6d". Phases 1–5 (+6c, 6f) are complete;
**6d (CMake install/export) is the last open gate.**

A 2026-08-19 header check from our side found the load-bearing API already in
place — nothing blocking:

- `PanZoomTool::applyZoom(factor, anchorScenePos)` — zoom about cursor ✅
  (no eased/animated camera; Corbomite will drive `QVariantAnimation` itself).
- `compassAnchors(rect)` + `AnchorIds::{Top,Right,Bottom,Left}` — the exactly-4
  face-midpoint anchor model Obsidian's `.canvas` requires ✅.
- `CreateEdgeTool` — anchor snap + preview + `edgeRequested(source, anchorId,
  target, anchorId)` intent signal ✅.
- `GraphScene` subclass hooks (`mouseDoubleClickEventBackground`,
  `placeNodeAt`, sub-item click protocol) ✅.

## Decision item 1 — consumption mechanism (blocks M0)

Pick one; Corbomite is fine with either:

1. **Land 6d** (`docs/specs/cmake-install-export.md`) and Corbomite consumes an
   installed `Graffodil::Core` package, or
2. **Bless submodule consumption**: Corbomite adds Graffodil as a git submodule
   and `add_subdirectory`s it (no install/export needed). Per `~/dev/CLAUDE.md`
   (no cross-repo symlinks) this is the markoff-family precedent.

If (2): Corbomite's `origin` is GitHub since 2026-08-18 and its existing
submodule (`libs/markoff-family`) points at a **GitHub mirror** of a
Codeberg-primary repo. Graffodil has no GitHub mirror today — either create one
(`git@github.com:clintonthegeek/Graffodil.git`, mirror-push like Markoff's) or
confirm a Codeberg submodule URL is acceptable for Corbomite's CI (`.deb` build
on `v*` tags clones submodules anonymously — Codeberg HTTPS works if the repo is
public; Graffodil's visibility should be confirmed).

## Decision item 2 — edge-creation gesture (M3, not urgent)

`CreateEdgeTool` documents a **click-click** flow (click source → preview →
click target). Obsidian's canvas gesture is **press-drag-release** from a hover
connection point, and "drop on empty space" has semantics (cancel/create-menu).
Question: does `CreateEdgeTool` also complete on press-drag-release, and is
there a hook for the release-on-empty case? If not, is that an upstream
enhancement you'd take, or should Corbomite subclass the tool?

## Nice-to-have (no gate)

- `IAlignmentStrategy`: Corbomite will implement Obsidian's
  snap-to-grid/snap-to-objects (corners+centers, 15px/scale, guide lines) as a
  consumer strategy per your integration-feedback doc — if another consumer
  wants it, say so and we'll shape it upstream-cleanly from the start.
- Version tag: a `v0.2.x` tag (or 6d-landed `v0.3.0`) for Corbomite to pin
  against would be appreciated; we pin submodules by SHA regardless.

## What Corbomite does next

M0 waits on decision item 1. M1 (feature-frozen rebase of ~3.8k LOC
`libs/canvas` onto GraphScene/IGraphNode/GraphEdgeItem + a CanvasBezier
`EdgePathStrategy`) starts immediately after and needs no further Graffodil
input unless item 2 turns up gaps.
