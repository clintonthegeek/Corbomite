# Settings: which setting writes to which on-disk file, the declarative definition layer, and the real tab registry

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/settings.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`).
**Supersedes / extends:** `domains/settings.md` §1, §2, §3, §7, §8, §9, §10, §13; `addenda/2026-10-03-vault-config-split-and-defaults.md` (cross-checked from the settings side); `VAULT-FORMAT.md` §1/§2.
**Relevant cluster plans:** any plan touching `libs/storage/VaultConfig`, the settings dialog, or `.obsidian/*.json` read/write.

Every citation below is `bundle/rel:line` in `RE/obsidian/1.13.7/renamed/src/`.
All `domains/*.md` citations are dead (`utils/apiVersion.js`, `settings/Setting.js`, `_internal.js:NNNNN`
line numbers) — the domain doc was written against a destroyed 1.12.x tree.

---

## Finding

### The headline: the tab → config-key → on-disk-file table

`domains/settings.md` §3 has a "Key settings owned by each built-in tab and their file destinations"
table with seven rows (Editor, Files & Links, Appearance, Hotkeys, Core Plugins, Community Plugins,
Plugin). Three of them are wrong, and the errors split into four kinds:

1. **four keys it names do not exist in 1.13.7** — `lineWrap` (Editor row),
   `nativeTitleBar`, `fontFamily`, `editorFontSize` (Appearance row). Verified by exhaustive
   scan: `rg 'setConfig\("([a-zA-Z0-9]+)"'` and `getConfig\("…"` over the whole corpus yields
   **55 unique literal config keys**, and every one of them is a key of the defaults object
   `ME` at `app/own/0920-eE.js:1174-1237`. `lineWrap` appears in the corpus only as CodeMirror's
   `lineWrapping` facet; `nativeTitleBar`, `editorFontSize` and a bare `fontFamily` config key
   appear **nowhere**.
2. **twelve real keys are missing from the Editor row** (it names 14, of which one is fictional):
   `showInlineTitle`, `propertiesInDocument`, `foldIndent`, `showIndentGuide`, `rightToLeft`,
   `autoConvertHtml`, `livePreview`.
3. **five real keys are missing from the Files & Links row**: `openBehavior`, `focusNewTab`,
   `deleteUnlinkedAttachments`, `userIgnoreFilters`, `uriCallbacks`.
4. **`nativeMenus` is attributed to the wrong tab.** The doc has one "Appearance" row; 1.13.7 has
   **two** tabs, `appearance` (icon `palette`) and `interface` (icon `monitor`, swapping to
   `lucide-tablet` / `lucide-smartphone`), and `nativeMenus` lives in the latter. Five more
   appearance-file keys — `showViewHeader`, `showRibbon`, `slidingSidebar`, `floatingNavigation`,
   `autoFullScreen` — are Interface-tab keys the table omits entirely.

`saveConfig` splits on membership of a **16-string array** at `app/own/0920-eE.js:1173` (counted
programmatically; `addenda/README.md` line 116 says "18-key", which is wrong — see *Correction 8*),
consumed at `app/own/1129-UD.js:144`. The defaults object `ME` has exactly **57** top-level keys;
all 16 array members are among them, so **41 keys materialise in `app.json`** and 16 in
`appearance.json` when Obsidian writes a fully-defaulted config.

#### `app.json` — 41 keys, by owning tab

Tab identity column gives `id` = `SettingTab.id` (the `data-setting-id` written on the nav item,
`app/own/2066-Die.js:166`) and the line that assigns it.

| Tab (id) | id assigned at | definitions at | `app.json` keys it owns |
|---|---|---|---|
| Editor (`editor`) | `app/own/2062-uie.js:5` | `:7` | `defaultViewMode`, `livePreview`, `showInlineTitle`, `readableLineLength`, `strictLineBreaks`, `propertiesInDocument`, `foldHeading`, `foldIndent`, `showLineNumber`, `showIndentGuide`, `rightToLeft`, `spellcheck`, `spellcheckLanguages`, `autoPairBrackets`, `autoPairMarkdown`, `smartIndentList`, `useTab`, `tabSize`, `autoConvertHtml`, `vimMode` |
| Files & Links (`file`) | `app/own/2062-uie.js:471` | `:521` | `openBehavior`, `focusNewTab`, `newFileLocation`, `newFileFolderPath`, `attachmentFolderPath`, `newLinkFormat`, `alwaysUpdateLinks`, `useMarkdownLinks`, `showUnsupportedFiles`, `promptDelete`, `deleteUnlinkedAttachments`, `trashOption`, `userIgnoreFilters`, `uriCallbacks` |
| Interface (`interface`) | `app/own/2062-uie.js:913` | `:924` | `settingsPopoutWindow`, `mobilePullAction`, `mobileToolbarCommands`, `mobileQuickRibbonItem` |
| *(no settings-tab owner)* | — | — | `pdfExportSettings` (written only by the PDF export dialog, `app/own/1961-Q3.js:1291`), `hotkeys` (default `{}`), `types` (default `{}`) |

#### `appearance.json` — 16 keys, by owning tab

| Key | Owning tab | Written at |
|---|---|---|
| `theme`, `cssTheme`, `accentColor`, `enabledCssSnippets` | Appearance (`appearance`) | `app/own/2061-oie.js:23-230` |
| `textFontFamily`, `interfaceFontFamily`, `monospaceFontFamily` | Appearance | `app/own/2061-oie.js:271` |
| `baseFontSize`, `baseFontSizeAction` | Appearance | `app/own/2061-oie.js:277-295` (`key:` at `:282`, `:293`) |
| `translucency` | Appearance | `app/own/2061-oie.js:423-430` |
| `showViewHeader`, `showRibbon`, `slidingSidebar`, `floatingNavigation`, `autoFullScreen` | **Interface** (`interface`) | `app/own/2062-uie.js:927-968` |
| `nativeMenus` | **Interface** | `app/own/2062-uie.js:1021-1030` |

So **the file column of the doc's table is accidentally right** (every key it names does go to the
file it names), but the **key list and the tab attribution are not** — which matters because the
key list is what a Corbomite settings page would enumerate. Separately,
`plugins/<id>/data.json` is the community-plugin path only: core-plugin data lives at
**`.obsidian/<pluginId>.json`**, a whole file class the doc never mentions (see *NEW* below).

#### Value encodings a faithful writer must reproduce

These are the part that actually corrupts a vault if you get them wrong.

**`openBehavior` is a prefixed composite, not an enum.** `app/own/2062-uie.js:527-529`:

```js
l = function() {
  var e, i, r = t.getValue();
  "file" === r && (r = "file:" + (null !== (i = null === (e = n.selected) || void 0 === e ? void 0 : e.value) && void 0 !== i ? i : "")),
  a.app.vault.setConfig("openBehavior", r);
};
```

The dropdown offers `""`, `"new"`, `"file"`, and `"daily"` (the last only when the `daily-notes`
core plugin is enabled, `:541`). On disk: `"daily"`, `"new"`, or **`"file:" + <path>`**. So the
audit's `openBehavior?: 'default' | 'tab' | 'split' | 'window'` union is fabricated in both
directions — none of those four values is reachable. `baseFontSizeAction` is a **toggle**, not a
3-way select (`app/own/2061-oie.js:288-295`), consistent with the vault addendum's boolean-`false`
default correction.

**`attachmentFolderPath` is a four-way discriminated string.** Decode at
`app/own/2062-uie.js:594`, encode at `:598`:

| on-disk value | dropdown value | subfield |
|---|---|---|
| `"/"` | `root` | — |
| `"."` or `"./"` | `current` | — |
| `"./" + X` | `subfolder` | `X` (`t.slice(2)`, `:642`) |
| anything else | `folder` | the value itself (`:644`) |

**The three font keys are comma-joined strings, not arrays.** Write at
`app/own/2061-oie.js:271` (`t.vault.setConfig(i, e.join(","))`), read through
`app/own/2049-zne.js:2`:

```js
function zne(e) {
  return e.replace(/["]/g, "").split(",").map(function(e) {
    return e.trim();
  }).filter(function(e) {
    return e;
  });
}
```

Every `"` is stripped, so a quoted family name containing a comma cannot round-trip. Each font
page also computes `status: "warning"` when any name in the stack is not installed
(`app/own/2061-oie.js:268`).

**`nativeMenus` is tri-state.** Default is `null` (`app/own/0920-eE.js:1203`), the toggle's *read*
side is `my.useNativeMenu` (the live `Menu` singleton, `app/own/2062-uie.js:1025`) and only the
*write* side touches config (`:1026`). `null`/absent means "follow the OS".

**`userIgnoreFilters` normalises empty → `null` on write**, not `[]`
(`app/own/2062-uie.js:738`: `a.app.vault.setConfig("userIgnoreFilters", e.length > 0 ? e : null)`).
The list renders a `Regex` flair (`span.flair.mod-flat`) on any entry longer than 2 chars that
starts *and* ends with `/` (`:765-775`) — purely presentational, but it confirms the regex-not-glob
finding in the vault addendum.

**`pdfExportSettings` grows a key the defaults table doesn't have.** Written shape is
`{includeName, pageSize, landscape, margin, downscalePercent}` (`app/own/1961-Q3.js:1286-1291`);
the default at `app/own/0920-eE.js:1215-1220` has **no `includeName`**. `margin` is persisted as
the raw dropdown string, and re-parsed with `parseInt(n.margin)` at export time.

**`.obsidian` config-dir override is not in the vault at all** — it is
`localStorage[app.appId + "-config"]` (`app/own/2062-uie.js:795`), validated by
`XD.validateConfigDir`, and applied only via a hidden CTA "Relaunch" button →
`qf(app, i)` + `window.location.reload()` (`:797-807`). Same for three other rows: **zoom level**
and **frame style** (`hidden` / `custom` / `native`) are Electron-owned via `electron.webFrame` and
`ipcRenderer.sendSync("frame")` in the Interface tab (`app/own/2062-uie.js:999-1046`, each needing
`app.relaunch()`), and the **custom app icon** is `ipcRenderer.sendSync("set-icon", …)` in the
Appearance tab (`app/own/2061-oie.js:301-427`).

### CORRECTION 1 — the core tab registry: wrong order, wrong ids, and there is no Feedback tab (§7, §13 Q1)

All nine tabs are registered in the `SettingModal` **constructor**, not from a boot module
(`app/own/2066-Die.js:48`):

```js
n.addSettingTab(new zee(t, n)), n.addSettingTab(new sie(t, n)), n.addSettingTab(new Cie(t, n)),
n.addSettingTab(new uie(t, n)), n.addSettingTab(new mie(t, n)), n.addSettingTab(new z6(t, n)),
n.addSettingTab(new u6(t, n)), n.addSettingTab(new Eie(t, n)), n.addSettingTab(new cie(t, n)),
```

| # | ctor | `id` | `name` source | `icon` | assigned at |
|---|---|---|---|---|---|
| 1 | `zee` | `"about"` | `Vee.name()` | `circle-user` | `app/own/2040-Vee.js:7` |
| 2 | `sie` | `"appearance"` | `oie.name()` (`bd.setting.appearance`) | `palette` | `app/own/2061-oie.js:16` |
| 3 | `Cie` | `"interface"` | `wie.name()` (`bd.setting.interface`) | `monitor` / `lucide-tablet` / `lucide-smartphone` | `app/own/2062-uie.js:913` |
| 4 | `uie` | `"editor"` | `bd.setting.editor.name()` | `pen-line` | `app/own/2062-uie.js:5` |
| 5 | `mie` | `"file"` | `vie.name()` (`bd.setting.filesAndLinks`) | `folder-cog` | `app/own/2062-uie.js:471` |
| 6 | `z6` | `"hotkeys"` | `V6.name()` | `command` | `app/own/1926-z6.js:5` |
| 7 | `u6` | `"keychain"` | `c6.name()` (`bd.setting.keychain`) | `key-round` | `app/own/1921-l6.js:62` |
| 8 | `Eie` | `"plugins"` | `bd.setting.builtinPlugins()` | `toy-brick` | `app/own/2062-uie.js:1221` |
| 9 | `cie` | `"community-plugins"` | `lie.name()` (`bd.setting.thirdPartyPlugin`) | `puzzle` | `app/own/2061-oie.js:443` |

The doc's §7 order ("Editor, Files & Links, [Mobile-only tab], Appearance, Hotkeys, Core Plugins,
Keychain, About, [Feedback tab]") is wrong in six of nine positions. There is **no** `interface`
entry in the doc at all, and there is **no Feedback tab** — `fie` is a phone-only feedback
*banner* injected into the sidebar search container (`app/own/2066-Die.js:25`). `interface` is a
distinct entry, not a "[Mobile-only tab]".

**§13 Q1 is answered.** The doc's two confirmed ids hold (`keychain`, `community-plugins`); the
other seven are above. Note `file`, not `files-and-links`, and `plugins`, not `core-plugins` —
though the *sidebar group* carrying the plugin tabs uses `data-section="core-plugins"`
(`app/own/2066-Die.js:35`), which is probably where the doc's name came from.

Three sidebar groups, each with a stable `data-section` (`app/own/2066-Die.js:30`, `:35`, `:40`):
`options`, `core-plugins`, `community-plugins`, titled `bd.setting.options()`,
`bd.setting.builtinPlugins()`, `bd.setting.thirdPartyPlugin.name()`.

### CORRECTION 2 — settings search is one global sidebar search, not per-tab (§9)

The doc's §9 bullet "The user can search settings by typing in the search field **inside certain
tabs** (e.g. Hotkeys, Community Plugins)" inverts the architecture. There is exactly **one**
search box, in the sidebar header (`app/own/2066-Die.js:16-24`), debounced with `Ml(…, 0)`, and it
queries a **cross-tab index** (`app/own/2062-uie.js:1307-1377`, `Mie`).

Indexer rules (`app/own/2063-Sie.js:2`), all of which are behaviour a Corbomite port must match:

- Recurse over `settingItems`; a `page` or group/list whose `visible` evaluates false is
  **pruned entirely** — its children never enter the index.
- A leaf enters only if `visible !== false` **and** `searchable !== false`.
- Entries are `{tab, definition, page, pagePath}`, `pagePath` being the accumulated page names.
- Page *names* are indexed separately (`app/own/2064-xie.js:2`) and contribute a `tabNameMatch`
  to their whole group.
- `aliases: [...]` on a definition are fuzzy-matched alongside `name` and `desc`, and the best of
  the three scores is the entry score (`app/own/2062-uie.js:1343-1352`). Only the Appearance tab
  uses them (`app/own/2061-oie.js:28`, `:44`, `:64`, `:279`), e.g. `oie.aliasBaseTheme()`.

`def.searchable` does double duty: it gates the index *and* opts an item out of an in-list
`search.match` filter (`app/own/1914-t6.js`, the `o.addSearch` branch). Results are grouped by
`page ? page.name : tab.name` and **capped at 3 items per group** with a "show more" row
(`app/own/2066-Die.js:434`, `:443-448`).

### CORRECTION 3 — `display()` is no longer the tab contract; there is a declarative definition layer (§8, §10)

The doc's §10 ("`display()` is the **only required override**") and §8 invariant ("`display()` is
always called after `containerEl` is appended") describe the legacy path. The real base class is
`app/own/1921-l6.js:2-57`:

```js
e.prototype.renderTab = function() {
  this.settingItems.length > 0 ? W2(this) : this.display();
}, e.prototype.display = function() {
  W2(this);
}, e.prototype.hide = function() {}, e;
```

`update()` (`app/own/1921-l6.js:8-9`) rebuilds `settingItems = this.getSettingDefinitions()`, runs
the duplicate-page-name validator `H2`, then `setting.refreshSearch()` and
`setting.refreshCurrentPage(this)`. So a tab overrides **`getSettingDefinitions()`**, and
`display()` is only reached when that returns `[]`.

**Consequence worth writing down:** the search index walks `tab.settingItems`
(`app/own/2062-uie.js:1319`). A `display()`-only tab therefore contributes **zero** entries to
global settings search. Of the nine core tabs, exactly one is still on the legacy path — Hotkeys
(`z6`, `app/own/1926-z6.js:41`); it has no `getSettingDefinitions` anywhere in the corpus.

**§13 Q3 answered:** `hide()` is a no-op stub on the shared base, and `SettingModal.openTab` /
`closePage` / `clearPageStack` wrap every call in `try { … } catch (e) { console.error(e) }`
(`app/own/2066-Die.js:252-256`, `:337-341`, `:364-370`), so a throwing override degrades to a
console error rather than breaking navigation.

#### The `SettingDefinition` shape

Built by `app/own/1920-s6.js`, indexed by `app/own/1913-e6.js`/`1914-t6.js`/`1915-n6.js`:

```
type?: "group" | "list" | "page"
name?, desc?, cls?, heading?, icon?, aliases?: string[]
visible?: boolean | (() => boolean)      // evaluated by U2, app/own/1903-U2.js:2
disabled?: boolean | (() => boolean)     // def.disabled ?? def.control.disabled
searchable?: boolean | (() => boolean)
items?: SettingDefinition[]
control?: { type, key, defaultValue?, options?, placeholder?, rows?,
            min?, max?, step?, filter?, includeRoot?, displayFormat?, validate? }
render?: (setting: Setting, group: SettingGroup) => (() => void)   // return value = cleanup
action?: (settingEl, index) => void
displayValue?, status?: "warning"
search?: { placeholder?, match(def, query) }
addItem?: { name, action(settingEl) }, onDelete?(index), onReorder?(from, to), emptyState?
cleanup?                                    // internal slot, set from render()'s return
```

- **`control.type` is a closed set of 10** — `toggle`, `dropdown`, `text`, `textarea`, `number`,
  `file`, `folder`, `slider`, `color`, `secret` (`app/own/1920-s6.js:64-175`). The doc's §1
  builder table has no notion of these.
- **Conditional enablement is first-class**: `X2` (`app/own/1908-X2.js:2`) calls
  `setting.setDisabled(U2(def.disabled ?? def.control.disabled, false))` on every refresh, which
  is what `refreshDomState()` re-runs after each write (`app/own/1921-l6.js:31-33`).
- **A group with no visible `def` child is hidden**, not rendered empty
  (`app/own/1907-Y2.js:2`: `U2(i.def.visible, !0) && (!r || o)`).
- **Bare items are auto-grouped**: any run of leaf defs is wrapped in a synthesised
  `{type:"group"}` (`app/own/1913-e6.js:2`), so every settings row lives inside a `.setting-group`.
- **Re-render identity of a group is its heading text**, not its position:
  `type + ":" + heading`, falling back to `type + "#" + index` (`app/own/1910-$2.js:2`). Leaf
  identity is `ctrl:<key>` / `name:<name>` / `item#<index>` (`app/own/1909-Q2.js:2`). Duplicate
  leaf keys log ``<tabName>: duplicate setting key "X". This may cause unexpected behavior when
  re-rendering.`` (`app/own/1912-J2.js:2`).
- **In-list search re-filters after a re-render** by re-dispatching `input` on the group's search
  box (`app/own/1902-j2.js:2`).
- **`containerEl` is emptied only on the first render** — `0 === prev.length && containerEl.empty()`
  (`app/own/1913-e6.js:2`) — which is what lets `update()` rebuild in place without flicker.
- **`list` defs** add an inline `mod-add-item` row on mobile but a header `lucide-plus` extra
  button on desktop (`app/own/1914-t6.js`); `onReorder` gets **both** a SortableJS drag
  (`childSelector: ".setting-item:not(.mod-add-item)"`, `handleSelector: ".mod-drag-handle"`) and
  Alt+Arrow keyboard reorder; `Delete`/`Backspace` on a row triggers `onDelete`
  (`app/own/0817-Yb.js:137-167`).
- **`validate` is async and gates the write**: an error string calls `setting.setErrorMessage(…)`
  and `setControlValue` is **not** called (`app/own/1920-s6.js:36-48`).
- **`slider` and `color` controls get an auto "restore default" `lucide-rotate-ccw` extra button**
  when `defaultValue !== undefined`, and the button sets `aria-disabled="true"` on itself while
  the current value equals the default (`app/own/1920-s6.js:60-63`, `:140-152`, `:156-168`).
  The tooltip string is `bd.setting.hotkeys.tooltipRestoreDefault()` — a hotkeys-namespace reuse.
- **`folder` controls display `"/"` as `""`** and attach a path suggester
  (`app/own/1920-s6.js:129-137`) — the UI reason `attachmentFolderPath`'s default looks blank.

### CORRECTION 4 — `community-plugins.json` absence is *not* safe mode (§2, §3, §8)

The doc says this three times (§2 line 211, §3 line 239, §8 line 315). Safe mode is a
**`localStorage` flag**, per app id, entirely outside the vault
(`app/own/1939-m4.js:310-312`):

```js
t.prototype.isEnabled = function() {
  return "true" === localStorage.getItem("enable-plugin-" + this.app.appId);
},
```

`community-plugins.json` absence just means an empty enabled set. Read at
`app/own/1939-m4.js:124`:

```js
(t = u.sent()) && Array.isArray(t) || (t = []), this.enabledPlugins = new Set(t),
```

— note the precedence: a **non-array** value (object, string, `null`) is silently discarded and
replaced with `[]`. Write at `:293`: `writeConfigJson("community-plugins", Array.from(this.enabledPlugins))`
from a `Set`, so the file is always insertion-ordered and **`Array.from` order is the plugin load
order** (`:329`). Enable/disable go through `requestSaveConfig` = `Ml(saveConfig, 1e3)`
(`app/own/1939-m4.js:6`); `clearEnabledPlugins()` bypasses the debounce and writes `[]` (`:304`).

**§13 Q5 answered: order is significant** — insertion order of a `Set`, persisted verbatim, and
consumed in file order at load. A Corbomite writer must **not** sort this array.

The Community Plugins tab renders a completely different definition set in safe mode
(`app/own/2061-oie.js:461-465`: `isEnabled() ? buildEnabledSettings() : buildRestrictedModeSettings()`).

### CORRECTION 5 — `core-plugins-migration.json` is never deleted, and its values are discarded (§2, §3)

The doc §3 says it is "deleted after migration completes (not observed explicitly…)". It is not
deleted — the corpus contains exactly three references to the filename: the read at
`app/own/1939-m4.js:737`, and two Sync-classifier arms at `app/own/2043-Xte.js:314`.

The array-format branch is `app/own/1939-m4.js:733-745`:

```js
[4, t.vault.readConfigJson("core-plugins")];
case 1: return (r = p.sent()) ? Array.isArray(r) ? [4, t.vault.readConfigJson("core-plugins-migration")] : [3, 3] : [3, 4];
case 2: for ((o = p.sent()) && "object" == typeof o && (i = o), a = new Set(r), …)
             u = l[s], n.hasOwnProperty(u) && (i[u] = a.has(u));
```

The migration object is seeded into `i` **first**, then `i[u] = a.has(u)` runs for **every**
registered core-plugin id in `E4` (`app/own/1939-m4.js:692`, 26 ids) — so for any id that is a
*currently registered* core plugin, the array wins and the migration file's value is thrown away.
The migration file only survives for ids that no longer exist as core plugins. It is not a merge.

Two more load-bearing details:

- **`E4` is the canonical core-plugin id list**, and it is the thing the migration iterates:
  `["file-explorer", "global-search", "switcher", "graph", "backlink", "outgoing-link",
  "tag-pane", "page-preview", "daily-notes", "templates", "note-composer", "command-palette",
  "slash-command", "editor-status", "starred", "markdown-importer", "zk-prefixer",
  "random-note", "outline", "word-count", "slides", "audio-recorder", "workspaces",
  "file-recovery", "publish", "sync"]` (`app/own/1939-m4.js:692`).
- **Merely omitting a plugin from the file schedules a write at boot.** `app/own/1939-m4.js:744`:
  for an id with no file entry, it enables `defaultOn` and calls `this.requestSaveConfig()`
  (500 ms debounce, `app/own/1939-m4.js:696`). So opening a vault in Corbomite and saving
  nothing can still produce a `core-plugins.json` Obsidian didn't have before — matching this
  matters for round-trip tests.
- `saveConfig` (`app/own/1939-m4.js:751-763`) iterates registered plugins and writes `i[o] = n[o].enabled`, so
  **unknown ids present in the file are round-tripped unchanged**, and the enabled-id array it
  builds (`r`) is never written — the output is always object form, self-healing the array shape.

### NEW — `.obsidian/<corePluginId>.json`, a file class the doc omits

Core-plugin data is **not** `plugins/<id>/data.json`. `C4.loadData` / `C4.saveData`
(`app/own/1939-m4.js:633`, `:642`):

```js
return e = this.instance.id, [2, this.manager.app.vault.readConfigJson(e)];
…
t = this.instance.id, n = this.manager.app, i = Date.now(), this.lastSave = i, [4, n.vault.writeConfigJson(t, e, { mtime: i })];
```

External-change detection matches on **direct children of `configDir`**
(`app/own/1939-m4.js:698-705`): `Pl(e) === this.app.vault.configDir && "json" === Fl(e)` then
`Ll(e)` as the plugin id. So the complete on-disk inventory for plugin state is:

| Path | Owner | Write |
|---|---|---|
| `.obsidian/<corePluginId>.json` | internal plugins (`M4`/`C4`) | 500 ms debounce, boot-scheduled for absent keys |
| `.obsidian/plugins/<id>/data.json` | community plugins | `requestSaveConfig` on enable/disable; `Plugin.saveData` on every change |
| `.obsidian/plugins/<id>/{manifest,main,styles}.json\|js\|css` | both | install |

The Sync classifier agrees: a **single-segment** `.json` inside `configDir` is `core-plugin-data`
(`app/own/2043-Xte.js:314`, the `1 === a.length && "json" === l` arm), which is the `.obsidian/<id>.json`
class, distinct from the three-segment `plugins/<id>/data.json` → `community-plugin-data`.

### NEW — the config-dir override and three Electron-owned Interface rows

Already noted above (localStorage, `app/own/2062-uie.js:791-807`). Also:

- **The settings modal opens in a popout *window* by default.** `shouldUsePopout()` is
  `this.app.vault.getConfig("settingsPopoutWindow")` (`app/own/2066-Die.js:152-154`), whose default
  is `true` (`app/own/0920-eE.js:1224`), overriding the base class's `return !0`
  (`app/own/1925-P6.js:313-315`, which also supplies `getPopoutOptions() → {}`,
  `isInPopoutWindow()` and `getPopoutWindow()`). `getPopoutOptions()` is overridden to
  `{width:900, height:700, minWidth:600, minHeight:400}` (`app/own/2066-Die.js:146-151`). The
  toggle's own `onChange` closes and reopens the modal (`app/own/2062-uie.js:1053-1057`).
  `settingsPopoutWindow` is a **vault** key and therefore lands in `app.json` — one of the 41.
- **Closing the settings modal flushes the pending config save**: `app/own/2066-Die.js:485`
  `this.app.vault.requestSaveConfig.run()`. Corbomite has no equivalent, so a config change made
  in the last second before closing the dialog is lost in Obsidian but not in a naive port.

### CORRECTION 6 — six undocumented `Setting` methods, four undocumented `SettingGroup` ones

`app/own/0817-Yb.js`, `Setting` = `Yb`:

| Method | Line | What it actually does |
|---|---|---|
| `setErrorMessage(msg)` | `:14` | creates `.setting-item-error` in `controlEl`, toggles `is-invalid` on the row |
| `addDisplayValue(cb)` | `:17` | appends a `tw` component (below) to `controlEl` |
| `setIcon(id)` | `:19` | `prepend`s a `.setting-item-icon` to `settingEl` (not to `infoEl`) |
| `setNavigable(cb)` | `:91` | adds `mod-navigable` `tappable`, appends `.setting-item-chevron` with `lucide-chevron-right`, then `setRowClick` |
| `setAction(cb)` | `:95` | adds `mod-action` `tappable`, then `setRowClick` |
| `setRowClick(cb)` | `:97` | **one** delegated listener installed lazily; the handler checks `defaultPrevented` and `this.disabled` before firing |
| — | | `clear()` also resets `errorEl = null` and removes `is-invalid` (`:87`), which the doc's "Empties `controlEl`, resets `components = []`" omits |

`setClass` **splits on whitespace** and adds each token (`:28-30`), so it is multi-class.

`tw` (`app/own/0817-Yb.js:291-301`) is an undocumented component: `.setting-item-value` plus an
optional sibling `.setting-item-status`, which gets `mod-warning` and `lucide-alert-triangle` when
`setStatus("warning")` and `detach()`s on any other value. It is the widget behind the settings
search result counts and the font-page warning.

`sw` (`app/own/0817-Yb.js:405-421`) is a `TextComponent` subclass — `input[type=number]` with
`getValueAsNumber` / `setValueAsNumber` / `setLimits`. It has **no `Setting.addNumber`**; the
declarative `number` control reaches it via `addComponent` (`app/own/1920-s6.js:92-118`), where
`step` defaults to `"any"`, `inputMode` is `"decimal"`, blur/Enter commit, Escape reverts, an empty
field resets to `defaultValue ?? 0`, and errors are `bd.setting.msgInvalidNumber()` /
`msgValueAtLeast({min})` / `msgValueAtMost({max})`.

`SettingGroup` = `Xb` (`app/own/0817-Yb.js:106-181`), undocumented methods (plus the
`addClass` signature):

- `addSearchControl(cb)` (`:131`) — creates a `.setting-group-search-control` div. The Hotkeys
  tab uses it for its filter chips (`app/own/1926-z6.js:58`).
- `onDeleteItem(cb)` (`:137`) — Delete/Backspace on a `.setting-item`, `cb(index)`, then
  `focusNearestItem(index)`.
- `onReorderItem(cb)` (`:150`) — Alt+ArrowUp/ArrowDown, `cb(from, to)`, clamped.
- `focusNearestItem(index)` (`:168`) — forward then backward scan for the first `isShown()` row,
  focusing with `preventScroll` and `scrollIntoView({block:"nearest"})`.
- `addClass` (`:122`) is **variadic** (`arguments`), not the single-arg form the doc gives.

**Correction to the doc's DOM shape:** `div.setting-group-search` is **not** created lazily by
`addSearch()`. It is built unconditionally in the constructor, along with `div.setting-items`
(`app/own/0817-Yb.js:110-115`). Likewise the heading `div.setting-item.setting-item-heading` is
always constructed detached (`createDiv`, `:109`); `setHeading` prepends it when the text is truthy
**or** `controlEl.childElementCount > 0`, and detaches it otherwise (`:117-121`), and `addExtraButton`
force-prepends it (`:134`).

### CORRECTION 7 — two component descriptions are wrong

- **`SliderComponent.setDynamicTooltip()` is an empty stub.** `app/own/0817-Yb.js:549-551` is
  literally `return this;`, and `showTooltip()` at `:561` is `function() {}`. The doc lists
  `setDynamicTooltip()` as a working feature in both §1's table and its `Key methods` column.
  `SliderComponent` also owns a live `span.slider-value` display element (`:506`), fires
  `changeCallback` from `setValue` (`:538-539`), sets a `--slider-fill-ratio` CSS var
  (`:551-560`), and `getValuePretty()` is ``"any" === e.step || parseFloat(e.step) < 1 ? n.toFixed(2) : n.toString()`` (`:542-546`).
- **`DropdownComponent` builds a hidden measuring twin.** Besides `select.dropdown` it creates
  `select.dropdown.is-measuring` with `aria-hidden="true"` and one probe `<option>`, then sets a
  `--dropdown-fitted-width` custom property from the probe's `offsetWidth`
  (`app/own/0817-Yb.js:448-466`). The doc's component table says only `select.dropdown`.
- **`SecretComponent` click-reveals.** `app/own/1921-l6.js:288-329`: `setValue(key)` takes a
  **secret name**, renders `peekSecret(key)` as a literal `"•••••••"` inside
  `span.setting-password`, and a **click on that span toggles to plaintext**. An `lucide-x` in
  `span.setting-hotkey-icon` clears it. The button label flips `Link` ⇄ `Change`. A
  `clickable-icon mod-warning` `lucide-alert-triangle` is shown when
  `secretStorage.isEncryptionAvailable()` is false.

### CONFIRMED — doc claims re-verified, now citable

| Claim (`domains/settings.md`) | 1.13.7 location |
|---|---|
| `new SettingTab(app, setting)`; `containerEl` is a `div.vertical-tab-content`; `navEl` created on first registration | `app/own/1921-l6.js:4` (`createDiv("vertical-tab-content")`), `:160-166` of the modal |
| `hide()` is a no-op stub on the base class | `app/own/1921-l6.js:56` |
| `PluginSettingTab` sets `id`/`name` from `plugin.manifest`, stores `this.plugin`, extends `SettingTab` | `app/own/1938-c4.js:217-232` (`p4`) |
| The internal-plugin tab (`Y0`) takes `plugin.instance.name` / `plugin.instance.id` | `app/own/1938-c4.js:233-240` (`d4`) |
| `isPluginSettingTab` routes plugin tabs to the plugin section; plugin tabs sorted A–Z by `name` on every `updatePluginSection` | `app/own/2066-Die.js:196`, `:205-212` |
| `Plugin.addSettingTab` registers `removeSettingTab` as cleanup | `app/own/1938-c4.js:68-74` |
| `Plugin.saveData` → `writePluginData(dir, data, {mtime})`; `loadData` → `readPluginData(dir)`; absence → `null` | `app/own/1938-c4.js:152-159`, `:136-138` |
| `Plugin._onConfigFileChange` debounced **50 ms**, compares `mtime`, calls `onExternalSettingsChange()` | `app/own/1938-c4.js:34`, `:199-212` |
| `HotkeyManager.save()` → `writeConfigJson("hotkeys", data)`, no debounce | `app/own/0833-$w.js:19-29` |
| `hotkeys.json` re-read on `raw` filtered to `configDir + "/hotkeys.json"` | `app/own/0833-$w.js:16-18` (load itself 50 ms debounced, `:7`) |
| Only user overrides persist; clearing a hotkey **deletes** the entry rather than writing `[]` | `app/own/0833-$w.js:48-53` |
| `core-plugins.json` is `{[id]: boolean}`; a missing id defaults to `defaultOn` | `app/own/1939-m4.js:733-749` |
| `SettingModal.lastTabId` is in-memory only and reopens the last tab | `app/own/2066-Die.js:6` (`lastTabId = ""`), `:483` |
| Settings opens via `Mod+,` | `app/own/2075-Vie.js:678-685` (`Yw(["Mod"], ",")`) |
| `ExtraButtonComponent` default icon `lucide-settings` | `app/own/0817-Yb.js:270` |
| `ToggleComponent` fires `navigator.vibrate(100)` | `app/own/0817-Yb.js:333` |
| `addText` adds Enter-to-blur only when `!hasPhysicalKeyboard` | `app/own/0817-Yb.js:52-59` |

Additional hotkey facts the doc gets structurally right but does not state: the baked matching
record is **normalised at bake time** — `{modifiers: uy.compileModifiers(mods), key: a.code ?
Xw(a.code) : a.key}`, so a record carrying `code` has its `key` derived and loses `code`
(`app/own/0833-$w.js:57-76`); `compileModifiers` maps `Mod` → `Meta`/`Ctrl` by platform, **sorts**,
and joins with `,` into a single string (`app/own/0747-ly.js:112-115`); `Xw` strips a 4-char
`Key` prefix (`app/own/0831-Xw.js:2`); and the on-disk
record is built by `Yw(modifiersArray, key)` → `{modifiers, key}` (`app/own/0830-Yw.js:2`), so
`modifiers` **is** an array on disk even though it is a string in the baked form. `customKeys`
is exposed as a non-enumerable getter returning a copy (`app/own/0833-$w.js:9-14`).

### CORRECTION 8 — the appearance array is 16 keys, and `README.md` says 18

`addenda/README.md` line 116 says "the `app.json`/`appearance.json` partition is an **18-key**
allow-list". The literal at `app/own/0920-eE.js:1173` has **16** members (counted
programmatically), and the addendum file itself says 16. Flagged here because README is
edit-locked by concurrent agents — **do not cite README for the count.**

### Also worth recording

- Version constants live next to the settings modal:
  `Pie = "1.13.7"`, `Iie = "28.2.3"` (Electron) at `app/own/2066-Die.js:582-583`.
- The settings modal (`Aie`, `app/own/2066-Die.js:3-581`) is a popout-capable modal
  (`N6`) with a full keyboard-navigation layer: vim
  `j/k/h/l/g/G`, `/` and `Mod+F` focus the search, `Tab`/`Shift+Tab` walk settings via
  `getTabSequence()` (which unions the row selector with a focusable-element selector),
  `G` jumps to the last row, and `Escape` walks *out* of the content into the sidebar
  (`app/own/2066-Die.js:90-141`, `:500-511`, `:569-579`). The row selector is
  `".setting-item:not(.setting-item-heading), .setting-group-search, .setting-item-heading
  .extra-setting-button"` (`app/own/2066-Die.js:2`).
- **Settings sub-pages are a first-class navigation stack**: `openPage` / `closePage` /
  `pageStack` / `clearPageStack`, a back button `clickable-icon.setting-page-back-button`
  (`lucide-chevron-left`), and a `setting-page-title` (`app/own/2066-Die.js:261-301`). The page
  object is `N2` (`app/own/1895-N2.js`), extending `Qb` — a **different** base from `SettingTab`,
  whose `rootEl` is `setting-page vertical-tab-content` with a `setting-page-titlebar` and a
  `setting-page-content` (`app/own/0817-Yb.js:183-188`). So `containerEl`'s class differs
  depending on whether you are in a tab or a page.
- A page def's row gets `setNavigable` + an optional `addDisplayValue`
  (`app/own/1920-s6.js:7-19`), which is why `optionExcludedFiles` shows a count
  (`app/own/2062-uie.js:780-789`) and `CSS snippets` shows an enabled count
  (`app/own/2061-oie.js:196-200`).
- The `plugins` tab's hotkey button cross-navigates: `openTabById("hotkeys")` then
  `n.setQuery(t)` with the plugin id (`app/own/2062-uie.js:1262-1265`).
- `bd.setting.*` is the live i18n namespace: **252 `bd.setting.` references** across `app/own/`.
  The seed map's "`bd.setting` (0 refs)" is a false negative of its own query pattern.

---

## Why noticed now

The vault-domain pass found the `app.json`/`appearance.json` partition and the defaults table but
explicitly could not say *which tab owns which key* — the settings domain was the natural place to
look, and §3's table there turned out to be wrong in three of its seven rows. Reading the corpus from the
settings side also surfaced something the vault pass structurally could not: 1.13.7 has a
**declarative `getSettingDefinitions()` layer** that sits above the `Setting` builder the audit
documented, with conditional visibility, conditional enablement, async validation, sub-page
navigation, and a cross-tab fuzzy-search index. An implementer following `settings.md` §8/§10 would
build only the builder path — and would then also get global settings search wrong, because a
`display()`-only tab is invisible to it.

## Action taken

- The tab → key → file table above is the normative one for Corbomite. `libs/storage/VaultConfig`
  must split on the **16**-member array at `app/own/0920-eE.js:1173` and must reproduce the value
  encodings for `openBehavior` (`file:` prefix), `attachmentFolderPath` (4-way),
  `interfaceFontFamily` / `textFontFamily` / `monospaceFontFamily` (comma-joined string, `"`-
  stripped on read), `nativeMenus` (tri-state) and `userIgnoreFilters` (`null` when empty).
- Two new on-disk facts for `VAULT-FORMAT.md`: `.obsidian/<corePluginId>.json` is a distinct file
  class from `.obsidian/plugins/<id>/data.json`, and `core-plugins-migration.json` is never
  deleted.
- Safe mode is `localStorage["enable-plugin-" + appId] === "true"`, not the absence of
  `community-plugins.json` — the latter is only an empty set, and a non-array value in it is
  silently discarded by Obsidian on load.
- `community-plugins.json` array order is significant (Set insertion order = load order). Do not
  sort it.
- Nothing here changes the "desktop-UX translation of the settings *widget* design is intended"
  stance. The declarative definition layer is worth mirroring as a *data* contract (it is what
  makes conditional-visibility and cross-tab search tractable); its `control.type` vocabulary maps
  onto Qt widgets; its CSS classes are desktop-UX and are explicitly out of scope.
- The JS plugin API (`addSettingTab`, `PluginSettingTab`, `getSettingDefinitions`) is **not**
  mirrored, per doctrine — but note that a `PluginSettingTab`'s declarative `control.key` resolves
  to `plugin.settings` / `data.json`, not to vault config
  (`app/own/1938-c4.js:224-232`), which is worth remembering if Corbomite ever grows a plugin
  settings API.
