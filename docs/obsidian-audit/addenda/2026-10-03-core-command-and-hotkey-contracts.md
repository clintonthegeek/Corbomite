# Core: `App.registerCommands` is 115 commands, `hotkeys.json` is an override-only map, and opening a `Modal` cuts the global hotkey chain

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/core.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/core.md` §1 (`App` method groups, `setTheme`), §2 (`KeyHandler`, vault-switch), §3 (localStorage keys), §4 (`css-change` / `quit`), §5 (`quit` payload), §6 (**entire command table**), §7 (`Command` interface, `Commands` API, `InternalPlugins` API and id list), §8 (hotkey-scope invariants), §9 (`F1` help target), §10 (`nextFrame`, command surface), §11 (`App.commands` row), §13 (**Q1, Q2, Q3, Q4, Q5, Q6 answered; Q7 refuted as unanswerable from this corpus**), §14 (items 1 and 3), §15 (all short symbols; the `Modal`/`Menu` scope claim)
**Relevant cluster plans:** `superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md` (closed — the `editor:*` command inventory and the four undocumented Command gate fields are the missing half of O3's context-sensitivity work), `superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md` (the `quit` chain and the `requestSaveLayout.cancel()` finding).

**Corpus:** `RE/obsidian/1.13.7/renamed/src/<bundle>/own/*.js`. Citations are
`<bundle>/<rel>:<line>`. `domains/core.md` cites `core/App.js:<line>`, `core/Events.js:<line>`
and `core/Scope.js:<line>` throughout; **every one of those citations is dead** — those three
extractions no longer exist. Remap table at §A.

> **Doctrine reminder.** Corbomite deliberately does **not** mirror the JS plugin API. What
> follows is the transferable part: the command-id inventory (which *is* a compat contract,
> because `.obsidian/hotkeys.json` is keyed by command id), the `hotkeys.json` read/write
> semantics, the `Scope` chain's effect on shortcut dispatch, and the on-disk theme/snippet
> layout. Callback plumbing is quoted only where it changes an observable rule.

---

## Finding

### §A. Citation remap — the whole domain is uncitable as written

| `core.md` symbol | Doc's cite | Live location (1.13.7) |
|---|---|---|
| `App` (class, `zie`) | `core/App.js:1-3308` | `app/own/2075-Vie.js:4-2069` |
| `App.prototype.on` no-op stub | `App.js:3254` | `app/own/2075-Vie.js:2042` |
| `Events` (`FD`) | `core/Events.js:1-70` | `app/own/1126-FD.js:2-50` |
| `Scope` (`ly`) / `Rg` (`cy`) | `core/Scope.js:1-223` | `app/own/0747-ly.js:2-29` / `:30-39` |
| `Keymap` (`uy`) | `platform/Keymap.js` | `app/own/0747-ly.js:40-139` |
| `Commands` (`Y6` → `s5`) | "unextracted" | `app/own/1963-s5.js:2-48` |
| `HotkeyManager` (`zb` → `ek`) | "unextracted" | `app/own/0833-$w.js:5-89` |
| `InternalPlugins` (`o2` → `M4`) | "unextracted" | `app/own/1939-m4.js:693-763` |
| `internalPluginWrapper` (`C4`) | "unextracted" | `app/own/1939-m4.js:545-691` |
| `FoldManager` (`Q6` → `l5`) | "unextracted" | `app/own/1963-s5.js:49-70` |
| `MobileNavbar` / `MobileTabSwitcher` / `MobileToolbar` / `QuickActions` | "unextracted" | `app/own/1963-s5.js:71-190` / `:234-465` / `:466-552` / `:553-587` |
| `ShareReceiver` (`t4` → `f5`) | "unextracted" | `app/own/1963-s5.js:588-733` |
| `AudioRecorderPlugin` (`b5`) | — | `app/own/1963-s5.js:785-936` |
| `customCss` (`Ib` → `Kw`) | "unextracted" | `app/own/0829-Ww.js:21-462` |
| `Notice` (`Aw`) | — | `app/own/0819-ww.js:241-330` |
| `Suggester` nav helper (`ww`) | — | `app/own/0819-ww.js:2-240` |
| `Modal` (`fw`) | — | `app/own/0817-Yb.js:627-…` |
| `Menu` (`my`) | — | `app/own/0751-my.js:2-…` |
| `SuggestModal` (`TR`) | — | `app/own/1319-xR.js:26-…` |
| `DOM layout wrapper` (`wte` → `Oie`) | "source unknown" (Q2) | `app/own/2069-Oie.js:2-4` |
| `quit-tasks collector` (`Eb` → `Bw`) | "unextracted" (Q1) | `app/own/0826-Bw.js:2-15` |
| `KeyBinding ctor` (`Fb` → `Yw`) | `platform/Keymap.js` | `app/own/0830-Yw.js:2-6` |
| `hotkey display formatter` (`Qw`) | — | `app/own/0832-Qw.js:2-13` |
| `KeyboardEvent.code` normaliser (`Xw`) | — | `app/own/0831-Xw.js:2-4` |
| modifier display labels/order (`ay`/`sy`) | — | `app/own/0746-ay.js:2-15` |
| i18n tree (`gm` → `bd`) | "unextracted" | used throughout, e.g. `app/own/2075-Vie.js:614` |
| platform predicates (`Zp`=mac, `Jp`=win, `ed`=linux, `nd`=android) | — | `app/own/0519-Qp.js:2-16` |
| build feature flags (`Ab`) | — | `app/own/0803-Ab.js:2-10` |
| theme manifest filenames (`zw`/`Hw`) | — | `app/own/0828-Hw.js:2-3` |
| extension families (`hb`/`pb`/`db`/`fb`/`vb`/`mb`/`gb`/`yb`/`bb`/`wb`) | — | `app/own/0794-hb.js:2-11` |

**NEW — the `Scope` + `Keymap` pair is triplicated, not extracted once.** The same
`ly`/`cy`/`uy` trio appears at `app/own/0747-ly.js:2-139`, `help/own/0080-Ga.js` and
`starter/own/0116-qe.js`. The starter copy is the **vault-chooser window's own `Keymap`**.
That is why `windowScopes` is a `WeakMap` keyed by window rather than a single stack
(`app/own/0747-ly.js:48-53`). `core.md`'s "duplicate-extraction sibling" note (§19 preamble,
§15) needs to become "three live copies"; when porting, note that **the chooser and help
windows each have a separate root scope**, so a Corbomite vault-chooser dialog must not
share the main window's action-shortcut context.

### §B. HEADLINE CORRECTION — `App.registerCommands` registers **115** command ids, not "30+", and ~2/3 of them are `editor:*`

`core.md` §6 says "registers 30+ commands" and its table lists 25 rows, none of them
`editor:*` except two toggles. The real body is `app/own/2075-Vie.js:609-1709` and contains
**96 `addCommand` call sites → 94 literal ids + 2 templated ids expanding to 6 and 15 = 115
distinct ids.** Measured, not estimated:

- 94 literal ids
- `editor:set-heading-1` … `editor:set-heading-6` from `id: "editor:set-heading-".concat(e)`
  with `s = [1,2,3,4,5,6]` (`:1152-1163`)
- 15 `editor:table-*` ids from the `l()` factory at `:1532-1545` with 15 call sites
  (`:1546-1618`): `row-before`, `row-after`, `row-up`, `row-down`, `row-copy`, `row-delete`,
  `col-before`, `col-after`, `col-left`, `col-right`, `col-copy`, `col-delete`,
  `col-align-left`, `col-align-center`, `col-align-right`

Per-platform split: 7 desktop-only (`app:open-another-vault` `:642`, `app:open-sandbox-vault`
+ 4 `window:*` `:841-874`, `open-with-default-app:show` `:963`), 1 mobile-only
(`app:show-tab-switcher` `:649`, plus 10 further `mobileOnly: !0` specs), 3 mobile-app-only
(`editor:cut|copy|paste`, `:1619-1667`). Desktop therefore sees **104**, mobile app **108**.

This matters because **`.obsidian/hotkeys.json` is keyed by command id** (§D). A Corbomite
that implements only the 25 documented ids will silently drop every `editor:*` override in a
real vault, and the palette will be missing two thirds of Obsidian's built-in entries.

Complete default-hotkey table (every `hotkeys:` field in `:609-1709`, verbatim
`Yw(modifiers, key)` calls = `{modifiers, key}`):

| Command id | Default | Line |
|---|---|---|
| `app:go-back` | `Mod+Alt` `ArrowLeft` | 620 |
| `app:go-forward` | `Mod+Alt` `ArrowRight` | 629 |
| `app:open-settings` | `Mod` `,` | 684 |
| `markdown:toggle-preview` | `Mod` `E` | 699 |
| `markdown:add-metadata-property` | `Mod` `;` | 726 |
| `app:open-help` | (none) `F1` | 827 |
| `file-explorer:new-file` | `Mod` `N` | 882 |
| `file-explorer:new-file-in-new-pane` | `Mod+Shift` `N` | 897 |
| `editor:open-search` | `Mod` `F` | 1011 |
| `editor:open-search-replace` | **mac** `Mod+Alt` `F` / else `Mod` `H` | 1020 |
| `editor:insert-link` | `Mod` `K` | 1105 |
| `editor:toggle-bold` | `Mod` `B` | 1171 |
| `editor:toggle-italics` | `Mod` `I` | 1179 |
| `editor:toggle-comments` | `Mod` `/` | 1222 |
| `editor:toggle-checklist-status` | `Mod` `l` | 1251 |
| `editor:delete-paragraph` | `Mod` `D` | 1472 |

`editor:open-search-replace` is the **only platform-conditional default hotkey in the whole
set** (`Zp ? ... : ...`, `Zp = "mac" === Qp` at `app/own/0519-Qp.js:10`). A faithful
`hotkeys.json` reader must not assume one default per id.

### §C. CORRECTION — the `Command` spec has four undocumented gate fields, and `Commands` is `Events`-mixed with a second registry

`core.md` §7's `interface Command` lists `id`, `name`, `icon`, `hotkeys`, `mobileOnly` and
four callback shapes. Real usage adds five more fields, all consumed by
`app/own/1963-s5.js:7-22`:

| Field | Set at | Effect |
|---|---|---|
| `allowPreview: !0` | `:1045`, `:1053`, `:1062` | command stays enabled in reading view (default: disabled) |
| `allowProperties: !0` | `:1045`, `:1053`, `:1062` | command stays enabled while focus is inside `.metadata-container` (default: disabled) |
| `showOnMobileToolbar: !0` | any | force-include in `editorCommands` even without an editor callback (`:22`) |
| `repeatable: !0` | `:1316`, `:1324`, `:1465`, `:1474`, `:1500` | fire on key auto-repeat (see §D) |
| `mobileOnly: !0` | `:1332`–`:1407` | **dropped entirely** off-mobile: `e.mobileOnly && !rd.isMobile \|\| (…)` (`:9`) |

`Commands` (`s5`) **extends `FD` (`Events`)** (`app/own/1963-s5.js:2-6`, `:48`) and fires
`trigger("changed")` on every `addCommand` and every `removeCommand` (`:22`, `:24`). So
`app.commands.on("changed", …)` is a live subscription — the palette and the hotkey modal
both use it. `core.md` §7 lists neither the inheritance nor the event.

Two registries, not one (`app/own/1963-s5.js:5`):

```js
n.commands = {}, n.editorCommands = {}
```

`editorCommands[id]` is set for any command with an `editorCallback`/`editorCheckCallback`
**or** `showOnMobileToolbar` (`:9-22`). It is the mobile toolbar's and the mobile
command-palette's index; it is *not* a filtered view of `commands`.

**`addCommand` synthesises the runnable check.** For an editor command it installs its own
`checkCallback` (`:9-21`) which returns falsy when there is no `activeEditor`; returns
`undefined` when `allowPreview` is unset and the mode is `"preview"`; and — for a
`MarkdownView` — refuses when the inline title or the file title has focus, or when
`activeDocument.activeElement.closest(".metadata-container")` matches without
`allowProperties`. Only then does it delegate. A Corbomite action-collection analogue needs
the same three refusals or `editor:*` actions will be wrongly enabled in reading view and
over the properties widget.

Other behaviours `core.md` §7 does not record:

- **No duplicate-id check.** `this.commands[e.id] = e` (`:22`) silently overwrites.
- `removeCommand(id)` also calls `hotkeyManager.removeDefaultHotkeys(id)` (`:24`).
- `listCommands()` **calls `checkCallback(true)` on every command** and filters on truthiness
  (`:28-35`), so the palette shows only currently-valid commands. A throw is logged as
  `"Command failed to execute: "` + id and treated as not-visible (`:33`) — note the message
  says *execute* although it is a *check*.
- `executeCommand(cmd, ev)` sets `app.lastEvent = ev || null` before dispatch (`:40`).
- `findCommand`/`listCommands`/`executeCommandById` confirmed as documented (`:25`, `:27`, `:36`).

### §D. NEW — `.obsidian/hotkeys.json` is an **override-only** map, and the dispatcher is a single root-scope catch-all

`core.md` §3 delegates `hotkeys.json` to "the plugin domain" and never describes it. Since it
is a named compat contract, here it is in full. `HotkeyManager` = `ek`,
`app/own/0833-$w.js:5-89`.

**Dispatch is one handler, not per-command registrations** (`:7`):

```js
this.app.scope.register(null, null, this.onTrigger.bind(this))
```

The manager registers a single `(null, null)` catch-all on the **root** scope at construction
and matches its own baked table on every `keydown`. There are no per-command `Scope`
registrations anywhere. `core.md` §8's "hotkeyManager registers hotkeys against `app.scope`
(root)" is right, but §1/§8's framing of `Scope.register` as the hotkey mechanism is not: the
scope stack only ever sees **two** handlers in practice (this one, plus whatever a pushed
view/modal/suggester scope registers).

**`bake()` precedence — presence-with-empty-array disables the default** (`:57-76`):

```js
for (var o in t) t.hasOwnProperty(o) && r(o, t[o]);                      // custom first
for (var o in e) e.hasOwnProperty(o) && !t.hasOwnProperty(o) && r(o, e[o]);  // defaults only where absent
```

`hotkeys.json` is therefore `Record<commandId, KeyBinding[]>`, and:

- an id **absent** → defaults apply;
- an id present with a non-empty array → **only** the custom bindings apply;
- an id present with **`[]`** → **no hotkey at all**, and the default is *not* restored.

That third case is the one a naive reader gets wrong, and it is the only way Obsidian
expresses "unbind this command".

**A binding is `{modifiers: string[], key: string}`** (`Yw`, `app/own/0830-Yw.js:2-6`) and
may additionally carry **`code`** (a DOM `KeyboardEvent.code`). `bake()` prefers it
(`key: a.code ? Xw(a.code) : a.key`, `:69`); `Xw` only strips the `Key` prefix off a 4-char
`KeyX` code (`app/own/0831-Xw.js:2-4`). This is the 2026-06-10 addendum's "`code` field"
note, now with a mechanism and a reason:

**NEW — the `code` field exists for the macOS Ctrl+Option+I case.** `onKeyEvent` rewrites the
vkey when the platform reports the caret character instead of the letter
(`app/own/0747-ly.js:83`):

```js
54 === t.which && "^" == t.key && "KeyI" === t.code && (o = "KeyI")
```

so a mac-authored `hotkeys.json` override for `editor:toggle-italics` can say
`{"modifiers":["Mod"],"code":"KeyI"}` and still match Ctrl+Option+I. A Corbomite
`QKeySequence`-based reader **cannot** express this; it needs the raw binding stored and
matched.

**`save()` writes overrides only** — `writeConfigJson("hotkeys", this[Jw])` (`:24`) — never the
defaults. So a faithful writer must not merge `addDefaultHotkeys`' table into the file, or
Obsidian will start treating every default as a user override. `load()` (`:30-41`) accepts the
parsed JSON only when `typeof === "object"`; a non-object (array, string, number) is **ignored
silently**, and `load()` never validates contents or writes back.

**Live external-edit reload**: `registerListeners()` attaches `vault.on("raw")` (`:15-18`) and
reloads on `path === configDir + "/hotkeys.json"` through
`onConfigFileChange = Ml(this.load, 50)` (`:9`) — a **50 ms leading-edge throttle**, per the
cross-domain `Ml` finding. `registerListeners()` is called *after* `vault.load()`
(`app/own/2075-Vie.js:446`), while `load()` runs earlier (`:319`), so external edits are not
watched during boot.

**`repeatable` gates key auto-repeat** (`:84`):

```js
if (s && (!e.repeat || s.repeatable))
  if (this.app.commands.executeCommand(s)) return !1;
```

So a command without `repeatable: true` fires once per physical press even when the key is
held, and `onTrigger` returns `false` (consume) only when the command's own dispatch succeeded.

**Display format** — `Qw(binding)` (`app/own/0832-Qw.js:2-13`) renders **only the first**
binding (`printHotkeyForCommand`, `:54-56`, returns `""` when there is none). Modifier labels
and order are `ay`/`sy` (`app/own/0746-ay.js:2-15`): order `Mod, Ctrl, Meta, Alt, Shift`;
labels on macOS `⌘ ⌃ ⌘ ⌥ ⇧`, elsewhere `Ctrl / Ctrl / Win / Alt / Shift`; joined with `" "` on
macOS and `" + "` elsewhere; the key is title-cased with a space before each capital
(`ArrowLeft` → `Arrow Left`, `PageDown` → `Page Down`).

### §E. `Modal` allocates a **parentless** `Scope`, and pushing it cuts the global hotkey chain — CORRECTION to §15

`core.md` §15 (`ui/popups` row) says "`Modal`/`Menu` each allocate `new Scope(app.scope)`".
Only `Menu` does:

| Class | Scope | Cite |
|---|---|---|
| `Modal` (`fw`) | `new ly()` — **no parent** | `app/own/0817-Yb.js:630` |
| `Menu` (`my`) | `new ly(uy.global.getRootScope())` | `app/own/0751-my.js:8` |
| `SuggestModal` (`TR`) | `new ly(t \|\| e.scope)` | `app/own/1319-xR.js:29` |
| `EditorSuggest` base | `new ly(app.scope)` | `app/own/1576-Yj.js:83` |
| Page-preview search (`Xq`) | `new ly(e)` (passed parent) | `app/own/1481-_q.js:555` |
| Bases property menu (`U5`) | `new ly(a.scope)` | `app/own/1969-L5.js:271` |

`Modal.open()` does `e.keymap.pushScope(this.scope)` (`app/own/0817-Yb.js:709`), and `Scope`
has **no mechanism other than the constructor to acquire a parent** — there is no
`scope.parent =` assignment anywhere in the app bundle. `handleKey` ends with
`if (this.parent) return this.parent.handleKey(e, t)`
(`app/own/0747-ly.js:27`), so a parentless scope simply stops.

**Consequence: while any plain `Modal` is open, the root scope is never consulted, and since
the root scope is where `HotkeyManager` registers its single catch-all (§D), no
`.obsidian/hotkeys.json` binding fires.** Only the modal's own `register([], "Escape", …)`
(`app/own/0817-Yb.js:631`) and anything its subclass adds. This is why `SuggestModal`
deliberately *replaces* `this.scope` with a parented one — a suggester must keep hotkeys
alive.

Corbomite translation: **any `QDialog`-based modal must run in its own shortcut context with
`Qt::WidgetShortcutContext::WidgetShortcutContext`**, and it must not inherit the main
window's action shortcuts; a suggester popup must. `core.md` §11's "need a `Scope` shim via
`QApplication::installEventFilter`" is the right instinct, but the modal-cuts-hotkeys rule is
the behavioural half it omits.

`Menu` also copies a **static** `useNativeMenu` onto the instance at construction
(`app/own/0751-my.js:8`, set by `App.updateUseNativeMenu` at `app/own/2075-Vie.js:1799-1801`),
so the native-menu switch only reaches menus built *after* the change.

### §F. `Events.off(name, fn)` deletes **all** listeners for `name` when the filter comes up empty — NEW

`core.md` §1/§8 say `off(name, fn)` "removes every listener where `listener.fn === fn`
(identity)". That is true, but it omits the hazard. `app/own/1126-FD.js:17-23`:

```js
e.prototype.off = function(e, t) {
  var n = this._, i = n[e], r = [];
  if (i && t)
    for (var o = 0; o < i.length; o++) i[o].fn !== t && r.push(i[o]);
  r.length > 0 ? n[e] = r : delete n[e];
};
```

If `fn` is falsy, the loop is skipped and `r` is empty — so `delete n[e]` fires. And if `fn`
is truthy but **no listener matches**, `r` is again empty and the whole event name is deleted.
Both `emitter.off("quit")` and `emitter.off("quit", unrelatedFn)` therefore **nuke every
listener for `quit`**. `offref(ref)` (`:24-33`) has the same shape but is guarded by
`if (ref)`, so it is safe when the ref is null and only misbehaves for a ref that was already
removed — and `Component.registerEvent` holds a real ref, so it never hits this.

### §G. `quit` is a save barrier with an adapter-flush sentinel, and it **cancels** the layout save — NEW

`core.md` §4 gives the payload as `(tasks: Eb)` and §13 Q1 asks for `Eb`'s shape. Answered:

`Bw` (`app/own/0826-Bw.js:2-15`) — `promises: []`; `add(fn)` **calls `fn()` immediately** and
pushes the result (an eager thunk, not a deferred one); `addPromise(p)` pushes an
already-created promise; `isEmpty()` is `0 === promises.length`; `promise()` is
`Promise.all(this.promises)`. Because `add` pushes whatever `fn()` returns, `add(() => {})`
still makes `isEmpty()` false and forces the `"Saving..."` interlock.

`quit` has exactly **three** in-app subscribers, and only one of them uses the collector:

1. **`Workspace`** (`app/own/1932-r4.js:250-265`) — the real barrier:

```js
i.on("quit", function(e) {
  i.requestSaveLayout.cancel();
  var t = i.app.vault.requestSaveConfig.run();
  t && e.addPromise(t);
  for (var n = 0, r = s(); n < r.length; n++) e.addPromise(r[n]);
});
```

where `s()` (`:250-257`) walks every leaf, pushes `view.save()` for each `MarkdownView` whose
`data !== lastSavedData`, and — **if any view has `saving === true` — pushes
`app.vault.adapter.promise`**, a sentinel that resolves when all pending adapter writes
settle. Without that sentinel the window could close mid-write.

Two consequences `core.md` does not have:

- **`requestSaveLayout.cancel()` on quit.** Obsidian deliberately does **not** write
  `workspace.json` on the quit path; the layout is only persisted during normal operation
  (`layout-change` / `requestSaveLayout.run()`). Relevant to the Cluster L story: a vault
  whose window is killed rather than quit loses the layout, and a faithful Corbomite port
  must not add a save-on-quit.
- **`add()` has zero in-app call sites.** Only `addPromise` is used
  (`app/own/1932-r4.js:260`, `:264`). So `core.md` §8's "plugins push async cleanup promises
  via `tasks.add(promise)`" names the wrong method; a plugin doing that would have its
  function *invoked* at registration.

2. **Popout window** (`app/own/1928-Y6.js:346-348`) — `close()` the electron window, ignoring
   the collector.
3. **Popout `Modal`** (`app/own/1925-P6.js:396-399`) — same.

`registerQuitHook` itself (`app/own/2075-Vie.js:2010-2031`) is confirmed as documented, with
one addition: the **first** thing it does is `ipcRenderer.sendSync("is-closing")` and the
handler body is entirely inside that check, so a `beforeunload` that is *not* an app close
(an in-page `window.close()` the renderer initiates) is ignored. It also **runs
`workspace.requestSaveLayout.run()` before triggering `quit`** (`:2017`) — which `quit`'s own
handler then cancels only if a save is in flight.

### §H. NEW — the theme / CSS-variable contract, which is a compat surface Corbomite must reproduce

`core.md` §1 lists the `update*` methods without their effects. Exact body-class and CSS
custom-property contract (all on `document.body` unless noted):

| Method | Reads | Writes | Fires `css-change`? |
|---|---|---|---|
| `updateTheme` `:1756` | `theme` | classes `theme-light` / `theme-dark` | **only if a class actually changed** |
| `updateFontFamily` `:1789` | `interfaceFontFamily`, `textFontFamily`, `monospaceFontFamily` | `--font-interface-override`, `--font-text-override`, `--font-monospace-override`, and `--font-print-override` (**removed** when no text font) | **unconditionally** |
| `updateFontSize` `:1796` | `baseFontSize`, **clamped to `[10, 30]`** | `--font-text-size` on `body`, `font-size` on **`document.documentElement`** | **unconditionally** |
| `updateTabSize` `:1802` | `tabSize` | `--indent-size` = `String(value)` | no |
| `updateAccentColor` / `setAccentColor` `:1834` | `accentColor` | `--accent-h` (unitless), `--accent-s`/`--accent-l` (with `%`), `--text-on-accent: var(--text-on-accent-inverted)` when the colour is light, all **removed** when the value is absent **or unparseable** | no |
| `updateUseNativeMenu` `:1799` | `nativeMenus` (`null` ⇒ `true` on macOS) | static `Menu.useNativeMenu` | no |
| `updateInlineTitleDisplay` `:1778` | `showInlineTitle` | class `show-inline-title` | no |
| `updateFloatingNavigationDisplay` `:1780` | `floatingNavigation` | class `is-floating-nav` | no |
| `updateAutoFullScreenDisplay` `:1783` | `autoFullScreen` | class `auto-full-screen` | no |
| `updateViewHeaderDisplay` `:1785` | `showViewHeader` | class `show-view-header` | no |
| `updateRibbonDisplay` `:1787` | `showRibbon` | class `show-ribbon` | no |
| `disable/enableCssTransition` `:1729`/`:1731` | — | class `no-transition` on **`appContainerEl`** | no |

Notes that matter for fidelity:

- **`getTheme()` returns only `"obsidian"` (dark) or `"moonstone"` (light)** and derives it
  from `customCss.isDarkMode()` = `document.body.hasClass("theme-dark")`
  (`app/own/2075-Vie.js:1747-1750`; `app/own/0829-Ww.js:195-197`). It never returns
  `"system"` or the `cssTheme` name. `App.isDarkMode` delegates to the same check.
- **`"system"` resolution is platform-branched** (`:1758-1760`): `matchMedia` everywhere, but
  on desktop **Linux** (not macOS, not Windows) `electron.remote.nativeTheme.shouldUseDarkColors`
  wins.
- **`updateTheme` gates `css-change` on an actual class change** (`:1761-1762`), whereas
  `updateFontFamily`/`updateFontSize` fire it unconditionally. So a config write that resolves
  to the same theme value produces **no** `css-change`. `core.md` §4's trigger table does not
  distinguish.
- **`customCss` fires `css-change` *and* `resize` together**, and only when the `<style>`
  text actually changed (`app/own/0829-Ww.js:128`, `:154`). `resize` is a workspace event
  `core.md` §4 does not list as App-emitted.
- **`setTheme(name)` is a legacy no-op**: `app/own/2075-Vie.js:2042-2046` ignores its
  argument entirely and only schedules `updateTheme()` on a `setTimeout`. The real theme
  setters are `App.changeTheme(name)` (writes `theme`) and `app.customCss.setTheme(name)`
  (writes `cssTheme`, `app/own/0829-Ww.js:178-180`).
- **`changeTheme` only writes config** (`:1751-1755`), wrapped in
  `disableCssTransition()` + a **200 ms** re-enable; boot re-enables after **500 ms**
  (`:461-463`).

### §I. NEW — theme/snippet on-disk layout (`.obsidian/themes/`, `.obsidian/snippets/`)

`customCss` (`Kw`, `app/own/0829-Ww.js:21-462`) is a `Component` with its own `onload`
(`:26-45`) and does real file I/O. Filenames are `app/own/0828-Hw.js:2-3`:
`Hw = "theme.css"`, `zw = "manifest.json"`.

- Theme folder form: `.obsidian/themes/<name>/manifest.json` + `.obsidian/themes/<name>/theme.css`.
- **A theme folder is rejected unless `manifest.name` equals the folder name** (`:75`);
  a mismatch is a silent skip, not an error.
- Legacy form: `.obsidian/themes/<name>.css` — kept in a separate `oldThemes` list, sorted
  with the same comparator as snippets, and shown in the chooser with a `" (Legacy)"` suffix
  (`:491-497`).
- `getThemePath(name)` picks the form by membership: `oldThemes` ⇒ `<name>.css`, else
  `<name>/theme.css` (`:201-202`).
- **Snippet identity is the bare filename *including* `.css`.** `readSnippets` pushes
  `Ll(w)` = basename-with-extension (`:101`), and `setCssEnabledStatus` writes
  `Array.from(enabledSnippets)` into the `enabledCssSnippets` config key (`:175-177`).
  A Corbomite `appearance.json` writer must store `"my-snippet.css"`, not `"my-snippet"`.
- Snippet `<style>` nodes are inserted **after** `customCss.styleEl`
  (`d.parentNode.insertAfter(f, d)`, `:152`), one per enabled snippet, in snippet order;
  surplus nodes are popped and detached (`:153`). Community-plugin CSS is inserted
  *before* `customCss.styleEl` (per the plugin-loader addendum), so the cascade order is
  **plugin CSS → theme → snippets**.
- `loadTheme`/`loadSnippets` are throttled at 100 ms and `readThemes` at 2 s
  (`requestLoadTheme = Ml(loadTheme, 100)`, `requestReadThemes = Ml(readThemes, 2000)`,
  `app/own/0829-Ww.js:24`) — leading-edge throttles per the cross-domain `Ml` finding.
- `csscache` is an unbounded `Map<path, css>`; a raw filesystem event invalidates exactly one
  entry (`:110`).
- The **theme chooser previews live** on selection change and **restores on Escape**
  (`app/own/0829-Ww.js:508-513`), registering `scope.register(null, "Tab", () => false)` and
  `scope.register(null, "Enter", …)` with an `!e.isComposing` guard (`:475-479`).

### §J. NEW — the `localStorage` pseudo-disk, with correct scoping

`core.md` §3's key list is right in substance but wrong in two scopings and missing five
keys. `loadLocalStorage`/`saveLocalStorage` (`app/own/2075-Vie.js:2032-2042`):

```js
// load:  localStorage.getItem(this.appId + "-" + key) → JSON.parse, null on miss/throw
// save:  truthy → setItem(appId+"-"+key, JSON.stringify(v)); falsy → removeItem(...)
```

| Key | Scope | Cite |
|---|---|---|
| `<appId>-DebugMode` | appId | read `app/own/2075-Vie.js:11`, written `:2052` |
| `DebugMode` (**bare**) | global | written `:2052` — **never read anywhere**; legacy dead key |
| `EmulateMobile` | **global, raw `getItem`, no JSON** | `Vie = "EmulateMobile"` `:2`; read `:13`; written `:2048` |
| `<appId>-note-fold-<path>` | appId | `app/own/1963-s5.js:62`, `:66`; swept by `cleanup()` `:53-60` |
| `<appId>-config` | appId, **raw string, not JSON** | `getOverrideConfigDir` `:2053-2056` |
| `spellcheck-languages` | global, JSON array | `:2056-2065` |
| `vim` | global, raw | `isVimEnabled` `:2067` (mobile only) |
| `most-recently-installed-version` | global | `:481` |
| `local-themes` | global, JSON array of installed theme names | `app/own/0829-Ww.js:365` |

Two behaviours worth copying deliberately:

- **`debugMode(false)` writes both keys and `debugMode(true)` writes only one.** `e ?
  this.saveLocalStorage(Hie, "1") : (this.saveLocalStorage(Hie, null),
  localStorage.removeItem(Hie))` (`:2051-2052`). The boot read is `"1" ===
  this.loadLocalStorage(Hie)` (`:11`) — a strict comparison against the *string* `"1"`, so
  `saveLocalStorage("DebugMode", true)` would **not** enable debug mode.
- **The spellcheck-languages migration is one-way and destroys vault config.**
  `getSpellcheckLanguages` (`:2056-2063`): if the legacy `localStorage["spellcheck-languages"]`
  exists **and** `vault.getConfig("spellcheckLanguages")` is set, it does
  `setConfig("spellcheckLanguages", null)` — **writing the `app.json` key back as `null`** —
  and adopts the localStorage array. If localStorage is absent it seeds localStorage *from*
  the vault config. So opening a vault that still has `spellcheckLanguages` in `app.json`
  mutates that file on first launch. A faithful Corbomite reader should not "fix" this, but it
  must tolerate the `null`.

- **`FoldManager` is a per-file localStorage namespace the doc omits entirely.** Key
  `appId + "-note-fold-" + path`; `savePath` **nulls the value when `folds.length === 0`**
  (`app/own/1963-s5.js:65-67`), i.e. un-folding every heading in a note *deletes* the key
  rather than writing `[]`. `cleanup()` enumerates `localStorage` and removes any
  `note-fold-` key whose path no longer resolves (`:53-60`).

### §K. §13 answered

| Q | Answer |
|---|---|
| **Q1** `Eb` shape | `Bw`, `app/own/0826-Bw.js:2-15`. See §G. `add(fn)` invokes `fn()` eagerly; `addPromise(p)`; `isEmpty()`; `promise()` = `Promise.all`. |
| **Q2** `wte` (DOM layout) | `Oie`, `app/own/2069-Oie.js:2-4`. A 4-field object, not a class: `appContainerEl` = `<body> > .app-container`, then `.horizontal-main-container > .workspace`, and `.status-bar` as a **sibling of** `.horizontal-main-container` under `.app-container`. Those four class names are a CSS/theme compat contract, and `.app-container.no-transition` is what `disableCssTransition` targets. |
| **Q3** `Y6` (Commands) | `s5`, `app/own/1963-s5.js:2-48`. See §C. Events-mixed, two registries, synthesised `checkCallback`, no duplicate check. |
| **Q4** `zb` (HotkeyManager) | `ek`, `app/own/0833-$w.js:5-89`. See §D. |
| **Q5** `o2` (InternalPlugins) | `M4`, `app/own/1939-m4.js:693-763`: `loadPlugin`, `getPluginById` (returns the **`C4` wrapper**), `getEnabledPluginById` (returns the **instance**), `getEnabledPlugins` (wrappers), `enable()`, `saveConfig()`, `onRaw`, `on`, `trigger`. **There is no `disable` and no `setPluginEnabled` on the manager** — both live on the wrapper `C4` (`:584-598`). `loadPlugin` **throws** `Error("Plugin " + name + " has already been loaded")` on a duplicate id (`:714-721`). `enable()` seeds `config[id]` for every loaded id, defaulting to `instance.defaultOn` and then `requestSaveConfig()` (500 ms leading-edge throttle, `:744`, `:696`), so `core-plugins.json` ends up carrying a key for **every loaded plugin**. |
| **Q6** the 31 ctor ids | **Answered, and `core.md` §7's list is wrong.** See the table in §L. |
| **Q7** how is `appId` generated | **Unanswerable from this corpus.** The `App` IIFE `zie` (`app/own/2075-Vie.js:4`) has **zero references** in the whole corpus, so the `new App(adapter, appId)` call site — and therefore the derivation — is in the Electron main process or the mobile bootstrap, neither of which is readable here. `app/all.js` is a concatenated non-split build and contains only the class body again. |

### §L. NEW — the real 31 internal-plugin ids, in load order (refutes `core.md` §7's inferred list)

`core.md` §7 guesses the ids from "the published `.obsidian/core-plugins.json` schema" and
names `unique-note`, which **does not exist anywhere in the corpus**, and misses `footnotes`
and `webviewer`. The 31 `loadPlugin` calls are at `app/own/2075-Vie.js:321`, gated by the
build flags `Ab.publish` / `Ab.sync` and by `rd.isDesktopApp && Ab.webViewer`
(`app/own/0803-Ab.js:2-10`).

| # | ctor | id | id literal | `defaultOn` |
|---|---|---|---|---|
| 1 | `F9` | `file-explorer` | `app/own/2036-I9.js:162` | `!0` |
| 2 | `Z1` | `global-search` | `app/own/1848-Y1.js:209` | `!0` |
| 3 | `wte` | `switcher` | `app/own/2040-Vee.js:2958` | `!0` |
| 4 | `r0` | `graph` | `app/own/1848-Y1.js:753` | `!0` |
| 5 | `$3` | `backlink` | `app/own/1961-Q3.js:5` | `!0` |
| 6 | `m9` | `canvas` | `app/own/2028-n9.js:580` | `!0` |
| 7 | `lee` | `outgoing-link` | `app/own/2037-j9.js:972` | `!0` |
| 8 | `gne` | `tag-pane` | `app/own/2044-sne.js:1786` | `!0` |
| 9 | `_9` | **`footnotes`** | `app/own/2037-j9.js:169` | **`!1`** |
| 10 | `See` | `properties` | `app/own/2039-vee.js:528` | `!0` |
| 11 | `yee` | `page-preview` | `app/own/2039-vee.js:397` | `!0` |
| 12 | `A9` | `daily-notes` | `app/own/2034-A9.js:4` | `!0` |
| 13 | `Mne` | `templates` | `app/own/2044-sne.js:2068` | `!0` |
| 14 | `nee` | `note-composer` | `app/own/2037-j9.js:482` | `!0` |
| 15 | `E9` | `command-palette` | `app/own/2032-E9.js:4` | `!0` |
| 16 | `Mte` | `slash-command` | `app/own/2040-Vee.js:3115` | **`!1`** |
| 17 | `L9` | `editor-status` | `app/own/2036-I9.js:82` | `!0` (also `hiddenFromList = !0`) |
| 18 | `k8` | `bookmarks` | `app/own/1972-Z5.js:2301` | `!0` |
| 19 | `G9` | `markdown-importer` | `app/own/2037-j9.js:196` | **absent** |
| 20 | `Ane` | `zk-prefixer` | `app/own/2044-sne.js:2364` | **absent** |
| 21 | `Ete` | `random-note` | `app/own/2040-Vee.js:3090` | **absent** |
| 22 | `mee` | `outline` | `app/own/2039-vee.js:51` | `!0` |
| 23 | `Fne` | `word-count` | `app/own/2048-Fne.js:4` | `!0` |
| 24 | `Dte` | `slides` | `app/own/2040-Vee.js:3209` | **absent** |
| 25 | `b5` | `audio-recorder` | `app/own/1963-s5.js:787` | **absent** |
| 26 | `Nne` | `workspaces` | `app/own/2048-Fne.js:111` | **absent** |
| 27 | `B9` | `file-recovery` | `app/own/2036-I9.js:260` | `!0` |
| 28 | `gte` | `publish` | `app/own/2040-Vee.js:2418` | **absent**, gated on `Ab.publish` |
| 29 | `vne` | `sync` | `app/own/2044-sne.js:428` | **absent**, gated on `Ab.sync` |
| 30 | `h8` | `bases` | `app/own/1972-Z5.js:1344` | `!0` |
| 31 | `R4` | `webviewer` | `app/own/1939-m4.js:1024` | `!1`, gated on desktop **and** `Ab.webViewer` |

"absent" means the ctor sets no `defaultOn` field, so `M4.enable()` treats it as off
(`app/own/1939-m4.js:744`). Eleven plugins are therefore off by default: the three with an explicit `defaultOn: !1`
(`footnotes`, `slash-command`, `webviewer`) plus the eight that set no `defaultOn` at all
(`markdown-importer`, `zk-prefixer`, `random-note`, `slides`, `audio-recorder`,
`workspaces`, `publish`, `sync`).

**NEW — the `core-plugins` array-migration list `E4` is not the plugin set.**
`app/own/1939-m4.js:692` is a 26-item array: the 25 of the above that a pre-1.0 vault could
have recorded, **plus `"starred"`, which no ctor in 1.13.7 defines**, and it **omits** `canvas`,
`properties`, `bases`, `bookmarks`, `footnotes` and `webviewer`. So a vault whose
`core-plugins.json` is still in array form silently loses the enable-state of those six on
migration (they fall back to `defaultOn`). The plugin-loader addendum already flagged `E4`'s
size; this is the reconciliation.

### §M. Other corrections and additions, itemised

1. **§9 is wrong about `F1`.** `openHelp` (`:1929-1932`) is
   `rd.isDesktopApp ? sendSync("help") : window.open("https://help.obsidian.md/", "_blank")`.
   There is **no `help.obsidian.md`** — the non-desktop branch opens the help *website*.
2. **`app:toggle-default-new-pane-mode` does not flip a boolean.** `:816-821`: it writes
   `defaultViewMode` `"source"`↔`"preview"` only when the current value is exactly one of
   those two, and shows a Notice either way. Any third value is a silent no-op — so a vault
   with `defaultViewMode: "legacy"` cannot toggle. The other toggles in the doc's grouped row
   *do* use `setConfig(key, !getConfig(key))`.
3. **Three commands are disabled inside a popout window.** `app:toggle-ribbon` returns `!1`
   unless `activeWindow === window` (`:766-773`); `app:toggle-left-sidebar` and
   `-right-sidebar` likewise (`:793`, `:806`). Conversely `window:toggle-always-on-top` is
   enabled **only** when `activeWindow !== window`, and additionally refuses while maximized
   (`:851-856`).
4. **`window:zoom-{in,out}` step by 0.5 frame-zoom levels and `reset-zoom` resets to `0`,
   not `1`** (`:858-874`).
5. **`markdown:clear-metadata-properties` is gated on extension `["md"]` exactly**
   (`vb = ["md"]`, `app/own/0794-hb.js:6`) — not on a broader list. The clear itself is
   `vault.process(file, d => SR(d, {}))` (`:750-751`), and `SR` = `MR({}) +
   d.slice(frontmatter.contentStart)` (`app/own/1318-SR.js:2-6`) where `MR` returns `""` for an
   empty mapping (`app/own/1317-MR.js:2-9`) — i.e. the whole block is deleted.
6. **`markdown:add-metadata-property` / `add-alias` require `canShowProperties()`**, via
   `iN(workspace)` returning the active `metadataEditor` (`app/own/1339-iN.js:2-11`) — not
   merely "the properties plugin is enabled" as the doc's row says (`app/own/2075-Vie.js:701-708`).
7. **`nextFrame(cb)` passes the rAF timestamp to the callback** (`onNextFrame(e)` calls
   `(0, i[n])(e)`, `:224-230`), and **`nextFrameOnceCallback` is a self-resetting latch, not a
   once-ever callback** (`:231-238`): repeated calls before the frame fires collapse to one,
   and the latch re-arms after the frame runs. §10's `() => void` signature is wrong.
8. **The boot sequence is instrumented** with exact `oA("…")` markers — `initialization.construct`,
   `vault.setup`, `workspace.components.managers`, `workspace.components.settings`,
   `workspace.managers.metadataType`, `workspace.managers.secretStorage`,
   `workspace.managers.hotkeys`, `corePlugins`, `vault.load`, `workspace.layout.load`,
   `workspace.layout.open`, `workspace.layout.finalize`, `workspace.extra`, `finalize`
   (`:153`…`:511`). A 20 s watchdog shows Reload / Reload-in-safe-mode / Open-another-vault
   (`:117-151`).
9. **Invalid-vault detection is desktop-only** and is a `readRaw("")` ENOENT probe →
   `this.vault = null; this.openVaultChooser(true)` (`:294-301`).
10. **`openVaultChooser` / `openHelp` are asynchronous over IPC.** `openVaultChooser`
    (`:1923-1928`) calls `lf(...)` → `ipcRenderer.sendSync("starter")` and only then
    `window.close()` when the argument is truthy; the mobile branch does
    `localStorage.removeItem("mobile-selected-vault")` and reloads after **500 ms**. §8's
    "process-death" claim holds but the sequencing does not.
11. **`Notice` defaults to a 4000 ms auto-hide and pauses on hover** — `setAutoHide` sets a
    `mouseenter` flag that suppresses the hide and re-arms it **1000 ms** after `mouseleave`
    (`app/own/0819-ww.js:243`, `:261-275`). `.notice-container` is created once per window
    in a `WeakMap` and detached when empty (`:247-253`, `:284-295`); `addButton` hides the
    notice *before* invoking the callback (`:276-283`).
12. **`importAttachments` renames `"Pasted image"` to `"Pasted image <YYYYMMDDHHmmss>"`**
    (`app/own/2075-Vie.js:1846-1878`, the rename at `:1863`) — a filename-generation rule worth
    matching for clipboard-image parity.
13. **`garbleText()`** (`app/own/2075-Vie.js:1844-1845`) adds `is-text-garbled` to `appContainerEl`. Undocumented
    debug affordance.
14. **`fixFileLinks` is an App-owned post-processor**, desktop-only, rewriting
    `img/audio/video/source/iframe` `src` for `file:///` and vault-relative resources
    (`:1726-1727`, `:1737-1746`).
15. **`onConfigChanged` is a 14-arm switch** (`:566-608`) including two the doc omits:
    `"cssTheme"` → `customCss.reloadTheme()` and `"mobileQuickRibbonItem"` →
    `mobileNavbar.updateRibbonMenuItem()`; and `"showRibbon"` calls
    `workspace.updateFrameless()` after `updateRibbonDisplay()`.
16. **Extension families** are one table (`app/own/0794-hb.js:2-11`): images
    `["bmp","png","jpg","jpeg","gif","svg","webp","avif"]`, audio
    `["mp3","wav","m4a","3gp","flac","ogg","oga","opus"]`, video
    `["mp4","webm","ogv","mov","mkv"]`, `["pdf"]`, `["md"]`, `["canvas"]`, `["base"]`,
    `["json","css","js"]`, plus derived `bb` (md+canvas+base) and `wb` (everything
    embeddable).
17. **The base `Scope` walk semantics are confirmed exactly**, including the "non-catch-all
    returning `undefined` stops the walk without consuming" rule
    (`app/own/0747-ly.js:18-27`), and `Rg`/`cy` (`:30-39`) is genuinely forward-**or**-fallback:
    if the delegate returns `undefined`, there is no further chain — the `Rg` does not retry
    its own keys or its parent. §13 Q8's answer stands.
18. **`isMatch` semantics are richer than documented** (`:130-133`): modifiers are
    `null`-wildcard or **exact string equality** (an extra held modifier breaks the match);
    the key is `null`/`""`-wildcard, else it matches `ctx.vkey` **or** `ctx.key`
    **case-insensitively**. A `""` modifier array therefore means "no modifiers held", which is
    why `app:open-help`'s `Yw([], "F1")` works.
19. **Modifier strings are always alphabetically sorted** because `getModifiers` builds
    `Ctrl, Meta, Alt, Shift` then `compileModifiers` sorts (`:109-115`). §8's invariant holds;
    the `decompileModifiers` round-trip is `Meta`↔`Mod` on macOS but `Ctrl`↔`Mod` elsewhere
    (`:116-121`), so a mac-authored `hotkeys.json` is not portable and vice versa.
20. **`keydown` is handled in the capture phase** (`addEventListener("keydown", …, !0)`, `:42`),
    `focusin` in the bubble phase. The tab-trap re-checks `t.scope === n` inside a
    `setTimeout(…, 0)` and **blurs the active element** when `focus()` returns false
    (`:92-106`). Modifier-only keydowns are dropped entirely (`isModifierKey`, `:122-123`), so
    a hotkey bound to a bare modifier can never fire.
21. **`InternalPlugins` and `HotkeyManager` both watch `vault.on("raw")`** for external edits
    to their own `.obsidian/<id>.json` / `hotkeys.json`, both through a **50 ms leading-edge
    throttle** (`app/own/1939-m4.js:548`, `app/own/0833-$w.js:9`), and the internal-plugin one
    suppresses its own writes with an mtime comparison (`app/own/1939-m4.js:613-631`).

### §N. CONFIRMED (re-verified unchanged in 1.13.7)

| `core.md` claim | Cite |
|---|---|
| `App.prototype.on` is a no-op returning `undefined` | `app/own/2075-Vie.js:2042` |
| `Events.on` allocates `this._ = {}` and returns `{e, name, fn, ctx}` | `app/own/1126-FD.js:4`, `:6-16` |
| `offref(ref)` removes exactly the ref | `app/own/1126-FD.js:24-33` |
| `trigger` snapshots with `.slice()` before dispatch | `app/own/1126-FD.js:38` |
| `tryTrigger` swallows synchronously and re-throws via `setTimeout(…, 0)`; one bad listener does not stop the others | `app/own/1126-FD.js:41-48` |
| `Scope.register(modifiers, key, fn)` returns the `KeyHandler` and normalises modifiers via `compileModifiers` | `app/own/0747-ly.js:6-13` |
| `Scope.unregister` is identity-remove | `app/own/0747-ly.js:14-15` |
| `Scope.setTabFocusContainerEl` drives the focus trap | `app/own/0747-ly.js:16-17` |
| `compileModifiers`: `"Mod"`→`"Meta"` on mac else `"Ctrl"`, sorted, comma-joined | `app/own/0747-ly.js:112-115` |
| Multiple handlers with identical `(modifiers, key)` coexist, registration order, first non-`undefined` wins | `app/own/0747-ly.js:18-26` |
| `Keymap.init()` is a lazy per-process singleton; `app.scope === Keymap.global.getRootScope()` | `app/own/0747-ly.js:44-47`; `app/own/2075-Vie.js:11` |
| exactly **five** `.trigger(` sites in the App class: four `css-change` (`:665`, `:1762`, `:1795`, `:1798`) and one `quit` (`:2019`) | `app/own/2075-Vie.js` |
| `App.vault` listens `closed` → `openVaultChooser(true)` and `config-changed` → `onConfigChanged` | `app/own/2075-Vie.js:303-305` |
| `registerQuitHook` sets `window.onbeforeunload = null` on entry for self-idempotence | `app/own/2075-Vie.js:2012-2014` |
| `Eb` payload: `isEmpty()` ⇒ sync return, else `preventDefault()` + `returnValue = "Saving..."` + `await i.promise()` + `window.close()` | `app/own/2075-Vie.js:2019-2028` |
| `getWebviewPartition()` returns `"persist:vault-" + appId` | `app/own/2075-Vie.js:1735-1736` |
| `getObsidianUrl` = `"obsidian://open?vault=" + enc(vaultName) + "&file=" + enc(path)` | `app/own/2075-Vie.js:2002-2005` |
| `getAppTitle(suffix)` = `suffix + " - " + title` | `app/own/2075-Vie.js:1733-1734` |
| slow-startup notice: `"1" === loadLocalStorage("slow-startup-check") && sA() > 8e3` | `app/own/2075-Vie.js:557-561` |
| release notes shown once per version bump via `most-recently-installed-version` | `app/own/2075-Vie.js:481` |
| `changeTheme` only writes config; the `config-changed` event loops back | `app/own/2075-Vie.js:1751-1755` + `:304-305` |
| suggesters register `ArrowUp/ArrowDown/PageUp/PageDown/Home/End` with `[]` modifiers plus `Ctrl+P`/`Ctrl+N`, and guard `Enter` with `!e.isComposing` | `app/own/0819-ww.js:11-16` |
| `Menu` pushes its scope in `onload` and pops it in `onunload` | `app/own/0751-my.js:28`, `:30` |
| `Scope` is not `Events`-derived (no pub-sub) | `app/own/0747-ly.js:2-28` |
| `App` mixes in neither `Component` nor `Events` | `app/own/2075-Vie.js:5`, `:2042` |

### §O. Unverifiable / out of scope

- **`appId` derivation** (§13 Q7). `zie` has zero corpus references; the constructor call site
  is outside the readable bundles.
- **`help` window behaviour.** `help/own/0080-Ga.js` exists and contains its own `Keymap`
  copy, but `sendSync("help")` targets the Electron main process, which is not in the corpus.
- **Display names.** Every command `name` is an i18n call (`bd.commands.…`,
  `bd.plugins.fileExplorer.…`). The English strings live in `bd`, which the index does not
  carry as readable source, so `core.md` §6's "Display name" column cannot be re-verified
  here — only the keys can.
- **`Notice` layout/animation timings** are stated from the animation call sites only; the
  CSS itself lives in `asar/obsidian/app.css` and was not cross-read for this addendum.

---

## Why noticed now

Cluster O closed on 2026-08-20 and its own audit report found that of ~79 `KAction`s in
Corbomite **zero** belonged to canvas/bases/graph, 17 were permanently disabled and 7
silently no-op off-markdown. That pass worked from `docs/audit-2026-08-20-context-sensitive-ui.md`
rather than from `core.md`, so the domain doc's command inventory was never load-bearing for
it — and it turns out to be missing two thirds of the surface. Separately, Cluster L's
`workspace.json` work made `quit` load-bearing (the `requestSaveLayout.cancel()` finding
bears directly on whether a layout save belongs on the shutdown path), and the
`.obsidian/hotkeys.json` compat contract named in the project doctrine had never been read
from source by anyone — §3 of `core.md` delegates it to "the plugin domain", and the plugin
addendum does not cover it either.

## Action taken

No implementation changes; this is a source-verification addendum. Three items are flagged
for the next implementation surface rather than applied here:

1. **`hotkeys.json` is now specifiable.** §D gives the read semantics (override-only,
   presence-with-`[]` unbinds, `code` field, mac-only `KeyI` case), the write semantics
   (overrides only, never merged with defaults), and the 16 default bindings. A Corbomite
   `HotkeyManager` over `KConfigGroup("Shortcuts")` cannot express the `code` field or the
   presence-empty case through `QKeySequence`, so it needs a stored-binding table plus a
   `QKeySequence`-based matcher — worth prototyping before any `hotkeys.json` import.
2. **The command inventory is a checklist, not a suggestion.** 115 ids is the row axis for
   any Corbomite `CommandRegistry`; the four undocumented gate fields (`allowPreview`,
   `allowProperties`, `showOnMobileToolbar`, `repeatable`) are the missing half of Cluster O's
   context-sensitivity model and belong in whatever spec that work produces.
3. **Modals must cut the shortcut chain.** §E is a one-line rule in any Corbomite dialog
   implementation (own `QShortcutContext`), and it is the concrete behavioural content that
   `core.md` §11's "need a `Scope` shim" recommendation was missing.

Cite this addendum from `superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md`
(the command inventory and gate fields) and from
`superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md` (the `quit` chain and
`requestSaveLayout.cancel()`).