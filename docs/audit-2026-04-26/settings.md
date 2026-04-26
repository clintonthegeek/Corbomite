# Settings domain audit

Spec: `/home/clinton/dev/Corbomite/docs/obsidian-audit/domains/settings.md`
Cross-check: `/home/clinton/dev/Corbomite/docs/obsidian-audit/VAULT-FORMAT.md`
Audited: 2026-04-26

---

## Architecture fit (KConfig vs `.obsidian/*.json` + `Setting` fluent UI)

Corbomite, by project convention, **uses KConfig (KCfg/KConfigXT) for end-user
preferences and Qt widgets directly for the settings UI**. This deliberately
diverges from Obsidian's fluent `Setting`/`SettingTab` builder. The audit spec
endorses that direction (translation principle in §11). The audit-relevant
question is therefore *not* "does Corbomite clone the fluent builder" but two
narrower ones:

1. Does the per-user Qt/KConfig surface cover the settings Obsidian users
   expect? (cosmetic — partial.)
2. Does Corbomite read **and** write the `.obsidian/*.json` files Obsidian
   expects to find in a vault, byte-compatibly enough to round-trip?
   (compatibility — partial; one major correctness bug, several missing
   files.)

The KConfig schema is `/home/clinton/dev/Corbomite/src/app/corbomite.kcfg:1-87`
with codegen options at `corbomitesettings.kcfgc:1-6` (singleton, mutators,
properties). It defines five groups (`Editor`, `Files`, `Appearance`,
`Templates`, `DailyNotes`) holding 14 keys total. None of those key names
match Obsidian's `app.json` / `appearance.json` schemas (Corbomite uses Qt
camelCase like `FontSize`, `LineWrap`, `MarkoffTheme`; Obsidian uses
`baseFontSize`, `lineWrap`, `cssTheme`). KConfig is **the source of truth**;
the vault `.obsidian/` files are written to as a *secondary, partial mirror*
in `MainWindow::applyVaultPortableSettings()`
(`src/app/MainWindow.cpp:2428-2487`).

The `.obsidian/*.json` plumbing lives in `libs/storage` not `libs/vault`:
- Generic JSON I/O + typed accessors:
  `libs/storage/include/corbomite/storage/VaultConfig.h:26-122` and
  `libs/storage/src/VaultConfig.cpp:1-277`.
- Obsidian-format serialiser (2-space indent, no trailing newline, key
  order preserved): `libs/storage/src/VaultConfig.cpp:19-48`.
- A second I/O path lives on `Vault` itself —
  `Corbomite::Vault::readConfigJson` / `writeConfigJson`
  (`libs/vault/src/Vault.cpp:434-461`) — which is the entry point
  re-exposed to plugins via `VaultProxy::readConfigJson` /
  `writeConfigJson` (`libs/vault/src/proxies/VaultProxy.cpp:269-285`).
  See **Notable concerns** §1 — the two paths use *different* JSON
  formatters.
- Plugin-private storage: `Corbomite::PluginDataStore`
  (`libs/vault/include/corbomite/vault/PluginDataStore.h:14-30`,
  `libs/vault/src/PluginDataStore.cpp:11-38`) writes
  `<pluginDir>/data.json` via `QSaveFile` (Obsidian shape: arbitrary
  plugin-defined JSON).

The Qt-side dialog is `Corbomite::SettingsDialog` (a `KPageDialog` with
`KPageDialog::List` face) at `src/dialogs/SettingsDialog.h:14-35` and
`src/dialogs/SettingsDialog.cpp:23-249`. It owns five hard-coded pages
(Editor, Files, Appearance, Daily Notes, Plugins), constructed eagerly in
the constructor (no `display()`-on-open contract — see Obsidian invariant
in spec §8 / §11: Corbomite does **not** rebuild pages on tab switch).
There is no runtime `addPage(KPageWidgetItem*)` API exposed for plugins.

Apply-side wiring: `SettingsDialog::applySettings()`
(`src/dialogs/SettingsDialog.cpp:217-247`) writes back via `findChild<>` by
`objectName`, then calls `settings->save()`. `MainWindow::onSettingsApplied`
(`src/app/MainWindow.cpp:2496-2501`) chains `applyTheme`,
`applyVaultPortableSettings`, and `applyAutosaveDelay`, so changes persist
to **both** `~/.config/corbomite[-dev]rc` and (selectively) the open
vault's `.obsidian/`.

---

## End-user settings UI parity

Compared against the canonical Obsidian tab list (spec §7, §13 Q1):

| Obsidian tab | Corbomite page | Status |
|---|---|---|
| Editor | `setupEditorPage()` (`SettingsDialog.cpp:42-80`) | Partial — covers font size, tab size, line numbers, line wrap, autosave delay. **Missing:** vimMode, smartIndentList, foldHeading, `defaultViewMode`, spellcheck/spellcheckLanguages, readableLineLength, strictLineBreaks, autoPairBrackets, autoPairMarkdown |
| Files & Links | `setupFilesPage()` (`SettingsDialog.cpp:82-104`) | Partial — covers trash behaviour and confirm-before-delete. **Missing:** `newFileLocation`, `newFileFolderPath`, `attachmentFolderPath`, `useMarkdownLinks`, `newLinkFormat`, `alwaysUpdateLinks`. (`attachmentFolderPath` is referenced in `libs/vault/src/FileManager.cpp:281-313` as TODO.) |
| Appearance | `setupAppearancePage()` (`SettingsDialog.cpp:106-171`) | Partial — system/light/dark theme + Markoff editor theme + QOwnNotes import. **Missing:** `accentColor`, `translucency`, `nativeMenus`, `nativeTitleBar`, all four font families (`interfaceFontFamily`, `textFontFamily`, `monospaceFontFamily`), `baseFontSize`, `cssTheme` selector, CSS-snippets toggle list. |
| Hotkeys | none | **Missing.** No hotkeys page; Corbomite does not surface a `KShortcutsDialog` from `SettingsDialog`. (`KXmlGuiWindow` machinery is present at `src/mdi/CorbomiteMDI.cpp:1466`, and ad-hoc `Shortcuts` group reads at `src/mdi/CorbomiteMDI.cpp:142-151` — but no UI gate.) |
| Core Plugins | `setupPluginsPage()` → `PluginsPage` (`src/dialogs/PluginsPage.cpp:1-251`) | **Conflated** — Corbomite has one Plugins page that lists every discovered KPluginFactory module (built-ins + community). Obsidian splits these into *Core Plugins* and *Community Plugins* with different UX (toggling a core plugin opens its sub-tab; community plugin tab includes install/uninstall). |
| Community Plugins | (same `PluginsPage`) | Partial — no distinction from core; no install/marketplace UX (P2 — community-plugins.json compat sub-blocked on plugin loader). |
| About | none | **Missing.** No version / KAboutData page exposed. |
| Daily Notes | `setupDailyNotesPage()` (`SettingsDialog.cpp:173-208`) | Implemented — Corbomite-extra (Obsidian carries this inside the Daily Notes core plugin's settings tab). |

The Plugins page is non-trivial: it shows compat state (`IncompatibleVersion`,
`IncompatibleApiLevel`), declared permissions (read-only for trusted plugins,
toggleable for untrusted), and disables the row checkbox when the load state
isn't `Compatible` (`PluginsPage.cpp:81-84, 173-196, 203-218`). This is more
sophisticated than Obsidian's binary on/off toggle.

`SettingsDialog` does not honour Obsidian's "rebuild on open" contract (spec
§8). All pages are constructed once at dialog construction
(`SettingsDialog.cpp:32-36`); reopening the dialog instantiates a new
dialog. That happens to work for current built-in pages because they all
read live KConfig values at constructor time, but it forecloses any
future plugin-tab API where the page contents depend on plugin state that
changed since dialog construction.

---

## On-disk schema compatibility matrix (per-file)

Reference: `docs/obsidian-audit/VAULT-FORMAT.md:33-43, 77-247`. Status flags
below: **R** = Corbomite reads it, **W** = writes it, **U** = unknown-keys
preserved on round-trip, **(--)** = not handled.

| File | R | W | U | Where | Notes |
|---|---|---|---|---|---|
| `app.json` | only `userIgnoreFilters` | -- | n/a | `libs/storage/src/VaultConfig.cpp:115-118, 263-274` (read); `VaultScanner` (consumer at `libs/vault/include/corbomite/vault/VaultScanner.h:20`) | Major gap. Editor settings (`vimMode`, `defaultViewMode`, `livePreview`, `showLineNumber`, `tabSize`, `useTab`, `readableLineLength`, `lineWrap`, …) and files-and-links settings (`newLinkFormat`, `useMarkdownLinks`, `attachmentFolderPath`, `alwaysUpdateLinks`, `trashOption`, `promptDelete`, `newFileLocation`, `newFileFolderPath`) are **not** read into Corbomite's runtime, and **not** written back from KConfig. The user-facing settings dialog reads/writes KConfig only. **A vault opened by both Obsidian and Corbomite will have its app.json silently ignored on the Corbomite side.** |
| `appearance.json` | not read | only `theme` key | n/a | `MainWindow::applyVaultPortableSettings` (`src/app/MainWindow.cpp:2440-2453`) | Write path uses `vc.mergeJson(...)` so unknown keys at the top level survive. **However:** the value Corbomite writes for `theme` is its own `system`/`light`/`dark` token (KConfig `Appearance/Theme` — see `corbomite.kcfg:50`). Obsidian writes `theme: "obsidian"` / `"moonstone"` / specific css theme name — a different set of values. So even when Corbomite writes the key, the value isn't compatible. `cssTheme`, `accentColor`, `baseFontSize`, font-family keys, `translucency`, `nativeMenus`, `nativeTitleBar` — all unread, unwritten. Markoff-editor theme persists only in KConfig (`MarkoffTheme`). |
| `hotkeys.json` | parser exists | parser exists | yes (insertion order + empty arrays + unknown ids) | Schema: `libs/core/include/corbomite/core/Hotkey.h:38-59`. Parser/serialiser: `libs/core/src/Hotkey.cpp:32-123`. I/O wrapper: `libs/storage/src/VaultConfig.cpp:179-187`. | **Library is well-built but unused.** No call site reads or writes hotkeys.json into Corbomite's runtime. Corbomite's actual shortcut binding goes through `KSharedConfig` (`Shortcuts` group in `corbomiterc`, see `src/mdi/CorbomiteMDI.cpp:142-151`), which is `kxmlguirc`-style. **Round-trip risk: high** — see dedicated section below. |
| `core-plugins.json` | yes (with legacy-array migration) | yes | object form preserved | `libs/storage/src/VaultConfig.cpp:209-261`. Test: `tests/storage/tst_obsidian_vault_roundtrip.cpp:79-84, 187, 195, 212`. | Schema (object `{id: bool}`) modelled correctly; legacy array → object migration implemented incl. delete of `core-plugins-migration.json` (`VaultConfig.cpp:223-255`). **Not wired to PluginManager** — `PluginManager::enablePlugin` / `disablePlugin` (used by `PluginsPage::onItemChanged` at `src/dialogs/PluginsPage.cpp:102-119`) does not consult or mutate `core-plugins.json`. Toggling a plugin in Corbomite does not affect what Obsidian sees. Toggling core plugins in Obsidian does not change which Corbomite plugins are enabled when the vault re-opens. |
| `community-plugins.json` | yes (`QStringList`) | yes | n/a (top-level array) | `libs/storage/src/VaultConfig.cpp:135-177`. | Implemented at the I/O level; preserves Obsidian's 2-space indent + no-trailing-newline. **Not wired** to `PluginManager`: enabling an "untrusted" plugin doesn't append it to `community-plugins.json`. |
| `workspace.json` | yes (separate audit) | yes | yes | `src/app/SessionManager.h:16-72`, `libs/core/src/Workspace.cpp:856-888`, `libs/core/src/WorkspaceSerializer.h:24`. | Out of scope here; mentioned because workspace.json overlaps the settings domain in Obsidian (`SettingModal.lastTabId` is in-memory only — see spec §13 Q2). |
| `graph.json` | -- | -- | -- | not implemented | Owned by graph internal plugin in Obsidian. Corbomite's `GraphViewPlugin` (`src/plugins/graph-view/GraphViewPlugin.cpp`) does not round-trip `.obsidian/graph.json`. |
| `bookmarks.json` | yes | yes | not preserved (`BookmarksStore::toJson()` rebuild) | `src/plugins/bookmarks/BookmarksPlugin.cpp:44, 82` via `VaultProxy::readConfigJson`/`writeConfigJson` | Reads as object root, writes via `VaultProxy::writeConfigJson` which uses `QJsonDocument::Indented` (4-space) — see Notable concerns §1. Not via the 2-space `VaultConfig::serializeObsidianStyle` path. |
| `templates.json` | yes | yes (only `folder`) | yes via `mergeJson` | `libs/storage/src/VaultConfig.cpp:199-207`; `libs/models/src/TemplateService.cpp:149`; `MainWindow.cpp:2473-2486`. | Schema at `addenda/2026-04-15-daily-notes-templates-schemas.md`. **`date_format` and `time_format` are not written back** even though KConfig holds equivalents in the `Templates` group (`DefaultDateFormat`, `DefaultTimeFormat` at `corbomite.kcfg:64-71`); only `folder` round-trips. |
| `daily-notes.json` | yes | yes (folder, format, template) | yes via `mergeJson` | `libs/storage/src/VaultConfig.cpp:189-197`; `libs/models/src/DailyNoteService.cpp:43-63`; `MainWindow.cpp:2455-2471`. | `autorun` is preserved on read (via `mergeJson`'s top-level union) but never consumed by `DailyNoteService` (acknowledged TODO comment at `DailyNoteService.cpp:60-62`). |
| `zk-prefixer.json` | -- | -- | -- | not implemented | Unique-note core plugin's settings; Corbomite has no equivalent. |
| `canvas.json` | -- | -- | -- | not implemented | Canvas internal plugin's settings; Corbomite's canvas (`libs/canvas`, `src/canvas`) has its own per-canvas file but no `.obsidian/canvas.json` global state. |
| `file-recovery.json` | -- | -- | -- | not implemented | File recovery snapshot store; Corbomite has no equivalent. (Plan exists at `addenda/2026-04-19-file-recovery-plugin.md`.) |

---

## Hotkeys: kxmlguirc vs `hotkeys.json` — round-trip risk

Corbomite has built two complete hotkey models that **do not talk to each
other**:

1. **KXmlGui / KSharedConfig path (live).** Actions are added via
   `KActionCollection` inside `GUIClient::registerToolView`
   (`src/mdi/CorbomiteMDI.cpp:138-209`); shortcuts are read out of
   `KSharedConfig::openConfig() → group("Shortcuts")` keyed by action name
   (`CorbomiteMDI.cpp:142-151`). This persists in `~/.config/corbomite[-dev]rc`,
   not in any vault file. This is the only path that actually binds
   keystrokes today.

2. **`hotkeys.json` parser/serialiser (dead).** Schema-faithful types are
   defined in `libs/core/include/corbomite/core/Hotkey.h` (`HotkeyModifier`,
   `Hotkey`, `HotkeyFile`). The serialiser (`libs/core/src/Hotkey.cpp:69-123`)
   manually emits Obsidian's exact 2-space indent and preserves declared
   command-id `order` plus the bindings hash, plus empty arrays (explicit
   unbind), plus unknown command ids — matching the spec invariants in
   §8 ("Defaults MUST NOT appear in the file" / "Unknown command ids must
   be preserved"). The `VaultConfig::readHotkeys` / `writeHotkeys`
   accessors (`libs/storage/src/VaultConfig.cpp:179-187`) use the generic
   2-space writer.

Nothing reads `hotkeys.json` into the live action system, and nothing
writes Corbomite's customised `Shortcuts` group out to it. Concrete risks:

- **Silent loss on first save in Obsidian.** A vault that arrives with a
  user-tuned `hotkeys.json` will see those overrides ignored under
  Corbomite. As soon as a user reassigns even one shortcut in Corbomite
  (via the `KShortcutsDialog` mechanism if exposed), Corbomite writes to
  `corbomiterc` only, leaving `hotkeys.json` untouched. If/when Obsidian
  is opened, its hotkeys still reflect the *old* file — divergent state.
- **Default-emission risk if the bridge is later wired naively.** Spec §8
  warns: "Corbomite must not write defaults into this file, or it will
  mask future Obsidian default changes." Any future bridge has to filter
  by "user-modified vs default" — the existing KConfig surface doesn't
  carry that flag, so the bridge will need to compare against
  `KStandardShortcut` / per-action default sequences.
- **No `Mod` token resolution path.** `HotkeyModifier::Mod`
  (`libs/core/include/corbomite/core/Hotkey.h:17`) exists, but no code
  resolves it to platform-specific `Ctrl` (Linux) at dispatch time. When
  the bridge lands, this needs a dedicated runtime resolver in the Scope
  layer (currently absent).
- **No `vault.on('raw')`-style external-edit detection.** Spec §3
  mandates: "External-edit detected via `vault.on('raw', cb)` filtered to
  `configDir + '/hotkeys.json'` → `HotkeyManager.load` re-reads. No
  debounce on write." Corbomite has no corresponding watcher.

---

## Per-internal-plugin settings files coverage

Cross-referenced against `VAULT-FORMAT.md:33-43` and the cluster Q internal
plugins shipped at `src/plugins/`:

| File | Owning Obsidian core plugin | Corbomite plugin | Round-trip status |
|---|---|---|---|
| `daily-notes.json` | Daily Notes | (none — service in `libs/models/src/DailyNoteService.cpp`) | Read + partial write (`folder`, `format`, `template`); `autorun` preserved but unconsumed |
| `templates.json` | Templates | (none — service in `libs/models/src/TemplateService.cpp`) | Read + partial write (`folder` only); `date_format` / `time_format` ignored on write |
| `bookmarks.json` | Bookmarks | `src/plugins/bookmarks/BookmarksPlugin.cpp:44, 82` | Read + write — but via the wrong I/O path (`VaultProxy::writeConfigJson`, 4-space indent — see Notable concerns) |
| `graph.json` | Graph | `src/plugins/graph-view/GraphViewPlugin.cpp` | Not implemented |
| `canvas.json` | Canvas | `libs/canvas`, `src/canvas` | Not implemented |
| `zk-prefixer.json` | Unique Note | (none) | Not implemented |
| `file-recovery.json` | File Recovery | (none) | Not implemented |
| `workspaces.json` (named-workspace snapshots) | Workspaces | (none) | Not implemented (in-vault `workspace.json` is single-snapshot only) |

---

## Plugin settings storage (`data.json`) parity

`Corbomite::PluginDataStore` (`libs/vault/include/corbomite/vault/PluginDataStore.h`,
`libs/vault/src/PluginDataStore.cpp:11-38`) maps to Obsidian's
`Plugin.loadData` / `Plugin.saveData`:

- **Path:** `<pluginDir>/data.json` — `pluginDir` is documented at
  `PluginContext.h:92` as `<vault>/.obsidian/plugins/<plugin-id>/`. Path
  shape matches Obsidian (`PluginSettingTab.js:255-270` per spec §3).
- **Atomic write:** `QSaveFile` (`PluginDataStore.cpp:32-37`) — equivalent
  to Obsidian's mtime-tracked write.
- **Schema:** arbitrary plugin-defined JSON, returned as `QJsonObject` via
  `PluginContext::loadData` / `saveData`
  (`libs/vault/src/PluginContext.cpp:186-200`). Empty object on missing /
  malformed (matches Obsidian's "absence returns null" semantics weakly —
  Obsidian returns `null`, Corbomite returns `{}`).

Two divergences vs the spec:

1. **Indent.** `PluginDataStore::save`
   (`libs/vault/src/PluginDataStore.cpp:35`) writes
   `QJsonDocument(obj).toJson(QJsonDocument::Indented)` — 4-space indent,
   trailing newline. Obsidian uses 2-space + no trailing newline. This is
   a cosmetic round-trip diff (Obsidian-rewritten files will look
   different on `git diff`) but not a parse-compat blocker.
2. **No external-edit notification.** Spec §3 says
   `Plugin._onConfigFileChange` is a debounced 50 ms watcher firing
   `plugin.onExternalSettingsChange()`. Corbomite has no equivalent —
   plugins do not get notified when their `data.json` is edited externally
   (e.g. by `git pull`).

---

## Implemented

- KConfig schema covering 14 keys across 5 groups
  (`src/app/corbomite.kcfg:1-87`).
- `KPageDialog`-backed Settings dialog with 5 pages
  (`src/dialogs/SettingsDialog.cpp:23-249`).
- `VaultConfig` library with generic JSON I/O + `mergeJson` for unknown-key
  preservation, plus typed accessors for app/appearance/community-plugins/
  hotkeys/daily-notes/templates/core-plugins
  (`libs/storage/src/VaultConfig.cpp:1-277`).
- Obsidian-style serialiser (2-space indent, no trailing newline,
  insertion-order keys) at `VaultConfig.cpp:19-48`. Used by every typed
  writer except `Vault::writeConfigJson` (the plugin-facing path).
- `core-plugins.json` legacy-array → object migration with deletion of
  `core-plugins-migration.json` (`VaultConfig.cpp:223-255`).
- `Hotkey.h` / `Hotkey.cpp` schema-faithful parser+serialiser of
  `hotkeys.json` (with `Mod`/`Ctrl`/`Meta`/`Shift`/`Alt` modifier set,
  insertion-order preservation, empty-array preservation).
- `PluginDataStore` for `<pluginDir>/data.json` atomic write
  (`libs/vault/src/PluginDataStore.cpp`).
- Cluster-Q `PluginsPage` with permission-grant integration, compat-state
  surfacing, and `pluginEnabled`/`pluginDisabled` reactive rebuild
  (`src/dialogs/PluginsPage.cpp:1-251`).
- Daily Notes / Templates per-vault override applied on top of KConfig
  defaults (`DailyNoteService.cpp:43-63`, `TemplateService.cpp:149`).
- Round-trip integration test asserting unknown-key preservation across
  app.json / appearance.json / core-plugins.json / community-plugins.json /
  hotkeys.json / workspace.json
  (`tests/storage/tst_obsidian_vault_roundtrip.cpp:45-232`).
- `userIgnoreFilters` extracted from `app.json` into `VaultScanner`
  (`VaultConfig.cpp:263-274`).

## Partial / divergent

- **Settings UI** — 5 of Obsidian's 8–9 tabs. Missing Hotkeys, Community
  Plugins (conflated with core), About. Editor / Files / Appearance pages
  expose only a small subset of Obsidian's keys.
- **`appearance.json`** — `theme` key is written but the value vocabulary
  (`system`/`light`/`dark`) doesn't match Obsidian's
  (`obsidian`/`moonstone`/cssTheme name); Markoff-editor theme persists in
  KConfig only.
- **`templates.json`** — `folder` round-trips, `date_format`/`time_format`
  do not.
- **`daily-notes.json`** — `autorun` preserved but unconsumed.
- **`PluginDataStore`** — works, but uses 4-space indent + trailing newline
  (cosmetic round-trip diff vs Obsidian).
- **`core-plugins.json` / `community-plugins.json`** — fully readable +
  writable at the I/O layer, but **`PluginManager` does not consult or
  mutate them**, so toggle state does not transfer between Corbomite and
  Obsidian.
- **Settings dialog lifecycle** — pages constructed once at dialog
  construction, not rebuilt on tab switch (Obsidian's `display()`-on-open
  contract is not honoured; spec §11 flagged this).

## Missing

- **`app.json` write-side entirely** — none of Corbomite's editor / files
  KConfig keys map back to `app.json` keys. A user setting font size, line
  wrap, tab size, trash option, etc. in Corbomite has zero effect on what
  Obsidian sees in the same vault.
- **`app.json` read-side, except `userIgnoreFilters`** — Obsidian-set
  values for `vimMode`, `defaultViewMode`, `livePreview`, `showLineNumber`,
  `tabSize`, `useTab`, `readableLineLength`, `lineWrap`,
  `autoPairBrackets`, `autoPairMarkdown`, `spellcheck`,
  `spellcheckLanguages`, `strictLineBreaks`, `newLinkFormat`,
  `useMarkdownLinks`, `attachmentFolderPath`, `alwaysUpdateLinks`,
  `trashOption`, `promptDelete`, `newFileLocation`, `newFileFolderPath` —
  none are honoured.
- **`appearance.json` read-side entirely.** No path consumes the file.
- **Hotkeys round-trip** — parser/serialiser exist; nothing wires them to
  the action system. See dedicated section.
- **Plugin enable-state round-trip** — `core-plugins.json` /
  `community-plugins.json` not wired to `PluginManager`.
- **About page** — no surface exposing `KAboutData` or qutepart/Markoff
  build provenance.
- **Hotkeys page** — `KShortcutsDialog` is not invoked from
  `SettingsDialog`.
- **Plugin runtime settings-tab API** — `SettingsDialog::addPage` is not
  exposed publicly; spec §11 calls this out as the actionable gap. Plugins
  ship `metadata.json` permissions and a `data.json` store but cannot
  contribute a settings page.
- **`graph.json`, `canvas.json`, `zk-prefixer.json`, `file-recovery.json`,
  `workspaces.json`** — none implemented.
- **External-edit watchers** for `hotkeys.json` and per-plugin `data.json`
  (Obsidian fires `vault.on('raw')` and a debounced 50 ms reload — see
  spec §5, §3). Corbomite has no equivalent path.

## Notable concerns / suspected bugs

1. **Two `.obsidian/*.json` writers with different formatters.** This is
   the most consequential issue.
   - `Corbomite::VaultConfig::writeJson` (`libs/storage/src/VaultConfig.cpp:95-100`)
     calls `serializeObsidianStyle` (`VaultConfig.cpp:19-48`) → 2-space
     indent, no trailing newline. **Matches Obsidian.**
   - `Corbomite::Vault::writeConfigJson` (`libs/vault/src/Vault.cpp:448-461`)
     calls `doc.toJson(QJsonDocument::Indented)` → 4-space indent,
     trailing newline. **Does not match.** This is the entry point
     re-exposed to plugins (`VaultProxy::writeConfigJson` at
     `libs/vault/src/proxies/VaultProxy.cpp:269-285`) and is what the
     Bookmarks plugin actually calls
     (`src/plugins/bookmarks/BookmarksPlugin.cpp:82`). Result:
     `bookmarks.json` (and any future plugin-written config file) is
     written with 4-space indent — `git diff` against an Obsidian-written
     file will show every non-blank line as changed even if no value
     changed. Recommended fix: route `Vault::writeConfigJson` through
     `VaultConfig::writeJson` (or share the formatter).

2. **`Vault::readConfigJson` parses arrays too, but `writeConfigJson`
   accepts only object/array.** Empty objects and unrecognised types
   silently fail (`Vault.cpp:454-457`). Plugins calling
   `writeConfigJson("name", QJsonValue::Null)` would get false and may
   not notice. Minor, but inconsistent with `VaultConfig::writeJson`
   (which only accepts `QJsonObject`).

3. **`appearance.json` theme value vocabulary mismatch.** Corbomite writes
   `"theme": "system"` / `"light"` / `"dark"`
   (`MainWindow.cpp:2440-2453`, vocabulary from `corbomite.kcfg:50`).
   Obsidian's `theme` key uses `"obsidian"` / `"moonstone"` (or absence
   for system). Round-trip will silently flip the user's theme each time
   the vault is opened in the other tool.

4. **`MainWindow::applyVaultPortableSettings` only writes "outbound"
   keys.** It pushes `theme`, `daily-notes` keys, and `templates.folder`
   from KConfig into `.obsidian/`, but there is **no inbound read** at
   vault open that pulls `app.json`/`appearance.json` into KConfig. The
   per-vault override pattern only works for daily-notes and templates;
   for app/appearance settings, opening a different vault doesn't change
   any user-visible setting.

5. **`PluginsPage` toggling does not write `core-plugins.json` /
   `community-plugins.json`.** `onItemChanged`
   (`src/dialogs/PluginsPage.cpp:102-119`) calls
   `m_mgr->enablePlugin(id)` / `disablePlugin(id)` only;
   `PluginManager`'s implementation does not call
   `VaultConfig::writeCorePlugins` or `writeCommunityPlugins`. This means
   the well-built I/O layer for those files is dead code on the
   write-back path. Cluster-Q's plugin enable state lives elsewhere
   (likely a plugin-dir scan + KConfig flag) and does not synchronise to
   the vault.

6. **`HotkeyFile::serialise` open-bracket whitespace bug (cosmetic).** At
   `libs/core/src/Hotkey.cpp:82-104`, the empty-bindings case writes
   `"id": []` while the non-empty case prepends a newline before the
   closing `]`. Obsidian writes both styles; survives parse but adds a
   diff if a binding gets unbound (the `[]` form). Low priority; flag for
   the bridge work.

7. **`PluginDataStore::load` returns empty object on missing file**
   (`PluginDataStore.cpp:22`); plugins cannot distinguish "first run" from
   "corrupted". Obsidian's `Plugin.loadData` returns `null` for absent.
   Plugins porting over from Obsidian will need to adapt their first-run
   detection logic. Document or change.

8. **No CSS-snippets directory support.** `VAULT-FORMAT.md:427` notes that
   each `.obsidian/snippets/*.css` becomes a togglable entry persisted in
   `appearance.json`. Corbomite has no surface for this; live theming is
   limited to KColorScheme.

9. **`SettingsDialog` is not modal-singleton-managed.** Each invocation
   instantiates a new dialog; if invoked twice quickly two dialogs will
   stack. Obsidian's `app.setting` is a singleton with `lastTabId`
   restored on reopen. Minor UX issue.
