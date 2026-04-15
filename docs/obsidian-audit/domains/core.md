# `obsidian/core` — top-level App, scope, event bus

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/core/`
**File count:** 3
**Files:** `App.js` (3308 lines, `app.js:225984-229287`), `Events.js` (70 lines, `app.js:79976-80040`), `Scope.js` (223 lines, `app.js:60149-60366`).

**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> Boot wiring. `App` is the "god object" — owns `vault`, `metadataCache`, `workspace`, `commands`, `hotkeyManager`, `dragManager`, `viewRegistry`, `embedRegistry`, `customCss`, `renderContext`, `secretStorage`, `plugins`, `internalPlugins`, `setting`. `Events` is the universal observer mixin (`on`, `off`, `offref`, `trigger`, `tryTrigger`). `Scope` is a hierarchical key-handler stack rooted in `Keymap.global`.
>
> **Key exports / primitives:**
> - `App` — central registry / DI container; `commands`, `keymap`, `plugins`, `internalPlugins`, `setting`, `dragManager`, `embedRegistry`, `viewRegistry`, `renderContext`, `customCss` all live here.
> - `Events` — `on/off/offref/trigger/tryTrigger`. Subclass for any pub-sub object (Vault, MetadataCache, Workspace, ViewRegistry...).
> - `Scope` — push/pop keybinding context; `register(modifiers, key, fn)`, `handleKey`, parent-chain lookup. `Modal` and `Menu` each own a Scope for in-popup hotkeys.
>
> **On-disk contracts:** none directly; delegates to `vault.adapter` for `.obsidian/` config files (App initialises `appId` and `loadLocalStorage('DebugMode')`).
>
> **Cross-domain dependencies:** the root — every other domain consumes one of these three.

**De-minifier artifact note.** `core/Scope.js` is **byte-identical to `platform/Keymap.js` except for the `// public API symbol:` header comment** — both carry the same `app.js:60149-60366` range and both define `Scope`, `Rg` (dynamic-child-delegate subclass used by `Workspace.scope`), and the full `Keymap` class. Canonical ownership: `Scope`/`Rg` audited here; `Keymap` audited in `platform/Keymap.js`. `Events.js` has bleed-over after the declared range (utils path-validation constants `HT, zT, qT, jT, UT, WT, _T`) — not audited. `App.js` has no bleed-over.

---

## 1. Public API surface

Three symbols. `App` is a god-object; `Events` and `Scope` are primitives. `Events` and `Scope` are exposed to plugins via the `obsidian` module export; `App` is reached via `this.app` on every `Plugin`, `View`, `Modal`, `Component`.

### `App`

- **Kind:** class. **Exported as:** `App`.
- **Signature:** `new App(adapter, appId)`. Two-phase construction: synchronous ctor builds **boot-minimal** fields (`embedRegistry`, `viewRegistry`, `keymap`, `scope`, `commands`, `hotkeyManager`, `dragManager`, `dom`, `customCss`, `shareReceiver`, `renderContext`, `secretStorage`, `cli`), then async `initializeWithAdapter(adapter)` builds **vault-dependent** fields (`vault`, `workspace`, `fileManager`, `statusBar`, `metadataCache`, `metadataTypeManager`, `setting`, `foldManager`, `internalPlugins`, `plugins`). `App.js:460-1174`.
- **Purpose:** The DI container. Every other domain reaches siblings via `this.app.<field>`. `App` itself does little work — CSS/theme glue, command registration, local-storage shims, platform-adapter wrappers.
- **Lifecycle:** Constructed once per Electron/Capacitor window. No `unload()`. **Destroyed by process exit.** "Open another vault" → `openVaultChooser()` → `ipcRenderer.sendSync("starter") + window.close()` (`App.js:3051`). No same-process teardown.
- **Mixes in:** Neither `Component` nor `Events`. The prototype **stubs `on` as a no-op** (`App.js:3254`) so `app.on(...)` silently registers nothing. This **invalidates the Pass 1 hypothesis** that `app.on('css-change')`/`app.on('quit')` are real — those events fire on `app.workspace`, not `app` (Section 4).
- **Method groups:**
  - **Boot / async.** `initializeWithAdapter` (`:463`), `runOpeningBehavior` (`:3070`), `registerQuitHook` (`:3211`), `registerCommands` (`:1220`).
  - **Config reaction.** `onConfigChanged(key)` (`:1175`) — switch on key name → matching `update*` method. Subscribed to `vault.on("config-changed")` at `:534`.
  - **Theme/CSS/appearance.** `update{Theme,AccentColor,FontFamily,FontSize,TabSize,InlineTitleDisplay,FloatingNavigationDisplay,AutoFullScreenDisplay,ViewHeaderDisplay,RibbonDisplay,UseNativeMenu,MobileFrameTheme}`; `changeTheme`, `get{Theme,AccentColor}`, `setAccentColor`, `isDarkMode`, `disable/enableCssTransition`.
  - **Local-storage shims.** `loadLocalStorage`, `saveLocalStorage`, static `getOverrideConfigDir(appId)`. Section 8.
  - **Platform wrappers.** `openWithDefaultApp`, `showInFolder`, `getObsidianUrl`, `copyObsidianUrl`, `getAppTitle`, `getWebviewPartition`, `fixFileLinks`, `importAttachments`, `saveAttachment`.
  - **Vault/help flow.** `openVaultChooser(closeWindow?)` (`:3051`), `openHelp()` (`:3063`).
  - **Settings flags.** `get/setSpellcheckLanguages`, `isVimEnabled`, `emulateMobile`, `debugMode`, `showReleaseNotes`.
  - **Frame scheduler.** `nextFrame(cb)` / `nextFrameOnceCallback(cb)` coalesce DOM mutations onto a single `requestAnimationFrame`.

### `Events`

- **Kind:** class. **Exported as:** `Events`. Plugins subclass this; instances that extend it are everywhere (`Vault`, `Workspace`, `MetadataCache`, `ViewRegistry`, `WorkspaceItem`, `WorkspaceLeaf`).
- **Signature:** `new Events()` — zero-arg. Allocates `this._ = {}` (listener map).
- **Purpose:** Universal pub-sub mixin. Every Obsidian emitter is an IIFE `(function(e){ … })(Events)` so the returned ctor inherits `Events.prototype`. Subclasses call `this.trigger(name, …args)`; consumers call `emitter.on(name, cb, ctx)`.
- **Lifecycle:** Trivial — no explicit destroy. Cleanup is the consumer's job via `off(name, fn)` / `offref(ref)` or (usual) `Component.registerEvent(ref)` which auto-`offref`s on `onunload`. Stale listeners on a dead emitter silently stop firing.
- **Methods:**
  - `on(name, fn, ctx?) → EventRef` (`:10`). Pushes `{e, name, fn, ctx}` onto `this._[name]`. Return value is the **EventRef**.
  - `off(name, fn)` (`:22`). Removes every listener where `listener.fn === fn` (identity). **Collision:** two plugins sharing a `fn` reference both get removed.
  - `offref(ref)` (`:31`). Scalpel — removes exactly the `EventRef`. Used by `Component.registerEvent`.
  - `trigger(name, …args)` (`:42`). Snapshots `this._[name].slice()` so a self-unregistering listener doesn't shift iteration, then `tryTrigger`s each. Registration-order delivery.
  - `tryTrigger(ref, args)` (`:51`). Invokes `ref.fn.apply(ref.ctx, args)` inside `try/catch`; a thrown error is **re-thrown asynchronously** via `setTimeout(…, 0)`. One throwing listener does **not** short-circuit delivery to others. Corbomite's Qt-signal default is opposite (synchronous propagate) — compat requires emulating the async-rethrow.
- **Mixes in:** Neither (it is the bottom of the inheritance chain).

### `Scope`

- **Kind:** class. **Exported as:** `Scope`. Also private subclass `Rg` in the same file.
- **Signature:** `new Scope(parent?)`. Root scope is `Keymap.global.rootScope` (aliased as `app.scope` at `App.js:31`).
- **Purpose:** Hierarchical keybinding stack. Each `Scope` owns an array of `KeyHandler` records. `Modal`, `Menu`, `EditorSuggest`, `MarkdownView`, `BasesView`, `PopoverSuggest`, `AbstractInputSuggest`, top-level `Workspace` each allocate their own `Scope` chained to a parent. `Keymap.global` routes every `window.keydown` to `activeScope.handleKey(ev, ctx)`; fallback-delegates to parent on "unhandled".
- **Lifecycle:** Created with the owning widget. `Keymap.pushScope(s)`/`popScope(s)` swap the active scope. **Not `Events`-derived** — no pub-sub.
- **Methods:**
  - `register(modifiers, key, fn) → KeyHandler` (`:12`). `modifiers`: `null` (any) or array like `["Mod","Shift"]`. `key`: vkey string (`"ArrowDown"`, `"E"`) or `null` (any non-modifier). `fn(ev, ctx)` returns `false` to consume (calls `preventDefault`+`stopPropagation`), `undefined` to let parent try, truthy to consume silently. Modifiers normalised via `Keymap.compileModifiers` — `"Mod"` → `"Meta"` on macOS, `"Ctrl"` elsewhere; sorted comma-joined.
  - `unregister(handler)` (`:21`). Identity remove.
  - `setTabFocusContainerEl(el)` (`:24`). Tab-trap: a `focusin` outside `el` re-focuses inside (see `Keymap.onFocusIn`). Used by `Modal`.
  - `handleKey(ev, ctx)` (`:27`). Iterate `keys`. If `Keymap.isMatch(h, ctx)`: invoke `h.func`; if it returned `undefined` **and** the handler was a catch-all (`key===null && modifiers===null`), keep walking; else return. On no match, delegate to `parent.handleKey`.
- **Subclass `Rg(parent, cb)`** (`:41`). Overrides `handleKey` to first call `cb()`; if it returns a `Scope`, delegate to that; else super. Used by `Workspace`: `new Rg(app.scope, () => activeLeaf?.view?.scope ?? null)` (`workspace/Workspace.js:68`). **This is the bridge from global hotkeys to per-view hotkeys.**
- **Mixes in:** Neither.

---

## 2. Data structures

### `App` field catalog

Every `app.<field>` referenced by Wave 1+2 Pass 2 docs; grouped by boot phase.

**Boot-minimal (ctor, App.js:10-40)** — constructed before vault adapter:
- `appId: string` — ctor arg. Persistent per-vault; keys localStorage, IndexedDB, webview partition. Section 8.
- `title`, `isMobile`, `lastEvent`, `nextFrameEvents`, `nextFrameTimer` — core bookkeeping.
- `embedRegistry` (`aJ`, → rendering), `viewRegistry` (`ViewRegistry`, → views, Events-mixed), `keymap` (`Keymap.init()`, → platform; window-scoped singleton), `scope` (`Keymap.global.rootScope`, root of hotkey stack).
- `commands` (`Y6`, → plugin; Section 7), `hotkeyManager` (`zb`, → plugin; reads `.obsidian/hotkeys.json`), `dragManager` (`xP`, → plugin).
- `dom` (`wte`, → ui; `appContainerEl`/`workspaceEl`/`statusBarEl`/`horizontalMainContainerEl`), `customCss` (`Ib`, → plugin; loads `.obsidian/snippets/*.css`), `shareReceiver` (`t4`, mobile), `renderContext` (`RenderContext`, → rendering), `secretStorage` (→ secrets), `cli` (`CA`, → platform), `appMenuBarManager` (desktop), `mobile{Toolbar,Navbar,TabSwitcher,QuickActions}` (mobile).

**Vault-dependent (initializeWithAdapter, :515-555)** — require `adapter`:
- `vault` (`Vault(adapter)`, → vault, Events-mixed), `workspace` (`Workspace(app, el)`, → workspace, Events-mixed), `fileManager` (→ vault; rename/frontmatter/trash), `statusBar` (`Nee`, → ui), `metadataCache` (`MetadataCache(app, vault)`, → metadata, Events-mixed; `appId` keys IndexedDB), `metadataTypeManager` (`RL`, → metadata), `setting` (`vte`, → settings), `foldManager` (`Q6`, → editor), `internalPlugins` (`o2`, → plugin; Section 7), `plugins` (`$0`, → plugin).

#### Vault-switch behaviour (CRITICAL for Corbomite)

**No App field survives a vault switch, because the entire App does not survive.**

1. User clicks "Open another vault" → `App.openVaultChooser()` (`App.js:3051`).
2. Desktop: `ipcRenderer.sendSync("starter")` spawns a **new Electron window** for the vault chooser; `window.close()` tears down the current window. Every `App` field, `Events` listener map, `Scope` stack, `Vault`, `Workspace`, `MetadataCache`, plugin state — all go with the window.
3. Mobile: `localStorage.removeItem("mobile-selected-vault")` + `location.reload()` — same effect.

There is **no in-process vault swap**. Boot-minimal fields (`keymap`, `scope`, `commands`, etc.) live in that window's memory. `appId` is vault-derived (Section 8) so the new process has a different `appId` → different IndexedDB, localStorage namespace, cache.

**Corbomite implication:** the Phase-1 vault-switching crash bug (memory `project_vault_switching.md`) arises from **reusing `VaultService` / `MainWindow` / `NoteService` / models across a vault switch**, which Obsidian never does. Align by either (a) destroying `MainWindow` on vault switch (Obsidian-like; Kate-session pattern — `memory/reference_kate_sessions.md`), or (b) rigorously re-constructing every child service in dependency order. Option (a) is simpler and compat-aligned. Pass 3 `GAP-ANALYSIS.md` should promote this.

### `EventRef` and `KeyHandler`

```typescript
// core/Events.js:14
interface EventRef { e: Events; name: string; fn: Function; ctx: any; }

// core/Scope.js:13
interface KeyHandler {
  scope: Scope;
  modifiers: string | null;    // normalised "Alt,Ctrl,Meta,Shift" substring, or null = any
  key: string | null;          // vkey string (e.g. "ArrowDown"), or null = any non-modifier
  func: (ev: KeyboardEvent, ctx: {modifiers, key, vkey}) => false | undefined | any;
}
```

`EventRef` is opaque — only `offref(ref)` / `Component.registerEvent(ref)`. `KeyHandler.func` return: `undefined` = delegate to parent (only if catch-all), `false` = consume w/ `preventDefault+stopPropagation`, truthy = consume silently.

---

## 3. On-disk contracts

`No on-disk contracts.` The three `core/` files do not read or write any file. Vault-config I/O is delegated to `vault.adapter` (see `vault.md` §3) and `.obsidian/hotkeys.json` / `.obsidian/snippets/` are owned by `hotkeyManager` / `customCss` (plugin domain).

**One exception — `localStorage` as pseudo-disk** (`App.loadLocalStorage`/`saveLocalStorage` at `:3237-3253`):

- `localStorage["<appId>-<key>"]` — JSON-encoded values. Known keys: `"DebugMode"`; plugins may add their own.
- `localStorage["<appId>-config"]` — override config-dir path (read by static `App.getOverrideConfigDir`, `:3272`).
- `localStorage["mobile-selected-vault"]` — mobile-only vault selector (not appId-scoped — it *is* the selector).
- `localStorage["spellcheck-languages"]` — global, not appId-scoped.
- `localStorage["EmulateMobile"]` (desktop debug flag), `localStorage["vim"]` (mobile vim flag), `localStorage["most-recently-installed-version"]`.

Not vault-format compat surface; App's own namespace.

---

## 4. Events emitted

**No events emitted on `app` itself.** `App.prototype.on` is a no-op stub (`:3254`); `app.on(...)` silently fails. Events Pass 1 attributed to `app` are emitted on `app.workspace`.

### `app.workspace` (extends `Events`) — events the App domain triggers

| Event | Payload | Trigger sites | Notes |
|---|---|---|---|
| `css-change` | `()` | `:1293` (theme toggle), `:2808` (`updateTheme` class-change), `:2883` (`updateFontFamily` post), `:2896` (`updateFontSize` post) | **On workspace**, not app. Theme-aware plugins use `workspace.on("css-change")`. |
| `quit` | `(tasks: Eb)` | `:3219` in `onbeforeunload` | `Eb` = quit-tasks collector; plugins push async cleanup promises; `isEmpty()` ⇒ synchronous return, else `e.preventDefault(); returnValue="Saving..."`, await `i.promise()`, `window.close()`. `Eb` declared outside `core/`. Corbomite equivalent: `QCloseEvent::ignore()` + async save orchestrated by `MainWindow::closeEvent`. |

Total `.trigger(` call sites in `App.js`: five (four `css-change`, one `quit`).

`Events` (mixin) and `Scope` emit nothing intrinsic. Pass-1 speculative `app.on('resize')`/`app.on('layout-ready')` confirmed to be **workspace-level** (`workspace.md` §4).

---

## 5. Events consumed

| Listener file | Subscribes to | Why |
|---|---|---|
| `App.initializeWithAdapter` (`:531`) | `vault.on("closed")` | Calls `openVaultChooser(true)` to reopen chooser + kill window. |
| `App.initializeWithAdapter` (`:534`) | `vault.on("config-changed", onConfigChanged)` | Central reactor rebroadcasting `.obsidian/app.json` + `appearance.json` mutations to the matching `update*` method. `changeTheme` therefore only writes the config; the event loops back. |
| `App` ctor (`:127`) | `matchMedia("(prefers-color-scheme: dark)")` change | Re-runs `updateTheme` when `theme === "system"`. |
| `App` ctor (`:562`) | `electron.remote.nativeTheme.on("updated")` | Desktop OS-theme fallback. |
| `App.registerQuitHook` (`:3213`) | `window.onbeforeunload` | Emits `workspace.trigger("quit", Eb)` — Section 4. |
| `Keymap` (via `Keymap.init()` at `:30`) | `window.addEventListener("keydown"/"focusin")` | Routes every key to `app.scope.handleKey`; focus events drive tab-trap. Only reason `Scope.handleKey` ever runs. |

---

## 6. Commands registered

`App.registerCommands()` (`App.js:1220`) registers 30+ commands on `app.commands`. Hotkeys via `Fb(modifiers, key)` (from `platform/Keymap.js`).

| Command ID | Display name | Hotkey | Effect | Line |
|---|---|---|---|---|
| `app:go-back` / `app:go-forward` | Navigate back/forward | `Mod+Alt+Arrow{Left,Right}` | `activeLeaf.history.{back,forward}()` gated by `view.navigation` | 1223, 1234 |
| `app:open-vault` | Manage vaults | — | `openVaultChooser()` | 1245 |
| `app:switch-vault` / `app:open-another-vault` | Change/open vault | — | Vault-switcher modal (`xD`/`TD`) | 1251, 1262 |
| `app:show-tab-switcher` | Show tab switcher | — | `mobileTabSwitcher.show()` (phone) | 1271 |
| `theme:toggle-light-dark` | Toggle light/dark | — | Flips body class + `css-change`; if not `"system"`, `changeTheme("moonstone"/"obsidian")` | 1280 |
| `theme:switch` | Change theme | — | Theme-chooser modal | 1301 |
| `app:open-settings` | Open settings | `Mod+,` | `setting.open()` | 1308 |
| `app:show-release-notes` | Release notes | — | `showReleaseNotes()` | 1317 |
| `markdown:toggle-preview` | Toggle reading view | `Mod+E` | `activeEditor.toggleMode()` | 1324 |
| `markdown:add-metadata-property` | Add property | `Mod+;` | `metadataEditor.addProperty()` (properties plugin enabled) | 1342 |
| `markdown:add-alias` | Add alias | — | `addProperty("aliases")` | 1369 |
| `markdown:clear-metadata-properties` | Clear properties | — | Strip frontmatter | 1396 |
| `app:delete-file` | Delete current file | — | `fileManager.promptForDeletion(file)` | 1412 |
| `app:toggle-ribbon` / `editor:toggle-readable-line-length` / `editor:toggle-line-numbers` / `app:toggle-{left,right}-sidebar` / `app:toggle-default-new-pane-mode` | Toggles | — | All do `vault.setConfig(key, !val)` or sidebar toggle | 1421-1501 |
| `app:open-help` | Open help | `F1` | `openHelp()` | 1502 |
| `app:reload` / `app:show-debug-info` / `app:open-sandbox-vault` | Diagnostics | — | Reload / debug modal / sandbox IPC | 1509-1524 |
| `window:toggle-always-on-top` / `window:zoom-{in,out}` / `window:reset-zoom` | Window | — | Electron window APIs | 1532-1563 |
| `file-explorer:new-file` / `new-file-in-current-tab` / `new-file-in-new-pane` | Create note | `Mod+N`, —, `Mod+Shift+N` | `fileManager.createAndOpenMarkdownFile("", target)` | 1570-1587 |
| `open-with-default-app:open` | Open with default | — | `openWithDefaultApp(file.path)` | 1596 |
| `file-explorer:move-file` / `duplicate-file` | Move / duplicate | — | Move modal / duplicate via `fileManager` | 1607, 1616+ |

**Design note:** Despite `file-explorer:*`/`open-with-default-app:*` prefixes, these are **App-core** commands, not plugin-registered — they must be palette-visible before the corresponding plugin loads.

---

## 7. Registries owned

### `Commands` (`app.commands`, class `Y6`)

Class lives in adjacent app.js lines (unextracted). Call-site-inferred shape:

```typescript
interface Command {
  id: string;             // globally unique; convention "<domain>:<slug>"
  name: string;           // i18n'd display name
  icon?: string;          // Lucide icon id or legacy glyph
  hotkeys?: KeyBinding[];
  mobileOnly?: boolean;
  // exactly one of:
  callback?: () => void;
  checkCallback?: (checking: boolean) => boolean | void;
  editorCallback?: (editor: Editor, view: MarkdownView | MarkdownFileInfo) => void;
  editorCheckCallback?: (checking: boolean, editor, view) => boolean | void;
}
```

**Callback dispatch** (inferred from `App.js:1227-1619` + `workspace.md` §6):
- `callback` — always runnable; executes immediately.
- `checkCallback(true)` — runnability check, return truthy ⇒ enabled. `checkCallback(false)` actually runs.
- `editorCallback` / `editorCheckCallback` — same, but only dispatched when `app.workspace.activeEditor` is an `Editor`. `editor:…` commands never surface when a non-markdown view is active.

- **API:** `addCommand(cmd)`, `removeCommand(id)`, `findCommand(id)`, `executeCommandById(id) → boolean`, `listCommands()`. Hotkeys bind **by command id**, so overwriting `findCommand(id).callback` at runtime works.
- **Populated by:** `App.registerCommands` (Section 6), `Workspace.registerCommands` (`workspace.md` §6), `MarkdownView.registerCommands` (`editor-markdown.md` §6), every internal plugin, every community plugin via `Plugin.addCommand`.
- **Read by:** `HotkeyManager`, `command-palette` internal plugin, `setting` (hotkeys tab).
- **Persistence:** in-memory (user overrides via `.obsidian/hotkeys.json`, owned by `hotkeyManager`).
- **Lifecycle:** populated after `hotkeyManager.load()` (`App.js:602`), then by each plugin's `onload`. Dies with the process.

### `InternalPlugins` (`app.internalPlugins`, class `o2`)

**Canonical Obsidian built-in-feature registry.** Loaded at `App.js:611-641` — 31 `loadPlugin(new Ctor())` calls (30 always + 1 desktop-only). Each ctor's `manifest.id` lives in its own (unextracted) class; ids are not literal in `App.js`.

IDs confirmed in-tree: `"daily-notes"` (`:775,3096`), `"sync"` (`:1106`), `"properties"` (`:1336`), `"bases"` (`plugin/Plugin.js:172`), `"global-search"` (`rendering.md` §1). Remaining 25 (from published `.obsidian/core-plugins.json` schema): `"file-explorer"`, `"canvas"`, `"backlink"`, `"outline"`, `"graph"`, `"outgoing-link"`, `"tag-pane"`, `"switcher"`, `"command-palette"`, `"bookmarks"`, `"templates"`, `"note-composer"`, `"random-note"`, `"slash-command"`, `"unique-note"`, `"workspaces"`, `"word-count"`, `"zk-prefixer"`, `"page-preview"`, `"editor-status"`, `"audio-recorder"`, `"markdown-importer"`, `"slides"`, `"publish"` (desktop-only, `M2` at `:641`).

**Canonical feature-matrix input for Corbomite.** Pass 3 `FEATURE-MATRIX.md` must enumerate. `.obsidian/core-plugins.json` stores `Record<id, boolean>`.

**API:** `loadPlugin`, `enable`, `disable`, `getPluginById`, `getEnabledPluginById`, `setPluginEnabled`, `plugins: Record<id, Plugin>`. Closed registry — third-party plugins cannot add.

### `Scope` (root hotkey scope)

`app.scope` **is** `Keymap.global.rootScope` — a `Scope` instance, not a registry. Listed here because it's the anchor of the hotkey-registration graph. `hotkeyManager` registers hotkeys against `app.scope` (root) and individual views/modals register against their own child scopes. See `editor.md` §15 (`EditorSuggest.scope`), `editor-markdown.md` §15, `bases.md` §1 (`BasesView.scope`).

### Not registries, but same-shape internal maps

- `App.nextFrameEvents: Array<fn>` — per-frame scheduler queue. Drained once per `requestAnimationFrame`.
- `Events._: Record<eventname, EventRef[]>` — the listener-map storage inside every `Events` subclass. Not public.
- `Scope.keys: KeyHandler[]` — the per-scope handler list. Mutated via `register`/`unregister`.

---

## 8. Invariants

- **`App.on` is a no-op** (`App.js:3254`). `app.on(name, cb)` returns `undefined` and registers nothing. Workspace-wide events go on `app.workspace`.
- **`Events.tryTrigger` swallows exceptions synchronously but re-throws on the next tick** (`setTimeout(…, 0)`). One listener throwing does not abort dispatch; the error appears in console with no emitter-stack correlation.
- **`Events.off(name, fn)` is identity-match**. Two plugins sharing a `fn` reference both get removed. Use `offref(ref)` / `Component.registerEvent(ref)` instead.
- **`Events.trigger` iterates a snapshot** (`.slice()`, `:47`). A listener can self-`off` without skipping subsequent listeners.
- **`Scope.handleKey` bubbles to parent only for catch-all handlers returning `undefined`** (`key===null && modifiers===null`). Non-catch-all returning `undefined` **stops the walk** — easy to miss.
- **`Scope` modifier strings are normalised.** `"Mod"` → `"Meta"` on macOS, `"Ctrl"` elsewhere; array sorted, comma-joined. Call `Keymap.compileModifiers` before storing.
- **`appId` is injected by the bootstrapper**; stable for process lifetime. Keys localStorage, IndexedDB (`<appId>-cache`, `<appId>-sync`), webview partition (`"persist:vault-<appId>"` at `:2749`), config-dir override lookup (`getOverrideConfigDir`).
- **`openVaultChooser()` is process-death on desktop** (`ipcRenderer.sendSync("starter") + window.close()`); mobile is `location.reload()`. No in-process swap. `onunload` hooks run via `workspace.trigger("quit")` before window close, **not** on vault-switch path.
- **`registerQuitHook` owns `window.onbeforeunload` exclusively** (`App.js:3213`, sets `onbeforeunload = null` on entry for self-idempotence). Plugins must not override.
- **`Eb` quit-tasks collector.** Plugins with async cleanup push into the payload via `workspace.on("quit", (tasks) => tasks.add(promise))` — don't block `onbeforeunload` directly.
- **`Keymap.global` is a per-window singleton.** All `keydown` events route through one instance. Corbomite's `KActionCollection` is per-action-context and doesn't stack — structural gap (Section 11).
- **Multiple `Scope.register` handlers with identical `(modifiers, key)` coexist.** No shadow/replace; walk is registration-order, first non-undefined result wins.

---

## 9. Observable user features

- Command palette (`Mod+P`) surfaces every registered command — all Section 6 entries.
- Hotkeys modal overrides any command's hotkey; persists to `.obsidian/hotkeys.json`.
- Vault switcher (`app:switch-vault`, `app:open-another-vault`) opens another vault; current window closes.
- Light/dark toggle (`theme:toggle-light-dark`); respects `theme === "system"` → OS-relative.
- Reactive appearance settings: accent color, font family/size, tab size, ribbon/inline-title/header visibility, readable line length, line numbers, native menus — all persist to `.obsidian/app.json`/`appearance.json`; `onConfigChanged` loops back and re-renders.
- F1 → help vault (desktop) or `help.obsidian.md` (web).
- Debug mode (`app.debugMode(true)` in console) — reloads with `"DebugMode"` in localStorage.
- Emulate mobile (`app.emulateMobile(true)`) — desktop-only; reloads with `"EmulateMobile"` flag; `Platform.isMobile` becomes `true`.
- Release notes shown once per version bump (`localStorage["most-recently-installed-version"]`).
- Slow-startup diagnostic if boot > 8s and `slow-startup-check === "1"`.
- Every view/menu/modal/suggester "Escape to close", "Enter to confirm" pattern powered by `Scope.register` + `Rg`-delegation.

---

## 10. Extension surfaces exposed

| Surface | Registration verb | Consumer | What plugins supply |
|---|---|---|---|
| Any event on any `Events` subclass | `emitter.on(name, cb, ctx?)` / `Component.registerEvent(...)` | `Events.trigger` (`core/Events.js:42`) | `(…args) => void` |
| Global hotkey | `Plugin.addCommand({id, name, hotkeys, callback})` | `HotkeyManager` → `commands.executeCommandById` | `Command` (Section 7) |
| View-scoped hotkey | `view.scope.register(mods, key, fn)` / `modal.scope.register(...)` | `Scope.handleKey` | `(ev, ctx) => false \| undefined` |
| Custom top-level scope | `new Scope(parent)` + `app.keymap.{push,pop}Scope(s)` | `Keymap.onKeyEvent` → `scope.handleKey` | a `Scope` |
| Next-frame deferral | `app.nextFrame(cb)` | `App.onNextFrame` | `() => void` |

`App` itself exposes no `register*` verbs. Child registries (`commands`, `viewRegistry`, `embedRegistry`, `hotkeyManager`, `renderContext`, `customCss`, `metadataTypeManager`, `fileManager`, `dragManager`, `internalPlugins`, `plugins`, `foldManager`, `setting`) are the plugin-facing surfaces — see respective domain docs.

---

## 11. Corbomite mapping

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `App` (DI container) | `MainWindow` + `VaultService` + `SessionManager` + `CorbomiteApp` | Partial | No god object. Qt/KDE idiom is constructor-injection + `QObject` parent ownership, not a central registry. Pass 3 should **not** recommend cloning `App`. |
| `App.appId` | None | Missing | No per-vault process identifier. Required if we add per-vault caches/webview partitions/config-dir overrides. Derive from vault path hash. |
| `App.vault`/`workspace`/`metadataCache`/`fileManager` | `VaultService::vault()`, `noteService()`; workspace absent | Partial | See `vault.md`/`workspace.md` §11. |
| `App.commands` | `KActionCollection` + `KStandardAction` | Partial | KDE actions bind one hotkey per action; compat with Obsidian's one-cmd-many-hotkeys + `.obsidian/hotkeys.json` overrides needs a `CommandRegistry` wrapper. |
| `App.scope` / `Scope` stack | Qt focus chain | **Missing** | KDE actions don't stack like `Scope.parent`. Need a `Scope` shim via `QApplication::installEventFilter` walking the active-scope stack. **Flag:** prototype `include/core/Scope.h` before suggester/modal plugins. |
| `App.keymap` | `QApplication` key events + `Qt::KeyboardModifier` + `QKeySequence` | Partial | `"Mod"` ↔ `Qt::ControlModifier` on Linux/Win, `Qt::MetaModifier` on macOS. |
| `App.hotkeyManager` | `KConfigGroup("Shortcuts")` + `KActionCollection::readSettings` | Partial | `.obsidian/hotkeys.json` compat is the work. |
| `App.dragManager` / `viewRegistry` / `embedRegistry` / `renderContext` / `foldManager` / `plugins` / `internalPlugins` / `setting` | mostly absent | Missing | See domain-specific §11 (rendering/views/editor/settings). |
| `App.customCss` | Qt stylesheet + `QFileSystemWatcher` on `.obsidian/snippets/` | Partial | Implementable. |
| `App.secretStorage` | `KWallet` / `QtKeychain` | Partial | Audit `secrets/`. |
| `App.nextFrame(cb)` | `QTimer::singleShot(0, cb)` coalescer | Partial | No Qt built-in; implement on top of `QTimer`. |
| `App.openVaultChooser` | `MainWindow::openVaultDialog()` + `WelcomeScreen` | Present | **Bug target.** `MainWindow::closeVault` does in-process teardown, unlike Obsidian's window-close-and-respawn. Either (a) destroy `MainWindow` and rebuild for the new vault (Kate-session pattern, recommended), or (b) re-construct every service topologically. |
| `registerQuitHook` / `workspace.trigger("quit", tasks)` | `MainWindow::closeEvent` + `QCloseEvent::ignore()` | Partial | `Eb` collector → `QList<QFuture<void>>`. |
| `Events` mixin | Qt signals/slots | **Partial with gap** | Qt signals are statically typed; `Events.trigger(name, …)` is dynamic — plugins invent names. Need either (i) a `QVariantList` dispatch layer or (ii) compile-time signals + dynamic fallback. Option (i) is simpler; loses static type-check on listener args. |
| `Events.offref` | `QObject::disconnect(conn)` | Partial | `QMetaObject::Connection` ↔ `EventRef`. |
| `Events.tryTrigger` async-rethrow | — | **Mismatch** | Qt propagates synchronously; compat requires per-listener `try/catch` + `QTimer::singleShot(0, [] { throw; })`. |

**Vault-switching bug:** Obsidian sidesteps it by destroying the window. Corbomite keeps the window; per-vault children hold stale references. Simplest compat-aligned fix: destroy `MainWindow` on vault switch and let `CorbomiteApp` construct a fresh one. Kate session pattern (`memory/reference_kate_sessions.md`).

---

## 12. Markoff gap confirmations / discoveries

`N/A — no editor/rendering surface in this domain.`

The `App` class orchestrates theme/font/CSS mutations but does not render markdown. `css-change` is emitted on `workspace` and **consumed by** Markoff-relevant code in `rendering/` and `editor/markdown/` (see those docs' §5 / §12). `App` is emit-only.

---

## 13. Open questions

1. **`Eb` (quit-tasks collector) shape.** `new Eb()` at `:3218`; `.isEmpty()` / `.promise()` inferred. Pass 3 grep `Eb = (function` in full `app.js`.
2. **`wte` (DOM layout wrapper).** `new wte(document.body)` at `:35`; exposes `appContainerEl`/`workspaceEl`/`statusBarEl`/`horizontalMainContainerEl`. Source unknown.
3. **`Y6` (Commands class) method surface.** Not extracted. Shape (Section 7) inferred from call sites. Pass 3 should extract `Y6 = (function` to a `commands/Commands.js` file.
4. **`zb` (HotkeyManager) method surface.** Not extracted; `.load()`, `.registerListeners()` observed.
5. **`o2` (InternalPlugins) full API.** `getPluginById`, `getEnabledPluginById`, `loadPlugin`, `.enable()` observed; `setPluginEnabled`, `.plugins` map inferred.
6. **Internal-plugin id list by ctor.** The 31 ctors (`P8`, `dJ`, `g9`, `bJ`, `V6`, `i8`, `r7`, `dee`, `q8`, `k7`, `f7`, `v8`, `wee`, `Z8`, `u8`, `k9`, `b8`, `l3`, `U8`, `See`, `w9`, `p7`, `Pee`, `S9`, `a4`, `Iee`, `O8`, `f9`, `uee`, `Z4`, `M2`) at `:611-641`. Pass 3 must grep each ctor's `manifest.id`. **Single most valuable feature-matrix input** from this domain.
7. **How is `appId` generated?** Derived from vault path by an outer bootstrapper; not in `core/`. Pass 3 locate (Electron main-process bundle or mobile flow).
8. **`Rg` forward-or-fallback?** Confirmed from `:47-52`: if `cb()` returns a scope, delegate; else super. Forward-OR-fallback, not forward-THEN-fallback. (Closes a `workspace.md` OQ of the same shape.)
9. **Is `workspace.trigger("quit")` idempotent under navigation-cancel?** `:3214` sets `onbeforeunload = null` on entry — self-idempotent. Plugin listeners need no guard.

---

## 14. Recommended Pass 3 synthesis input

1. **Internal-plugins list is the canonical Obsidian feature enumeration.** Section 7 names the 31 ids (most known, a few inferred) — Pass 3 `FEATURE-MATRIX.md` must start with this list as the row axis. **Before Pass 3 writes anything, extract the missing manifest.ids from the minified ctors.**
2. **Vault-switch process-death model.** Obsidian never swaps a vault in-process. Corbomite's vault-switch crash bug (memory note) is unambiguously the gap from trying to do what Obsidian refuses to do. Pass 3 `GAP-ANALYSIS.md` should promote "destroy `MainWindow` on vault switch" (Kate-session pattern) as the compat-aligned fix, not "reconstruct all services in topological order".
3. **`App.on` is a no-op; `Scope` stack has no Qt equivalent.** Two structural facts that must be called out in the plugin-API spec before Corbomite exposes any plugin hooks. The `Scope` gap is the bigger one — Corbomite needs a `QObject`-based `Scope` shim (probably via `QApplication::installEventFilter`) to support `Modal` / `Menu` / suggesters faithfully. Flag as gating for any future plugin SDK.

---

## 15. Cross-domain references

| Other domain | Reference type | Brief description |
|---|---|---|
| `vault` | owner ← subscriber | `App.vault = new Vault(adapter)` (`:516`). `App` listens to `vault.on("closed")` (`:531`) and `vault.on("config-changed")` (`:534`); `registerCommands` calls `vault.setConfig` for each togglable. `vault.md` §4. |
| `workspace` | owner + emitter | `App.workspace = new Workspace(app, dom.workspaceEl)` (`:545`). `App` emits **`css-change` and `quit` on workspace** (`:1293,2808,2883,2896,3219`). Workspace consumes `App.commands`, `keymap`, `scope`, `viewRegistry`, `hotkeyManager`, `setting`, `dragManager`, `internalPlugins`, `plugins.loadingPluginId`, + helper methods. `workspace.md` §15. |
| `metadata` | owner | `App.metadataCache = new MetadataCache(app, vault)` (`:549`); `metadataTypeManager = new RL(app)` (`:550`). `appId` keys IndexedDB (`<appId>-cache`). `metadata.md` §3. |
| `views` | owner | `App.viewRegistry = new ViewRegistry()` (`:12`). `views.md` §15. |
| `rendering` | owner | `renderContext = new RenderContext(app)` (`:38`), `embedRegistry = new aJ()` (`:11`). `rendering.md` §15. |
| `editor` / `editor-markdown` | consumer | `EditorSuggest.scope`, `MarkdownView.scope` are children of `app.scope`. Access `app.foldManager`, `renderContext`, `vault.getConfig("livePreview")`. `editor.md`/`editor-markdown.md` §15. |
| `bases` | consumer | `BasesView.scope` child of `app.scope`; reads `workspace`, `metadataCache`, `renderContext`, `fileManager`, `internalPlugins.getEnabledPluginById("bases")`. `bases.md` §15. |
| `platform` | **duplicate-extraction sibling** + consumer | `core/Scope.js` and `platform/Keymap.js` are byte-identical except for the header comment — same `app.js:60149-60366` range. `Scope`/`Rg` audited here; `Keymap` audited in platform. `app.keymap = Keymap.init()` (`:30`). |
| `plugin` | consumer / owner | App constructs `commands` (`Y6`), `hotkeyManager` (`zb`), `dragManager` (`xP`), `customCss` (`Ib`), `plugins` (`$0`), `internalPlugins` (`o2`). Plugin base imports `Events`, `Scope`, reaches `app.*` via `this.app`. |
| `secrets` | owner | `secretStorage = new SecretStorage(app)` (`:39`). |
| `ui/popups` | consumer | `Modal`/`Menu` each allocate `new Scope(app.scope)`; `app.setting` is a `Modal` subclass. |
| `settings` | owner | `app.setting = new vte(app)` (`:551`). |
| `utils` | consumer | `cD` boot-timing, `gm.*` i18n, `Fb`, `normalizePath`, `debounce`, `y`/`b` async-generator runtime, `m` inherit helper. |

### Short-symbol cross-reference table

| Short symbol | Defined in | Used here for |
|---|---|---|
| `Y6` | commands (unextracted) | `App.commands`. |
| `zb` | plugin (unextracted) | `App.hotkeyManager`. |
| `xP` | plugin (unextracted) | `App.dragManager`. |
| `aJ` | rendering (unextracted) | `App.embedRegistry`. |
| `Ib` | plugin (unextracted) | `App.customCss`. |
| `vte` | settings (unextracted) | `App.setting`. |
| `RL` | metadata (unextracted) | `App.metadataTypeManager`. |
| `Q6` | editor (unextracted) | `App.foldManager`. |
| `o2`, `$0` | plugin (unextracted) | `App.internalPlugins`, `App.plugins`. |
| `Eb` | utils/ui (unextracted) | Quit-tasks collector; `new Eb()` at `:3218`. |
| `wte`, `Nee` | ui (unextracted) | `App.dom`, `App.statusBar`. |
| `t4`, `CA` | platform (unextracted) | `App.shareReceiver`, `App.cli`. |
| `Keymap` | `platform/Keymap.js` (canonical) | `app.keymap`; `Keymap.init`, `compileModifiers`, `isMatch`, `global.rootScope`. |
| `Rg` | `core/Scope.js` (local) | `Scope` subclass with dynamic child-delegate; used by `Workspace.scope`. |
| `pD` | ui/progress (unextracted; static singleton) | `pD.instance` is the load-progress UI; **not** `app.loadProgress`. |
| `gm` | i18n (unextracted) | Translation tree. |
| `Fb` | `platform/Keymap.js` | Hotkey-binding constructor `(modifiers, key) => KeyBinding`. |
