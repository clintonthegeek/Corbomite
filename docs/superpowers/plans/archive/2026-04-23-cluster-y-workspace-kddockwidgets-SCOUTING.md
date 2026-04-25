# Cluster Y — Workspace migration onto KDDockWidgets (SCOUTING)

> **Status:** Scouting (post-brainstorm). Decisions landed; writing-plans session pending.
> **Brainstorm date:** 2026-04-23.
> **Scope agreed:** β — substrate swap + plugin-API shape alignment (see §3 Decisions).
> **Next step:** superpowers:writing-plans to produce the full implementation plan.
> **Expansion trigger:** user authorisation to begin execution; Cluster Z brainstorm will follow this one (Z sequencing decision: Y first, per §3).

---

## 1. Goal

Replace the hand-rolled Corbomite Workspace tree substrate (`libs/core/src/Workspace.cpp` + `WorkspaceLeaf.cpp` + `WorkspaceTabs.cpp` + associated `QSplitter`/`QTabBar` mechanics, ~1,302 LOC shipped in Cluster G) with `KDockWidgets::MainWindow` + `DockWidget` + `FloatingWindow`, while **preserving all code above the tree substrate** (leaf identity, pinning, groupId, navigation history, undo-close, view-state + ephemeral-state, Obsidian `workspace.json` round-trip, deferred-load, `EmptyView` fallback, plugin proxy API).

**User-visible wins:**

- Tab drag between panes.
- Tab drag to split (edge drop-zones with visual indicators).
- Tab drag out to new floating window.
- Tab drag back from floating window to main window.
- Popout-window geometry + maximize restore on session reload (completes Cluster G follow-up #6).

**Internal-architecture wins:**

- Delete ~900 LOC of widget mechanics we shouldn't have been hand-rolling (per saved feedback memory "Harvest don't hand-roll").
- Absorb Cluster G follow-ups #3 (centralised `openLinkText` dispatcher) and #6 (`WorkspaceWindow` popout integration).
- Rename tree-node classes to match Obsidian's model so plugin developers find what they expect.
- Unify leaf-creation on a `getLeaf(mode, dir)` factory matching Obsidian's canonical entry point.

**Non-goals (explicitly scoped out):**

- Linked-pane state sync / `receiveSyncState` / `setGroup` consumers — **Cluster Z**.
- Sidebar-hosting model change — CorbomiteMDI stays; `WorkspaceSidedock` is a stub class for schema compat only.
- `registerObsidianProtocolHandler` (`obsidian://` URL routing) — post-parity cluster, tracked in backlog.
- Obsidian's Workspaces core plugin (`.obsidian/workspaces.json` named layout snapshots) — post-parity cluster, tracked in backlog.
- Mobile layout (`workspace-mobile.json`) — desktop-only project.
- `registerEditorExtension` (CodeMirror 6 extensions) — Markoff owns its extension surface; N/A for Corbomite.
- Full plugin-API Workspace-event parity (the full "γ" surface) — split across Cluster R, Cluster H follow-up #6, Cluster Z, Markoff, and new post-parity clusters (see §9 "γ deferrals").

---

## 2. Authoritative references

**Obsidian reverse-engineered specification (the compat target):**

- [`docs/obsidian-audit/domains/workspace.md`](../../obsidian-audit/domains/workspace.md) — 566 lines, complete workspace model.
- [`docs/obsidian-audit/domains/views.md`](../../obsidian-audit/domains/views.md) — 404 lines, view + eState + ViewState + deferred-placeholder (`eD`) / empty (`tD`) / unknown-type (`nD`).
- [`docs/obsidian-audit/PLUGIN-API-SKETCH.md`](../../obsidian-audit/PLUGIN-API-SKETCH.md) — 847 lines, plugin-API sketch including `Workspace` / `WorkspaceLeaf` / menu events.
- [`docs/obsidian-audit/VAULT-FORMAT.md`](../../obsidian-audit/VAULT-FORMAT.md) — `.obsidian/workspace.json` schema authority.

**Current Corbomite surface:**

- `libs/core/src/Workspace.cpp` + headers — tree, signals, serialize, focus routing.
- `libs/core/src/WorkspaceLeaf.cpp` — leaf lifecycle, state, history, deferred-load.
- `libs/core/src/WorkspaceTabs.cpp` — tab widget (to be demoted).
- `libs/core/src/WorkspaceWindow.cpp` — popout stub (to be completed).
- `libs/core/src/CorbomiteMDI.cpp` — sidebar host (unchanged).
- `libs/vault/include/Corbomite/vault/WorkspaceController.h` — plugin-facing proxy.
- `docs/cluster-retros/cluster-g.md` — Cluster G retro + 6 follow-ups.
- `docs/decisions-archive.md` — 2026-04-19 sidebar-invisible fix (sets synchronous-deletion-on-vault-switch pattern).

**External library (harvest target):**

- `~/src/KDDockWidgets` — local checkout of KDAB's KDDockWidgets library. Also present as a system library in this build environment.
- Key public API surface: `KDDockWidgets::MainWindow`, `DockWidget`, `FloatingWindow`, `DockRegistry`, `LayoutSaver` (latter **not used** by Y — we own persistence).
- Qt6 backend required; Qt Widgets (not Qt Quick).

**Saved feedback memories consulted:**

- "Harvest don't hand-roll" ([file](../../../.claude/projects/-home-clinton-dev-Corbomite/memory/feedback_smoothscroll.md)) — motivates B over A'/A''.
- "Dev builds only" — unchanged by this cluster (everything continues to work under `-DCORBOMITE_DEV_BUILD=ON`).
- "No feature branches" — scouting doc assumes direct-to-master execution.

---

## 3. Decisions made in brainstorm (2026-04-23)

Decisions are recorded here for historical context; writing-plans session inherits them.

### 3.1 Substrate approach → **B**

Three options weighed:

- **A'** — hand-roll drag UX on current substrate. Rejected: ~1.5 weeks, we'd be writing the exact kind of GUI-framework internals "Harvest don't hand-roll" was saved to stop.
- **A''** — harvest KDDW's dumb parts (drop-zone geometry, overlay painting) as reference material, hand-roll the smart parts. Real middle ground; saves ~0.5 weeks vs A'. Rejected: still ~500–800 LOC of drag-state-machine ownership forever; upstream-maintained drag choreography is precisely what we want from KDDW.
- **B** — host tree in KDDW's `MainWindow` + `DockWidget`, keep everything above the substrate (persistence, leaf lifecycle, history, undo). **Accepted.**

### 3.2 Public API preservation → **(ii) loosely opaque**

- `Workspace` and `WorkspaceLeaf` public surface preserved (stable plugin contract).
- `WorkspaceTabs` and `WorkspaceSplit` demoted from public widget classes to internal serialization structs. Tests poking their internals (expected: 2–3 of 13) get rewritten.
- New classes added to match Obsidian's shape (§4).

### 3.3 Scope → **β (substrate swap + API shape alignment)**

Weighed three scopes:

- **α** Narrow substrate swap only (tab drag + popout). Rejected: false economy — we'd rewrite the substrate once and revisit for class-renames later.
- **β** Substrate swap + API shape alignment (class names, `getLeaf(mode)` factory, `openLinkText` dispatcher, iteration methods on proxy, `workspace.json` shape). **Accepted.**
- **γ** β + full Obsidian event-surface parity. Rejected: many γ events are owned by other clusters (R, H, Z, Markoff); Y shouldn't absorb cross-cluster scope.

### 3.4 Sequencing Y vs Z → **Y first, then Z on the KDDW substrate**

- Z's `receiveSyncState` design hinges on what the new substrate provides for leaf-to-leaf binding. Building it on a tree we're about to delete is waste.
- Z brainstorm will happen after Y plan lands; Z execution after Y execution.

### 3.5 Popout scope in Y → **geometry + maximize; zoom deferred**

- `x, y, width, height` — trivial via `QWindow::geometry`. Ship.
- `maximize` — trivial via `QWindow::windowState` check. Ship.
- `zoom` — deferred. Per-window zoom requires infrastructure we don't have (current zoom is app-scoped via Cluster V `ThemeService`). Popouts restore at current app zoom with a known-missing field in `workspace.json`. Tracked in backlog as a V.2 companion item.

### 3.6 γ event deferrals → **explicit cross-cluster assignments**

Every Obsidian workspace event Y does **not** emit gets an owner cluster. The owner's backlog entry / plan / retro is updated to reference Y as a dependency. See §9 for the full table.

---

## 4. Architecture

### 4.1 Core move

`Corbomite::Workspace` becomes a controller that **composes** a `KDockWidgets::MainWindow` set as the central widget of our existing `KXmlGuiWindow`. Each `WorkspaceLeaf` **composes** a `KDDW::DockWidget` whose `setGuestView()` holds the `View*` instance.

**Corbomite owns** (above the substrate):

- Leaf identity: 16-char random ids.
- View-state + ephemeral-state (our split: `getViewState()`/`setViewState()` + `getEphemeralState()`/`setEphemeralState()`).
- Pinning (`WorkspaceLeaf::setPinned`).
- GroupId binding (`WorkspaceLeaf::setGroup`) — the Z-consumer hook stays, Z wires the actual sync.
- Deferred-load placeholder (`eD`) with cached icon+title.
- Navigation history (`LeafHistory`, 20-cap).
- Undo-close stack (10-cap `UndoEntry`, Ctrl+Shift+T).
- `.obsidian/workspace.json` serialization in Obsidian-shape.
- Active-leaf routing (via `WorkspaceActiveLeafRouter`).
- Plugin proxy API contract (`WorkspaceController`).

**KDDW owns** (the substrate):

- Tree topology (splits, tabs, floating windows).
- Tab mechanics (tab bar, reorder, close button).
- Split resize handles.
- Drag/drop choreography (`DragController`, `DropIndicatorOverlay`, `WindowBeingDragged`).
- Floating-window management (creation on drag-out, reparenting on drag-in).
- Platform edge cases (multi-monitor, fractional DPI, Wayland vs X11).

### 4.2 Class topology (post-migration)

| Class | Fate | Role |
|---|---|---|
| `Workspace` | Stays — refactored internals | Public controller; `getLeaf(mode, dir)`; `openLinkText()` dispatcher; serialize/deserialize entry points; signal surface; adds `rootSplit()` alias for `mainRoot()` |
| `WorkspaceLeaf` | Stays — refactored internals | Composes a `KDDW::DockWidget`; carries id / pinned / group / history / view-state / eState; public API preserved |
| `WorkspaceWindow` | Stays — completed (was stub) | Wraps `KDDW::FloatingWindow`; geometry + maximize persistence; per-window focus routing |
| `WorkspaceRoot` | **New** | Thin wrapper for main-area root; aliased by `Workspace::rootSplit()` |
| `WorkspaceContainer` | **New** | Obsidian base class for Root/Window in their model; we match for shape compat even if behaviour is minimal |
| `WorkspaceFloating` | **New** | Represents `workspace.json` `floating` array container; owns `WorkspaceWindow[]` |
| `WorkspaceSidedock` | **Stub only** | Defined for Obsidian-schema / plugin-API compat. Never instantiated by Y. Reserved for future sidebar-migration cluster. |
| `WorkspaceSplit` | **Demoted** — widget class deleted | Internal serialization struct (~100 LOC). Not a QWidget. Not in public headers. |
| `WorkspaceTabs` | **Demoted** — widget class deleted | Same fate: serialization struct only. `QTabBar`+`QStackedWidget` mechanics are now `KDDW::Group`. |
| `WorkspaceSerializer` | **New module** | Walks KDDW in-memory dock tree → emits Obsidian JSON; parses Obsidian JSON → reconstructs KDDW tree via `addDockWidget` / `addDockWidgetAsTab` / `FloatingWindow` |
| `WorkspaceActiveLeafRouter` | **New module** | Composes KDDW focus + `QApplication::focusChanged` into one `Workspace::activeLeafChanged(leaf*)` signal (identity-gated, vault-switch-suppressing, matching Obsidian semantics) |
| `DropIndicatorBridge` | **New (optional, small)** | ~100 LOC if KDDW default indicators need KDE-theme styling. MVP non-blocker. |

### 4.3 Net code change

- **Delete:** ~900 LOC of widget mechanics (`WorkspaceTabs.cpp`, `WorkspaceSplit.cpp` as widgets, parts of `Workspace.cpp` that do splitter/tab geometry).
- **Add:** ~700 LOC (`WorkspaceSerializer`, `WorkspaceActiveLeafRouter`, completed `WorkspaceWindow`, new stub classes, `DropIndicatorBridge`).
- **Net:** slight decrease, major complexity reduction. Upstream-maintained drag choreography (~50k+ LOC in KDDW) replaces our ~400 LOC of almost-drag-UX.

### 4.4 Embedding verification (Phase 1 risk reducer)

`KDDW::MainWindow` inherits `QMainWindow`. Our `MainWindow` is a `KXmlGuiWindow` (extends `KMainWindow` → `QMainWindow`). Plan: set the KDDW MainWindow as the `centralWidget()` of the KXmlGuiWindow. Menus, toolbars, and `KActionCollection` actions owned by the outer KXmlGuiWindow.

KDE precedent: Kate's plugin-view frame uses analogous nesting. If embedding fails for surprise reasons (e.g. `QMainWindow`-in-`QMainWindow` layout issues), the tractable pivot is inheriting from `KDDW::MainWindow` instead of `KXmlGuiWindow` — we'd re-home KXmlGuiWindow's functionality via `KXMLGUIClient` on the inheritor. Plan only this pivot if Phase 1 smoke fails.

---

## 5. Data flow

### 5.1 Save flow (write `.obsidian/workspace.json`)

1. User performs any mutation (split, pin, tab-select, drag, close, window move).
2. KDDW fires internal signals: `DockRegistry::dockWidgetAdded/Removed`, `DockWidget::parentChanged`, `FloatingWindow::geometryChanged`, `TabBar::currentIndexChanged`, etc. We connect a **debounced 1 s trailing-edge** handler to all relevant ones (matches Obsidian's `requestSaveLayout()` cadence).
3. On fire: `WorkspaceSerializer::toJson()` walks `MainWindow::layout()` in DFS, consulting Corbomite's `leafId → WorkspaceLeaf*` map. Emits Obsidian-shape tree:
   ```json
   {
     "main": { "id": "...", "type": "split", "direction": "vertical",
               "children": [ { "type": "tabs", "children": [ { "type": "leaf", ... } ] } ] },
     "active": "<leaf-id>",
     "lastOpenFiles": [ "...", "..." ],
     "floating": { "id": "...", "type": "floating", "children": [ ... ] }
   }
   ```
4. Routed through existing `SessionManager::setWorkspaceLayout()` — preserves unknown-keys written by Obsidian (post-Cluster-G contract, tested).

### 5.2 Load flow (read `workspace.json` at vault open)

1. `SessionManager` reads `workspace.json` → `Workspace::deserialize(json)`.
2. `WorkspaceSerializer::fromJson()` walks the JSON tree:
   - For each `leaf` node: construct `WorkspaceLeaf` with cached `icon`+`title` (deferred; no view instantiated); allocate `KDDW::DockWidget` with `uniqueName = "{vaultId}:{leafId}"`; attach leaf to dock widget via `setGuestView()`.
   - For each `split` / `tabs` node: issue the right sequence of `MainWindow::addDockWidget(widget, location, relativeTo, InitialOption)` / `addDockWidgetAsTab(widget)` calls to reconstruct topology. `dimension` (flex %) maps to KDDW's width/height post-restore.
3. For each `floating[]` entry: create `KDDW::FloatingWindow`, reconstruct its subtree, restore `{x, y, width, height, maximize}` via `QWindow` APIs. Zoom deferred.
4. `active` id → `WorkspaceLeaf::focus()` trampolines through `DockWidget::setAsCurrentTab()` + `raise()`.
5. Leaves stay deferred until their tab becomes visible/active → `loadIfDeferred()` instantiates the real view.
6. Emit `Workspace::layoutReady()` — plugins subscribed via `onLayoutReady(cb)` receive the signal.

### 5.3 Active-leaf routing

KDDW has no single `focusedDockWidgetChanged` signal. `WorkspaceActiveLeafRouter` composes it:

- Subscribes to `QApplication::focusChanged(old, new)`.
- Walks `new`'s parent chain; if it crosses a `KDDW::DockWidget`, look up the owning `WorkspaceLeaf` in our `dockWidget → leaf` map.
- Identity-gates: if new active-leaf equals current, no signal (matches Obsidian's no-self-refire).
- Null handling: focus left app → keep last active-leaf, no signal (Obsidian semantics).
- Vault-switch: during `closeVault`, suppress signals until next `layoutReady`.
- Emits `Workspace::activeLeafChanged(WorkspaceLeaf*)`. Identical signature to current.

Tested in isolation (`tst_workspace_active_leaf_router`) before live wiring.

### 5.4 Drag / drop

KDDW owns it entirely. We write zero drag code. We subscribe to post-drag signals (`DockWidget::parentChanged`, `FloatingWindow::focusedDockWidgetChanged`, `Group::numDockWidgetsChanged`) and trigger the 1 s serialization debounce. If KDDW's default drop indicators need visual-style tweaks for KDE Plasma theme integration, `DropIndicatorBridge` (~100 LOC, optional) applies them. MVP ships with KDDW defaults.

---

## 6. Error handling & edge cases

1. **Unknown `viewType`** in loaded `leaf.state.type`: `WorkspaceLeaf::setViewState` falls back to `"empty"` view (existing post-Cluster-G behaviour). Unchanged.
2. **Missing file** in `MarkdownView` state: MarkdownView's own "file not found" presentation. Unchanged.
3. **Malformed `workspace.json`** (parse error, schema drift): catch at `WorkspaceSerializer::fromJson()` entry; `qWarning()`; construct default tree `WorkspaceRoot("vertical") > tabs > EmptyView` (matches Obsidian's `{}` handling); open `lastOpenFiles[0]` if present.
4. **Vault switch (Cluster Q.0 lifecycle):** `Workspace::closeAllLeaves()` iterates leaf map and calls `KDDW::MainWindow::closeDockWidgets(/*force=*/true)` **synchronously** (matches decisions-archive 2026-04-19 sidebar-invisible fix: no queued `deleteLater` races). `DockRegistry` is a process singleton — unique-name collision across future multi-vault prevented by `{vaultId}:{leafId}` naming. After close, emit `activeLeafChanged(nullptr)` once.
5. **KDDW layout restore failure** (e.g. referenced `relativeTo` DockWidget missing due to ordering bugs): catch; log; append the orphaned leaf to main-area root's first tab group. Never lose a leaf.
6. **Leaf-close undo** (Ctrl+Shift+T): undo stack (10-cap, existing) captures `{leafId, parent-breadcrumb, state, eState, history, group, pinned}`. On restore: walk breadcrumb to find surviving ancestor; `addDockWidget(location, relativeTo=ancestor, InitialOption{asHidden})`; `setAsCurrentTab()`. Fully-severed breadcrumb → fall back to main-area root (matches Obsidian behaviour — restored leaf may land in a slightly different spot).
7. **Plugin proxy contract**: `WorkspaceController` / `WorkspaceProxy` signatures unchanged. All 8 internal plugins in `src/plugins/` recompile untouched. β adds new methods (`getLeaf`, `getLeavesOfType`, `iterateAllLeaves`, `getActiveViewOfType`, `openLinkText`) without breaking existing ones.
8. **`lastOpenFiles` sibling key** — preserved by existing post-Cluster-G-follow-up-#5 plumbing (done 2026-04-19). Not touched.
9. **Obsidian-vault round-trip** (user opens vault in Obsidian, then Corbomite, then Obsidian): tested by fixture suite (§7). Unknown-keys written by Obsidian are preserved verbatim via `SessionManager`'s unknown-keys retention (Cluster G Phase 6 contract).

---

## 7. Testing strategy

### 7.1 Preserve existing

All 13 current tests:

- `tst_workspace_integration`, `tst_workspace_tree`, `tst_workspace_serialize`, `tst_workspace_tabs`, `tst_workspace_tabs_lifecycle`, `tst_workspaceleaf`, `tst_workspace_deferred`, `tst_workspace_leaf_navigate`, `tst_leaf_history`, `tst_leaf_undo`, `tst_leaf_service_propagation`, `tst_workspace_session`, `tst_workspace_window`, `tst_proxy_workspace`, `tst_vault_switch`.

Most are behaviour-level and pass unchanged after substrate swap. Budget: 2–3 of 13 need rewrites (the ones that poke `QTabBar`/`QSplitter` internals). Rewrites happen in Phase 4 alongside the substrate flip.

### 7.2 New tests

| File | Coverage |
|---|---|
| `tst_workspace_dragdrop.cpp` | Simulate drag via `QTest::mousePress/mouseMove/mouseRelease` on KDDW tab headers; verify leaf reparents; drop to edge creates split; drop to empty area creates floating window |
| `tst_workspace_popout.cpp` | `getLeaf(mode=Window)` creates `WorkspaceWindow`; close window closes children; restore preserves geometry + maximize |
| `tst_workspace_roundtrip_obsidian.cpp` | Load 6–8 real Obsidian `workspace.json` fixtures (nested splits, stacked tabs, floating windows, group bindings, pinned tabs, missing keys, `lastOpenFiles` sibling, unknown keys); assert byte-equivalent re-serialization (modulo unknown-key retention) |
| `tst_workspace_active_leaf_router.cpp` | Router in isolation — mock KDDW, simulate focus changes, assert no self-refire on same leaf, null handling, vault-switch suppression |
| `tst_workspace_serializer.cpp` | Serializer against synthetic KDDW trees — deep nesting, empty tabs, orphaned-leaf recovery |
| `tst_workspace_embed_kxmlgui.cpp` | Phase 1 smoke: KDDW MainWindow embeds inside KXmlGuiWindow; menu actions still reach targets |

### 7.3 Manual QA checklist (codified at verification phase)

- [ ] Obsidian vault opens → layout restores to same topology seen in Obsidian.
- [ ] Tab drag across panes works.
- [ ] Drag to edge splits pane; drop indicator shown.
- [ ] Drag to empty desktop area creates floating window.
- [ ] Close all tabs in a pane → pane dissolves (Obsidian behaviour).
- [ ] Ctrl+Shift+T restores last-closed leaf roughly in place.
- [ ] Popout window drag-back-in works (re-dock).
- [ ] Vault switch leaves clean state (no ghost dock widgets, no stale registry entries).
- [ ] Plugin sidebars (CorbomiteMDI) unaffected by the migration.
- [ ] Round-trip: open vault in Obsidian, mutate layout, open in Corbomite, confirm layout matches; reverse.
- [ ] Wayland-first QA (primary dev env is Manjaro + KWin Wayland). X11 and nested-Xwayland secondary.

---

## 8. Phasing (rough — writing-plans will flesh)

Total: ~2.5 weeks (13–17 days).

| Phase | Scope | Days |
|---|---|---|
| 1 | **Risk reducer:** verify `KDDW::MainWindow` embeds inside `KXmlGuiWindow` with menus intact. `tst_workspace_embed_kxmlgui`. | 1 |
| 2 | Add KDDW dependency (CMakeLists.txt, `find_package(KDDockWidgets-qt6 REQUIRED)`, version pin). Dual-compile gate: hand-rolled substrate still active. | 1 |
| 3 | Build `WorkspaceSerializer` module against synthetic KDDW trees. Obsidian-fixture round-trip tests. No live integration yet. | 2–3 |
| 4 | Flip `Workspace` internals to use KDDW tree. Delete `WorkspaceTabs`/`WorkspaceSplit` as widgets (keep as internal serialization structs). Rewire `WorkspaceLeaf` to compose `DockWidget`. Plugin-API untouched. Rewrite the 2–3 widget-poking tests. | 3–4 |
| 5 | Complete `WorkspaceWindow` atop `KDDW::FloatingWindow`. Popout drag-out / drag-in flows. Geometry + maximize persistence. | 2 |
| 6 | `WorkspaceActiveLeafRouter` + remaining signal plumbing (`layoutReady`, `resize`, `window-frame-change`). | 1–2 |
| 7 | `getLeaf(mode, dir)` factory + `openLinkText()` dispatcher + proxy surface additions (`getLeavesOfType`, `iterateAllLeaves`, `getActiveViewOfType`). Class renames (`rootSplit` alias, new `WorkspaceRoot` / `WorkspaceContainer` / `WorkspaceFloating` / `WorkspaceSidedock` stub). | 2 |
| 8 | Verification: full `ctest` pass; Obsidian-fixture round-trip green; manual QA; plugin-regression against all 8 internal plugins; dev-build smoke. | 2 |

---

## 9. γ deferrals — cross-cluster assignments

Y explicitly does **not** deliver these Obsidian workspace events / APIs, though plugin developers coming from Obsidian expect them. Each is assigned to an owner cluster. **This brainstorm's follow-up sweep updates each target's backlog entry to add a "contributes to Cluster Y γ parity" note.**

| Obsidian event / API | Owner cluster | Status today | Cross-ref target |
|---|---|---|---|
| `file-menu`, `editor-menu`, `leaf-menu`, `tab-group-menu`, `markdown-viewport-menu`, `url-menu` events | **Cluster R** (shipped) + **Markoff C6** (for editor-menu) | R closed 2026-04-19 across 4 phases; some menu sites remain (EditorViewSpace tab bar, TextControl, CorbomiteMDI Sidebar — R follow-up #2 in backlog §2) | R retro + R follow-up #2 backlog entry |
| `hover-link` + `registerHoverLinkSource` | **Cluster H follow-up #6** | Deferred — "build when first plugin consumer demands" | Backlog §2 "Plugin-facing wrappers for hover/suggest surfaces" |
| `active-leaf-change` linked-pane consumers (`receiveSyncState`) | **Cluster Z** | Brainstorm pending (immediately after Y plan lands) | Cluster Z scouting doc (to be written) + backlog §1 Cluster Z entry |
| `quick-preview` debounced editor-content sync | Markoff (no Corbomite cluster) | Not scoped | New backlog item under §3 "Editor, Views, Workspace" |
| `registerObsidianProtocolHandler` (`obsidian://`, `corbomite://`) | New post-parity cluster (no letter yet) | Not scoped | New backlog item under §2 "Plugin API and extension surfaces" |
| Workspaces core plugin (`.obsidian/workspaces.json` named-layout snapshots) | New post-parity cluster (no letter yet) | Not scoped | New backlog item under §2 |
| Mobile layout (`workspace-mobile.json`) | — | Explicitly out of scope (desktop-only project) | No cross-ref |
| `registerEditorExtension` (CodeMirror 6 extensions) | — | N/A — Markoff owns its extension surface; different model | No cross-ref |

**Y *does* ship directly (no deferral):**

- `layoutReady`, `resize`, `window-frame-change` signals.
- `rootSplit` / `leftSplit` / `rightSplit` / `floatingSplit` property surface (the `Sidedock` ones return `nullptr` today; shape present for compat).
- `iterateAllLeaves`, `iterateRootLeaves`, `getLeavesOfType`, `getActiveViewOfType` on `WorkspaceController`.
- `getLeaf(mode, dir)` factory.
- `openLinkText(linktext, source, mode, opts?)` centralised dispatcher.
- `setActiveLeaf(leaf, {focus?})` with Obsidian semantics (identity-gate, no self-refire).
- `eD` (deferred-load), `tD` (empty-view), `nD` (unknown-viewType fallback) — all already done; Y verifies.

---

## 10. Risks

1. **KDDW version pinning.** System library present in build env. Pin minimum version in `CMakeLists.txt` at Phase 2. If a distro ships too-old a version, `find_package(... REQUIRED)` fails clearly; document in project-level build docs.
2. **Active-leaf routing composition.** Subtle. Mitigated by dedicated `WorkspaceActiveLeafRouter` module + isolated unit test; tested before wired live.
3. **Obsidian byte-compat round-trip** (not Corbomite→Corbomite only). KDDW's layout structure might not naturally produce every Obsidian split arrangement. Mitigated by 6–8 real-Obsidian fixture suite at Phase 3 (de-risks before live integration).
4. **KDDW MainWindow inside KXmlGuiWindow.** Expected to work (KDE precedent). Phase 1 smoke test. If surprise failure, tractable pivot: inherit from `KDDW::MainWindow` instead, re-home KXmlGuiWindow functionality via `KXMLGUIClient` on the inheritor.
5. **`DockRegistry` process singleton.** Unique-name collision across future multi-vault. Mitigated by `{vaultId}:{leafId}` naming from day one.
6. **Plugin API subtle breakage.** β opacity means some internal APIs shift. Mitigated by pre-Phase-4 grep across all `libs/` + `src/plugins/` for reaches into `WorkspaceTabs`/`WorkspaceSplit` internals. Expected result: zero outside `libs/core`.
7. **Popout-window drag-back-in platform edge cases.** Most platform-dependent part of drag UX. Phase 5 includes Wayland-first QA (primary dev env), X11/nested-Xwayland secondary.

### Non-risks (worth naming)

- **Qt version** — Qt6 throughout; KDDW's Qt6 branch mature.
- **Licence** — KDDW is dual-licensed (GPL + commercial); Corbomite is GPLv3, no issue.
- **Performance** — KDDW used in commercial CAD/IDE tools with hundreds of docks; ~20-tab realistic load is trivial.

---

## 11. Next step

superpowers:writing-plans session, consuming this scouting doc + the Obsidian audit references, producing a full implementation plan at `docs/superpowers/plans/2026-04-23-cluster-y-workspace-kddockwidgets.md` (dropping the `-SCOUTING` suffix per convention once the plan is dispatchable).

After Y plan lands: Cluster Z brainstorm.
