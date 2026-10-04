# Plugin 1.13.7 — the loader is in the corpus, `loadCSS` is *not* opt-in, and there is no permission model at all

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/plugin.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/plugin.md` §1 (`loadCSS`, `_onConfigFileChange`), §2 (`PluginManifest`, `CommandSpec`, `CliHandlerSpec`), §3 (`manifest.json`, `data.json`, `community-plugins.json`), §8 (invariants), §9 (`disable` → leaf detach), §10 (delegates), §12 (last bullet), §13 (Q1, Q2, Q3, Q4, Q6, Q7, Q9 all answered), §15 (`QW`/`JD`/`Y6`/`$0` rows)
**Relevant cluster plans:** none open. Any future Corbomite native plugin-ABI design must cite this. Note `docs/superpowers/plans/INDEX.md` has no live plugin cluster.

**Corpus:** `RE/obsidian/1.13.7/renamed/src/<bundle>/own/*.js`. Citations below are
`<bundle>/<rel>:<line>`. `domains/plugin.md` cites `plugin/Plugin.js:<line>` throughout;
**every one of those citations is dead** — that extraction no longer exists. Remap table at §A.

> **Doctrine reminder.** Corbomite deliberately does **not** mirror Obsidian's JS plugin API
> (no `require()`, no `manifest.json`+`main.js` bundling, no DOM, no `eval`, no npm
> ecosystem). That is an intentional divergence and is **not** a gap. What follows documents
> only what is transferable regardless of language: the manifest-as-intent record, the
> capability/gating surface, the lifecycle state machine and its ordering, the
> registration/hotkey/menu points, and the settings-storage contract. Web-delivery-only
> behaviour is quarantined in §E and flagged **not applicable to Corbomite by design**.

## Finding

### 1. HEADLINE CORRECTION — `loadCSS()` is called automatically by the loader; `plugin.md` §12 says the opposite

`plugin.md` §12's last bullet is explicitly labelled "**New** (not in Pass-1 signals)" and asserts:

> **`loadCSS()` is opt-in, not automatic.** Plugins that ship a `styles.css` must explicitly
> call `await this.loadCSS()` in `onload`. […] Corbomite's QSS loader should preserve opt-in
> semantics (don't load all QSS from all plugins on startup).

Both halves are wrong for community plugins. `loadPlugin` does:

```js
this.plugins[e] = n, [4, n.load()];      // app/own/1939-m4.js:91
...
return p.sent(), [4, n.loadCSS()];        // app/own/1939-m4.js:93
...
case 3: return m.sent(), t && n.onUserEnable(), [2, n];   // app/own/1939-m4.js:95
```

`loadCSS()` is **unconditionally awaited after `load()` resolves**, for every community plugin
whose `styles.css` exists. `loadCSS` itself is the opt-in-shaped half — it checks
`vault.exists(dir + "/" + nL)` and no-ops when the file is absent (`app/own/1938-c4.js:167-170`).
A Corbomite QSS loader must therefore follow the *existence* gate, not an author-call gate.

**And the method has no idempotence guard**, so a plugin that *also* calls `this.loadCSS()`
from its own `onload` gets **two** `<style>` nodes with identical content:

```js
i = n.dir + "/" + nL; [4, t.vault.exists(i)];                 // app/own/1938-c4.js:167
...
(o = createEl("style", { type: "text/css" })).textContent = r,
  a = this.app.customCss.styleEl,
  document.head.insertBefore(o, a),
  this.register(function() { return o.detach(); });           // app/own/1938-c4.js:171-174
```

Note `register` is only reached on success; a missing file registers nothing. `insertBefore`
relative to `app.customCss.styleEl` confirms §3's "vault CSS wins" ordering.

**Asymmetry worth copying deliberately:** the *internal*-plugin wrapper `C4`
(`app/own/1939-m4.js:545-691`) has **no `loadCSS` at all** — core plugins never load
`styles.css` through this path; they inject through the theme/custom-CSS pipeline
(`app/own/0829-Ww.js`, whose `Hw = "theme.css"`, `zw = "manifest.json"` at
`app/own/0828-Hw.js:2`). A native host should decide explicitly which side of that line it is
on rather than inheriting Obsidian's split silently.

### 2. HEADLINE NEW — there is no per-plugin permission or capability model

`grep -n permission` over `app/own/1939-m4.js`, `app/own/1938-c4.js` and `app/own/2061-oie.js`
returns **zero hits**. Obsidian community plugins get the whole `App` object and there is no
manifest field, prompt, or declaration that scopes what they may touch. The **complete** set of
gates that stand between an installed plugin and a running one, in evaluation order, is:

| # | Gate | Source | Failure UX |
|---|---|---|---|
| 1 | `Db(Ab.plugins, id)` — build/distribution feature flag | `app/own/1939-m4.js:191` | `enablePlugin` returns `false`, no toast |
| 2 | manifest present in `manifests[id]` (a parseable `manifest.json` with a truthy `id`) | `app/own/1939-m4.js:192` | returns `false`, no toast |
| 3 | not on the remote deprecation list | `app/own/1939-m4.js:193`, `:369-375` | toast `msgPluginVersionIssues` |
| 4 | hardcoded version refusals (`cmdr` < `0.5.4`, `obsidian-image-toolkit` < `1.4.3`, or an unparseable version — see §5; `settings-search` always; `better-pdf-plugin` at exactly `1.4.0`) | `app/own/1939-m4.js:193-203` | toasts `msgPluginIncompatible` / `msgPluginNonfunctional` (6 s) |
| 5 | `!isDesktopApp && manifest.isDesktopOnly` | `app/own/1939-m4.js:204` | **returns `false` with no message at all** |
| 6 | load-time: export found, class extends `Plugin`, ctor + `load()` + `loadCSS()` all succeed | `app/own/1939-m4.js:89-95`, `:211-213` | toast `msgFailedToLoadPlugin`, `console.error("Plugin failure: "+id, err)` |
| 7 | whole-plugin safe mode: `localStorage["enable-plugin-" + appId] === "true"` | `app/own/1939-m4.js:310-311` | modal `g4` on first run (`:127`, `:508-535`) |

Two things follow that matter for a native ABI:

- **`Ab.plugins` is not a permission system, it is a distro switch — and its array form is a
  coarse id allow-list.** `Db` is the whole mechanism:

  ```js
  Ab = { plugins: !0, themes: !0, snippets: !0, sync: !0,
         publish: !0, webViewer: !0, devTools: !0, insider: !0 },   // app/own/0803-Ab.js:2-11
  Pb = !1;                                                          // app/own/0803-Ab.js:12

  function Db(e, t) { return !0 === e || Array.isArray(e) && e.includes(t); }  // app/own/0802-Db.js:2
  ```

  So `Db(Ab.plugins, id)` means "`true` → all, `false` → none, `[…]` → exact-id allow-list".
  The same flag is re-checked on every plugin CLI verb as
  `if (!1 === Ab.plugins) throw "Restricted: plugins are disabled."`
  (`app/own/1225-SL.js:633`, `:657`, `:696`, `:735`). **A Corbomite native ABI that ships without a
  capability declaration is byte-for-byte as permissive as Obsidian** — that is the bar, and
  Obsidian does not clear it. If Corbomite ships per-plugin capabilities, that is a *divergence
  to document*, not a parity gap.
- **Gate 5 is silent.** The install UI does warn — `!rd.isDesktopApp && u.isDesktopOnly` renders
  `div.mod-warning` with `labelUnsupported()` (`app/own/1926-z6.js:534-537`) — but the enable
  path just returns `false`, and the settings toggle then snaps itself back
  (`n.sent() || e.setValue(!1)`, `app/own/2061-oie.js:846-859`) with no reason shown.

### 3. NEW — the community-plugin lifecycle state machine, with ordering, from `loadPlugin`/`enablePlugin`/`disablePlugin`

All in `app/own/1939-m4.js`. This is the piece `plugin.md` §13 filed as unverifiable
("Need `app.plugins.loadPlugin` implementation") — it is fully in the corpus.

**Startup (`initialize`, `:107-182`), strictly sequential:**

1. `readConfigJson("community-plugins")`; if not an array, **silently coerce to `[]`**
   (`:122-124`). `enabledPlugins = new Set(that)`.
2. `loadManifests()` (`:20-51`) rescans `<configDir>/plugins/*/manifest.json`.
3. **If zero manifests parsed, return immediately** (`:126`) — no safe-mode prompt, no
   `didChange`.
4. If `!isEnabled()` **and** the localStorage key is absent entirely (`null === localStorage.getItem(…)`,
   `:127`), open the trust modal `g4` — `cls: "mod-lg", "mod-trust-folder"`, three description
   paragraphs, `buttonDontTrustAuthor` (cancel, initial focus) / `buttonEnablePlugins`
   (`:508-532`). Choosing trust calls `app.plugins.setEnable(true)` then
   `setting.openTabById("community-plugins")` (`:525-527`).
5. Enable **one at a time, awaited in array order** (`:165-171`). Each is wrapped in a
   **3-second watchdog** (`:133-158`) that shows `msgPluginHang` with a **Disable** button which
   removes the id from `enabledPlugins`, saves, and does `window.location.reload()`.
   ⇒ *Load order is exactly `community-plugins.json` order.* A native host must state its own.
6. `requestSaveConfig()`; `lastUpdateCheck` from `localStorage["last-plugin-update-check"]`;
   auto-update on window `focus` / mobile `resume`, throttled by `2592e5` ms = **30 days**
   (`:173-176`); `checkForDeprecations()` immediately and every `432e5` ms = **12 hours**
   (`:177-178`).

**Per-plugin load (`loadPlugin`, `:75-99`), in order:**
`isEnabled()` gate → return existing instance if present → require manifest → read
`<dir>/main.js` → **strip the inline sourcemap** (see §E) → `window.eval` the body → pick
`exports.default || exports` (throw `"Failed to load plugin <id>. No exports detected."` if
neither) → `new Klass(app, manifest)` → **`instanceof h4` check**, else throw
`"Failed to load plugin <id>"` (`:90`) → `plugins[id] = instance` → `await load()` →
`await loadCSS()` → `t && onUserEnable()`.

**`onUserEnable` ordering is now answered (§13 Q9).** The second parameter
(`void 0 === t && (t = !1)`) is threaded from `enablePlugin(e, t)` (`:185-186`), whose only
`true` caller is `enablePluginAndSave` (`:239-248`) — i.e. **only a user-initiated enable**.
So `onUserEnable` fires after `load()` *and* after `loadCSS()`, never on startup load, and never
when safe mode re-enables the set (`setEnable` calls `enablePlugin(id)` with the default `false`,
`:331`).

**Disable (`disablePlugin` → `unloadPlugin`, `:221-239`, `:100-106`) — answers §13 Q1:**
```js
return (n = this.plugins[e]) ? (n._userDisabled = t, n.unload(), delete this.plugins[e], [2]) : [2];
```
`_userDisabled` is **the loader's `unloadPlugin(id, userDisabled)` second parameter**, defaulting
`false`. Only `disablePluginAndSave` passes `true` (`:255`). `setEnable(false)` (safe mode) calls
`disablePlugin(id)` with no flag (`:321`), so **entering safe mode does not close the plugin's
open tabs**, while a real disable does (`_userDisabled && detachLeavesOfType`, `app/own/1938-c4.js:76`).
The CLI `plugin:reload` path also passes no flag (`app/own/1225-SL.js:2230`) ⇒ **reload does not
close leaves either**. `plugin.md` §9 asserts the disable-closes-leaves rule but never proves the
`_userDisabled` provenance; this is it, plus the two cases where it does *not* apply.

**`disablePluginAndSave` mutates the enabled-set and schedules the write *before* awaiting the
unload** (`:250-260`):
```js
return this.enabledPlugins.delete(e), this.requestSaveConfig(), [4, this.disablePlugin(e, !0)];
```
⇒ a crash mid-unload still leaves the plugin disabled on disk. `requestSaveConfig` is
`Ml(saveConfig, 1e3)` — a **1000 ms trailing debounce** (`:6`) onto
`writeConfigJson("community-plugins", Array.from(this.enabledPlugins))` (`:293`).
Internal plugins use **500 ms** (`:696`). Worth not copying: two different debounce constants
for the same class of write.

**The reload sequence for a community plugin is a full folder rescan**
(`app/own/1225-SL.js:2226-2245`): `disablePlugin(e)` → `loadManifests()` → `enablePlugin(e)`.
Re-reading every `manifest.json` in the folder to reload one plugin is a defect, not a contract.

### 4. NEW — §13 Q2 answered: a throw from `onload` is *not* recovered, and the plugin stays half-loaded

`Plugin.prototype.load` (`:36-48`) sets `this._loaded = !0` **before** awaiting `onload()`:
```js
return this._loaded ? [2] : (this._loaded = !0, [4, this.onload()]);
```
There is no `try`/`catch`. The rejection propagates to `loadPlugin`'s `await` at
`app/own/1939-m4.js:91`, is caught at `:210-213`, toasts `msgFailedToLoadPlugin`, logs, and
**returns `false` — without calling `unload()`**. So every `register*`/`addChild` the plugin
completed before throwing stays in `_events`/`_children` forever, and `_loaded` is `true`, so a
later `unload()` *would* run them but is never called: `enablePlugin` bailed at `:213`, and
`plugins[id]` was already assigned at `:91`. **The instance stays in the `plugins` map.** A retry
via `enablePlugin` short-circuits at `:81` (`(n = this.plugins[e]) ? [2, n]`), so the user gets
back the *broken* instance with no error and no re-attempt. This is a real leak and it is worth
recording so a native host can do better deliberately.

**Related, and load-bearing for cleanup semantics:** `Component.unload` is guarded by
`if (this._loaded)` (`app/own/0712-fg.js:57`). Calling `unload()` on a never-loaded or
already-unloaded component is a **silent no-op** — the queued cleanups in `_events` simply never
run. A native host that models "registered cleanups" as a scope object must replicate the guard
or state explicitly that unloading an unloaded plugin is a no-op, because plugin code relies on
it (`Component.prototype.unload` `:56-66`; also note children are drained to empty **before**
any `_events` cleanup runs, and `onunload()` runs last, after both loops — §1's ordering claim
is right, but the reason `addChild`-during-`onload` works is `addChild` itself:
`this._children.push(e), this._loaded && e.load(), e` (`:67-68`) loads the child *immediately*,
and the loop's later `i[n].load()` is a no-op on the child's own `_loaded` guard).

### 5. NEW — the manifest record: verified field inventory, and two records not one

`plugin.md` §2 presents `PluginManifest` as *inferred*. The fields it lists are all real, and the
inventory can now be pinned to a read site:

| Field | Read at | Role |
|---|---|---|
| `id` | `app/own/1939-m4.js:39`, `:64`, `:90`, `:100`; `app/own/1938-c4.js:51, 63, 67` | map key; truthiness is the manifest's only validity test |
| `name` | `app/own/1938-c4.js:63` (`"<name>: " + cmdName`), `:133` (CLI help prefix), `:220` (setting-tab title); `app/own/2061-oie.js:865-871` | command/CLI display prefix; settings-tab and list-row label |
| `version` | `app/own/1939-m4.js:193`, `:196`, `:202`, `:490`; `app/own/0799-Sb.js:8` | deprecation + install/update comparison |
| `minAppVersion` | `app/own/0799-Sb.js:8-9` | **install-time version resolution** (below) |
| `description` | `app/own/2061-oie.js:866`; `app/own/1225-SL.js:641` | list row / `plugin info` |
| `author` | `app/own/2061-oie.js:868`; `app/own/1225-SL.js:640`; `app/own/1926-z6.js:497`, `:1069` | **normalised at load, see below** |
| `authorUrl` | app/own/1926-z6.js:501, 1073 | details-modal link (`target="_blank"`, `rel="noopener"`) |
| `fundingUrl` | `app/own/2061-oie.js:865`, `:890-893`; `app/own/1926-z6.js:627-631` | Donate button |
| `isDesktopOnly` | `app/own/1939-m4.js:204`; `app/own/1926-z6.js:534` | silent enable refusal on mobile + `mod-warning` |
| `dir` | `app/own/1939-m4.js:40`, `:65`; `app/own/1938-c4.js:167, 189` | **injected by the loader; vault-relative** |

**CORRECTION — `dir` is exactly vault-relative, never "absolute-ish".** §2's
`dir: string; // absolute-ish vault path .obsidian/plugins/<id>/` is wrong on "absolute-ish":
`manifest.dir` is a bare folder name from `adapter.list(configDir + "/plugins").folders`
(`app/own/1939-m4.js:26-30`, `:40`), e.g. `.obsidian/plugins/my-plugin`. `getPluginFolder()` is
`"".concat(this.app.vault.configDir, "/plugins")` (`:183-184`). Everything downstream
(`loadData`, `saveData`, `getModifiedTime`, `loadCSS`) joins onto it verbatim. This matters for a
native host: the on-disk layout is `.obsidian/plugins/<id>/{manifest.json,main.js,styles.css,data.json}`
with no absolute path stored anywhere.

**NEW — `author` is normalised at manifest-load time.** Both loaders run the same expression
(`app/own/1939-m4.js:40` and `:65`):
```js
l.dir = a, l.author && "obsidian" !== l.author.toLowerCase() || (l.author = ""), t[l.id] = l;
```
⇒ **a missing `author`, or one equal to `"obsidian"` case-insensitively, is overwritten with the
empty string.** A native host reading these manifests must not treat `""` as "absent" in a way
Obsidian doesn't, and should not round-trip `"obsidian"` back out of a re-serialised manifest.

**NEW — there are two manifest records, and the loader only ever sees one.** The community
modal merges a **remote registry entry** with the **on-disk manifest**, with explicit precedence:
`(w = e.author || u.author)` — registry `author` wins over installed (`app/own/1926-z6.js:497`);
the installed `authorUrl` wins for the link (`:501`); the registry version renders with the
installed version appended as `labelCurrentlyInstalledVersion` (`:494-496`). The registry is
`Dy("obsidianmd/obsidian-releases", "community-plugins.json")` fetched through a 5-minute-TTL,
60-second-timeout memoiser that throws `"Failed to parse community plugins."` unless the payload
is an array (`app/own/1211-JI.js:6-17`, `rL`). Install and update both read the manifest from
`raw.githubusercontent.com/<repo>/HEAD/manifest.json` (`Dy`, `:480`, `app/own/1225-SL.js:2272`)
while the **code comes from a GitHub *release tag*** chosen by `Sb` — manifest-at-HEAD vs
code-at-tag. `installPlugin` additionally enforces id agreement on both paths:
`JSON.parse(text).id === expected` else `"Plugin ID mismatch."`
(`app/own/1939-m4.js:387`, `app/own/1225-SL.js:2275`).

**NEW — `minAppVersion` is a real install gate, with this exact algorithm** (`Sb`,
`app/own/0799-Sb.js:2-25`): if the running app version satisfies `manifest.minAppVersion`, use
`manifest.version` verbatim; otherwise fetch the repo's `versions.json` and pick the newest key
whose own requirement the app satisfies, else `"No compatible version found for current Obsidian
version."` (`app/own/1225-SL.js:2282`). Comparison is `Uy` (`app/own/0774-Uy.js:2-5`) over
`jy`, which is `split(".").map(parseInt)` with **no prerelease handling** — `"1.2.3-beta"`
parses as `1.2.3` (`app/own/0773-jy.js:2-10`) — and which **returns true when the installed
version is unparseable**, so `Uy(n.version, "0.5.4")` is `true` for a garbage `version` string
and blocks the plugin.

### 6. NEW — hotkeys: the registration point `plugin.md` §10 has no row for, and the full modifier contract

`plugin.md` §10 never covers hotkeys beyond `hotkeys?:` in `CommandSpec`. The whole mechanism is
one line in the command registry (app/own/1963-s5.js:22):

```js
e.hotkeys && this.app.hotkeyManager.addDefaultHotkeys(e.id, e.hotkeys)
```

and `removeCommand` symmetrically calls `removeDefaultHotkeys` (`:23-24`). So **a plugin's hotkey
is a field on its command spec, not a separate registration verb.** The manager
(`app/own/0833-$w.js:5-…`) keeps two layers and custom always wins:

- `defaultKeys = {}` (from plugins/core), `this[Jw] = {}` under `Symbol("customKeys")`
  (from `.obsidian/hotkeys.json`); `baked = false` is cleared by every mutation.
- `bake()` (`:57-76`) walks **custom keys first**, then defaults skipping any id already in
  custom (`!t.hasOwnProperty(o)`), and flattens into `bakedHotkeys[]` + a **parallel
  `bakedIds[]`** — so **one command can hold several accelerators** and the id is looked up by
  parallel index. Each record is normalised to
  `{modifiers: uy.compileModifiers(a.modifiers), key: a.code ? Xw(a.code) : a.key}` — this
  re-verifies and pins down the 2026-06-10 `2026-06-10-workspace-serialization-corrections.md`
  note that `hotkeys.json` records may carry a `code` field. `Xw` is exactly
  `e.startsWith("Key") && 4 === e.length ? e.charAt(3) : e` (`app/own/0831-Xw.js:2`).
- `load()` (`:30-41`) **only accepts an object** — a corrupted or array-shaped `hotkeys.json` is
  ignored wholesale, and `save()` (`:19-30`) writes it straight back out.
- Third instance of the **same 50 ms-debounced `vault.on("raw")` exact-path watcher** pattern:
  `onRaw(e) { e === configDir + "/hotkeys.json" && this.onConfigFileChange() }`
  (`:17-18`), alongside the per-plugin `data.json` watcher (§7) and the internal-plugin `.json`
  watcher (§8). That triple is the transferable bit: *compare against a last-write stamp, debounce,
  call an opt-in hook.*
- `printHotkeyForCommand` renders only `[0]` (`:54-56`) — the UI shows the first accelerator.
- Display formatter `Qw` (`:2-13`) iterates the canonical modifier order `sy` pushing `ay[n]`,
  title-cases the key (`replace(/([A-Z])/g, " $1").trim()` then capitalise), and joins with
  **`" + "` on desktop, `" "` when `Zp` (mobile)**. `ay`/`sy` are platform-conditional
  (`app/own/0746-ay.js:2-15`): `sy = ["Mod","Ctrl","Meta","Alt","Shift"]` and on desktop
  `Meta` displays as `"Win"` while `Mod` and `Ctrl` both display as `"Ctrl"`.

**The modifier string itself is alphabetically sorted, not in `sy` order.**
`compileModifiers` maps `"Mod"` → `Zp ? "Meta" : "Ctrl"`, then `.sort().join(",")`
(`app/own/0747-ly.js:112-115`). So the canonical value is e.g. `"Alt,Ctrl,Shift"`, and
`isMatch` compares it by **exact string equality** (`:130-133`). `getModifiers` reads only
`Ctrl/Meta/Alt/Shift` off the event — **`Mod` is derived, never observed** (`:110-112`).

**The keymap dispatch rule, which is what `registerScopeEvent` actually unwinds.** Scope
(`ly`, `app/own/0747-ly.js:2-29`): `register(modifiers, key, func)` returns
`{scope, modifiers: e ? uy.compileModifiers(e) : null, key, func}` — **`null` modifiers when
omitted**, not `""`. `unregister` is `this.keys.remove(e)` — `Array.prototype.remove`, so it
removes only the first reference. `handleKey` iterates in insertion order and:
```js
if (uy.isMatch(r, t)) {
  var o = r.func(e, t);
  if (void 0 !== o) return o;
  if (null !== r.key || null !== r.modifiers) return o;
}
```
⇒ **a binding that matched and whose handler returned `undefined` still stops propagation**, unless
it was registered with *both* `key === null` and `modifiers === null` (then it falls through to
`this.parent.handleKey`). The global listener is capture-phase on `window`, resolves the
**active window's** current scope, and only then `preventDefault()` + `stopPropagation()`
(`uy.onKeyEvent`, `:74-91`). One hardcoded platform special case lives there:
`54 === t.which && "^" == t.key && "KeyI" === t.code && (o = "KeyI")` (`:83`).
`isModEvent(t)` (`:134-139`) is the middle-click / `Mod+Alt` leaf contract: button-1 → `"tab"`;
`Mod+Alt+Shift` → `"window"`; `Mod+Alt` → `"split"`; `Mod` → `"tab"`.

**Commands also drive the native application menu by id.** `applyHotkeys` walks the Electron menu
template, and for any item with an `"appCommand"` key resolves
`getHotkeys(id) ?? getDefaultHotkeys(id)`, takes `[0]`, sets `accelerator`, and sets
`registerAccelerator = false`; `hideUnregisteredCommands` then sets `visible = false` for every
`appCommand` not in `app.commands` (`app/own/0754-wy.js:614-640`). ⇒ a plugin's commands appear
in the native menu **for free**, and disappear when it unloads. That is a genuine cross-surface
coupling a native host would have to reproduce (or consciously drop) via its action collection.

### 7. NEW — `_onConfigFileChange` also refreshes the settings tab, and the watcher is one global handler

§3 describes the watcher as "the plugin loader wires a `Vault.on('raw', …)` subscription gated on
manifest dir". True in effect, but the wiring is **a single subscription on the manager**, not
per-plugin, and the filter is an exact string comparison (`app/own/1939-m4.js:8`, `:12-19`):

```js
i.app.vault.on("raw", i.onRaw.bind(i));                              // :8
t.prototype.onRaw = function(e) {
  if (e.startsWith(this.app.vault.configDir)) {
    var t = Al(Pl(e));                                               // basename of parent dir
    if (this.enabledPlugins.has(t) && e === "".concat(this.getPluginFolder(), "/").concat(t, "/data.json")) {
      var n = this.plugins[t];
      n && n.onConfigFileChange();
    }
  }
};                                                                    // :12-19
```

⇒ **§13 Q6 answered.** Not a glob: parent-dirname lookup, membership test against the enabled
set, then exact `…/plugins/<id>/data.json` equality.

**NEW — the debounced handler also calls `settingTab.update()`.** `plugin.md` §1 lists
`_onConfigFileChange` as "calls `onExternalSettingsChange()` when strictly greater" and stops
there. The full tail (`app/own/1938-c4.js:199-214`):

```js
return this.onExternalSettingsChange ? [4, this.getModifiedTime()] : [2];   // :205
case 1: e = n.sent(), this._lastDataModifiedTime < e
        ? [4, this.onExternalSettingsChange()] : [3, 3];                    // :207
case 2: n.sent(), null === (t = this.settingTab) || void 0 === t || t.update(), n.label = 3;  // :209
case 3: return this._lastDataModifiedTime = e, [2];                         // :211
```

So an external `data.json` edit **re-renders the open settings tab even for a plugin that
defines no `onExternalSettingsChange` at all** (the `settingTab.update()` at `:209` is outside
the guard). `Plugin.settingTab` is set by `addSettingTab` (`:70`) and nulled by its cleanup
(`:71`). The internal-plugin twin `C4.handleConfigFileChange` is line-for-line the same logic
with `lastSave` instead of `_lastDataModifiedTime` (`app/own/1939-m4.js:613-627`) — **confirming
the two paths are copy-pasted, not shared.**

`getModifiedTime` returns `0` on any throw (`app/own/1938-c4.js:181-198`, `:193`) — so a
transient stat failure makes `0 > stamp` false and the change is **silently missed**.

### 8. NEW — internal (core) plugins: a different, buffered registration model, and a `deleteData` that community plugins do not have

`C4` (`app/own/1939-m4.js:545-691`) does **not** register on `registerX`. It buffers into arrays
(`commands`, `ribbonItems`, `mobileFileInfo`, `views = {}`, plus a `hasStatusBarItem` boolean)
and `enable()` (`:553-572`) applies the whole set, idempotently (`if (this.enabled) return`),
**every time it is enabled**. Order in `enable`: `enabled = true` → re-add all commands (`:560`)
→ re-add ribbon (`:561`) → mobile file infos (`:562-563`) → status bar item, with the **same
`"plugin-" + id.toLowerCase().replace(/[^_a-zA-Z0-9-]/, "-")` class** as `Plugin`
(`app/own/1938-c4.js:58` vs `app/own/1939-m4.js:564`) → re-register views (`:564`) → `instance.onEnable?.(app,this)`
(`:565`) → `e && instance.onUserEnable?.(app)` (`:569`) → **`this.load()`** (`:569`, the
*Component* load, not `loadData`) → `manager.requestSaveConfig()` → `manager.trigger("change", this)`.

`disable(e)` (`:573-594`): `instance.onDisable?.()` then **`e && instance.onUserDisable?.()`**
(`:580`). ⇒ **core plugins have an `onUserDisable` hook that community `Plugin` does not
provide** — a real asymmetry in the lifecycle surface. Then remove commands, detach buttons,
`leftRibbon.removeRibbonAction`, detach status bar, unregister views and `e &&
detachLeavesOfType` (`:592`), `unload()`, save, trigger.

Storage differs by path, not by method:
- community: `readPluginData(manifest.dir)` → `<configDir>/plugins/<id>/data.json`
  (`app/own/1129-UD.js:158-163`)
- core: `readConfigJson(instance.id)` → `<configDir>/<id>.json`
  (`app/own/1129-UD.js:164-169`, called `app/own/1939-m4.js:633`)
- **`C4.deleteData()` → `deleteConfigJson(id)` → `adapter.remove`** (`:650-659`,
  `app/own/1129-UD.js:170-172`). **`Plugin` has no `deleteData`** — it is not in the 26-member
  prototype (§A). There is therefore **no supported way for a community plugin to delete its own
  `data.json`**; the doc's §3 "plugin authors call `adapter.remove` manually" is the only route,
  which is precisely the kind of asymmetry a native ABI should flatten.

Two more internal-plugin-only facts:
- `registerGlobalCommand(e)` prefixes only the **name**, never the id (`:663-664`), unlike
  `Plugin.addCommand` which rewrites **both** (`app/own/1938-c4.js:63`). So core commands can
  collide; plugin commands cannot collide *because* they are prefixed.
- `C4.registerCliHandler(e,t,n,i)` calls `app.cli.registerHandler(e, t, n, i)` **without** the
  `"[<name>]: "` help prefix that `Plugin.registerCliHandler` prepends (`:687` vs
  `app/own/1938-c4.js:133`). Same registry, inconsistent help text.
- `E4` (`:692`) is the **26-item list of core-plugin ids** the `core-plugins` array
  migration walks: `file-explorer, global-search, switcher, graph, backlink, outgoing-link,
  tag-pane, page-preview, daily-notes, templates, note-composer, command-palette,
  slash-command, editor-status, starred, markdown-importer, zk-prefixer, random-note, outline,
  word-count, slides, audio-recorder, workspaces, file-recovery, publish, sync`.
  `enable()` (`:729-750`) reads `core-plugins`; **if it is an array** it also reads
  `core-plugins-migration` and uses *that object* as the config, then fills
  `i[id] = new Set(core-plugins).has(id)` for every id in `E4` that has a loaded plugin; if
  `core-plugins` is an object it is used directly. Any plugin key absent from the config is
  enabled **iff `instance.defaultOn`** (`:744`) — a field with no `Plugin` counterpart.
  ⇒ `plugin.md` §15's "31-ish built-ins" is wrong on the number for this list.

### 9. NEW — CORRECTION: `addCommand` does **not** produce a duplicate-id error

`plugin.md` §8 states:

> **Every `register*`/`add*` calls `this.register(cleanupFn)` exactly once per registration.**
> […] `addCommand` mutates its argument. […] A second `addCommand` call with the same object
> re-mutates (`"<id>:<id>:<id>"`), typically producing a duplicate-id error from `app.commands.addCommand`.

The first sentence holds. The second half is **wrong**: `commands` is a plain object and
`addCommand` has no duplicate check.

```js
e.mobileOnly && !rd.isMobile || ( …, this.commands[e.id] = e, e.hotkeys && …, this.trigger("changed"));  // app/own/1963-s5.js:7-22
```
⇒ re-adding the same object **silently overwrites** the previous entry (and re-writes its
default hotkeys under a longer, multiply-prefixed id). Contrast the four registries that *do*
throw, which is the real inconsistency worth documenting:

| Registry | Duplicate behaviour | Site |
|---|---|---|
| commands | **silent overwrite** | app/own/1963-s5.js:22 |
| view types | throws `'Attempting to register an existing view type "<t>"'` | `app/own/1925-P6.js:19-22` |
| file extensions | throws, **after** validating every element (two-pass, atomic) | app/own/1925-P6.js:26-35 |
| code-block processors | throws `"Code block postprocessor for language <l> is already registered"` | `app/own/1506-TW.js:735-738` |
| `obsidian://` actions | throws `'Action "<a>" is already registered as a handler.'` | `app/own/1928-Y6.js:7-9` |
| CLI handlers | throws `'Command "<f>" is already registered as a handler.'` | app/own/1225-SL.js:6-13 |
| Bases formula functions | **silent overwrite**, keys lowercased | `app/own/1656-LK.js:37-46` |

### 10. NEW — `CommandSpec` is missing four fields, and the availability chain is the real contract

`plugin.md` §2's `CommandSpec` block is right about the fields it lists but incomplete. The
synthesis of `addCommand` (`app/own/1963-s5.js:7-22`) plus `printHotkeyForCommand`:

| Field | Effect | Site |
|---|---|---|
| `mobileOnly` | whole command skipped off-mobile | `:22` |
| `allowPreview` | without it, an editor command is **unavailable in reading mode** | `:12` |
| `allowProperties` | without it, unavailable while focus is in `.metadata-container` | `:13` |
| `showOnMobileToolbar` | adds to `editorCommands` even without an editor callback | `:22` |
| `hotkeys` | `addDefaultHotkeys(id, hotkeys)` | `:22` |

And the synthesized `checkCallback` for `editorCallback`/`editorCheckCallback`, in order:

1. `if (!activeEditor) return null` — no active editor ⇒ unavailable (`:10-11`).
2. `allowPreview || "preview" !== i.getMode()` (`:12`).
3. For a `MarkdownView`: return `null` if `inlineTitleEl.isActiveElement()` **or**
   `titleEl.isActiveElement()` — **editor commands are dead while the inline title has focus**
   (`:15-17`).
4. `!allowProperties && activeDocument.activeElement.closest(".metadata-container")` ⇒
   unavailable (`:13`).
5. `editorCheckCallback(checking, editor, view)` if present, else
   `checking || (editorCallback(editor, view), true)` — **the callback only fires on the
   non-checking pass** (`:20`).

Two further registry facts: `listCommands()` wraps `checkCallback(true)` in a try/catch that
**logs and hides** a throwing command (`:27-35`) — a broken `checkCallback` makes a command
vanish from the palette rather than error visibly; and `executeCommand` sets `app.lastEvent`
then calls the shared invoker inside a try/catch returning `false` (`:39-47`).

### 11. NEW — CORRECTION: `registerCliHandler`'s signature, and §13 Q3 answered

`plugin.md` §2 infers `registerCliHandler(flag, helpText, handler, extra)` and §13 Q3 asks what
the fourth argument is. Both wrong. The registry stores `{handler, description, flags}`
(app/own/1225-SL.js:6-13), and the callers pass an **options object third, a function fourth**:

```js
t.registerCliHandler("search", "Search vault for text", {        // app/own/1848-Y1.js:273
    query:  { value: "<text>",  description: "Search query", required: !0 },
    path:   { value: "<folder>", description: "Limit to folder" },
    limit:  { value: "<n>",     description: "Max files" },
    total:  { description: "Return match count" },
    case:   { … } }, function(t) { … });
```
and `null` is a legal third argument: `t.registerCliHandler("bases", "List all base files in vault", null, function() {…})`
(`app/own/1972-Z5.js:1595`).

⇒ **signature is `(flag, helpText, flags, handler)`**, where `flags` is
`Record<string, {value?: string; description: string; required?: true}>`. A native ABI can carry
exactly this descriptor and get declarative CLI help for free.

**§13 Q3 answered:** the fourth argument is the **handler function itself**, and it doubles as
the ownership token for unregistration:
```js
e.prototype.unregisterHandler = function(e, t) {
  var n = this.handlers, i = n.get(e);
  (!t || i && i.handler === t) && n.delete(e);
};                                                                   // app/own/1225-SL.js:24-28
```
`Plugin.registerCliHandler`'s cleanup is `app.cli.unregisterHandler(flag, handler)`
(`app/own/1938-c4.js:134`) — identity-checked, so **a plugin can never unregister another's
CLI handler**. (Contrast the URI handler, §12, where the same shape has the bug.)

Bonus from the same file: the CLI has a full plugin-management vocabulary —
`plugin` (info), `plugins`, `plugins:enabled`, `plugin:enable`, `plugin:disable`, `plugin:reload`,
`plugin:install`, `plugin:uninstall` (`app/own/1225-SL.js:626` `plugin`, `:644` `plugins`, `:683` `plugins:enabled`, `:2146` `plugin:enable`, `:2162` `plugin:disable`, `:2215` `plugin:reload`, `:2249` `plugin:install`, `:2299` `plugin:uninstall`). The **id-namespacing rule
for the collision case** lives at `:2163-2165`: with no `filter`, an id present in *both*
namespaces resolves to **core** (`n = i ? "core" : r ? "community" : null`, where `i` is
`internalPlugins.getPluginById(e)` and `r` is `plugins.manifests[e]`); `filter=core|community`
disambiguates. `plugin info` prints tab-separated `type`, `name`, `version`, `author`, `enabled`,
optional `description` (`:626-646`); `plugins` merges core `{id}` and community `{id, version}`
rows and sorts by `Intl.Collator({numeric:true}).compare` on the id (`:657-682`).

### 12. NEW — the URI-handler registry has an inverted ownership check, and clears its trust list on construction

`plugin.md` §10 says the delegate is `workspace.registerObsidianProtocolHandler(action, handler)`
at `workspace/Workspace.js:3839-3844`. In 1.13.7 `Plugin` reaches past that method into the
registry object directly (`app/own/1938-c4.js:121-125`) — `this.app.workspace.protocolHandler`
is constructed at `app/own/1932-r4.js:10`, and the `Workspace` wrappers are at `:2005-2008`.
Functionally equivalent; worth noting because the plugin path and the core path no longer share a
single chokepoint.

The registry itself (`Y6`, `app/own/1928-Y6.js:2-…`) has three things `plugin.md` does not have:

```js
t.saveLocalStorage("trusted-uri-actions", null);                       // :5
e.prototype.unregister = function(e, t) {
  t && this.handlers.get(e) !== t || this.handlers.delete(e);           // :10-11
};                                                                      // ← NOT an ownership check
```

1. **NEW localStorage contract `trusted-uri-actions` is cleared on every construction** (`:5`) —
   and it is *not* `getItem`-scoped to the vault, so a Corbomite URI handler that trusts
   previously-seen actions is stricter than Obsidian by default.
2. **The unregister identity check is inverted** — operator precedence makes it
   `(t && get(e) !== t) || delete(e)`, so **passing a non-matching handler still deletes the
   entry**. Compare `SL.unregisterHandler` (§11), which correctly requires a match. ⇒ A plugin
   calling `registerObsidianProtocolHandler` for an action another plugin owns, then unloading,
   deletes the other plugin's handler. Do not copy.
3. `dispatch(e)` focuses the window first, then honours a bare `obsidian://open` exemption
   (`if ("open" !== t || 1 !== Object.keys(e).length)`, `:29`) — a valueless `open` is
   silently ignored rather than erroring. Unknown action ⇒ `msgInvalidUriAction` toast.
   `execCapacitorUrl` matches the vault name **case-insensitively** and, on mismatch, stashes the
   URL in `sessionStorage["obsidian-uri"]` and **reloads the window** (`:18-23`, stash at `:22`).
4. **§10's list of 11 built-in actions is confirmed exactly** (`app/own/1928-Y6.js:53-199`):
   `open`, `search`, `new`, `show-plugin`, `show-theme`, `show-release-notes`, `debug-info`,
   `publish-sites`, `sync-setup`, `vault-setup`, `hook-get-address`.

### 13. NEW — the two registration verbs' return values differ, and `sortOrder` is written onto your function

- `registerMarkdownPostProcessor(fn, sortOrder)` **returns `fn`** (`app/own/1938-c4.js:92`).
- `registerMarkdownCodeBlockProcessor(lang, fn, sortOrder)` **returns the generated wrapper**,
  not `fn` (`:98`). Anyone keeping the return value gets different objects from the two verbs.
- `registerBasesView` is the only verb returning a **boolean** — `!!n && (… , !0)` where `n` is
  the enabled `bases` internal plugin, else `false` (`:99-103`). §8's claim is confirmed.
- `addRibbonIcon` returns the ribbon **element handle** and registers **two** cleanups in one
  closure — `removeRibbonAction(id)` *and* `handle.detach()` (`:53-55`) — so the ribbon map is
  cleaned even if the element was already detached.
- `addCommand` returns the mutated command object (`:65`); `removeCommand`, `addSettingTab`,
  `registerView`, `registerHoverLinkSource`, `registerExtensions`, `registerGlobalFunc`,
  `registerInstanceFunc`, `registerEditorExtension`, `registerObsidianProtocolHandler`,
  `registerEditorSuggest`, `registerCliHandler` all return `undefined`.

Also confirmed from the rendering addendum's territory but not in `plugin.md`: `sortOrder` is
written **onto the function object** (`e.sortOrder = t`) and the list is re-sorted on every
registration (`app/own/1506-TW.js:723-729`); `EditorSuggestManager` is a **module-load singleton**
pre-seeded with three core suggests before any plugin's (`app/own/1925-P6.js:143-173`),
`addSuggest` pushes, `removeSuggest` uses `Array.prototype.remove`, and `trigger` is
**first-truthy-wins** with the current suggest only being set on the non-checking pass.
Cross-ref `addenda/2026-10-03-rendering-postprocessor-recycler-registries.md` — do not duplicate.

### 14. NEW — §11's status-bar CSS-class quirk is confirmed, and the sanitiser is on this path too

`addStatusBarItem` (`:56-59`):
```js
e.addClass("plugin-" + this.manifest.id.toLowerCase().replace(/[^_a-zA-Z0-9-]/, "-"))
```
Confirmed: **no `g` flag, so only the first illegal character is rewritten** — `a$b$c` yields
`plugin-a-b$c`. One precision the doc gets backwards: the order is **`toLowerCase()` first, then
`replace`**, and since the character class admits uppercase, lowercasing first means fewer
substitutions happen than the doc's "replaced … and lowercased" implies. (Idempotent for
single-illegal-char ids, divergent for two.) §8's "`manifest.id + ':' + title`" ribbon-id rule is
confirmed at `:51`.

Community plugin **details** (readme) is remote markdown pushed through the app's own sanitiser:
`P = GT(WT(D)); A.appendChild(RF(P))`, with `labelNoReadme()` when the fetch fails
(`app/own/1926-z6.js:632-639`). That is the *same* `RF` documented in
`addenda/2026-10-03-rendering-sanitiser-policy.md` — including its
`ALLOW_UNKNOWN_PROTOCOLS: true`. Relevant here only so nobody assumes plugin-supplied remote
content is sanitised differently from plugin-supplied post-processor HTML. It is not.

### 15. NEW — defect: `saveData` cannot report a failed write

`writeJson` swallows write errors with **no logging at all**
(`app/own/1129-UD.js:196-211`):
```js
case 2: return r.sent(), [3, 4];   // success → skip to return
case 3: return r.sent(), [3, 4];   // catch  → skip to return
case 4: return [2];                 // void 0 in both cases
```
So `Plugin.saveData(data)` **resolves successfully on a failed write** (`:152-160`), and so does
`app.plugins.saveConfig` → `writeConfigJson` (`:288-298`). `readJson` by contrast does log —
`console.error("failed to read JSON", path, err)` — and returns `null` only for `"ENOENT"`, else
`undefined` (`:174-194`). This confirms §3's truncation-on-crash paragraph and its "2-space
indent, no trailing newline" claim exactly (`JSON.stringify(t, void 0, 2)`, `:199`), but adds the
point that **the failure is invisible to the plugin**, which is worse than the truncation itself.
A native host should propagate write errors. Both `readJson` and `writeJson` also normalise the
path with `Nl` at the top, so `readPluginData`'s own `Nl(e + "/data.json")` is applied twice —
harmless, but note `getConfigFile` does **not** normalise (`:164`), the one inconsistency in that
family.

### 16. NEW — `PluginSettingTab` is in this file, and the settings modal discriminates by class identity

`plugin.md` §1 hands `PluginSettingTab` to the `settings` domain, which has no doc. It is
`p4` in the same statement (`app/own/1938-c4.js:217-232`), 16 lines:

```js
function t(t, n) { … i.plugin = n, i.name = n.manifest.name, i.id = n.manifest.id; }   // :218-220
t.prototype.getSettingDefinitions = function() { return []; };                          // :222-223
t.prototype.getControlValue = function(e) {
  var t = this.plugin.settings;
  if (t) return t[e];
  console.warn('PluginSettingTab.getControlValue("' + e + '"): this.plugin.settings is undefined. ' +
    'Set this.settings in onload(), or override getControlValue/setControlValue for non-standard storage.');
};                                                                                      // :224-227
t.prototype.setControlValue = function(e, t) { var n = this.plugin.settings; if (n) return n[e] = t, this.plugin.saveData(n); };  // :228-230
```

⇒ the base implementation reads/writes **`this.plugin.settings`**, and *every* control change
calls `saveData` with the whole object. So **the settings tab is the only writer of
`data.json` for most plugins** — which is what makes the 50 ms watcher in §7 fire on ordinary
user interaction, not just external edits. (`saveData` sets `_lastDataModifiedTime` first, so the
self-edit suppression does hold — §3's claim confirmed.)

The internal-plugin variant is `d4` (`:233-247`): `i.plugin = null, i.plugin = n, i.name = n.instance.name, i.id = n.instance.id`,
and its `getControlValue`/`setControlValue` read **`this.plugin.instance.options`** and
**fall back to the base implementation** when `options` is falsy (`:240-246`) — an asymmetric
override that community plugins don't get.

**The settings modal classifies tabs by `instanceof`, not by a marker field**
(`app/own/2066-Die.js:195-197`):
```js
t.prototype.isPluginSettingTab = function(e) { return e instanceof d4 || e instanceof p4; };
```
A setting tab that is neither class lands in the **core `settingTabs`** bucket instead of
`pluginTabs` (`:190`), and consequently in the wrong nav group. Related: `addSettingTab`
**eagerly builds** the tab (`e.update()` at `:190`), gives it a nav element carrying
`data-setting-id = e.id` (`:166`), and attaches a right-click menu offering
`actionRevealPluginInList()` → `openTabById("plugins" | "community-plugins")` then
`.revealPlugin(id)` — whose target list is chosen by that same `instanceof` (`:180-189`).
`removeSettingTab` **closes the tab if it is the active one** (`:191-194`). Core and community
plugin tabs are rendered as two separate labelled groups, each sorted by
`Intl.Collator({numeric:true}).compare` on `name` (`Sw`, `app/own/2066-Die.js:197-213`;
`Sw = Mw`, `app/own/0819-ww.js:228-233`).

The settings-modal popout geometry is `{width: 900, height: 700, minWidth: 600, minHeight: 400}`,
gated on `vault.getConfig("settingsPopoutWindow")` (`app/own/2066-Die.js:145-154`) — recorded
only because it is the container a plugin's settings tab renders into.

### CONFIRMED — verified against 1.13.7, no new information

| Claim in `plugin.md` | Verdict | 1.13.7 site |
|---|---|---|
| §10's `register*`/`add*` enumeration is **complete** (Pass-2 focus item) | **confirmed — exactly 26 prototype members, all accounted for** | `app/own/1938-c4.js:36-215` |
| §2 `CommandSpec.hotkeys?: Array<{modifiers: string[]; key: string}>` | confirmed; records may also carry `code` | app/own/1963-s5.js:22; `app/own/0833-$w.js:68` |
| §2 `addCommand` rewrites `cmd.id`/`cmd.name` in place; §8 ribbon id is `id + ":" + title` | confirmed | `:63`, `:51` |
| §3 `data.json` = 2-space indent, no trailing newline, no framework `version` field; non-atomic write; corrupt read logs and returns falsy | confirmed | `app/own/1129-UD.js:199`, `:196-211`, `:174-194` |
| §3 `saveData` stamps `_lastDataModifiedTime` **before** the write and passes the same stamp as `mtime` | confirmed | `app/own/1938-c4.js:156-158` |
| §8 `Component.unload` order: children, then `_events` (LIFO), then `onunload()` | confirmed | `app/own/0712-fg.js:56-66` |
| §8 `load()` is idempotent via `_loaded` | confirmed | `app/own/1938-c4.js:42`; `app/own/0712-fg.js:45` |
| §8 `registerCodeMirror` is a no-op | confirmed — empty body | `app/own/1938-c4.js:116` |
| §8 `registerBasesView` is the only boolean-returning verb | confirmed | `:99-103` |
| §8 `saveData` wins over concurrent external writes (last-writer-wins, no merge) | confirmed | `:152-160` |
| §10 `registerView` cleanup detaches leaves **iff** `_userDisabled` | confirmed | `:76` |
| §10 `registerCodeMirror` / duplicate `lang` throws | confirmed | `:116`; `app/own/1506-TW.js:735-738` |
| §10 `registerExtensions` validates every element before assigning | confirmed | app/own/1925-P6.js:26-35 |
| §10 `registerEditorSuggest` delegates to a `EditorSuggestManager`, insertion-order first-wins | confirmed | `app/own/1925-P6.js:147-165` |
| §12 `registerEditorExtension` is a 1:1 wrapper onto one flat shared list | confirmed | `:116-120`; app/own/1932-r4.js:2038-2041 |
| §4 post-processors fire `post-processor-change` on **both** register and unregister | confirmed — 4 trigger sites, one per (register/unregister) × (post-processor/code-block) | `:90-91`, `:96-97` |
| §3 `community-plugins.json` is a JSON array of enabled ids | confirmed, with write cadence: `Ml(saveConfig, 1000)` → `writeConfigJson("community-plugins", Array.from(enabledPlugins))` | `app/own/1939-m4.js:6`, `:288-298` |
| §3 safe mode is `localStorage["enable-plugin-" + appId] === "true"` | confirmed | `:310-311` |
| §1 `onExternalSettingsChange` is referenced but **not** defined on the prototype | confirmed — absent from the 26 members | `app/own/1938-c4.js:36-215` |
| §10 `HoverLinkSource` is stored raw and read by Page Preview | confirmed + the trigger event: `hoverLinkSources[e] = t; this.trigger("hover-link-sources-changed")` | `app/own/1932-r4.js:2009-2012` |
| §10 `post-processor-change` reaches `MarkdownPreviewView` | deferred — owned by the rendering addendum | see `2026-10-03-rendering-postprocessor-recycler-registries.md` |

### §A — citation remap: every dead `plugin.md` reference

The extraction `obsidian/plugin/Plugin.js` does not exist in 1.13.7. Map by method name:

| `plugin.md` `Plugin.js:<line>` | 1.13.7 |
|---|---|
| `35-44` ctor | `app/own/1938-c4.js:32-36` |
| `48-68` `load` | `:36-48` |
| `69` `onload` | `:49` |
| `70-79` `addRibbonIcon` | `:49-55` |
| `81-94` `addStatusBarItem` | `:56-60` |
| `96-106` `addCommand` | `:61-65` |
| `108-110` `removeCommand` | `:66-67` |
| `111-116` `addSettingTab` | `:68-72` |
| `118-124` `registerView` | `:73-77` |
| `122-123` leaf detach | `:76` |
| `126-131` `registerHoverLinkSource` | `:78-82` |
| `133-139` `registerExtensions` | `:83-87` |
| `140-151` post-processor | `:88-92` |
| `144,147` `post-processor-change` | `:90, :91` |
| `152-170` code-block processor | `:93-98` |
| `162,166` `post-processor-change` | `:96, :97` |
| `171-181` `registerBasesView` | `:99-103` |
| `182-188` `registerGlobalFunc` | `:104-109` |
| `189-195` `registerInstanceFunc` | `:110-115` |
| `196` `registerCodeMirror` | `:116` |
| `197-203` `registerEditorExtension` | `:116-120` |
| `204-213` protocol handler | `:121-125` |
| `214-220` `registerEditorSuggest` | `:126-130` |
| `221-232` `registerCliHandler` | `:131-135` |
| `233-253` `loadData` | `:136-151` |
| `255-271` `saveData` | `:152-160` |
| `272-302` `loadCSS` | `:161-180` |
| `281` the `JD` styles constant | `:167` — the constant is **`nL`**, not `JD` |
| `303` `onUserEnable` | `:181` |
| `304-329` `getModifiedTime` | `:181-198` |
| `331-350` `_onConfigFileChange` | `:199-214` |
| `355-366` `PluginSettingTab` | `:217-232` (`p4`) |
| `367-379` internal `Y0` | `:233-247` (`d4`) |
| `380-382` `Q0`/`X0` | `:248-249` (`f4` = sourcemap regex, `v4` = `"\n/* nosourcemap */"`) |

§15 short symbols: `QW` → **`RK`**, a `FK` instance at `app/own/1656-LK.js:33-69` (globals keyed
by `fn.name.toLowerCase()`; per-type via the value-type prototype chain; **silent overwrite, no
duplicate check**) — *answers §13 Q4*. `JD` → **not** the styles constant; `JD = !1` at
`app/own/1129-UD.js:889` is the sourcemap-retention switch, and the styles constant is
`nL = "styles.css"` at `app/own/1211-JI.js:5` — *answers §13 Q7*. `$0` → **`m4`**,
`app/own/1939-m4.js:2`. `o2` → **`M4`**, `:693`. `Nee` → status bar, `:2048-Fne.js:397`.
`Y6` → **ambiguous**: `Y6` is the URI handler (`app/own/1928-Y6.js:2`), while the ribbon class is
`X6` (constructed `app/own/1932-r4.js:10`). `f0` (`EditorSuggestManager`) → **`L6`**,
`app/own/1925-P6.js:143`. `MarkdownPreviewRenderer` → **`AW`**, `app/own/1506-TW.js:8`.
`MarkdownRenderChild` and the `P8`/`dJ`/… internal-plugin constructor list: **not verified**, see §C.

### §B — the capability surface worth having in a native ABI

Per the doctrine, ignore the JS shape; these are the *semantics* Obsidian actually enforces and
that a C++ ABI could carry.

**Must-have (Obsidian has it; a host without it cannot round-trip a vault):**

1. **Per-plugin data file**, `.obsidian/plugins/<id>/data.json`, path derived from `manifest.dir`
   (vault-relative, no normalisation surprises), **2-space-indented JSON with no trailing
   newline** (`:199`), **atomic write** (Obsidian is not — §15), **errors propagated** (Obsidian
   swallows them — §15).
2. **A last-write stamp with self-edit suppression**: stamp *before* writing, pass the same stamp
   to the writer, compare on the watcher side, fire only on `>` (`:152-160`, `:199-214`). Corbomite
   should keep the *shape* and drop the millisecond-granularity fragility of `Date.now()`.
3. **A debounced external-change watcher per data file**, exact-path, 50 ms trailing, that also
   **refreshes the open settings UI** (`:209`). Three sibling watchers use the identical pattern
   (`data.json`, core `<id>.json`, `hotkeys.json`) — it is the single most repeated design idea
   in this subsystem.
4. **The enable-set as a plain ordered list**, `.obsidian/community-plugins.json`, and **that order
   is the load order** (`:165-171`). Debounced write (1000 ms) with the set mutated *before* the
   unload (`:250-260`).
5. **The manifest-as-intent record**: `id, name, version, minAppVersion, description, author,
   authorUrl, fundingUrl, isDesktopOnly` (§5). These are plain data; a native manifest can carry
   them verbatim and stay interoperable. **`minAppVersion` is load-bearing at install** (§5).
6. **Id-namespaced command ids** `"<plugin-id>:<cmd-id>"` — not cosmetic: the in-app plugin menu
   recomputes "does this plugin contribute commands?" with
   `Object.keys(app.commands.commands).some(e => e.startsWith(t.id + ":"))`
   (app/own/2061-oie.js:871-873), and the hotkeys tab is pre-filtered with `setQuery(t.id)`
   (`:878-881`). Core-vs-community id collision is resolved **core-first** unless the caller
   passes `filter=core|community` (`app/own/1225-SL.js:2163-2165`).
7. **Deterministic registration scopes**: every registration returns an owning scope that
   unwinds in reverse order, children before parent cleanups, `onunload` last — and
   **unloading an unloaded scope is a no-op** (`app/own/0712-fg.js:56-66`).
8. **Disable-closes-my-tabs**: `_userDisabled` distinguishes "the user turned me off" (close
   every leaf of every view type I registered) from "I'm being reloaded" or "safe mode kicked in"
   (don't) — `:76`, `:104`, `:255`, `app/own/1225-SL.js:2230`, `app/own/1939-m4.js:321`.
9. **A capability gate that is actually a gate.** Obsidian's is `Ab.plugins` (`true` / `false` /
   id-array) checked in five places (`:191`, `app/own/1225-SL.js:633/657/696/735`). Corbomite should
   at minimum match that, and can honestly document any per-plugin capability declaration as an
   intentional divergence.

**Worth having, cheap:**

10. **The declarative CLI flag descriptor** `{value, description, required?}` plus a
    table formatter with `json|tsv|csv` output (`:6-29`; flags example `app/own/1848-Y1.js:273`).
11. **The two-phase command availability check** (`checkCallback(true)` to ask, then invoke) with
    the exact gate list in §10 — including "unavailable while the inline title or the properties
    block has focus", and "reading mode requires an explicit opt-in".
12. **Multiple accelerators per command**, custom-over-default precedence, `code`-vs-`key`
    normalisation, and a fixed modifier display order (`:57-76`, `app/own/0746-ay.js:15`).
13. **A URI/command registry with duplicate rejection and identity-checked unregistration** — but
    fix `app/own/1928-Y6.js:10-11` before copying it (§12).
14. **`post-processor-change`-style invalidation**: any registry mutation that alters render
    output triggers a re-render signal, on register *and* unregister (`:90-91`, `:96-97`).
15. **A source-rewriting back-channel for block processors** (`ctx.replaceCode`), which needs
    per-block source-offset tracking — deferred to
    `addenda/2026-10-03-rendering-postprocessor-recycler-registries.md`, but it is a
    plugin-facing capability, so it belongs on the native-ABI wishlist.

### §C — not applicable to Corbomite by design (web delivery)

Everything in this section is a consequence of plugins being JavaScript bundles loaded by
`window.eval`. **Do not implement, and do not treat its absence as a gap.**

- **`window.eval` of the plugin body**, with a `//# sourceURL=plugin:<encodeURIComponent(id)>`
  marker (`app/own/1939-m4.js:87-89`). The entire security model rests here; there is no sandbox,
  no permission check, no isolated realm.
- **The CommonJS shim.** `s(e)` = deprecated-package map → namespaced map → mobile → `sf(e)`
  (a *real* `require`, so desktop plugins can pull in arbitrary bundled Node modules). On mobile
  it toasts `"<id> attempted to load NodeJS package: "<pkg>""` and returns `null`
  (`:84`). Deprecated CM6 packages are remapped with a stack-trace-begging console error
  (`app/own/1934-o4.js:2`), and `tree.iterate` is syntax-shimmed (`s4`, `:4`).
- **`c4`, the namespaced module map** (`:2-16`) — 13 entries: `obsidian`, and
  `@codemirror/{autocomplete,collab,commands,language,lint,search,state,text,view}`,
  `@lezer/{common,lr,highlight}`. **`u4`, the deprecated map** (`:17-30`) — 12 entries:
  `@codemirror/{closebrackets,comment,fold,gutter,highlight,history,matchbrackets,panel,rangeset,rectangular-selection,stream-parser,tooltip}`.
  **Do not copy this map even in spirit** — it is Obsidian's private bundling shim, not a
  contract. While auditing it, note two of its own bugs: **`"@codemirror/state"` and
  `"@codemirror/text"` are mapped to the same binding `e`** (`:8-9`), so a plugin cannot tell
  those two packages apart; and `t.PluginField` is overwritten with a stub such that all four
  members are the *same* object `{from(){}}` (`app/own/1937-expressionstatement.js:2-7`,
  `app/own/1936-l4.js:2-4`), disabling the CM6 `PluginField` API outright.
- **Inline-sourcemap stripping.** `f4` (`/^\/\/[@#] sourceMappingURL=data:application\/json(?:;charset[:=][^;]+)?;base64,.*$/gm`)
  and `v4` (`"\n/* nosourcemap */"`) at `:248-249`. Load: `JD || (r = r.endsWith(v4) ? r : r.replace(f4, ""))`
  (`app/own/1939-m4.js:83`), where `JD = !1` (`app/own/1129-UD.js:889`) — i.e. a global flag to
  keep sourcemaps for debugging. Install: `d.replace(f4, "") + v4` (`:415`), always appending the
  marker. Note the mismatch: load strips *or keeps* the marker, install *always writes* it.
- **`exports.default || exports`** and `instanceof h4` as the two validity tests (`:89-90`).
- **GitHub-release installation.** `Py(repo, tag, file)` → `https://github.com/<repo>/releases/download/<tag>/<file>`
  (`app/own/0760-Py.js:2`); `Dy(repo, file, ref)` → `https://raw.githubusercontent.com/<repo>/<ref||HEAD>/<file>`
  (`app/own/0757-xy.js:2-3`, `app/own/0758-Dy.js:2`); registry
  `obsidianmd/obsidian-releases/{community-plugins.json, community-plugin-stats.json, community-plugin-deprecation.json}`
  (`app/own/1211-JI.js:6-13`, `app/own/1939-m4.js:537`); community page
  `https://community.obsidian.md/plugins/<id>` (app/own/2061-oie.js:887-889); deep link
  `obsidian://show-plugin?id=<encoded>` (`app/own/1926-z6.js:621`). **A native host should
  reimplement the URL *shapes* only if it wants registry interop; the transport is irrelevant.**
- **Remote deprecation polling** every 12 h and auto-update on focus every 30 days
  (`:173-178`) — Obsidian's update channel, not a plugin-API concern.
- **The 3-second plugin-hang watchdog with a full `window.location.reload()` escape hatch**
  (`:133-158`), and `execCapacitorUrl`'s `sessionStorage["obsidian-uri"]` + reload (`:18-23`).
- **App restart / vault reload as the only "hot reload"** (`reload`, `restart`,
  `"Reload app"`, `"Reload app without saving"`, `"Reload app in Restricted Mode"` at
  `main/own/0033-Ke.js:945-946`; `app/own/1225-SL.js:620-625`). There is **no `reload-plugins`
  command and no hot-reload path in 1.13.7** — see §D.
- **`window.request` / `window.requestUrl` as bare globals** (`app/own/0776-expressionstatement.js:2`;
  `Vy(e) = By(Ny(e))` at `app/own/0768-Vy.js:2`). Not on `App`, not gated, not namespaced per
  plugin. **The transferable fact is the absence of any gate**: a native host that grants a
  plugin network access grants it unconditionally, exactly as Obsidian does.

### §D — CORRECTION: `manifest.version` is not used by a "reload-plugins UI"

`plugin.md` §2 annotates `version: string; // semver string (used by reload-plugins UI; see
ViewRegistry.js:386-387)`. In 1.13.7 `version` is consumed in exactly four places, none of them
a reload UI: the deprecation check (`:193`), the two hardcoded version refusals (`:196`, `:202`),
the exact-match `better-pdf-plugin` refusal (`:202`), the update comparison `Uy(u, f)`
(`:490`), plus display in the actions menu, `plugin info`, and `plugins --versions`.
`grep -rni "reload.{0,4}plugin"` over the app bundle returns **nothing**; there is no
`reload-plugins` command and no `reloadPlugin` method. The nearest thing is the CLI
`plugin:reload id=<plugin-id>` (§3), which is a disable → `loadManifests()` → enable cycle, not
a hot reload. `ViewRegistry.js:386-387` does not exist; the ViewRegistry is
`app/own/1925-P6.js:2-56`.

### §E — Obsidian's own inconsistencies, collected (worth *not* copying)

1. **`saveData` cannot fail observably** — write errors swallowed with no log (§15).
2. **A throw from `onload` leaks a half-loaded instance that is never retried** (§4).
3. **`unload()` on a never-loaded component silently skips every queued cleanup**
   (`app/own/0712-fg.js:57`).
4. **The URI-handler's unregister ownership check is inverted** — a mismatched handler still
   deletes the entry (§12).
5. **`addCommand` silently overwrites on duplicate id** while five sibling registries throw (§9).
6. **`c4` maps `@codemirror/state` and `@codemirror/text` to the same object**, and `PluginField`
   is stubbed to a no-op (§C).
7. **`isDesktopOnly` refuses silently** on mobile from the enable path, and the UI only warns
   (§2 gate 5).
8. **Two config-save debounce constants** (1000 ms community, 500 ms internal) for the same kind
   of write (`:6`, `:696`).
9. **`registerCliHandler`'s help prefix is applied for community plugins and skipped for core
   ones** (`:687` vs `app/own/1938-c4.js:133`).
10. **`registerGlobalCommand` prefixes the name but not the id**, unlike `addCommand` (`:663`).
11. **`getConfigFile` is the only path helper in its family that doesn't normalise**
    (`app/own/1129-UD.js:164` vs `:158`, `:161`, `:170`).
12. **`loadCSS` is called by the loader *and* by most plugins, with no idempotence guard** (§1).
13. **`plugin:reload` re-reads every `manifest.json` in the folder** to reload one plugin
    (`app/own/1225-SL.js:2236-2243`).
14. **Manifest-at-HEAD, code-at-tag** for installs, with id agreement checked but the tag chosen
    by a separate heuristic (§5).
15. **`trusted-uri-actions` is cleared on every construction** — a persisted trust list that never
    persists (§12).
16. **The core-plugin list `E4` has 26 ids, not the "31-ish" of §15**, and its array-form
    `core-plugins` migration reads a second file whose shape is undocumented in the plugin doc
    (§8).
17. **Disabled-but-enabled rows render a toggle locked at `true`** — `setValue(!0)` with an
    `onChange` that can never fire (`app/own/2061-oie.js:571-584`).

### §F — citation-quality notes for the next verifier

- **`seeds/plugin.md` has three false leads**, all of which cost time:
  `loadData — app/own/0829-Ww.js:46` is the **custom-CSS core plugin's** `loadData`
  (it reads `enabledCssSnippets` / `cssTheme` / `translucency` from config, no plugin data at
  all); `getManifest — app/own/0829-Ww.js:443` is the **theme** registry fetch; and
  `manifest — app/own/1938-c4.js:34` is the `this.manifest = n` assignment, not a definition.
  The real `Plugin` verbs are the 26 `Plugin.prototype.*` members at `app/own/1938-c4.js:36-215`.
- **`registerView` is not at `app/own/1925-P6.js:19` for `Plugin`** — that is the *ViewRegistry's*
  `registerView`; `Plugin.prototype.registerView` is `app/own/1938-c4.js:73`. The seed's single
  hit per name does not distinguish the wrapper from the delegate, which is the main trap in this
  domain: **every name in this vocabulary appears at both ends.** Grep `Plugin.prototype\.` /
  `t.prototype\.` in the right file rather than trusting `find`.
- **`app.plugins` / `app.internalPlugins` return 0 refs** in the API-path index, as the seed
  notes, yet both are load-bearing. Use `grep -rn "enablePluginAndSave\|disablePluginAndSave"`
  to find the consumer side.
- `Pie = "1.13.7"` (app version) and `Iie = "28.2.3"` sit at `app/own/2066-Die.js:582-583`.

## Why noticed now

Part of the systematic 1.13.7 verification sweep of `docs/obsidian-audit/domains/*.md` against the
restored, partially-renamed corpus. `plugin.md` is unusual in that its Pass-2 doc says so itself
("**Missing**. Lightly in scope") and then §13 files nine questions against a "plugin loader
(not in this domain)" — but the loader *is* in the corpus (`app/own/1939-m4.js`, 1956 lines: the
community manager `m4`, the internal manager `M4`, and the internal per-plugin wrapper `C4`). So
almost every §13 question was answerable, and three claims that read as settled turned out to be
wrong in ways that would have been implemented.

## Action taken

No code changed. This addendum is the reference for a future native plugin-ABI design; nothing in
`libs/` currently implements plugins, and no open cluster plan touches them. Linkage
(`00-taxonomy.md`, `01-markoff-gaps.md`, `02-extension-surfaces.md`) is centralised per the
addenda rules and deliberately not touched here.

Two compat decisions recorded for whoever builds it: **`loadCSS` follows the file-existence gate,
not an author-call gate** (refuting §12, §1) — a Corbomite `loadQss()` should be called
unconditionally after `load()` and no-op when `<plugin-dir>/styles.qss` is absent; and **plugin
writes must propagate errors and be atomic** (§15), which is a deliberate divergence from
Obsidian rather than a compat requirement. If a per-plugin capability declaration is ever
introduced, §2 is the evidence that Obsidian has none and §B item 9 is the argument to write the
divergence down explicitly.