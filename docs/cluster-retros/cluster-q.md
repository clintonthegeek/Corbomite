# Cluster Q — Internal-Plugin Wrapping + Permissions — Retrospective

**Closed:** 2026-04-17 (overnight session, 8 plugins + infrastructure)
**Plan:** [`docs/superpowers/plans/2026-04-16-cluster-q-internal-plugin-wrapping.md`](../superpowers/plans/2026-04-16-cluster-q-internal-plugin-wrapping.md)
**Spec:** [`docs/superpowers/specs/2026-04-16-cluster-q-internal-plugin-wrapping-design.md`](../superpowers/specs/2026-04-16-cluster-q-internal-plugin-wrapping-design.md)
**Prerequisite:** Cluster Q.0 (Vault architecture refactor) — closed 2026-04-17

## What changed vs the original plan

Tasks 1–6 (PluginMetaData, Plugin base, PluginContext, PermissionGrantDialog,
PluginManager discovery + load) had landed earlier. Tasks 7 (Vault proxies)
landed inside Q.0 Phase 9. Tasks 8 (MetadataCacheReader + WorkspaceController
wire-up) was already done. This session executed Tasks 9–21:

- **Task 9** wired CommandRegistrar / ViewRegistrar / MenuInjector with
  auto-cleanup on destruction (`tst_proxy_ui` covers 14 cases).
- **Task 10** wired SecretStorage (in-process per-plugin namespaced QHash;
  persistent KWallet/QtKeychain backend deferred — see follow-ups) and
  ProcessSpawner (QProcess wrapper with qCDebug logging tagged by plugin id).
- **Task 11** built the Plugins page in SettingsDialog with discovered-plugin
  list + per-plugin enable checkbox + permission review (read-only for trusted
  plugins, editable for untrusted).
- **Task 12** wired PluginManager into CorbomiteApp at startup (discovery
  only) and into MainWindow's vault-open lifecycle (enable + setCoreServices
  via PluginManager::setContextConfigurator).
- **Task 13** delivered the canonical Backlinks plugin migration: source
  files moved verbatim under `src/plugins/backlinks/`, KPluginFactory wrap,
  metadata.json with permissions and dock placement, build-tree plugin
  output dir + CorbomiteApp dev-mode setSystemSearchPath.
- **Tasks 14–19** repeated the canonical pattern for Outlinks, Outline,
  Properties, Search, FileExplorer, LocalGraph (six panels, six commits).
- **Task 20** shipped GraphView as a *shell* plugin — see deviations below.
- **Task 21** (this commit): retro + PROJECT-STATE + INDEX + memory.

The substrate gained one infrastructure refactor not in the original plan:
**libvault flipped from STATIC to SHARED.** `qobject_cast<Plugin *>` across
the host/.so boundary fails when each binary has its own copy of `Plugin`'s
metaobject. Building libvault as `.so` makes the metaobject single-instance,
matching the standard KDE plugin pattern. This unlocks all eight migrations
in one swoop.

## What surprised

The scariest landmines were silent:

- **`KPluginFactory::create<T>()` doesn't walk inheritance.** It looks up
  registered classes by exact metaobject class-name. `registerPlugin<
  BacklinksPlugin>()` registers under `"Corbomite::BacklinksPlugin"`;
  `create<Plugin>()` asks for `"Corbomite::Plugin"` and returns nullptr.
  The fix is `factory->create<QObject>(this)` followed by `qobject_cast<
  Plugin *>` — works because the cast walks the metaobject chain (now
  single-instance after the SHARED flip).
- **`K_PLUGIN_CLASS_WITH_JSON` token-pastes `<class>Factory`** which can't
  survive `Corbomite::BacklinksPlugin`. Switched every plugin to
  `K_PLUGIN_FACTORY_WITH_JSON(<Name>Factory, "metadata.json", registerPlugin
  <Corbomite::<Name>>();)` at global scope.
- **KConfig stale entries break "default-on" semantics.** `disablePlugin`
  wrote `Enabled=false` to KConfig on every vault-close teardown, meaning
  the next vault open silently kept plugins disabled. Fixed with a
  `bool persist=true` parameter on `disablePlugin` — vault-lifecycle
  teardown calls with `persist=false`. Lost an hour to this.
- **Stale plugin contexts across MainWindow lifetimes** crashed
  `tst_e2e_gui::testCleanShutdown` because a recreated MainWindow
  inherited the previous one's loaded-plugin instances, which still
  pointed at the dead Workspace. Fix: MainWindow constructor disables
  every existing instance with `persist=false` before connecting
  pluginLoaded/pluginUnloading. Plugins reload cleanly per MainWindow
  lifetime now.
- **MetadataCacheReader couldn't stay in libs/core.** Making it a QObject
  with method-pointer connect requires `MetadataCache::staticMetaObject`
  at link time, which forced every test that linked Corbomite::Core
  (without Corbomite::Storage) to fail. Moved the proxy to libs/storage —
  cleaner architecturally anyway since it co-locates with its underlying
  service. Plan still references `libs/core/include/.../proxies/
  MetadataCacheReader.h` — a follow-up doc edit should reconcile.

What did *not* surprise: the per-plugin migrations themselves were
mechanical once the canonical pattern landed. Tasks 14–19 each took
~5 minutes of structural work; the meat was always the active-leaf
subscription wiring.

## Downstream effects

- **Cluster N (Plugin-ready surfaces)** scope shrinks substantially.
  All eight built-in panels are now plugin-shaped with permission-gated
  proxies; what's left for N is distribution-UX (plugin browsing UI for
  third-party plugins) + sandbox-decision (process isolation strategy).
- **Cluster M (Internal-plugin feature audits — Graph, Canvas)** is now
  trivially actionable for Graph (the .so exists, even if shell-only).
  Canvas is still in CorbomiteApp.
- **The 12-token permission model is real.** Plugins declare X-Corbomite-
  Permissions in metadata.json; trusted plugins auto-grant, untrusted
  ones go through PluginPermissionGrantDialog. Permission gating is
  declarative-contract not runtime-sandbox — documented as such.

## Lessons for the next cluster

- **For panel migrations: the bottleneck is always service-injection
  refactoring.** Every panel had `setVault / setMetadataCache /
  setCurrentNote` pattern with externally-driven state. Replacing those
  with proxy-driven subscription took longer per panel than the actual
  KPluginFactory wiring. A panel that uses Vault* directly (rather than
  via abstraction) costs ~5 mins; a panel that uses NoteDocument*
  directly with edit-source-of-truth semantics (Properties) costs
  ~30 mins to refactor properly.
- **Prefer SHARED libraries early when plugins exist.** The
  qobject_cast-across-static-libs failure took ~30 min to diagnose
  (looked like a metadata-format issue first, then a
  registerPlugin-class-name issue). Marking libvault SHARED resolved
  it. For future libraries that will host plugin-base classes, default
  to SHARED.
- **Document deferred-state-persistence carefully.** The FileExplorer
  plugin lost SessionManager's expandedFolders round-trip (host had no
  way to query the plugin-internal model). The Outline plugin lost
  scroll-to-line in editor (no editor accessor on WorkspaceController).
  Properties tests went away (too coupled to the old direct-injection
  shape to retrofit quickly). All three are documented as Cluster Q
  follow-ups; closing them needs a richer per-plugin host-callable
  interface (e.g., `Plugin::saveSessionState(QJsonObject &out)`,
  `WorkspaceController::goToLine(activeLeaf, line)`).
- **Plan deviations were honestly recorded inline in commit messages.**
  The pattern works: every commit that diverged from the plan called it
  out with **PLAN DEVIATION** + reason + follow-up. The retro just
  consolidates. Future clusters should keep this discipline.
- **GraphView shell-only is a real follow-up.** Main-area-view-type
  registration via ViewRegistrar would let a real GraphViewPlugin own
  the view tree. Estimated ~hour to ~half-day depending on whether
  GraphControlsPanel comes along.

## Cluster Q follow-ups (deferred — see PROJECT-STATE)

1. **Real keyring backend for SecretStorage** (KWallet via KF6::Wallet
   or QtKeychain). Current backend is in-process per-plugin QHash —
   loses secrets on app exit.
2. **GraphView main-area view-type registration.** Move GraphView,
   GraphViewTab, GraphControlsPanel into the plugin .so; register
   "graph" view type via ViewRegistrar in onLoad.
3. **WorkspaceController::goToLine(activeLeaf, line)** for the Outline
   plugin's scroll-to-heading regression.
4. **SessionManager round-trip for FileExplorer expanded folders.**
   Add `Plugin::saveSessionState` / `loadSessionState` virtuals (or
   per-plugin KConfigGroup helpers) so plugins can persist UI state.
5. **Rewrite tst_propertiespanel against PropertiesView with mock
   proxies.** End-to-end coverage works via e2e suite for now.
6. **Reconcile Cluster Q plan with MetadataCacheReader move.** Plan
   still says `libs/core/include/.../proxies/MetadataCacheReader.h`;
   actual location is `libs/storage/include/.../proxies/`.
7. **Dedicated SearchProxy** wrapping SQLiteIndex's compiled-search
   API. Currently SQLiteIndex is exposed directly via
   `PluginContext::searchIndex()` gated on `metadata.read` —
   stop-gap.
8. **Vault* / MetadataCache* raw exposure via `vaultRaw()` /
   `metadataCacheRaw()`** is also a stop-gap. Right design is
   richer proxies (e.g. TreeModelProxy wrapping NotesTreeModel
   without exposing Vault*).
9. **`focusSearchInput` shortcut** lost when SearchPanel migrated.
   Plugin tool view is reachable but the inner QLineEdit isn't from
   the host — needs a `Plugin::focus(activeChild)` virtual.
10. **Per-plugin tests for the seven undocumented plugins.**
    Only Backlinks shipped with `tst_backlinks_plugin`. The other
    six should follow the same pattern (mock context with permissions,
    assert createView returns / fails appropriately).

Length: ~830 words.
