# `obsidian/settings` — Setting, SettingTab, PluginSettingTab, SettingGroup

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/settings/`
**File count:** 4
**Files:** `Setting.js`, `SettingGroup.js`, `SettingTab.js`, `PluginSettingTab.js`
**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> The fluent builder Obsidian uses for every settings row across the entire app. `Setting` is one row in a settings panel: `.setName(...).setDesc(...).addToggle(...).addText(...).addButton(...)`. `SettingTab` is a tab in the settings modal. `PluginSettingTab` is the subclass plugins extend. `SettingGroup` is a collapsible/expandable group of settings. **On-disk contracts:** via `Plugin.saveData` for plugin settings; via `app.vault.setConfig(key, value)` for built-in settings (writes `.obsidian/app.json`, `.obsidian/appearance.json`, `.obsidian/hotkeys.json`, `.obsidian/core-plugins.json`, `.obsidian/community-plugins.json`). **Cross-domain dependencies:** depends on `ui/components`, `ui/icons`, `core` (App), `vault`; consumed by `plugin`, every settings UI.

---

**De-minifier artifact note:** `SettingGroup.js` and `Setting.js` have identical md5sums (`dc54df3cc8e7c9b7141125649bb1ab65` vs `e30d69598269b60fb052c10ba45e48cd`) — different hashes, but they cover the **same source range** (`app.js lines 68774–69546`). Both files contain the full definitions of `Setting`, `SettingGroup`, and all component classes (`BaseComponent`, `ValueComponent`, `ButtonComponent`, `ExtraButtonComponent`, `ToggleComponent`, `AbstractTextComponent`, `TextComponent`, `SearchComponent`, `TextAreaComponent`, `MomentFormatComponent`, `DropdownComponent`, `ProgressBarComponent`, `SliderComponent`, `ColorComponent`). `Setting.js` is the canonical extraction; `SettingGroup.js` is an artefact of the de-minifier pulling the same range for the sibling public symbol. Both files are identical in content. Only `Setting.js` is audited as canonical.

`PluginSettingTab.js` (source range `app.js lines 167963–168340`) contains the full `Plugin` class, then `PluginSettingTab` and the internal `Y0` (core-plugin-settings-tab) at the bottom. Code before `PluginSettingTab` (lines 1–354 of the file) belongs to the `plugin` domain; only `PluginSettingTab` (lines 355–366) and `Y0` (lines 367–379) are in scope.

`SettingTab.js` (source range `app.js lines 159325–160472`) contains only one true `SettingTab` declaration (lines 35–46 of the file); the remainder is adjacent extraction of the Keychain settings tab, several modals, and `SecretComponent`. Only the `SettingTab` class itself is in scope; the keychain UI belongs to the `secrets` domain.

---

## 1. Public API surface

### `Setting`

- **Kind:** class
- **Exported as:** `Setting`
- **Source:** `Setting.js:5–113`
- **Signature:** `new Setting(containerEl: HTMLElement)`. Builder — every method returns `this`. All `add*` methods call their callback immediately with the new component, enabling a declarative pattern: `new Setting(el).setName("foo").addToggle(t => t.setValue(true).onChange(cb))`.
- **Purpose:** Represents a single labelled row in a settings panel. Creates a fixed three-zone DOM structure on construction. Builder methods populate the label zone (`infoEl`) and attach input widgets to the control zone (`controlEl`). Multiple components can be attached to one row (e.g. an `ExtraButton` + a `Text` field).
- **Lifecycle:** Instantiated directly by settings tab `display()` methods and by `SettingGroup.addSetting`. Not owned by any lifecycle manager — it is a plain DOM builder.
- **Mixes in:** neither `Component` nor `Events`.
- **DOM shape produced:**
  ```
  div.setting-item
    div.setting-item-info
      div.setting-item-name        ← setName()
      div.setting-item-description ← setDesc()
    div.setting-item-control       ← add*() widgets appended here
  ```
- **Builder API (all return `this`):**

| Method | What it does |
|---|---|
| `setName(text \| DocumentFragment)` | Sets `nameEl` text |
| `setDesc(text \| DocumentFragment)` | Sets `descEl` text |
| `setClass(cls)` | Adds CSS class to `settingEl` |
| `setTooltip(text, opts?)` | Calls `setTooltip(nameEl, …)` |
| `setHeading()` | Adds `setting-item-heading` class — makes the row a section heading |
| `setDisabled(bool)` | Toggles `is-disabled` on `settingEl`; calls `setDisabled` on every component |
| `setNoInfo()` | Hides `infoEl` (used for compact/control-only rows) |
| `setVisibility(bool)` | Toggles row visibility |
| `clear()` | Empties `controlEl`, resets `components = []` |
| `then(cb)` | Escape hatch: `cb(this)` — useful for conditional decoration without breaking the chain |
| `addToggle(cb)` | Constructs `ToggleComponent`, adds `mod-toggle` class, passes to callback |
| `addText(cb)` | Constructs `TextComponent`; adds `Enter`-blurs-field on non-physical-keyboard platforms |
| `addTextArea(cb)` | Constructs `TextAreaComponent` |
| `addSearch(cb)` | Constructs `SearchComponent` |
| `addDropdown(cb)` | Constructs `DropdownComponent` |
| `addSlider(cb)` | Constructs `SliderComponent` |
| `addColorPicker(cb)` | Constructs `ColorComponent` |
| `addMomentFormat(cb)` | Constructs `MomentFormatComponent` |
| `addProgressBar(cb)` | Constructs `ProgressBarComponent` |
| `addButton(cb)` | Constructs `ButtonComponent` |
| `addExtraButton(cb)` | Constructs `ExtraButtonComponent` (icon-only gear/action button) |
| `addComponent(factory)` | Generic: `factory(controlEl) → component`; adds result to `components` |

---

### `SettingGroup`

- **Kind:** class
- **Exported as:** `SettingGroup`
- **Source:** `Setting.js:114–158` (canonical)
- **Signature:** `new SettingGroup(containerEl: HTMLElement)`.
- **Purpose:** A collapsible group of `Setting` rows with an optional heading and optional search filter. Used for the Keychain secrets list, core-plugin group, community-plugin group, etc.
- **DOM shape produced:**
  ```
  div.setting-group
    [div.setting-item.setting-item-heading]  ← prepended by setHeading() if text is non-empty
      div.setting-item-name
      div.setting-item-control
    [div.setting-group-search]               ← created lazily by addSearch()
    div.setting-items                        ← listEl; Setting rows appended here
  ```
- **Key methods:**
  - `setHeading(text)` — prepends or detaches the header depending on whether `text` is truthy.
  - `addClass(cls)` — adds class to `groupEl`.
  - `addSetting(cb)` — creates `new Setting(listEl)`, passes to callback.
  - `addSearch(cb)` — lazily creates `setting-group-search` div and a `SearchComponent` inside it.
  - `addExtraButton(cb)` — adds icon button to the heading's control zone.

---

### `SettingTab`

- **Kind:** class
- **Exported as:** `SettingTab`
- **Source:** `SettingTab.js:35–46`
- **Signature:** `new SettingTab(app: App, setting: SettingModal)`.
- **Purpose:** Base class for all settings tabs (core and plugin). Supplies `containerEl` (a `div.vertical-tab-content`) that the tab's `display()` method populates. `app` and `setting` are stored for access to vault config and the modal itself.
- **Instance properties:** `app`, `setting` (the modal), `containerEl`, `navEl` (the sidebar nav item, created by `addSettingTab` on first registration), `id: string`, `name: string`, `icon?: string`.
- **Lifecycle:** Instantiated once at app boot (core tabs) or at plugin load (`PluginSettingTab`). `display()` is called every time the tab is opened. `hide()` is a no-op stub in the base class (subclasses may override to clean up live widgets).
- **Mixes in:** neither `Component` nor `Events`.
- **Required overrides:**
  - `display(): void` — must empty `containerEl` and rebuild the UI. Obsidian always calls `containerEl.empty()` as the first line in every core tab's implementation.

---

### `PluginSettingTab`

- **Kind:** class
- **Exported as:** `PluginSettingTab`
- **Source:** `PluginSettingTab.js:355–366`
- **Signature:** `new PluginSettingTab(app: App, plugin: Plugin)`.
- **Purpose:** Thin subclass of `SettingTab` that community plugins extend. Sets `id` and `name` from `plugin.manifest`, stores `this.plugin`. Plugins override `display()` only.
- **Lifecycle:**
  1. Plugin calls `this.addSettingTab(new MyTab(this.app, this))` in `onload()`.
  2. `Plugin.addSettingTab` calls `app.setting.addSettingTab(tab)` and registers `app.setting.removeSettingTab(tab)` as a cleanup callback via `this.register(...)`.
  3. When the plugin is unloaded, cleanup fires automatically.
- **Mixes in:** inherits `SettingTab` (neither `Component` nor `Events`).

The internal `Y0` class (also in `PluginSettingTab.js:367–379`) is the equivalent for core (internal) plugin tabs. It takes an internal plugin object (`plugin.instance.name`, `plugin.instance.id`) rather than a `Plugin`. Corbomite's built-in pages should follow this pattern.

---

### Component classes (from `Setting.js`)

These live in the same extraction window as `Setting`. They belong to the `ui/components` domain conceptually, but their definitions appear here as artefacts of the de-minifier window.

| Class | Extends | Control element | Key methods |
|---|---|---|---|
| `BaseComponent` | — | — | `then(cb)`, `setDisabled(bool)` |
| `ValueComponent` | `BaseComponent` | — | `registerOptionListener(opts, key)` — bidirectional binding helper |
| `ButtonComponent` | `BaseComponent` | `button` | `setButtonText`, `setIcon`, `setCta`, `setWarning`, `setLoading`, `onClick(cb)`, `setTooltip`, `setClass`. Click handler is async; shows `mod-loading` spinner while awaiting. |
| `ExtraButtonComponent` | `BaseComponent` | `div.clickable-icon.extra-setting-button` | `setIcon`, `setTooltip`, `onClick(cb)`. Default icon `lucide-settings`. |
| `ToggleComponent` | `ValueComponent` | `label.checkbox-container > input[type=checkbox]` | `getValue()`, `setValue(bool)`, `onChange(cb)`, `setSmall()`. Fires `navigator.vibrate(100)` on click. |
| `AbstractTextComponent` | `ValueComponent` | (abstract; wraps an `inputEl`) | `getValue()`, `setValue(str)`, `setPlaceholder(str)`, `onChange(cb)`. Spellcheck disabled by default. |
| `TextComponent` | `AbstractTextComponent` | `input[type=text]` | `autoSelect(bool?)`. |
| `SearchComponent` | `AbstractTextComponent` | `div.search-input-container > input[type=search]` | `setClass`, `autoSelect()`, `addRightDecorator(cb)`. Has built-in clear button. |
| `TextAreaComponent` | `AbstractTextComponent` | `textarea` | — |
| `MomentFormatComponent` | `TextComponent` | `input[type=text]` | `setDefaultFormat(str)`, `setSampleEl(el)` — live preview of moment format string. |
| `DropdownComponent` | `ValueComponent` | `select.dropdown` | `addOption(value, text)`, `addOptions({value: text})`, `getValue()`, `setValue(str)`, `onChange(cb)`. |
| `SliderComponent` | `ValueComponent` | `input[type=range].slider` | `setLimits(min, max, step)`, `getValue()`, `setValue(num)`, `getValuePretty()`, `setDynamicTooltip()`, `setInstant(bool)` — instant fires on `input` event, non-instant on `change` (mouseup). |
| `ColorComponent` | `ValueComponent` | `input[type=color]` | `getValue()` (hex string), `getValueRgb()`, `getValueHsl()`, `getValueInt()`, `setValue(hex)`, `setValueRgb`, `setValueHsl`, `setValueInt`. |
| `ProgressBarComponent` | `ValueComponent` | `div.setting-progress-bar > div.setting-progress-bar-inner` | `getValue()`, `setValue(0–100)`, `setVisibility(bool)`. |
| `SecretComponent` | `BaseComponent` | composite (warning icon + value span + link button) | `setValue(key)`, `onChange(cb)`. Integrates with `SecretStorage`; shows masked value or "Link" button. |

---

## 2. Data structures

### `SettingModal` (inferred; not exported — lives in `utils/apiVersion.js`)

The settings modal is a `Modal` subclass that owns the tab registry. It is assigned to `app.setting` at boot.

```typescript
{
  settingTabs: SettingTab[];     // core + per-plugin tabs; order = registration order
  pluginTabs: SettingTab[];      // Y0 (core-plugin tabs) + PluginSettingTab; sorted A-Z by name
  activeTab: SettingTab | null;  // currently displayed tab
  lastTabId: string;             // persisted across open/close; reopens last active tab
  tabContainer: HTMLElement;     // sidebar nav items for settingTabs
  corePluginTabContainer: HTMLElement;     // sidebar group for Y0 tabs
  communityPluginTabContainer: HTMLElement; // sidebar group for PluginSettingTab tabs
}
```

### `HotkeyConfig` (inferred from `_internal.js:93907–93939`)

Written to / read from `.obsidian/hotkeys.json`.

```typescript
// .obsidian/hotkeys.json
{
  [commandId: string]: Hotkey[];  // key = full command id e.g. "editor:toggle-bold"
                                   // value = array of assigned key combos (overrides)
                                   // absence means "use default" defined at command registration
}

type Hotkey = {
  modifiers: ('Mod' | 'Ctrl' | 'Meta' | 'Shift' | 'Alt')[];
  key: string;  // e.g. "b", "Enter", "ArrowUp"
}
```

- Only **user-overridden** hotkeys appear in the file. Default hotkeys (from `addDefaultHotkeys`) are not persisted; they live in memory only.
- Absence is valid (empty = all-defaults). No version field.
- External edits detected via `vault.on('raw', cb)` filtered to `configDir + '/hotkeys.json'`.

### `CorePluginConfig` (inferred from `_internal.js:736988–737053`)

Written to / read from `.obsidian/core-plugins.json`.

```typescript
// .obsidian/core-plugins.json — new format
{
  [corePluginId: string]: boolean;  // true = enabled, false = disabled
}
```

- Migration path: older vaults have `core-plugins.json` as an array of enabled-plugin IDs. On load, if an array is detected, `core-plugins-migration.json` is also read (a `{id: {enabled: bool}}` shape), the two are merged, and `core-plugins.json` is rewritten in the new `{id: bool}` object format.
- A plugin missing from the file defaults to its `defaultOn` property at registration time.

### `CommunityPluginEnabledList`

Written to / read from `.obsidian/community-plugins.json`.

```typescript
// .obsidian/community-plugins.json
string[];  // array of enabled plugin IDs; order not significant
```

- Absence means safe-mode (no community plugins). Plugins are installed in `.obsidian/plugins/<id>/` with `manifest.json`, `main.js`, `styles.css` and an optional `data.json`.
- The global community plugin registry (`community-plugins.json` from `obsidianmd/obsidian-releases` on GitHub) is a separate network resource, not a vault file.

---

## 3. On-disk contracts

### `.obsidian/hotkeys.json`

- **Written by:** `HotkeyManager.save()` → `vault.writeConfigJson("hotkeys", data)` (`_internal.js:93907`). No debounce — written on every hotkey change.
- **Read by:** `HotkeyManager.load()` → `vault.readConfigJson("hotkeys")` at boot (`_internal.js:93922`). Re-read on `vault.on('raw', …)` when the file is externally modified.
- **Schema:** `{ [commandId]: Hotkey[] }` — see Section 2. Only user overrides appear; default key assignments are in-memory-only.
- **Lifecycle:** Created on first user customisation; absence is valid.
- **Migration:** None observed. Unknown command IDs are preserved on load (harmless — commands may not yet be registered).

### `.obsidian/core-plugins.json`

- **Written by:** `InternalPlugins.saveConfig()` → `vault.writeConfigJson("core-plugins", config)` (`_internal.js:737047`).
- **Read by:** `InternalPlugins.loadConfig()` → `vault.readConfigJson("core-plugins")` at boot (`_internal.js:736995`).
- **Schema (current):** `{ [pluginId: string]: boolean }`.
- **Migration:** Old array format migrated to object format on first load; migration file `core-plugins-migration.json` deleted after migration completes (not observed explicitly, but the array path rewrites the file).
- **Lifecycle:** Created at first plugin enable/disable action; absence → all plugins default to their `defaultOn` value.

### `.obsidian/community-plugins.json`

- **Written by:** community plugin manager on enable/disable.
- **Read by:** community plugin manager at boot.
- **Schema:** `string[]` — array of enabled plugin IDs.
- **Lifecycle:** Absence = safe mode (no plugins run). Contents determine which `.obsidian/plugins/<id>/` directories are loaded.

### `.obsidian/app.json` and `.obsidian/appearance.json`

Settings tabs call `this.app.vault.setConfig(key, value)` which triggers a debounced `saveConfig`. The `saveConfig` function partitions the in-memory config into two files using the `AC` allow-list set (defined outside the settings domain — see Section 15). Full schema documented in `domains/vault.md` Section 2.

Key settings owned by each built-in tab and their file destinations:

| Tab | Config key examples | File |
|---|---|---|
| Editor | `vimMode`, `smartIndentList`, `foldHeading`, `defaultViewMode`, `showLineNumber`, `tabSize`, `useTab`, `spellcheck`, `spellcheckLanguages`, `readableLineLength`, `strictLineBreaks`, `lineWrap`, `autoPairBrackets`, `autoPairMarkdown` | `app.json` |
| Files & Links | `newFileLocation`, `newFileFolderPath`, `attachmentFolderPath`, `useMarkdownLinks`, `newLinkFormat`, `alwaysUpdateLinks`, `trashOption`, `promptDelete` | `app.json` |
| Appearance | `theme`, `cssTheme`, `accentColor`, `translucency`, `nativeMenus`, `nativeTitleBar`, `fontFamily`, `monospaceFontFamily`, `textFontFamily`, `interfaceFontFamily`, `baseFontSize`, `editorFontSize` | `appearance.json` (via AC allow-list) |
| Hotkeys | (writes to `hotkeys.json` via `HotkeyManager.save`) | `hotkeys.json` |
| Core Plugins | (writes to `core-plugins.json` via `InternalPlugins.saveConfig`) | `core-plugins.json` |
| Community Plugins | (writes to `community-plugins.json`) | `community-plugins.json` |
| Plugin (per-plugin) | (writes to `.obsidian/plugins/<id>/data.json` via `Plugin.saveData`) | `plugins/<id>/data.json` |

### `.obsidian/plugins/<id>/data.json`

- **Written by:** `Plugin.saveData(data)` → `vault.writePluginData(manifest.dir, data, {mtime})` (`PluginSettingTab.js:255–270`).
- **Read by:** `Plugin.loadData()` → `vault.readPluginData(manifest.dir)` (`PluginSettingTab.js:233–253`).
- **Schema:** arbitrary plugin-defined JSON. Absence returns `null`.
- **External-edit detection:** `Plugin._onConfigFileChange` is debounced 50 ms and fires when the vault's `raw` event hits `<pluginDir>/data.json`; it compares `mtime` and calls `plugin.onExternalSettingsChange()` if the file was changed externally.

---

## 4. Events emitted

No events emitted. `SettingTab` and `Setting` do not extend `Events`. The settings modal is not an `Events` emitter.

The only event-adjacent behaviour: `Plugin.addSettingTab` indirectly fires `Vault.on('config-changed', key)` via `vault.setConfig` calls inside a `display()` method, but that is a `Vault` event, not a settings event.

---

## 5. Events consumed

| Listener | Subscribes to | Why |
|---|---|---|
| `HotkeyManager.onRaw` (`_internal.js:93903`) | `vault.on('raw', cb)` filtered to `configDir + '/hotkeys.json'` | Reload hotkeys when file is externally modified (e.g. by `git pull` or sync) |
| Settings modal `onOpen` | none — imperative call | Restores `lastTabId` on open |

---

## 6. Commands registered

No commands registered in this domain. The Hotkeys tab provides a UI for browsing and overriding commands registered in other domains.

---

## 7. Registries owned

### Settings Tab Registry (on `SettingModal`)

- **Stores:** `SettingTab` instances, split into `settingTabs[]` (core) and `pluginTabs[]` (plugin and internal-plugin tabs).
- **Populated by:**
  - At boot: 8–9 core tabs registered via `n.addSettingTab(new Xyz(t, n))` (`utils/apiVersion.js:3602–3610`). Order = Editor, Files & Links, [Mobile-only tab], Appearance, Hotkeys, Core Plugins, Keychain, About, [Feedback tab].
  - At plugin load: community plugins call `Plugin.addSettingTab(tab)` which delegates to `app.setting.addSettingTab(tab)`.
  - At internal plugin load: internal plugins call their equivalent registration path resulting in `Y0` instances in `pluginTabs`.
- **Read by:** `SettingModal.openTab`, `SettingModal.openTabById`, the sidebar nav rendered during `addSettingTab`.
- **Persistence:** In-memory-only. The sidebar nav DOM is persistent for the lifetime of the modal (re-used across open/close cycles).
- **Lifecycle:** Tabs added on plugin load; removed on plugin unload via `removeSettingTab`. Plugin tabs are sorted A-Z by `name` every time `updatePluginSection` is called.

---

## 8. Invariants

- `display()` is always called after `containerEl` is appended to the modal's content area — the DOM is live and can be safely queried inside `display()`.
- `containerEl.empty()` must be called at the top of every `display()` implementation to prevent double-rendering on repeated tab switches. All core tabs do this.
- A `PluginSettingTab`'s `id` equals `plugin.manifest.id` and its `name` equals `plugin.manifest.name`. These are stable across Obsidian sessions.
- `SettingModal.lastTabId` is used to restore the previously-active tab on re-open; it is not persisted across app restarts.
- Core tabs appear in `settingTabs[]` in registration order (fixed at boot). Plugin tabs appear in `pluginTabs[]` and are always rendered sorted A-Z; the user cannot reorder them.
- `isPluginSettingTab(tab)` returns `true` for both `PluginSettingTab` (community) and `Y0` (internal) instances. This is the routing decision that puts a tab in the plugin section rather than the core section of the sidebar.
- A `Setting` row's `setDisabled(true)` propagates to every attached component — the whole row becomes non-interactive atomically.
- `.obsidian/hotkeys.json` only records user overrides. A command ID absent from the file is at its default binding. Corbomite must not write defaults into this file, or it will mask future Obsidian default changes.
- `.obsidian/core-plugins.json` must be written in object form (`{id: bool}`), not array form. Array form is treated as a migration signal.
- `.obsidian/community-plugins.json` absence is legal and means "safe mode" — do not write an empty array as a sentinel.

---

## 9. Observable user features

- The user can open Settings via the gear icon in the sidebar, any "…" header button, or the global hotkey (`Ctrl/Cmd + ,`).
- The user can navigate between tabs by clicking sidebar entries; the active tab is re-rendered on every switch.
- The user can search settings by typing in the search field inside certain tabs (e.g. Hotkeys, Community Plugins).
- The user can install, enable, disable, and uninstall community plugins from the Community Plugins tab.
- The user can toggle each core plugin on/off from the Core Plugins tab; toggling navigates to that plugin's settings tab.
- The user can assign or clear custom hotkeys for any command from the Hotkeys tab.
- The user can change font, theme, accent colour, and translucency from the Appearance tab; changes apply immediately.
- Plugin settings tabs appear in the sidebar under "Core plugins" (for internal plugins with their own settings) and "Community plugins" (for third-party plugins) sections.
- The user can click on a core plugin in the Core Plugins tab to jump directly to that plugin's settings tab.

---

## 10. Extension surfaces exposed

| Surface | Registration verb | Consumer call site | What plugins supply |
|---|---|---|---|
| Plugin settings tab | `Plugin.addSettingTab(tab: PluginSettingTab)` | `PluginSettingTab.js:111–117` → `app.setting.addSettingTab(tab)` | A `PluginSettingTab` subclass with `display(): void` that builds arbitrary `Setting` rows into `this.containerEl`. |

`display()` is the **only required override**. The tab is automatically removed when the plugin unloads (registered cleanup callback at `Plugin.addSettingTab`).

**Plugins must not hold references to widget DOM nodes beyond the `display()` call** — the next `display()` call empties the container. Use `this.plugin.settings` to save state and re-read it on each `display()`.

---

## 11. Corbomite mapping

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `SettingModal` (tab modal, `app.setting`) | `src/dialogs/SettingsDialog` (KPageDialog) | Partial | Corbomite uses `KPageDialog` with fixed pages; no runtime tab registration API |
| Core tabs (Editor, Files, Appearance, …) | `SettingsDialog::setupEditorPage()`, `setupFilesPage()`, `setupAppearancePage()` | Partial | 4 pages exist; missing Hotkeys, Core Plugins, Community Plugins, About tabs |
| `SettingTab.display()` rebuild-on-open | KPageDialog populates all pages at construction time | Gap | Corbomite does not re-run page setup on open; dynamic content requires manual refresh signals |
| Plugin tab registration (`addSettingTab`) | No equivalent | Missing | Corbomite has no `app.setting` object and no runtime `addPage` API on `SettingsDialog` |
| `Setting` fluent builder | `QFormLayout` + Qt widgets directly | Replaced | Corbomite builds form rows with `QFormLayout::addRow`; no fluent abstraction |
| `SettingGroup` | No equivalent | Missing | Corbomite has no collapsible group primitive in settings |
| `app.vault.setConfig(key, val)` → `app.json` / `appearance.json` | `CorbomiteSettings` (KConfig/KConfigXT) | Partial | Corbomite uses `corbomiterc` / `corbomite-devrc`; vault-level config does not map to per-vault `.obsidian/app.json` |
| `.obsidian/app.json` (vault config) | Not implemented | Missing | Corbomite does not read or write `.obsidian/app.json`; this is the primary Obsidian vault-compatibility gap for settings |
| `.obsidian/appearance.json` | Not implemented | Missing | Not read; Corbomite uses system theme via KDE |
| `.obsidian/hotkeys.json` | Not implemented | Missing | No hotkey persistence in per-vault `.obsidian/` |
| `.obsidian/core-plugins.json` | Not implemented | Missing | Corbomite has no internal-plugin model yet |
| `.obsidian/community-plugins.json` | Not implemented | Missing | No community plugin system |
| `.obsidian/plugins/<id>/data.json` | Not implemented | Missing | No plugin data persistence |

**Key actionable gap:** Corbomite's `SettingsDialog` must grow a `addSettingsPage(id, name, icon, factory)` API before any plugin system can be implemented. The Qt-idiomatic form: `SettingsDialog::addPage(KPageWidgetItem*)` already exists on `KPageDialog` — the missing piece is a runtime-callable slot and a registry that survives dialog re-open.

---

## 12. Markoff gap confirmations / discoveries

N/A — no editor/rendering surface in this domain.

---

## 13. Open questions

1. What is the canonical tab ID and `name` string for each of the 8–9 core setting tabs registered in `utils/apiVersion.js:3602–3610`? The constructor symbols (`N7`, `rte`, `fte`, `ute`, `tte`, `C0`, `K1`, `mte`, `ite`) are minified; their `id` fields are visible for `K1` (`"keychain"`) and `ite` (`"community-plugins"`), but not for the others. Pass 3 should grep `apiVersion.js` for each symbol's `.id =` assignment to build the full canonical ID list.
2. Is `SettingModal.lastTabId` persisted to `localStorage` or any config file across app restarts, or only in-memory for a session? The source only shows `this.lastTabId = e.id` in `openTab`; no persistence call is visible.
3. Does the `SettingTab.hide()` stub get a meaningful override in any core tab? If so, which ones and what do they clean up? Only the Keychain tab's `display()` is visible in `SettingTab.js`; `hide()` was not observed overriding anywhere in the extraction window.
4. What is the full content of the `AC` set that partitions `app.json` vs `appearance.json`? The set is defined outside the `settings` domain (not in the 4 source files). `vault.md` describes the partition but does not enumerate `AC` exhaustively. Pass 3 should locate and list `AC` contents.
5. Does `community-plugins.json` store plugin IDs in insertion order (enabled order), alphabetical, or as an unordered set? Plugin load order may depend on array order.

---

## 14. Recommended Pass 3 synthesis input

1. **Vault-format compat priority:** `.obsidian/app.json`, `.obsidian/appearance.json`, `.obsidian/hotkeys.json`, `.obsidian/core-plugins.json`, and `.obsidian/community-plugins.json` are all read by Obsidian from any vault it opens. Corbomite must implement read-and-preserve (at minimum) for these files to interoperate with Obsidian-managed vaults. Schemas are fully specified in Sections 2 and 3. This should be the first entry in `VAULT-FORMAT.md`.
2. **Plugin-settings API design:** Pass 3 should use this document together with `plugin.md` to draft the Corbomite plugin-settings API: specifically, a `KPageDialog`-backed `SettingsDialog::addPluginPage(pluginId, name, icon, QWidget*)` method with auto-removal on plugin unload. The `PluginSettingTab.display()` contract maps cleanly to a Qt signal `settingsPageAboutToShow()` that the plugin connects to in order to rebuild its widget.
3. **`display()`-on-open pattern:** Pass 3's feature-matrix entry for Settings should flag that Obsidian rebuilds each tab's entire UI on every open. Corbomite's current static `QFormLayout` construction-time approach is fine for built-in pages but will not satisfy the dynamic plugin tab requirement. `GAP-ANALYSIS.md` should capture this as a design constraint for the future plugin settings API.

---

## 15. Cross-domain references

| Other domain | Reference type | Brief description |
|---|---|---|
| `core` | dependency | `App` passed to every `SettingTab` constructor; `app.setting` is the `SettingModal` registry |
| `vault` | dependency | `app.vault.setConfig(key, val)` is how every settings tab persists config; `readConfigJson`/`writeConfigJson` used for `hotkeys.json`, `core-plugins.json` |
| `plugin` | consumer | `Plugin.addSettingTab` is the sole external registration verb; `Plugin.loadData`/`saveData` manages `data.json` |
| `ui/components` | dependency | All `add*` methods on `Setting` construct `ui/components` classes (`ToggleComponent`, `TextComponent`, `DropdownComponent`, etc.) |
| `ui/icons` | dependency | `ExtraButtonComponent` and `ButtonComponent.setIcon` call `setIcon(el, id)` from `ui/icons` |
| `secrets` | peer | `SecretComponent` and the Keychain `SettingTab` implementation appear in the `SettingTab.js` extraction window but belong to the `secrets` domain |

**Short symbols from other domains referenced here:**

| Short symbol | Defined in | Used here for |
|---|---|---|
| `AC` | `vault` domain (partition constant, outside `settings/` source files) | Determines which config keys go to `appearance.json` vs `app.json` at `saveConfig` time |
| `Y0` | `settings/PluginSettingTab.js` (same file, internal) | Internal-plugin settings tab constructor; sibling of `PluginSettingTab` for `app.internalPlugins` |
| `gm` | `core` (i18n/string bundle) | Localised strings used throughout every settings tab's `display()` |
| `ub` | unknown (likely `utils`) | Locale-aware string comparison used to sort plugin tabs A-Z in `updatePluginSection` |
| `nb` | `ui/popups` (Modal subclass) | Base class for Keychain modals visible in `SettingTab.js` extraction window |
