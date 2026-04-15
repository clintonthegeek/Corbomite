# `obsidian/plugin` — plugin base class (extension surfaces only)

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/plugin/`
**File count:** 1
**Files:** `Plugin.js`
**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> **Files:** 1 (`Plugin.js`). **Key exports / primitives:** see `02-extension-surfaces.md` — `Plugin.js` is the canonical list. **On-disk contracts:** reads/writes `.obsidian/plugins/<plugin-id>/data.json` (via `loadData`/`saveData`) and the manifest at `.obsidian/plugins/<plugin-id>/manifest.json` (read by the loader, not `Plugin`). **Cross-domain dependencies:** delegates to `viewRegistry`, `commands`, `workspace`, `setting`, `app.workspace.registerHoverLinkSource`, etc. Effectively a thin facade over the `App` registries. **Corbomite-relevance note:** **MISSING**. Lightly in scope. When a Corbomite plugin system is designed, mirror these registration verbs (with KDE-native primitives): `addCommand` ↔ `KActionCollection::addAction`, `addRibbonIcon` ↔ toolbar action, `addStatusBarItem` ↔ `QStatusBar::addPermanentWidget`. **Pass 2 focus:** complete enumeration of `register*` and `add*` methods; see seeded list in `02-extension-surfaces.md` and confirm completeness.

**De-minifier artifact note:** The file `plugin/Plugin.js` and the sibling `settings/PluginSettingTab.js` are byte-for-byte identical except for their `// public API symbol:` comment (`Plugin` vs `PluginSettingTab`). Both are extractions of the same source window (`app.js lines 167963-168340`) which contains **three** exported classes in sequence: `Plugin` (lines 34-354 in the file), `PluginSettingTab` (lines 355-366), and an internal-plugin `Y0` variant of the settings tab (lines 367-379). The `plugin` domain owns the `Plugin` class; the `settings` domain owns `PluginSettingTab`. Trailing regex `Q0` and sourcemap-stripper constant `X0` on lines 380-382 are from adjacent minified code and belong to the plugin-loader (outside this domain) — I ignore them here. No other near-duplicates in `obsidian/plugin/`; canonical file is `plugin/Plugin.js`.

---

## 1. Public API surface

Only one class is in scope: `Plugin`. Three helper classes (`PluginSettingTab`, the internal `Y0`, and the legacy regex `Q0`) live in the same extraction window but belong to neighbouring domains.

### `Plugin`

- **Kind:** class.
- **Exported as:** `Plugin` (via `// public API symbol: Plugin`).
- **Extends:** `Component` (`ui/components/Component.js`, so inherits `load`, `unload`, `addChild`, `removeChild`, `register`, `registerEvent`, `registerDomEvent`, `registerScopeEvent`, `registerInterval`, `onload`/`onunload` hooks).
- **Constructor:** `new Plugin(app, manifest)` (`plugin/Plugin.js:35-44`). Callers: the plugin loader (outside this domain) invokes `new PluginClass(app, manifest)` exactly once per vault load. Subclass constructors that forget to call `super(app, manifest)` crash.
- **Fields populated at construction:** `this.app` (shared `App` singleton), `this.manifest` (see §2), `this._lastDataModifiedTime = 0` (used by the external-edit watcher), `this._userDisabled = !1` (flipped true when the user toggles off in settings), `this.onConfigFileChange = debounce(this._onConfigFileChange, 50)` (bound, debounced external-watch handler).
- **Purpose:** thin facade over `App`-level registries (`app.commands`, `app.workspace`, `app.viewRegistry`, `app.setting`, `app.statusBar`, `app.cli`, `app.internalPlugins`, `app.vault`). Every `register*`/`add*` verb delegates to an underlying registry and **ties the unregister call to `this.register(...)` so cleanup happens deterministically in `Component.unload`**.
- **Lifecycle (as class):** constructed by the plugin loader; owned by `app.plugins` (community) or `app.internalPlugins` (built-ins); destroyed by `Component.unload` which pops every registered cleanup (LIFO) then calls `onunload()`. See §8 for ordering invariants.
- **Mixes in:** `Component`. Every `Events` subscription should be routed through `this.registerEvent(...)` and every DOM listener through `this.registerDomEvent(...)` so they unwind on unload. Note that `Plugin` itself does **not** mix `Events`; plugins that want to emit events instantiate `new Events()` themselves.

Methods grouped by role. Per TEMPLATE discipline for large classes the enumeration per-method is deferred to §10 (where every `register*`/`add*` has its own row); the role-paragraphs below follow.

#### Lifecycle role

- **`load()`** (`Plugin.js:48-68`) — idempotent guard via `this._loaded`; awaits `this.onload()`, then loads every `_children[]` entry. Children mutated during `onload` are included (slice taken after `await`).
- **`onload()` / `onunload()`** (`Plugin.js:69`, inherited from `Component`) — plugin-supplied; `onload` is where every `register*`/`add*`/`addChild` call happens. `onunload` runs **after** every registered cleanup; use it for cross-cutting cleanup the registration-verb pattern does not cover (e.g. closing a `TcpServer`).
- **`onUserEnable()`** (`Plugin.js:303`, empty default) — called by the loader the first time the user toggles the plugin on **in this session** (after initial load). Distinct from `onload`. Consumers grep: `MarkdownView.js:180` overrides a sibling `onUserEnable` but does not invoke the plugin one. Use for one-shot UX (e.g. "welcome" notices).
- **`onExternalSettingsChange()`** (referenced but not defined on the base class; `Plugin.js:244, 337, 344`) — plugin-supplied optional hook. Fires when `.obsidian/plugins/<id>/data.json` mtime advances past `this._lastDataModifiedTime` (see §3). Typical pattern: plugin calls `await this.loadData()` inside and re-applies settings.
- **`_onConfigFileChange()`** (`Plugin.js:331-350`, private, debounced 50 ms as `onConfigFileChange`) — wired by the loader to the `Vault` raw-watch of `.obsidian/plugins/<id>/data.json`; compares fresh `stat.mtime` vs `_lastDataModifiedTime` and calls `onExternalSettingsChange` when strictly greater.

#### Registration role

See §10 for the exhaustive table. Every method in this role follows the same template: (a) delegate to the underlying registry's `add*`/`register*`, (b) push the matching `unregister*` into `Component._events` via `this.register(...)`, (c) return the handle the registry returned (where applicable).

#### Data persistence role

- **`loadData()`** (`Plugin.js:233-253`) — returns `await app.vault.readPluginData(manifest.dir)`. If a non-null result **and** the subclass defines `onExternalSettingsChange`, also stamps `_lastDataModifiedTime = await getModifiedTime()` so the watcher doesn't immediately re-fire.
- **`saveData(data)`** (`Plugin.js:255-271`) — sets `_lastDataModifiedTime = Date.now()` *before* the write so the watcher ignores the self-caused `modify` event, then returns `app.vault.writePluginData(manifest.dir, data, {mtime: <that same Date.now()>})`.
- **`getModifiedTime()`** (`Plugin.js:304-329`) — `await adapter.stat(normalizePath(manifest.dir + '/data.json')).mtime`; returns `0` on any throw (ENOENT / permission).
- **`loadCSS()`** (`Plugin.js:272-302`) — reads `<manifest.dir>/<JD>` where `JD` is a constant defined outside this domain (the conventional `styles.css` filename), inserts a `<style type="text/css">` node into `document.head` **before** `app.customCss.styleEl` (so vault CSS wins), and registers an unload hook to detach the node. Never auto-invoked — subclasses opt in from `onload`.

---

## 2. Data structures

### `PluginManifest` (inferred — `this.manifest`)

```typescript
{
  id: string;                 // unique plugin id; used as command prefix and ribbon id
  name: string;               // human-readable plugin name; prefixes command display + CLI handler help
  version: string;            // semver string (used by reload-plugins UI; see ViewRegistry.js:386-387)
  minAppVersion?: string;     // required minimum Obsidian version (validated by the loader)
  description?: string;       // long-form description shown in settings
  author?: string;            // author name
  authorUrl?: string;         // URL to author site
  fundingUrl?: string | Record<string, string>;  // donation links
  isDesktopOnly?: boolean;    // if true, loader skips plugin on mobile
  dir: string;                // absolute-ish vault path .obsidian/plugins/<id>/ — injected by loader,
                              // consumed by loadData/saveData/getModifiedTime/loadCSS
}
```

- **Origin:** the plugin loader (outside this domain) reads `.obsidian/plugins/<id>/manifest.json` from disk and injects `dir` before constructing the `Plugin`. Nothing in `plugin/Plugin.js` reads the file directly.
- **Invariant:** `manifest.id` is used as the `"<id>:<cmdId>"` namespace for every `addCommand` the plugin registers (`Plugin.js:99`), and as the ribbon-button key (`Plugin.js:72`), and in the status-bar item's auto-added CSS class `plugin-<id>` (`Plugin.js:85-88`, with `[^_a-zA-Z0-9-]` replaced by `-` and lowercased).
- **Status-bar CSS class quirk:** the regex is applied without the `g` flag — only the **first** illegal character gets rewritten. A plugin-id containing two non-alnum characters (`a$b$c`) yields `plugin-a-b$c`, not `plugin-a-b-c`. A bug in Obsidian, but one Corbomite should replicate for compat. (`Plugin.js:87-88`.)

### `CommandSpec` (argument to `addCommand`)

```typescript
{
  id: string;                              // plugin-local id, gets namespaced to "<manifest.id>:<id>"
  name: string;                            // plugin-local name, gets prefixed "<manifest.name>: <name>"
  icon?: string;                           // lucide icon id
  callback?: () => void;                   // simple invoke
  checkCallback?: (checking: boolean) => boolean;       // palette availability + invoke
  editorCallback?: (editor, view) => void;              // requires active MarkdownView
  editorCheckCallback?: (checking, editor, view) => boolean;
  hotkeys?: Array<{modifiers: string[]; key: string}>;  // default hotkeys (user-overridable)
  mobileOnly?: boolean;
  repeatable?: boolean;
}
```

Mutated in-place: `Plugin.addCommand` rewrites `cmd.id` and `cmd.name` before delegating (`Plugin.js:99-100`) — plugin authors that stash a reference before `addCommand` see the namespaced form afterward.

### `HoverLinkSource` (argument to `registerHoverLinkSource`)

```typescript
{
  display: string;        // human-readable source name, shown in Page-Preview settings
  defaultMod: boolean;    // if true, requires Mod-key while hovering to trigger the preview
}
```

Matches `workspace.md §7` shape exactly. Stored raw into `workspace.hoverLinkSources[id]`.

### `EditorSuggest<T>` (argument to `registerEditorSuggest`)

Instance subclass of `EditorSuggest`, inheriting `Component`. Required overrides: `onTrigger`, `getSuggestions`, `renderSuggestion`, `selectSuggestion`. Lifecycle governed by `EditorSuggestManager` (`views/ViewRegistry.js:238-281`); see `domains/editor.md §1 EditorSuggest`.

### `CliHandlerSpec` (implicit in `registerCliHandler`)

```typescript
registerCliHandler(flag: string, helpText: string, handler: Function, extra: unknown): void
```

Signature inferred from `Plugin.js:221-232`; `help` is wrapped as `"[<manifest.name>]: " + helpText` before delegation to `app.cli.registerHandler`. The `app.cli` registry itself is outside the audit tree (desktop-only, not extracted). Flag for Pass 3: the fourth argument's semantics are unclear from the call site alone.

---

## 3. On-disk contracts

### `.obsidian/plugins/<plugin-id>/data.json`

- **Path:** `<manifest.dir>/data.json` where `manifest.dir` is injected by the loader as `.obsidian/plugins/<id>`.
- **Written by:** `Plugin.saveData(data)` → `vault.writePluginData(manifest.dir, data, {mtime: Date.now()})` (`Plugin.js:255-271`) → `Vault.writePluginData` (`vault/Vault.js:296-299`) → `Vault.writeJson` (`vault/Vault.js:338-362`) → `adapter.write(path, JSON.stringify(data, undefined, 2), opts)`.
- **Read by:** `Plugin.loadData()` → `vault.readPluginData(manifest.dir)` (`vault/Vault.js:292-295`) → `Vault.readJson` → `JSON.parse(adapter.read(path))`. Returns `null` if ENOENT.
- **Schema:** plugin-defined JSON-serialisable object. No framework-enforced fields. Plugins typically version via a `version` or `schemaVersion` field and migrate in their own `loadData` override.
- **Format invariants:** `JSON.stringify(obj, undefined, 2)` — 2-space indent, no trailing newline, keys in `Object.keys` insertion order (unstable across engines; tolerate any order on read). No `version` field is emitted by the framework.
- **Lifecycle:** created on first `saveData`; absent before that. Deletion is not a framework operation — plugin authors call `adapter.remove` manually.
- **Atomic-write behaviour:** Obsidian's `adapter.write` is **not atomic** in the POSIX sense; it opens, writes, and closes. A crash mid-write can leave `data.json` truncated. `readJson` swallows the resulting `SyntaxError` to `console.error` and returns `undefined` (`Vault.js:329-333`), which trickles up as falsy — plugins treat it as "no data yet" and fall back to defaults. Corbomite's equivalent should use rename-based atomic writes (`QSaveFile`) to avoid the silent-corruption pattern.
- **External-edit watcher:** The plugin loader wires a `Vault.on('raw', ...)` subscription gated on the manifest dir that invokes the plugin's debounced `onConfigFileChange` (50 ms trailing). That handler calls `getModifiedTime()` and, if strictly greater than `_lastDataModifiedTime`, invokes `onExternalSettingsChange()` if defined (`Plugin.js:331-350`). This is how third-party tools (git pull, Obsidian Sync, user editing the file) reload plugin settings without a manual toggle. Corbomite must mirror: per-plugin inotify/QFileSystemWatcher on `data.json`, debounced 50 ms, compare against last-written mtime.
- **Self-edit suppression:** `saveData` sets `_lastDataModifiedTime = Date.now()` **before** the write, and passes the same timestamp as `mtime` to the adapter. The watcher fires on every `modify` event but sees `freshMtime === _lastDataModifiedTime` and no-ops. This is the only reason `saveData` avoids echo-reload loops.

### `.obsidian/plugins/<plugin-id>/styles.css`

- **Path:** `<manifest.dir>/<JD>` where `JD` is a string constant defined outside this domain (conventional filename `styles.css`).
- **Written by:** not framework-owned; plugin authors ship it in their release zip.
- **Read by:** `Plugin.loadCSS()` (`Plugin.js:272-302`) on **explicit** opt-in. Not called automatically. If the file exists, its text content is inserted into a `<style>` element placed immediately before `app.customCss.styleEl` in `document.head`, with an unload hook that detaches the node.
- **Lifecycle:** read once per `loadCSS()` call. No hot-reload; if the plugin updates its stylesheet on disk, the user must disable-and-re-enable the plugin.
- **Corbomite parity note:** Qt is not DOM-CSS; a literal port isn't possible. A nearer equivalent is loading a per-plugin QSS fragment on load and concatenating into the KConfig theme pipeline.

### `.obsidian/plugins/<plugin-id>/manifest.json`

- **Path:** `<manifest.dir>/manifest.json`.
- **Written by:** plugin author ships it; never written by the framework.
- **Read by:** the plugin loader (outside this domain). `Plugin.js` itself never opens the file; the loader parses it, constructs `manifest`, injects `dir`, and hands the object to `new Plugin(app, manifest)`.
- **Schema:** see §2 `PluginManifest`. Minimum required: `id`, `name`, `version`, `minAppVersion`, `description`, `author`. Optional: `authorUrl`, `fundingUrl`, `isDesktopOnly`.

### `.obsidian/community-plugins.json`

- **Path:** root-level `.obsidian/community-plugins.json`.
- **Written by / read by:** `app.plugins` loader (outside this domain — inferred from `core/App.js:554` `this.plugins = new $0(this)` and `utils/apiVersion.js:1434`-ish `disablePluginAndSave(id)` / `enablePluginAndSave(id)` calls).
- **Schema (inferred):** `string[]` — JSON array of enabled plugin IDs. Order is insertion order = user's explicit enable order. Absence = no community plugins enabled.
- **Gaps for Pass 3:** the exact on-disk format (array vs object, whether disabled-but-installed plugins appear) is not visible in `plugin/Plugin.js`. Pass 3 must audit the plugin-loader source (not in the current extract) to confirm.

### `.obsidian/core-plugins.json` and `.obsidian/core-plugins-migration.json`

Mentioned in `vault.md §3` but owned by `settings`/`internal-plugins` domain — out of scope here. The `Plugin` class itself never reads or writes them. Cross-ref only.

---

## 4. Events emitted

`Plugin` does not emit any direct events (it extends `Component`, not `Events`). The only trigger call sites in the domain are indirect: `registerMarkdownPostProcessor` and `registerMarkdownCodeBlockProcessor` fire `workspace.trigger("post-processor-change")` on **both** register and unregister (`Plugin.js:144,147,162,166`).

| Event name | Payload | Triggered when | Typical consumers |
|---|---|---|---|
| `post-processor-change` | `()` | A plugin (un)registers a markdown post-processor or code-block processor via the `Plugin.*` wrapper | Every live `MarkdownPreviewView` rerenders all its sections so plugin-supplied processors take effect. See `domains/editor-markdown.md §4`. |

Note the wrapper emits **one** event per register and **one** per unregister; a plugin that adds five post-processors in `onload` triggers five re-renders, debounced downstream in `MarkdownPreviewView`.

---

## 5. Events consumed

`Plugin.js` subscribes to no `Events` emitters directly. The external-edit watcher is wired by the plugin-loader (outside this domain) to call `this.onConfigFileChange` in response to `Vault.on('raw', ...)` events scoped to the plugin's `manifest.dir`. Every other event-subscription happens inside subclasses via inherited `this.registerEvent(...)` (which unwinds on unload).

| Listener file | Subscribes to | Why |
|---|---|---|
| `plugin/Plugin.js:40` (indirectly) | `Vault.on('raw', cb)` for `.obsidian/plugins/<id>/data.json` | Trigger `onExternalSettingsChange` when third-party tools edit the file. Subscription is set up in the loader, not in `Plugin.js`. |

---

## 6. Commands registered

`No commands registered here.` The `Plugin` class is the mechanism by which plugins register commands, but `Plugin.js` itself defines none. Internal core commands (`editor:toggle-source`, `app:open-settings`, etc.) are registered in their respective domains (editor-markdown, core, workspace).

---

## 7. Registries owned

`Plugin` owns no registries of its own. Every `register*` delegates to an **underlying registry** owned by another domain. See §10 for the mapping. The closest thing to a local registry is `Component._events` (inherited) — a LIFO stack of cleanup callbacks that `unload()` drains in reverse-insertion order. This is the backbone of the auto-unregister semantics.

---

## 8. Invariants

- **Cleanup is LIFO.** `Component.unload` pops `_events` and `_children` from the end, so the **last** cleanup registered runs **first**. Plugins relying on ordering (rare — most cleanups are independent) must register in reverse-dependency order.
- **Every `register*`/`add*` calls `this.register(cleanupFn)` exactly once per registration.** `Plugin.js:70-232` — grep shows no exceptions. Plugins never need to manually call `unregister*` from `onunload` — `Component.unload` does it.
- **`addCommand` mutates its argument.** `cmd.id` and `cmd.name` are rewritten in place (`Plugin.js:99-100`). A second `addCommand` call with the same object re-mutates (`"<id>:<id>:<id>"`), typically producing a duplicate-id error from `app.commands.addCommand`.
- **`load()` is idempotent.** Second call returns early via `this._loaded` guard (`Plugin.js:54-56`). Third-party code that calls `plugin.load()` a second time gets a no-op.
- **`onload` runs before children load.** The children-loading loop runs after `await this.onload()` resolves. A child added in `onload` is loaded as part of that same loop.
- **Throws from `onload` reject `load()`'s promise but do not unwind registered cleanups.** Any `register*`/`addChild` calls made before the throw have already pushed cleanups into `_events`. The plugin sits in a half-loaded state (`_loaded = true`) until someone calls `unload()`. Flag for Corbomite: wrap `onload` in a try/catch that auto-unloads on throw. See OPEN QUESTIONS.
- **`saveData` always wins against concurrent external writes.** Because `_lastDataModifiedTime` is updated **before** the write completes, any external modify that occurred between the plugin's last read and this save is silently overwritten. Plugin authors who want merge semantics must read-check-write themselves. Corbomite's atomic-write path should preserve this behaviour for compat (i.e. last-writer-wins, no three-way-merge in the framework).
- **`manifest.id`-prefix on command IDs is mandatory.** `addCommand` unconditionally prepends `manifest.id + ":"`. A plugin that accidentally supplies an already-prefixed id ends up with `"<id>:<id>:cmdId"`.
- **Ribbon id is `manifest.id + ":" + title` (not `+ id`).** (`Plugin.js:72`.) Two `addRibbonIcon` calls with the same `title` clash on the underlying `leftRibbon.items` map. Use distinct titles or call `removeRibbonAction` first.
- **`registerCodeMirror(cb)` is a no-op (`Plugin.js:196`).** Legacy stub for CodeMirror-5-era plugins. Pass a function; nothing happens. Corbomite need not port.
- **`registerBasesView(type, factory)` returns `boolean`.** `false` if the Bases internal plugin is not enabled; `true` after successful registration. Only verb on `Plugin` that returns a bool. (`Plugin.js:171-181`.)

---

## 9. Observable user features

The `Plugin` class itself has no direct user-visible surface — it's the scaffolding third-party code uses. User-observable behaviours it enables:

- Commands added by a plugin appear in the command palette and are hotkey-bindable.
- Ribbon buttons added by a plugin appear in the left activity bar and can be hidden via the ribbon context menu (workspace domain).
- Status-bar items added by a plugin appear in the bottom status bar; styled via auto-added `plugin-<sanitised-id>` class for theming.
- Settings tabs added by a plugin appear in the Settings modal's plugin section.
- Markdown post-processors and code-block processors added by a plugin affect every `MarkdownPreviewView` rerender.
- Hover-link previews from plugin views fire when the registered source matches the hovered link.
- `obsidian://<action>?...` URLs are dispatched to the plugin's protocol handler; OAuth callbacks and deep-links work.
- Plugin settings persist across sessions in `.obsidian/plugins/<id>/data.json`; external edits (git pull, Sync, manual) reload live via `onExternalSettingsChange`.
- Disabling a plugin in Settings (`app.plugins.disablePluginAndSave(id)`) causes `_userDisabled = true`, triggers the plugin's `Component.unload` (which pops every `register*`'s cleanup), and additionally **detaches every leaf of every view-type the plugin registered** (`Plugin.js:122-123`) — open tabs for those view-types close on disable, not just at the next restart.

---

## 10. Extension surfaces exposed

**Every verb on `Plugin` is an aggregator.** The first column names the verb; the second the consumer-side call site; the third what the plugin supplies; the fourth cross-ref to a completed Pass-2 doc.

| Surface | Registration verb (`Plugin.js:<line>`) | Consumer call site | What plugins supply |
|---|---|---|---|
| Ribbon button (left activity bar) | `addRibbonIcon(icon, title, cb)` (`Plugin.js:70-79`) → `leftRibbon.addRibbonItemButton(id, icon, title, cb)` | `workspace/Workspace.js:143` `WorkspaceRibbon.addRibbonItemButton`; drawn by `WorkspaceFloating.js:51`. See `domains/workspace.md §7`. | `(event: MouseEvent) => void` callback; lucide icon id; friendly title |
| Status-bar item | `addStatusBarItem()` (`Plugin.js:81-94`) → `app.statusBar.registerStatusBarItem()` | `core/App.js:548` `this.statusBar = new Nee(...)`. Element auto-classed `plugin-<sanitised-id>`. | Caller mutates the returned `HTMLElement` directly |
| Palette command | `addCommand(cmd)` (`Plugin.js:96-106`) → `app.commands.addCommand(cmd)` | `core/App.js:32` `this.commands = new Y6(this)`. See `domains/workspace.md §6` (commands table). | `CommandSpec` (see §2); id/name auto-namespaced |
| Palette command removal | `removeCommand(id)` (`Plugin.js:108-110`) → `app.commands.removeCommand(<namespaced-id>)` | Same registry as above. | Plugin-local id (without `<manifest.id>:` prefix) |
| Settings tab | `addSettingTab(tab)` (`Plugin.js:111-116`) → `app.setting.addSettingTab(tab)` | `settings/PluginSettingTab.js` (same de-min source). See `domains/workspace.md §7` cross-ref and the future `settings` Pass-2 doc. | `PluginSettingTab` subclass with `display()` override |
| View type | `registerView(type, factory)` (`Plugin.js:118-124`) → `viewRegistry.registerView(type, factory)` | `views/ViewRegistry.js:47` (throws on dup); leaf instantiation at `workspace/WorkspaceLeaf.js:1094`. See `domains/views.md §10, §7`. | `(leaf: WorkspaceLeaf) => View` factory. On unload, if `_userDisabled`, also `detachLeavesOfType(type)` |
| Hover-link source | `registerHoverLinkSource(id, info)` (`Plugin.js:126-131`) → `workspace.registerHoverLinkSource(id, info)` | `workspace/Workspace.js:3850-3854` (Map write); consumed by Page-Preview internal plugin. See `domains/workspace.md §7`. | `{display: string, defaultMod: boolean}` |
| File-extension binding | `registerExtensions(exts, type)` (`Plugin.js:133-139`) → `viewRegistry.registerExtensions(exts, type)` | `views/ViewRegistry.js:73` (atomic across array); `MetadataCache.js:994` re-evaluates indexability on change. See `domains/views.md §10`. | `string[]` extensions + pre-registered viewType |
| Markdown post-processor | `registerMarkdownPostProcessor(fn, sortOrder)` (`Plugin.js:140-151`) → `MarkdownPreviewRenderer.registerPostProcessor(fn, sortOrder)` | `editor/markdown/MarkdownPreviewRenderer.js:1167-1174`; walked by `MarkdownRenderer.postProcess` (`editor/markdown/MarkdownRenderer.js:160, 234`). See `domains/editor-markdown.md §10, §2`. Fires `post-processor-change` on workspace. | `(el: HTMLElement, ctx: MarkdownPostProcessorContext) => void \| Promise<void>` plus optional `sortOrder` number |
| Markdown code-block processor | `registerMarkdownCodeBlockProcessor(lang, fn, sortOrder)` (`Plugin.js:152-170`) → `createCodeBlockPostProcessor(lang, fn)` + `registerPostProcessor(wrapper, sortOrder)` + `registerCodeBlockPostProcessor(lang, fn)` | `editor/markdown/MarkdownPreviewRenderer.js:1184-1197` (throws on duplicate `lang`); `:1198-1233` wraps with `replaceCode(newSrc)` helper and `div.block-language-<lang>` DOM shell. See `domains/editor-markdown.md §10`. | `(source: string, el: HTMLElement, ctx) => void \| Promise<void>` |
| Bases view-type | `registerBasesView(type, factory)` (`Plugin.js:171-181`) → `app.internalPlugins.getEnabledPluginById('bases').registerView(type, factory)` | `bases/BasesView.js:228,1930,2231,2254` via `plugin.getRegistration(type)`. See `domains/bases.md §10`. | `{name(): string, icon: string, options(viewConfig): OptionDescriptor[], create(controller): BasesView-layout}`. Returns `false` if Bases disabled |
| Bases formula global function | `registerGlobalFunc(fn)` (`Plugin.js:182-188`) → `QW.addGlobal(fn)` | `QW` defined outside audit tree (Bases plugin bundle). Used by Bases formula parser. See OPEN QUESTIONS. | Named function; `fn.name` is the registration key |
| Bases formula instance function | `registerInstanceFunc(type, fn)` (`Plugin.js:189-195`) → `QW.addForType(type, fn)` | Same `QW` registry; methods available as `value.fnName()` in Bases formulas. | `type` is a `Value`-subclass tag; `fn` is named |
| (Legacy) CodeMirror 5 | `registerCodeMirror(cb)` (`Plugin.js:196`) | **No-op.** CM5 shim; retained for backward compatibility with pre-CM6 plugins. | Ignored |
| CodeMirror 6 editor extension | `registerEditorExtension(ext)` (`Plugin.js:197-203`) → `workspace.registerEditorExtension(ext)` | `workspace/Workspace.js:3889-3890`; triggers `updateOptions()` at `:3441-3448` which reconfigures every `MarkdownView`'s CM `Compartment`. See `domains/workspace.md §7, §10` and `domains/editor.md §10`. | CM6 `Extension` value (`ViewPlugin.fromClass`, `StateField.define`, `Decoration.mark`, etc.) |
| `obsidian://` URL handler | `registerObsidianProtocolHandler(action, handler)` (`Plugin.js:204-213`) → `workspace.registerObsidianProtocolHandler(action, handler)` | `workspace/Workspace.js:3839-3844` (throws on duplicate); dispatched by `window.OBS_ACT`. 11 built-ins: `open`, `new`, `search`, `show-plugin`, `show-theme`, `show-release-notes`, `debug-info`, `publish-sites`, `sync-setup`, `vault-setup`, `hook-get-address`. See `domains/workspace.md §7, §10`. | `(args: {action: string, [k: string]: string}) => void` |
| Editor suggest (inline autocomplete) | `registerEditorSuggest(suggest)` (`Plugin.js:214-220`) → `workspace.editorSuggest.addSuggest(suggest)` | `views/ViewRegistry.js:247` (`EditorSuggestManager.addSuggest`); iterated by `EditorSuggestManager.trigger` at `views/ViewRegistry.js:254`. See `domains/editor.md §10`. | `EditorSuggest<T>` subclass instance |
| CLI handler (desktop-only) | `registerCliHandler(flag, help, handler, extra)` (`Plugin.js:221-232`) → `app.cli.registerHandler(flag, "[<name>]: <help>", handler, extra)` | `app.cli` — **defined outside the audit tree**. Almost certainly electron `commandLine` hooks. | `flag: string`, `help: string`, `handler: Function`, `extra` (purpose unknown — see OPEN QUESTIONS) |

**Inherited from `Component` — not defined in this file, but completing the plugin-surface list for Corbomite API mapping:**

| Surface | Inherited verb | Consumer | What plugins supply |
|---|---|---|---|
| DOM event with auto-removeListener | `registerDomEvent(el, type, cb, opts?)` (`ui/components/Component.js:53-58`) | Adds listener via `addEventListener`; unload pops via `removeEventListener` | Standard DOM event callback |
| `Events` subscription with auto-offref | `registerEvent(eventRef)` (`ui/components/Component.js:48-52`) | `EventRef.e.offref(ref)` on unload | An `EventRef` returned by any `events.on(...)` |
| `setInterval` with auto-clear | `registerInterval(handle)` (`ui/components/Component.js:64-71`) | `clearInterval(handle)` on unload | Timer handle |
| Arbitrary cleanup fn | `register(fn)` (`ui/components/Component.js:45-47`) | `fn()` called on unload, LIFO | Any zero-arg function |
| Keymap scope event | `registerScopeEvent(kmRef)` (`ui/components/Component.js:59-63`) | `kmRef.scope.unregister(kmRef)` on unload | A `KeymapEventHandler` returned by `scope.register` |
| Child component | `addChild(component)` (`ui/components/Component.js:37-39`) | Child's `load()` during parent load; `unload()` during parent unload | `Component` subclass instance |

**Cross-ref:** the `Component` base class is audited as a sibling in Wave 3 (`ui/components/`). Full audit of the 16 component classes (`BaseComponent`, `AbstractTextComponent`, `TextComponent`, `TextAreaComponent`, `ButtonComponent`, `ExtraButtonComponent`, `ColorComponent`, `DropdownComponent`, `MomentFormatComponent`, `ProgressBarComponent`, `SearchComponent`, `SecretComponent`, `SliderComponent`, `ToggleComponent`, `ValueComponent`, `Component`) is not in this doc — see `domains/components.md` (pending).

---

## 11. Corbomite mapping

Corbomite has **no plugin API today**. Every row in this table is Missing. The Notes column gives the Qt/KDE idiomatic translation.

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `Plugin` base class | — | Missing | `CorbomitePlugin : public QObject` with `onLoad()`/`onUnload()` virtuals; owned by a `PluginManager` singleton under `libs/pluginhost/`. |
| `Component` scoped cleanup | — | Missing | `QObject` parent/child already handles widget lifetime; for non-widget cleanups (signals, timers) adopt a `ScopedConnectionBag` (`QList<QMetaObject::Connection>`) helper. |
| `PluginManifest` schema | — | Missing | Parse `.obsidian/plugins/<id>/manifest.json` into a `PluginManifest` struct in `libs/pluginhost/`; preserve the exact field names for Obsidian compat. |
| `addRibbonIcon` | — | Missing | `KToolBar` action on the left toolbar via `KActionCollection::addAction`; icon from `QIcon::fromTheme`. |
| `addStatusBarItem` | — | Missing | `QStatusBar::addPermanentWidget(new QWidget)`; auto-style via ObjectName `"plugin-<sanitised-id>"` + QSS selector. |
| `addCommand` | — | Missing | `KActionCollection::addAction(id, action)`; namespace id as `"<plugin-id>:<cmd-id>"` like Obsidian for hotkey-config compat. Callback variants (`checkCallback`, `editorCallback`) map to `QAction::isEnabled()` + `QAction::triggered()` signal. |
| `removeCommand` | — | Missing | `KActionCollection::removeAction(action)`. |
| `addSettingTab` | — | Missing | `KPageDialog::addPage`. Currently ad-hoc in `SettingsDialog.cpp`; promote to an extension surface. |
| `registerView` + `registerExtensions` | — | Missing | The single highest-leverage surface. A `Corbomite::ViewRegistry` emitting `viewRegistered`/`viewUnregistered`/`extensionsUpdated` Qt signals; maps `QString viewType` → factory `std::function<View*(WorkspaceLeaf*)>`. See `domains/views.md §11`. |
| `registerHoverLinkSource` | — | Missing | `Workspace::registerHoverLinkSource(QString id, HoverLinkSource)` + `HoverPreview` widget. |
| `registerMarkdownPostProcessor` | — | Missing | `MarkoffRenderEngine::registerPostProcessor(Callback cb, int sortOrder)` — walks the rendered `QGraphicsScene`/`QTextDocument` after initial render. Stable sort by integer priority. |
| `registerMarkdownCodeBlockProcessor` | — | Missing | Per-language map in `MarkoffRenderEngine`; fired on `code.language-<lang>` blocks. Needs `ReplaceCode` back-channel (currently impossible without block-source-position tracking). |
| `registerEditorExtension` (CM6) | — | N/A for direct port | Markoff is not CodeMirror. Offer a different extension model: `Markoff::EditorPlugin` with virtual `onKeyPress`, `onSelectionChange`, `customPainter` — not a 1:1 port. |
| `registerEditorSuggest` | `libs/markoff` completion popup (partial) | Partial | `CompletionPopup` handles `[[` and `#`. Promote to `Markoff::SuggestRegistry` with insertion-order iteration (matches Obsidian's first-non-null-wins semantic — see `editor.md §12`). |
| `registerObsidianProtocolHandler` | — | Missing | `KDBusService(Unique)` single-instance + custom URL scheme `obsidian://` (and `corbomite://`) handler. Map built-in actions 1:1. |
| `registerBasesView` | — | Missing (pre-Bases) | Bases itself is not yet implemented in Corbomite; this surface blocked on that. |
| `registerGlobalFunc` / `registerInstanceFunc` | — | Missing | Bases formula plugin-extension point. Blocked on a `libs/formula/` parser that accepts plugin-registered functions. |
| `registerCliHandler` | `main.cpp` `QCommandLineParser` (partial) | Missing | CLI is currently baked in; expose via `PluginManager::registerCliOption(name, helpText, handler)`. |
| `loadData` / `saveData` | — | Missing | Per-plugin JSON at `<vaultRoot>/.obsidian/plugins/<id>/data.json`. `QSaveFile` for atomic writes (fixing Obsidian's truncation-on-crash gap). `QFileSystemWatcher` for external-edit detection; debounced 50 ms; compare mtime against last-written. |
| `loadCSS` | — | Missing (not a direct port) | Qt uses QSS not CSS. Offer `loadQss()` reading `<plugin-dir>/styles.qss` and concatenating into the app's QSS sheet with unload hook. |
| `onExternalSettingsChange` | — | Missing | Wired into the `QFileSystemWatcher` handler; invoke on mtime-advance only. |
| `onUserEnable` | — | Missing | Fire once per session after first user-toggle-on; used for welcome notices. |
| Per-plugin id-prefixed commands | — | Missing | Prefix pattern `"<plugin-id>:<cmd-id>"` matches Obsidian's hotkey config file format; Corbomite must preserve for `.obsidian/hotkeys.json` compat. |
| `post-processor-change` event | — | Missing | Signal on `MarkoffRenderEngine` — every open reading view rerenders. |
| `_userDisabled` → `detachLeavesOfType` on disable | — | Missing | When a plugin is disabled (not just reloaded), every open tab of its view-types must close; mirror in `Workspace::detachLeavesOfType`. |

---

## 12. Markoff gap confirmations / discoveries

Most editor/rendering extension surfaces land in `editor` / `editor-markdown` / `rendering`. The `plugin` domain mostly *funnels* into those. Confirmations worth recording from the funnel side:

- **Confirmed gap** (from `01-markoff-gaps.md` "Editor extensions registry"): `registerEditorExtension` is a **1:1 wrapper** over `workspace.registerEditorExtension`, which appends to a **flat shared list** applied to every `MarkdownView` via `updateOptions()` → CM `Compartment` reconfigure. Markoff must use the same "one list, applied to every `Markoff::Editor` instance" semantics so plugins don't accidentally scope to one leaf. (`Plugin.js:197-203`; `workspace/Workspace.js:3889-3894`.)
- **Confirmed gap** (from `01-markoff-gaps.md` "Editor suggesters"): `registerEditorSuggest` delegates to `EditorSuggestManager.addSuggest`; **insertion-order iteration, first-non-null-`onTrigger` wins** (see `editor.md §12`). Markoff's current `CompletionPopup` handles `[[`/`#` by hard-coded logic; promote to a registry with the same iteration semantic. (`Plugin.js:214-220`.)
- **Confirmed gap** (from `01-markoff-gaps.md` "Progressive rendering"): `registerMarkdownPostProcessor` and `registerMarkdownCodeBlockProcessor` both fire `post-processor-change` on both register **and** unregister (`Plugin.js:144,147,162,166`). Every live `MarkdownPreviewView` re-renders. Markoff's analog must emit an equivalent "processors changed" signal so active reading views recompute.
- **New** (confirmed in this audit): `registerMarkdownCodeBlockProcessor` throws on duplicate `lang` (`MarkdownPreviewRenderer.js:1186-1192`). The plugin wrapper has no pre-check — it re-throws the underlying error. Markoff must use the same non-idempotent behaviour so plugin authors detect conflicts at registration time rather than silently shadowing.
- **New** (confirmed in this audit): `registerMarkdownCodeBlockProcessor` wraps the user's `fn` with a generated post-processor that **supplies `ctx.replaceCode(newSrc)` back to the user callback** (`MarkdownPreviewRenderer.js:1198-1233`). This is how plugins like Dataview can rewrite their own source back. Markoff needs equivalent source-position tracking so a code-block processor can replace its own fenced-code content. Currently blocked on Markoff not tracking per-block source offsets in the scene graph.
- **New** (not in Pass-1 signals): `loadCSS()` is opt-in, not automatic. Plugins that ship a `styles.css` must explicitly call `await this.loadCSS()` in `onload`. The file is read synchronously-then-cached in a `<style>` element; there's no hot-reload. Corbomite's QSS loader should preserve opt-in semantics (don't load all QSS from all plugins on startup).

---

## 13. Open questions

1. **`_userDisabled` set path.** `Plugin.js` reads `_userDisabled` at unload (`Plugin.js:123`) but never writes it. Presumably the plugin loader sets `plugin._userDisabled = true` before calling `plugin.unload()` when the user toggles off. Confirm in the plugin-loader source (not extracted in this audit).
2. **Throws from `onload` — recovery.** If `onload` throws, `load()` rejects but `_loaded` is already `true` and cleanups registered before the throw sit in `_events` indefinitely. Is there a loader-side try/catch that auto-unloads on throw? Need to audit `app.plugins.loadPlugin` (outside this domain).
3. **`registerCliHandler`'s fourth argument.** `Plugin.js:227-230` — `extra` (my name) is passed through to `app.cli.registerHandler` and `unregisterHandler`. Purpose unclear from the call site. Candidate: a token used as the unregister key so the same plugin can register the same flag multiple times. Need `app.cli` source.
4. **`QW` identity.** `registerGlobalFunc` / `registerInstanceFunc` delegate to `QW.addGlobal`/`addForType`. `QW` is not grep-findable in the extracted tree; almost certainly lives inside the `bases` internal plugin bundle (same place the Bases-plugin formula parser `DK` lives). Confirm in Pass 3 after extracting `plugin/internal-plugins/bases/`.
5. **`app.cli` identity.** Similar — `app.cli` is referenced but never defined in the extracted tree. Almost certainly desktop-only Electron command-line hooks.
6. **Watcher glob path.** The 50 ms `onConfigFileChange` debounce is bound in the constructor, but the subscription to `Vault.on('raw')` is not in this file. Where is the filter wired? (A plugin's watcher shouldn't fire on every raw event — only events scoped to `manifest.dir`.) Need `app.plugins.loadPlugin` implementation.
7. **`JD` (styles filename constant).** Referenced at `Plugin.js:281` but defined elsewhere. Almost certainly the string `"styles.css"`. Confirm in Pass 3.
8. **`MarkdownPreviewRenderer` is accessed statically** (`Plugin.js:143,146,160,164-165`). The class exposes `registerPostProcessor` as a static method, not via `this.app.workspace`. This is an architectural exception to the "delegate via `this.app`" pattern — why? The most likely reason is that post-processors are process-global (same list for every `MarkdownPreviewView`), and there's no per-`App` scoping needed. But it leaks a static registry to the plugin surface, meaning the registry survives across vault switches. Document behaviour for Corbomite compat: is a plugin-registered post-processor torn down when the vault is closed? (The plugin's `unload` will be called and the cleanup will run, so yes — but if the vault is closed *without* unloading plugins, the processors leak. Flag.)
9. **`onUserEnable` vs `onload` invocation order.** Is `onUserEnable` guaranteed to run *after* `onload` completes, or can it race? The empty default makes this hard to test; the plugin-loader source will answer.

---

## 14. Recommended Pass 3 synthesis input

1. **This doc is the primary input to `PLUGIN-API-SKETCH.md`.** Every row of §10 becomes a row in the Corbomite plugin-API scaffold with the Qt/KDE-idiomatic translation from §11. Start from the table, not from first principles.
2. **`FEATURE-MATRIX.md` plugin-surface rows.** For the user-facing plugin-system UI (install/enable/disable/update, settings, community-plugins list), Pass 3 must audit `app.plugins` itself (not extracted here). The manifest schema in §2 is the authoritative source for the plugin-directory format Corbomite must read.
3. **`VAULT-FORMAT.md` plugin-data contract.** Two files per plugin: `.obsidian/plugins/<id>/manifest.json` (schema in §2) and `.obsidian/plugins/<id>/data.json` (arbitrary JSON per §3). Plus `.obsidian/community-plugins.json` (array of enabled ids — schema inferred, needs loader-source confirmation). Corbomite's vault migration must copy these directories verbatim to avoid re-install across tools.
4. **Highlight: the auto-unload contract is the single most important invariant.** Every Corbomite plugin-API design must preserve the "register*-pairs-with-unload" property. Design the Corbomite `PluginContext` so plugin authors cannot easily register without also registering the cleanup. Pass 3 `PLUGIN-API-SKETCH.md` should lead with this.

---

## 15. Cross-domain references

| Other domain | Reference type | Brief description |
|---|---|---|
| `views` | consumer | `Plugin.registerView(type, factory)` delegates to `viewRegistry.registerView` (`views/ViewRegistry.js:47`); `Plugin.registerExtensions(exts, type)` delegates to `viewRegistry.registerExtensions` (`views/ViewRegistry.js:73`). On disable, auto-detaches open leaves of the type. See `domains/views.md §10`. |
| `workspace` | consumer | Five verbs flow through `Workspace`: `registerEditorExtension` (`Workspace.js:3889`), `registerObsidianProtocolHandler` (`Workspace.js:3839`), `registerHoverLinkSource` (`Workspace.js:3850`), `registerEditorSuggest` (via `workspace.editorSuggest` = `EditorSuggestManager` at `Workspace.js:179`), and `addRibbonIcon` (via `workspace.leftRibbon.addRibbonItemButton`). Plus `post-processor-change` event fired at `Plugin.js:144,147,162,166`. See `domains/workspace.md §7, §10`. |
| `editor-markdown` | consumer | `registerMarkdownPostProcessor` and `registerMarkdownCodeBlockProcessor` delegate to static methods on `MarkdownPreviewRenderer` (`editor/markdown/MarkdownPreviewRenderer.js:1167, 1184, 1198`). The 1:1-wrapper pattern; post-processor re-rendering invariants owned by `editor-markdown`. See `domains/editor-markdown.md §10, §2, §4`. |
| `editor` | consumer | `registerEditorSuggest` delegates to `EditorSuggestManager.addSuggest` (`views/ViewRegistry.js:247`, a class that logically belongs to `editor` but is physically extracted in `views/`). See `domains/editor.md §10, §1 EditorSuggest`. |
| `bases` | consumer | `registerBasesView` delegates to the enabled `bases` internal plugin's `registerView`. `registerGlobalFunc`/`registerInstanceFunc` delegate to the Bases formula `QW` registry. See `domains/bases.md §10`. |
| `vault` | consumer | `loadData`/`saveData` delegate to `Vault.readPluginData`/`writePluginData` (`vault/Vault.js:292-299`); `getModifiedTime` uses `Vault.adapter.stat`. See `domains/vault.md §3, §10`. |
| `rendering` | sibling | Shares the `post-processor-change` event pipeline; `Plugin.registerMarkdownPostProcessor` is the top-level verb whose *effect* shows up in the rendering pipeline. See `domains/rendering.md §10` (where the verb is cited as a consumer-of-rendering). |
| `ui/components` | parent | `Plugin` extends `Component` (`ui/components/Component.js`). All `registerEvent`/`registerDomEvent`/`registerInterval`/`register`/`addChild`/`removeChild` verbs are inherited. `Component` itself is a Wave-3 sibling doc (pending). |
| `settings` | sibling (de-min overlap) | `settings/PluginSettingTab.js` is a byte-identical extract of the same source window. The `PluginSettingTab` / `Y0` classes and regex `Q0` belong there. `Plugin.addSettingTab(tab)` delegates to `app.setting.addSettingTab` — consumer lives in the `settings` domain (pending). |
| `metadata` | indirect consumer | `MetadataCache.js:994` re-evaluates indexability when `extensions-updated` fires; a plugin-supplied `.foo` extension binding thus affects metadata indexing. See `domains/metadata.md §5` (pending citation — listens to the `extensions-updated` viewRegistry event). |
| `core` (`App`) | dependency | `this.app` is every delegate's ambient root: `app.commands`, `app.workspace`, `app.viewRegistry`, `app.setting`, `app.statusBar`, `app.cli`, `app.internalPlugins`, `app.vault`. `App` owns every registry `Plugin` writes to; see `core/App.js:32,548,553,554`. Also see `core/App.js:611-641` which shows the full list of built-in internal plugins auto-loaded at startup. |

**Short symbols from other domains referenced by name:**

| Short symbol | Defined in | Used here for |
|---|---|---|
| `QW` | Bases plugin bundle (not yet extracted) | `registerGlobalFunc` / `registerInstanceFunc` delegate target; Bases formula globals/instance-methods registry |
| `JD` | outside this domain (likely `settings` or `core`) | Styles filename constant — conventional value `"styles.css"` based on plugin-distribution convention |
| `Y6` | `core` | `app.commands` constructor (`core/App.js:32`); plugin command registry |
| `$0` | `core` / plugin-loader | `app.plugins` constructor (`core/App.js:554`); community-plugins manager |
| `o2` | `core` / internal-plugin-loader | `app.internalPlugins` constructor (`core/App.js:553`) |
| `Nee` | `core` | `app.statusBar` constructor (`core/App.js:548`) |
| `f0` | `editor` / `views` | `EditorSuggestManager` (`workspace/Workspace.js:179`, `views/ViewRegistry.js:247`) |
| `P8`, `dJ`, `g9`, `bJ`, `V6`, `i8`, `r7`, `dee`, `q8`, `k7`, `f7`, `v8`, `wee`, `Z8`, `u8`, `k9`, `b8`, `l3`, `U8`, `See`, `w9`, `p7`, `Pee`, `S9`, `a4`, `Iee`, `O8`, `f9`, `uee`, `Z4`, `M2` | `plugin/internal-plugins/*` (not extracted) | Internal-plugin constructors auto-loaded at `core/App.js:611-641` — the 31-ish built-ins including File Explorer, Search, Graph, etc. Out-of-scope short-symbols to Pass 3. |
| `Component` | `ui/components/Component.js` | `Plugin`'s direct superclass |
| `MarkdownPreviewRenderer` | `editor/markdown/MarkdownPreviewRenderer.js` | Static target of `registerPostProcessor` / `registerCodeBlockPostProcessor` |
| `MarkdownRenderChild` | `editor/markdown/MarkdownRenderChild.js` | Referenced as the correct return type for plugin post-processors that want auto-unload; surface verb lives in `editor-markdown` (see `editor-markdown.md §10`) |
