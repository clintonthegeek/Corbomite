# MainWindow decomposition — design

**Date:** 2026-06-10
**Status:** Approved-for-planning (audit-derived)
**Cluster:** Architecture hygiene (source: 2026-06-10 code-quality audit, god-class finding #1). Sequenced against the Markoff contract-v2 adoption brief (`~/dev/Markoff/docs/handoff/2026-06-09-corbomite-api-adoption-brief.md`).

## Problem

`src/app/MainWindow.cpp` is a confirmed god class: **2,778 cpp lines + 266 header lines,
38 member variables, 64 `connect()` calls, 106 `#include`s, ~24 TODO/FIXME lines,
~18 distinct responsibilities** (vault lifecycle, action setup, editor dispatch, plugin
hosting, theme mapping, session restore, status bar, suggesters, ribbon, popover, …).
Verified hot spots (line numbers as of 2026-06-10):

- **`onVaultOpened` (2160–2438, ~280 lines) / `onVaultClosed` (2440–2520)** manually
  `delete`/`new` **12 per-vault service objects** in a fragile hand-maintained order:
  Vault, AutosaveReactor, LinkResolver, MetadataCache, FileManager, SQLiteIndex,
  SessionManager, TemplateService, DailyNoteService, plugin VaultConfig, popover
  resources (`VaultScopedResources`), plus the RibbonStateController rebind. Teardown
  order is load-bearing (FileManager before MetadataCache, plugins before everything)
  and documented only in comments.
- **Known bug riding in this blob:** `m_linkResolver` is populated once at open via
  `setVaultPaths` (2199–2208) and **never updated** — the `Vault::created/renamed/
  deletedFile` bridges (2274–2313) refresh only MetadataCache, so links to notes
  created/renamed/deleted during the session resolve stale until reopen.
- **`setupActions` (1202–1657, ~455 lines)** — one function building every action,
  including the editor block whose lambdas switch on leaf type
  (BasesView / Source / Live) per verb.
- **Plugin host callbacks** — `rewirePluginCoreServices` (874–927), `hostPluginView`
  (929–988), `releasePluginView` (990–1019), ~145 lines, with **two near-duplicate
  `setCoreServices`/`setExtensionRegistries` blocks** (879–891 vs 906–916) that have
  already drifted once and must be edited in tandem.
- **Theme/appearance.json mapping inline in `onVaultOpened`** (2358–2385): Obsidian
  `moonstone`/`obsidian`/custom-name → internal theme-name translation.

Every new feature pays a comprehension tax here, and the per-vault raw-pointer churn is
the single most likely source of teardown-order regressions.

## Goals

- Extract four cohesive controllers/owners out of MainWindow, in priority order, each
  landable as an independent commit with the test suite green.
- Replace raw `delete`/`new` per-vault churn with RAII (`std::unique_ptr`) and a single
  build/teardown sequence in one place.
- Fix the LinkResolver-freshness bug as a natural consequence of giving the index
  bridges a real owner.
- Dedupe the twin plugin-wiring blocks.
- Bundle the naming fix: `Corbomite::MarkdownView` (`src/editor/MarkdownView.h`) →
  `MarkdownFileView` — it collides with `Markoff::MarkdownView` in the same TUs.

## Non-goals

- **No behavior change.** Pure extraction; mechanical moves with mechanical renames.
  (Exception: the LinkResolver-freshness fix, called out explicitly as a bug fix.)
- **No dir-level renames** (`src/app/` layout etc.) — tracked in the release-hygiene plan.
- No KXmlGuiWindow/action-framework redesign; `actionCollection()` ownership and the
  XMLGUI client identity **stay in MainWindow** (KF6 requires the KXmlGuiWindow to own
  its KActionCollection).
- No plugin-API surface change; `PluginContext` signatures untouched.
- Not touching `setupEditor` (1659+), sidebars, status bar in this pass — they are
  smaller and can follow the same pattern later if warranted.

## Design

All controllers are **plain QObjects, children of MainWindow** (QObject parent
ownership), living in `src/app/`. MainWindow keeps its public accessors
(`vaultObj()`, `fileManager()`, `commandRegistry()` — e2e tests use them) and forwards
to the controllers. Per the INVARIANTS discipline (a new owner retires the old one in
the same plan), each step names the authority it retires.

### Step 1 — `VaultSessionController` (highest value)

Owns the per-vault service graph as `std::unique_ptr` members: Vault, AutosaveReactor,
LinkResolver, MetadataCache, FileManager, SQLiteIndex, SessionManager, TemplateService,
DailyNoteService, plugin VaultConfig, popover resources. One `build(const QString
&vaultPath)` and one `teardown()`, encoding today's construction/destruction order in
exactly one place; `teardown()` is just members resetting in declaration-reverse order
plus the explicit pre-steps (plugin disable, workspace reset). Ribbon rebind stays a
call-out to MainWindow's `m_ribbonState` (ribbon is window-scoped, not vault-scoped).

Also moves here:

- The **Vault→MetadataCache signal bridges** (2260–2313: `noteSaved`, `created`,
  `modified`, `deletedFile`, `renamed`) — connected once inside `build()`.
- The **LinkResolver-freshness fix**: the same `created`/`renamed`/`deletedFile`
  handlers additionally update `m_linkResolver`'s path set. This is the one
  deliberate behavior change; it gets its own test.

MainWindow's `onVaultOpened`/`onVaultClosed` shrink to: UI chrome (central stack,
sidebars, window title, recent vaults), `controller->build()/teardown()`, then the
re-wiring fan-out (suggesters, popover, plugin rewire, session-driven workspace
restore — restore stays in MainWindow this pass since it touches KDDW/Workspace).
Exposes `vault()`, `fileManager()`, etc. accessors plus `vaultOpened()`/
`vaultClosed()` signals for the fan-out.

**Retires:** MainWindow as owner of the 12-object graph and of the index bridges.

### Step 2 — `EditorActionController` (gated on Markoff contract v2)

`setupActions`' editor block (format verbs, heading levels, callout/table insertion,
undo/redo dispatch, `cycleEditorMode`), `refreshEditorActions`, `triggerEditorAction`,
and the per-leaf dispatch helpers. **Deliberately sequenced AFTER the contract-v2
adoption** (per the 2026-06-09 brief): v2 collapses the per-leaf-type switches into
base-pointer `Markoff::MarkdownView` calls, so the controller is born small instead of
inheriting today's switch ladders. It receives `actionCollection()` from MainWindow and
registers actions into it — collection ownership does not move.

**Retires:** the editor third of `setupActions` and the leaf-type dispatch lambdas in
MainWindow. Bundle the `Corbomite::MarkdownView` → `MarkdownFileView` rename into this
step (same files are being touched; the collision is worst in exactly these TUs).

### Step 3 — `PluginHostController`

`hostPluginView`, `releasePluginView`, `rewirePluginCoreServices`, and
`m_hostedPluginViews`. The twin wiring blocks collapse into one private
`wireContext(PluginContext *ctx, const QString &pluginId)` called from both the
configurator lambda and the rewire loop. Needs a thin seam to MainWindow's
`createToolView` (CorbomiteMDI) — passed as the parent window reference; tool-view
creation/destruction semantics (synchronous delete to avoid id collisions) preserved
verbatim with their comments.

**Retires:** MainWindow as plugin-view host and the duplicate wiring blocks.

### Step 4 — Theme/appearance mapping → `ThemeService`

The `appearance.json` → internal-theme-name mapping (2358–2385) becomes
`ThemeService::applyVaultAppearance(const QJsonObject &)` (or equivalent), called from
the vault-open fan-out. **Caveat:** the ThemeService body is currently `#if 0`-disabled
(`libs/core/src/ThemeService.cpp:7`) with stubs outside the guard — this step must
coordinate with the ThemeService revival; if revival hasn't happened, the method lands
on the stub surface and the mapping logic moves verbatim.

**Retires:** the inline mapping in `onVaultOpened`.

## Alternatives considered

- **One mega "MainWindowController".** Rejected: moves the god class sideways without
  separating lifecycles (per-vault vs per-window vs per-plugin).
- **Vault services as a passive struct (`VaultContext`) without a QObject.** Rejected:
  the index bridges need `connect()` receiver lifetime; a QObject controller gives
  automatic disconnect on teardown, which is precisely the churn we're killing.
- **Doing EditorActionController first** (it's the biggest single function). Rejected:
  extracting before contract v2 means moving ~200 lines of leaf-type switches that v2
  deletes weeks later — double work, double review.
- **Service locator / DI container.** Rejected as over-engineering for one window; the
  project convention is explicit constructor/setter wiring.

## Risks

- **Hidden ordering dependencies in teardown.** The current order is comment-documented
  only; a mechanical move could silently reorder. Mitigation: Step 1 transcribes the
  exact current sequence, with the comments, and the vault open/close e2e test runs
  open→close→reopen cycles.
- **Contract-v2 slippage** stalls Step 2. Acceptable: Steps 1/3/4 are independent of it
  and can land in any order relative to each other.
- **ThemeService `#if 0` state** makes Step 4 partially blind. Mitigation: keep the
  mapping logic byte-identical and covered by the existing theme-switch behavior; if
  revival lands first, fold in.
- **e2e tests reaching into MainWindow internals** (offscreen wiring fallbacks per the
  header comment). Mitigation: keep the public accessors stable as forwarders; grep
  tests for `MainWindow::` member access during planning.

## Test strategy

- **Baseline:** `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -j 10` — 250/251
  (excl. `benchmark` label; the known `tst_metadataparser` failure is Markoff-pin-gated).
  Each step must hold this baseline; no step lands red.
- **Step 1:** new `tst_vault_session_controller` — build/teardown idempotence,
  reopen-cycle (open→close→open same vault), teardown-order assertions via QObject
  destroyed-signal spies for the FileManager-before-MetadataCache constraint. Plus a
  regression test for the LinkResolver fix: create/rename/delete a note post-open,
  assert resolution reflects it.
- **Step 2:** existing action-wiring/e2e tests (`onEditorContextChanged` is public for
  exactly this) re-pointed at the controller; no new behavior to test beyond green.
- **Step 3:** existing plugin host/lifecycle tests; add one assertion that configurator
  wiring and rewire-loop wiring produce identical context state (the dedup proof).
- **Step 4:** existing theme tests + one table-driven mapping test
  (`moonstone`→Light, `obsidian`→Dark, custom-name passthrough, absent→Follow system).
- **Manual:** one vault open/close/switch smoke per step on the live build (offscreen
  can't see KDDW visuals).

## Definition of done

Each step: extraction committed separately, baseline green, retired authority named in
the commit message, INVARIANTS-style ownership note in the controller header.
Whole spec: MainWindow.cpp under ~1,400 lines, zero raw `delete` of vault-scoped
services, single plugin-wiring block, `MarkdownFileView` rename complete, closeout in
`decisions-archive.md`, PROJECT-STATE updated.
