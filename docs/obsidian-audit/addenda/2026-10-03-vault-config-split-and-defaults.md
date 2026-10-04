# Vault config: the real `app.json` / `appearance.json` key split, the defaults table, and the Sync allow-list

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/vault.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`, rebuilt 2026-10-03 from `obsidian_1.13.7_amd64.deb`).
**Supersedes / extends:** `domains/vault.md` §1 (`Vault` → *Config / disk*), §2 (`VaultConfig`, `AppConfig`, `AppearanceConfig`), §3 (`.obsidian/`), §13 Q1 + Q2; `VAULT-FORMAT.md` §1/§2 config sections.
**Relevant cluster plans:** any plan touching `libs/storage/VaultConfig`, `CoreAddons`-era per-vault settings, or `.obsidian/*.json` read/write.

## Finding

### CORRECTION (HIGH — vault settings on-disk format): ten keys the audit puts in `app.json` actually go to `appearance.json`

`domains/vault.md` §2 lists only six appearance keys (`theme`, `cssTheme`, `accentColor`, `baseFontSize`, `baseFontSizeAction`, `textFontFamily`) and files `showRibbon`, `showViewHeader`, `openBehavior`, `slidingSidebar`, `floatingNavigation`, `autoFullScreen`, `nativeMenus` under `AppConfig`. The real allow-list is a **16-element array literal** — sixteen, not the six the audit documents, and note the literal also contains two keys (`enabledCssSnippets`, `translucency`) that `domains/settings.md` discusses as appearance settings without recording their file:

> Container type resolved: it is a plain array, and `contains` is a polyfilled alias for `includes`, installed at `enhance/enhance.js:68`:
>
> ```js
> // enhance/enhance.js:68
> e(Array.prototype, "contains", Array.prototype.includes || function (t) { … })
> ```
>
> So `EE.contains(s)` is exactly `Array.prototype.includes`, not a `Set`. A Corbomite
> reimplementation must split on **membership of this 16-string set**, in either
> direction — the difference between "unknown key" and "known key" is load-bearing.

```js
// app/own/0920-eE.js:1173  (verbatim, also at asar/obsidian/app.js offset 1246402)
}, ["accentColor", "theme", "cssTheme", "enabledCssSnippets", "showViewHeader", "showRibbon", "nativeMenus", "translucency", "textFontFamily", "interfaceFontFamily", "monospaceFontFamily", "baseFontSize", "baseFontSizeAction", "slidingSidebar", "floatingNavigation", "autoFullScreen"]),
```

Consumed at:

```js
// app/own/1129-UD.js:144
for (a in i = this.config, r = {}, o = {}, i)
  s = a, i.hasOwnProperty(s) && (EE.contains(s) ? o[s] = i[s] : r[s] = i[s]);
```

So **10 keys the audit places in `app.json` are written to `appearance.json`**: `enabledCssSnippets`, `showViewHeader`, `showRibbon`, `nativeMenus`, `translucency`, `interfaceFontFamily`, `monospaceFontFamily`, `slidingSidebar`, `floatingNavigation`, `autoFullScreen`. (`openBehavior` is genuinely in `app.json` — the audit was right about that one.)

Consequences for a faithful writer:
- `saveConfig` rewrites **both** files on every save, unconditionally (`app/own/1129-UD.js:145-149`), so `appearance.json` always materialises as `{}` at minimum.
- A Corbomite writer that puts `showRibbon` in `app.json` will *also* leave a stale copy in `appearance.json` that Obsidian then overwrites on its next save (Obsidian's `this.config` is the single merged map, `app/own/1129-UD.js:87`), silently reverting the user's Corbomite-side change.
- Conversely, a Corbomite writer that **drops** the 10 keys when round-tripping `appearance.json` loses them.

### NEW: the full defaults table (`PC` in the audit) — audit open question 2, answered

`Vault.getConfig` falls back to `ME[key]` (`app/own/1129-UD.js:155`). The complete table is `app/own/0920-eE.js:1174-1241`. Values the audit asserts nowhere, and which are on-disk-affecting once written:

| Key | Default | Note |
|---|---|---|
| `attachmentFolderPath` | `"/"` | **vault root**, not `"."` — the default puts attachments next to nothing, at the root |
| `newFileLocation` | `"root"` | |
| `newFileFolderPath` | `"/"` | |
| `newLinkFormat` | `"shortest"` | |
| `trashOption` | `"system"` | |
| `promptDelete` | `true` | |
| `deleteUnlinkedAttachments` | `"ask"` | |
| `useMarkdownLinks` | `false` | |
| `alwaysUpdateLinks` | `false` | |
| `defaultViewMode` | `"source"` | |
| `tabSize` | `4` | |
| `propertiesInDocument` | `"visible"` | |
| `mobilePullAction` | `"command-palette:open"` | |
| `pdfExportSettings` | `{pageSize:"Letter", landscape:false, margin:"0", downscalePercent:100}` | nested default object |
| `userIgnoreFilters` | `null` | **not** `[]` — see the userIgnoreFilters addendum |
| `spellcheckLanguages` | `null` | not `[]` |

### CORRECTION: two audited type signatures are wrong

- `openBehavior?: 'default' | 'tab' | 'split' | 'window'` — the default is the **empty string** `""` (`app/own/0920-eE.js`), not `'default'`.
- `baseFontSizeAction?: 'increase' | 'decrease' | ''` — the default is the **boolean `false`** (`!1`), not `''`.

### NEW: six config keys the audit's `AppConfig`/`AppearanceConfig` tables omit entirely

`enabledCssSnippets: []`, `translucency: false`, `interfaceFontFamily: ""`, `monospaceFontFamily: ""`, `settingsPopoutWindow: true`, `hotkeys: {}`, `types: {}` (all `app/own/0920-eE.js:1174-1241`).

`hotkeys` is a *config-map* key (default `{}`), distinct from the `hotkeys.json` **file** — Obsidian Sync classifies the two differently (§ below), so a Corbomite implementation must not conflate them.

`types` has a sibling **file**: `.obsidian/types.json`, classified alongside `app.json` for Sync purposes (`app/own/2043-Xte.js:314`). It is absent from the audit's `.obsidian/<name>.json` consumer list (§3).

### NEW: Obsidian Sync's file allow-list (`sync/ignore rules`, named in the seed map's scope but absent from the doc)

`_allowSyncFile` (`app/own/2043-Xte.js:300-322`) is the whole rule:

1. Any path under `ignoreFolders` → **not** synced.
2. Inside `configDir/`: any path with a `node_modules` segment or a **dot-prefixed** segment → not synced. (So `.obsidian/plugins/<id>/data.json` *is* classified, but see 4.)
3. Otherwise classify and gate on `allowSpecialFiles` (defaults `["app","appearance","appearance-data","hotkey","core-plugin","core-plugin-data"]`, `app/own/0918-XC.js:49`): `app.json`/`types.json`→`app`; `appearance.json`→`appearance`; `hotkeys.json`→`hotkey`; `core-plugins.json`/`core-plugins-migration.json`→`core-plugin`; `.obsidian/*.json`→`core-plugin-data`; `snippets/*.css` and `themes/<t>/theme.css`|`manifest.json`→`appearance-data`; `plugins/<id>/<pluginfile>`→`community-plugin-data`.
4. **`workspace.json` and `workspace-mobile.json` are never synced** — the whole expression short-circuits to `false` for them (`app/own/2043-Xte.js:314`). Likewise `community-plugin-data` and `community-plugin` are absent from the default `$C`.
5. Any path whose **first segment** starts with `.` → not synced. **This is why `.trash/` never syncs.**
6. Non-config files: extension must be `md`/`canvas`/`base`, or a media type enabled in `allowTypes` (default `["image","audio","pdf","video"]`, `app/own/0918-XC.js:48`) — images (`bmp png jpg jpeg gif svg webp avif`), audio (`mp3 wav m4a 3gp flac ogg oga opus`), video (`mp4 webm ogv mov mkv`, with `webm` also accepted as audio), pdf.

### CONFIRMED (doc claims re-verified, now citable)

| Claim (`domains/vault.md`) | 1.13.7 location |
|---|---|
| `saveConfig` splits via an `AC.contains(key)` allow-list | `app/own/1129-UD.js:144` (`EE.contains(s)`) |
| `getConfig(key)` falls back to `PC[key]` and deep-clones arrays/objects | `app/own/1129-UD.js:153-155` (`JSON.parse(JSON.stringify(t))`) |
| `setConfig` no-ops on equal, deletes on `undefined`, schedules save + fires `config-changed` | `app/own/1129-UD.js:156-157` |
| `reloadConfig` debounced 500 ms leading-edge; merges `Object.assign({}, appearance, app)`; fires `config-changed` per changed **and** per deleted key | `app/own/1129-UD.js:63`, `:87`, `:88` |
| `requestSaveConfig` debounced 1 s leading-edge | `app/own/1129-UD.js:63` (`Ml(fn, 1e3, !0)`) |
| `setupConfig` migrates `editorFontFamily → textFontFamily`, deleting the old key, and only if the new key is absent | `app/own/1129-UD.js:125-127` |
| `getConfigFile(name)` → `configDir + "/" + name + ".json"` | `app/own/1129-UD.js:164-165` |
| `readJson` returns `null` on ENOENT, `undefined` on parse failure | `app/own/1129-UD.js:185-187` |
| `writeJson` uses `JSON.stringify(data, undefined, 2)` | `app/own/1129-UD.js:199` |
| `readPluginData`/`writePluginData` → `<dir>/data.json`, normalised | `app/own/1129-UD.js:158-163` |
| `load()` starts `adapter.watch` then, **desktop only**, `watchHiddenRecursive(configDir)` | `app/own/1129-UD.js:209-220` (`this.adapter instanceof Ql`) |
| `raw` re-installs hidden watchers inside `configDir` and triggers `reloadConfig` for `app.json`/`appearance.json` | `app/own/1129-UD.js:275` |
| `configDir` must start with `.`, not equal `.`, and pass the illegal-char check | `app/own/1129-UD.js:105-108` |

Note the reloadConfig merge order: `Object.assign({}, appearance, app)` (`app/own/1129-UD.js:87`, `:125`) — **`app.json` wins** over `appearance.json` on a key collision. Also `reloadConfig` *replaces* `this.config` wholesale from disk, then deletes in-memory keys absent from disk (`:88`); it does not merge into the existing map.

## Why noticed now

The prior corpus this audit was built from was destroyed, so every `AC`/`PC` citation was unverifiable. Restoring citability meant locating the two module-level tables the audit guessed at (`settings/SettingTab.js` for `AC`, "still-to-find `PC` constants table"). Both turned out to live in the same minified statement as the mobile-onboarding screen, in a statement whose decompiled form reads like an unrelated UI class — which is why grep-by-symbol never found them and why open questions 1 and 2 stayed open for the whole life of the audit.

## Action taken

- Recorded the 16-key allow-list verbatim as the normative partition. Any `Corbomite::VaultConfig` implementation must split on exactly this set; the audit's 6-key `AppearanceConfig` is wrong and the audit's placement of 10 keys is wrong.
- Open questions 1 and 2 (`domains/vault.md` §13) are now **closed**: `AC` = the array at `app/own/0920-eE.js:1173`; `PC` = the object at `app/own/0920-eE.js:1174-1241`.
- Sync rules are recorded here rather than in the Sync domain because the ignore predicate is a *vault-path* predicate and shares `Il`/`Fl`/`Al` with the file-tree layer. `workspace.json` never syncing and `.trash/` never syncing are the two facts most likely to change someone's expectations.
