# `obsidian/ui/{components,icons,menu,popups}` — UI primitives bundle (widgets, icons, menus, popups)

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/ui/{components,icons,menu,popups}/`
**File count:** 32 (components 16, icons 5, menu 3, popups 8)
**Files:**
- `components/`: `AbstractTextComponent.js`, `BaseComponent.js`, `ButtonComponent.js`, `ColorComponent.js`, `Component.js`, `DropdownComponent.js`, `ExtraButtonComponent.js`, `MomentFormatComponent.js`, `ProgressBarComponent.js`, `SearchComponent.js`, `SecretComponent.js`, `SliderComponent.js`, `TextAreaComponent.js`, `TextComponent.js`, `ToggleComponent.js`, `ValueComponent.js`
- `icons/`: `addIcon.js`, `getIcon.js`, `getIconIds.js`, `removeIcon.js`, `setIcon.js`
- `menu/`: `Menu.js`, `MenuItem.js`, `MenuSeparator.js`
- `popups/`: `AbstractInputSuggest.js`, `FuzzySuggestModal.js`, `HoverPopover.js`, `Modal.js`, `Notice.js`, `PopoverState.js`, `PopoverSuggest.js`, `SuggestModal.js`

**Pass 1 summary (verbatim from `00-taxonomy.md`, one blockquote per sub-directory):**

> **`obsidian/ui/components` — form & input components.** Form-input widgets used by `Setting`, plugin UIs, modal contents. `Component` is the universal lifecycle base (load/unload + child registration + event-cleanup). `BaseComponent`/`AbstractTextComponent`/`ValueComponent` are abstract bases the concrete inputs extend. Key exports: `Component` — `load`, `unload`, `addChild`, `removeChild`, `register(cb)`, `registerEvent(eventRef)`, `registerDomEvent(el, type, cb)`, `registerInterval(timer)` — base of `Plugin`, `View`, `Modal`, `MarkdownRenderChild`, almost everything. `ButtonComponent`/`ExtraButtonComponent` (buttons). `TextComponent`/`TextAreaComponent`/`SearchComponent`/`SecretComponent` (text inputs). `ToggleComponent`/`DropdownComponent`/`SliderComponent`/`ColorComponent`/`MomentFormatComponent`/`ProgressBarComponent` (typed inputs). `ValueComponent` — abstract base for components with a `getValue/setValue/onChange` contract. No on-disk contracts. Depends on `ui/icons`, `core`; consumed everywhere UI is built. Replaceable — Corbomite uses Qt widgets (`QPushButton`, `QLineEdit`, `QSlider`, `QComboBox`, `QCheckBox`, etc.). `Component` lifecycle ↔ `QObject` parent ownership + `QObject::destroyed`. Pass 2 focus: the `Component` lifecycle contract; only relevant for plugin-API spec.

> **`obsidian/ui/icons` — icon registry.** Tiny registry for Lucide-style SVG icons. Plugins call `addIcon(id, svgString)` to inject; `setIcon(el, id)` writes the SVG into a DOM element; `getIcon(id)` returns the SVG node; `getIconIds()` lists all known IDs; `removeIcon` uninstalls. Primary API: `setIcon(el, id)` — idempotent (no replace if same icon already present). `getIcon(id)`, `getIconIds()` — lookup. `addIcon(id, svg)`, `removeIcon(id)` — registry mutation; built-in IDs use a `lucide-*` prefix. No on-disk contracts. Consumed by `Setting`, `Menu`, `ButtonComponent`, ribbon, status bar — everywhere a button needs glyph. Replaceable — Corbomite uses `QIcon::fromTheme` (KDE icon theme). Plugin-API-wise this should map to a "register your custom icon SVG, retrieve as `QIcon`" wrapper. Pass 2 focus: complete list of built-in `lucide-*` IDs Obsidian ships with (so any plugin asking for one finds it in Corbomite).

> **`obsidian/ui/menu` — context menus.** Obsidian's HTML/CSS context menu (with optional native-menu fallback on macOS). Owns its own `Scope` for arrow-key navigation, supports submenus, sections, checked state, icons, danger styling, custom click handlers. `Menu` — `addItem(cb => MenuItem)`, `addSeparator()`, `setNoIcon()`, `showAtMouseEvent(evt)`, `showAtPosition({x,y})`, `addSections(...)`. Static `Menu.useNativeMenu` toggle. `MenuItem` — `setTitle`, `setIcon`, `setChecked`, `setDisabled`, `setIsLabel`, `setSection`, `setSubmenu`, `setWarning`, `onClick`. `MenuSeparator` — `addSeparator()` returns one. No on-disk contracts. Depends on `core` (Scope), `platform`, `ui/icons`; consumed by `workspace`, every right-click in the app, plugin-built UIs. Replaceable — Corbomite uses `QMenu` directly. The plugin-API-equivalent must offer a builder pattern over `QMenu`. Pass 2 focus: the `addSections` ordering protocol (Workspace fires `file-menu`/`url-menu`/`editor-menu` events letting plugins add sections to a menu mid-construction); the ordered section IDs.

> **`obsidian/ui/popups` — modals, suggesters, hover popovers, notices.** Five flavours of overlay UI: `Modal` — full-screen dim-bg dialog with title/header/content; owns its own Scope, animation, Esc-to-close. `Notice` — toast notifications, top-right of viewport, auto-dismiss. `SuggestModal` / `FuzzySuggestModal` — text-input + scrolling suggestion list (quick switcher pattern). `PopoverSuggest` / `AbstractInputSuggest` — anchored popover suggesters (used inside form inputs). `HoverPopover` — the hover-preview popover (e.g. mouse over a `[[link]]` to preview the target note); has a focus-mode for pinning. `PopoverState` — internal state-machine constants. Key exports: `Modal` — `open()`, `close()`, `onOpen()`, `onClose()`, `containerEl`, `contentEl`, `titleEl`, `scope`, `setTitle`. `Notice` — `new Notice(text, durationMs)`; `setMessage`, `hide`. `SuggestModal<T>` — abstract `getSuggestions(query)`, `renderSuggestion(item, el)`, `onChooseSuggestion(item, evt)`. `PopoverSuggest`/`AbstractInputSuggest` — anchored popover suggesters. `HoverPopover` — owned by parent `Component`; `containerEl`, `hoverEl`, `isFocused`. Consumed by `workspace` (quick switcher, command palette), plugins, hover-link rendering. Replaceable — Corbomite uses `KMessageBox`, `QDialog`, `KMessageWidget` (notice-equivalent), `QCompleter` (suggester-equivalent), `QToolTip`. `QuickSwitcher.cpp` already shadows `FuzzySuggestModal`. Pass 2 focus: the hover-popover lifecycle and the SuggestModal scope-vs-input keyboard handling.

**De-minifier artifact note:** Heavy collisions.
- `components/`: 14 of 16 files are **byte-identical** (same extracted chunk, lines 68774-69546; only `// public API symbol` comment differs). Canonical = `BaseComponent.js`. `Component.js` (44715-44784) and `SecretComponent.js` (159325-160472) are distinct; `SecretComponent.js` is mis-extracted and contains the `SecretComponent` widget at `:478-550` plus adjacent out-of-scope keychain/release-notes code.
- `menu/`: `MenuItem.js` and `MenuSeparator.js` are byte-identical (60392-60523); canonical = `MenuItem.js`, which trails with a four-liner `MenuSeparator`. `Menu.js` (60530-61084) is distinct.
- `popups/`: `Notice.js`, `SuggestModal.js`, `FuzzySuggestModal.js` all equal (63514-64027), containing `ob → ab → SuggestModal → FuzzySuggestModal → Notice` in sequence. Canonical = `SuggestModal.js`. `AbstractInputSuggest.js` and `PopoverSuggest.js` are byte-identical (99017-99683); canonical = `PopoverSuggest.js`. `PopoverState.js` is mis-extracted — it holds Obsidian Publish site nav/outline/password-modal code (137213-137878), not the enum. The real `PopoverState` enum (members `Hidden`/`Showing`/`Shown`/`Hiding`) is defined elsewhere in the bundle (`tree/_internal.js` has 124 references); treat `PopoverState.js` as out-of-scope and document the enum from observed use sites. `Modal.js` and `HoverPopover.js` are distinct canonicals.
- `icons/`: all five files tiny, distinct, canonical.

---

## 1. Public API surface

### `obsidian/ui/components`

#### `Component` (`components/Component.js:5-74`)

- **Kind:** class (constructor-function transpile).
- **Exported as:** `Component` (via `// public API symbol: Component`).
- **Signature:** `new Component()`; instance fields `_loaded: boolean`, `_children: Component[]`, `_events: Array<() => void>`. Methods `load()`, `unload()`, `onload()`, `onunload()`, `addChild(c: Component): Component`, `removeChild(c: Component): Component`, `register(cb: () => void)`, `registerEvent(eventRef: EventRef)`, `registerDomEvent(el: EventTarget, type: string, handler: EventListener, options?)`, `registerScopeEvent(keymapEventHandler)`, `registerInterval(id: number): number`.
- **Purpose:** universal lifecycle+ownership primitive. `load()` recurses into children and calls `onload()`; `unload()` pops children LIFO and calls every registered cleanup thunk, then `onunload()`.
- **Lifecycle:** constructed by subclass; loaded manually or by a parent's `addChild`. `unload()` is safe to call at any time (idempotent behind `_loaded`). `addChild(c)` auto-loads the child if the parent is already loaded; `removeChild(c)` auto-unloads it.
- **Mixes in:** neither. `Component` is the base class; `Events` is a separate mixin. Classes like `Workspace` extend both (`Workspace` IS a `Component` AND has `Events`-style `.on/.off/.trigger`).

#### `BaseComponent` (`components/BaseComponent.js:159-172`)

- **Kind:** class. Fields `disabled: boolean`. Methods `then(cb: (self) => void): this`, `setDisabled(flag: boolean): this`.
- **Purpose:** shared base for non-lifecycle-bound form widgets (they hang off a DOM element, not a Component tree).
- **Mixes in:** neither. Does NOT extend `Component`.

#### `ValueComponent` (`components/BaseComponent.js:173-190`)

- **Kind:** abstract class extending `BaseComponent`. Adds `registerOptionListener(record: object, key: string): this` — sets `record[key] = (v?) => v === undefined ? getValue() : (setValue(v), getValue())`.
- **Purpose:** any widget that has a `getValue`/`setValue`/`onChange` contract. Subclasses supply all three.

#### `AbstractTextComponent` (`components/BaseComponent.js:361-398`)

- **Kind:** abstract class extending `ValueComponent`. Constructed over an `HTMLInputElement` or `HTMLTextAreaElement`. Sets `spellcheck="false"` on the input. Implements `getValue()`, `setValue(s)`, `setPlaceholder(s)`, `onChanged()` internal, `onChange(cb)` public.

#### Concrete form widgets (all in `components/BaseComponent.js`)

Each is a `BaseComponent`/`ValueComponent`/`AbstractTextComponent` subclass with a chainable builder API. Constructor signature is `new FooComponent(parentEl: HTMLElement)`; the widget builds its DOM and attaches it to `parentEl`. Methods all return `this` unless annotated.

- **`ButtonComponent`** (`:191-257`): `<button>`. `setButtonText(s)`, `setIcon(name)`, `setTooltip(s,opts?)`, `setClass(c)`, `setCta()`/`removeCta()`, `setWarning()`, `setLoading(flag)`, `onClick(async cb)`. Async click auto-toggles `.mod-loading` around the await.
- **`ExtraButtonComponent`** (`:259-293`): icon-only `<div class="clickable-icon extra-setting-button">`, default `lucide-settings`. `setIcon`, `setTooltip`, `onClick`.
- **`ToggleComponent`** (`:295-359`): `<label class="checkbox-container"><input type="checkbox"></label>`. `on: boolean`, `getValue()`, `setValue(flag)` (fires onChange only on actual change), `setSmall()`, `setTooltip`, `onChange(cb)`. Calls `navigator.vibrate(100)` on click (mobile).
- **`TextComponent`** (`:399-412`): `<input type="text">`. Adds `autoSelect(selectAll=false)`.
- **`SearchComponent`** (`:414-461`): `<div class="search-input-container"><input type="search" enterkeyhint="search"><div class="search-input-clear-button"></div></div>`. Clear empties + focuses. `setClass(c)`, `addRightDecorator(cb)`.
- **`TextAreaComponent`** (`:463-467`): `<textarea>`.
- **(unnamed `$k`, internal)** (`:469-493`): numeric `TextComponent` (`type="number"`, `getValueAsNumber`, `setValueAsNumber`, `setLimits(min,max,step)`). May be exported as `NumberComponent` without the `// public API symbol` comment — see §13.
- **`MomentFormatComponent`** (`:495-533`): extends `TextComponent`. Fields `defaultFormat`, `sampleEl?`. `setDefaultFormat(fmt)` (sets placeholder + sample), `setSampleEl(el)`. Every input event calls `updateSample()` → `window.moment().format(inputEl.value || defaultFormat)` → writes into `sampleEl`. Live format-preview widget.
- **`DropdownComponent`** (`:535-585`): `<select class="dropdown">`. `addOption(v,t)`, `addOptions(dict)`, `getValue()`, `setValue(v)`, `onChange(cb)`.
- **`ProgressBarComponent`** (`:586-616`): `<div class="setting-progress-bar"><div class="setting-progress-bar-inner"/></div>`. `setValue(0-100)` → inner width %. `setVisibility(bool)`. No onChange.
- **`SliderComponent`** (`:618-711`): `<input type="range" class="slider" data-ignore-swipe="true">`. `setLimits(min,max,step)`, `setInstant(flag)` (fire on `input` vs `change`), `getValue()`/`getValuePretty()` (2-decimal if step<1), `setDynamicTooltip()` (`displayTooltip(el, value, {placement:"top"})` on hover).
- **`ColorComponent`** (`:713-776`): `<input type="color">`. `getValue()`/`setValue()` in `#rrggbb`; also `getValueRgb`/`getValueHsl`/`getValueInt` plus matching setters (`setValueInt` hex-pads to 6 chars).

#### `SecretComponent` (`components/SecretComponent.js:478-550`)

- **Kind:** class extending `BaseComponent`. Constructor takes `(app, parentEl)`. Shows a warning icon if `app.secretStorage.isEncryptionAvailable()` is false; a value-placeholder div (bullets or toggle-to-reveal); a `<button>` that opens either `$1` (secret picker modal) or `Y1` (new-secret modal) depending on whether secrets exist.
- **Purpose:** one-tap "link this setting to a keychain-stored secret". `getValue()` returns a secret ID; `app.secretStorage.peekSecret(id)` resolves the real value.
- **Used by:** any setting that needs a password/API-key/OAuth-token — cross-domain to `secrets`.

#### `Setting` (also in this source chunk, `:5-113`)

- **Kind:** class (declared in `components/BaseComponent.js` but owned by `obsidian/settings/` — shown here because the de-minifier extracted the chunk). Out-of-domain; see `obsidian/settings/` Pass-2 doc.

### `obsidian/ui/icons`

#### `setIcon(el, name)` (`icons/setIcon.js:5-12`)

- **Kind:** function. Idempotent: if the first child is already an SVG with a class matching `name`, no replacement. Otherwise detaches first child (if any) and appends `getIcon(name)`.

#### `getIcon(name)` (`icons/getIcon.js:5-27`)

- **Kind:** function. Returns a fresh `<svg>` DOM node or `null`. Resolution order:
  1. If `name.startsWith("lucide-")`: cache-lookup on `Jm(name, factory)`; factory builds from `Um` (Lucide SVG path table).
  2. If `name` is in `Xm` (`addIcon`-registered custom icons): wraps in a blank `<svg>` element via `$m` attr preset.
  3. If `name` is in `Qm` (Obsidian's legacy/built-in non-Lucide icons): blank `<svg>` via `Wm` preset.
  4. Otherwise check `Ym` (alias map, short-name → canonical Lucide key) and retry step 1 with `"lucide-" + resolved`.
  - Returns `null` on miss.

#### `addIcon(name, svg)` (`icons/addIcon.js:5-7`)

- **Kind:** function. `delete Zm[name]` (cache bust), `Xm[name] = svg` (store). `svg` is the inner HTML of the SVG body (viewBox/width/height come from the `$m` attr preset).

#### `removeIcon(name)` (`icons/removeIcon.js:5-7`)

- **Kind:** function. `delete Zm[name]; delete Xm[name];` — note: removes custom-registered icons only; Lucide (`Um`) is immutable.

#### `getIconIds()` (`icons/getIconIds.js:5-14`)

- **Kind:** function. Returns the full list of known icon IDs: every key of `Um` prefixed with `lucide-`, plus every key of `Xm` (custom), plus every key of `Qm` (built-in non-Lucide). Aliases in `Ym` are NOT listed.

### `obsidian/ui/menu`

#### `Menu` (`menu/Menu.js:5-559`)

- **Kind:** class extending `Component`. Constructor takes no args. Statics: `Menu.useNativeMenu: boolean`, `Menu.forEvent(evt): Menu` (lazy per-event singleton via internal `WeakMap<Event, Menu>`).
- **Methods:** `addItem(fn: (mi) => void)`, `addSeparator()`, `addSections(ids: string[])` (unknown ids append before the `""` bucket), `setSectionSubmenu(id, {title, icon?, disabled?})`, `setNoIcon()`, `setUseNativeMenu(flag)`, `setShowMacWritingTools(flag)`, `setParentElement(el)`, `showAtMouseEvent(evt)`, `showAtPosition({x,y,width?,overlap?,left?}, doc?)`, `hide()`/`close()`, `onHide(cb)`.
- **Lifecycle:** `showAtPosition` → `unload()` (idempotent reset) → `sort()` (see §2) → appends DOM → `load()` via `setTimeout(…,0)`. `hide()` detaches, unloads, pops scope. Never persists.
- **Scope:** owns a `Scope` with arrow/Enter/Escape handlers; pushed onto `Keymap.global` in `onload`.

#### `MenuItem` (`menu/MenuItem.js:5-131`)

- **Kind:** class. `MenuItem.create(menu)` static factory.
- **Signature:** `new MenuItem(parentMenu: Menu)`; attached to the menu during construction. Methods (all chainable, return `this`): `setTitle(str | HTMLElement)`, `setIcon(name)` / `removeIcon()`, `setChecked(flag)` (adds a secondary check-icon Lucide `check`), `setActive(flag)` (alias for `setChecked`), `setDisabled(flag)`, `setWarning(flag)` (adds `.is-warning`), `setIsLabel(flag)` (adds `.is-label`, removes `.tappable`), `setSection(id)` (sets `data-section` attr), `setSubmenu(): Menu` (lazily creates a submenu, returns it), `onClick(cb)`.
- **Phone mobile:** clicking a submenu item pushes a back-button row into the submenu and animate-slides in (see `:90-112`).

#### `MenuSeparator` (`menu/MenuItem.js:133-135`)

- **Kind:** two-field class literal. `{ menu, dom }`. `dom = createDiv("menu-separator")`. No methods.

### `obsidian/ui/popups`

#### `Modal` (`popups/Modal.js:5-275`)

- **Kind:** class (NOT a `Component`; owns a `Scope` with manual push/pop on `open()/close()`).
- **Signature:** `new Modal(app: App)`. Fields `containerEl`, `bgEl`, `modalEl`, `headerEl`, `titleEl`, `contentEl`, `scope`, flags `shouldRestoreSelection=true`, `shouldAnimate=true`, `dimBackground=true`, `bgOpacity="0.85"`, `selection=null`, `win=null`.
- **Methods:** `open()`, `close()`, `onOpen()`/`onClose()` (overrides), `onClickOutside(evt)`, `onEscapeKey(evt)`, `setTitle`, `setContent(string|HTMLElement)`, `setBackgroundOpacity`, `setCloseCallback`, `setDimBackground`. Mobile: swipe-down-to-dismiss on title bar.
- **Selection restore:** on `open`, snapshots `(win, range, focusEl)`; on `close` with `shouldRestoreSelection`, refocuses + restores range (skipped inside CodeMirror `.cm-content`).

#### `nb` / `ib` (internal confirm/prompt, `popups/Modal.js:277-409`)

- `nb` extends `Modal`; adds `buttonContainerEl` + `addCheckbox`/`addButton`/`addCancelButton`. Buttons auto-close unless handler returns truthy.
- `ib` extends `nb`; adds Enter-accept + promise-based `prompt(): Promise<boolean>`. Used for `KMessageBox::questionYesNo`-equivalent dialogues.

#### `SuggestModal<T>` (`popups/SuggestModal.js:231-341`)

- **Kind:** class extending `Modal`. Abstract; subclasses override `getSuggestions(query): T[] | Promise<T[]>`, `renderSuggestion(item: T, el: HTMLElement)`, `onChooseSuggestion(item: T, evt: MouseEvent | KeyboardEvent)`.
- **Key methods:** `setPlaceholder(s)`, `setInstructions([{command, purpose}])` (renders the footer hint row), `setSuggestions(arr)` (internal; called by `updateSuggestions`), `updateSuggestions()` (re-runs `getSuggestions` and populates), `onInput()` (override hook, default calls `updateSuggestions`), `onNoSuggestion()` (override; default writes `emptyStateText`), `selectActiveSuggestion(evt)`.
- **Limit:** `this.limit = 100` default, slices results.
- **Internal chooser:** an `ob` instance (see below) owns the keyboard nav/scroll.

#### `ob` / `ab` (internal list scaffolding, `popups/SuggestModal.js:5-230`)

- `ob`: arrow/PgUp/PgDn/Home/End/Ctrl-P/Ctrl-N/Enter/mouse + `setSelectedItem(n, evt?)` with wrap-around; `numVisibleItems = floor(clientHeight/rowHeight)`. Modal owns the input, `ob` owns the list.
- `ab` extends `ob`; partitions by `.group` into `<div class="suggestion-group" data-group="…">` wrappers.

#### `FuzzySuggestModal<T>` (`popups/SuggestModal.js:342-380`)

- Extends `SuggestModal<FuzzyMatch<T>>`. Abstract subclass contract: `getItems(): T[]`, `getItemText(item): string`, `onChooseItem(item, evt)`. Default `getSuggestions(q)` runs `fuzzySearch(prepareQuery(q), text)` (see `search` domain) for each item, sorts via `sortSearchResults(results)`, returns `{match, item}` records. Default `renderSuggestion({item, match}, el)` calls `renderResults(el, text, match)` (from `rendering/`). Default `onChooseSuggestion({item}, evt)` → `onChooseItem(item, evt)`.

#### `Notice` (`popups/SuggestModal.js:394-518`)

- **Kind:** class. Not a Modal; no Scope.
- **Signature:** `new Notice(message: string | DocumentFragment, durationMs: number = 4000)`. `durationMs = 0` means "no auto-hide". Appended to a per-window `.notice-container` in body.
- **Methods:** `setMessage(text)`, `setAutoHide(ms)`, `addButton(text, cb)` (adds a `.notice-cta` clickable; clicking runs cb then hides), `hide()`.
- **Behaviour:** 350 px slide-in on desktop (`translateX(350px)` → `""`), translateY on mobile. Hover pauses auto-hide until mouseleave + 1 s. Click-anywhere hides. Stacking: all active notices share the per-window container; on last-hide, the container is detached.

#### `PopoverSuggest` (`popups/PopoverSuggest.js:43-118`)

- **Kind:** class (base). Owns its own `Scope` (child of `app.scope` or passed-in parent). Fields `suggestEl` (`<div class="suggestion-container"><div class="suggestion"></div></div>`), `suggestions: ob`, `isOpen`.
- **Methods:** `open()`, `close()`, `attachDom()` / `detachDom()` (override points for subclasses with custom placement), `reposition(anchorRect, horizontalAlignment?)` (uses `Nv(…, {gap:5, preventOverlap:true, horizontalAlignment})` placement helper), `setAutoDestroy(el)` (uses `Rv(el, 500ms, cb)` to close the popover 500 ms after `el` leaves the DOM).

#### `AbstractInputSuggest<T>` (`popups/PopoverSuggest.js:412-523`)

- **Kind:** abstract class extending `PopoverSuggest`. Attached to an `<input>` / contenteditable. Constructor `(app, textInputEl)` wires `input`/`focus`/`blur` listeners.
- **Abstract contract:** `getSuggestions(query: string): T[] | Promise<T[]>`, `renderSuggestion(item, el)`, `selectSuggestion(item, evt?)` (default invokes the `onSelect` callback).
- **Limit:** `this.limit = 100`; slices results.
- **Reposition on scroll:** attaches a capturing `scroll` listener on the input's document and calls `reposition` if the anchor rect shifts.
- **Difference from `SuggestModal`:** modal with its own input & overlay vs a popup anchored to an existing input. Plugins subclass one or the other depending on whether the picker is the entire screen (SuggestModal) or a decoration on a form (AbstractInputSuggest).

#### Concrete `AbstractInputSuggest` subclasses (internal, `popups/PopoverSuggest.js:524-671`)

`xI` file-path (pred-filtered, cap 100), `TI` md-only, `DI` predicate-bound, `AI` folder (with `includeRoot`/`allowNullSelection`), `PI` folder-predicate. Used by "default new-file location" and similar settings.

#### `HoverPopover` (`popups/HoverPopover.js:72-303`)

- **Kind:** class extending `Component`. `new HoverPopover(parent: Component, targetEl: HTMLElement, waitTime = 300, staticPos? = null)`.
- **State:** `state: PopoverState` (`Showing → Shown → Hiding → Hidden`; §2). Starts in `Showing`; schedules `show()` via `setTimeout(…, waitTime)`.
- **`show()`**: position → `load()` → 80 ms fade-in. If `targetEl.offsetHeight > 300`, anchors to mouse position (`staticPos`) not target rect.
- **`hide()`**: detach, unload, cascade to `childHovers`.
- **Global state (module-scoped):** `XQ` (pending), `$Q` (visible). A 500 ms poll (`nX`, `:35-60`) runs `detect()` on each visible popover with `elementFromPoint(mouseX, mouseY)` — this drives all "should stay open?" logic without per-popover mousemove listeners. Click/contextmenu outside the popover tree hides.
- **Pin:** `setIsFocused(true)` short-circuits `shouldShowSelf()`; Page-Preview plugin sets this when user holds the configured modifier.
- **Children:** `childHovers` (`:132-143`) filters `$Q` to popovers whose `targetEl` lives inside `this.hoverEl`. Parent's `shouldShow()` stays true while children are open.

---

## 2. Data structures

### `obsidian/ui/components` — `Component` internals

```typescript
// Component (components/Component.js:5-74)
{
  _loaded: boolean;       // initial false
  _children: Component[]; // LIFO drained on unload
  _events: Array<() => void>; // cleanup thunks, LIFO drained on unload
}
```

Invariants: `_children` is drained *before* `_events`; each pop invokes the child's `unload()`. `registerDomEvent(el, type, cb, opts)` pushes `() => el.removeEventListener(type, cb, opts)`. `registerInterval(id)` pushes `() => clearInterval(id)` and returns `id`. `registerEvent(eventRef)` pushes `() => eventRef.e.offref(eventRef)` (so the `Events` emitter's internal list is cleaned). The contract is that any callback registered after `load()` gets auto-cleanup on `unload()`; callbacks registered before first `load()` also work.

### `obsidian/ui/menu` — section registry per-Menu

```typescript
// Menu (menu/Menu.js)
{
  items: (MenuItem | MenuSeparator)[];   // insertion order pre-sort; section-grouped post-sort
  sections: string[];                    // declared order; unknown ids appended by addSections()
  submenuConfigs: Record<sectionId, {title, icon?, disabled?}>;
  scope: Scope;  parentEl?; parentMenu?; currentSubmenu?;
  useNativeMenu: boolean;                // defaults to Menu.useNativeMenu
}

// MenuItem (menu/MenuItem.js)
{
  menu: Menu;
  section: string;          // "" = uncategorised
  title: HTMLElement;       // .menu-item-title
  icon?: string;            // Lucide name
  submenu?: Menu | null;    // lazy via setSubmenu()
  disabled: boolean;  checked: boolean | null;
  callback?: (evt: MouseEvent) => void;
}
```

**Canonical section orders (from `Menu.addSections(…)` call sites):**

- Folder context (`workspace/WorkspaceContainer.js:55-66`): `["title", "open", "action-primary", "action", "info", "info.copy", "view", "system", "", "danger"]`.
- Tab/leaf more-options (`ItemView.onMoreOptionsMenu`, see `views.md`): `["close", "pane", "open", "action", "find", "info", "info.copy", "view", "view.linked", "system", "", "danger"]`.
- Markdown-preview viewport (`MarkdownPreviewView.js:197`, `MarkdownView.js:1298`): `["view", ""]` — thin skeleton; plugins populate via `markdown-viewport-menu`.
- Leaf tab (`WorkspaceLeaf.js:80-128`): `action`, `close`, `tablist` (no explicit `addSections`).
- Tab-list "…" (`WorkspaceLeaf.js:1383`): `title` + `order` + `ribbon`.
- Bases cell (`bases/Value.js:62`): `action` + `danger` universal; rest cell-type specific.

**Sort algorithm** (`Menu.prototype.sort`, `Menu.js:96-180`):
1. Bucket `items` by `item.section`. Unknown sections auto-push onto `sections[]`.
2. Walk `sections[]`; insert `MenuSeparator` between non-empty sections.
3. `submenuConfigs[id]` present → wrap that section in a child `Menu` via `setSubmenu()`.
4. `sections[""]` items only appear if `""` is explicit in `sections[]`; otherwise appended at end (`:166-170`).
5. Trailing separators popped (`:171`). Items between separators wrapped in a `.menu-group` div.

### `obsidian/ui/popups` — `PopoverState` enum

```typescript
// PopoverState (defined in tree/_internal.js, extensively used by HoverPopover)
enum PopoverState {
  Hidden = 0,
  Showing = 1,
  Shown = 2,
  Hiding = 3,
}
```

Inferred from use sites: `HoverPopover` starts in `Showing` (construction), transitions `Showing → Shown` on `show()`, `Shown → Hiding` when `shouldShow()` goes false (starts `waitTime` timer to confirm), `Hiding → Shown` if hover re-enters, `any → Hidden` on `hide()`. Exact numeric values not verified; order above matches the transition semantics observed in `HoverPopover.js`.

### `obsidian/ui/popups` — Notice-stacking globals

```typescript
// Per-window Notice container
db: WeakMap<Window, HTMLDivElement>;  // "notice-container" singleton per window
```

`new Notice(…)` looks up `db.get(activeWindow)`, creates the container on first use, detaches it when empty on hide. Every `Notice` is a child `<div class="notice">` in that one container.

### `MomentFormatComponent` — format-string contract

`updateSample()` calls `window.moment().format(inputEl.value || defaultFormat)`. Format grammar is **Moment.js verbatim** (global `window.moment`, see `utils/moment.js`); no Obsidian-specific extensions inside the component. `{{date:YYYY-MM-DD}}` template resolution happens in Templates/Daily-Notes/Templater (out of domain), which call `moment().format(fmt)` at placeholder time. **Compatibility:** Corbomite must parse Moment tokens exactly — `YYYY/MM/DD/HH/mm/ss/dddd/MMMM/Do/X/x/LL/LLL/LLLL`, `[literal]` escape, `Q` quarters, `W/WW` ISO weeks, `e/E` locale day-of-week, `Z/ZZ` offsets. `QDateTime::toString` uses different tokens (`yyyy` lowercase, `d` not `D`, `AP` vs `A`). Corbomite needs a dedicated Moment-syntax adapter.

---

## 3. On-disk contracts

`No on-disk contracts.`

None of these four sub-directories reads or writes files. Some public consumers do — `MomentFormatComponent` values are frequently persisted in `.obsidian/daily-notes.json`, plugin `data.json`, and Templater settings, but the component itself is purely a string in/out control. Icons registered by `addIcon` are in-memory only (plugins re-register on every load). Menu state is not persisted. Modal state is not persisted. Notices are transient. `HoverPopover` is transient.

---

## 4. Events emitted

`No events emitted.`

None of the classes in this domain mix in `Events`. `Component` hosts an event *cleanup queue* (`_events`) but does not *emit*; it only receives `EventRef`s from other emitters (`Workspace`, `Vault`, `MetadataCache`) via `registerEvent`. Menu has a `hideCallback` set through `onHide(cb)` — a single-listener callback, not an `Events`-pattern emitter. Modal has an `onCloseCallback` of the same single-listener shape. Notice emits nothing.

---

## 5. Events consumed

| Listener | Subscribes | Why |
|---|---|---|
| `Component.register{Event,DomEvent}` (`Component.js:48-58`) | EventRefs from `Events`-derived emitters; any DOM `addEventListener` | auto-cleanup on unload |
| `Menu.onload` (`Menu.js:60-73`) | `mousedown`/`click`/`contextmenu` on owning window | auto-hide on click outside menu tree |
| `Modal.open` (`Modal.js:156-157`) | `beforeunload` on popped-out windows | close modal if window torn down |
| `ob` ctor (`SuggestModal.js:14-28`) | delegated `click`/`auxclick`/`mousemove` on `.suggestion-item` | selection + choose |
| `HoverPopover` globals (`HoverPopover.js:37-43, 61-70`) | window `click`, `contextmenu` (capture), `mousemove` | hover-state bookkeeping |
| `HoverPopover` per-popover (`:89-97`) | `mouseover`/`mouseout` on `targetEl`/`hoverEl` | show/hide |
| `AbstractInputSuggest` (`PopoverSuggest.js:419-424, 506-509`) | input `input`/`focus`/`blur`, capturing `scroll` | input-driven updates, auto-reposition |

Indirect via `Scope.register`: Modal (`Escape`), Menu (arrows, Enter, Escape), SuggestModal/`ob` (arrows, PgUp/PgDn, Home/End, Enter, `Ctrl+P`/`Ctrl+N` on macOS/iOS).

---

## 6. Commands registered

`No commands registered here.`

None of these files call `commands.addCommand`. Commands that open modals/suggesters (e.g. `app:open-quick-switcher`) are registered in their owning internal plugins (`internal-plugins/switcher`, etc.).

---

## 7. Registries owned

### Icon registry (`obsidian/ui/icons`)

- **Stores:** four module-level maps — `Um` (Lucide SVG paths, immutable), `Xm` (`addIcon`-registered custom SVG inner-HTML, mutable), `Qm` (legacy non-Lucide built-ins, effectively immutable), `Ym` (alias map short-name → canonical Lucide key), plus `Zm` (SVG-node memo cache).
- **Populated by:** core bundle baking (`Um`/`Qm`/`Ym`); plugins via `addIcon(name, svg)` into `Xm`.
- **Read by:** `getIcon` / `setIcon` / `getIconIds` — every button/menu/ribbon/status-bar/breadcrumb/plugin glyph.
- **Persistence:** in-memory only. Plugins re-register on `onload`.
- **Lifecycle:** `removeIcon(name)` deletes from `Xm` + `Zm` only, never `Um`/`Qm`. `addIcon("lucide-…", …)` loses to built-in because the `.startsWith("lucide-")` branch runs first.

### Section registry (per-`Menu` instance, `obsidian/ui/menu`)

- **Stores:** `sections: string[]` (declared order) + `submenuConfigs: Record<id, {title, icon?, disabled?}>` (sections rendered as a submenu).
- **Populated by:** `menu.addSections(ids)` (idempotent, dedupes); `menu.setSectionSubmenu(id, config)`.
- **Read by:** `Menu.prototype.sort` (`Menu.js:96-180`) at show time.
- **Persistence:** per-invocation only.
- **Lifecycle:** called by first-party view code immediately before `workspace.trigger("file-menu" / "editor-menu" / …, menu, …)`; plugins slot via `item.setSection(id)`.

### AddIcon custom icon table

- **Stores:** `Xm: Record<name, svgInnerHtml>`.
- **Populated by:** `Plugin.addIcon(name, svg)` → `icons/addIcon.js:5`. No auto-`removeIcon` on plugin unload (see §13 Q5).
- **Scope rule:** public plugin-facing → also §10 / `02-extension-surfaces.md`.

---

## 8. Invariants

- `Component.unload()` is **idempotent** (guarded by `_loaded`). Children drain LIFO via `unload()` *before* the event-cleanup queue; subclass `onunload()` runs last.
- `Component.addChild(child)` auto-loads the child if this is already loaded. No second `onload` fires when this loads later.
- `Component.registerDomEvent(el, type, cb, opts)` stores the same opts for both add- and remove-listener. Safe for `{ once: true }` handlers.
- `Menu` is a `Component`. `Menu.addItem(cb)` is a **silent no-op after load** (`Menu.js:206`) — plugins that add items post-`await` in a `file-menu` listener silently drop. `Menu.sort()` runs once per `showAtPosition`; `setSection` post-sort mutates the DOM attribute only. `MenuItem.setSection` defaults to `""`; items in `""` appear at end unless `""` is explicit in `sections[]` (then they get a separator).
- `setIcon(el, name)` is idempotent on same-class firstChild. `getIcon(unknown)` returns `null` (callers must guard). `addIcon("lucide-…")` loses to the built-in because Lucide resolution runs first.
- `removeIcon(name)` is a silent no-op for Lucide/`Qm` entries.
- `Modal.open()` / `close()` are both idempotent via a `containerEl.parentNode` guard.
- `Modal.shouldRestoreSelection = true` restores both focus widget **and** `QTextCursor` range (skipped for `.cm-content`). Qt's `QDialog` only restores focus.
- `SuggestModal.isOpen` is separate from Modal's — override chains must call `super.onOpen/onClose`.
- `FuzzySuggestModal.getSuggestions` handles async only if `getItems()` is sync; full-async plugins must override `getSuggestions` directly.
- `Notice(text, 0)` disables auto-hide. Hover freezes the timer; leave extends by 1 s. No cap on stacked notices.
- `HoverPopover.state === Hidden` is terminal; a new hover constructs a new instance. At most one popover per `parent.hoverPopover` — chain-of-popovers is only via `childHovers` (popover anchored inside another popover), not siblings on the same parent.
- `AbstractInputSuggest.selectSuggestion` does NOT auto-update the input value — subclasses/`onSelect` must.
- `PopoverState.Hidden === 0` (inferred from `!state` truthy-checks).

---

## 9. Observable user features

- Every right-click menu is a `Menu`; arrows navigate, Enter chooses, submenus open on 250 ms hover (`openSubmenuSoon` debounce) or right-arrow.
- macOS-native menu fallback via `Menu.useNativeMenu`.
- Modals dim background, trap focus, close on Esc / click-outside, restore prior selection on close.
- Quick Switcher, Command Palette, Template Picker — all `FuzzySuggestModal<T>` subclasses with fuzzy match + highlight spans + `Ctrl+P`/`Ctrl+N` on macOS.
- Hover any `[[link]]` for 300 ms → `HoverPopover` with markdown preview; configured mod-key pins (`setIsFocused`); hovering a link inside spawns a child popover kept open while parent is visible.
- Toast notifications via `new Notice(...)`; mouse-hover pauses auto-hide.
- Build-your-own form rows via `Setting` + `*Component` widgets, consistent look with descriptions + disabled state.
- Live Moment.js date-format preview inside Daily Notes / Templates settings via `MomentFormatComponent.setSampleEl`.
- Plugin-supplied icons via `addIcon` instantly usable in ribbons/menus/status-bar.
- Inline-input autocomplete via `AbstractInputSuggest` for file-path/folder-path/template-name input fields in settings.
- Sliders show a dynamic tooltip above the thumb while dragging (`SliderComponent.setDynamicTooltip`).

---

## 10. Extension surfaces exposed

| Surface | Registration verb | Consumer | Plugins supply |
|---|---|---|---|
| Custom Component subclass | `class extends Component` | `Plugin.addChild`, `View`/`Modal`/`MarkdownRenderChild` ctors | lifecycle-scoped DOM/event ownership. Plugins rarely subclass `Component` directly — they subclass `MarkdownRenderChild`/`Plugin`/`View`/`Modal`. |
| Custom icon | `Plugin.addIcon(name, svgInnerHtml)` → `icons/addIcon.js:5` | all icon consumers | SVG inner-HTML; wrapper provides viewBox. No auto-cleanup in `Plugin.unload` (see §13 Q5). |
| Menu items on context menus | `workspace.on("file-menu"/"url-menu"/"editor-menu"/"files-menu"/"leaf-menu"/"tab-group-menu"/"markdown-viewport-menu", cb)` | `Workspace.trigger(…)` from many call sites (see `workspace.md` §4) | `(menu, …ctx) => void` that calls `menu.addItem(i => i.setTitle(…).setIcon(…).setSection(…).onClick(…))`. |
| New menu section | `menu.addSections([...ids])` | `Menu.sort()` | unknown ids auto-land at end via `sort` fallback. |
| Section-as-submenu | `menu.setSectionSubmenu(id, {title, icon?, disabled?})` | `Menu.sort()` | rarely used by plugins. |
| Modal subclass | `class extends Modal` + `new MyModal(app).open()` | ad-hoc | override `onOpen()` to populate `contentEl`. |
| Suggest modal | `class extends SuggestModal<T>` or `FuzzySuggestModal<T>` | `new MySuggest(app).open()` | `getSuggestions(q)` / `getItemText(item)` / `renderSuggestion(item, el)` / `onChooseSuggestion(item, evt)` / `onChooseItem(item, evt)`. |
| Input-anchored suggester | `class extends AbstractInputSuggest<T>` | `new MySuggest(app, inputEl)` | `getSuggestions`, `renderSuggestion`, `selectSuggestion` / `.onSelect(cb)`. |
| Notice | `new Notice(message, durationMs)` + chainable `.addButton(text, cb)` | singleton stacker | no override surface. |
| Hover-popover source | `workspace.registerHoverLinkSource(id, {display, defaultMod})` | Page-Preview internal plugin | `{display, defaultMod}`; plugin still constructs `HoverPopover` directly. See `workspace.md` §7. |

Cross-reference: **`Plugin`** domain (see `plugin.md` / `02-extension-surfaces.md`) adds `registerObsidianProtocolHandler`, `registerEditorExtension`, `registerMarkdownPostProcessor`, etc. Surfaces *rooted in this domain* are: `addIcon`, `addChild`/`registerDomEvent`/`registerEvent`/`registerInterval` (inherited via `Plugin extends Component`), and the Modal/SuggestModal/AbstractInputSuggest subclassing surfaces.

---

## 11. Corbomite mapping

### `obsidian/ui/components` → Qt widgets

| Obsidian | Corbomite | Status | Notes |
|---|---|---|---|
| `Component` (load/unload, child tree, register* helpers) | `QObject` parent-child + new `Corbomite::Component` helper | **Missing** | Qt's parent-child covers *QObject-children* destruction only. Need: a cleanup-thunk vector, `onload`/`onunload` hook pair, adapters `registerDomEvent/registerEvent/registerInterval`. Belongs in `libs/core/`. Plugin-API spec depends on this. |
| `ButtonComponent` | `QPushButton` | Have | `setCta()` → QSS class; `setLoading(true)` → icon swap. |
| `ExtraButtonComponent` | `QToolButton::setAutoRaise(true)` | Have | |
| `ToggleComponent` | `QCheckBox` styled as toggle (QSS) | Have | `onChange` → `stateChanged(int)`. |
| `TextComponent` / `TextAreaComponent` | `QLineEdit` / `QPlainTextEdit` | Have | |
| `SearchComponent` | `KLineEdit` with `setClearButtonEnabled(true)` | Have | `addRightDecorator` → `QLineEdit::addAction(…, TrailingPosition)`. |
| `DropdownComponent` | `QComboBox` | Have | |
| `SliderComponent` | `QSlider` | Have | `setInstant(true)` → emit on `valueChanged` vs `sliderReleased`; `setDynamicTooltip()` → `QToolTip::showText` in `sliderMoved`. |
| `ColorComponent` | `KColorButton` | Have | `getValueRgb/Hsl/Int` are trivial `QColor` wrappers. |
| `ProgressBarComponent` | `QProgressBar` | Have | |
| `MomentFormatComponent` | new `Corbomite::MomentFormatLineEdit` + Moment-syntax adapter | **Missing** | Daily-Notes compatibility requires verbatim Moment tokens. `QDateTime::toString` tokens differ. Ship a ~300-LOC Moment-format function in `libs/core/`. |
| `SecretComponent` | new `Corbomite::SecretLineEdit` + KWallet/QtKeychain adapter | **Missing** | Cross-cuts `obsidian/secrets` domain. |
| Builder chaining (`.setValue(...).onChange(fn).setPlaceholder(...)`) | thin facade wrappers returning `this` | **Missing** | Needed for plugin-shim compat. Put in `libs/markoff/` as `Corbomite::Components::*`. |
| `Setting` row container | `QFormLayout` tri-split | Partial | `src/dialogs/SettingsDialog.cpp` shadows the shape. |

### `obsidian/ui/icons` → `QIcon::fromTheme` + theme map

| Obsidian | Corbomite | Status | Notes |
|---|---|---|---|
| Lucide registry (`Um`) | Freedesktop theme names + SVG fallbacks | **Partial** | ~70% direct theme matches; remainder need bundled SVGs. Translation table in `libs/core/IconMap.h`. |
| `setIcon(el, name)` / `getIcon(name)` / `addIcon` / `removeIcon` / `getIconIds` | `Corbomite::Icons::{setIcon,getIcon,registerCustom,removeIcon,ids}` in `libs/core/` | Missing | Back with `QHash<QString, QIcon>` + `QIcon::fromTheme(resolve(name), fallback)` resolution. |

**Icon translation table** (Freedesktop where available; `[M]` = ship bundled SVG):

| Lucide | Freedesktop |
|---|---|
| `plus{-circle}` / `minus-circle` | `list-add` / `list-remove` |
| `x{-circle}` | `dialog-close` |
| `settings` / `inspect` | `configure` / `document-properties` |
| `link` / `unlink` | `insert-link` / `remove-link` |
| `search` | `system-search` |
| `trash{-*}` | `user-trash` |
| `chevron-{left,right,up,down}` | `go-{previous,next,up,down}` |
| `chevrons-up-down` / `arrow-up-right` | `[M]` |
| `arrow-{left,right,up,down}` | `arrow-{left,right,up,down}` |
| `info` / `alert-triangle` / `-circle` | `dialog-information` / `-warning` / `-error` |
| `help-circle` | `help-about` |
| `check{,-square}` / `circle-check` | `dialog-ok` / `emblem-checked` |
| `eye{-off}` | `view-visible` / `view-hidden` |
| `edit{-*}` / `copy` / `clipboard` | `document-edit` / `edit-copy` / `edit-paste` |
| `file{,-down,-question}` | `text-x-generic` / `document-save` / `[M]` |
| `files` / `folder-documents` | `folder-documents` |
| `list{,-filter}` / `layout-list` / `table` | `view-list-details` / `-filter` / `-list-text` / `-table` |
| `calendar{-range}` | `view-calendar{,-month}` |
| `clock` / `tags` / `pin` / `image` | `view-history` / `tag` / `pin` / `image-x-generic` |
| `moon` / `sun` | `weather-clear-night` / `weather-clear` |
| `github` / `regex` / `baseline` / `binary` / `square-function` / `square-dashed` | `[M]` |
| `users` / `bug` / `book-open` | `system-users` / `tools-report-bug` / `books` |
| `more-vertical` / `menu` | `application-menu` / `open-menu` |
| `rotate-ccw` / `sort-asc` | `edit-undo` / `view-sort-ascending` |

125 unique Lucide IDs; ~70% covered. Remainder → bundled SVGs in `resources/icons/obsidian-lucide-shim/` (Adwaita/Breeze/Papirus drift on non-core names).

### `obsidian/ui/menu` → `QMenu` + section-sort helper

| Obsidian | Corbomite | Status | Notes |
|---|---|---|---|
| `Menu` / `MenuItem` / `MenuSeparator` | `QMenu` / `QAction` / `QAction::setSeparator(true)` | Have | `MenuItem.setWarning` → QSS property; `setIsLabel` → `setEnabled(false)`. Native-menu toggle unnecessary (Qt uses platform chrome by default). |
| `addSections(ids)` + `setSection(id)` + `sort` | new `Corbomite::MenuSectionRouter` in `libs/core/` | **Missing** | Qt has no section concept. Buffer `(section, QAction*)` pairs; on `apply(QMenu*)` sort by declared order, insert separators between non-empty sections, handle `submenuConfigs` by wrapping a section in a child `QMenu`. Every Workspace menu signal (`file-menu`/`editor-menu`/`url-menu`/`leaf-menu`/`files-menu`/`tab-group-menu`/`markdown-viewport-menu` — names from `workspace.md` §4) must route through it. |
| `showAtMouseEvent` / `showAtPosition({x,y,overlap,left?})` | `QMenu::exec(globalPos)` | Partial | `overlap`/`left` flags need manual `popup()` + `move()` after `sizeHint()`. |
| `Menu.forEvent(evt)` singleton | WeakMap on triggering widget | **Missing** | `QMouseEvent` is ephemeral; key on widget instead. |

### `obsidian/ui/popups` → `QDialog` / `KMessageBox` / `QCompleter` / `KMessageWidget`

| Obsidian | Corbomite | Status | Notes |
|---|---|---|---|
| `Modal` | `QDialog` (modal) | Have | Use a custom `QVBoxLayout` title row + content area (Obsidian's title is in-content, not window-chrome). Plugin-shim needs a `setTitle(QString)`-returns-`this` facade. |
| `Modal.shouldRestoreSelection` (incl. `QTextCursor` range) | `QDialog` focus-restore only | Partial | Extend `closeEvent` to snapshot/restore `QTextCursor::position()` + selection for prior focus widget. |
| Confirm dialogue (`nb`/`ib`) | `KMessageBox::questionYesNo` | Have | `addCheckbox` → `"dontShowAgainKey"` param. |
| `SuggestModal<T>` / `FuzzySuggestModal<T>` | `Corbomite::SuggestModal` (new abstract) + fuzzy variant | **Partial** | `src/dialogs/QuickSwitcher.cpp` partially shadows it. Cross-ref `search.md` for `fuzzySearch`/`prepareQuery`/`sortSearchResults`. |
| `PopoverSuggest` / `AbstractInputSuggest<T>` | custom `QFrame` + `QListView` anchored to a line-edit | **Partial** | `QCompleter` is too thin. `src/editor/CompletionPopup.cpp` shadows the in-editor case; input-anchored variant needs a new class. |
| `Notice` (floating toast) | new `Corbomite::Notice` in `libs/markoff/` | **Partial** | `KMessageWidget` is in-layout; Obsidian's is a floating corner overlay. Use a frameless `QWidget` stacked in a per-`QWidget::window()` container; hover-pauses-auto-hide + 1-s grace on leave. `addButton(text, cb)` maps to `KMessageWidget::addAction`. |
| `HoverPopover` | new `Corbomite::HoverPopover` | **Missing** | Frameless `QWidget` with `Qt::ToolTip\|Qt::WindowStaysOnTopHint` hosting `Markoff::ReadingView`. `QTimer` for the 300 ms delay; 500 ms poll via `QTimer` on a global registry tracking `$Q/XQ`-equivalents and child-popover chains. |
| `PopoverState` enum | `enum class Corbomite::PopoverState { Hidden=0, Showing, Shown, Hiding }` | Missing | Trivial. |
| Hover-link source registry | `Corbomite::HoverLinkSourceRegistry` — see `workspace.md` §7 | Missing | |

---

## 12. Markoff gap confirmations / discoveries

Most UI primitives are KDE-native replaceable and NOT Markoff gaps. The exception is `HoverPopover` (hover-link preview), which cross-cuts with editor-markdown.

- **Hover-link preview (`HoverPopover`) — confirmed + corrected.** The Pass 1 "500 ms hover" number was wrong: the per-popover initial delay is **300 ms** (`HoverPopover.js:73`); 500 ms is the `nX` poll interval on `elementFromPoint`. **Markoff needs three pieces wired:** (1) hover-link source registry (`workspace.md` §7), (2) `Markoff::Editor`/`ReadingView` emitting `hover-link` with payload `{event, source, hoverParent, targetEl, linktext, sourcePath?}`, (3) a `Corbomite::HoverPopover` with mod-key pinning + child-popover chains. All three missing today.
- **New signals:** `waitTime` is configurable per-construction (expose as user preference); `setIsFocused(true)` pin-to-keep-open is required for "click link inside preview" UX; `childHovers` chain needs a shared registry; `staticPos` anchor-to-mouse for `targetEl.offsetHeight > 300` must be ported; `watchResize`'s 10-call cap is trivial.
- **Adjacent non-editor gap:** `MomentFormatComponent` format-string parity — confirmed gap (see `01-markoff-gaps.md` Pass-2 additions).

All the above are appended to `01-markoff-gaps.md` under `## Pass 2 additions — ui-bundle`.

---

## 13. Open questions

1. **`PopoverState` exact numeric values.** Order inferred from transitions; `Hidden === 0` from `!state` truthy-checks. Confirm from `tree/_internal.js` in Pass 3.
2. **Is the unnamed `$k` numeric-input component publicly exported as `NumberComponent`?** No `// public API symbol` comment. Grep `plugin/Plugin.js` to confirm.
3. **`Menu.addItem`-after-load silent-drop**: does Obsidian's plugin-dev docs warn about this? Corbomite decision: mirror (silent drop) or throw (surface the bug).
4. **Icon-alias map `Ym` contents.** Not captured in `ui/icons/`; extract from `tree/_internal.js` to get the short-name→Lucide table.
5. **Does `Plugin.addIcon` auto-`removeIcon` on plugin unload?** Not visible in this domain — see `plugin.md` §10.
6. **`{{date:…}}` template resolver location.** Which domain expands the placeholder — Templates, Daily-Notes, Templater, FileManager? Confirms whether any Obsidian-specific token extensions beyond Moment exist.
7. **`Notice` stack order** — new-at-bottom or new-at-top? `appendChild` implies new-at-bottom; confirm visually.
8. **`HoverPopover.parent` expected class.** Constructor accepts any `Component`; which classes does `parent.hoverPopover` contract apply to?
9. **`gm.*` i18n namespace full table.** Needs capture for Corbomite localisation parity.

---

## 14. Recommended Pass 3 synthesis input

1. **`Component` = THE plugin-API lifecycle primitive.** Every subclassable Obsidian class (`Plugin`, `MarkdownView`, `ItemView`, `MarkdownRenderChild`, `Modal`, `Menu`, `HoverPopover`) extends it. Corbomite lacks the equivalent — promote to `GAP-ANALYSIS.md` as a first-order missing primitive.
2. **Menu section-ordering protocol** is the handshake between built-in views and plugins for `file-menu`/`editor-menu`/etc. Cross-ref `workspace.md` §4; canonicalise section IDs `title`/`close`/`pane`/`open`/`action-primary`/`action`/`find`/`selection`/`info`/`info.copy`/`view`/`view.linked`/`system`/`""`/`danger` + menu-specific (`tablist`, `order`, `ribbon`). Needs `Corbomite::MenuSectionRouter`.
3. **Moment.js format-string parity.** Daily-notes filename → `VAULT-FORMAT.md`; template/setting placeholders → `FEATURE-MATRIX.md`. `QDateTime::toString` tokens differ — ship a Moment adapter.
4. **`HoverPopover` = three wires.** Cross-refs `workspace.md` §7 + `editor-markdown.md` §12 + this doc §1. Promote to GAP-ANALYSIS as one coordinated feature ("hover-link preview").
5. **125 unique Lucide IDs.** Translation table (§11) is ~70% Freedesktop matches; remainder needs bundled SVGs under `resources/icons/obsidian-lucide-shim/`. Canonical list in `getIcon.js`'s resolution chain (`Um`→`Xm`→`Qm`→`Ym`-aliased).

---

## 15. Cross-domain references

| Other domain | Type | Description |
|---|---|---|
| `core` | dependency | `Modal` takes `App`; `Menu`/`PopoverSuggest`/`AbstractInputSuggest`/`HoverPopover` use `Scope`/`Keymap`/`activeWindow`/`activeDocument`. `Component.registerEvent` accepts `EventRef`s. |
| `workspace` | dependency + consumer | Emits `file-menu`/`url-menu`/`editor-menu`/`leaf-menu`/`files-menu`/`tab-group-menu`/`markdown-viewport-menu` with `Menu` mid-construction (see `workspace.md` §4). `workspace.hoverLinkSources` consumed by Page-Preview → spawns `HoverPopover` from `workspace.trigger("hover-link", payload)`. |
| `editor-markdown` | consumer | `MarkdownView`/`MarkdownPreviewView` build menus via `Menu.forEvent`; emit `hover-link`; `MarkdownRenderChild extends Component`; save-failure surfaces `Notice`. |
| `views` | consumer | `ItemView.onMoreOptionsMenu(menu)` uses canonical section order; `View`/`FileView`/`TextFileView`/`MarkdownView`/`BasesView` all `extends Component`. |
| `plugin` | consumer | `Plugin extends Component`. `Plugin.addIcon` wraps `icons/addIcon.js`. `Plugin.registerHoverLinkSource` wraps `workspace.registerHoverLinkSource`. |
| `bases` | consumer | `BasesView extends` a `Component`-derived `View`. Cells build per-value menus with `Menu.forEvent(t).addSections([...])`. |
| `settings` | consumer | `Setting` uses every `*Component`. `PluginSettingTab extends Component`. `SettingsDialog` uses `Modal`. |
| `search` | dependency | `FuzzySuggestModal` / `AbstractInputSuggest` call `fuzzySearch`/`prepareQuery`/`sortSearchResults`. |
| `rendering` | dependency | `renderResults`, `renderMatches`, `displayTooltip`, `setTooltip`, `sanitizeHTMLToDom`. |
| `secrets` | dependency | `SecretComponent` reaches into `app.secretStorage`. |
| `platform` | dependency | `Platform.*` branching; `navigator.vibrate` in `ToggleComponent`/`SliderComponent`. |
| `vault` / `metadata` | consumer | save-failure / external-modify / vault-scan toast `Notice`s. |
| `parsing` | sibling | `MomentFormatComponent.updateSample` uses `window.moment` from `utils/moment.js`. |

**Short symbols from other domains referenced by name in this doc:**

| Short symbol | Defined in | Used here for |
|---|---|---|
| `Scope`, `Keymap`, `Events`, `activeWindow`/`activeDocument`, `createDiv`/`createEl` and DOM helpers | `core` | menu/modal/popover keyboard scope, EventRef lifetime, window accessors, DOM-building helpers used in every file |
| `Platform.is{Phone,Desktop,MacOS,DesktopApp,MobileApp,IosApp,AndroidApp,hasPhysicalKeyboard}` | `platform` | branching throughout |
| `gm.*` | `core` (i18n table) | user-facing strings like `gm.dialogue.buttonCancel()` |
| `displayTooltip` / `setTooltip` / `renderResults` / `renderMatches` / `sanitizeHTMLToDom` | `rendering` | tooltip, highlight-span, sanitiser primitives |
| `prepareQuery` / `fuzzySearch` / `sortSearchResults` | `search` | `FuzzySuggestModal.getSuggestions`, every `AbstractInputSuggest` subclass |
| `window.moment` | `utils/moment` | `MomentFormatComponent.updateSample` |
| `debounce`, `Nv` (placement), `Rv` (auto-destroy), `yl`+`dl` (animation), `Cv` (tooltip-hide), `Pl`/`Ll` (modal anim), `_g` (swipe), `Nk`/`Ik`/`Fk`/`Lk` (colour conversion), `Lc`/`Uc`/`Ov`/`Lv`/`Vl` (hit-test/fallback-parse) | `utils` | misc helpers used across all four sub-directories |
| `Um`/`Xm`/`Qm`/`Ym`/`Zm`/`Jm`/`Km`/`_m`/`$m`/`Wm` | same domain (`ui/icons/` internals, with some declarations in broader bundle) | icon registry backbone — see §7. `Um` (Lucide paths) and `Qm` (legacy built-ins) declarations must be captured from `tree/_internal.js` in Pass 3. |
| `ob`/`ab`/`nb`/`ib`/`qg`/`Wg` | same domain (popups/menu internals) | list suggester scaffolding, confirm/prompt subclasses, current-menu singleton, `forEvent` WeakMap |
| `XQ`/`$Q`/`ZQ`/`JQ`/`nX`/`iX`/`eX`/`tX` | same domain (`ui/popups/HoverPopover.js`) | hover-popover global state: pending/visible lists, poll interval, mouse pos, poll fn, install-listeners, click-outside, mousemove |
| `X1`/`Y1`/`$1` | `secrets` | keychain modals referenced by `SecretComponent` |
| `PopoverState` | declaration out-of-scope | enum captured from use sites — see §2 |
