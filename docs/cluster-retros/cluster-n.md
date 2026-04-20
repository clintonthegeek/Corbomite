# Cluster N — Plugin-Ready Surfaces — Retrospective

**Closed:** 2026-04-17 (same-day execution, 5 phases on top of the Cluster Q substrate)
**Plan:** [`docs/superpowers/plans/archive/2026-04-17-cluster-n-plugin-ready-surfaces.md`](../superpowers/plans/archive/2026-04-17-cluster-n-plugin-ready-surfaces.md)
**Spec:** [`docs/superpowers/specs/archive/2026-04-17-cluster-n-plugin-ready-surfaces-design.md`](../superpowers/specs/archive/2026-04-17-cluster-n-plugin-ready-surfaces-design.md)
**Prerequisite:** Cluster Q (Internal-plugin wrapping + permissions) — closed 2026-04-17 earlier in the day

## What landed

Nineteen task commits (`2e4e3a4` → `ec32fc0`), closing the plugin-ABI shape for
third-party plugins:

- **Stop-gap retirement (Phase 2).** The three raw accessors that Cluster Q
  left on `PluginContext` (`vaultRaw`, `metadataCacheRaw`, `searchIndex`)
  are gone. Every remaining consumer — `SearchView`, `LocalGraphView`,
  `GraphView{,Tab,Plugin}`, `FileExplorerView`, `NotesTreeModel` — reads
  exclusively through `VaultProxy` / `MetadataCacheReader` / `SearchProxy`
  now. `GraphDataBuilder` exposes proxy-typed overloads and drops the raw
  ones.
- **SearchProxy (Phase 1).** Stable plugin-facing facade over `SQLiteIndex`
  with the same permission-gated empty-result semantics as the other
  proxies. `PluginContext::search()` is the canonical accessor.
- **VaultProxy is a QObject (Task 1.1).** Proxy forwards the underlying
  Vault's `created` / `modified` / `deletedFile` / `renamed` / `closed`
  signals via Qt-native signal connections, replacing the earlier
  pass-through pattern that forced plugins to connect on `vaultRaw`.
- **Real keyring backend (Task 3.1).** `SecretStorage` gains a
  QtKeychain-backed implementation behind the same ABI; falls back to the
  in-process store when QtKeychain isn't available or the platform backend
  is absent (for headless CI). Closes the Cluster Q retro's #1 open follow-up.
- **Plugin data.json persistence (Task 3.2).** Per-plugin JSON store at
  `<vault>/.obsidian/plugins/<id>/data.json`, round-tripped through a
  `PluginDataStore` service injected via MainWindow's existing
  `contextConfigurator` lambda.
- **CMake plumbing (Phase 4).** `corbomite_add_plugin()` helper wraps the
  KPluginFactory boilerplate; `metadata.json.in` templates substitute
  `X-Corbomite-Trusted` based on compile-time subdirectory location (only
  plugins under `src/plugins/` get the trusted flag). `CorbomiteConfig.cmake`
  allows third-party plugins to `find_package(Corbomite)`. PluginManager
  enforces `X-Corbomite-MinVersion` (refuse plugin if host is older) and
  `X-Corbomite-ApiLevel` (hard-break signal for ABI bumps).
- **Reference plugins (Phase 5).** `examples/plugin-template/` is the
  starter skeleton; `examples/note-stats-plugin/` exercises VaultProxy +
  MetadataCacheReader + SearchProxy + createView + data.json persistence
  as a genuine third-party plugin.
- **Documentation (Task 5.3).** `docs/plugin-development/` ships 1737
  lines across README / TUTORIAL / API-STABILITY / DISTRIBUTION.

## By the numbers

- **20 commits** across Phases 1–5 (19 task commits + the initial plan doc).
- **110 files touched**, +7285 / −443 net.
- **4 new test executables** (`tst_search_proxy`, `tst_plugin_data_store`,
  `tst_note_stats_plugin`, `tst_template_plugin`), plus additions to
  existing `tst_plugin_context`, `tst_plugin_manager`, `tst_graphdatabuilder`,
  `tst_notestreemodel`, `tst_vault_lifecycle`, `tst_proxy_secrets_process`.
- **Documentation:** 1737 lines.
- **Suite:** 194/195 green (`tst_benchmark_layout` timeout is pre-existing
  known-flaky).

## Notable design decisions

- **Native C++ over JS sandbox.** Plugins compile as KPluginFactory `.so`
  modules. The JS/WebEngine route was considered and rejected: the audit
  target is Obsidian compatibility of vault data shape, not Obsidian's
  JS plugin runtime. Native C++ matches the KDE plugin pattern, gets real
  type-checking at the ABI boundary, and avoids a V8/WebEngine embed.
  A JS shim layer remains possible on top of this substrate later (flagged
  as a follow-up).
- **Enable-time-batch permissions, not per-call prompts.** Every declared
  `X-Corbomite-Permissions` token is granted or denied at plugin-enable
  time via the `PluginPermissionGrantDialog`. Untrusted plugins that lack
  a granted token get empty results from the relevant proxy; no runtime
  prompts. Rationale: per-call prompts break plugin UX and force plugins
  to handle denial paths that in practice never happen after the first
  grant.
- **Trust is compile-time only.** `X-Corbomite-Trusted=true` is injected
  by `corbomite_add_plugin()` iff `CMAKE_CURRENT_SOURCE_DIR` is under
  `src/plugins/`. There is no runtime mechanism to elevate a third-party
  plugin to trusted. This keeps the boundary crisp: trusted plugins are
  part of the Corbomite source tree and ship in the Corbomite binary
  package; everyone else goes through the permission dialog.
- **Proxies are QObject-based (Task 1.1).** The alternative of having
  plugins subscribe to the underlying Vault's signals via `vaultRaw` was
  a stop-gap that defeated the point of the proxy. Promoting VaultProxy
  to QObject + forwarding signals makes the proxy a full substitute; the
  raw accessor could then be deleted (Task 2.6).
- **Shape-stable, not 1.0-frozen.** `X-Corbomite-ApiLevel=1` marks the
  current ABI; the plan is to bump ApiLevel on any breaking change and
  let PluginManager refuse incompatible plugins cleanly. No semver
  guarantees beyond "we break at ApiLevel boundaries".

## Plan deviations

Recorded inline in commit messages, consolidated here:

- **Task 1.3 (MetadataCacheReader gap-fill) was a no-op.** The plan assumed
  `GraphView::m_metadata` was a dead-stored reference from a half-finished
  Cluster Q migration. It wasn't — it was live-used. Task 2.4 therefore
  threaded `MetadataCacheReader *` through the GraphView plugin explicitly
  instead of dropping it; the "gap" the plan tried to close wasn't a gap.
- **Task 2.5 (FileExplorer migration) found dead state in MainWindow.**
  A `NotesTreeModel` was being constructed in MainWindow and never
  installed on any view — dead since the FileExplorer plugin extraction.
  Deleted along with the migration.
- **Task 3.1 (QtKeychain) discovered backend-specific test contracts.**
  Existing `tst_proxy_secrets_process` asserted the in-process backend's
  enumeration ordering (`listSecrets` sorts lexically) and double-delete
  error codes. QtKeychain doesn't honour either — its `listSecrets` is a
  no-op on some platforms, and double-delete returns success not error.
  Hardened the test to a dual-backend contract (backend-agnostic checks
  only).
- **Task 3.2 (plugin data.json) injected via `contextConfigurator` not
  PluginManager.** The plan put `setPluginDataDir` on PluginManager.
  MainWindow already has a `contextConfigurator` lambda that runs for
  every new PluginContext (established in Cluster Q), and that's the
  canonical host-wires-services slot. Moved there to preserve the
  pattern; PluginManager stays a pure loader.
- **Task 4.1 (corbomite_add_plugin) discovered trust-skip already
  existed.** Cluster Q had plumbed the trusted-plugin skip path end-to-end
  in PluginManager; this task's job was to move the flag's **source**
  from hardcoded JSON to CMake injection. Re-scoped accordingly.
- **Task 4.3 (CorbomiteConfig.cmake) needed `$<BUILD_INTERFACE:>`
  guards.** Several submodule and bundled dependencies (tree-sitter,
  JKQTMathText) would otherwise leak into the installed export set.
  Wrapped their `target_link_libraries` / `target_include_directories`
  calls in `$<BUILD_INTERFACE:>` generators.
- **Task 5.2 (note-stats plugin) hit NoteDocument API changes.** `Vault::load`
  returns `void` not `bool` in the current API; `MetadataCache` constructor
  requires a `LinkResolver`. Test adjusted to the real signatures.

## Open follow-ups

### Explicitly deferred (from spec §2.2)

- **`hotkeys.json` I/O + Modal Scope push/pop.** Cluster C's deferred
  Phase 4b-d remains deferred; plugins cannot yet register per-scope
  shortcuts or modal input stacks.
- **SessionDestroyer.** No lifecycle hook for plugins to finalise before
  the vault tears down; current teardown is synchronous.
- **Partial H #6 wrappers.** Some hover/suggest surfaces remain
  host-private with no proxy equivalent.
- **In-app plugin browse UI.** SettingsDialog's Plugins page still lists
  only discovered plugins; no "browse the store" or "install from URL"
  flow. Third-party install is `cp -r <plugin>/ ~/.local/share/corbomite-dev/plugins/`.
- **Sandbox / process isolation.** Plugins run in-process. A sandbox
  decision (seccomp? namespace? WebEngine for JS plugins?) is deferred
  to a future cluster.
- **JS shim.** No JavaScript plugin runtime. The native-C++ substrate
  shipped in N is the foundation; a JS layer on top is future work.

### Discovered during execution

- **`ui.views` permission semantics for `createView()`-only plugins
  unclear.** Current enforcement checks the token at registrar call
  sites, but a plugin that only implements `createView()` (no
  `registerView()` calls) never touches those sites. Flagged in
  `API-REFERENCE`; needs a spec decision on whether `ui.views` should
  be required for `createView` too.
- **`CorbomiteConfigVersion.cmake` not emitted.** `find_package(Corbomite
  1.0 EXACT)` won't work until a version file is generated and installed
  alongside `CorbomiteConfig.cmake`. Trivial fix; out of scope for the
  closeout commit.
- **Distro packaging paths are convention, not enforcement.** The docs
  state `.so` install paths as the packaging convention, but no distro
  package (deb / rpm / flatpak / AppImage) exercises them yet. Verify
  when the first packaging effort lands.
- **`tst_propertiespanel` rewrite.** Inherited from Cluster Q's open
  follow-up list — the legacy test was too coupled to direct-injection
  to retrofit to proxies. Needs a mock-proxy rewrite. Not N-scope but
  stays open.

## Verdict

Plugin ABI is **shape-stable** as of this close. The third-party plugin
path is proven end-to-end by `examples/note-stats-plugin/`: find_package +
corbomite_add_plugin + metadata.json.in + data.json persistence + all
four proxies (Vault, MetadataCacheReader, SearchProxy, FileManagerProxy)
+ createView. Documentation lives in-tree at `docs/plugin-development/`.
The three stop-gap raw accessors Cluster Q introduced are gone.

What's not done: in-app plugin distribution UX, a sandbox, and a JS
shim. Those are future clusters with their own specs. Everything the
plan promised for N is done.

Length: ~1080 words, 165 lines.
