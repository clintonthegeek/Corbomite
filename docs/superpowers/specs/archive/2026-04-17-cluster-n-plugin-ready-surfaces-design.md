# Cluster N — Plugin-Ready Surfaces — Design Spec

**Date:** 2026-04-17
**Status:** Design approved; awaiting implementation plan (writing-plans).
**Cluster designation:** Cluster N (Plugin-ready surfaces).
**Prerequisite clusters:** Q (Internal-plugin wrapping + permissions) and
Q.0 (Vault architecture refactor). Both closed 2026-04-17.
**Supersedes / builds on:** the stop-gap escape hatches
`PluginContext::vaultRaw()`, `PluginContext::metadataCacheRaw()`, and
`PluginContext::searchIndex()` introduced in Cluster Q to unblock the
eight built-in InternalPlugins without first designing stable proxy
surfaces. These accessors are explicitly documented in their own header as
"stop-gap pending a dedicated SearchProxy" — this spec is the instrument
that retires them.

---

## 1. Problem statement

Cluster Q shipped the foundational plugin-loading infrastructure: an
abstract `Plugin` base class, a `PluginContext` carrying granted
permissions, a `PluginManager` that discovers `.so` modules via
`KPluginFactory`, a permission system with 12 declarative tokens, an
enable-time `PluginPermissionGrantDialog`, and eight built-in panels
(Backlinks, Outlinks, Outline, Properties, Search, FileExplorer,
LocalGraph, GraphView) migrated into `src/plugins/<slug>/` as real `.so`
plugins. Q's retro lists four open follow-ups; this cluster addresses
three of them head-on and defers the fourth with rationale.

Three concrete problems remain:

1. **The plugin ABI is not actually stable.** `PluginContext` hands
   built-in plugins raw `Vault *`, `MetadataCache *`, and `SQLiteIndex *`
   pointers via `vaultRaw()` / `metadataCacheRaw()` / `searchIndex()`.
   These accessors exist because four of the eight built-ins
   (LocalGraph, FileExplorer, GraphView, Search) need to pass object
   pointers into widget constructors that were designed against the
   internal types. No capability is being privileged through these —
   every call the plugins make against the raw pointers is also
   expressible through existing proxy surfaces (or trivially so with
   minor additions). But their presence pins the plugin ABI to the
   internal schema of `SQLiteIndex` and to the exact signal set of
   `Vault`, making future schema evolution a breaking change. A
   third-party plugin author inspecting `PluginContext.h` today would
   see two APIs: a "real" proxy surface and a "raw" escape hatch. That
   is not a shippable contract.

2. **No proof-of-concept third-party path.** Every plugin in the tree
   today is built into the Corbomite source, compiled with the host, and
   installed from the host's build tree. The "third-party" scenario —
   independent developer, own `CMakeLists.txt`, own release cadence,
   distro package, dropped into `/usr/lib/corbomite/plugins/` by the
   package manager, discovered at vault open, permission-prompted on
   first enable, loaded, run — has never been exercised end-to-end. A
   first-class plugin story requires that somebody outside the
   Corbomite repo be able to follow a documented procedure and end up
   with a working plugin.

3. **Supporting infrastructure is stubbed.** `SecretStorage` stores
   secrets in an in-process `QHash` with a "TODO: KWallet/QtKeychain"
   comment — fine for tests, useless for a real plugin that wants to
   remember an API token across restarts. `PluginContext::config()`
   returns a `KConfigGroup` which is appropriate for Corbomite-native
   per-plugin preferences, but Obsidian's plugin-data-persistence
   convention (atomic JSON at `.obsidian/plugins/<id>/data.json`)
   doesn't exist, so plugins ported from Obsidian have no analog home
   for their `loadData()` / `saveData()` calls. And there is no load-time
   check of `X-Corbomite-MinVersion`, so an older Corbomite silently
   loading a plugin built against a newer ABI is currently a crash
   waiting to happen.

This cluster closes all three. It does not attempt to build a plugin
marketplace, a sandbox, a JavaScript host, or any surface whose consumer
does not yet exist.

---

## 2. Goals and non-goals

### 2.1 Goals

1. **Retire the stop-gap raw-pointer accessors from `PluginContext`.**
   Every internal plugin should be expressible through permission-gated
   proxies only. Third-party plugins must be too, by construction, since
   they have no other access.
2. **Commit to a stable plugin ABI.** Document the contract (what
   proxies exist, what permissions gate what, what signals fire when),
   commit to a 12-month stability window from 1.0, and enforce
   `X-Corbomite-MinVersion` at plugin load.
3. **Prove the third-party path end-to-end.** A reference plugin,
   developed in-tree but built as a separate CMake target with its own
   installable artifact, shipped alongside a documented distribution
   procedure. Someone should be able to follow the `README.md` in
   `examples/` and have a working plugin.
4. **Replace stubbed persistence with real persistence.** `SecretStorage`
   backed by QtKeychain. Plugin `data.json` under
   `.obsidian/plugins/<id>/` backed by atomic `QSaveFile` I/O.

### 2.2 Non-goals (explicit deferrals, each with rationale)

Each deferral below is gated on a specific consumer showing up. The
rationale exists because several of these are audit-flagged or
retro-flagged and their absence from this cluster would otherwise read
as an oversight.

1. **Hotkey customization via `.obsidian/hotkeys.json`.** The primitive
   (`libs/core/Hotkey.h`, Cluster C output) exists. Building the file
   I/O now would create dead code — no current plugin surface exposes
   user-configurable hotkeys. Build when the first third-party plugin
   wants one.

2. **Modal/Menu `Scope` push/pop.** QuickSwitcher and KCommandBar today
   handle their own Esc dispatch; no plugin-provided Modal exists yet
   to demand a stackable `Scope`. Deferred to when the first
   plugin-provided Modal lands.

3. **SessionDestroyer / per-vault plugin-state rebuild.** Today's vault
   switch tears down plugins at `onVaultClosed` with `persist=false`
   (preserving user's enable/disable choices in KConfig) and reloads
   them on `onVaultOpened`. Per-vault KConfig isolation is a Kate-style
   concern that matters only once plugins start writing vault-scoped
   state outside of the existing `.obsidian/` conventions. Deferred.

4. **H follow-up #6 (plugin-facing wrappers for HoverLinkSourceRegistry,
   EditorSuggestManager, RibbonSlot, MenuEventEmitter).** These four
   registries are already reachable through existing `PluginContext`
   proxies (`views()`, `menus()`, `commands()`). Dedicated wrappers are
   a polish pass — build only the wrappers the reference plugin needs;
   defer the rest until a consumer demands them. This follow-up gets
   downgraded from "Cluster N work" to "ongoing plugin-surface polish"
   in PROJECT-STATE.

5. **In-app plugin discovery / browse / install UI.** Distribution is
   via distro package managers. Corbomite's role is to discover what's
   already on disk, present it in `PluginsPage`, and gate it behind a
   permission dialog. No `curl | sh`, no plugin store, no auto-update.
   (The design is compatible with a future plugin store built as a
   separate feature; we just don't build it now.)

6. **Runtime sandboxing (process isolation, seccomp, WASM, etc.).**
   Explicit design decision: plugins run in-process as native code.
   Security is an ecosystem-layer concern (distro packaging review, code
   inspection, reputation). The permission system is a declaration-of-
   intent honesty layer, not an enforcement layer. This matches
   Obsidian's model and keeps the plugin ABI straightforward.

7. **JavaScript plugin host.** A future plugin could, in principle,
   embed V8 or QtWebEngine and offer a JS-shaped plugin API that
   translates to our C++ proxies. That plugin (if anyone builds it)
   would ship through the same distribution mechanism as any other
   third-party plugin. It is not an N deliverable and is not being
   designed for here.

---

## 3. Design principles

### 3.1 One plugin ABI, not two

Internal and third-party plugins differ in exactly one way: a
compile-time-only `X-Corbomite-Trusted` flag suppresses the one-time
enable dialog for built-ins. Their capabilities, API surface, permission
tokens, lifecycle hooks, and context objects are identical.

**Rationale:** maintaining two APIs (a "full" internal one and a
"reduced" third-party one) was considered and rejected. The stop-gap raw
accessors look like such a split but aren't — they're ergonomic
shortcuts that let built-in plugins pass internal-typed pointers into
widgets. Formalizing the split would be permanent: it would preclude
internals from ever becoming packageable as third-party plugins, and it
would make porting an Obsidian plugin harder than porting a Corbomite
one. Symmetry keeps the surface honest and keeps the internal plugins
serving as reference implementations for third-party developers.

### 3.2 Trust is a compile-time property of the host, not a runtime
       policy

The `X-Corbomite-Trusted` flag is injected into `metadata.json` at
Corbomite-tree build time via CMake property, specifically for plugins
under `src/plugins/`. A third-party plugin's CMake cannot set it.
There is no user-facing "mark this plugin trusted" toggle, no signed
trust root, and no install-path-based heuristic.

**Rationale:** the trust flag exists to suppress one dialog. A user
who disagrees with our default for a third-party plugin can click
through that one dialog. Building a user-facing trust toggle — or a
signing system, or install-path detection — is strictly more work than
the dialog it would skip, and each of those mechanisms introduces a
new attack surface (e.g., "a plugin that flips itself trusted"). The
simplest mechanism that conveys the intended default wins.

Consequence: if we ever want a "silent-install plugin" UX (e.g., for
enterprise deployments), it would be done by an entirely separate code
path (e.g., a KIOSK-style config override), not by extending the trust
flag. That's fine.

### 3.3 Permissions are enable-time batch, granted all-or-nothing

When a non-trusted plugin is first enabled, `PluginPermissionGrantDialog`
(already exists from Cluster Q) displays its full declared permission
set in one modal. The user accepts all or cancels the enable. Granted
permissions persist in KConfig for the plugin's enabled lifetime;
disabling and re-enabling re-prompts.

**Rationale:** we chose enable-time batch over per-first-use prompts.
Per-first-use is more "honest" in the sense that the user sees *when*
a permission is exercised, but in practice it produces a dialog storm on
every new plugin + a tendency toward muscle-memory "allow". A single
batch prompt at enable time forces the user to make the decision once,
deliberately, with the full context of "I'm about to enable this plugin
and it wants X, Y, Z". This matches what Q already ships and matches
Obsidian's model.

Consequence: the declared permission set in `metadata.json` is the
contract. A plugin that wants to access something it hasn't declared
gets `nullptr` / `false` / empty — it should fall back gracefully or
document the requirement.

### 3.4 API stability is a published contract, not an implementation
       detail

From this cluster's landing onward, the plugin ABI is a public surface
under a documented stability commitment:

- Additive changes (new permissions, new proxy methods, new registry
  types) are always allowed.
- Breaking changes (removing methods, renaming, changing semantics)
  require a 12-month deprecation window: the deprecated surface must
  continue to work, emit `qCWarning`, and be documented as such.
- Plugins declare `X-Corbomite-MinVersion` (Corbomite host version they
  were built against). `PluginManager` refuses to load a plugin whose
  minimum exceeds the host version, with a clear message in
  `PluginsPage`.
- An `X-Corbomite-ApiLevel` integer in plugin metadata optionally hard-
  gates: plugins declaring a level higher than the host's published
  level are refused. This gives us one bit of version break-glass if
  some future breaking change can't wait out the deprecation window.

**Rationale:** this is the difference between "we have a plugin API" and
"a third-party can bet on our plugin API". The 12-month window matches
Obsidian. The `ApiLevel` bit is cheap insurance against a hypothetical
future where some security-critical break can't wait.

### 3.5 Proxies expose Qt signals where widgets want Qt signals

The reason `vaultRaw()` exists today is that widgets like
`NotesTreeModel` and `GraphView` need `QObject::connect(vault,
&Vault::created, ...)` — closure-based subscription (`VaultProxy::on()`)
is not interchangeable with Qt's signal/slot mechanism.

The fix is to make `VaultProxy` a `QObject` that re-emits
`Vault`'s signals as its own, under the same gating. Subscription
becomes `connect(vaultProxy, &VaultProxy::created, widget, ...)`. The
closure-based `on()` stays (useful for plugin code that doesn't want to
declare a slot), now implemented on top of the Qt signals.

**Rationale:** Qt signals are the lingua franca of widget code in this
tree. A plugin API that doesn't speak them forces plugins to either
(a) keep raw pointers around as Qt-connect handles (what they do today
via the stop-gaps), or (b) write closure-forwarding glue. Both are
friction; neither gains us anything. `MetadataCacheReader` (the proxy)
is already a `QObject` with forwarded signals — good precedent. We
extend the pattern.

---

## 4. Work items

Each work item below includes its rationale inline. The implementation
plan (writing-plans step) will phase these appropriately; this spec
does not commit to a phase ordering.

### 4.1 Retire `vaultRaw()`, `metadataCacheRaw()`, `searchIndex()`

**What:** Delete these three accessors from `PluginContext`. Before
deletion: extend `VaultProxy` and `MetadataCacheReader` to expose
everything the four built-in consumers (LocalGraph, FileExplorer,
GraphView, Search) currently reach through the raw pointers; introduce
`SearchProxy` for the SQLiteIndex surface.

**Specifically:**

- `VaultProxy` inherits from `QObject` and re-emits `created` /
  `modified` / `deletedFile` / `renamed` / `closed` signals from the
  underlying `Vault`. Gated on `vault.events`. The existing `on()` /
  `off()` closure-based subscription stays (reimplemented on top).
- `SearchProxy` (new, in `libs/storage/include/corbomite/storage/
  proxies/`) exposes a stable FTS + backlinks + tags + outlinks query
  API. The surface is derived from current `SQLiteIndex` consumers
  (Search, LocalGraph, GraphView) and excludes schema-shaped methods
  (`createSchema`, direct SQL, raw table access). Gated on
  `metadata.read` — matches the existing `searchIndex()` gate.
- `MetadataCacheReader` already covers the read surface; gap-fill any
  methods the built-ins reach into `MetadataCache` directly for.
- Migrate LocalGraph, FileExplorer, GraphView, Search onto the new
  surfaces. Widgets that took `Vault *` / `MetadataCache *` /
  `SQLiteIndex *` get reshaped to take `VaultProxy *` /
  `MetadataCacheReader *` / `SearchProxy *`.
- Delete the three stop-gap accessors and `setSearchIndex` from
  `PluginContext`.

**Rationale:** this is the architectural gate the whole cluster hinges
on. Until these three accessors are gone, the plugin ABI is not
actually a single surface. The implementation reshapes four internal
widgets, which is bounded work — we already know all the call sites
(see §4 of this spec's planning notes; effectively `git grep
vaultRaw\|metadataCacheRaw\|searchIndex(`).

**Open inside the plan:** whether a separate `TreeModelProxy` is needed
for FileExplorer or whether `VaultProxy`-as-QObject is sufficient for
`NotesTreeModel` to subscribe directly. Likely the latter — decide in
the plan.

### 4.2 Real keyring backend for `SecretStorage`

**What:** Replace the in-process `QHash<QString, QString>` with a
QtKeychain-backed implementation. Each plugin's secrets are stored
under a namespaced key (e.g. `corbomite-plugin-<id>-<key>`) via the
platform's native keyring (KWallet on KDE, GNOME Keyring, macOS
Keychain, Windows Credential Manager).

**Test-env behavior:** detect headless/test environment (already done
in several places via `QT_QPA_PLATFORM=offscreen`) and fall back to the
in-process QHash so unit tests don't require a working keyring or
trigger modal unlock dialogs in CI.

**Rationale:** this was deferred from Cluster Q with the note "SSH
session can't safely drive KWallet/QtKeychain unlock dialogs". That's
an SSH-development-session concern, not a production concern. Real
plugins — a Readwise importer, a Git-sync plugin, anything with an API
token — need persistent secrets. Keeping the in-process QHash is worse
than not having `SecretStorage` at all, because it looks durable but
isn't. The test-env fallback preserves the CI ergonomics Q relied on.

**Dependency:** Add `qt6keychain` to the CMake `find_package` chain
behind a `CORBOMITE_USE_KEYRING` option defaulting to `ON`. When the
option is off or the package is not found, `SecretStorage` compiles
against the QHash-only implementation and logs the degraded-persistence
warning described above. Package is already widely available across
distros (shipped as `qt6keychain`, `libqt6keychain-dev`, or similar).

### 4.3 Obsidian-shape plugin data persistence

**What:** Add `PluginContext::loadData()` → `QJsonObject` and
`PluginContext::saveData(QJsonObject)` → `bool`. Storage is atomic
`QSaveFile` write to `.obsidian/plugins/<plugin-id>/data.json`, relative
to the active vault's config dir. Gated on the existing `config`
permission.

**Interaction with KConfig:** `PluginContext::config()` (which returns
a `KConfigGroup` under Corbomite's global KConfig) stays. The two serve
different audiences:

- `config()` for Corbomite-native plugins that want to integrate with
  KDE's preference model.
- `loadData()` / `saveData()` for vault-scoped, portable state —
  matches Obsidian's convention, travels with the vault, plugins
  ported from Obsidian map directly.

**Rationale:** plugins ported from Obsidian (and there will be some; the
audit shape-parity is deliberate) expect `await this.loadData()` /
`await this.saveData(obj)` to persist alongside the vault. Without
this, every ported plugin has to be rewritten to use KConfig, which
(a) breaks vault portability (vault on a USB stick loses its plugin
data) and (b) is not what `X-Corbomite-Permissions` → `config` was
designed to describe. Shipping the Obsidian-shape form now, before
third-party developers start porting, avoids a migration later.

### 4.4 Trust-flag build wiring

**What:** A CMake property `CORBOMITE_TRUSTED_PLUGIN` (or a
`corbomite_add_plugin()` helper function) that, for plugins in
`src/plugins/`, injects `X-Corbomite-Trusted: true` into their
`metadata.json` at configure time (via `configure_file` substitution of
a `metadata.json.in` template, or equivalent). `PluginManager` reads
the flag and, when true, skips `PluginPermissionGrantDialog` on first
enable.

Third-party `CMakeLists.txt` examples in `examples/plugin-template/`
must not expose the helper.

**Rationale:** this closes the trust-mechanism question. Keeping the
flag controlled by Corbomite's build system — not by plugin authors —
is the simplest mechanism that conveys "these specific plugins ship
with the host". Our build system is the only trusted thing here; a
third-party plugin's `CMakeLists.txt` running under the user's shell
is not. (Yes, a malicious third-party could copy the helper into their
own CMake. We are explicitly not trying to prevent that — per §3.3,
the flag saves one dialog; if a user is already installing a malicious
distro package, one dialog was not going to save them.)

### 4.5 API stability enforcement

**What:**

- Add `X-Corbomite-MinVersion` handling: `PluginManager::discover`
  reads the field, `PluginManager::enable` refuses plugins whose
  minimum exceeds `QCoreApplication::applicationVersion()` with a
  clear `qCWarning` and a `PluginsPage` state "requires Corbomite ≥ X".
- Add `X-Corbomite-ApiLevel` as an optional integer. Semantics:
  - Host publishes a `constexpr int CORBOMITE_PLUGIN_API_LEVEL` in a
    public header. Default at ship is `1`.
  - Plugin omits the key → treated as level `1` (today's API). Loads.
  - Plugin declares level ≤ host level → loads. Host maintains a
    compatibility shim for one major version back when level is bumped.
  - Plugin declares level > host level → refuses to load with a clear
    `PluginsPage` state "requires plugin API level ≥ N".
  The field is deliberately independent of human-readable
  `X-Corbomite-MinVersion` so we can make a hard ABI break without
  coordinating with version-number cosmetics.
- Publish a `docs/plugin-development/API-STABILITY.md` committing to
  the 12-month deprecation window (§3.4).
- Document the stable surface: every public method of `VaultProxy`,
  `FileManagerProxy`, `MetadataCacheReader`, `SearchProxy`,
  `WorkspaceController`, `CommandRegistrar`, `ViewRegistrar`,
  `MenuInjector`, `SecretStorage`, `ProcessSpawner`, and the permission
  tokens. Anything not documented is not stable.

**Rationale:** the commitment is the deliverable. Without a written
stability promise, third-party authors have no basis to decide whether
to invest in a Corbomite plugin. The `ApiLevel` integer is cheap and
saves us one headache if we ever need a break-glass; we default it to
`1` today and only bump on genuinely breaking changes.

### 4.6 Reference third-party plugin

**What:** An in-tree example under `examples/note-stats-plugin/` that
ships as a separate CMake target with its own `metadata.json`,
`CMakeLists.txt`, `.so` output, tests, and `README.md` (doubling as
tutorial). The plugin reads markdown files via `VaultProxy`, reads
frontmatter via `MetadataCacheReader`, registers a sidebar view via
`ViewRegistrar`, registers a "Show note stats" command via
`CommandRegistrar`, and renders a summary of vault-level statistics
(note count, word count, tag count, link count).

Its `CMakeLists.txt` uses only public Corbomite CMake targets
(`find_package(Corbomite)` — which we must ship as a CMake package
config if we haven't already). It explicitly does *not* use the
internal `corbomite_add_plugin()` helper — it uses
`find_package(KF6::CoreAddons)` + `kcoreaddons_add_plugin()` directly,
proving that the third-party path works without Corbomite-internal
CMake surfaces.

It does not set `X-Corbomite-Trusted: true` in its metadata, so
enabling it exercises `PluginPermissionGrantDialog` end-to-end.

**Rationale:** this is the proof. A plugin API without an external
consumer is an unverified hypothesis. Note-stats is a deliberate choice:
- Small enough to read in one pass.
- Touches enough of the surface (read, metadata, view, command) to
  exercise the plumbing.
- Useful enough on its own that we might actually ship it, rather than
  deleting it once the cluster is done. (Decide at cluster close: keep
  as bundled, keep as separate package, or archive as pure example.)

**Open inside the plan:** whether to keep it in-tree forever as a
sanity-check smoke test, whether to eventually move it to its own repo
once the third-party path is proven, or whether to bundle it as a
ninth InternalPlugin on merit. Leave this decision for cluster
closeout.

### 4.7 Plugin-author documentation

**What:** a new `docs/plugin-development/` directory containing:

- `README.md` — table of contents, quickstart, link to tutorial.
- `TUTORIAL.md` — walks through the reference plugin end-to-end:
  scaffolding, writing the plugin class, permissions, building,
  installing, testing.
- `API-REFERENCE.md` — lists every public proxy method, permission
  token, lifecycle hook, registry. Not Doxygen-generated (yet) —
  hand-maintained in this cluster, can be Doxygen-ified later without
  breaking the contract.
- `API-STABILITY.md` — the commitment from §4.5.
- `DISTRIBUTION.md` — how to ship a plugin via distro packaging:
  filesystem layout, `metadata.json` conventions, suggested Debian
  / RPM packaging skeletons, system-path discovery.
- `examples/plugin-template/` — a minimal CMake skeleton
  third-party developers copy to start a new plugin. Includes the
  minimum `metadata.json`, a stub `Plugin` subclass, a `tests/`
  directory with one passing smoke test, and a `README.md` pointing
  at the main docs.

**Rationale:** the plugin ABI is not usable until a developer can learn
it. We don't need perfect documentation — we need sufficient
documentation that a motivated C++/Qt developer, who has built a
plugin for something like KDevelop or Kate before, can produce a
working Corbomite plugin in an afternoon. The reference plugin is the
executable part of this; the prose fills in the rest.

---

## 5. Stability contract

### 5.1 What becomes stable on cluster close

Two levels of stability are promised:

- **Shape-stable** (from cluster close): no removals. Additions, minor
  ergonomic tweaks (argument defaults, method overloads that don't
  break existing signatures) are allowed. This is the commitment we
  make immediately so third-party authors can start building.
- **1.0-frozen** (after a proving period, not part of this cluster):
  the full 12-month deprecation-window rule applies to all changes,
  including ergonomic tweaks. The bump to 1.0 is a separate event
  after real plugin consumers have shaken out the surface.

The surfaces below are **shape-stable from cluster close**:


- All public methods of the proxy classes listed in §4.5.
- All 12 permission tokens currently declared. New tokens may be
  added; existing tokens' semantics cannot change.
- `Plugin` virtual lifecycle hooks: `onLoad(PluginContext*)`,
  `onUnload()`, `createView(MainWindow*)`, `focus(QObject*)`,
  `saveSessionState(QObject*)`, `loadSessionState(QObject*,
  QJsonObject)`.
- `PluginContext` accessors (after stop-gap removal).
- `metadata.json` schema, including the `X-Corbomite-*` keys.

### 5.2 What remains unstable (and documented as such)

- `libs/core/` internal types — `Component`, `Events`, anything not
  reachable through a proxy.
- `Vault`, `FileManager`, `MetadataCache`, `SQLiteIndex`,
  `NotesTreeModel` — direct types, not in the plugin API.
- Widget / View class hierarchies. Plugins register factories
  producing `View *` subclasses but do not inherit from our
  non-View widget types. (If a plugin wants a QWidget, it
  instantiates a QWidget.)

### 5.3 How we evolve

- **Additive:** always allowed, at any time.
- **Deprecations:** add `[[deprecated("use X instead")]]` at the
  declaration; document in the release notes; maintain for 12 months.
- **Removals:** only after the 12-month window; breaking change notes
  in `API-STABILITY.md`.
- **Major rewrites:** bump `X-Corbomite-ApiLevel`. Plugins declaring
  the old level (or omitting the field, which defaults to `1`)
  continue to load against a compatibility shim for one major version;
  plugins declaring the new level get the new surface.

---

## 6. Risks and mitigations

1. **Risk: migrating the four built-in plugins off the raw pointers is
   harder than it looks.** Specifically, `FileExplorer` uses
   `NotesTreeModel` which is built around direct `Vault *` access
   including `QObject::connect` to vault signals. If `VaultProxy`-as-
   QObject doesn't fully substitute, we need `TreeModelProxy` or
   similar.
   **Mitigation:** §4.1 says "decide in the plan". The implementation
   plan phases can start with `VaultProxy` QObject-ification and
   measure whether the four migrations work cleanly before committing
   to additional proxies. If `TreeModelProxy` is needed, its scope is
   narrow: expose `NotesTreeModel *` directly, gated on `vault.read`,
   since the model itself doesn't leak vault internals.

2. **Risk: the 12-month stability commitment is premature.** We haven't
   yet observed what third-party developers actually build; the proxy
   surface may have ergonomic warts we don't know about until real
   plugins hit them.
   **Mitigation:** commit to the window only for 1.0 of the plugin
   API, not for the current state. Let the reference plugin and any
   early adopters shake out ergonomic issues; bump to 1.0 after a
   proving period (not part of this cluster). In the meantime, the
   stability doc can say "the API is stable *in shape* but may see
   minor ergonomic revision before 1.0 freeze."

3. **Risk: QtKeychain dependency on platforms where it's awkward
   (non-KDE Linux without GNOME Keyring, headless server installs).**
   **Mitigation:** the test-env fallback becomes a production fallback
   too — if the keyring is unreachable, `SecretStorage` logs a clear
   warning and falls back to the in-process QHash with an explicit
   "secrets will not persist across restarts" message in
   `PluginsPage`. The plugin author decides whether that's acceptable
   for their use case.

4. **Risk: scope creep.** The deferrals in §2.2 are each tempting to
   include. Hotkeys in particular look like "table stakes".
   **Mitigation:** this spec explicitly calls out each deferred item
   and its gating consumer. If during implementation the reference
   plugin demands one of them, we add it to scope with a written
   rationale in the plan and PROJECT-STATE entry. We do not add items
   on aesthetic grounds.

---

## 7. Blocks / enables

### 7.1 What this cluster unblocks

- **Any future third-party plugin development.** No specific consumer
  is named; that's the point.
- **Cluster K (Bases).** Bases is independently blocked on DSL
  extraction, but once unblocked, its rendering extensions will
  naturally flow through the same `EmbedRegistry` and
  `CodeBlockProcessorRegistry` surfaces a plugin would use. A stable
  plugin ABI makes Bases' API shape easier to settle.
- **Future JavaScript plugin host** (if anyone builds it). Its job is
  to translate `obsidian` module calls to our C++ proxies; a stable
  C++ API makes that translation layer possible.
- **"Move built-ins to separately packageable" option** (not planned,
  but enabled). If we ever want the Graph view to ship as a distro
  package rather than a built-in, the symmetric ABI means we can
  flip the `X-Corbomite-Trusted` flag and move the CMake target
  without reshaping the plugin.

### 7.2 What this cluster does not unblock

- **Runtime sandboxing.** Plugins remain native in-process code. If
  security ever demands sandboxing, that's a separate cluster with
  its own architecture (process isolation, WASM host, etc.).
- **Plugin marketplace / in-app install.** Explicit non-goal.
- **Obsidian plugin direct compatibility.** An Obsidian plugin is
  JavaScript running in Electron; our plugin is C++ running in a Qt
  process. They're the same shape at the API level (by design) but
  are not binary-compatible. A JS shim (future, separately) would
  bridge them.

---

## 8. Audit references

- `docs/obsidian-audit/PLUGIN-API-SKETCH.md` — the 9-registry,
  7-menu-event, 6-subclass surface this cluster's proxies implement.
  All 9 registries and 7 menu events are present in the tree already;
  this cluster stabilizes them.
- `docs/obsidian-audit/SHARED-SYMBOLS.md` — Obsidian's `App.vault` /
  `App.fileManager` / `App.metadataCache` shape, which Q.0 landed and
  this cluster exposes through proxies.
- `docs/obsidian-audit/FEATURE-MATRIX.md` — plugin-adjacent rows in
  particular; most are "parity" since Q, a few become "parity" only
  after this cluster lands (notably SecretStorage and plugin
  data.json).
- `docs/obsidian-audit/GAP-ANALYSIS.md` §12 (plugin-portability
  sandbox recommendation) — this cluster explicitly defers the
  JavaScript shim to a future plugin, documenting that decision.
- `docs/cluster-retros/cluster-q.md` — open follow-ups 1, 7, 8 are
  addressed directly by this cluster; 2 (rewrite tst_propertiespanel)
  is out of scope but noted.

---

## 9. Preserved compat quirks

None introduced by this cluster. The stop-gap raw accessors are *not*
preserved — they are deleted. No third-party plugin exists yet to depend
on them, so there is nothing to preserve. Internal plugins are migrated
in the same cluster.

---

## 10. Open inside-the-plan questions

These are decisions deliberately left for the implementation plan
(writing-plans) to settle based on what the code reveals:

1. **Whether `TreeModelProxy` is needed.** (§4.1)
2. **Whether to keep the reference plugin bundled as a ninth
   InternalPlugin, keep it as a pure example, or move it to its own
   repo.** (§4.6)
3. **Exact shape of `SearchProxy`'s query API.** Derive from the
   three consumers (Search, LocalGraph, GraphView) during plan
   drafting; do not design it in the abstract.
4. **Whether `API-REFERENCE.md` should be hand-written or
   Doxygen-generated from the headers.** Hand-written in this cluster
   per §4.7; revisit after the surface stabilizes.
5. **CMake package config shape** (`CorbomiteConfig.cmake`) — we need
   to ship one so `find_package(Corbomite)` works in the reference
   plugin's CMake. Design in plan; shape is standard.
