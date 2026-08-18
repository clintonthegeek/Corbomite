# Cluster L — Workspace/KDDW stabilization & nativization

**Date:** 2026-08-17
**Type:** Full plan
**Status:** **L0-L3 landed 2026-08-17. L4 code landed same day but NOT
YET LIVE-VERIFIED** — D1 (title-bar/tab chrome) and D4 (sidebar width
restore) explicitly need a live dogfood pass before being called done;
offscreen-green is not sufficient for this phase (see project memory on
keyboard/focus-change fixes). L5 not started. See `docs/PROJECT-STATE.md`
(Cluster L row) for the one-line current state and
`docs/decisions-archive.md` (dated 2026-08-17, five "Cluster L
Phase..." entries) for full closeout detail per phase, including exact
commit shas and test results. This plan file's phase descriptions below
are left as originally written (the *intent*); do not infer completion
status from this section — check PROJECT-STATE first.
**Source:** Full re-evaluation of the workspace/tab/dock layer (this
session), building on `docs/audit-2026-04-26/workspace.md` (many of whose
top findings have since been fixed — this plan covers what's *still*
live) and the open `[workspace]` punch-list items.
**Depends on:** nothing external. Interleaves fine with Cluster K Phase 4;
Phase L1 (crash-safety) should land **before** K's dogfood-as-default,
since vault open/close is exercised constantly while dogfooding.

---

## 1. The verdict (evaluation summary)

The architecture is **not over-engineered at the top level**: delegating
tab-drag/split/float to KDDockWidgets while keeping an Obsidian-shaped
`Workspace`/`WorkspaceLeaf` facade is the right call, and the worst
2026-04-26 audit findings (nested-split round-trip, per-group currentTab,
serializer consolidation, popout leak, `m_tabGroupOf` drag-lag) were
genuinely fixed. What remains is three specific disease clusters:

1. **Lifetime/teardown fragility** — four hand-rolled, mutually
   inconsistent leaf-destruction disciplines, two of which skip the
   `releaseDockWidget()` dance entirely. This is the crash engine; it is
   the same defect class as the 2026-06-10 first-run SIGSEGV (vault
   teardown UAF).
2. **Split-brain persistence** — two parallel workspace.json writers
   (one test-only), production saves that drop `floating` +
   `lastOpenFiles`, process-global sidecar statics that never clear, and
   Corbomite-private keys (`_corbomite`, base64 Qt blobs) written into
   the Obsidian-shared file where Obsidian will destroy them.
3. **Obsidian-literalism residue** — dead-shell classes kept so
   Obsidian-shaped code "compiles" (`leftSplit()` returning literal
   nullptr is a crash invitation, not compatibility), unde­bounced
   event mirrors, and Kate-MDI sidebar persistence that exists but was
   never wired.

**The compatibility doctrine that resolves the tension** (adopt in
Phase L0): *compatibility lives at the file-format boundary, not in the
object model.* `workspace.json` must round-trip Obsidian's exact schema —
that is the interop contract. Nothing about that contract requires
Corbomite's in-memory shapes, event timing, or teardown order to mimic
Obsidian's JS runtime. The serializer is the adapter; everything behind
it is free to be idiomatic Qt/KDE. Where Obsidian's runtime shape was
copied *speculatively* (for plugins that don't exist yet), the copy is a
liability and gets deleted or stubbed honestly.

---

## 2. Findings register

Severity-ordered. `[PL]` = already on the punch list.

### A. Crash-safety (teardown & signal re-entry)

- **A1.** `Workspace::deserialize` (`libs/core/src/Workspace.cpp:816`)
  does `qDeleteAll(m_leaves)` — synchronous delete, **no
  `releaseDockWidget()`**, while the KDDW MainWindow is alive and the
  widgets are still docked. `~WorkspaceLeaf` then `delete m_dockWidget`
  on a widget KDDW's registry/layout still references, and dock-widget
  destruction emits `isCurrentTabChanged`/`isOpenChanged`/focus events
  into half-cleared Workspace state. This runs on **every vault open
  with a saved session** (audit concern #15, never fixed). Same pattern
  in `resetToDefaultLayout` (`Workspace.cpp:935`).
- **A2.** Four teardown disciplines exist: `~Workspace`
  (snapshot-then-clear + release), `closeLeaf` (unregister +
  `deleteLater`), `deserialize`/`resetToDefaultLayout` (immediate
  `qDeleteAll`, no release), and the `m_kddwMain` `destroyed` hook
  (release-all). Each was written reactively after a crash. There is no
  single primitive that owns "make a leaf and its dock widget die
  safely", so every new call site re-derives the dance and the next
  crash is pre-scheduled.
- **A3.** `wireLeafKddwSignals` lambdas capture raw `leaf` pointers,
  guarded by `m_leavesById.contains(leaf->id())`. The guard depends on
  strict unregister-before-delete ordering (which A1 violates: it clears
  the hash first, then deletes — safe today only by accident of
  ordering). `QPointer`/`QObject::connect` with `leaf` as context object
  would make aliveness structural instead of conventional.
- **A4.** `resetToDefaultLayout` clears leaves/ids/groups/undo but not
  `m_stackedGroups` (deserialize does). Minor state leak across resets.

### B. Persistence split-brain & interop fidelity

- **B1.** Two writers: production =
  `MainWindow::saveSessionState` → `SessionManager::doSave`;
  `Workspace::{read,write}WorkspaceJson` are **test-only** (grep
  confirms: only `tst_workspace_session`, `tst_workspace_serialize`,
  e2e). Production extracts only `main`+`active` from `serialize()` and
  **drops `floating` + `lastOpenFiles`** `[PL P1 audit-2026-06-10]`
  (`MainWindow.cpp:851-861`).
- **B2.** `_corbomite` tail (base64 `windowGeometry`/`windowState`,
  sidebar, plugin state) and `left-ribbon` are written into
  `.obsidian/workspace.json` (`SessionManager.cpp:270-275`). Obsidian
  rewrites workspace.json wholesale on every layout change → these keys
  are destroyed by any Obsidian session (window-geometry amnesia), and
  Corbomite churns a file Obsidian also churns (sync-conflict bait).
  The shakedown item "move Corbomite artifacts out of `.obsidian/`"
  `[PL]` already points here.
- **B3.** Split/tabs nodes are written with **empty `id`** and **no
  `dimension`** `[PL P2 audit-2026-06-10]`
  (`WorkspaceSerializer.cpp` walkLayoutContainer/walkLayoutNode never
  assign; parse drops `dimension` too) → Obsidian re-open loses all
  pane-size ratios.
- **B4.** `leafSidecar()`/`stackedSidecar()` — process-global static
  `QHash`es in the serializer (`WorkspaceSerializer.cpp:34-61`),
  populated on every `parseLeaf`, **never cleared**: unbounded growth
  across vault switches, cross-vault contamination in one process, and
  a test-only fallback path living inside production code paths.
- **B5.** Sidebar persistence is a placebo: `saveSidebarState(visible,
  200, false, 200)` hardcoded (`MainWindow.cpp:850`); KateMDI
  `Sidebar::saveSession/restoreSession` exist and are never called
  `[PL P2]`. Also Corbomite has no live model of Obsidian's
  `left`/`right` subtrees (pass-through-until-dirty, option B — fine,
  but the *Corbomite-native* sidebar state should then be first-class
  in its own store).
- **B6.** `eState.scroll` is a 0.0–1.0 fraction; Obsidian's is a line
  number `[PL P4]`. Internal-only today; becomes an interop bug the
  moment `eState` round-trips.

### C. Cruft / over-engineering to trim

- **C1.** `WorkspaceRoot`/`WorkspaceContainer`/`WorkspaceSidedock`/
  `WorkspaceFloating`/`WorkspaceWindow` — Phase 7.5 bookkeeping shells
  (~130 LOC + 5 headers). `leftSplit()`/`rightSplit()` return literal
  `nullptr` (`Workspace.h:256-257`): any plugin-shaped caller crashes.
  Compiling ≠ compatible. Decide per class: delete, or make the
  accessor honestly absent until Cluster F's sidedock-as-tree work.
- **C2.** `Workspace::{read,write}WorkspaceJson` — dead in production
  (B1). Either they *become* the production path (preferred — they
  already persist full fidelity) or they go.
- **C3.** Duplicated `ensureKddwInit` in `Workspace.cpp:45` and
  `WorkspaceLeaf.cpp:19` — two static guards in two TUs setting the
  same global flags.
- **C4.** `WorkspaceActiveLeafRouter::onFocusChanged` is
  O(chain × leaves) with an `allLeaves()` QVector copy per ancestor
  (`WorkspaceActiveLeafRouter.cpp:32-38`), on **every focus change
  app-wide** including sidebar/dialog focus. Build a
  `QHash<QWidget*, WorkspaceLeaf*>` or early-out when focus isn't
  under the KDDW main window.
- **C5.** Un-debounced Obsidian event mirrors: `Workspace::resize()`
  per QEvent::Resize; `activeLeafChanged` per transition (Obsidian
  debounces both). Cheap `QTimer{0}` coalescing; matters once plugins
  hook these.
- **C6.** `m_tabGroupOf`/`m_stackedGroups`: membership already reads
  live from `DockRegistry::groups()`; the cached id survives only as
  the serializer key + a `stacked` bit KDDW can't render. Keep the
  round-trip (interop) but consider deriving group identity at
  serialize time and deleting the per-leaf cache.
- **C7.** `MainWindow.cpp` is 2921 lines / 66 `connect`s and owns all
  workspace wiring. The decomposition spec already exists
  (`specs/2026-06-10-mainwindow-decomposition-design.md`); this cluster
  should extract only the workspace-host wiring if it gets in the way —
  don't front-load the full decomposition here.

### D. Qt/KDE-native UX gaps

- **D1.** KDDW chrome audit: config sets only
  `AlwaysShowTabs|AllowReorderTabs|TabsHaveCloseButton`. Verify live
  whether groups render a redundant title bar above the tab bar
  (`Flag_HideTitleBarWhenTabsVisible` is *not* set); check
  middle-click-close on tabs (KDE convention), tab-bar styling under
  KDE color schemes, and drop-indicator legibility.
- **D2.** Back/forward completion: `navigateActiveLeafTo` landed
  2026-08-17, but Ctrl+Alt+←/→, mouse buttons 4/5, and per-leaf
  back/forward enablement state remain (road-to-dogfood Phase 3 item;
  `[PL P2]` tab-history now partially closed).
- **D3.** Missing tab commands: `Mod+1..8`/`Mod+9` jump-to-tab,
  pin-tab, move-to-new-window (one-call wrappers over existing
  primitives), toggle-stacked (decide: advisory-only or hide).
- **D4.** Sidebar behavior should feel Kate-like since it *is* the Kate
  port: restore persisted widths/visibility (B5), keyboard toolview
  focus, and the KXMLGUI `Index 18 not within range` merge-index noise
  `[PL P5]`.

---

## 3. Phases

### Phase L0 — Compat-boundary doctrine (half a session) — **DONE**

Landed as `docs/superpowers/specs/2026-08-17-workspace-compat-boundary.md`
(Accepted, Clinton sign-off 2026-08-17). The three-tier model below was
refined during design review beyond this phase's original two-option (a)/(b)
framing — read the spec, not just this paragraph, before touching storage
code. Original phase description follows for context:

Write a short spec (`specs/2026-08-17-workspace-compat-boundary.md`)
stating: schema-at-rest fidelity is mandatory; in-memory Obsidian
mimicry is opt-in per demonstrated need. Record the B2 decision: which
keys are allowed in `.obsidian/workspace.json` (answer: **only**
Obsidian-schema keys) and where native state goes (a per-vault file
under `~/.local/share/corbomite[-dev]/vaults/<id>/session.json`, or
`.obsidian/corbomite.json` if it should travel with the vault — pick
one, document sync implications). Gate: user sign-off on the doctrine.

### Phase L1 — Teardown unification (the crash killer) — **DONE**

Landed via `Workspace::destroyLeaves(QVector<WorkspaceLeaf*>, TeardownMode)`
(`90d994f3` — A1 + A4; `93eb8e01` — A3). ASAN-clean (all 11
`tst_workspace_*` binaries, zero diagnostics) — exit gate met. Live
dogfood-a-session soak not separately done as its own gate; folded into
ongoing use. Original phase description follows for context:

1. Introduce one private primitive, e.g.
   `Workspace::destroyLeaves(QVector<WorkspaceLeaf*>, TeardownMode)`,
   that owns: unregister-first, KDDW signal disconnect/blocking,
   `releaseDockWidget()` where the substrate outlives the leaf, and a
   single delete-vs-deleteLater policy. Route `~Workspace`, `closeLeaf`,
   `deserialize`, `resetToDefaultLayout`, and `detachLeavesOfType`
   through it.
2. Fix A1 concretely: leaves torn down during `deserialize` must have
   their dock widgets removed from the layout (or released) before
   deletion.
3. A3: use `leaf` as the connect context object (auto-disconnect on
   leaf destruction) instead of hash-aliveness checks where possible.
4. A4: clear `m_stackedGroups` in `resetToDefaultLayout`.
5. **Tests:** a vault-switch/reload stress test (deserialize N layouts
   back-to-back offscreen, incl. nested-split and floating fixtures);
   one full open→close→reopen cycle. Run the workspace suite under
   ASAN (`-fsanitize=address` one-off config) and record the result in
   the closeout.

Exit gate: ASAN-clean workspace suite + user dogfoods vault switching
without a crash for a session.

### Phase L2 — One writer, full fidelity — **DONE (one gap punch-listed)**

Landed via `2f5d5760` (B3 id-assignment + B4), `13396015` (B1 + C2 +
denylist + golden fixture), `03511566` (`SessionManager` tier-2/tier-3
split). All sub-items done **except** the `dimension`-round-trip half of
B3: ids are assigned and persist, but a parsed `dimension` does not
survive a live load→save cycle (writer rebuilds the split/tabs tree fresh
from KDDW's `LayoutSaver` dump each save, which has no id/dimension
memory) — filed to `docs/punch-list.md` as `[cluster-l]`, not silently
closed. Golden fixture at
`tests/core/fixtures/workspace-obsidian/15-golden-full-fidelity.json`.
Original phase description follows for context:

1. Make production persistence consume the **full** `serialize()`
   payload: either `SessionManager` stores `floating`+`lastOpenFiles`
   alongside `main`/`active`, or `MainWindow` hands the whole object
   through. Kill B1's data drop.
2. Move `_corbomite` + `left-ribbon` out of workspace.json per the L0
   decision; migrate-on-load from the old location.
3. B3: assign 16-hex ids to split/tabs nodes on write; parse+carry
   `dimension` per child and apply as KDDW relative sizes on
   materialize (KDDW `Layout` supports percentages via
   `InitialOption`/resize post-dock — investigate; if lossy, at least
   round-trip the values unchanged).
4. B4: make the sidecar maps instance state of a serializer context
   object (or delete the test-only fallback and fix the tests).
5. Decide C2: promote `Workspace::writeWorkspaceJson` to the production
   path or delete it.
6. **Golden test:** commit a real Obsidian-authored workspace.json
   fixture (nested splits, dimensions, stacked, floating, sidedocks);
   assert load→save preserves every Obsidian key byte-for-byte modulo
   known-allowed rewrites. This is the interop contract made
   executable.

### Phase L3 — Cruft removal — **DONE (C6 deliberately skipped, C5 partial, eState.scroll left as-is)**

Landed: C1 (`0060cedc` — deleted `WorkspaceRoot`/`WorkspaceContainer`/
`WorkspaceSidedock` + the nullptr-returning `leftSplit()`/`rightSplit()`;
kept `WorkspaceFloating`/`WorkspaceWindow`, which have real popout-window
callers), C3 (`0b3bd503` — consolidated `ensureKddwInit`), C4 (`98204f9b`
— router early-out + single `allLeaves()` fetch per focus change). C5
partial (`2f4cd239` — `Workspace::resize()` debounced; `activeLeafChanged`
deliberately left synchronous — it has real synchronous production
consumers and existing tests assert exact-count emission; debouncing it
is a workspace-wide UX-timing change judged out of scope for this
session). **C6 skipped**: `m_tabGroupOf` turned out to have two live
production uses beyond the serializer key, one on the deferred-tab
materialization path Phase L1 specifically hardened — judged not a
marginal-win case, left alone per the plan's own "err toward not
touching it" guidance. `eState.scroll` (B6) confirmed still accurately
described on the punch list (fraction, not Obsidian's line-number
convention) but not fixed — the value lives in the Markoff submodule's
contract-v2 API, so a real fix needs a Markoff-side change, out of scope
for a Corbomite-only phase. Original phase description follows for
context: C1 shell decision (delete or honest-absence), C3 single init,
C4 router hash, C5 debounces, C6 tab-group cache slimming, `eState.scroll`
(B6) re-anchored to a visual line for future interop. Coordinate C1/C7
with `2026-06-10-release-hygiene.md`'s dead-code purge so nothing is
deleted twice or resurrected.

### Phase L4 — Native UX polish — **Code landed, live verification pending**

Landed: D1 (`9fa87398` — `Flag_HideTitleBarWhenTabsVisible` +
`Flag_ShowButtonsOnTabBarIfTitleBarHidden`, middle-click tab close via
event filter; tab-bar/KDE-palette styling and drop-indicator legibility
not independently fixed — no code bug found, purely visual, can't be
assessed offscreen), D2 (`8fddb2b5` — Ctrl+Alt+←/→, mouse buttons 4/5,
`LeafHistory`-backed enablement), D3 (`0e6f4906` — Ctrl+1..9 jump-to-tab,
pin-tab wrapper, move-to-new-window wrapper, toggle-stacked decided
**advisory-only** since KDDW has no stacked-rendering mode to hook), D4
(`03dd494e` — sidebar pixel-width persistence wired end-to-end; found
Phase L2 had built the tier-2/3 storage but never connected real width
values, only visibility — keyboard toolview focus turned out to already
exist via `GUIClient::registerToolView`'s per-toolview Focus action, no
change needed; KXMLGUI "Index 18" merge-index noise left to its existing
punch-list entry rather than an unscoped archaeology dig). **D1's
title-bar removal and D4's width restore have not been seen rendered
live** — this is the gate before Phase L4 can be marked done, not a
formality. Original phase description follows for context: D1 KDDW flag/chrome pass (live eyeball with user — per memory:
keyboard/focus changes need live confirmation, not just offscreen
green), D2 back/forward completion, D3 tab commands via KStandardAction/
KActionCollection where applicable, D4 sidebar persistence revival
(wire the dormant KateMDI save/restore; delete the hardcoded
200/false). Each lands as its own small commit against the punch list.

### Phase L5 — Soak & closeout — **Not started**

One dedicated dogfood session hammering tabs: drag between groups,
split, popout, close-undo, vault switch, Obsidian round-trip (open the
vault in Obsidian, rearrange, reopen in Corbomite, and back). File
findings `[cluster-l]` to the punch list; Ritual 3 closeout; update
`PARITY-MATRIX.md` workspace rows.

---

## 4. Explicit non-goals

- Sidedock-as-workspace-tree (Obsidian `left`/`right` full modeling) —
  stays with Cluster F; L only makes the pass-through honest.
- MainWindow full decomposition — Phase 5 of road-to-dogfood; L extracts
  nothing unless workspace wiring blocks it.
- Multi-vault-per-process — the vaultId namespacing stays, but no new
  work.
- `workspace-mobile.json`, `zoom`, frameless-titlebar behaviors — N/A.
