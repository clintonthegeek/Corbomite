# Cluster C — Workspace serializer fidelity rebuild

> **Created 2026-04-26 from audit reset.** Stub plan; needs brainstorm + full plan expansion before dispatch. The Cluster Y migration onto KDDockWidgets shipped a working layout substrate but the `.obsidian/workspace.json` serializer is round-trip-lossy in 5+ orthogonal ways. This cluster rebuilds it.

## Goal

`.obsidian/workspace.json` round-trips byte-faithfully (within Obsidian's own tolerance) for: nested splits, per-tab-group active-tab selection, sidedocks, popout windows, floating tab-groups, ribbon icons, and named workspaces. Cross-tool vault sharing produces zero spurious diffs in `workspace.json`.

## Audit references

- [audit-2026-04-26/workspace.md](../../audit-2026-04-26/workspace.md) §"Layout JSON compatibility risks" — primary
- [audit-2026-04-26/workspace.md](../../audit-2026-04-26/workspace.md) §"Notable concerns / suspected bugs" — `m_tabGroupOf` lag, two parallel serializers, popout window leak
- Cluster Y retro at `cluster-retros/cluster-y.md` (γ-scope events deferred items)

## Scope (in scope)

1. Resolve the two parallel serializers (`Workspace::serialize` + `WorkspaceSerializer`) — pick one source of truth
2. Preserve nested splits in serialization (currently flattens to root + N flat tabs)
3. Per-group `currentTab` (currently emits single global `currentTab`)
4. Bridge KDDW → corbomite tab-group enumeration (close `m_tabGroupOf` lag during user drags)
5. `SessionManager::m_unknownRoot` round-trip — stop blindly preserving keys that diverge from live state
6. `undoCloseLeaf` preserve original parent + history + ephemeral state
7. Popout window lifecycle on X-close (currently leaks)
8. Sidedock-as-workspace-tree (currently sidebars live outside the tree in `CorbomiteMDI::Sidebar`)
9. Named-workspaces support (`.obsidian/workspaces.json`)

## Out of scope

- Workspace event emission completeness → punch list (or Cluster B if plugin-facing)
- `Workspace::activeLeafChanged` wiring (was legacy Cluster Z) → punch list
- Linked views API → defer (legacy Cluster Z scope; pull into a future cluster)

## Phases

TBD — brainstorm. Likely 3 phases: (1) consolidate serializer + nested splits, (2) sidedock-as-tree + named workspaces, (3) polish (popout lifecycle, undo state).

## Status

**Plan-needed** (stub).
