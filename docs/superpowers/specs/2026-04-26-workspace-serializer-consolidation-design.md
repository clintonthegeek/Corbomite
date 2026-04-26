# Workspace serializer consolidation — design

**Date:** 2026-04-26
**Scope:** Punch-list P1 items #1, #2, #3 (`docs/punch-list.md` lines 34-36).
**Deferred:** P1 #4 (`m_unknownRoot` `left`/`right` write-through) — out of scope here, see "Deferred follow-ups" below.
**Out of scope:** P1 #5–#9 (popout leak, `m_tabGroupOf` lag, `undoCloseLeaf` parent/history/eState restore, `MenuEventEmitter::fileMenu` source discriminator, `addAction` order). Each remains an independent punch-list item.

---

## Problem statement

Today Corbomite has two parallel writers for `<vault>/.obsidian/workspace.json`:

| Writer | File | Live callers | What it walks |
|---|---|---|---|
| `Workspace::serialize / deserialize` | `libs/core/src/Workspace.cpp:649-852` | Production: `MainWindow::saveSessionState` (`MainWindow.cpp:816`) → `SessionManager::setWorkspaceLayout`; load: `MainWindow.cpp:2147-2158` | `Workspace::m_leaves` + `m_tabGroupOf` (flat) |
| `Corbomite::WorkspaceSerializer::toJson / fromJson` | `libs/core/src/WorkspaceSerializer.{h,cpp}` | **Tests only** (`tests/core/tst_workspace_serializer.cpp`) | `KDDockWidgets::DockRegistry::dockwidgets()` (also flat — `walkKddwTreeSimple`) |

The audit (`docs/audit-2026-04-26/workspace.md`) flags three high-severity round-trip bugs caused by the live writer's flat model:

1. **#1 — Two competing serializers** drift (audit §"High severity" #3). The dead-but-tested writer is structurally richer (parses nested splits, stacked tabs, floating windows, preserves unknown leaf keys) but never runs in production, so its test coverage is a fiction relative to what users hit.
2. **#2 — Nested-split round-trip broken** (audit §"High severity" #2). `Workspace::serialize` flattens any KDDW nested-split tree to one level. An Obsidian-authored vault opened in Corbomite and re-saved degrades layout permanently.
3. **#3 — Per-group `currentTab` collapsed** to a single global `is-active-leaf-index` (audit §"High severity" #5). Multi-group layouts lose per-group tab selection on round-trip.

## Audit correction

`docs/audit-2026-04-26/workspace.md:14-15` and `libs/core/include/corbomite/core/Workspace.h:267-271` both claim **"KDDW exposes no public Group enumeration API."** This is **stale** for KDDockWidgets 2.x. The following are public in `/usr/include/kddockwidgets-qt6/`:

- `KDDockWidgets::Core::MainWindow::layout()` → `Core::Layout*`
- `KDDockWidgets::Core::Layout::groups()` → `Vector<Core::Group*>`
- `KDDockWidgets::Core::Layout::rootItem()` → `Core::ItemContainer*`
- `KDDockWidgets::Core::DropArea::rootItem()` → `Core::ItemBoxContainer*`
- `KDDockWidgets::Core::Group::currentDockWidget()`, `currentTabIndex()`, `dockWidgets()`
- `KDDockWidgets::LayoutSaver::serializeLayout()` returns a complete KDDW-native JSON layout.

This correction unblocks the design. An addendum will be written to `docs/obsidian-audit/addenda/` after the implementation lands, and `Workspace.h:267-271` updated in-place once `m_tabGroupOf` is repurposed (deferred — see follow-ups).

---

## Architecture

### One writer, one reader, hybrid input

`Corbomite::WorkspaceSerializer::toJson(MainWindow*, Workspace*)` and `fromJson(json, MainWindow*, Workspace*)` are promoted to the canonical path. `Workspace::serialize / deserialize` become thin one-line forwarders that delegate to the serializer. Every production read/write goes through the same code as the tests.

The unified writer joins **two sources of truth**:

| Layer | Owns | Read via |
|---|---|---|
| Layout topology | KDDW | `MainWindow::layout()->rootItem()` recursion (splits + tab groups), `Layout::groups()` + `Group::currentTabIndex()` for per-group current-tab, `DockRegistry::floatingWindows()` for popouts |
| Per-leaf payload | Corbomite | `Workspace::findLeafById(uniqueName-derived id)` → `WorkspaceLeaf::serialize()` for `state`/`pinned`/`group`/icon/title |

The two views are joined on `KDDockWidgets::Core::DockWidget::uniqueName()`. `Workspace::registerLeaf` already namespaces these as `<vaultId>:<leafId>` (`Workspace.cpp:31-36`); the writer strips the `<vaultId>:` prefix to recover `leafId` for the lookup.

### Why this is clumsy and we're doing it anyway

The split is acknowledged as architecturally unattractive — a writer that pulls from two places is one place too many. Documenting the rationale because this **will** look weird to the next reader and we want the constraint to be visible in code review:

- **KDDW alone is insufficient.** KDDW's `DockWidget` is opaque to Corbomite — it carries no `viewState` / `pinned` / `group` / cached title. `LayoutSaver::serializeLayout()` produces a KDDW-native JSON shape that is *not* Obsidian-shape and not what plugins or `workspace.json` consumers expect.
- **Workspace alone is insufficient.** `Workspace` doesn't model KDDW's actual nested-split topology. `m_tabGroupOf` is a flat string-keyed cache; sibling order is leaf-insertion order; split direction is never stored. Inventing those values without consulting KDDW is precisely how today's writer produces lossy output.
- **The alternative is worse.** Replicating KDDW's split tree in `Workspace`'s data model would duplicate KDDW bookkeeping and re-introduce drift between the live KDDW state and a parallel shadow tree — exactly the failure mode `m_tabGroupOf` already exhibits (audit §"High severity" #1).
- **Substrate translation is intrinsically dual-source.** The audit at `workspace.md:7-13` calls out the architectural shape: KDDW owns the substrate, Corbomite owns leaf identity. Any writer that produces a `workspace.json`-shape JSON has to merge both.

The clumsiness is worth swallowing because it confines the awkwardness to one module (`WorkspaceSerializer`). Outside that module, every consumer sees a single `serialize()` / `deserialize()` surface on `Workspace`.

### Module structure (option B from brainstorm)

`WorkspaceSerializer.{h,cpp}` keeps its location and namespace. The implementation is rewritten:

- `LeafNode`, `TabsNode`, `SplitNode`, `WindowNode` internal types are kept (already the right shape).
- `parseLeaf` / `parseTabs` / `parseSplit` / `parseWindow` (read side) are kept; the in-process `leafSidecar` and `stackedSidecar` static maps are **deleted** — when `workspace` is non-null, leaf state lives on `WorkspaceLeaf` and stacked-ness becomes a property of the live KDDW group (currently TBD whether KDDW exposes a per-group "stacked" bit; if not, fall back to a per-leaf flag carried on `WorkspaceLeaf`).
- `walkKddwTreeSimple` (single flat tabs node) is **replaced** by a recursive `walkKddwTree(KDDockWidgets::Core::ItemContainer*)` that emits nested `SplitNode` / `TabsNode` from the live `Layout::rootItem()`.
- `materializeSplit` / `materializeTabs` / `materializeFloatingWindow` (write side) are **kept** and extended: when `workspace` is non-null, per-leaf `setViewState` / `setPinned` / `setGroup` are driven through `WorkspaceLeaf` (i.e. the serializer tells `Workspace` to construct a leaf for each parsed `LeafNode` rather than instantiate raw `DockWidget`s).
- `toJson` and `fromJson` keep their signatures; `workspace` remains nullable to preserve the existing 9-fixture test contract (placeholder-leaf shape-only round-trips).

### `Workspace::serialize` / `deserialize` after consolidation

```cpp
QJsonObject Workspace::serialize() const {
    return WorkspaceSerializer::toJson(m_kddwMain, const_cast<Workspace *>(this));
}

void Workspace::deserialize(const QJsonObject &json) {
    setLayoutReady(false);
    qDeleteAll(m_leaves);    // existing teardown
    // ... clear m_leaves, m_leavesById, m_tabGroupOf, m_activeLeaf, m_undoHistory ...
    WorkspaceSerializer::fromJson(json, m_kddwMain, this);
    // ... rest of existing post-load logic: defer non-current leaves,
    //     find activeLeaf, emit layoutChanged, setLayoutReady(true),
    //     ping-through-null setActiveLeaf hop ...
}
```

The non-trivial `Workspace::deserialize` post-load logic (defer non-current leaves, ping-through-null active-leaf hop, `lastOpenFiles` hydration) **stays in `Workspace`**; only the *parse + materialize* portion moves into the serializer. This keeps the layout-ready gate semantics unchanged and avoids cross-module ownership confusion.

---

## Data flow

### Serialize (production: `MainWindow::saveSessionState` → `Workspace::serialize` → `WorkspaceSerializer::toJson`)

1. Walk `MainWindow::layout()->rootItem()` recursively.
2. For each KDDW `Group` encountered (a tab container), emit `TabsNode { children: [...], currentTab: group->currentTabIndex() }`. **This solves item #3.**
3. For each child of the root `ItemContainer` that is itself an `ItemContainer` (nested split), recurse and emit `SplitNode { direction, children }`. **This solves item #2.**
4. For each `DockWidget` inside a `Group`, look up the corresponding `WorkspaceLeaf` via `Workspace::findLeafById(stripVaultPrefix(dw->uniqueName()))`. Use `WorkspaceLeaf::serialize()` to produce the `LeafNode` (carries `state` / `pinned` / `group` / icon / title from the live leaf; `WorkspaceLeaf.cpp:329-342`).
5. For each `FloatingWindow` in `DockRegistry::floatingWindows()`, emit a `WindowNode` with live geometry stamped from `fw->geometry()`. Floating-window contents are walked with the same recursive routine (parity with main-area: nested splits inside popouts will round-trip).
6. Emit `active: <leafId>` from `Workspace::activeLeaf()->id()`.
7. Emit `lastOpenFiles` from `Workspace::m_lastOpenFiles` (unchanged).

### Deserialize (production: `MainWindow::openVaultAt` → `SessionManager::workspaceLayout()` → `Workspace::deserialize` → `WorkspaceSerializer::fromJson`)

1. Parse the input JSON's `main` subtree into `SplitNode` / `TabsNode` / `LeafNode` (existing `parseSplit`/`parseTabs`/`parseLeaf`). Sidecar maps deleted.
2. `materializeSplit` recursively places `Group`s under `MainWindow::addDockWidget(dw, location, relativeTo)`, building real nested splits. **This solves item #2 on the read side.**
3. For each `LeafNode` encountered: when `workspace` is non-null, drive `Workspace::createLeafInGroupOf(anchor)` (creates a `WorkspaceLeaf` + KDDW `DockWidget`); call `WorkspaceLeaf::setId(leafNode.id)`, `setPinned`, `setGroup`, `setViewState(leafNode.stateBlock)`. The KDDW `DockWidget` produced by `createLeafInGroupOf` is then docked into the materialization site. (When `workspace` is nullptr — tests — the existing raw-`DockWidget`-construction path is used.)
4. Per-group `currentTab` from the parsed `TabsNode::currentTab` is applied via `Group::setCurrentTabIndex` (or `DockWidget::setAsCurrentTab()` on the indexed leaf). **This solves item #3 on the read side.**
5. `materializeFloatingWindow` already handles single-tabs popouts; extend it to use the same recursive walker so popouts with nested splits round-trip too. (Note: the existing fixture 05 only exercises single-tabs popouts — adding a nested-popout fixture is part of testing scope.)
6. Return to `Workspace::deserialize`, which then runs the existing defer-non-current logic, resolves `activeLeaf` from `json["active"]`, hydrates `m_lastOpenFiles`, emits `layoutChanged`, sets layout-ready, performs the ping-through-null `setActiveLeaf` hop.

### `m_tabGroupOf` interaction

`m_tabGroupOf` is currently the source of group identity for `nextLeafInActiveGroup`, `closeOtherLeavesInGroupOf`, etc. **This design does not touch those consumers.** `m_tabGroupOf` continues to be populated by `Workspace::createLeafInGroupOf` exactly as today; it's just no longer the source of truth for serialization. The audit's "stale-after-drag" lag (#1 high-severity) is unchanged here — that's a deferred follow-up, see below.

---

## Test contract

The existing 9 fixtures (`tests/core/fixtures/workspace-obsidian/01-09`) and `tst_workspace_serializer.cpp` continue to pass with `workspace=nullptr`. They exercise shape-only round-trip (split topology, currentTab, stacked, floating-window geometry, pinned/group, unknown-key preservation, malformed JSON fallback, orphan-leaf re-homing). No fixture changes required.

New tests added:

- **F10 — production round-trip with real `Workspace`**: construct a `Workspace`, programmatically build a layout (1 split, 2 groups, leaf in each), call `Workspace::serialize` → write, `Workspace::deserialize` → read, assert leaf ids/states/pinned/group all preserved. This is the path that exercises the `workspace`-non-null join and is the one the audit cares about.
- **F11 — per-group currentTab**: 2 groups, 3 tabs each, currentTab=2 in group A and currentTab=1 in group B. Round-trip; assert both currentTabs survive. Direct test for item #3.
- **F12 — nested splits with leaves**: equivalent of fixture 03 but with `Workspace`-non-null and real `WorkspaceLeaf` state per leaf. Tests for item #2 with real payload.
- **F13 — popout with nested split**: extends fixture 05; popout window contains a 2-pane horizontal split. Verifies the recursive walker works inside `FloatingWindow` materialization.

`tst_workspace_serializer.cpp` is the home for F10–F13.

The `MainWindow::saveSessionState` / `openVaultAt` paths are exercised end-to-end via the existing `tst_session_manager` (or its successor — verify in implementation phase) covering full disk round-trip.

---

## Failure modes and fallbacks

- **Orphan dock widget** (in `DockRegistry` but absent from `Workspace::m_leavesById`): emit with placeholder `viewType:"empty"`, `icon:"lucide-file"`, `title:"New tab"` — preserves current `walkKddwTreeSimple` behavior at `WorkspaceSerializer.cpp:225-230`. Logged via the existing `lcWorkspaceSerializer` category.
- **Unknown leaf key in input JSON**: continue to round-trip via `LeafNode::unknownKeys` (`WorkspaceSerializer.cpp:94-99`). The deletion of `leafSidecar` does not affect this — `unknownKeys` is per-`LeafNode` and round-trips through the parse → materialize → walk-back chain via the live `WorkspaceLeaf`. **Open question for impl phase:** carrying `unknownKeys` on `WorkspaceLeaf` (new field) vs. keeping a serializer-internal map keyed by leaf id. Lean toward a `WorkspaceLeaf::m_unknownLeafKeys QJsonObject` field — colocates with `pinned`/`group`/etc., disposed when leaf is.
- **Malformed `main` key** (wrong type, missing children, etc.): existing fallback at `WorkspaceSerializer.cpp:419-431` installs a default empty leaf. Preserved.
- **Missing `Group` for a `DockWidget`** during walk: skip; logged. Should not happen in a healthy layout.
- **`workspace`-null toJson when there are leaves with rich state in `Workspace`**: the function takes only `MainWindow*` and falls back to placeholder leaves. Documented in the header — tests use this; production must always pass a non-null `workspace`. A `Q_ASSERT` in `Workspace::serialize`'s call site guards against accidental null-passing in production.

---

## Deferred follow-ups (out of scope, all to be added to punch-list)

| Item | Why deferred | New punch-list entry |
|---|---|---|
| **P1 #4** — `m_unknownRoot` `left`/`right`/`floating` write-through | Not a serializer-fidelity bug; it's a **sidedock-modeling gap**. Corbomite has no internal model for `WorkspaceSidedock` (the audit lists this as Phase 7.5 stubs returning nullptr at `Workspace.h:220-221`). Fixing it correctly requires either translating Obsidian's `left`/`right` subtree into Corbomite's `CorbomiteMDI::Sidebar` state and back (large), or adding a "dirty bit" to detect Corbomite-side sidebar mutations and selectively drop the keys (smaller). Both are independent of the serializer consolidation. | "Decide `left`/`right`/`floating` JSON write-through policy in `SessionManager::doSave`. Three options: (A) drop on save; (B) pass through unmodified unless Corbomite mutated sidebar state; (C) translate to/from `CorbomiteMDI::Sidebar`. Recommend B with a sidebar-dirty bit. Audit ref: `workspace.md` §"High severity" #4 + Layout JSON compat table row `left`/`right`." |
| **`m_tabGroupOf` lag-after-drag** (audit #1) | Now solvable thanks to public `Layout::groups()`. Either: (a) refresh `m_tabGroupOf` from `Layout::groups()` on demand (cache); (b) delete `m_tabGroupOf` and derive group membership from KDDW each call. Either way is a separate refactor of the consumers (`nextLeafInActiveGroup`, `closeOtherLeavesInGroupOf`, etc.) and not the serializer's concern. The serializer reads `Layout::groups()` directly so it's unaffected by the lag. | "Repurpose `m_tabGroupOf` against live `Layout::groups()` (cache or eliminate). Update `Workspace.h:267-271` comment claiming KDDW has no public Group enumeration API. Audit ref: `workspace.md` §"High severity" #1." |
| **`stacked` per-tab-group bit storage** (if KDDW lacks a native one) | Resolved during impl: either KDDW exposes a per-`Group` "stacked" mode (check `core/Group.h`) and we read/write directly, or we add `WorkspaceLeaf::m_stacked` (a per-tab-group flag carried on the first leaf of the group, mirroring today's `stackedSidecar` shape but colocated). Decision deferred to impl phase. | (folded into impl plan) |
| **Audit doc addendum** | Write `docs/obsidian-audit/addenda/<implementation-date>-kddw-public-enumeration.md` documenting the corrected KDDW API surface (`Layout::groups()`, `rootItem()`, `Group::currentTabIndex()`, `LayoutSaver`). Read-only correction; the audit doc itself stays frozen. | (folded into impl plan as final task) |

---

## Risks

- **KDDW API stability.** This design depends on `Layout::rootItem()`, `Group::currentTabIndex()`, `Group::dockWidgets()`. These are documented public, but KDDW is on a 2.x release line; any signature drift requires a quick adapter. Acceptable risk.
- **Test fixture compatibility.** The 9 existing fixtures use 16-char hex-suffix-padded ids (e.g. `cccccccccccccccc`). When `workspace=nullptr`, the materializer constructs raw `DockWidget`s with those ids verbatim. When `workspace` is non-null, `WorkspaceLeaf` ids must be set on the new leaf via `setId` *before* `Workspace::registerLeaf`'s `m_leavesById.insert(leaf->id(), leaf)` (or rewired after, as the existing `Workspace::deserialize` does at `Workspace.cpp:786-790`). The implementation plan must order these operations carefully.
- **Defer-non-current logic interaction.** `Workspace::deserialize` defers non-active, non-currentTab leaves at `Workspace.cpp:814-839`. With per-group currentTab now real (not synthetic from `firstInGroup`), the defer set should *shrink* (fewer leaves are deferred because each group has its own current). This is a behavioral improvement, not a regression — but should be covered by a test (call it F14: 2 groups × 3 tabs each, currentTab=1 in group A, currentTab=0 in group B; assert leaves at indices 0 and 2 in A and indices 1 and 2 in B are deferred while the currentTabs are loaded).
- **Single-vault-at-a-time assumption.** Today's `DockRegistry` is process-global; `walkKddwTreeSimple` reads it directly. The new walker reads `MainWindow::layout()->rootItem()` which is workspace-local — *better*, but if any production code ever opens two `Workspace`s in one process, the new walker handles that correctly while the old one didn't. Strict improvement.

---

## Acceptance criteria

- `Workspace::serialize` and `WorkspaceSerializer::toJson` both produce *byte-identical* output for the same in-memory layout (with `workspace=this` passed). Verified by a unit test that calls both and `QCOMPARE`s.
- Every test in `tst_workspace_serializer.cpp` continues to pass — the 9 existing fixtures + the 4 new ones (F10–F13 and F14 deferred-set test).
- `MainWindow` end-to-end save → quit → relaunch → load preserves: split topology (nested), per-group current tab, leaf state per leaf, pinned/group flags, floating-window geometry, `lastOpenFiles`, `active` leaf id.
- The on-disk JSON shape produced by Corbomite, opened in Obsidian, displays the same nested-split layout with the same per-group tab selection. (Manual verification — no Obsidian-replay automation.)
- Punch-list items P1 #1, #2, #3 marked `[x]`. P1 #4 unchanged. New follow-up entries added per the table above.

---

## File touch list

**Modified:**
- `libs/core/src/WorkspaceSerializer.h` — clarify `workspace` semantics; remove "deferred to later phase" wording.
- `libs/core/src/WorkspaceSerializer.cpp` — replace `walkKddwTreeSimple` with recursive walker; delete `leafSidecar` / `stackedSidecar` statics; wire `Workspace*` into materialize path; extend floating-window walker for nested splits.
- `libs/core/src/Workspace.cpp` — `serialize()` and `deserialize()` reduced to forwarders + post-load defer/active-leaf logic.
- `libs/core/include/corbomite/core/WorkspaceLeaf.h` + `libs/core/src/WorkspaceLeaf.cpp` — possibly add `m_unknownLeafKeys` and `m_stacked` (decision in impl phase).
- `tests/core/tst_workspace_serializer.cpp` — add F10–F14.
- `tests/core/fixtures/workspace-obsidian/` — add fixtures for new tests.
- `docs/punch-list.md` — mark #1, #2, #3 done; add deferred-follow-up entries from the table above.

**Created:**
- `docs/obsidian-audit/addenda/<date-when-created>-kddw-public-enumeration.md` (audit correction; created during implementation).

**Untouched:**
- `src/app/SessionManager.{h,cpp}` — no API change. `setWorkspaceLayout(mainJson, activeLeafId)` continues to be the seam.
- `src/app/MainWindow.cpp:809-831, 2130-2160` — call sites unchanged.
- All `WorkspaceLeaf` consumers (panels, plugins, hosts) — `WorkspaceLeaf::serialize/deserialize` semantics preserved.
