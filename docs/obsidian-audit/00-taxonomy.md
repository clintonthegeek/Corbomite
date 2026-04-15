# Obsidian audit — Pass 1 taxonomy

Routing table over `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/`. One section per in-scope domain. Pass 2 agents pick from this index; do not re-derive scope.

## Architectural shape (one paragraph)

Obsidian is a layered Electron app. At the bottom sits **`core/App`** wiring the three universal services: `vault` (filesystem + abstract `TFile`/`TFolder` tree), `metadataCache` (worker-driven incremental parser producing link/tag/heading caches and resolved-link maps), and `workspace` (split/tab/sidedock layout that mounts `View` instances inside `WorkspaceLeaf` containers). Everything else plugs into those: a `ViewRegistry` maps file extensions to `View` subclasses (`MarkdownView`, `BasesView`, image/PDF/canvas viewers, etc.); the `editor` layer wraps CodeMirror as `Editor`; the `rendering` layer turns Markdown to HTML and lazy-loads MathJax/Mermaid/PDF.js/Prism; `bases` is a separate, opinionated database-view feature with its own typed value system. UI primitives (`Menu`, `Modal`, `SuggestModal`, `Notice`, `Setting`, `Component`, `HoverPopover`) are HTML/CSS-built and shared across both first-party views and plugins. Every long-lived object inherits a tiny `Component` lifecycle (load/unload + child registration), and a tinier `Events` mixin (`on`/`off`/`offref`/`trigger`) is the universal pub-sub. **Plugins** subclass `Plugin` (a `Component`) and register: views, editor extensions, post-processors, code-block processors, commands, ribbon icons, settings tabs, hover-link sources, and Obsidian protocol handlers.

## Reading order for Pass 2

1. **`vault`** — defines `TFile`/`TFolder`/`Vault` and the on-disk contract; everything reads through here.
2. **`parsing`** — frontmatter + YAML; trivial but vault-format-critical.
3. **`metadata`** — `MetadataCache` consumes vault + parsing; emits the `resolved/changed/finished/deleted` events the rest of the app waits on.
4. **`core`** — `App`, `Events`, `Scope`. Read after `vault`/`metadata` so the wiring lines up.
5. **`workspace`** — depends on `core`; defines the layout into which views are mounted.
6. **`views`** — `View`/`FileView`/`TextFileView`/`ItemView`/`ViewRegistry`. Read before `editor`/`bases` since they both subclass `TextFileView`.
7. **`editor` + `editor/markdown`** — `Editor`, `MarkdownView`, `MarkdownPreview*`. Largest by LOC; the live-preview/source-mode/preview tri-state lives here.
8. **`rendering`** — Markdown-to-HTML, plus Math/Mermaid/PDF/Prism loaders. Pairs with `editor/markdown`.
9. **`search`** — fuzzy matcher and query parser; small, self-contained.
10. **`bases`** — large, depends on `metadata`, `parsing`, `views`, `rendering`. Read last among data domains.
11. **`ui/components`, `ui/popups`, `ui/menu`, `ui/icons`, `settings`** — UI primitives, fan-out of dependencies. Quick.
12. **`plugin`** — extension-surface census only. Skim.
13. **`network`, `platform`, `secrets`, `utils`** — leaf utilities.

---

## `obsidian/bases` — typed-value database view ("Bases" feature)

**Files:** 25 (`BasesEntry.js`, `BasesEntryGroup.js`, `BasesQueryResult.js`, `BasesView.js`, `BasesViewConfig.js`, `BooleanValue.js`, `DateValue.js`, `DurationValue.js`, `FileValue.js`, `HTMLValue.js`, `IconValue.js`, `ImageValue.js`, `LinkValue.js`, `ListValue.js`, `NotNullValue.js`, `NullValue.js`, `NumberValue.js`, `ObjectValue.js`, `PrimitiveValue.js`, `RegExpValue.js`, `RelativeDateValue.js`, `StringValue.js`, `TagValue.js`, `UrlValue.js`, `Value.js`).

**What it does:** Bases is Obsidian's database/spreadsheet view over the vault's frontmatter. A user creates a `.base` file describing filters, formulas, groupings and a view config (table/cards/list/etc.); Bases rolls every matching note's frontmatter into a `BasesEntry`, evaluates expressions/formulas through a typed `Value` hierarchy, and renders interactive table-like views. This is the post-1.9 release feature parity-blockers complain about most; **Corbomite has no equivalent**.

**Key exports / primitives:**
- `BasesView` — `TextFileView` subclass that owns `.base` rendering and edit UI.
- `BasesViewConfig` — declarative description of columns, filters, sort, group-by.
- `BasesEntry` — one note's resolved frontmatter as a `Value`-typed record, `local`/`note` projections.
- `BasesEntryGroup` — groupings produced by group-by expressions.
- `BasesQueryResult` — materialised result of running a query against the vault.
- `Value` — abstract base for the type system; `renderTo(el, ctx)` and rich-comparison API.
- `StringValue`/`NumberValue`/`DateValue`/`DurationValue`/`BooleanValue`/`FileValue`/`LinkValue`/`TagValue`/`ListValue`/`ObjectValue`/`UrlValue`/`HTMLValue`/`IconValue`/`ImageValue`/`RegExpValue`/`RelativeDateValue`/`NullValue`/`NotNullValue`/`PrimitiveValue` — all extend `Value`, drive coercion, formatting, comparison, sorting, and "render this cell" for the table.

**On-disk contracts:** reads + writes `.base` files in the vault (YAML-ish JSON describing the view); reads frontmatter from every note matched by its filter through `MetadataCache`.

**Cross-domain dependencies:** depends on `metadata`, `parsing`, `vault`, `views`, `rendering`, `ui/menu`, `editor` (suggesters); registered via `ViewRegistry` for `.base` extension.

**Corbomite-relevance note:** **MISSING** — no Corbomite equivalent exists. Largest single feature gap. Corbomite would need a new `libs/bases/` library plus a `BasesView` widget; the typed `Value` hierarchy maps cleanly onto C++ `std::variant` + virtual dispatch.

**Pass 2 focus:** (a) the `.base` file schema (column/filter/group/sort grammar); (b) the `Value` type hierarchy and its formula/comparison semantics; (c) how `BasesQueryResult` integrates with `MetadataCache` events for incremental refresh.

---

## `obsidian/core` — top-level App, scope, event bus

**Files:** 3 (`App.js`, `Events.js`, `Scope.js`).

**What it does:** Boot wiring. `App` is the "god object" — owns `vault`, `metadataCache`, `workspace`, `commands`, `hotkeyManager`, `dragManager`, `viewRegistry`, `embedRegistry`, `customCss`, `renderContext`, `secretStorage`, `plugins`, `internalPlugins`, `setting`. `Events` is the universal observer mixin (`on`, `off`, `offref`, `trigger`, `tryTrigger`). `Scope` is a hierarchical key-handler stack rooted in `Keymap.global`.

**Key exports / primitives:**
- `App` — central registry / DI container; `commands`, `keymap`, `plugins`, `internalPlugins`, `setting`, `dragManager`, `embedRegistry`, `viewRegistry`, `renderContext`, `customCss` all live here.
- `Events` — `on/off/offref/trigger/tryTrigger`. Subclass for any pub-sub object (Vault, MetadataCache, Workspace, ViewRegistry...).
- `Scope` — push/pop keybinding context; `register(modifiers, key, fn)`, `handleKey`, parent-chain lookup. `Modal` and `Menu` each own a Scope for in-popup hotkeys.

**On-disk contracts:** none directly; delegates to `vault.adapter` for `.obsidian/` config files (App initialises `appId` and `loadLocalStorage('DebugMode')`).

**Cross-domain dependencies:** the root — every other domain consumes one of these three.

**Corbomite-relevance note:** Partial. `App` ↔ `MainWindow` + Qt application; `Events` ↔ Qt signals/slots (free); `Scope` ↔ `KActionCollection` + `QShortcut` contexts (close enough). No central `App` registry exists in Corbomite — services are constructed and wired in `MainWindow`/`VaultService`. Pass 2 should not recommend cloning `App`; flag whether per-vault state currently leaks across vault switches.

**Pass 2 focus:** event names emitted on `App` itself (e.g. `css-change`, `quit`); the `appId` lifecycle and how `loadLocalStorage` is keyed on it.

---

## `obsidian/editor` — editor wrapper, suggesters, CodeMirror state-fields

**Files:** 6 (`Editor.js`, `EditorSuggest.js`, `editorEditorField.js`, `editorInfoField.js`, `editorLivePreviewField.js`, `editorViewField.js`).

**What it does:** Wraps the underlying CodeMirror 6 `EditorView` in an Obsidian-flavoured `Editor` API (cursor/selection getters, `replaceRange`, `posAtMouse`, list-aware `getLine` helpers, IME handling). The four `editor*Field` exports are CodeMirror `StateField`s plugins use to read the live `Editor` / `EditorInfo` / `EditorView` / live-preview-state from inside CodeMirror extensions. `EditorSuggest` is the plugin-extensible inline autocomplete (slash-menu, `[[wikilink]]`, `@mention` patterns).

**Key exports / primitives:**
- `Editor` — public abstraction over CM6 (`getDoc`, `setLine`, `getCursor`, `replaceRange`, `posAtCoords`, `posAtMouse`, IME-aware `insertText`).
- `EditorSuggest` — base class for inline pop-up suggesters; `onTrigger(cursor, editor, file)`, `getSuggestions(ctx)`, `renderSuggestion`, `selectSuggestion`.
- `editorEditorField`, `editorInfoField`, `editorViewField`, `editorLivePreviewField` — CM6 `StateField` accessors plugins import to bridge CM extensions back to Obsidian.

**On-disk contracts:** none.

**Cross-domain dependencies:** depends on CodeMirror (vendored), `vault`, `core`; consumed by `editor/markdown`, `bases`, `plugin`.

**Corbomite-relevance note:** Partial / divergent. Corbomite replaces CodeMirror with **Markoff** (`libs/markoff/Editor`). The `Editor` API surface here informs what Markoff must expose to satisfy a future Corbomite plugin shim. `EditorSuggest` ↔ `src/editor/CompletionPopup` (similar role).

**Pass 2 focus:** the exact `Editor` method surface (the contract any plugin assumes); `EditorSuggest` trigger lifecycle; what `editorLivePreviewField` exposes (live-preview state machine).

---

## `obsidian/editor/markdown` — Markdown view, preview, render pipeline

**Files:** 6 (`MarkdownView.js`, `MarkdownPreviewView.js`, `MarkdownPreviewRenderer.js`, `MarkdownPreviewSection.js`, `MarkdownRenderChild.js`, `MarkdownRenderer.js`).

**What it does:** The marquee feature. `MarkdownView` is the `TextFileView` subclass for `.md` files; it owns the editor, the preview pane, the toggle between "source / live-preview / preview" modes, the rendered markdown sections (`MarkdownPreviewSection`), and section recycling. `MarkdownPreviewRenderer` does progressive async section-by-section rendering with a worker. `MarkdownRenderer` is the public static API plugins call to render markdown into a DOM element. `MarkdownRenderChild` is the lifecycle wrapper for plugin-rendered fragments (so they unload when the section is recycled).

**Key exports / primitives:**
- `MarkdownView` — view registered for `.md`; manages mode switching, search-in-document, scroll sync, breadcrumbs, sort buttons.
- `MarkdownPreviewView` — preview-only view (used inside hover popovers, embeds).
- `MarkdownPreviewRenderer` — section-recycling, progressive-render engine; tracks `sections`, `asyncSections`, `recycledSections`.
- `MarkdownPreviewSection` — one HTML chunk + heading-collapse state + frontmatter flag + height cache.
- `MarkdownRenderer` — static `render(app, markdown, el, sourcePath, component)`; what plugins call.
- `MarkdownRenderChild` — `Component` subclass for plugin DOM children (unload-on-recycle).

**On-disk contracts:** reads/writes the `.md` file via `TextFileView.save`; reads `.obsidian/appearance.json`-driven CSS classes.

**Cross-domain dependencies:** depends on `editor`, `views`, `rendering`, `metadata`, `vault`, `parsing`, `ui/popups` (HoverPopover for embeds), `ui/menu`; registered in `ViewRegistry` for `.md`.

**Corbomite-relevance note:** Partial. Corbomite has `NoteEditorWidget` (edit) + `Markoff::ReadingView` + canvas-card `MarkdownRenderEngine`. The progressive-section rendering, recycle pool, and live-preview mode-toggle behavior are NOT yet matched — feed several into `01-markoff-gaps.md`.

**Pass 2 focus:** the section-recycling algorithm; live-preview vs reading vs source mode state machine; checkbox-click-to-toggle markdown round-trip; embed/hover-popover integration.

---

## `obsidian/metadata` — MetadataCache

**Files:** 4 (`MetadataCache.js`, `getAllTags.js`, `iterateCacheRefs.js`, `iterateRefs.js`).

**What it does:** Background indexer. Spawns `worker.js`, parses every markdown file into a `CachedMetadata` record (links, embeds, tags, headings, sections, frontmatter, footnotes, blocks). Maintains `resolvedLinks` and `unresolvedLinks` reverse maps. Persists to IndexedDB. Emits `changed`, `deleted`, `resolve`, `resolved`, `finished` events that the rest of the app subscribes to.

**Key exports / primitives:**
- `MetadataCache` — `getCache(path)`, `getFileCache(file)`, `getFirstLinkpathDest(linkpath, sourcePath)`, `getBacklinksForFile`, `getLinkSuggestions`, `resolvedLinks`, `unresolvedLinks`. Events: `changed`, `deleted`, `resolve`, `resolved`, `finished`.
- `getAllTags(cache)` — extracts all tags (frontmatter + inline `#tag`) from a `CachedMetadata`.
- `iterateRefs(refs, cb)`, `iterateCacheRefs(cache, cb)` — helpers to walk reference arrays/objects.

**On-disk contracts:** persistent cache in IndexedDB (`obsidian-cache-<vault-id>` or similar); reads every file's content via `vault.cachedRead`.

**Cross-domain dependencies:** depends on `vault`, `parsing`, web worker `worker.js`; consumed by `workspace`, `editor/markdown`, `bases`, `views`, `search`, virtually every UI feature that needs link/tag awareness.

**Corbomite-relevance note:** Partial. Corbomite has `libs/storage/SQLiteIndex` doing similar work via SQLite + FTS5. Persistence backend differs (IndexedDB vs SQLite) but the conceptual API (cache-by-path, link resolution, tag aggregation) is parallel.

**Pass 2 focus:** the exact shape of `CachedMetadata` (links/embeds/tags/headings/sections/frontmatter/blocks/listItems); the `linkResolver` algorithm and what triggers re-resolution; the worker-message protocol.

---

## `obsidian/network` — HTTP request API

**Files:** 2 (`request.js`, `requestUrl.js`).

**What it does:** Plugin-facing wrappers around node's `http`/`https` (via internal helper) so plugins don't need to hit Electron or Node APIs directly. `request(opts)` returns the body as text; `requestUrl(opts)` returns `{status, headers, text, json, arrayBuffer}`.

**Key exports / primitives:**
- `request(opts)` — text response, awaits internal helper.
- `requestUrl(opts)` — full structured response, supports binary.

**On-disk contracts:** none.

**Cross-domain dependencies:** consumed by `plugin`-installed code only.

**Corbomite-relevance note:** Lightly relevant. If Corbomite ever offers a plugin API, plugins should get a `QNetworkAccessManager`-backed equivalent. No Corbomite equivalent today; not needed for vault compat.

**Pass 2 focus:** option-bag schema (headers, body, contentType, throw, method); enough to spec the future Corbomite analogue.

---

## `obsidian/parsing` — frontmatter + YAML + linktext

**Files:** 8 (`getFrontMatterInfo.js`, `parseFrontMatterAliases.js`, `parseFrontMatterEntry.js`, `parseFrontMatterStringArray.js`, `parseFrontMatterTags.js`, `parsePropertyId.js`, `parseYaml.js`, `stringifyYaml.js`).

**What it does:** Vault-format-critical text parsers. `getFrontMatterInfo(text)` returns the `---`-delimited frontmatter range. The `parseFrontMatter*` family extracts typed values (aliases, tags, generic entries, string-arrays). `parseYaml`/`stringifyYaml` wraps the underlying YAML library. `parsePropertyId` normalises a frontmatter key to its display id.

**Key exports / primitives:**
- `getFrontMatterInfo(text)` — `{exists, frontmatter, from, to, contentStart}`. The on-disk frontmatter offset contract.
- `parseFrontMatterAliases(fm)` — both string and array `aliases:` forms.
- `parseFrontMatterTags(fm)` — `tags:`/`tag:`/inline-mixed forms; prepends `#`.
- `parseFrontMatterStringArray(fm, regex)` — generic array-or-string getter.
- `parseFrontMatterEntry(fm, key)` — typed single-key getter.
- `parsePropertyId(key)` — display-id normalisation.
- `parseYaml(text)` / `stringifyYaml(obj)` — YAML wrapper; `stringifyYaml` is what Obsidian writes back.

**On-disk contracts:** defines the wire format for any code editing frontmatter — Pass 2 must capture quoting, list, and date round-trip behaviors faithfully.

**Cross-domain dependencies:** consumed by `metadata`, `bases`, `vault` (`FileManager` writes frontmatter), `editor/markdown` (Properties UI).

**Corbomite-relevance note:** Partial. Corbomite has frontmatter handling in `libs/markoff-parser/`; degree of Obsidian-fidelity (especially `parseYaml` quirks, `aliases` handling) is unknown.

**Pass 2 focus:** corner cases in `aliases`/`tags` parsing; `stringifyYaml`'s formatting quirks (these dictate diff-stability when Corbomite writes a frontmatter back).

---

## `obsidian/platform` — platform detection + keymap

**Files:** 2 (`Platform.js`, `Keymap.js`).

**What it does:** `Platform` is a flag object (`isDesktop`, `isMobile`, `isMacOS`, `isWin`, `isLinux`, `canExportPdf`, `canPopoutWindow`, `canStackTabs`, `supportsIndexedDb`, `version`, ...). `Keymap` owns the global root scope, modifier-key tracking (`isModifier`, `isModEvent`), and link-open hot-modifier behavior (`pushScope`/`popScope`, `getRootScope`).

**Key exports / primitives:**
- `Platform` — booleans + getters; `version`, `resourcePathPrefix`, `mobileSoftKeyboardVisible`.
- `Keymap` — `init()`, `global`, `compileModifiers`, `isMatch`, `isModifier`, `isModEvent`, `pushScope`, `popScope`, `getRootScope`. Hosts the modifier-key drag/click semantics (Ctrl-click to open in new tab, etc.).

**On-disk contracts:** none.

**Cross-domain dependencies:** consumed by basically everything that branches on OS or modifier keys.

**Corbomite-relevance note:** Partial / replaceable. Qt has `QSysInfo`, `QGuiApplication::keyboardModifiers()`, `KStandardShortcut`. Corbomite uses these directly. Plugin-API equivalents would re-export a `Platform`-shaped struct for compat.

**Pass 2 focus:** which getters are read at runtime by other domains (PDF export, stack-tabs, popout-window) — these gate UI features.

---

## `obsidian/plugin` — plugin base class (extension surfaces only)

**Files:** 1 (`Plugin.js`).

**What it does:** `Plugin` extends `Component` and is the base every community plugin subclasses. Beyond `onload`/`onunload`, it owns the entire plugin-facing extension surface: command registration, ribbon icons, status-bar items, settings tabs, view registration, file-extension association, editor extensions, markdown post-processors (priority-sorted), code-block processors, hover-link sources, editor suggests, Obsidian-protocol handlers, plus `loadData`/`saveData` for `data.json`. **Per scope brief: do NOT clone the plugin loader. Catalogue what plugins extend.**

**Key exports / primitives:** see `02-extension-surfaces.md` — `Plugin.js` is the canonical list.

**On-disk contracts:** reads/writes `.obsidian/plugins/<plugin-id>/data.json` (via `loadData`/`saveData`) and the manifest at `.obsidian/plugins/<plugin-id>/manifest.json` (read by the loader, not `Plugin`).

**Cross-domain dependencies:** delegates to `viewRegistry`, `commands`, `workspace`, `setting`, `app.workspace.registerHoverLinkSource`, etc. Effectively a thin facade over the `App` registries.

**Corbomite-relevance note:** **MISSING**. Lightly in scope. When a Corbomite plugin system is designed, mirror these registration verbs (with KDE-native primitives): `addCommand` ↔ `KActionCollection::addAction`, `addRibbonIcon` ↔ toolbar action, `addStatusBarItem` ↔ `QStatusBar::addPermanentWidget`.

**Pass 2 focus:** complete enumeration of `register*` and `add*` methods; see seeded list in `02-extension-surfaces.md` and confirm completeness.

---

## `obsidian/rendering` — markdown→HTML, lazy renderers, sanitiser

**Files:** 11 (`displayTooltip.js`, `finishRenderMath.js`, `htmlToMarkdown.js`, `loadMathJax.js`, `loadMermaid.js`, `loadPdfJs.js`, `loadPrism.js`, `RenderContext.js`, `renderMath.js`, `renderResults.js`, `sanitizeHTMLToDom.js`).

**What it does:** Rendering primitives shared across preview, embeds, hover popovers, search-result snippets, callouts, and Bases cells. `RenderContext` is the central object that knows how to render an internal link, a tag, a date, an image, a file embed; passed into every `Value.renderTo`. The `load*` family lazy-loads heavyweight engines (MathJax, Mermaid, PDF.js, Prism syntax-highlighting). `htmlToMarkdown` (Turndown) reverses paste-from-browser. `sanitizeHTMLToDom` is the DOMPurify wrapper for any plugin-supplied HTML.

**Key exports / primitives:**
- `RenderContext` — `renderFileLink`, `renderTag`, `renderDate`, `renderProperty`, `renderImage`, etc. The ambient render API.
- `loadMathJax()`, `loadMermaid()`, `loadPdfJs()`, `loadPrism()` — promise-returning lazy loaders.
- `renderMath(source, display)` / `finishRenderMath()` — MathJax invocation + post-batch typeset.
- `htmlToMarkdown(html)` — Turndown wrapper for paste/import.
- `sanitizeHTMLToDom(html)` — DOMPurify wrapper, returns sanitised `DocumentFragment`.
- `renderResults(el, text, matches)` — render a search hit with highlighted spans.
- `displayTooltip(el, text, opts)` — Obsidian's tooltip primitive (the underlying `setTooltip` writes attributes; `displayTooltip` actually shows it).

**On-disk contracts:** none directly; reads CSS variables.

**Cross-domain dependencies:** depends on `vault`, `metadata`, `parsing`, `ui/popups`; consumed by every renderer (`editor/markdown`, `bases`, `views`, `search`, hover popover).

**Corbomite-relevance note:** Partial. Corbomite has `Markoff::ReadingView` + `MarkdownRenderEngine` for canvas cards, plus `libs/mmdr` (Rust Mermaid bridge). MathJax/PDF.js parity NOT confirmed. `RenderContext`-equivalent ambient context is implicit in Markoff today.

**Pass 2 focus:** what fields/methods `RenderContext` exposes (this is the contract for any custom renderer); how Math/Mermaid loaders integrate with section recycling; the search-result highlight DOM structure.

---

## `obsidian/search` — fuzzy matcher + query parser

**Files:** 6 (`fuzzySearch.js`, `prepareFuzzySearch.js`, `prepareQuery.js`, `prepareSimpleSearch.js`, `QueryController.js`, `sortSearchResults.js`).

**What it does:** Two layers. The lower (`fuzzySearch`/`prepareQuery`/`prepareFuzzySearch`/`prepareSimpleSearch`/`sortSearchResults`) is a generic fuzzy-match engine plugins reuse for any list-of-strings (commands, files, tags). The upper `QueryController` is the global-search engine: parses Obsidian's search-DSL (`tag:`, `path:`, `file:`, `line:`, `block:`, `section:`, regex, quoted-phrase, AND/OR/NOT, `match-case`/`whole-word` flags), runs it against vault content + `MetadataCache`, and streams `BasesQueryResult`-shaped results to the search panel.

**Key exports / primitives:**
- `prepareQuery(text)` — tokenise into `{query, tokens, fuzzy}` for `fuzzySearch`.
- `prepareFuzzySearch(text)` — returns a function `(text) => {score, matches} | null`.
- `prepareSimpleSearch(text)` — non-fuzzy variant for autocomplete.
- `fuzzySearch(prepared, text)` — score one string.
- `sortSearchResults(results)` — descending by score.
- `QueryController` — owns DSL parsing, file iteration, result aggregation, view notification (`requestNotifyView` debounced); event source for the search panel.

**On-disk contracts:** none directly; reads vault content + frontmatter.

**Cross-domain dependencies:** depends on `vault`, `metadata`, `parsing`, `bases` (`gm.plugins.bases`); consumed by the search view, command palette, quick switcher, every `SuggestModal`.

**Corbomite-relevance note:** Partial. Corbomite has `src/sidebar/SearchPanel` + SQLite FTS5. Obsidian's full DSL is broader than what Corbomite's FTS layer handles today; pass 2 should enumerate operators.

**Pass 2 focus:** the search-DSL grammar (operators, escaping, regex syntax, line/block/section qualifiers); the scoring algorithm in `sortSearchResults`; how `QueryController` events drive result UI.

---

## `obsidian/secrets` — keychain-backed secret storage

**Files:** 1 (`SecretStorage.js`).

**What it does:** Tiny Electron `safeStorage` wrapper exposed on `App.secretStorage`; plugins (and core sync/Publish) store API tokens here so they don't end up in `data.json`.

**Key exports / primitives:**
- `SecretStorage` — `get(key)`, `set(key, value)`, `remove(key)`, `isAvailable()`. Backed by OS keychain via Electron.

**On-disk contracts:** stored outside the vault, in OS keychain (Keychain on macOS, libsecret on Linux, Credential Manager on Windows).

**Cross-domain dependencies:** consumed by sync/publish/community plugins.

**Corbomite-relevance note:** Partial / replaceable. Map directly onto `KWallet` (and `QtKeychain` as fallback). Not currently implemented in Corbomite; only relevant once a plugin API exists.

**Pass 2 focus:** the API shape only — adopt verbatim for the future Corbomite wallet wrapper.

---

## `obsidian/settings` — Setting, SettingTab, PluginSettingTab

**Files:** 4 (`Setting.js`, `SettingGroup.js`, `SettingTab.js`, `PluginSettingTab.js`).

**What it does:** The fluent builder Obsidian uses for every settings row across the entire app. `Setting` is one row in a settings panel: `.setName(...).setDesc(...).addToggle(...).addText(...).addButton(...)`. `SettingTab` is a tab in the settings modal. `PluginSettingTab` is the subclass plugins extend. `SettingGroup` is a collapsible/expandable group of settings.

**Key exports / primitives:**
- `Setting` — fluent builder; methods `setName`, `setDesc`, `setHeading`, `setClass`, `setTooltip`, `setNoInfo`, `setDisabled`, `addToggle`, `addText`, `addTextArea`, `addDropdown`, `addSlider`, `addColorPicker`, `addMomentFormat`, `addProgressBar`, `addSearch`, `addExtraButton`, `addButton`, `then(cb)`.
- `SettingTab` — base class; tab navigation, `display()`/`hide()` lifecycle.
- `PluginSettingTab` — plugin-facing subclass with `app`, `plugin`.
- `SettingGroup` — collapsible group inside a `SettingTab`.

**On-disk contracts:** via `Plugin.saveData` for plugin settings; via `app.vault.setConfig(key, value)` for built-in settings (writes `.obsidian/app.json`, `.obsidian/appearance.json`, `.obsidian/hotkeys.json`, `.obsidian/core-plugins.json`, `.obsidian/community-plugins.json`).

**Cross-domain dependencies:** depends on `ui/components`, `ui/icons`, `core` (App), `vault`; consumed by `plugin`, every settings UI.

**Corbomite-relevance note:** Replaceable — Corbomite uses `KConfig` + `SettingsDialog.cpp`. Don't clone the fluent builder; adopt the on-disk file schemas (`.obsidian/*.json`) for compat.

**Pass 2 focus:** the **on-disk** schemas of `.obsidian/app.json`, `.obsidian/appearance.json`, `.obsidian/hotkeys.json` (these are the compat targets); the `Setting` builder's method list (informs Corbomite settings DSL, if we want one).

---

## `obsidian/ui/components` — form & input components

**Files:** 16 (`AbstractTextComponent.js`, `BaseComponent.js`, `ButtonComponent.js`, `ColorComponent.js`, `Component.js`, `DropdownComponent.js`, `ExtraButtonComponent.js`, `MomentFormatComponent.js`, `ProgressBarComponent.js`, `SearchComponent.js`, `SecretComponent.js`, `SliderComponent.js`, `TextAreaComponent.js`, `TextComponent.js`, `ToggleComponent.js`, `ValueComponent.js`).

**What it does:** Form-input widgets used by `Setting`, plugin UIs, modal contents. `Component` is the universal lifecycle base (load/unload + child registration + event-cleanup). `BaseComponent`/`AbstractTextComponent`/`ValueComponent` are abstract bases the concrete inputs extend.

**Key exports / primitives:**
- `Component` — `load`, `unload`, `addChild`, `removeChild`, `register(cb)`, `registerEvent(eventRef)`, `registerDomEvent(el, type, cb)`, `registerInterval(timer)`. Base of `Plugin`, `View`, `Modal`, `MarkdownRenderChild`, almost everything.
- `ButtonComponent`/`ExtraButtonComponent` — buttons (regular and icon-only).
- `TextComponent`/`TextAreaComponent`/`SearchComponent`/`SecretComponent` — text inputs of varying flavours.
- `ToggleComponent`/`DropdownComponent`/`SliderComponent`/`ColorComponent`/`MomentFormatComponent`/`ProgressBarComponent` — typed inputs.
- `ValueComponent` — abstract base for components with a `getValue/setValue/onChange` contract.

**On-disk contracts:** none (consumed by widgets that may persist).

**Cross-domain dependencies:** depends on `ui/icons`, `core`; consumed everywhere UI is built.

**Corbomite-relevance note:** Replaceable — Corbomite uses Qt widgets (`QPushButton`, `QLineEdit`, `QSlider`, `QComboBox`, `QCheckBox`, etc.) directly. `Component` lifecycle ↔ `QObject` parent ownership + `QObject::destroyed`.

**Pass 2 focus:** the `Component` lifecycle exact contract (`load`/`unload`/`onload`/`onunload`/`addChild`/`registerEvent`); only relevant for plugin-API spec.

---

## `obsidian/ui/icons` — icon registry

**Files:** 5 (`addIcon.js`, `getIcon.js`, `getIconIds.js`, `removeIcon.js`, `setIcon.js`).

**What it does:** Tiny registry for Lucide-style SVG icons. Plugins call `addIcon(id, svgString)` to inject; `setIcon(el, id)` writes the SVG into a DOM element; `getIcon(id)` returns the SVG node; `getIconIds()` lists all known IDs; `removeIcon` uninstalls.

**Key exports / primitives:**
- `setIcon(el, id)` — primary API; idempotent (no replace if same icon already present).
- `getIcon(id)`, `getIconIds()` — lookup.
- `addIcon(id, svg)`, `removeIcon(id)` — registry mutation; built-in IDs use a `lucide-*` prefix.

**On-disk contracts:** none.

**Cross-domain dependencies:** consumed by `Setting`, `Menu`, `ButtonComponent`, ribbon, status bar — everywhere a button needs glyph.

**Corbomite-relevance note:** Replaceable — Corbomite uses `QIcon::fromTheme` (KDE icon theme) per project convention. Plugin-API-wise this should map to a "register your custom icon SVG, retrieve as `QIcon`" wrapper.

**Pass 2 focus:** complete list of built-in `lucide-*` IDs Obsidian ships with (so any plugin asking for one finds it in Corbomite).

---

## `obsidian/ui/menu` — context menus

**Files:** 3 (`Menu.js`, `MenuItem.js`, `MenuSeparator.js`).

**What it does:** Obsidian's HTML/CSS context menu (with optional native-menu fallback on macOS). Owns its own `Scope` for arrow-key navigation, supports submenus, sections, checked state, icons, danger styling, custom click handlers.

**Key exports / primitives:**
- `Menu` — `addItem(cb => MenuItem)`, `addSeparator()`, `setNoIcon()`, `showAtMouseEvent(evt)`, `showAtPosition({x,y})`, `addSections(...)`. Static `Menu.useNativeMenu` toggle.
- `MenuItem` — `setTitle`, `setIcon`, `setChecked`, `setDisabled`, `setIsLabel`, `setSection`, `setSubmenu`, `setWarning`, `onClick`.
- `MenuSeparator` — `addSeparator()` returns one.

**On-disk contracts:** none.

**Cross-domain dependencies:** depends on `core` (Scope), `platform`, `ui/icons`; consumed by `workspace`, every right-click in the app, plugin-built UIs.

**Corbomite-relevance note:** Replaceable — Corbomite uses `QMenu` directly. The plugin-API-equivalent must offer a builder pattern over `QMenu`.

**Pass 2 focus:** the `addSections` ordering protocol (Workspace fires `file-menu`/`url-menu`/`editor-menu` events letting plugins add sections to a menu mid-construction); the ordered section IDs.

---

## `obsidian/ui/popups` — modals, suggesters, hover popovers, notices

**Files:** 8 (`AbstractInputSuggest.js`, `FuzzySuggestModal.js`, `HoverPopover.js`, `Modal.js`, `Notice.js`, `PopoverState.js`, `PopoverSuggest.js`, `SuggestModal.js`).

**What it does:** Five flavours of overlay UI:
- `Modal` — full-screen dim-bg dialog with title/header/content; owns its own Scope, animation, Esc-to-close.
- `Notice` — toast notifications, top-right of viewport, auto-dismiss.
- `SuggestModal` / `FuzzySuggestModal` — text-input + scrolling suggestion list (quick switcher pattern).
- `PopoverSuggest` / `AbstractInputSuggest` — anchored popover suggesters (used inside form inputs).
- `HoverPopover` — the hover-preview popover (e.g. mouse over a `[[link]]` to preview the target note); has a focus-mode for pinning.
- `PopoverState` — internal state-machine constants.

**Key exports / primitives:**
- `Modal` — `open()`, `close()`, `onOpen()`, `onClose()`, `containerEl`, `contentEl`, `titleEl`, `scope`, `setTitle`.
- `Notice` — `new Notice(text, durationMs)`; `setMessage`, `hide`.
- `SuggestModal<T>` — abstract `getSuggestions(query)`, `renderSuggestion(item, el)`, `onChooseSuggestion(item, evt)`. Also fuzzy variant.
- `PopoverSuggest`/`AbstractInputSuggest` — anchored popover suggesters for textareas/inputs.
- `HoverPopover` — owned by a parent `Component`; `containerEl`, `hoverEl`, `isFocused`, hover-link-source integration.

**On-disk contracts:** none.

**Cross-domain dependencies:** depends on `core` (Scope), `ui/icons`, `platform`; consumed by `workspace` (quick switcher, command palette), plugins, hover-link rendering.

**Corbomite-relevance note:** Replaceable — Corbomite uses `KMessageBox`, `QDialog`, `KMessageWidget` (notice-equivalent), `QCompleter` (suggester-equivalent), `QToolTip`. `QuickSwitcher.cpp` already shadows `FuzzySuggestModal`.

**Pass 2 focus:** the hover-popover lifecycle (especially the "hold mod-key to pin" semantics and child-popover chains); the SuggestModal scope-vs-input keyboard handling.

---

## `obsidian/utils` — miscellaneous helpers

**Files:** 14 (`apiVersion.js`, `arrayBufferToBase64.js`, `arrayBufferToHex.js`, `base64ToArrayBuffer.js`, `debounce.js`, `getBlobArrayBuffer.js`, `getLanguage.js`, `hexToArrayBuffer.js`, `logException.js`, `moment.js`, `requireApiVersion.js`, `resolveSubpath.js`, `stripHeading.js`, `stripHeadingForLink.js`).

**What it does:** Plugin-API utility grab-bag. `apiVersion`/`requireApiVersion` for plugin compat checks. `debounce` (the canonical one — used everywhere internally too). `moment` is re-exported moment.js (the entire library is exposed as `window.moment`). `resolveSubpath(cache, subpath)` resolves `#heading` and `#^block` references against a `CachedMetadata`. `stripHeading`/`stripHeadingForLink` clean heading text for use as link targets. The base64/hex/buffer helpers are utility conversions. `getLanguage()` returns the active locale. `logException(err)` is the structured error reporter.

**Key exports / primitives:**
- `debounce(fn, wait, immediate)` — used pervasively internally; `.run()` and `.cancel()`.
- `resolveSubpath(cache, subpath)` — heading + block-reference resolution (vault-format-critical).
- `stripHeading(text)` / `stripHeadingForLink(text)` — heading normalisation rules used in link targets.
- `apiVersion`, `requireApiVersion(v)` — plugin compat gate.
- `moment` — the moment.js library; date-format compat target.
- `arrayBufferToBase64`/`base64ToArrayBuffer`/`arrayBufferToHex`/`hexToArrayBuffer`/`getBlobArrayBuffer` — buffer conversions.
- `getLanguage()` — i18n locale getter.
- `logException(err)` — error-pipeline entry.

**On-disk contracts:** `resolveSubpath` and `stripHeading*` together define how `[[Note#Heading]]` and `[[Note#^block-id]]` resolve — vault-format-critical.

**Cross-domain dependencies:** consumed everywhere.

**Corbomite-relevance note:** Mostly replaceable — Qt provides `QByteArray::toBase64`, `QLocale`, etc. **`resolveSubpath` and `stripHeading*` are NOT replaceable** — they define link resolution semantics Corbomite must replicate exactly.

**Pass 2 focus:** the **exact** rules in `stripHeading`/`stripHeadingForLink`/`resolveSubpath`; moment.js format strings used by `MomentFormatComponent` (for date-formatted filenames in templates / daily notes).

---

## `obsidian/vault` — vault, file tree, filesystem adapters

**Files:** 10 (`CapacitorAdapter.js`, `FileManager.js`, `FileSystemAdapter.js`, `getLinkpath.js`, `normalizePath.js`, `parseLinktext.js`, `TAbstractFile.js`, `TFile.js`, `TFolder.js`, `Vault.js`).

**What it does:** The vault layer. `Vault` is the in-memory file tree (`TFile`/`TFolder` nodes rooted at a path), backed by either `FileSystemAdapter` (Electron desktop, uses `original-fs` to bypass ASAR + `btime` for birthtime + case-sensitivity probe) or `CapacitorAdapter` (mobile). `Vault` emits `create`/`modify`/`delete`/`rename`/`closed`/`raw` events and handles `.trash`/system-trash deletion modes. `FileManager` is higher-level: handles file-rename refactoring (link rewriting across the vault), trash, frontmatter rewrite (`processFrontMatter`), generate-markdown-link, copy/move with conflict resolution, attachments folder. `getLinkpath`/`normalizePath`/`parseLinktext` are the path-string utilities.

**Key exports / primitives:**
- `Vault` — `getFiles()`, `getMarkdownFiles()`, `getAbstractFileByPath`, `cachedRead`, `read`, `readBinary`, `modify`, `create`, `delete`, `trash`, `rename`, `getConfig(key)`, `setConfig(key, val)`. Events: `create`, `modify`, `delete`, `rename`, `closed`, `raw`, `config-changed`.
- `FileManager` — `renameFile`, `trashFile`, `processFrontMatter`, `generateMarkdownLink`, `getAvailablePathForAttachment`, `promptForFileRename`. Owns the attachments-folder + new-link-format settings.
- `TAbstractFile` / `TFile` / `TFolder` — tree node hierarchy. `TFile.basename`, `extension`, `stat`, `saving`. `TFolder.children`, `isRoot`.
- `FileSystemAdapter` — desktop adapter; case-sensitivity probe (`.OBSIDIANTEST`), `original-fs`, `btime`, IPC to main process.
- `CapacitorAdapter` — mobile adapter (skim, Corbomite is desktop).
- `normalizePath(s)` — NFC + collapse-slashes; **vault-format-critical**.
- `getLinkpath(linktext)` — strip `#heading`/`#^block` from a link.
- `parseLinktext(linktext)` — split into `{path, subpath}`.

**On-disk contracts:** every file in the vault, plus the `.obsidian/` config directory. Path normalisation rules (NFC, forward-slashes only, root-relative) define the wire-format.

**Cross-domain dependencies:** root of the data graph; `metadata`, `workspace`, `views`, `editor`, `bases`, `search` all read through `Vault`.

**Corbomite-relevance note:** Partial. Corbomite has `libs/storage/FileSystemAdapter` + `VaultScanner` + `libs/core/NoteDocument` + `VaultService`. The events shape (`create`/`modify`/`delete`/`rename`/`raw`) is the contract Corbomite's analogue must emit. `FileManager` link-rewriting on rename is partial in Corbomite (Pass 2 to confirm).

**Pass 2 focus:** the link-rewriting algorithm in `FileManager.renameFile`; `processFrontMatter` semantics (atomic? merge? overwrite?); `getConfig`/`setConfig` and which keys it reads/writes; the `.trash` vs system-trash decision logic; case-sensitivity handling.

---

## `obsidian/views` — View / FileView / ItemView hierarchy

**Files:** 6 (`EditableFileView.js`, `FileView.js`, `ItemView.js`, `TextFileView.js`, `View.js`, `ViewRegistry.js`).

**What it does:** The view-class hierarchy mounted into `WorkspaceLeaf`s. `View` is the abstract base (lifecycle, container, `getViewType`, `getDisplayText`, `getIcon`). `ItemView` adds an action-bar (header buttons). `FileView` is a `View` bound to a `TFile` (with breadcrumbs, file-menu integration, navigation). `EditableFileView` adds inline-rename. `TextFileView` adds debounced auto-save (`requestSave` → 2s debounce → `save`) for text-content views. `ViewRegistry` maps viewType↔factory and extension↔viewType; emits `view-registered`/`view-unregistered`.

**Key exports / primitives:**
- `View` — `getViewType()`, `getDisplayText()`, `getIcon()`, `onload()`/`onunload()`, `onOpen()`/`onClose()`, `containerEl`, `leaf`, `app`, `setEphemeralState`/`getEphemeralState`, `setState`/`getState`.
- `ItemView` — `addAction(icon, title, cb)`; `actionsEl`.
- `FileView` — `file`, `onLoadFile(file)`, `onUnloadFile(file)`, `renderBreadcrumbs`, `allowNoFile`, `navigation`.
- `EditableFileView` — inline-rename via `setEphemeralState({rename: 'start'|'end'})`.
- `TextFileView` — `data`, `dirty`, `requestSave()`, `save(immediate)`, `getViewData()`, `setViewData(data, clear)`, `clear()`. **Save-on-debounce contract.**
- `ViewRegistry` — `registerView(type, factory)`, `registerViewWithExtensions(extensions, type, factory)`, `unregisterView`, `getViewCreatorByType`, `typeByExtension`. Events: `view-registered`, `view-unregistered`, `extensions-updated`.

**On-disk contracts:** none directly; `TextFileView.save` writes through `Vault.modify`.

**Cross-domain dependencies:** depends on `core`, `vault`, `workspace`; consumed by `editor/markdown` (`MarkdownView`), `bases` (`BasesView`), built-in canvas/image/PDF viewers, every plugin-registered view.

**Corbomite-relevance note:** Partial. Corbomite has tab/view abstractions in `src/editor/` (`EditorViewSpace`, `EditorViewManager`, `NoteEditorWidget`). The `ViewRegistry` extension↔factory mapping is the right abstraction to mirror for canvas/base/PDF support.

**Pass 2 focus:** the exact `View` lifecycle order (`onload` vs `onOpen` vs `setState` vs `onLoadFile`); `setState`/`setEphemeralState` shape (this is what `Workspace.openLinkText` uses to communicate scroll position, search match, rename-mode).

---

## `obsidian/workspace` — workspace layout, splits, tabs, leaves

**Files:** 12 (`Workspace.js`, `WorkspaceContainer.js`, `WorkspaceFloating.js`, `WorkspaceItem.js`, `WorkspaceLeaf.js`, `WorkspaceParent.js`, `WorkspaceRibbon.js`, `WorkspaceRoot.js`, `WorkspaceSidedock.js`, `WorkspaceSplit.js`, `WorkspaceTabs.js`, `WorkspaceWindow.js`).

**What it does:** The whole layout subsystem. A tree of `WorkspaceItem`s: `WorkspaceRoot` at top, `WorkspaceSplit` for h/v splits, `WorkspaceTabs` for tab-groups, `WorkspaceLeaf` for one mounted view, `WorkspaceSidedock` for collapsible left/right docks, `WorkspaceWindow` for popout windows, `WorkspaceFloating` for floating tab-groups, `WorkspaceRibbon` for the activity-bar. `Workspace` itself is the central coordinator: tracks `activeLeaf`/`activeTabGroup`, emits the "everyone listens for" events (`active-leaf-change`, `file-open`, `layout-change`, `layout-ready`, `quick-preview`, `resize`, `window-frame-change`, `file-menu`, `url-menu`, `editor-menu`, `swipe`), persists/restores layout JSON, handles drag-drop tab reordering, popout windows, command-driven leaf creation (`getLeaf(false|'tab'|'split'|'window'|true)`), `openLinkText`, `revealLeaf`, hover-link-source registry, protocol-handler registry, edit-history (Workspace owns `undoHistory`).

**Key exports / primitives:**
- `Workspace` — `activeLeaf`, `getActiveFile`, `openLinkText`, `getLeaf`, `getLeavesOfType`, `revealLeaf`, `iterateAllLeaves`, `setActiveLeaf`, `splitActiveLeaf`, `duplicateLeaf`, `requestSaveLayout`, `getLayout`/`changeLayout`, `registerHoverLinkSource`, `registerEditorExtension`, `registerObsidianProtocolHandler`. Events: `active-leaf-change`, `file-open`, `layout-change`, `layout-ready`, `quick-preview`, `resize`, `window-frame-change`, `file-menu`, `url-menu`, `editor-menu`, `swipe`.
- `WorkspaceLeaf` — `view`, `parent`, `containerEl`, `tabHeaderEl`, `getViewState`, `setViewState`, `setPinned`, `togglePinned`, `setGroupMember`, `detach`. Events: `pinned-change`, `group-change`.
- `WorkspaceSplit` / `WorkspaceTabs` — split/tab containers, child management, drag-drop reorder, stack mode.
- `WorkspaceSidedock` — collapsible left/right dock; `expand`/`collapse`/`toggle`.
- `WorkspaceWindow` — popout window (separate Electron `BrowserWindow`).
- `WorkspaceFloating` / `WorkspaceContainer` / `WorkspaceItem` / `WorkspaceParent` — abstract bases / floating-window layer.
- `WorkspaceRoot` — root item.
- `WorkspaceRibbon` — activity bar (left or right edge ribbon icons + settings cog).

**On-disk contracts:** persisted layout in `.obsidian/workspace.json` (default) and per-named-workspace in `.obsidian/workspaces.json`.

**Cross-domain dependencies:** depends on `core`, `vault`, `views`; consumed by `plugin`, every `View`.

**Corbomite-relevance note:** Partial. Corbomite has split/tab plumbing in `src/editor/EditorViewManager`, `EditorViewSpace`, sidebar `KSelector`-like panels in `src/sidebar/`. Popout-windows, hover-link sources, protocol handlers, named-workspace switching: NOT confirmed. The persisted-layout JSON schema is the compat target for "open a vault and have your layout restored".

**Pass 2 focus:** the layout-JSON schema (most interop-critical part); `getLeaf` modes; `openLinkText` resolution; the **complete event list** (this is a primary plugin extension surface — see `02-extension-surfaces.md`).

---

## Addenda

> **Append-only.** When implementation reveals new or corrected facts about Obsidian, write a dated file in `addenda/` and link it here (most recent on top). See `addenda/README.md` for the format. **Never edit the taxonomy or domain docs above** — they are snapshots of audit-time belief and must remain stable.

- [2026-04-15 — Daily Notes + Templates JSON schemas](addenda/2026-04-15-daily-notes-templates-schemas.md)
