# Plugin domain audit

Scope: extension surface, lifecycle, manifest, registration verbs, permission
model, internal-plugin inventory, discovery/install, API stability. Sources:
`/home/clinton/dev/Corbomite/docs/obsidian-audit/domains/plugin.md`,
`PLUGIN-API-SKETCH.md`, `02-extension-surfaces.md`, the eight plugin trees
under `/home/clinton/dev/Corbomite/src/plugins/`, the proxy layer in
`libs/{core,storage,vault}/{src,include}/proxies/`, and the plugin host
itself in `libs/vault/{src,include}/PluginManager.{cpp,h}`,
`libs/vault/{src,include}/Plugin.{cpp,h}`,
`libs/vault/{src,include}/PluginContext.{cpp,h}`,
`libs/core/include/corbomite/core/{Component.h,Command.h,PluginInstance.h,
PluginMetaData.h,PluginApi.h,EditorSuggest.h,EditorSuggestManager.h,
HoverLinkSourceRegistry.h,PostProcessorRegistry.h,ViewRegistry.h}` and the
build glue at `cmake/CorbomitePlugin.cmake`.

## Architecture fit (KPluginFactory .so modules vs JS subclassing)

Corbomite has chosen the most Qt/KDE-idiomatic translation possible: each
plugin is a real `.so` module built via `K_PLUGIN_FACTORY_WITH_JSON` that
exposes a single subclass of `Corbomite::Plugin` and an embedded
`metadata.json`. The CMake helper at `cmake/CorbomitePlugin.cmake:28-98`
encapsulates the boilerplate: `corbomite_add_plugin(<target> METADATA_TEMPLATE
… SOURCES … [TRUSTED] [LINK_LIBRARIES …])` configures the metadata template
into the build dir, hands moc the `-I${CMAKE_CURRENT_BINARY_DIR}` so the
embedded JSON resolves at compile time, and (critically) drops the `.so`
into `CORBOMITE_PLUGIN_DEV_DIR` for in-tree dev runs while installing into
`${KDE_INSTALL_PLUGINDIR}/corbomite` for release. The `PREFIX ""` set on
the target removes the spurious `lib` prefix so KPluginMetaData's discovery
matches the bundled metadata's id.

This is the polar opposite of Obsidian's "drop a `main.js` + `manifest.json`
into `.obsidian/plugins/<id>/` and the loader `eval()`s it" model, and it is
a deliberate decision documented in `PLUGIN-API-SKETCH.md` §1 where the
audit notes "ship JavaScript plugins in a V8/WebEngine sandbox, reusing the
Obsidian API verbatim, rather than inventing a C++ ABI that will never
achieve parity". Corbomite has currently done the opposite — it shipped the
C++ ABI first (`Corbomite::Plugin`) and the spec sketch's recommended JS
sandbox is not yet present. That is fine for the in-tree plugin set
(everything in `src/plugins/` is C++), but when third-party plugins arrive
the architecture as it stands is **not** going to load any unmodified
Obsidian plugin — every Obsidian plugin would have to be rewritten in C++,
recompiled against the matching `Corbomite::*` ABI, and shipped as a
distro-built `.so`. The compat ceiling is therefore "Obsidian-shaped API"
not "Obsidian-binary-compatible".

The trade-off is sane: KPluginFactory gives Corbomite first-class KDE
integration (KPluginMetaData, KPluginInstaller, KConfigGroup persistence,
multi-arch packaging) at the cost of dynamic load/unload of arbitrary
third-party code. `PluginManager::enablePlugin` at `libs/vault/src/
PluginManager.cpp:146-246` drives the discovery → factory load → context
construction → plugin->load(ctx) sequence cleanly, and the `~PluginManager`
destructor at `:45-61` does a reverse-order unload that mirrors KDevelop's
convention.

What is missing relative to Obsidian's architecture:

1. No JS shim. Every plugin must be C++ today.
2. The `Corbomite::Plugin` ABI is not stable (current API level is `1` per
   `libs/core/include/corbomite/core/PluginApi.h:15`); any hard ABI break
   would require recompiling all plugins.
3. No "community plugin store" concept; KPluginInstaller is the assumed
   distribution channel but no UI exists for it (see Discovery section).

## Lifecycle parity (Component → load/unload semantics)

Corbomite's `Corbomite::Component` (at `libs/core/include/corbomite/core/
Component.h:34-97`) is a deliberate, near-1:1 port of Obsidian's
`ui/components/Component.js`. The match is the strongest single piece of
plugin-domain parity in the codebase:

- LIFO unload of children, then registered cleanups, matching Obsidian's
  semantics (`Component.h:76-77` plus the implementation invariants noted
  at `Component.h:24-25`).
- `addChild()` auto-loads when the parent is already loaded (`Component.h:
  53-55`).
- `registerInterval(int ms, std::function<void()> fn)` is stop-on-unload
  (`Component.h:65`) — the closest Qt analog to Obsidian's
  `setInterval`-with-auto-clear pattern.
- `registerQObjectConnection(const QMetaObject::Connection &)`
  (`Component.h:68`) is the conscious Qt-idiomatic substitute for
  `registerEvent`/`registerDomEvent`. The header even documents the
  divergence ("Qt's native event mechanism") at `Component.h:28-30`.
- `registerCleanup(std::function<void()>)` (`Component.h:70-73`) is the
  generic LIFO escape hatch matching Obsidian's `Component.register(cb)`.

The host plugin base class is `Corbomite::Plugin` at
`libs/vault/include/corbomite/vault/Plugin.h:32-96` — multi-inherits from
`QObject` (for signals + Q_OBJECT) and `Component` (for the lifecycle
mechanism). This was a careful design call: KPluginFactory needs `QObject`
roots for `factory->create<>(parent)`, and Component is intentionally
*not* a QObject (`Component.h:32-33`), so multi-inheritance is the
clean composition. `Plugin.h:29-31` documents that the entry point is
`load(ctx)` not `Component::load()`; the impl at `libs/vault/src/Plugin.cpp:
12-17` captures the context, then drives Component::load(), which fires
`onload()` → which in turn delegates to `onLoad(m_context)` via the
final-overrides at `Plugin.cpp:19-27`. Symmetric for unload.

What this means relative to the spec:

- LIFO cleanup invariant — **PARITY**.
- Idempotent load via `_loaded` guard — **PARITY** (`Component::load`
  is idempotent per `Component.h:43-44`).
- `onload()`-throws-leaks-cleanups gap that Obsidian has and the spec
  recommends fixing (`PLUGIN-API-SKETCH.md` §2 "if `onload()` throws…
  Corbomite should improve: on throw, auto-unload()") — **NOT FIXED**.
  Corbomite's `Plugin.cpp:19-22` calls `onLoad(m_context)` directly with
  no try/catch, so an exception from a plugin's `onLoad` will propagate
  out of `Component::load`, leaving the plugin half-loaded.
- `onUserEnable()` (one-shot welcome hook) — **MISSING**. Not a virtual
  on `Plugin.h`, no caller wired in `PluginManager::enablePlugin`.
- `onExternalSettingsChange()` (data.json mtime-watch) — **MISSING**.
  `PluginDataStore` at `libs/vault/include/corbomite/vault/
  PluginDataStore.h` does writes via `QSaveFile` (atomic, an improvement
  over Obsidian per `PLUGIN-API-SKETCH.md` §11.13) but no
  `QFileSystemWatcher` is wired, no `onExternalSettingsChange` virtual
  exists. Plugins editing `data.json` from a sister tool (git pull,
  Obsidian Sync) will not pick up the change without a manual disable/
  enable cycle.
- `_userDisabled` → `detachLeavesOfType` cascade — **PARTIAL/UNCLEAR**.
  `disablePlugin(persist)` at `PluginManager.cpp:248-265` deletes the
  instance, which destroys plugin-owned views via the `QObject`
  parent-child tree — but the spec invariant is "every leaf of every
  view-type the plugin registered detaches", and Corbomite's
  `ViewRegistrar` at `libs/core/include/corbomite/core/proxies/
  ViewRegistrar.h:31-42` only unregisters the type from `ViewRegistry`,
  it does not iterate live leaves to close them. If a plugin registered
  `.kanban` and the user has a `.kanban` tab open when they disable the
  plugin, the tab is orphaned (factory gone, view still alive).

Notable Corbomite-original lifecycle additions that are *not* in
Obsidian's `Plugin`:

- `Plugin::createView(MainWindow *mainWindow)` (`Plugin.h:50`) — per-window
  view factory. Obsidian plugins create views via `registerView`
  + `workspace.openLink`/leaf detach, but Corbomite plugins instead
  expose a top-level "give me your dock view" hook. This is sensibly
  KDE-idiomatic and surfaces in `MainWindow.cpp` around the `pluginLoaded`
  signal at `:244-247` — the host instantiates the view per window.
- `Plugin::saveSessionState`/`loadSessionState` (`Plugin.h:64-70`) —
  per-plugin session persistence into `workspace.json`'s
  `_corbomite.plugins.<pluginId>` key. Obsidian leaves session state to
  the per-view ItemView/TextFileView; Corbomite hoists it to the plugin.
  See `MainWindow.cpp:927, 945` for the call sites.
- `Plugin::focus(QObject *view)` (`Plugin.h:57`) — focus dispatcher so
  plugins like Search can hand focus to a non-root child widget
  (the QLineEdit, not the surrounding tree).
- `Plugin::configPages()` / `configPage(int, QWidget *)` (`Plugin.h:73-76`)
  — KTextEditor::ConfigPage-shaped settings page entry points (the
  KConfigGroup-backed analog of `addSettingTab`).

## Manifest format parity

Corbomite uses **embedded** KPluginMetaData JSON via
`K_PLUGIN_FACTORY_WITH_JSON("metadata.json", …)` at every plugin's
`*Plugin.cpp` (e.g. `src/plugins/backlinks/BacklinksPlugin.cpp:59-60`).
The CMake helper configures a `metadata.json.in` template into the build
dir before AUTOMOC sees it. Obsidian uses an external `manifest.json` next
to `main.js`. Field parity:

| Obsidian field | Corbomite field | Notes |
|---|---|---|
| `id` | `KPlugin.Id` | Optional in Corbomite — defaults to the .so basename. Some plugins set it explicitly (`bookmarks/metadata.json.in:3`, `backlinks/metadata.json.in:3`); others omit. |
| `name` | `KPlugin.Name` | Direct. |
| `version` | `KPlugin.Version` | Direct. All in-tree plugins use `"1.0"`. |
| `minAppVersion` | `X-Corbomite-MinVersion` | Custom X-key, parsed by `PluginMetaData::minAppVersion()` at `libs/core/include/corbomite/core/PluginMetaData.h:35`. Gated at discovery time and re-checked at enable time (`PluginManager.cpp:111-118, 155-162`). |
| `description` | `KPlugin.Description` | Direct. |
| `author` | `KPlugin.Authors[].Name` | KDE convention is array of `{Name,Email}`; richer than Obsidian. |
| `authorUrl` | — | **MISSING**. No equivalent in any in-tree metadata. |
| `fundingUrl` | — | **MISSING**. |
| `isDesktopOnly` | — | **N/A** (Corbomite is desktop-only). |
| `dir` (loader-injected) | — | Implicit — KPluginMetaData carries `fileName()`. |
| — | `X-Corbomite-Trusted` | Origin marker; CMake helper sets `true` for in-tree plugins, `false` otherwise. Demoted at runtime by `PluginMetaData::trusted()` if origin is User regardless of JSON. |
| — | `X-Corbomite-Permissions` | List of capability tokens (see Permission model). |
| — | `X-Corbomite-ApiLevel` | Integer ABI break marker; defaults to 1 if absent. Bookmarks declares `"X-Corbomite-ApiLevel": 1` at `bookmarks/metadata.json.in:16`; everyone else relies on the default. |
| — | `X-Corbomite-DockArea` | Where the plugin's tool view docks (`left`/`right`). All in-tree plugins set this. |
| — | `X-Corbomite-DockIcon` | Theme icon for the dock tab. |
| — | `X-Corbomite-DockTitle` | Display string for the dock tab. |
| — | `KPlugin.Icon` | KDE convention — separate from DockIcon. Some plugins duplicate. |
| — | `KPlugin.License` | Direct (KDE convention; "GPL-3.0-or-later" everywhere). |
| — | `KPlugin.Category` | Direct (`"Core"` for all in-tree plugins). |
| — | `KPlugin.EnabledByDefault` | Direct. All in-tree plugins set `true`. |

The custom `X-Corbomite-Dock*` keys are an unstated extension to
Obsidian's manifest model — they let the host pre-allocate a dock slot
without instantiating the plugin's `createView`. This is a sensible
optimisation but it is *not* in `PLUGIN-API-SKETCH.md` and would need
documenting before third-party plugins ship.

The `corbomite_add_plugin` helper preserves the `X_CORBOMITE_TRUSTED`
substitution model cleanly (`CorbomitePlugin.cmake:28-48`), and the
in-tree `TRUSTED` keyword is purely a social-convention switch — at
runtime `PluginMetaData::trusted()` always returns `false` for Origin::User
regardless of what the JSON declares (header docstring at
`PluginMetaData.h:16-19`). This belt-and-braces design is correct.

## Registration verb coverage matrix (each of the ~13 verbs above)

| # | Obsidian verb | Corbomite equivalent | Status | Citation |
|---|---|---|---|---|
| 1 | `addCommand({id, name, callback, hotkeys, editorCallback, checkCallback, editorCheckCallback})` | `Corbomite::CommandRegistrar::addCommand(Command &)` (proxy) → `CommandRegistry::addCommand` | **PARTIAL** | Proxy at `libs/core/include/corbomite/core/proxies/CommandRegistrar.h:28`; spec at `libs/core/include/corbomite/core/Command.h:38-54`. All four callback shapes (`callback`, `checkCallback`, `editorCallback`, `editorCheckCallback`) are present in `Command`. Hotkey defaults are not in `Command` — there is no `hotkeys: Hotkey[]` field. `mobileOnly` is in `Command` (carry-over). `repeatable` is **MISSING**. The `cmd.id` mutation-in-place that Obsidian does is preserved (header docstring at `CommandRegistrar.h:25-27`). `addCommandRaw` at `:35` exists for the bookmarks-style canonical-namespace case. |
| 2 | `addRibbonIcon(icon, title, cb)` | `Corbomite::RibbonToolBar::addRibbonIcon(Handle id, const QIcon &, const QString &title, std::function<void()>)` | **PARTIAL — NOT EXPOSED TO PLUGINS** | Implemented at `src/app/RibbonToolBar.h:40-43` and `src/app/RibbonToolBar.cpp:26`. Sets up a `KToolBar` next to the main toolbar with the documented Obsidian "id collision on duplicate title" quirk preserved (`RibbonToolBar.h:39-42`). However there is no proxy facade for plugins — no `RibbonRegistrar` in `libs/core/include/corbomite/core/proxies/`. Plugins cannot today add ribbon icons. None of the 8 in-tree plugins call this. |
| 3 | `addStatusBarItem()` | — | **MISSING** | No `QStatusBar` plugin proxy. No `StatusBarRegistrar`. The "first illegal char" sanitiser bug to preserve per `PLUGIN-API-SKETCH.md` §11.3 has no Corbomite implementation site to preserve in. |
| 4 | `addSettingTab(tab)` | `Plugin::configPages()` + `Plugin::configPage(int, QWidget *)` | **PARTIAL** | KTextEditor::ConfigPage-shaped at `libs/vault/include/corbomite/vault/Plugin.h:73-76`. The KDE idiom is `KCModule`/`KPageDialog`-driven, *not* a per-plugin tab list, so this is reasonable Qt translation. There is also a `PluginsPage` in `src/dialogs/PluginsPage.h` that lists plugins + per-plugin "Configure…" buttons — the spec's `addSettingTab` shape is not preserved verbatim, but the user-visible feature works. None of the 8 in-tree plugins override `configPages` today. |
| 5 | `registerView(type, factory)` | `Corbomite::ViewRegistrar::registerView(QString type, ViewFactory)` | **PARTIAL** | Proxy at `libs/core/include/corbomite/core/proxies/ViewRegistrar.h:31`. Cleans up registered types on destruction. **Gap:** the on-disable detach-leaves-of-type cascade documented in `domains/plugin.md §10` is not implemented (the proxy only calls `unregisterView`; the open leaves of that type are orphaned). |
| 6 | `registerExtensions(extensions, viewType)` | `Corbomite::ViewRegistrar::registerExtensions(QStringList, QString)` | **PARTIAL** | At `ViewRegistrar.h:32`. Atomicity unclear — Obsidian does the array as one operation; Corbomite's host signal `ViewRegistry::extensionsUpdated` (at `libs/core/include/corbomite/core/ViewRegistry.h:45`) fires once per call but the proxy doesn't batch. None of the 8 in-tree plugins use this (they all dock as fixed views, not file-extension handlers). |
| 7 | `registerHoverLinkSource(id, info)` | `Corbomite::HoverLinkSourceRegistry::registerSource(HoverLinkSource)` | **PARTIAL — NOT EXPOSED TO PLUGINS** | Registry at `libs/core/include/corbomite/core/HoverLinkSourceRegistry.h:22-47` is fully present and registers the four built-ins (`registerBuiltins()` at `:38`). However, there is **no proxy facade** in `libs/core/include/corbomite/core/proxies/` — plugins cannot register their own. Plugin docstring at `:19-21` even calls this out: "When Cluster N's plugin layer lands, plugin views will call registerSource() at onload…" — the registry exists but the plugin hookup never landed. |
| 8 | `registerEditorExtension(extension)` (CodeMirror 6) | — | **MISSING (by design)** | Markoff is not CodeMirror, so a literal port is impossible. `PLUGIN-API-SKETCH.md` §5.6 acknowledges this and recommends a different `Markoff::EditorPlugin` model. No such interface exists yet in `libs/markoff*`. |
| 9 | `registerEditorSuggest(suggest)` | `Corbomite::EditorSuggestManager::registerSuggest(EditorSuggest *)` | **PARTIAL — NOT EXPOSED TO PLUGINS** | Manager at `libs/core/include/corbomite/core/EditorSuggestManager.h:20-47` implements the insertion-order-first-non-null-wins dispatch correctly (`:31`), and `EditorSuggest` at `libs/core/include/corbomite/core/EditorSuggest.h:41` is a Component subclass with the four Obsidian overrides (`onTrigger`/`getSuggestions`/`selectSuggestion`, missing `renderSuggestion`). No plugin proxy in `proxies/`; not in `PluginContext`'s accessor list (`libs/vault/include/corbomite/vault/PluginContext.h:76-86`). |
| 10 | `registerMarkdownPostProcessor(cb, sortOrder)` | `Corbomite::Core::PostProcessorRegistry::registerProcessor(int priority, PostProcessorFn)` | **PARTIAL — NOT EXPOSED TO PLUGINS** | Registry at `libs/core/include/corbomite/core/PostProcessorRegistry.h:40-67`. Synchronous-by-contract semantics documented at `:32-39` (deviation from Obsidian, which awaits promises). Returns a Handle for unregister. **No plugin proxy.** Not in `PluginContext` accessor list. The `post-processor-change` event that Obsidian fires on register/unregister has no Corbomite signal. |
| 11 | `registerMarkdownCodeBlockProcessor(language, cb, sortOrder)` | — | **MISSING** | No code-block-language registry was found in `libs/core/include/corbomite/core/MarkoffRenderEngine.h` or sibling render-engine headers. The Mermaid renderer (`MermaidRenderer.h`) is hard-wired. The `\`\`\`dataview` / `\`\`\`tasks` / `\`\`\`chart` plugin extension point is wholly absent. |
| 12 | `registerObsidianProtocolHandler(action, handler)` | — | **MISSING** | No `KDBusService(Unique)` wiring + no `obsidian://` / `corbomite://` URL handler in the codebase. The 11 Obsidian built-in protocol actions (`open`, `new`, `search`, `show-plugin`, `show-theme`, `show-release-notes`, `debug-info`, `publish-sites`, `sync-setup`, `vault-setup`, `hook-get-address`) are all unimplemented. |
| 13 | `loadData()` / `saveData(data)` → `data.json` | `PluginContext::loadData()` / `PluginContext::saveData(QJsonObject)` | **IMPLEMENTED + IMPROVED** | `libs/vault/src/PluginContext.cpp:186-200` delegates to `PluginDataStore` at `libs/vault/include/corbomite/vault/PluginDataStore.h:14-30`. Atomic write via QSaveFile (`PluginDataStore.h:23`) — strictly better than Obsidian's open-write-close per `PLUGIN-API-SKETCH.md` §11.13. Gated by the `"config"` permission token (`PluginContext.cpp:188, 196`). **However:** the external-edit watcher (`onExternalSettingsChange`) is **NOT** wired up — see Lifecycle parity above. The self-edit suppression pattern (`_lastDataModifiedTime` mtime echo) is therefore moot but absent. |

Additional Component-inherited verbs:

| Component verb | Corbomite | Status |
|---|---|---|
| `register(fn)` | `Component::registerCleanup(std::function<void()>)` | **PARITY** (`Component.h:70-73`) |
| `registerEvent(EventRef)` | — | **MISSING** (no `Events` mixin yet on the plugin-facing classes; spec'd in `PLUGIN-API-SKETCH.md` §4) |
| `registerDomEvent` | `Component::registerQObjectConnection` | **PARITY** (Qt-idiomatic substitute, `Component.h:68`) |
| `registerInterval(ms, fn)` | `Component::registerInterval(int ms, std::function<void()>)` | **PARITY** (`Component.h:65`) |
| `registerScopeEvent` | — | **MISSING** (no Scope/keymap plugin surface) |
| `addChild` / `removeChild` | `Component::addChild` / `removeChild` | **PARITY** (`Component.h:53-58`) |

## Permission model (Corbomite-original — describe and assess)

This is Corbomite's biggest **architectural addition** over Obsidian, which
has no permission model at all. The model is sane, defensible, and
reasonably complete — easily the audit's strongest finding.

**Tokens (12 total).** Defined as constants in
`libs/vault/src/PluginContext.cpp:21-32`:

- `vault.read`, `vault.write`, `vault.events` — VaultProxy gates each
  method on the appropriate token; signal forwarding only happens when
  `vault.events` is granted (`libs/vault/include/corbomite/vault/proxies/
  VaultProxy.h:106-117`).
- `metadata.read` — gates `MetadataCacheReader`, `SearchProxy`, and the
  `FileManagerProxy::generateMarkdownLink` method (the last because it
  reads the host-side `MetadataCache`).
- `workspace` — gates `WorkspaceController`.
- `ui.commands`, `ui.views`, `ui.menus` — gate the UI registrars.
- `network` — gates a granted `QNetworkAccessManager*` reference.
- `secrets` — gates `SecretStorage` (KWallet/QtKeychain backend).
- `process` — gates `ProcessSpawner` (audit-logged QProcess).
- `config` — gates `KConfigGroup config()` and the `loadData()`/`saveData()`
  pair.

**Declaration.** Plugins list their tokens in `metadata.json` under
`X-Corbomite-Permissions: [...]`. Example (file-explorer is the heaviest
consumer): `["vault.read", "vault.write", "vault.events", "workspace",
"ui.views", "ui.menus", "ui.commands"]` at `src/plugins/file-explorer/
metadata.json.in:13`. Read by `PluginMetaData::permissions()` at
`libs/core/include/corbomite/core/PluginMetaData.h:28`.

**Grant flow.** `PluginManager::enablePlugin` at
`libs/vault/src/PluginManager.cpp:172-204`:

1. Compute version/api-level gates first (refuse unloadable plugins
   *before* prompting).
2. If the plugin is **trusted** (built-in, origin == System and JSON
   `X-Corbomite-Trusted == true`), auto-grant every declared permission.
3. Otherwise, look up previously-granted permissions in KConfig under
   `[PluginPermissions]` group, key `<id>Granted` (`:285-292`).
4. Compute `ungranted = declared \ granted`. If non-empty, route through
   either `m_promptHandler` (test seam) or the user-facing
   `PluginPermissionGrantDialog` (production).
5. The dialog uses careful UX language per its docstring at
   `libs/vault/include/corbomite/vault/PluginPermissionGrantDialog.h:18-19`:
   "This plugin DECLARES it needs …" not "This plugin CAN …" — important
   distinction.
6. Save the granted set back to KConfig (`saveGrantedPermissions` at
   `:294-303`, sorted for deterministic on-disk).

**Enforcement.** Each proxy is constructed with the `granted` QSet and
self-checks before every method, returning empty/false/nullptr on denial
(see `VaultProxy.h:106-119`, `SearchProxy.h:58-63`,
`SecretStorage.h:54`). The `PluginContext::xxx()` accessors do the
*coarse* check ("does the plugin hold any vault.* permission?") and return
nullptr otherwise (`PluginContext.cpp:69-84`); the proxy then does the
*fine* check per method.

**Trust posture.** `PluginMetaData::trusted()` at `PluginMetaData.h:32`
returns false unconditionally for Origin::User regardless of JSON declaration
— so a malicious user-installed plugin cannot self-grant by lying. The
CMake helper's `TRUSTED` keyword only sets the JSON-side claim; the
runtime origin gate is what actually decides.

**Gaps and concerns:**

1. The audit-spec `domains/plugin.md` says nothing about a permission model
   (Obsidian has none); Corbomite has invented one. There is no API-stability
   guarantee for the token list — adding/renaming tokens would invalidate
   every previously-granted KConfig entry. The token set should be frozen
   in a header (right now it is constants in `PluginContext.cpp:21-32`,
   which is the wrong place — they should be in
   `libs/core/include/corbomite/core/PluginPermissions.h` or similar).
2. The `secrets` permission gate is double-checked: `PluginContext::secrets()`
   gates on the token (`PluginContext.cpp:157-164`) **and** then
   `SecretStorage` self-gates via `hasSecretsPermission()` (`SecretStorage.h:54`).
   The proxy-side gate is dead code in production but useful for the
   single-arg constructor that bypasses `PluginContext`.
3. There is no permission for posting notifications, setting the clipboard,
   reading user idle status, or accessing the IPC bus — areas Obsidian
   plugins do touch via their general escape hatches (Electron). For C++
   plugins this matters less since they have access to anything they're
   linked against; but for any future sandbox layer these would need
   tokens.

Overall this is a well-considered addition that is hooked up end-to-end
(metadata → discovery → grant → KConfig persistence → proxy enforcement).
The finishing-school work is documenting the token set and surfacing it
in the developer-facing API docs.

## Internal-plugin inventory (Obsidian's ~25 vs Corbomite's 9 — which gaps)

Obsidian's internal-plugin set lives in `app.internalPlugins` and is
documented in `domains/02-extension-surfaces.md:42` as a 31-ish list
auto-loaded at `core/App.js:611-641`. The audit-spec's typical enumeration
covers ~25 user-visible: backlinks, outline, file-explorer, graph,
local-graph, search, command-palette, bookmarks, daily-notes, page-preview,
file-recovery, properties, sync, templates, zk-prefixer, canvas, bases,
note-composer, random-note, slash-commands, slides, unique-note,
word-count, workspaces, switcher, tag-pane, outgoing-links (= outlinks),
publish, starred-legacy.

Corbomite's `src/plugins/` directory contains exactly 9 (one per
subdirectory): backlinks, bookmarks, file-explorer, graph-view, local-graph,
outline, outlinks, properties, search.

| Obsidian internal | Corbomite | Notes |
|---|---|---|
| backlinks | **PRESENT** | `src/plugins/backlinks/` — view + `backlinks:open` command. |
| outline | **PRESENT** | `src/plugins/outline/`. |
| file-explorer | **PRESENT** | `src/plugins/file-explorer/` — heaviest plugin, only one with `vault.write` + `ui.menus`. |
| graph (global) | **PRESENT (renamed)** | `src/plugins/graph-view/`. |
| local-graph | **PRESENT** | `src/plugins/local-graph/`. |
| search | **PRESENT** | `src/plugins/search/`. |
| properties | **PRESENT** | `src/plugins/properties/` (frontmatter editor). |
| outgoing-links | **PRESENT (renamed outlinks)** | `src/plugins/outlinks/`. |
| bookmarks | **PRESENT** | `src/plugins/bookmarks/`. |
| command-palette | **MISSING** | Not a plugin in Corbomite — implemented as built-in via `KCommandBar` somewhere outside `src/plugins/`. Could/should be a plugin to round out the model (consistency win). |
| switcher (Quick Switcher) | **MISSING** | Built-in or unimplemented. Should be a plugin. |
| daily-notes | **MISSING** | Not in Corbomite at all. |
| page-preview (hover popover) | **MISSING (as plugin)** | Hover-link plumbing exists (`HoverLinkSourceRegistry`) but no plugin orchestrates the popover. Built-in elsewhere. |
| file-recovery | **MISSING** | No autosave-snapshot recovery plugin. |
| sync | **MISSING** | Commercial Obsidian feature. Out of scope. |
| publish | **MISSING** | Commercial Obsidian feature. Out of scope. |
| templates | **MISSING** | Common-need feature. Should be a plugin. |
| zk-prefixer | **MISSING** | Niche but trivial. |
| canvas | **NOT-A-PLUGIN** | `libs/canvas/` is a library, not a plugin. Obsidian has it as a plugin. |
| bases | **NOT-A-PLUGIN-YET** | Per CLAUDE.md "Cluster K done" memory — Bases is a built-in view, not a plugin. Spec calls it "internal plugin" in Obsidian. |
| note-composer | **MISSING** | Merge-by-link operations. |
| random-note | **MISSING** | Trivial. |
| slash-commands | **MISSING** | UI surface not present. |
| slides | **MISSING** | Niche. |
| unique-note | **MISSING** | Timestamped note creator. |
| word-count | **MISSING** | Common UX expectation. Trivial. |
| workspaces | **MISSING (as plugin)** | Workspace-save/restore exists at the `MainWindow` level (Cluster G work) but is not a plugin. |
| tag-pane | **MISSING** | Tag list/sidebar is implicit; Obsidian has a dedicated panel. |
| starred (legacy) | **N/A** | Folded into bookmarks in Obsidian. |

**Missing-but-arguably-should-be-plugins:** command-palette, switcher,
templates, daily-notes, word-count, file-recovery, page-preview,
note-composer. These are all features Obsidian users expect and which
naturally fit the plugin shape Corbomite already has. A particularly
glaring one is **page-preview** because the registry side already exists
(`HoverLinkSourceRegistry::registerBuiltins`); a tiny plugin-shaped
popover orchestrator would close the loop.

**Architectural risk:** the in-tree built-ins like Bases and Canvas being
*not* plugins means there are two parallel "internal feature" mechanisms
in Corbomite. The Obsidian model is "everything is a plugin, including
built-ins"; that uniformity is part of why Obsidian's plugin API is so
hardened. Corbomite should consider making Bases/Canvas plugins (with
`TRUSTED` so they auto-grant everything) so the same registration
codepaths get exercised at startup.

## Plugin discovery / installation workflow

**Discovery.** `PluginManager::discoverPlugins` at
`libs/vault/src/PluginManager.cpp:124-131`:

```
ingest(KPluginMetaData::findPlugins(m_systemPath), Origin::System);
ingest(KPluginMetaData::findPlugins(m_userPath), Origin::User);
```

System path defaults to the relative subdir `"corbomite"`, which
`KPluginMetaData::findPlugins` resolves against every entry of
`QCoreApplication::libraryPaths()` (typically `${KDE_INSTALL_PLUGINDIR}/
corbomite`). User path defaults to `XDG_DATA_HOME/corbomite/plugins`
(`PluginManager.cpp:29-33`). Both paths are overridable for tests.

**Dev path override.** `CorbomiteApp::CorbomiteApp` at
`src/app/CorbomiteApp.cpp:19-25` overrides the system path to
`CORBOMITE_PLUGIN_DEV_DIR` when the macro is defined, so dev builds
discover plugins from the build tree without `make install`. Matches
the dev-build isolation pattern in CLAUDE.md.

**Origin → trust.** `ingest` (`PluginManager.cpp:95-122`) stamps every
discovered plugin with its origin and pre-computes the version/api-level
gate state. No third-party install path can launder itself into
trusted-System status.

**Enable.** `PluginManager::enablePlugin(id)` at `:146-246` does the
gate-check → permission-grant → factory-load → context-construction →
`plugin->load(ctx)` sequence described above. The factory load uses
`KPluginFactory::loadFactory(metaData)` then `factory->create<QObject>(this)`
followed by `qobject_cast<Plugin *>` (`:211-229`). The cast-failure path
emits a useful diagnostic and deletes the misshapen object.

**Persistence.** Enable state lives in KConfig under `[Plugins]` group,
key `<id>Enabled` (`writeEnabledState` at `:305-311`). On startup,
`loadEnabledStateFromConfig` at `:267-283` walks every discovered plugin
and re-enables those persisted on, falling back to
`metaData.isEnabledByDefault()` (or trusted-always-on for built-ins).

**No installer UI.** There is no equivalent to Obsidian's
"Community Plugins" browser. Third-party plugins must be installed by
the user via `cp foo.so ~/.local/share/corbomite/plugins/`. KDE has
`KPluginInstaller`/`KNS3` (KNewStuff) which would be the idiomatic
attach point, but it is not wired. `PluginsPage` at
`src/dialogs/PluginsPage.h:24-63` lists discovered plugins and toggles
their enable state but does not install or update.

**No vault-scoping of installation.** Obsidian installs plugins per-vault
(`<vault>/.obsidian/plugins/<id>/`); Corbomite installs system-wide
(`~/.local/share/corbomite/plugins/`). This is a deliberate divergence
that follows Linux package conventions but breaks the "carry your vault
between machines and bring its plugins with it" Obsidian assumption.
Per-plugin **data** is still vault-scoped (via
`PluginContext::setPluginDataDir`, called from MainWindow when a vault
opens — though I didn't trace that wiring fully), but the .so itself is
not.

## Implemented

- KPluginFactory-based plugin host with full lifecycle (`PluginManager` +
  `Plugin` + `Component`).
- Permission system end-to-end: declare in metadata → grant via dialog
  → persist in KConfig → enforce in proxies. Origin-based trust override.
- Atomic per-plugin `data.json` via `PluginDataStore` (QSaveFile-backed).
- Per-plugin command namespacing with the Obsidian "mutate cmd.id in
  place" quirk preserved (`CommandRegistrar.h:25-27`).
- Per-plugin session-state persistence (Corbomite-original; not in
  Obsidian).
- `corbomite_add_plugin` CMake helper with TRUSTED gate, dev-dir override,
  and KDE-distro install path.
- Version + API level discovery-time gating (`X-Corbomite-MinVersion`,
  `X-Corbomite-ApiLevel`).
- Permission grant dialog with "DECLARES it needs" UX language.
- Plugins-management settings page (`PluginsPage`).
- 9 in-tree built-in plugins covering the most-used Obsidian internal
  views (backlinks, bookmarks, file-explorer, graph-view, local-graph,
  outline, outlinks, properties, search).
- Permission-gated proxies for: Vault (R/W/Events), FileManager, Metadata
  (read), Search (FTS), Workspace, CommandRegistry, ViewRegistry, Menus,
  SecretStorage (KWallet/QtKeychain), ProcessSpawner (audit-logged),
  Network (QNetworkAccessManager).
- HoverLinkSource, EditorSuggest, PostProcessor *registries* (host-side
  data structures) — but with no plugin-facing proxy yet.

## Partial / divergent

- **Ribbon icons.** `RibbonToolBar::addRibbonIcon` exists at the host but
  is not exposed via a `PluginContext` accessor or proxy. Plugins cannot
  add ribbon icons.
- **Hover-link sources.** `HoverLinkSourceRegistry` is built and
  initialised with builtins, but no plugin proxy. Header docstring at
  `libs/core/include/corbomite/core/HoverLinkSourceRegistry.h:19-21`
  acknowledges this is pending Cluster N.
- **Editor suggesters.** `EditorSuggestManager` + `EditorSuggest` ABI in
  place; missing the `renderSuggestion` virtual relative to Obsidian
  (`EditorSuggest.h:43-62`). No plugin proxy.
- **Markdown post-processors.** `PostProcessorRegistry` exists at
  `PostProcessorRegistry.h:40-67` but the contract divergence
  ("synchronous-by-contract") relative to Obsidian's "await ctx.promises"
  is documented at `:32-39` and may surface portability friction for
  Mermaid/MathJax-style async processors. No plugin proxy.
- **Settings tab.** `Plugin::configPages()` is the KDE-idiomatic shape, not
  Obsidian's `addSettingTab(PluginSettingTab)`. None of the in-tree
  plugins use it.
- **`registerView` cleanup.** The `ViewRegistrar` does not iterate live
  leaves on plugin disable, so open tabs of a disabled plugin's view-type
  are orphaned (factory unregistered, leaf still alive).
- **`Plugin::createView` invocation.** Corbomite's "the host calls
  createView per window" model is non-Obsidian. Sensible for KDE multi-
  window UX but not in PLUGIN-API-SKETCH.

## Missing

- **Status bar items** (`addStatusBarItem`).
- **Markdown code-block processors** (`registerMarkdownCodeBlockProcessor`).
- **Editor extensions** (`registerEditorExtension` / Markoff equivalent).
  Acknowledged as "different model" in the spec but no Markoff alternative
  shipped.
- **Protocol handlers** (`obsidian://` / `corbomite://`).
- **`onUserEnable()`** lifecycle hook.
- **`onExternalSettingsChange()`** + `data.json` mtime watcher.
- **`addRibbonIcon` plugin proxy** (host-side exists but no plugin facade).
- **`registerHoverLinkSource` plugin proxy** (registry exists; facade
  missing).
- **`registerEditorSuggest` plugin proxy** (manager exists; facade missing).
- **`registerMarkdownPostProcessor` plugin proxy** (registry exists; facade
  missing).
- **Plugin-side `Events` mixin** (`PLUGIN-API-SKETCH.md` §4 hybrid model
  not implemented; plugins use Qt signals direct).
- **`MarkdownRenderChild`** (post-processor lifecycle wrapper).
- **Modal / SuggestModal / FuzzySuggestModal** subclass entry points
  exposed to plugins (KDialog/KMessageBox exist but no Obsidian-shape
  port).
- **Plugin-installable icon registry** (`addIcon`/`removeIcon`).
- **`requireApiVersion()` / `apiVersion`** runtime exports — only the
  `PluginMetaData::apiLevel()` discovery-time gate exists.
- **Community plugin browser/installer UI** (no KNewStuff integration).
- **Per-vault plugin install dir** (Corbomite installs system-wide).
- **Auto-unload on `onLoad` throw** (still a gap; matches Obsidian but
  the spec recommends fixing).
- **Detach-leaves-of-type on plugin disable** cascade.
- **Internal plugins:** command-palette, switcher, daily-notes, templates,
  word-count, file-recovery, page-preview, note-composer (all listed
  under inventory above).

## Notable translation successes

1. **Permission model** is the standout: a non-trivial original addition
   that closes a real Obsidian security gap (plugins have full system
   access in Obsidian) without breaking the "thin facade over registries"
   shape of the API. The Origin-demotes-trust rule
   (`PluginMetaData.h:16-19`) is the right belt-and-braces design.
2. **`Component` port** is precise and Qt-idiomatic. The `registerInterval`
   / `registerQObjectConnection` / `registerCleanup` triplet covers the
   90% case for cleanup parity, and the explicit "Component is *not* a
   QObject" decision (`Component.h:32-33`) avoids MOC + MI traps.
3. **`Plugin` multi-inheritance** of `QObject + Component` is the cleanest
   resolution of the KPluginFactory-needs-QObject vs Component-isn't-one
   tension. Documented well at `Plugin.h:18-31`.
4. **`corbomite_add_plugin`** CMake helper is a small but real win — every
   in-tree plugin's `CMakeLists.txt` is 12 lines (e.g. `src/plugins/
   backlinks/CMakeLists.txt`). The TRUSTED keyword + dev-dir output dir
   are exactly the right ergonomic decisions.
5. **Atomic `data.json`** via `QSaveFile` is a documented improvement
   over Obsidian's silent-truncation-on-crash gap (`PLUGIN-API-SKETCH.md`
   §11.13). One of the rare cases where Corbomite is unambiguously
   *better* than the spec.
6. **`X-Corbomite-MinVersion` + `X-Corbomite-ApiLevel`** dual-axis
   versioning. Discovery-time gate state is recorded eagerly so
   `PluginsPage` can render "Requires Corbomite >= X.Y.Z" without
   instantiating the plugin. The defence-in-depth re-check at enable
   time is the right paranoia level.
7. **Trust + permission grant flow** correctly checks gates *before*
   prompting the user (`PluginManager.cpp:151-170`) — never asks for
   permissions on a plugin that won't load anyway.

## Notable concerns / suspected bugs

1. **`onLoad` throw leaks state.** `Plugin::onload` at
   `libs/vault/src/Plugin.cpp:19-22` calls `onLoad(m_context)` directly
   with no try/catch. An exception from a plugin's `onLoad` propagates
   out of `Component::load`, leaving `_loaded = true` and any registered
   cleanups dangling, exactly the gap `PLUGIN-API-SKETCH.md` §2 said to
   fix. **Recommend:** wrap in try/catch, on throw call
   `Component::unload()`, then rethrow or log+swallow.
2. **`disablePlugin` does not detach leaves of plugin-registered
   view-types.** `PluginManager.cpp:248-265` deletes the instance and
   context but the open `WorkspaceLeaf`s of plugin-registered types are
   only cleaned up by the `QObject` parent-child cascade (if the leaf was
   parented under the plugin's view, which is not a guaranteed ownership
   model). The Obsidian invariant from `domains/plugin.md §10` (`registerView`
   row) explicitly says "On unload, if `_userDisabled`, also
   `detachLeavesOfType(type)`" — the Corbomite equivalent is missing.
3. **`HoverLinkSourceRegistry`, `PostProcessorRegistry`,
   `EditorSuggestManager` exist with no plugin facade.** Internal-only
   today. Header docstrings at all three explicitly say "when plugins
   land". They've landed; the facades haven't.
4. **`Command::hotkeys` field missing.** The spec's `CommandSpec` has
   `hotkeys?: Hotkey[]` (default-bindings hint); `Command` at
   `libs/core/include/corbomite/core/Command.h:38-54` has no such field.
   This means `addCommand` cannot carry default keybindings — they
   must be configured separately via KActionCollection. Compat-break
   for Obsidian plugins that bake default hotkeys into their command
   declarations.
5. **Command id namespace double-prefix bug preserved but undocumented.**
   `CommandRegistrar::addCommand` mutates `cmd.id` to `<pluginId>:<id>`
   (header at `:25-27`). A second call with the same `Command &` would
   produce `<pluginId>:<pluginId>:<id>` per Obsidian's quirk. The header
   doesn't say whether Corbomite preserves this; need a unit test to
   confirm. `addCommandRaw` at `:35` is the escape hatch for canonical-
   namespace registration.
6. **`SecretStorage::listSecrets` returns only session-observed keys.**
   Header at `libs/core/include/corbomite/core/proxies/SecretStorage.h:
   42-49` notes QtKeychain doesn't expose enumeration. Plugins relying
   on `app.secretStorage.listSecrets()` for auto-discovery of stored
   credentials will see different behaviour. Documented but not in the
   spec's compat matrix.
7. **No `Events` mixin.** `PLUGIN-API-SKETCH.md` §4 specifies a hybrid
   approach (Qt signals for internal + Events facade for plugin compat).
   Today plugins connect Qt signals directly (e.g. VaultProxy is a QObject
   with `created`/`modified`/etc. signals at `VaultProxy.h:86-99`). Means
   Obsidian-shape `vault.on("create", cb)` plugin code will not work.
8. **`PluginContext` accessor list incomplete.** Missing accessors for
   the registries that *do* exist host-side: hover-link sources,
   editor suggesters, post-processors. These should be added to round
   out the proxy facade.
9. **`Plugin::createView` returns `QObject *`.** A `QWidget *` would be
   stronger typing. The `reinterpret_cast<QWidget *>(mainWindow)` at
   `src/plugins/backlinks/BacklinksPlugin.cpp:54` is a code smell —
   `MainWindow` is forward-declared but the cast says "trust me, it's
   really a QWidget". Should at least be `qobject_cast` or a concrete
   `QMainWindow *`.
10. **No `data.json` watcher.** Plugins editing data.json from a sister
    tool will not pick up the change. `PLUGIN-API-SKETCH.md` §7 specifies
    `QFileSystemWatcher` + 50ms debounce + mtime-hint comparison;
    nothing of the sort exists in `PluginDataStore`.
11. **Permission tokens are string constants in a .cpp.** `libs/vault/
    src/PluginContext.cpp:20-33` is the wrong location for what is
    effectively the public token registry. Plugins building against
    Corbomite have no header to grep against — they have to read source.
    Should be exported as a header (e.g. `corbomite/core/
    PluginPermissions.h`) with `inline constexpr auto kVaultRead = ...`.
12. **`saveSessionState` / `loadSessionState` value type is `QJsonObject`,
    not `QVariantMap`.** Inconsistent with KDE conventions and with
    `KConfigGroup` which uses QVariant-keyed entries. Workable but
    surprising for a KDE developer; `QJsonObject` was probably picked
    for the Obsidian-shape `data.json` correspondence, but session
    state goes into `workspace.json`'s `_corbomite.plugins.<id>` key
    not into per-plugin data — the JSON-shape choice is at least
    consistent with that.

## API stability / versioning (X-Corbomite-MinVersion etc.)

Corbomite has the **correct shape** of API versioning — two axes:

1. **`X-Corbomite-MinVersion`** — semver of the host *application*. A
   plugin declares the minimum Corbomite app version it requires.
   Compared at discovery time against `PluginManager::appVersion()`
   (`libs/vault/src/PluginManager.cpp:86-93`, which reads
   `CORBOMITE_APP_VERSION` macro or falls back to `0.0.0`). Plugins
   targeting newer Corbomite features cleanly refuse to load on older
   hosts.
2. **`X-Corbomite-ApiLevel`** — integer ABI break marker. Defined at
   `libs/core/include/corbomite/core/PluginApi.h:15` as
   `CORBOMITE_PLUGIN_API_LEVEL = 1`. Plugins declaring a level ≤ host's
   constant load; higher refuses. Header docstring explicitly says
   "When we make a hard ABI break, bump this integer; compat shims for
   level N-1 stay in place for one major Corbomite version" — sensible
   N-1 commitment.

The two-axis approach handles the orthogonal cases ("I need Vault::trash
which arrived in 0.5" vs "the Plugin ABI changed shape entirely between
1 and 2"). Obsidian only has the former (`minAppVersion`); Corbomite is
strictly more capable.

**Today's reality:** Corbomite is pre-1.0. The CLAUDE.md memory entry
"Cluster N done … API shape-stable (pre-1.0)" indicates the team intent.
All in-tree metadata declares `X-Corbomite-MinVersion: "0.1.0"`; Bookmarks
declares `X-Corbomite-ApiLevel: 1` explicitly, others default. The defaults
mean every existing plugin slips through the gate — which is correct
for now but means the gate is untested in anger.

**Risks:**

- `CORBOMITE_APP_VERSION` is `#ifdef`'d (`PluginManager.cpp:88-92`); if
  the build doesn't define it, `appVersion()` returns `{0,0,0}` and **no**
  MinVersion check ever fails. Need to confirm CMakeLists provides this
  define for production builds (it likely does, but the silent-fallback-
  to-zero pattern is a foot-gun).
- The `apiLevel` is per-plugin-API not per-host-feature. There is no way
  for a plugin to say "I need the editor-suggester registry, not just
  the command registry". A growing `apiLevel` will conflate "we added
  features" with "we broke ABI". Consider adopting capability strings
  (Obsidian doesn't, but X11 extensions and Wayland protocols do) once
  plugin variety grows.
- The token list in `PluginContext.cpp` is the de-facto public surface
  but isn't versioned. Adding a token to the gate set silently breaks
  every previously-issued grant. Tokens should live in a versioned
  header.
- `KPluginFactory` itself is a stable KDE Frameworks API, so the
  loader half is reasonably future-proof. But the `Corbomite::Plugin`
  vtable (`Plugin.h:36-89`) is **part of the ABI** — adding a virtual,
  reordering virtuals, changing return types all break compiled
  third-party plugins. The current count is ~10 virtuals and any change
  needs an `apiLevel` bump.

**Verdict on stability story:** The mechanism is correct; the
discipline is on the team. With only built-in plugins shipping, the
team can iterate freely. The first third-party plugin's release will
require a header freeze + an explicit "apiLevel 1 ABI" document that
doesn't currently exist. The `X-Corbomite-Permissions` token registry
needs the same treatment.

---

# Summary

Corbomite has built a real, working plugin system on KPluginFactory + .so
modules, with 9 in-tree built-ins, end-to-end permission enforcement
(declare → grant → persist → proxy-gate), atomic data.json (improving
on Obsidian), and dual-axis API versioning. The `Component` lifecycle
port and `Plugin` multi-inheritance design are precise and Qt-idiomatic.
However only 6 of the ~13 Obsidian registration verbs have any plugin
exposure: `addCommand`, `registerView`, `registerExtensions`, `loadData`/
`saveData`, plus a KDE-shaped `configPage`. Six more registries exist
host-side but lack plugin proxies (`addRibbonIcon`, `registerHoverLink
Source`, `registerEditorSuggest`, `registerMarkdownPostProcessor`, plus
the absent `addStatusBarItem`, `registerObsidianProtocolHandler`,
`registerMarkdownCodeBlockProcessor`, and the Markoff equivalent of
`registerEditorExtension`). Lifecycle gaps include `onUserEnable`,
`onExternalSettingsChange` + `data.json` watcher, on-disable detach-
leaves-of-type cascade, and on-throw auto-unload. The internal-plugin
inventory matches Obsidian on the high-traffic 9 (file-explorer, search,
graph etc.) but misses 8 user-expected ones (command-palette, switcher,
templates, daily-notes, page-preview, word-count, file-recovery, note-
composer). Permission tokens live in a .cpp instead of a header, which
will bite on first third-party plugin release. No plugin browser/installer
UI exists.
