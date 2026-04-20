# Cluster Q — Internal-Plugin Wrapping + Permissions (Design)

> **Status:** Design — brainstorm completed 2026-04-16. Awaiting spec review, then implementation plan via `writing-plans`.
> **Brainstorm:** this conversation.
> **Preceded by:** Cluster G closure (2026-04-16) — Workspace integration provides the leaf + view-registry substrate plugins need.
> **Unblocks:** Cluster N scope shrinks substantially (§12).

## 1. Goal

Unify Corbomite's built-in features and future community plugins under a single KDE-native plugin infrastructure. Eight of Corbomite's optional sidebar / main-area features become first-class `InternalPlugin`s loaded via `KPluginFactory`, user-toggleable in Settings. The same loader, metadata schema, lifecycle, and permission model that governs these InternalPlugins will govern community plugins when Cluster N lands — making N a distribution-UX cluster rather than an architectural one.

Cluster Q's concrete deliverables:

1. `Corbomite::Plugin` base class + `PluginContext` + `PluginManager` + `PluginMetaData` wrapper, in `libs/core/`.
2. Eight `.so` plugin targets at `src/plugins/<slug>/` — wrapping the existing FileExplorer, Search, Backlinks, Outlinks, Outline, Properties, LocalGraph, GraphView panels.
3. Plugin metadata schema (JSON embedded via `K_PLUGIN_FACTORY_WITH_JSON`).
4. 12-category permission model, capability-token proxy mechanism, grant UX.
5. Settings "Plugins" page with per-plugin enable/disable + permission review.
6. KConfig persistence for enable/disable and granted-permissions state.

## 2. Non-Goals

- **Rendering-extension plugins** (mermaid, math, syntax highlighting, post-processors, embed factories, hover-link sources). The gradient approach defers these to a follow-up cluster; existing internal registries are marked `// TODO: expose as plugin API in gradient phase 2` but otherwise untouched.
- **Obsidian-JavaScript-shim runtime.** Preserved as a design option (§13.3); API shapes stay shim-friendly, but no JS runtime is built in Q.
- **Community-plugin distribution UX.** No "install from .zip" dialog, no KNewStuff browser, no plugin marketplace. Cluster N.
- **Per-vault enable/disable.** Plugin state is global (app-wide KConfig). If a future cluster needs per-vault overrides, that's N's problem (§13.5).
- **Optional / graceful-degradation permissions.** The current model is binary (granted or ungranted). "Optional network permission — plugin runs offline without it" is flagged as future consideration (§13.1) but not built.
- **Runtime sandboxing.** `.so` plugins run in-process with full host privileges; the permission system is a declarative contract, not an enforcement boundary (§5.3). Real sandboxing via `QProcess` + seccomp / AppContainer / WASM is a future-cluster decision (§13.2).
- **Plugin-to-plugin IPC beyond the standard `Events` bus.** Plugins subscribe to and emit events through the shared `Events` mixin; no additional channels (§13.6).

## 3. Architecture

Five new classes in `libs/core/include/corbomite/core/`:

- **`Corbomite::Plugin`** — abstract base, `Component` subclass. All plugins (built-in + community) inherit from it. Virtual methods: `onLoad(PluginContext*)`, `onUnload()`, `createView(MainWindow*)` (per-window view; may return `nullptr` for headless plugins), `configPage(int, QWidget*)` (optional KConfig page, Kate pattern).
- **`Corbomite::PluginContext`** — handed to `plugin->onLoad(ctx)`. Owned by `PluginManager` for the lifetime of the plugin. Exposes typed accessors to core services, gated by the plugin's granted capability tokens (§5.1).
- **`Corbomite::PluginMetaData`** — thin wrapper over `KPluginMetaData` exposing Corbomite-specific JSON keys (`permissions()`, `trusted()`, `minAppVersion()`).
- **`Corbomite::PluginManager`** — singleton, owned by `CorbomiteApp`. Modeled on Kate's `PluginManager` (KDE-guide §"Kate Plugin Manager Pattern"). Discovers (`KPluginMetaData::findPlugins("corbomite")`), loads (`KPluginFactory::loadFactory`), enables/disables, unloads. Emits `pluginLoaded(id)`, `pluginUnloading(id)`, `pluginEnabled(id)`, `pluginDisabled(id)`.
- **`Corbomite::PluginPermissionGrantDialog`** — modal dialog for untrusted-plugin first-enable prompts. KDialog subclass with a checkbox list over declared permissions + human-readable descriptions.

No separate infrastructure for built-ins vs community plugins. One loader path, one API shape, one set of lifecycle signals.

## 4. JSON metadata schema

Each plugin ships one JSON blob, embedded at compile time via `K_PLUGIN_FACTORY_WITH_JSON`:

```json
{
  "KPlugin": {
    "Id": "corbomite-file-explorer",
    "Name": "File Explorer",
    "Description": "Browse vault contents in a sidebar panel",
    "Icon": "folder-open",
    "Version": "1.0",
    "License": "GPL-3.0-or-later",
    "Category": "Core",
    "EnabledByDefault": true,
    "Authors": [{"Name": "Corbomite Developers"}]
  },
  "X-Corbomite-Trusted": true,
  "X-Corbomite-Permissions": [
    "vault.read", "vault.write",
    "ui.views", "ui.menus",
    "workspace"
  ],
  "X-Corbomite-MinVersion": "0.1.0"
}
```

Corbomite-specific keys:

- **`X-Corbomite-Trusted`** (bool) — if `true`, plugin auto-grants declared permissions without a user prompt. Reserved for plugins shipped as part of Corbomite's release package. Community plugins MUST have this `false` or absent. Loader enforces this via install-path origin: plugins loaded from the **system install path** (default `${KDE_INSTALL_PLUGINDIR}/corbomite/` — normally writable only by the system package manager) may assert `Trusted: true`; plugins loaded from the **user install path** (default `~/.local/share/corbomite/plugins/`) have their `Trusted` claim forcibly reset to `false` at load regardless of what the JSON says. This is a first-line protection, not a security boundary (§5.3) — a user who writes to the system path has root and could trivially place a trusted plugin there anyway; the mechanism just ensures Corbomite doesn't *itself* trust a plugin dropped into the user dir by unvetted install tooling.
- **`X-Corbomite-Permissions`** (array of strings) — declared capability tokens (§5.1).
- **`X-Corbomite-MinVersion`** (string, semver) — minimum Corbomite version required. `PluginManager` rejects incompatible plugins at load with `qWarning`.

## 5. Permission model

### 5.1 Twelve capability tokens

| Token | Gates |
|---|---|
| `vault.read` | Reading note contents via Vault API |
| `vault.write` | Creating, modifying, deleting, or renaming notes (implies `vault.read`) |
| `metadata.read` | Querying MetadataCache (frontmatter, links, tags, headings, blocks) |
| `ui.commands` | Registering palette commands + default-hotkeys |
| `ui.views` | Registering sidebar panels + main-area tab types (via ViewRegistry) |
| `ui.menus` | Injecting items into context menus (file, editor, url, tab, leaf menus) |
| `workspace` | Opening files, splitting panes, closing tabs, popout windows, navigation |
| `network` | Outgoing HTTP / WebSocket via QNAM |
| `secrets` | KWallet / QtKeychain credential storage |
| `process` | Spawning external programs (QProcess) |
| `config` | Reading/writing Corbomite KConfig (own-plugin + global) |
| `render` | Registering render-side extensions — **reserved for gradient phase 2**; declaration parsed but unenforced in Q |

Standard Qt widget allocation, signal connections to plugin-owned objects, in-plugin computation, event reception — all implicit, no declaration required.

**Deliberate non-splits** (bundled for ergonomic reasons):

- `ui.hotkeys` folded into `ui.commands` — a command registering a hotkey is the common case.
- `workspace.navigate` vs `workspace.mutate` folded into `workspace` — same plugins use both in practice.
- `config.read` vs `config.write` folded — read-only access to your own plugin's config is trivial; all-or-nothing is simpler.

### 5.2 Capability-token proxy pattern

The `PluginContext` exposes typed accessors; each accessor returns a proxy object if the corresponding permission was granted, or `nullptr` otherwise:

```cpp
class PluginContext {
public:
    // Permission-gated accessors (nullptr if ungranted):
    VaultReader         *vaultReader() const;      // "vault.read"
    VaultWriter         *vaultWriter() const;      // "vault.write"
    MetadataCacheReader *metadataCache() const;    // "metadata.read"
    WorkspaceController *workspace() const;         // "workspace"
    CommandRegistrar    *commands() const;          // "ui.commands"
    ViewRegistrar       *views() const;             // "ui.views"
    MenuInjector        *menus() const;             // "ui.menus"
    QNetworkAccessManager *network() const;         // "network"
    SecretStorage       *secrets() const;           // "secrets"
    ProcessSpawner      *process() const;           // "process"
    KConfigGroup        config();                   // "config"

    // Component-inherited — no permission required:
    void registerCleanup(std::function<void()>);
    void registerEvent(EventRef);
    void registerInterval(QTimer*);
    // ...

    // Metadata:
    const PluginMetaData &metaData() const;
    const QSet<QString>  &grantedPermissions() const;
};
```

Proxy objects are thin C++ wrappers around the underlying core services. Once constructed (after permission check at `PluginContext` construction time), they perform no per-call permission re-check — the cost is paid once at plugin load. Proxies log calls at `qCDebug` level (off by default) tagged with the plugin ID, for audit (§5.3 purpose 4).

Plugin-author ergonomic: call sites look like `if (auto *reader = ctx->vaultReader()) reader->read(path);` — a terse null-check. Ungranted permissions are plainly visible at the call site, not buried in runtime exceptions.

New proxy classes required in `libs/core/`: `VaultReader`, `VaultWriter`, `MetadataCacheReader`, `WorkspaceController`, `CommandRegistrar`, `ViewRegistrar`, `MenuInjector`, `SecretStorage`, `ProcessSpawner`. Their internal method signatures mirror the existing underlying service APIs — mostly identity wrappers for now; value appears when Q's follow-up clusters tighten the plugin-visible API surface.

### 5.3 Honest scope: declarative contract, not runtime sandbox

A `.so` plugin loaded into Corbomite's process has the same memory, filesystem, and syscall access as the host. A plugin that declares `[]` can still:

- `::socket(2)` directly, bypassing `QNetworkAccessManager`
- Use `<fstream>` / `open(2)` to read/write any file the user can access
- `fork()` / `execve()` directly, bypassing `ProcessSpawner`
- D-Bus straight to KWallet, bypassing `SecretStorage`
- Access any linked library, mutate any global, overwrite any function pointer

**The permission system is a declarative contract, not an enforcement boundary.** It serves five purposes:

1. **Contractual declaration.** The manifest states intent. A plugin that phones home while declaring no `network` is unambiguously misbehaving — a reportable / reviewable bug or malice.
2. **User informed consent.** The grant prompt lets users make an informed trust decision.
3. **Audit surface.** An official plugin registry (future) can have reviewers flag mismatches via static analysis (e.g., "`#include <sys/socket.h>` but no `network` declared" — trivially detectable).
4. **Runtime logging.** `PluginContext` logs every proxy call with the plugin ID. Per-session summaries let users spot anomalies.
5. **Future-proofing.** If later clusters introduce real sandboxing (§13.2), the proxy API shape doesn't change — proxies become IPC shims. Design cost of preserving that future option today: ≈0.

Most native plugin systems operate this way (Kate, KDevelop, Audacity, OBS, Photoshop, Lightroom). Enforcement-grade sandboxing requires process isolation (Chrome/Firefox extensions) or bytecode runtimes (browser JS, Java applets, WASM). None of those are in Cluster Q's scope.

The UX language reflects this: grant dialog says "This plugin **declares** it needs…", Settings label reads "Declared permissions", not "Allowed permissions" / "Enforced permissions". We do not overclaim.

### 5.4 Permission grant UX

- **Trusted (built-in)**: auto-grant at load. No prompt. Settings page shows declared permissions read-only, for transparency.
- **Untrusted, first enable**: modal dialog lists declared permissions with human-readable descriptions; per-permission checkbox. User grants all, subset, or cancel (plugin stays disabled). Cancelling a required permission cancels the enable.
- **Untrusted, subsequent enable (no metadata changes)**: no prompt.
- **Untrusted, post-update with new permissions**: re-prompt only for the new permissions.
- **Revocation**: toggling a granted permission off in Settings triggers immediate plugin disable; re-enable re-prompts (the plugin can't honestly run without its declared permissions).

Grant state persists in `KConfigGroup("PluginPermissions")`.

## 6. Plugin lifecycle

1. **Discovery** — `PluginManager::discoverPlugins()` at app startup. `KPluginMetaData::findPlugins("corbomite")` scans both the system install path (`${KDE_INSTALL_PLUGINDIR}/corbomite/`) and the user install path (`~/.local/share/corbomite/plugins/`). Each discovered plugin records its origin (system vs user) for the trusted-claim check in step 4.
2. **Enable decision** — read `KConfigGroup("Plugins")` for `<pluginId>Enabled`; fall back to metadata `EnabledByDefault`.
3. **Version-compat check** — reject plugins with `X-Corbomite-MinVersion > APP_VERSION` (where `APP_VERSION` is the compile-time constant set by CMake's project-version field); emit `qWarning`.
4. **Trusted-claim normalization** — plugins loaded from the user install path have their `X-Corbomite-Trusted` claim forcibly reset to `false` regardless of what their JSON declared. Plugins from the system install path keep whatever they declared. This is a first-line UX protection (§5.3); not a security boundary.
5. **Permission grant** — trusted: auto-grant. Untrusted: check `KConfigGroup("PluginPermissions")`; if any declared permission ungranted, show grant dialog (blocking).
6. **Factory load** — `KPluginFactory::loadFactory(metaData)`. On failure, `qWarning` + skip.
7. **Plugin construction** — `factory->create<Corbomite::Plugin>(manager, QVariantList())`. Constructor runs.
8. **Context + onLoad** — `PluginManager` builds a `PluginContext` with proxy accessors per granted permissions, then calls `plugin->onLoad(context)`.
9. **Per-MainWindow view creation** — for each open MainWindow, `plugin->createView(mainWindow)` is called; the returned view is added to ToolView / Workspace / ribbon / menu (plugin's choice).
10. **Runtime disable** — views destroyed per MainWindow → `plugin->onUnload()` → `Component::unload` drains LIFO cleanups → `delete plugin` → KConfig updated.
11. **Runtime enable** — inverse; same factory-load path.
12. **Shutdown** — all plugins disabled in reverse load order (KDevelop convention).

## 7. Config persistence

All plugin state lives in Corbomite's KConfig (`corbomiterc`, dev: `corbomite-devrc`). **No `.obsidian/core-plugins.json` write** — Obsidian's plugin IDs don't map 1:1 to ours; translation would be lossy.

- **`[Plugins]` group**: `<pluginId>Enabled = true|false`. Matches Kate's pattern exactly.
- **`[PluginPermissions]` group**: `<pluginId>Granted = comma,separated,tokens`. Global, survives vault switches.
- **Per-plugin user data**: each plugin owns its own persistence. Built-ins may use `KConfigGroup("Plugin-<id>")` or `<vaultRoot>/.obsidian/plugins/<id>/data.json` (plugin's choice; Q recommends the latter for vault-portable data). Community plugins are free to write wherever; Q doesn't dictate.

## 8. Settings integration

New `Plugins` page in SettingsDialog, built on `KCMUtils::KPluginWidget`-style pattern:

- **Top list**: one row per discovered plugin — name, description, trusted badge (if applicable), enable checkbox. Filter chips: "All / Core / Community / Enabled only".
- **Expanded per-plugin detail panel**:
  - Version, author, homepage link.
  - **Declared permissions** section — per-permission checkbox + human-readable description. Read-only for trusted plugins; interactive for community plugins.
  - **Configure…** button if the plugin provides `configPage(int)` — opens the plugin's own KConfig page.
- **No separate "Core" vs "Community" pages.** One list; trusted badge + `Category` metadata distinguish. Q+N fusion whole point.

## 9. Directory structure

```
src/plugins/
  file-explorer/
    FileExplorerPlugin.{h,cpp}
    FileExplorerView.{h,cpp}       ← the KateMDI ToolView
    metadata.json
    CMakeLists.txt                 ← K_PLUGIN_FACTORY_WITH_JSON + install to
                                    ${KDE_INSTALL_PLUGINDIR}/corbomite/
  search/        ← sidebar/SearchPanel moves here
  backlinks/     ← sidebar/BacklinksPanel moves here
  outlinks/      ← sidebar/OutlinksPanel moves here
  outline/       ← sidebar/OutlinePanel moves here
  properties/    ← sidebar/PropertiesPanel moves here
  local-graph/   ← src/graph/ local-graph bits move here
  graph-view/    ← src/graph/GraphViewTab moves here (main-area tab)

libs/core/include/corbomite/core/
  Plugin.h
  PluginContext.h
  PluginManager.h
  PluginMetaData.h
  PluginPermissionGrantDialog.h
  (plus proxy class headers: VaultReader.h, VaultWriter.h, MetadataCacheReader.h,
   WorkspaceController.h, CommandRegistrar.h, ViewRegistrar.h, MenuInjector.h,
   SecretStorage.h, ProcessSpawner.h)

libs/core/src/
  Plugin.cpp, PluginContext.cpp, PluginManager.cpp, ...
```

The existing `src/sidebar/*.{h,cpp}` files physically relocate into their corresponding plugin directories; MainWindow loses its direct panel construction + `connect()` calls; those become plugin `createView(mainWindow)` implementations.

## 10. Migration plan (Q's scope)

Eight InternalPlugins ship at Q's close. All are `X-Corbomite-Trusted: true` with `EnabledByDefault: true`, preserving current behavior.

| Slug | Wraps | Permissions |
|---|---|---|
| `corbomite-file-explorer` | `FileExplorerPanel` | `vault.read`, `vault.write`, `ui.views`, `ui.menus`, `workspace` |
| `corbomite-search` | `SearchPanel` | `vault.read`, `metadata.read`, `ui.views`, `ui.commands`, `workspace` |
| `corbomite-backlinks` | `BacklinksPanel` | `metadata.read`, `ui.views` |
| `corbomite-outlinks` | `OutlinksPanel` | `metadata.read`, `ui.views` |
| `corbomite-outline` | `OutlinePanel` | `metadata.read`, `ui.views`, `workspace` |
| `corbomite-properties` | `PropertiesPanel` | `vault.write`, `metadata.read`, `ui.views` |
| `corbomite-local-graph` | `LocalGraphPanel` (from `src/graph/`) | `metadata.read`, `ui.views`, `workspace` |
| `corbomite-graph-view` | `GraphViewTab` | `metadata.read`, `ui.views`, `workspace` |

Existing registries (`EmbedRegistry`, `PostProcessorRegistry`, `CodeBlockProcessorRegistry`, `HoverLinkSourceRegistry`) get a `// TODO: expose as plugin API in gradient phase 2` comment in this cluster — otherwise untouched.

## 11. Testing strategy

- **Per-plugin** (`src/plugins/<slug>/tests/tst_<slug>.cpp`): loads in isolation against a minimal mock MainWindow, creates a view, honors declared permissions, unloads cleanly.
- **PluginManager** (`tests/core/tst_plugin_manager.cpp`): discovery, KConfig enable/disable round-trip, permission grant/revoke, load failure (missing factory, incompatible min-version, trusted-claim rejection), shutdown order.
- **PluginContext** (`tests/core/tst_plugin_context.cpp`): granted accessors non-null, ungranted accessors null, proxy forwarding behavior, per-call logging emission.
- **Integration** (`tests/integration/tst_plugin_lifecycle.cpp`, offscreen Qt): full MainWindow + PluginManager; toggle FileExplorer off/on; verify ToolView actually appears/disappears; verify `workspace.json` persists across the toggle.
- **JSON metadata validation** (`tests/core/tst_plugin_metadata.cpp`): parses real plugin JSON files from `src/plugins/*/metadata.json`; verifies required keys, permission-token validity, trusted-claim legitimacy.

## 12. Interaction with Cluster N

Cluster N shrinks substantially. Most of its original scope lands in Q:

- ✅ Plugin loader infrastructure → Q
- ✅ Plugin metadata schema → Q
- ✅ Plugin lifecycle → Q
- ✅ Permission model (declarative) → Q
- ✅ Settings integration → Q

What remains for N:

- **Community-plugin distribution UX** — "install from `.zip`" dialog, checksum-based update flow, optional KNewStuff integration (or explicit decision that KNewStuff isn't the right fit for compiled `.so` distribution).
- **Plugin-isolation hardening (decision)** — whether to run community plugins in an isolated `QProcess` + seccomp / AppContainer / WASM, or continue the in-process soft-contract model. Likely defer until a concrete need surfaces (first malicious plugin report, enterprise user requirement).
- **Optional `community-plugins.json` round-trip** — if future-vault-in-Obsidian compat warrants rough enable/disable parity for Obsidian-ported plugins.
- **Plugin-API versioning** — when Corbomite's plugin API itself has breaking changes, how do community plugins signal and migrate. Probably `X-Corbomite-API: 2` semver compare against app.

## 13. Future considerations

Flagged in the spec so they surface cleanly when decisions land:

### 13.1 Optional / graceful-degradation permissions

Today's model is binary (granted or ungranted). A more nuanced model: plugins declare permissions as `required` vs `optional`. Example: a sync plugin declares `network` as `optional` — runs offline if denied, runs with network access if granted. User can grant a subset; plugin adapts via the proxy null-check.

Adding this later means extending the metadata schema (`X-Corbomite-Permissions-Optional: [...]`) and plugin code (existing null-checks suffice; plugins just handle `nullptr` as a supported case instead of a failure). Backward-compatible with Q's binary model. Not in Q.

### 13.2 Hard-sandbox enforcement upgrade path

The proxy pattern designed in Q is IPC-shim-ready. A future cluster could introduce process-isolated plugin hosts:

- **Linux:** `QProcess` with `seccomp-bpf` filters preventing undeclared syscalls
- **Windows:** `AppContainer` with restricted filesystem/registry/network ACLs
- **macOS:** App sandbox entitlements
- **Cross-platform:** WASM runtime (Wasmtime / wasmer) with capability-based imports

All reuse Q's `PluginContext` + proxy API unchanged; proxies route to IPC-to-sandboxed-process instead of direct calls. Major implementation work, but Q's design doesn't foreclose it. The current soft-contract model is honest about its limits (§5.3) so a future switch to hard enforcement is an upgrade, not a pivot.

### 13.3 Obsidian-JavaScript-shim runtime

The original `PLUGIN-API-SKETCH.md` recommendation: run Obsidian JS plugins in a `QtWebEngine` sandbox, translating the `obsidian` module via a C++↔JS bridge (QtWebChannel). Q's API stays shim-friendly via symbolic identifiers, the `Events` mixin facade (per `PLUGIN-API-SKETCH.md §4`), and permission-category names that map cleanly to Obsidian concepts. Implementation of the shim remains an optional future cluster; ~1500 Obsidian community plugins become partially portable if it ships. Hard-sandbox is free for JS via QtWebEngine's renderer process.

### 13.4 Rendering-plugin gradient phase 2

The existing `EmbedRegistry`, `PostProcessorRegistry`, `CodeBlockProcessorRegistry`, `HoverLinkSourceRegistry` become plugin-author-reachable surfaces. Each has a different internal shape (post-processors walk rendered docs; code-block processors dispatch per-language; embed factories return widgets; hover-link sources register ID→display-name pairs). Phase-2 expansion happens only after:

- Q's panel-wrapping pattern has observed stability (few months or first community-plugin consumer).
- The Markoff library is itself stable standalone — per the 2026-04-16 design principle: *don't build plugin APIs over libraries that aren't yet stable standalone*.

### 13.5 Per-vault plugin state

Today's state is global. A future power-user feature: enable FileExplorer for vault A but disable for vault B. Adds `KConfigGroup("Plugins-<vault-uuid>")` overlay pattern. Not in Q.

### 13.6 Plugin-to-plugin IPC beyond Events

Plugins can subscribe to each other's events via the `Events` mixin. Richer IPC (direct method calls, shared state, ordered dependencies) needs explicit design — possibly an "interfaces" mechanism like KDevelop's `X-KDevelop-Interfaces`/`X-KDevelop-IRequired`. Not in Q.

## 14. Audit references

- `docs/obsidian-audit/domains/plugin.md` — Obsidian's `Plugin` base class + 18-verb register surface; Cluster Q's `Corbomite::Plugin` is the C++ translation.
- `docs/obsidian-audit/PLUGIN-API-SKETCH.md` §1-§7 — original synthesis design (JS-sandbox recommendation); Q's design diverges on the runtime (KDE-C++ primary) but preserves API-shape symmetry for the future shim track (§13.3).
- `docs/obsidian-audit/domains/plugin.md` §14 — investigated during session 2026-04-16; finding logged as an addendum (separate file) that Obsidian's "core plugins" use a parallel-but-distinct `i2` wrapper class, NOT the `Plugin` base class. Q deliberately does not replicate that split — one base class, distinguished only by the trusted flag.
- `docs/kde-power-software-design-guide/06-plugin-architecture.md` — KPluginFactory + Kate's `PluginManager` pattern + KDevelop's dependency-tracking extensions. Q's loader is modeled directly on this.
- `docs/obsidian-audit/domains/views.md` §10 — `ViewRegistry.registerView(type, factory)` contract. Q's `ViewRegistrar` proxy wraps this for `ui.views`-granted plugins.
- `docs/cluster-retros/cluster-g.md` — Workspace integration providing the leaf + view-registry substrate plugins need.

## 15. Definition of done

Cluster Q lands when:

1. All five new classes (`Plugin`, `PluginContext`, `PluginManager`, `PluginMetaData`, `PluginPermissionGrantDialog`) + nine proxy classes compile, ship in `libs/core/`, and have unit tests covering the load/unload/enable/disable/grant/revoke paths.
2. All eight InternalPlugins (`src/plugins/<slug>/`) build as `.so` MODULE targets, install to `${KDE_INSTALL_PLUGINDIR}/corbomite/`, and are discovered + loaded by `PluginManager` at app startup.
3. MainWindow no longer directly constructs `FileExplorerPanel`/`SearchPanel`/`BacklinksPanel`/`OutlinksPanel`/`OutlinePanel`/`PropertiesPanel`/`LocalGraphPanel`/`GraphViewTab` — those constructions live in their respective plugins' `createView(mainWindow)` methods.
4. Settings dialog has a Plugins page that lists all 8, shows enable checkbox + declared permissions + (where applicable) per-plugin config page link.
5. KConfig round-trip works: toggle a plugin off in Settings, restart, plugin stays off; toggle back on, plugin loads and view appears.
6. Full test suite passes (modulo the 4 known-flaky tests). New tests: per-plugin `tst_<slug>.cpp` × 8, plus `tst_plugin_manager`, `tst_plugin_context`, `tst_plugin_metadata`, `tst_plugin_lifecycle`.
7. Retro written at `docs/cluster-retros/cluster-q.md`. PROJECT-STATE Roadmap row Q → Done. INDEX.md row Q → Done.

## 16. Preserved-compat quirks

Behaviors inherited from Obsidian / Kate that Corbomite deliberately replicates rather than "improves":

- **Plugin id prefix in command IDs.** `Plugin::addCommand(cmd)` (via `CommandRegistrar`) prepends `<pluginId>:` to the command id. Matches Obsidian's `addCommand` namespacing (`domains/plugin.md §8`). Compat for `hotkeys.json` round-trip.
- **LIFO cleanup on unload.** `Component::unload` drains registered cleanups in reverse order. Matches Obsidian's `Component.unload` LIFO semantics (`domains/plugin.md §8`).
- **Ribbon id is `<pluginId>:<title>`, not `<pluginId>:<iconId>`.** Obsidian quirk (`domains/plugin.md §10`); preserved via `CommandRegistrar` API. Two `addRibbonIcon` calls with the same title clash; documented for plugin authors.
- **Plugins write `data.json` as pretty-printed JSON with 2-space indent, no trailing newline, keys in insertion order.** Matches Obsidian (`VAULT-FORMAT.md §3`). Atomic writes via `QSaveFile` (Corbomite improves on Obsidian's non-atomic `adapter.write` — this is a strict-better behavior that doesn't break compat).

---

**End of design.**
