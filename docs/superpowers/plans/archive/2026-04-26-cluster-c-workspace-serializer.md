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

(Plan never expanded — fidelity items closed inline through the P1 punch-list sweep + 2026-04-26 serializer-consolidation work-unit. See per-item status below.)

## Status

**Closed 2026-04-27.** Drained inline; the two remaining items are feature substrate (not serializer fidelity) and have been reassigned to Cluster F.

### Disposition

| # | Item | Status |
|---|---|---|
| 1 | Consolidate `Workspace::serialize` ⇄ `WorkspaceSerializer` | Closed (P1 punch-list, [serializer-consolidation spec](../specs/2026-04-26-workspace-serializer-consolidation-design.md)) |
| 2 | Preserve nested splits in serialization | Closed (P1 punch-list — KDDW `LayoutSaver::serializeLayout()` JSON drives split topology) |
| 3 | Per-group `currentTab` (replace global active-leaf-index) | Closed (P1 punch-list — round-trips via `Core::Group::currentTabIndex()`) |
| 4 | Bridge KDDW → Corbomite tab-group enumeration (`m_tabGroupOf` lag) | Closed (P1 punch-list — primitives now read from `DockRegistry::groups()`; [kddw-public-enumeration addendum](../../obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md)) |
| 5 | `SessionManager::m_unknownRoot` `left`/`right` blind write-through | Closed (P1 punch-list — Option B chosen: `m_sidebarDirty` bit, drop dirty subtrees on save) |
| 6 | `undoCloseLeaf` preserve original parent + history + ephemeral state | Closed (P1 punch-list — `closeLeaf` captures sibling id, restore re-keys + sets state) |
| 7 | Popout window lifecycle on X-close | Closed (P1 punch-list — `m_windows.removeOne(window)` on close path) |
| 8 | Sidedock-as-workspace-tree | **Reassigned to Cluster F** (item #10) — substrate refactor, not serializer fidelity; pairs with the Workspaces internal plugin. |
| 9 | Named-workspaces `.obsidian/workspaces.json` | **Reassigned to Cluster F** (item #9) — owned by the Workspaces internal plugin. |
