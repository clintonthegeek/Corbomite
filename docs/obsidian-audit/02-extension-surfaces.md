# Extension surfaces — Pass 1 signal log

Places in Obsidian that look "hookable" — events emitted, registry patterns, subclass entry points, plugin-registered providers. Informs what a future Corbomite plugin API should expose. Not a deep-read of `obsidian/plugin/*` yet; these are surfaces observed from the domains that own them.

## `Plugin` registration verbs (from `obsidian/plugin/Plugin.js`)

These are the methods every community plugin uses. Corbomite's plugin system, if/when built, should expose equivalents (even if keyed to Qt/KDE primitives under the hood).

- **`addCommand(cmd)`** — `obsidian/plugin/Plugin.js`. Registers a command in the palette. Plugins register: ID, display name, callback, hotkey hints, `checkCallback`/`editorCallback`/`editorCheckCallback` variants. Delegates to `app.commands.addCommand`.
- **`addRibbonIcon(icon, title, cb)`** — `obsidian/plugin/Plugin.js`. Adds an activity-bar icon. Plugins typically add: one button per view they want openable with a click.
- **`addStatusBarItem()`** — `obsidian/plugin/Plugin.js`. Returns a DOM element plugins draw word-count / cursor-position / sync-state etc. into.
- **`addSettingTab(tab)`** — `obsidian/plugin/Plugin.js`. Plugins push a `PluginSettingTab` into the settings modal.
- **`registerView(viewType, factory)`** — `obsidian/plugin/Plugin.js` → `obsidian/views/ViewRegistry.js`. Plugins register new `View` subclasses for new file types or side-panels (outline, calendar, graph, kanban, etc.).
- **`registerExtensions(extensions, viewType)`** — associates file extensions with a previously-registered view. Core registers `.md`/`.canvas`/`.pdf`/`.base`; plugins extend (`.kanban`, `.excalidraw`, `.odt`, etc.).
- **`registerMarkdownPostProcessor(fn, sortOrder)`** — `obsidian/plugin/Plugin.js`. Plugins add processors that walk the rendered DOM and mutate it (dataview, tasks, natural-language dates, tag pills). **Priority-sorted.** Fires `post-processor-change` on workspace.
- **`registerMarkdownCodeBlockProcessor(lang, fn)`** — `obsidian/plugin/Plugin.js`. Plugins claim a fenced-code language (`dataview`, `mermaid` (built-in), `tracker`, `chart`). Called with `(source, el, ctx)`.
- **`registerEditorExtension(ext)`** — `obsidian/plugin/Plugin.js` → `app.workspace.registerEditorExtension`. Plugins inject CodeMirror 6 extensions (ViewPlugin, StateField, Decoration). Huge plugin surface — the main reason raw CM is exposed.
- **`registerEditorSuggest(suggest)`** — `obsidian/plugin/Plugin.js`. Plugins add inline autocompleters triggered by text patterns.
- **`registerHoverLinkSource(id, info)`** — `obsidian/plugin/Plugin.js` → `workspace.registerHoverLinkSource`. Plugins declare: "when a `[[link]]` inside **this view type** is hovered, trigger a hover popover." Registers: a `HoverLinkSource` with `display` and `defaultMod`.
- **`registerObsidianProtocolHandler(action, handler)`** — `obsidian/plugin/Plugin.js` → `workspace.registerObsidianProtocolHandler`. Plugins register handlers for `obsidian://<action>?…` URLs (used for deep-links, OAuth callbacks, URL schemes).
- **`registerDomEvent(el, type, cb)`** (via `Component`) — `obsidian/ui/components/Component.js`. Auto-removed on unload. Used by plugins to attach listeners safely.
- **`registerEvent(eventRef)`** (via `Component`) — same. Auto-off'd on unload.
- **`registerInterval(timer)`** (via `Component`) — same. Auto-cleared on unload.
- **`loadData()` / `saveData(data)`** — `obsidian/plugin/Plugin.js`. Read/write `.obsidian/plugins/<id>/data.json`. Debounced watcher detects external edits and reloads.

## Event surfaces

Not exhaustive — just what was observable from the files opened this pass.

- **`Workspace` events** — `obsidian/workspace/Workspace.js`. Emits: `layout-ready`, `layout-change`, `active-leaf-change`, `file-open`, `quick-preview`, `resize`, `window-frame-change`, `swipe`, `file-menu`, `url-menu`, `editor-menu`, `post-processor-change`. Plugins subscribe to `active-leaf-change` and `file-menu` most often (the latter to inject right-click items). **`file-menu`/`url-menu`/`editor-menu` emit a `Menu` instance mid-construction so plugins can `menu.addItem(...)` section-wise.**
- **`Vault` events** — `obsidian/vault/Vault.js`. Emits: `create`, `modify`, `delete`, `rename`, `closed`, `raw`, `config-changed`. `raw` fires for any FS event the adapter observes, including `.obsidian/` internals.
- **`MetadataCache` events** — `obsidian/metadata/MetadataCache.js`. Emits: `changed` (one file's cache updated), `deleted` (file removed with pre-deletion cache for last-chance use), `resolve` (single file re-resolved), `resolved` (all links re-resolved), `finished` (initial index complete). Plugins that need link-awareness (graph, backlinks) subscribe to `resolved`/`changed`.
- **`ViewRegistry` events** — `obsidian/views/ViewRegistry.js`. Emits: `view-registered`, `view-unregistered`, `extensions-updated`. Not often subscribed by user plugins but important for UI refresh.
- **`App` events** (inferred) — `obsidian/core/App.js`. `app.on('css-change', …)`, `app.on('quit', …)` pattern. To confirm in Pass 2.

## Registry / provider patterns

- **`ViewRegistry.registerView(type, factory)` + `registerViewWithExtensions(exts, type, factory)`** — `obsidian/views/ViewRegistry.js`. The central "which class handles this file type?" table. **Primary mechanism for adding file-type support (canvas, base, PDF, image, and any plugin-added types).**
- **`EmbedRegistry`** (inferred from `App.embedRegistry = new aJ()`) — `obsidian/core/App.js`. Analogous registry for `![[embed]]` rendering; plugins can register custom embed types (e.g. a plugin that turns a `![[diagram.drawio]]` embed into a diagram preview).
- **`HoverLinkSource` registry** (`workspace.registerHoverLinkSource`) — `obsidian/workspace/Workspace.js`. Each view-type can opt into hover previews; plugins supply the "display name" and default-modifier-key for their link spans.
- **Obsidian protocol handler registry** (`workspace.registerObsidianProtocolHandler`) — `obsidian/workspace/Workspace.js`. Maps `obsidian://<action>` path segments to handlers.
- **Built-in/"core" plugins registry** — `app.internalPlugins` (seen referenced from `views/FileView.js`: `app.internalPlugins.getEnabledPluginById('file-explorer')`). Not plugin-overridable but worth noting: Obsidian's own features (File Explorer, Search, Quick Switcher, Graph, Backlinks, Outgoing Links, Tag Pane, Properties, Daily Notes, Sync, Publish, Bookmarks, Outline, Canvas, Bases, Templates, Note Composer, Random Note, Slash Commands, Slides, Unique Note, Word Count, Workspaces, ZK-Prefixer, Starred legacy) are implemented as internal plugins with the **same subclass + register* API**. Corbomite's built-in views should follow the same pattern (internal plugins registering views/commands) so the future external plugin API mirrors the internal one.
- **`Menu` section registry** — `obsidian/ui/menu/Menu.js` (`n.sections = []`, `.addSections(...)`, `item.setSection(id)`). Plugins hook `file-menu`/`editor-menu`/`url-menu` events and push items into ordered named sections (`info`, `open`, `selection`, `action`, `action-primary`, `system`, `danger`, `view.linked`). The section order is the coordination mechanism.
- **`Commands` registry** — referenced via `this.commands = new Y6(this)` in `obsidian/core/App.js`. `addCommand`/`removeCommand`/`findCommand` — command-palette backend. Hotkeys (`app.hotkeyManager`) bind to command IDs, not raw callbacks.

## Class-override entry points

- **`View` / `ItemView` / `FileView` / `TextFileView` subclassing** — `obsidian/views/*.js`. Plugins subclass to create new view types. Required overrides: `getViewType()`, `getDisplayText()`, `getIcon()`, `onOpen()`/`onClose()`. `TextFileView` subclasses add `getViewData()`/`setViewData()`/`clear()` and auto-get debounced save.
- **`EditorSuggest<T>` subclassing** — `obsidian/editor/EditorSuggest.js`. Plugins override `onTrigger`, `getSuggestions`, `renderSuggestion`, `selectSuggestion` for inline autocomplete.
- **`Modal` / `SuggestModal<T>` / `FuzzySuggestModal<T>` / `AbstractInputSuggest<T>` subclassing** — `obsidian/ui/popups/*.js`. Plugins subclass for dialogs and pickers.
- **`PluginSettingTab` subclassing** — `obsidian/settings/PluginSettingTab.js`. Single override: `display()` that builds `Setting` rows into `containerEl`.
- **`MarkdownRenderChild` subclassing** — `obsidian/editor/markdown/MarkdownRenderChild.js`. Plugins return one from inside a code-block processor / post-processor so their DOM fragment unloads cleanly when the section is recycled.

## Ambient-context globals plugins read

- **`app`** — passed to every `View.onOpen`, `Plugin.onload`, `Setting` constructor. The `App` instance is the DI root for any plugin operation.
- **`workspace.activeLeaf` / `workspace.getActiveFile()`** — the "currently-visible note" globals.
- **`workspace.activeEditor`** — the current `MarkdownEditView`/`Editor` plugin commands target.
- **`app.metadataCache.resolvedLinks`/`unresolvedLinks`** — read-only link graphs plugins inspect for backlinks / graph rendering.
- **`window.moment`** — moment.js exposed globally for plugin date formatting.

## Surfaces to expand in Pass 2

1. Dedicated pass on `plugin/Plugin.js` to enumerate any `register*`/`add*` missed above.
2. Workspace event payloads — I named the events but not their arg shapes.
3. Vault-config keys (what does `app.vault.setConfig(key, val)` accept? — these keys become the Corbomite-config compat surface).
4. Internal-plugin list — the canonical list of built-ins is load-bearing for Corbomite feature-matrix.

## Pass 2 additions — views

- **`ViewRegistry.registerView(type, factory)`** — factory is `(leaf: WorkspaceLeaf) => View`, not a no-arg factory. Throws on duplicate type (not idempotent). Fires `view-registered` event with `(type)` payload.
- **`ViewRegistry.registerExtensions(exts, type)`** — atomic across the array (pre-scans for dupes, throws before any mutation). Fires a single `extensions-updated` with no payload regardless of how many exts were added. `type` must be pre-registered.
- **`ViewRegistry.registerViewWithExtensions(exts, type, factory)`** — not a separate event surface; two internal `trigger` calls happen (one `view-registered`, one `extensions-updated`). Plugins typically use `Plugin.registerView` + `Plugin.registerExtensions` instead.
- **`canAcceptExtension(ext): boolean`** (override on `FileView` subclass) — if returns `true` for an ext, `WorkspaceLeaf.openFile` routes that ext into the *existing* view instance instead of constructing a new one. The main way a plugin's view can handle multiple extensions with shared state.
- **`TextFileView.requestSave()`** — 2000 ms trailing-edge debounce, sets `dirty = true`. Subclass calls this from input handlers; the save pipeline does the rest, including three-way-merge on external modify and backup-on-failure.
- **`ItemView.onMoreOptionsMenu(menu)`** — override-only hook invoked by the "…" button handler. The canonical section order is `['close','pane','open','action','find','info','info.copy','view','view.linked','system','','danger']`; plugins use `menu.addItem(...).setSection(name)` to slot items.
- **`workspace.on('leaf-menu', cb)`** — fires mid-construction inside `ItemView.onMoreOptions` (`views/View.js:340`). Distinct from `file-menu` (files only); `leaf-menu` is for any leaf's "…" button regardless of view type.
- **`workspace.on('file-menu', cb)`** — primary emission points are `EditableFileView.onPaneMenu` (`views/EditableFileView.js:245`, on right-click of any file view) and `FileView.renderBreadcrumbs`'s folder-context path (`views/FileView.js:106`, on right-click of a breadcrumb with source `'file-explorer-context-menu'`).
- **Deferred-view gate** (`WorkspaceLeaf.isDeferred` getter + `loadIfDeferred()`) — plugins that want to read view state must first `await leaf.loadIfDeferred()` or gate with `if (leaf.isDeferred) return;`. This is the performance optimisation that lets Obsidian restore large workspaces quickly; Corbomite currently has no equivalent.

## Pass 2 additions — vault

- **`FileManager.registerFileParentCreator(ext, fn)`** — underdocumented in Pass 1. Plugins can hook "where does a new file of extension X get created?" by registering a factory that returns a `TFolder`. Consumer: `FileManager.getNewFileParent` (FileManager.js:510). The default `md` factory honours the `newFileLocation` / `newFileFolderPath` config. Only `.md` is pre-registered; internal plugins (canvas, bases) register `.canvas`/`.base`. Counterpart `unregisterFileCreator(ext)` exists; `canCreateFileWithExt(ext)` is a public query.
- **`FileManager.processFrontMatter(file, mut, opts?)`** — not a registration verb but a method on the global `fileManager`, and de facto the plugin API for editing YAML frontmatter. Preserves key order via `yI` (ordered-assign); merges into existing keys rather than overwriting. Templater/Dataview/Tasks use this heavily. Should appear in Corbomite's future plugin API.
- **`FileManager.notifyForBulkUndo(backups, ms?)`** — public helper for any plugin doing bulk rewrites to offer an Undo toast. Not documented in the Obsidian API docs but is a method on the global `fileManager` object. 30 s default timeout.
- **`Vault.setFileCacheLimit(bytes)`** — public tuning knob; plugins handling unusually large notes may need to bump this from the 64 KiB default.
- **`vault.adapter` downcasting** — plugins frequently cast `app.vault.adapter` to `FileSystemAdapter` to reach `getBasePath()`, `getFullPath(rel)`, and `getResourcePath(rel)`. This is the informal "give me absolute paths" API and a mobile-incompat shortcut. Corbomite's `DataAdapter` shim must expose equivalents.
- **`Vault.on('raw', cb)`** — observed in Pass 1 but worth re-emphasising: this is the *only* way a plugin can observe file events inside `.obsidian/` (e.g. the community-plugins.json being edited externally, or plugins spotting their own `data.json` getting rewritten by `git pull`). Distinct from `create`/`modify`/`delete` which are filtered by `ru(path)` out of `fileMap`.
- **`Vault.on('config-changed', key)`** — plugins that change UI based on a vault-config setting (e.g. "respect user's `useMarkdownLinks` preference") must subscribe. Fires on both in-process `setConfig` and external file edits (via debounced `reloadConfig`).

## Pass 2 additions — workspace

Pass 1 named the Workspace events but not their payloads. Pass 2 fills these in (see `docs/obsidian-audit/domains/workspace.md` Section 4 for the authoritative table). Highlights:

- **Five plugin-facing registries** owned by Workspace:
  - `protocolHandlers: Map<action, fn>` — `registerObsidianProtocolHandler(action, fn)` (THROWS on duplicate). Built-ins: `open`, `new`, `search`, `show-plugin`, `show-theme`, `show-release-notes`, `debug-info`, `publish-sites`, `sync-setup`, `vault-setup`, `hook-get-address`. Read by `window.OBS_ACT`.
  - `hoverLinkSources: Record<id, {display, defaultMod}>` — `registerHoverLinkSource(id, info)`. Built-ins: `search`, `preview`, `editor`, `tab-header`. Read by Page-Preview internal plugin.
  - `editorExtensions: Extension[]` — `registerEditorExtension(ext)`; flat list applied to every `MarkdownView` via `updateOptions()` → CM `Compartment`.
  - `operatorFuncConfigs: Record<id, …>` — `registerOperatorFuncConfigs(id, config)`. Read by Bases plugin.
  - `WorkspaceRibbon.items: RibbonItem[]` (per side) — `addRibbonItemButton(id, icon, title, cb)` (wrapped by `Plugin.addRibbonIcon`). Order + hidden persisted at `workspace.json["left-ribbon"].hiddenItems` (key order = item order).
- **`file-menu` / `url-menu` / `editor-menu` / `files-menu` / `leaf-menu` / `tab-group-menu` / `markdown-viewport-menu`** all emit a `Menu` mid-construction. The triggering code calls `Menu.addSections([...]).addItem(...)` for built-ins, **then** `trigger(eventName, menu, …ctx)`. Plugin items added in the subscriber land *after* built-ins in insertion order within their `setSection(id)` bucket. The `""` empty-string section is the "uncategorised" bucket.
- **`active-leaf-change`** payload: `(leaf: WorkspaceLeaf | null)` — debounced 0 ms. **`file-open`** payload: `(file: TFile | null)` — only when `getActiveFile()` differs from `lastActiveFile`. Both gated on `layoutReady === true`.
- **`hover-link`** payload: `{event, source, hoverParent, targetEl, linktext, sourcePath?}`; the `source` MUST be `registerHoverLinkSource`-registered.
- **`quick-preview(file, unsavedContent)`** — per-keystroke debounced cross-pane sync; lets a separate preview pane re-render without waiting for save.
- **`swipe(gesture)`** — `gesture.registerCallback({move, cancel, finish})` is the per-gesture continuation API for plugin swipe handlers.

## Pass 2 additions — bases

Bases exposes a narrow plugin surface (it's a post-1.9 feature and has not yet opened up its internals), but the surfaces that exist are documented below — see `docs/obsidian-audit/domains/bases.md` §10 for details.

- **`Workspace.registerOperatorFuncConfigs(id, config)` + `unregisterOperatorFuncConfigs(id)`** — the workspace-owned registry (`workspace/Workspace.js`) consumed by the Bases-plugin's formula/filter parser (`DK`). Registers a per-`Value`-type comparison/transform operator descriptor (e.g. `contains`, `starts_with`, `matches`, `in`). Read by `DK` during formula parsing to look up operator semantics. **Not directly used inside `bases/`** — the Bases-plugin (at `plugin/internal-plugins/bases/`, not extracted in this audit) populates it during plugin init. Corbomite's equivalent `libs/formula/` parser must accept the same kind of operator-descriptor injection for plugin-added operators.
- **`BasesPluginInstance.registerView(typeKey, factory) + getRegistration(typeKey) + getRegistrations()`** — the Bases-plugin-owned view-type registry. Factory shape (inferred from consumer call sites `bases/BasesView.js:228,1930,2231,2254`): `{name() → string, icon: string, options(viewConfig) → OptionDescriptor[], create(controller) → BasesView-layout}`. Built-in entries: at minimum `"table"`. Plugin-extendability is unconfirmed (the `registerView` verb is not visible inside `bases/` itself) but the consumer side is built to iterate external entries. **Open question:** Pass 3 should confirm by grepping `plugin/internal-plugins/bases/` for `registerView` calls.
- **Value-subclass registration is NOT exposed.** No `registerValueType` / `registerValueSubclass` call is visible inside `bases/`. The 18-member `Value` hierarchy appears closed at source level. Plugin-defined typed values require forking. Flag this for the Corbomite design: if Bases is a "plugin-extensible" library in Corbomite's eventual plugin API, the `Value` hierarchy must be open (virtual `IValue` with plugin-registration) whereas Obsidian's is closed.
- **`RenderContext` overrides are NOT exposed.** The `renderFileLink`/`renderExternalLink`/`renderTag` primitives on `app.renderContext` (defined at `bases/Value.js:31-153` but really an app-wide service) are not plugin-overridable. A plugin that wants custom link rendering in Bases cells would have to replace `app.renderContext` wholesale.
- **No `.base`-file-format extension points.** Parsing is strict; `BasesQuery.parse` rejects unknown top-level keys only into `unrecognizedData` for round-trip preservation, but unknowns are **never consulted for behaviour**. Plugins cannot add custom top-level sections that Bases will honour.

## Pass 2 additions — settings

- **`Plugin.addSettingTab(tab: PluginSettingTab)`** — `settings/PluginSettingTab.js:111`. The sole plugin registration verb in this domain. Internally calls `app.setting.addSettingTab(tab)` (`utils/apiVersion.js:3682`) and registers `app.setting.removeSettingTab(tab)` as an auto-cleanup callback on plugin unload. The settings modal maintains separate lists for community tabs (`pluginTabs[] instanceof PluginSettingTab`) and internal-plugin tabs (`pluginTabs[] instanceof Y0`); both are sorted A-Z by `name` on every `updatePluginSection` call. Core tabs (`settingTabs[]`) stay in registration order.
- **`PluginSettingTab.display(): void`** — `settings/PluginSettingTab.js:355`. The **only required override**. Called every time the user navigates to the tab. Must call `this.containerEl.empty()` at the top (all core tabs do this). The `containerEl` is a `div.vertical-tab-content` that is live in the DOM during the call. Plugins that hold references to child widgets across `display()` calls risk memory leaks (the old DOM is discarded but closure references may survive).
- **`PluginSettingTab.containerEl`** — the root element into which `Setting` rows, `SettingGroup` groups, and raw DOM are appended during `display()`. Plugins may use `new Setting(this.containerEl).setName(…).addToggle(…)` freely. The `Setting` builder is not a `Component` and does not need to be `unload()`-ed.
- **Settings tab `id` contract** — `tab.id` must be unique across all registered tabs. For community plugins it equals `plugin.manifest.id`. For built-in tabs the IDs are fixed strings (`"keychain"`, `"community-plugins"`, others TBD — see `settings.md` Section 13 open question 1). The `openTabById(id)` method allows programmatic navigation: `app.setting.openTabById("hotkeys")` is used by the Community Plugins tab to jump directly to Hotkeys for a specific command.
- **`Plugin.onExternalSettingsChange(): void`** (optional override) — `settings/PluginSettingTab.js:331–350`. If a plugin implements this method, the plugin's debounced `_onConfigFileChange` watcher fires when `data.json` is externally modified (git pull, sync tool). Called only if `file.mtime > _lastDataModifiedTime`. Plugins that support hot-reload should implement this to re-read `loadData()` and refresh their settings UI.

## Pass 2 additions — plugin

Complete enumeration of `Plugin`'s `register*` / `add*` surface (confirming/extending the Pass 1 seed list above). Full consumer-side citations in `domains/plugin.md §10`.

- **`Plugin.addRibbonIcon(icon, title, cb)`** — `plugin/Plugin.js:70-79`. Registers id `"<manifest.id>:<title>"` (note: NOT `:<iconId>`); two calls with same `title` collide. Returns the ribbon button element. Auto-cleanup also calls `buttonEl.detach()`.
- **`Plugin.addStatusBarItem()`** — `plugin/Plugin.js:81-94`. Returns `HTMLElement` pre-classed with `plugin-<sanitised-id>` where only the **first** illegal character in id is rewritten (`[^_a-zA-Z0-9-]` without `g` flag — likely a bug; preserve for compat).
- **`Plugin.addCommand(cmd)`** — `plugin/Plugin.js:96-106`. **Mutates `cmd.id` and `cmd.name` in place** with `<manifest.id>:` and `<manifest.name>: ` prefixes. Returns the (mutated) spec.
- **`Plugin.removeCommand(id)`** — `plugin/Plugin.js:108-110`. Removes by `<manifest.id>:<id>`; unprefixed user-supplied id.
- **`Plugin.addSettingTab(tab)`** — `plugin/Plugin.js:111-116`. Verified as duplicate of the settings-domain entry above.
- **`Plugin.registerView(type, factory)`** — `plugin/Plugin.js:118-124`. On unload, if `_userDisabled === true`, **also calls `app.workspace.detachLeavesOfType(type)`** — disabling a plugin closes all tabs of its view-types, not just unregisters the type. Restart-without-disable leaves open tabs alone.
- **`Plugin.registerHoverLinkSource(id, info)`** — `plugin/Plugin.js:126-131`. Thin 1:1 wrapper.
- **`Plugin.registerExtensions(exts, type)`** — `plugin/Plugin.js:133-139`. Delegates to `viewRegistry.registerExtensions` which is atomic across the array. `type` must be pre-registered.
- **`Plugin.registerMarkdownPostProcessor(fn, sortOrder)`** — `plugin/Plugin.js:140-151`. Fires `workspace.trigger("post-processor-change")` on **both** register and unregister (so live `MarkdownPreviewView`s rerender).
- **`Plugin.registerMarkdownCodeBlockProcessor(lang, fn, sortOrder?)`** — `plugin/Plugin.js:152-170`. Throws on duplicate `lang` (underlying registry rejects). Wrapper auto-creates a post-processor that walks `code.language-<lang>`, replaces with `div.block-language-<lang>`, and provides `ctx.replaceCode(newSrc)` back to the user callback. Both registrations (post-processor + code-block-processor) are auto-cleaned.
- **`Plugin.registerBasesView(type, factory)`** — `plugin/Plugin.js:171-181`. **Returns `boolean`** (`false` if Bases plugin disabled). Only verb on `Plugin` that returns a bool.
- **`Plugin.registerGlobalFunc(fn)`** — `plugin/Plugin.js:182-188`. Delegates to `QW.addGlobal(fn)` where `QW` is defined outside the audit tree (almost certainly the Bases formula globals registry). Key is `fn.name`.
- **`Plugin.registerInstanceFunc(valueType, fn)`** — `plugin/Plugin.js:189-195`. Same `QW` registry, `QW.addForType(type, fn)`; registers a method callable as `value.fnName()` in Bases formulas.
- **`Plugin.registerCodeMirror(cb)`** — `plugin/Plugin.js:196`. **No-op** — legacy CM5 shim. Preserved for backward-compat.
- **`Plugin.registerEditorExtension(ext)`** — `plugin/Plugin.js:197-203`. 1:1 wrapper over `workspace.registerEditorExtension(ext)` which appends to a flat shared list applied to **every** `MarkdownView` via `updateOptions()` (`Workspace.js:3441-3448`). No per-leaf scoping possible.
- **`Plugin.registerObsidianProtocolHandler(action, handler)`** — `plugin/Plugin.js:204-213`. Throws on duplicate action. Built-ins registered at `workspace/Workspace.js:1238-1488`: `open`, `new`, `search`, `show-plugin`, `show-theme`, `show-release-notes`, `debug-info`, `publish-sites`, `sync-setup`, `vault-setup`, `hook-get-address` (11 total).
- **`Plugin.registerEditorSuggest(suggest)`** — `plugin/Plugin.js:214-220`. Delegates to `workspace.editorSuggest.addSuggest` (`views/ViewRegistry.js:247`). Insertion-order iteration; first non-null `onTrigger` wins (built-ins registered first, so plugin overrides of `[[`/`#` are shadowed not prioritised).
- **`Plugin.registerCliHandler(flag, help, handler, extra)`** — `plugin/Plugin.js:221-232`. Desktop-only. Help auto-wrapped `"[<manifest.name>]: <help>"`. Fourth parameter's purpose unclear from the call site (see `plugin.md §13` open question 3).
- **`Plugin.loadData()` / `Plugin.saveData(data)`** — `plugin/Plugin.js:233-271`. Path is `<manifest.dir>/data.json`. `saveData` sets `_lastDataModifiedTime = Date.now()` **before** the write and passes the same value as `{mtime}` to the adapter, which is how self-edit suppression works for the external-change watcher. Non-atomic writes — a crash mid-write silently truncates the file; `readJson` swallows the `SyntaxError` to `console.error` and returns `undefined`, which plugins treat as "no data" (falling back to defaults). Corbomite should fix this with `QSaveFile`-style atomic rename.
- **`Plugin.loadCSS()`** — `plugin/Plugin.js:272-302`. **Opt-in**, not automatic. Reads `<manifest.dir>/<JD>` where `JD` is a constant defined elsewhere (conventionally `"styles.css"`). Inserts `<style>` into `document.head` **before** `app.customCss.styleEl` so the vault's custom CSS wins.
- **`Plugin.onUserEnable()`** — `plugin/Plugin.js:303`. Empty default. Called once per session, after the user toggles the plugin on (not called on initial auto-load).
- **`Plugin.onExternalSettingsChange()`** — optional override. Invoked by `_onConfigFileChange` (`plugin/Plugin.js:331-350`, debounced 50 ms) when `data.json`'s on-disk mtime advances past `_lastDataModifiedTime`. The subscription to `Vault.on('raw')` is wired by the plugin loader (outside this domain).

Inherited from `Component` (audited fully in the pending `ui/components/` sibling doc):

- **`Component.register(fn)`** — LIFO cleanup stack; popped by `unload()`.
- **`Component.registerEvent(eventRef)`** — auto-calls `eventRef.e.offref(eventRef)` on unload.
- **`Component.registerDomEvent(el, type, cb, opts?)`** — auto-`removeEventListener` on unload.
- **`Component.registerInterval(handle)`** — auto-`clearInterval` on unload.
- **`Component.registerScopeEvent(kmRef)`** — auto-`kmRef.scope.unregister(kmRef)` on unload.
- **`Component.addChild(child)`** — auto-`child.unload()` on parent unload.

## Pass 2 additions — leaf-utilities (utils / platform / secrets / network)

- **`requestUrl(opts: RequestUrlParam | string): RequestUrlResponsePromise`** — `network/requestUrl.js:5`. Primary plugin HTTP surface. On Electron the underlying `Nf()` helper makes a native Node.js HTTP/HTTPS request — **CORS is bypassed entirely** because there is no browser to enforce it. The response promise exposes lazy `.text`, `.json`, `.arrayBuffer` accessors. Error on status ≥ 400 (unless `opts.throw = false`): throws `RequestUrlError` with `.status` and `.headers` fields. CORS bypass is a key plugin convenience; Corbomite's `QNetworkAccessManager` is also a native HTTP client and provides the same behaviour by default.
- **`request(opts): Promise<string>`** — `network/request.js:5`. Convenience wrapper returning only the text body. Equivalent to `(await requestUrl(opts)).text`.
- **`app.secretStorage.setSecret(id, value)` / `getSecret(id)` / `deleteSecret(id)` / `listSecrets()` / `isEncryptionAvailable()`** — `secrets/SecretStorage.js:800–826`. Plugin secret-key store backed by OS keychain. ID must match `/^[a-z0-9-]+$/`, ≤ 64 chars. Corbomite mapping: `KWallet` or `QtKeychain`.
- **`debounce(fn, delay?, immediate?)`** — `utils/debounce.js:4`. Plugins import and use directly. Returns the wrapped function plus `.cancel()` and `.run()` controls.
- **`moment`** — `utils/moment.js:5`. Plugins import the globally-configured Moment.js instance. Locale is already set to `getLanguage()` before `onload` fires.
- **`Platform`** — `platform/Platform.js:5`. Plugins read capability flags to branch on OS/form-factor. All `isDesktop`/`isMacOS`/`isLinux`/etc. flags are set before any plugin `onload`.
- **`Keymap.isModEvent(event)`** — `platform/Keymap.js:202`. Plugins that render clickable links call this to determine whether to open in same tab, new tab, split, or window. Returns `"tab" | "split" | "window" | false`. Corbomite link-click handlers must apply identical logic: middle-click or Mod → tab; Mod+Alt → split; Mod+Alt+Shift → window.
- **`resolveSubpath(cache, subpath)`** — `utils/resolveSubpath.js:5`. Plugins that render custom embeds or handle link clicks call this to resolve `#heading`, `#^block`, `#[^footnote]` subpaths to position ranges in `CachedMetadata`. Critical for any embed plugin that honours `[[Note#subpath]]` links.
- **`requireApiVersion(minVersion)` / `apiVersion`** — `utils/requireApiVersion.js:5` / `utils/apiVersion.js:3850`. Standard plugin-compatibility guard. Current value: `"1.12.7"`.
- **`arrayBufferToBase64` / `base64ToArrayBuffer` / `arrayBufferToHex` / `hexToArrayBuffer` / `getBlobArrayBuffer`** — byte-conversion utilities plugins import for binary attachments, cryptographic operations, and file reads.

## Pass 2 additions — ui-bundle (components / icons / menu / popups)

See `docs/obsidian-audit/domains/ui-bundle.md` §10 for full details. Highlights:

- **`Component` subclassing** (`ui/components/Component.js:5-74`) — the universal lifecycle primitive. `Plugin`, `View`, `Modal`, `MarkdownRenderChild`, `HoverPopover`, `Menu` all extend it. Plugin-API verbs `registerDomEvent`/`registerEvent`/`registerInterval`/`addChild` are all instance methods inherited from `Component`. Corbomite has no equivalent today; plugin-API spec must translate this onto a `QObject`-parent-ownership + cleanup-thunk-queue primitive.
- **`Plugin.addIcon(name, svg)`** → `ui/icons/addIcon.js:5`. `svg` is the SVG *inner-HTML*; the wrapper `<svg>` is synthesised by `getIcon`. Lucide names (`lucide-*` prefix) are reserved — `addIcon("lucide-x", …)` silently loses to the built-in because the `lucide-` branch runs first in `getIcon`. Plugins are expected to re-register in `onload`; no auto-cleanup wire-up is visible inside `ui/icons/`.
- **Menu subscription via Workspace events** (see `workspace.md` §4 / this doc under Workspace above). The receiver gets a `Menu` mid-construction with `addSections([...])` already called; plugins push via `menu.addItem(i => i.setSection(id).setTitle(…).setIcon(…).onClick(…))`. Adding items after `menu._loaded` is a silent no-op (`Menu.js:206`).
- **`Menu.addSections(ids)`** — plugins may declare new section IDs; unknown ones auto-push onto the end of `sections[]` during `sort` (so forgetting to pre-declare doesn't break). No priority/weight — strictly section-order position.
- **Modal / SuggestModal / FuzzySuggestModal / AbstractInputSuggest subclassing** — four popup-class extension points (`ui/popups/*`). The abstract contracts are stable across Obsidian minor versions and map to Qt `QDialog`/`QCompleter`/custom `QFrame` primitives.
- **`HoverPopover`** is instantiable by plugins directly (Page-Preview internal plugin does so on every `workspace.trigger("hover-link", …)` event), but it requires a parent `Component` for lifetime binding. Plugins that render hover previews for their own link-types must: (1) `registerHoverLinkSource` in `Plugin.onload`, (2) `trigger("hover-link", …)` from their view's hover handler, (3) ensure Page-Preview is enabled (else no popover fires).
- **`new Notice(msg, durationMs)`** — direct-construction API; no registration. `durationMs = 0` disables auto-hide. Chainable `.addButton(text, cb)` for actionable notices. Plugins often construct ad-hoc `new Notice("Error: " + err.message, 5000)` for error reporting.

## Pass 2 additions — core

- **`Events` subclassing** (`core/Events.js:5-62`) — the universal pub-sub mixin on which `Vault`, `Workspace`, `MetadataCache`, `ViewRegistry`, `WorkspaceItem`, `WorkspaceLeaf` all depend. Plugins subclass `Events` directly when they need their own emitter. Public methods: `on(name, fn, ctx?) → EventRef`, `off(name, fn)` (identity-match), `offref(ref)`, `trigger(name, …args)`, `tryTrigger(ref, args)`. **`tryTrigger` re-throws asynchronously** via `setTimeout(…, 0)` — one listener throwing does not abort dispatch. Corbomite's Qt-signal bridge must emulate this.
- **`new Scope(parent?)`** (`core/Scope.js:5-40`) — construct a hierarchical key-handler stack. Plugins typically create a `Scope` in a `Modal`/`Menu`/custom-view subclass and `app.keymap.pushScope(s)` when activating. `scope.register(modifiers, key, fn)` adds a handler; return `false` from `fn` to consume, `undefined` to fall through to parent (only if catch-all). `unregister(h)` removes by identity. `setTabFocusContainerEl(el)` wires a focus-trap. **`Modal`, `Menu`, `EditorSuggest`, `MarkdownView`, `BasesView`, `PopoverSuggest`, `AbstractInputSuggest` all allocate their own `Scope`.**
- **`app.nextFrame(cb)`** (`core/App.js:434-447`) — coalesces DOM mutations onto a single `requestAnimationFrame`. Plugins that do per-keystroke DOM work should route through this rather than per-call `requestAnimationFrame` to share the frame with the editor.
- **`app.on(name, cb)` is a no-op stub** (`core/App.js:3254`) — plugin code calling this silently registers nothing. `css-change` / `quit` / `resize` / `layout-ready` all fire on `app.workspace`, not `app`. Corbomite must not expose a plausible-looking but inert `app`-level `on`.
- **No `App` `register*` verbs.** App is a DI container; plugin-facing surfaces live on its children (`commands`, `viewRegistry`, `embedRegistry`, `hotkeyManager`, `renderContext`, `customCss`, `metadataTypeManager`, `fileManager`, `dragManager`, `internalPlugins`, `plugins`, `foldManager`, `setting`).
- **`InternalPlugins` is a closed registry** (`core/App.js:611-641`) — third-party plugins cannot add built-ins. `app.internalPlugins.getEnabledPluginById(id)` is the primary read API; Obsidian's 31 built-in IDs are documented in `domains/core.md` §7.
- **`app.commands.addCommand(cmd)`** is registered from `Plugin` via the `Plugin.addCommand` wrapper (see `domains/plugin.md` §10). The base registry API (`addCommand`, `removeCommand`, `findCommand`, `executeCommandById`) is reachable directly via `app.commands` for advanced use. See `domains/core.md` §6 for the full App-core command list.
