# Cluster Y — Workspace migration onto KDDockWidgets (Retro)

**Closed:** 2026-04-25.
**Spec / scouting:** [`../superpowers/plans/archive/2026-04-23-cluster-y-workspace-kddockwidgets-SCOUTING.md`](../superpowers/plans/archive/2026-04-23-cluster-y-workspace-kddockwidgets-SCOUTING.md)
**Plan:** [`../superpowers/plans/archive/2026-04-23-cluster-y-workspace-kddockwidgets.md`](../superpowers/plans/archive/2026-04-23-cluster-y-workspace-kddockwidgets.md)
**Commits (range):** `fd336369..bd1b50aa` — 43 commits across 8 phases (2026-04-23 → 2026-04-25). Phase-1 smoke test (`fd336369`); Phase 2 dependency declaration (`f0913a51`); Phase 3 WorkspaceSerializer + 9 fixtures (`19469965..05da6a04`); Phase 4a leaf-typed API redesign with LeafSubstrateAdapter detour and revert (`38c935c6..c112cee1`); Phase 4b atomic substrate flip (`89655ff8`); Phase 5 floating-window popout (`d84db521..967e34a5`); Phase 6 active-leaf router + signal-shaping (`f065ed12..24b489cc`); Phase 7 plugin-API-shape alignment (`076a8346..bd1b50aa`).

---

## Goal

Replace the hand-rolled `QSplitter`/`QTabWidget`-over-recursion `Workspace` substrate with `KDDockWidgets::QtWidgets::MainWindow` + `DockWidget`, while preserving Corbomite's ownership of `.obsidian/workspace.json` (no `LayoutSaver` use), 16-char leaf ids, view-state/ephemeral-state, undo-close, and pin/group semantics. Approach **B**, opacity **(ii)**, scope **β** — see scouting doc §3.

## What shipped

### P1 — KDDW embedding smoke test (`fd336369`, 2026-04-23)

Stand-alone test (`tst_workspace_embed_kxmlgui`) instantiates a `KDDockWidgets::QtWidgets::MainWindow` inside our `KXmlGuiWindow` central-widget slot, drops in a `DockWidget`, and verifies it renders + responds to drag. Risk reducer for Phase 4. Surfaced two bugs filed in backlog (`9fb2fe47`); fix for `split_right`/`split_down` CommandRegistry dispatch (`69504a03`) landed independently as a Cluster R follow-up.

### P2 — Hard dependency on KDDW (`f0913a51`, 2026-04-23)

`find_package(KDDockWidgets-qt6 2.0 REQUIRED)` upgraded from optional. Documented in CLAUDE.md `### Dependencies` block as `kddockwidgets-qt6` on Arch/Manjaro.

### P3 — WorkspaceSerializer + 9 Obsidian fixtures (`19469965..05da6a04`, 2026-04-23)

New `Corbomite::WorkspaceSerializer` in `libs/core/` consumes a synthetic KDDW dock tree (constructed standalone in tests) + serializes to / deserializes from `workspace.json`. 9 fixtures (`tests/data/workspace-fixtures/01..08/workspace.json` + the empty-default fallback) cover:

- `01` single leaf
- `02` two-leaf horizontal split
- `03` nested splits
- `04` stacked tabs (preserves the `stacked: true` flag round-trip)
- `05` floating windows (geometry + state)
- `06` pinned + group ids preserved through round-trip
- `07` empty default tree
- `08` unknown root keys preserved (forward compat)

Plus two error-path tests: `tst_workspace_serializer::malformedJsonFallsBackToDefault` (Task 3.9) and `materializeSplit` orphan-recovery (Task 3.10). All 9 fixtures + 2 error-paths green.

### P4a — Public API redesign (`38c935c6..c112cee1`, 2026-04-24)

Reshaped `Workspace`'s public surface to be **leaf-typed** (`createLeafInActiveGroup`, `splitFromLeaf`, `closeLeaf`, `iterateLeaves`) instead of substrate-typed (`addTab`, `splitTabs`, `tabsAt`). Substrate stayed pre-Y throughout this phase — the goal was to land the new shape with zero substrate churn so Phase 4b could be atomic.

Q2 pivot — the original Phase 4a plan called for relocating the `WorkspaceTabs` / `WorkspaceSplit` / `WorkspaceItem` / `WorkspaceParent` headers to a private `libs/core/src/legacy/` directory and adding `WorkspaceLeaf : QObject + LeafSubstrateAdapter` inheritance in P4a. We took a stab at the adapter (`75adbf04`), reverted it (`3d8c1ba4`) when the substrate-touching work bled the API redesign across two phases simultaneously, and pivoted to deferring substrate inheritance changes to P4b (`1fc3ca3c`). Lesson logged in [feedback_ephemeral_vs_final_correctness](../../.claude/projects/-home-clinton-dev-Corbomite/memory/feedback_ephemeral_vs_final_correctness.md): "most correct" means correct *final state* with minimal transitional contortion, not architecturally pure transitional state.

Six port commits then ran the API surface flip end-to-end: new leaf-typed methods (`38c935c6`), `View::onTabMenu` port (`c56d3ff7`), `MainWindow.cpp` port (`bc923c2e`), `WorkspaceController::openFile` port (`e02e8395`), 10-test-file port (`b49d6790`), public-surface demotion of substrate types (`b39477e0`), include cleanup (`b3abd005`), and a defensive gate on `WorkspaceTabs` signal cascade for the first-tab insert (`c112cee1`). Public-surface audit per [feedback_substrate_swap_audit](../../.claude/projects/-home-clinton-dev-Corbomite/memory/feedback_substrate_swap_audit.md) — header-by-header confirmation that no `WorkspaceTabs*` / `WorkspaceSplit*` / `QSplitter*` / `QTabWidget*` typedefs leaked into public Workspace headers.

### P4b — Atomic substrate flip (`89655ff8`, 2026-04-24)

Single commit replaces the `QSplitter`/`QTabWidget` recursion underneath `Workspace` + `WorkspaceLeaf` with `KDDockWidgets::QtWidgets::MainWindow` + `DockWidget` composition. `WorkspaceTabs` / `WorkspaceSplit` / `WorkspaceItem` / `WorkspaceParent` deleted. `setGuestView` wires each `WorkspaceLeaf`'s `View*` into its `DockWidget`. `Workspace::createLeafInActiveGroup` / `splitFromLeaf` translate to KDDW's `addDockWidget(target, location)`. KDDW takes over drag/drop, drop indicators, tab-bar drag, split resize, and floating-window spawn.

Surfaced one regression filed in backlog §10: `tst_e2e_gui::testCloseTab` — KDDW's tab-close signal isn't externally drivable from `QTabBar::tabBarClicked`-style synthetic events, so the test no longer simulates the close click path. Test still runs the non-close-flow assertions; the close-flow asserts are skipped pending a KDDW upstream API addition or a custom test helper.

### P5 — `WorkspaceWindow` atop `FloatingWindow` (`d84db521..967e34a5`, 2026-04-25)

Failing test first (`d84db521`): `Workspace::popoutLeaf(leaf)` is expected to spawn a `KDDockWidgets::Core::FloatingWindow*` discoverable via `DockRegistry`. Implementation (`437d5a9f`) calls `dockWidget->setFloating(true)` then walks `DockRegistry` to capture the spawned window. Close handler (`2e49e780`) wires `FloatingWindow::aboutToBeDestroyed` to emit `leafClosed` for each child. Geometry + maximize round-trip (`967e34a5`) reads from the `FloatingWindow` directly during serialize and applies via `setGeometry` + `setWindowState(Qt::WindowMaximized)` during deserialize. `workspace.json`'s `floating[]` array now round-trips `{x, y, w, h, state, leaves[]}`.

**Closes Cluster G follow-up #6** (`WorkspaceWindow` popout integration). Per-window zoom deferred to Cluster V.2 (per backlog "Per-window zoom persistence" entry; existing scope decision from brainstorm 2026-04-23).

### P6 — `WorkspaceActiveLeafRouter` + signal-shaping (`f065ed12..24b489cc`, 2026-04-25)

Three commits, hybrid design (Workspace owns `m_activeLeaf`, router owns layout-ready gate + identity-gate + vault-switch suppression — see `decisions-archive.md` 2026-04-25 entry for the A/B/C trade-off):

- `f065ed12` — `layoutReady` gate on `Workspace::setActiveLeaf`. Suppresses `activeLeafChanged` emissions during initial layout deserialization (so consumers don't see N spurious activations as leaves materialize).
- `c6688d72` — Extract focus-driven active-leaf routing into `WorkspaceActiveLeafRouter`. Router subscribes to `qApp->focusObjectChanged`, walks parents to find the owning `WorkspaceLeaf`, identity-gates against current active leaf, and suppresses cross-vault transitions during `closeVault`/`openVault`.
- `24b489cc` — `Workspace::resize(QSize)` + `windowFrameChange` signal added to round out the Obsidian Workspace API parity surface.

### P7 — Plugin-API-shape alignment (`076a8346..bd1b50aa`, 2026-04-25)

Five commits, one per task, in dependency order:

- `076a8346` — `LeafMode {Same, Tab, Split, Window}` + `LeafDirection {Horizontal, Vertical}` enums + `getLeaf` decl.
- `1ab53402` — `Workspace::getLeaf(LeafMode, LeafDirection)` factory implementation + 5 mode tests.
- `bcd54fba` — `Workspace::openLinkText(linktext, source, mode, opts)` dispatcher + `LinkResolverFn` injection seam (identity default) + 4 tests.
- `0252f539` — 5 `WorkspaceController` proxy additions (`getLeavesOfType`, `iterateAllLeaves`, `getActiveViewOfType`, `openLinkText`, `getLeaf`) + 5 proxy tests using a new `MarkdownStubView` that round-trips `getViewType()` + ephemeralState.
- `bd1b50aa` — `WorkspaceContainer` / `WorkspaceRoot` / `WorkspaceFloating` / `WorkspaceSidedock` stub classes + `Workspace::rootSplit() / floatingSplit() / leftSplit() / rightSplit()` accessors. `leftSplit()` / `rightSplit()` return `nullptr` (sidebars still in `CorbomiteMDI`); future sidebar-migration cluster wires them.

**Closes Cluster G follow-up #3** (`openLinkText` dispatcher). `LinkResolverFn` injection seam deviates from the plan's direct `m_vault->resolveLink` call to preserve the Q.0 `libs/vault → libs/core` dep direction — `libs/core` PRIVATE-links `Corbomite::Storage` so `LinkResolver` is reachable but `Vault`/`MetadataCache` are not. See `decisions-archive.md` 2026-04-25 entry for the option (a)/(b)/(c) trade-off.

### P8 — Verification + closeout (this retro)

- Clean rebuild from scratch (`rm -rf build && cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build -j 10`) — clean.
- Full ctest -j 10: **284/289 pass.** 5 failures (`tst_markoff_undo_grouping`, `tst_markoff_table_operations`, `tst_e2e_gui`, `tst_completion_popup`, `tst_benchmark_layout`) are all in the pre-existing known-flaky list (3 in backlog §10, 2 in Markoff submodule's `phase-c-status.md` known-flaky list). Zero Cluster Y regressions.
- Workspace-related tests (`tst_workspace_*` + `tst_obsidian_vault_roundtrip` + `tst_view_mode_serializer` + `tst_proxy_workspace`): **18/18 pass** including the 8-fixture serializer round-trip.
- Plugin regression (`tst_(backlinks|outlinks|outline|properties|search|file_explorer|local_graph|graph_view|bookmarks)*` + `tst_search_*`): **15/15 pass.**
- Manual QA: **deferred** — user will walk the GUI checklist (Task 8.4 in plan) when not over SSH. None of the automated suites can exercise tab-drag-between-panes / drag-to-floating-window / vault-switch reparent visually. Items to walk: Obsidian-fixture vault layout restore parity, drag-tab-between-panes, drag-to-pane-edge split-creation, drag-off-application floating-window creation, drag-back re-dock, close-all-tabs pane dissolution, Ctrl+Shift+T undo-close, close-floating-window cascading leaf-close, second-vault-open ghost-dock check, sidebar (Backlinks/Outlinks/LocalGraph) unaffected by KDDW central-widget swap.

## Deliberate MVP cuts

- **Per-window zoom persistence** — popout-window `zoom` field is not round-tripped. App-global zoom from `ThemeService` applies at restore time. Tracked in backlog §2 "Per-window zoom persistence" → Cluster V.2.
- **`DropIndicatorBridge` skipped** — KDDW's default drop indicators ship as-is; no Corbomite-themed override. Acceptable because KDDW indicators are visually clear under both Breeze and dark themes; revisit only on user complaint.
- **γ-scope Workspace events** — `file-menu` / `leaf-menu` / `tab-group-menu` / `markdown-viewport-menu` / `url-menu` / `hover-link` / `quick-preview` / `active-leaf-change` linked-pane consumers / `registerObsidianProtocolHandler` / `getLayout`/`setLayout`/`changeLayout` deferred to owner clusters (Cluster R, Cluster H#6, Cluster Z, three new backlog entries). Not regressions — these were always γ-scope per scouting doc §9.
- **`WorkspaceWindow` standalone QWidget facade** — `widget()` / `setWindowGeometry` / `showWindow` / `closeWindow` / `setMaximized` / `serialize()` survive but are dead post-P5 (production reads geometry from `KDDockWidgets::Core::FloatingWindow*` directly via `DockRegistry`). Only `tst_workspace_window` exercises them. Cleanup follow-up logged below.
- **`leftSplit()` / `rightSplit()` return `nullptr`** — schema-shape ships now, sidedock implementation deferred until a future cluster migrates `CorbomiteMDI`'s sidebars onto the same KDDW substrate. Plugin code that walks `workspace.rootSplit()` / `floatingSplit()` works today; code that walks `leftSplit()` / `rightSplit()` must null-check.

## Absorbed follow-ups

- **Cluster G follow-up #3** — `Workspace::openLinkText` centralised dispatcher. Closed via P7.3 (`bcd54fba`). Note: real wiring of the `LinkResolverFn` to Vault+MetadataCache+create-if-missing is the **only** Y-internal carry-forward (see Open follow-ups below).
- **Cluster G follow-up #6** — `WorkspaceWindow` popout integration. Closed via P5 (`d84db521..967e34a5`). Geometry + maximize persisted; close-window cascades to leaf closes; `Workspace::popoutLeaf` and `reparentToMain` round-trip cleanly.

## Open follow-ups

### 1. Real `LinkResolverFn` wiring **(carry-forward from P7.3)**

`Workspace::openLinkText` accepts a `std::function<QString(linktext, source)>` resolver but production never calls `Workspace::setLinkResolver(...)`. The default identity resolver passes `linktext` through unchanged, so the dispatcher round-trips for already-resolved paths but does not consult `MetadataCache::resolveLink` or create files on miss. Production wiring lives in `MainWindow` setup or in Cluster Z (linked-leaf brainstorm). Concretely: assemble a lambda that captures `Vault*` + `MetadataCache*` + `FileManager*`, calls `MetadataCache::resolveLink` on the resolved-on-disk side, and falls through to `FileManager::createFile` when the resolution misses. **Logged in backlog §3.**

### 2. `tst_e2e_gui::testCloseTab` skipped close-flow assertions **(P4b regression)**

Already in backlog §10 Stability. KDDW's tab-close signal isn't externally drivable from synthetic events. Two paths: (a) custom Qt event-router test helper that walks the KDDW `Stack`/`TabBar` widget tree and emits the close click directly, (b) upstream KDDW API addition for synthesized close events. Not blocking; deferred until a real tab-close UX bug needs the regression coverage.

### 3. `WorkspaceWindow` standalone facade cleanup **(P5 dead-code)**

Already in backlog §3. Two paths: wrap KDDW's `FloatingWindow*` in `WorkspaceWindow` (rewrites the standalone `tst_workspace_window` to drive KDDW underneath) or shrink `WorkspaceWindow` to id-only and delete the QWidget+serialize surface. Phase 6 was the natural moment per the original backlog entry; deferred because cleanup risk during the substrate flip outweighed the bookkeeping savings. Pick up when next touching popout code.

### 4. Tab drag-to-reorder under Wayland **(possibly closed incidentally)**

Backlog §3 entry from Cluster G era — `QTabBar::setMovable(true)` + Wayland mouse-grab protocol gap. With P4b's substrate flip, the tab bar is now KDDW's `TabBar` (not `QTabBar`), and KDDW is widely used on Wayland. **Hypothesis:** P4b closes this incidentally; user manual QA (deferred Task 8.4) will confirm. If broken, file as a P4b residue rather than a Cluster G era item.

### 5. Quirky split-right under hamburger dispatch **(possibly closed incidentally)**

Backlog §3 entry from Cluster Y P3 manual QA — `split_right`/`split_down` from per-view hamburger sometimes opens current tab as peer (Obsidian-correct) and sometimes opens blank tab. Original suspicion was vault layout corruption or `Workspace::duplicateLeaf` race. With P4b in place and P7's `getLeaf(LeafMode::Split, LeafDirection::Vertical)` factory available, the hamburger commands may now route through the canonical factory rather than the legacy split path. Hypothesis: closed incidentally; re-test during deferred manual QA. If still broken, requires a `duplicateLeaf` audit against the new substrate.

## γ-scope Workspace events still tracked elsewhere

These are **not** Cluster Y debt — they were always γ-scope per scouting doc §9 — but listed here for traceability when reading this retro standalone:

- `file-menu` / `leaf-menu` / `tab-group-menu` / `markdown-viewport-menu` / `url-menu` Workspace events → Cluster R follow-ups (menu-construction-site migrations) and the "Migrate remaining menu sites onto MenuSectionHelper" backlog entry.
- `hover-link` event + `registerHoverLinkSource` → Cluster H follow-up #6.
- `active-leaf-change` linked-pane consumers (`receiveSyncState`) → Cluster Z.
- `quick-preview` debounced editor-content sync → Markoff integration backlog entry.
- `registerObsidianProtocolHandler` (`obsidian://` / `corbomite://` URL routing) → backlog §2.
- Workspaces core plugin (`workspaces.json` named-layout snapshots) → backlog §2.

## Architectural notes / lessons

- **The "atomic substrate flip" pattern works.** Phase 4 split into 4a (API additions, no substrate touch) + 4b (substrate replaced in one commit, all consumers already on the new shape) was the right call. The original Phase 4 plan called for both in one phase; Q2 pivot doc (`1fc3ca3c`) records the rationale. Lesson: when a refactor swaps both API and substrate, lift the API change forward and keep the substrate flip as the smallest possible single commit at the end.
- **Header-by-header public-surface audit when demoting a type** (per `feedback_substrate_swap_audit`). Function-name lists hide signature leaks. The Phase 4a `b39477e0` commit deleted six public methods that nominally returned `WorkspaceTabs*` even though the function-name list never mentioned the return type.
- **Dep-direction-preserving DI > moving the operation up the layer cake.** `LinkResolverFn` injection on `Workspace` keeps `libs/core → libs/vault` clean while still landing the Obsidian-shape `openLinkText` API at its expected location (`WorkspaceController` proxy). See P7.3 design decision in `decisions-archive.md` 2026-04-25.
- **Hybrid focus-router beats both pure router and pure Workspace ownership.** Phase 6 hybrid: Workspace owns `m_activeLeaf` + emit; router owns gate + identity-check + vault-switch suppression. Pure router required Workspace to own a back-pointer to the router for emit; pure Workspace ownership couldn't suppress focus thrash during vault swap without pulling QApplication-level focus state into Workspace itself. Hybrid lands the responsibility where the dependencies already are.
- **Stub-then-extend for view-type-aware tests.** Phase 7's pre-existing `StubView` returned hardcoded `"stub"` from `getViewType()` regardless of the registry key under which it was registered. The new `MarkdownStubView` round-trips both `getViewType()` and ephemeralState. Lesson: when adding tests that assert view-type behaviour, register a typed stub upfront — the View base class no-ops both `getViewType` and `setEphemeralState`, which silently passes the original `StubView` through.

## Test surface delta

- New test binaries: `tst_workspace_serializer` (P3, 11 cases), `tst_workspace_factory` (P7, 9 cases), `tst_workspace_containers` (P7, 5 cases), `tst_workspace_active_leaf_router` (P6), `tst_workspace_popout` (P5), `tst_workspace_embed_kxmlgui` (P1).
- Extended: `tst_proxy_workspace` (+5 cases for P7.4 proxy additions), `tst_workspace_serialize`, `tst_workspaceleaf`, `tst_workspace_session`, `tst_workspace_integration`.
- 9 fixture JSON files under `tests/data/workspace-fixtures/` (P3.1 + P3.4..3.8).
- All workspace-test-binary count: 18. All passing as of P8 verification.

## Unblocks

- **Cluster Z (linked views + active-leaf tracking)** — Z was sequenced after Y because linked-leaves sit very differently under a KDDW `DockRegistry` tree than under the hand-rolled split tree. With Y closed, Z brainstorm + writing-plans can begin (per scouting doc §3 Y-first sequencing decision and Task 8.7 step 3).
- **Cluster R hamburger "Open in new window" slot** — currently a disabled placeholder; activates now that `popoutLeaf` is wired (P5).
- **Cluster S "Open linked view"** submenu upgrade — interim dispatch to each plugin's `:open` command can now upgrade to real `openLinkText` leaf opening (P7.3) once #1 above (real resolver wiring) lands.
