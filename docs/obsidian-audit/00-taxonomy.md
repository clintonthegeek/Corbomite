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

- [2026-10-04 — **CORRECTION + NEW (core):** the `["Mod"]` + `code:"KeyI"` example in `2026-10-03-core-command-and-hotkey-contracts.md` §D **cannot match Ctrl+Option+I — in Obsidian either**: `isMatch` compares modifier *sets* exactly (`app/own/0747-ly.js:130-133`) and Ctrl+Option also holds Alt, so `"Meta" !== "Alt,Ctrl,Meta"`; it needs `["Mod","Alt"]`. The addendum's *mechanism* is right — `bake()` folds `code` through `Xw` into the one `key` slot (`:69`) and `onTrigger` compares that string, so **a `code` binding is matched as a *name*, never as a physical position**, in Corbomite or Obsidian. What `QKeySequence` cannot express is the *spelling* (`NumpadAdd`, `MediaPlayPause`), which must then be refused rather than coerced. Measured Qt 6.11 quirks that a Qt reader must know: `ArrowLeft`/`PageUp`/`Minus`/`BracketLeft`… have no Qt key name; `Enter` is `Qt::Key_Enter` (numpad) not Return; an unknown name yields a **one-step** `Qt::Key_unknown` sequence, not an empty one; modifiers must precede the key. Now implemented end-to-end in `Corbomite::HotkeyDispatcher`](addenda/2026-10-04-hotkeys-json-dispatcher.md)
- [2026-10-03 — **CORRECTION + RESOLUTION (canvas/views/workspace):** **A canvas leaf's viewport is NOT in `eState` — it is `ViewState.state.viewState`**, a persisted `{x, y, zoom}` (`app/own/2028-n9.js:844,850` → `app/own/2018-k7.js:968-974`). `zoom` is **log2 of scale** (`scale = 2^zoom`, clamped `[-4,1]` = `[0.0625,2]` every frame); **`x`/`y` are the scene point at the viewport centre** (`translate(W/2,H/2) scale(2^zoom) translate(-x,-y)`). It reaches `workspace.json` via `requestSaveLayout` on viewport settle. **Settles the 2026-10-01 contradiction: `eState` is NEVER serialised** — `WorkspaceLeaf.serialize` writes only `id`/`type`/`dimension?`/`pinned?`/`state`/`group?` (`1209-jI.js:559-563`), so `views.md:151` "not persisted" is **right** and `workspace.md:214`/`:220` (the `UndoHistoryEntry` and `LeafHistoryState` shapes) are **in-memory runtime structs misdescribed as on-disk records**. A canvas leaf's `getEphemeralState()` returns **`{}`** — no class in the chain (`CZ`→`yZ`→`xI`→`ky`→`wy`) overrides it; the canvas reads only `focus` and `match` inbound. **Cluster M M5.3's channel is wrong** (field names right)](addenda/2026-10-03-canvas-viewport-ephemeral-state-resolution.md)
- [2026-10-03 — **DELIBERATE DIVERGENCE (rendering):** Corbomite will **not** match Obsidian's `ALLOW_UNKNOWN_PROTOCOLS: true`, which switches off DOMPurify's URI-scheme check so `javascript:` hrefs survive sanitisation on the main render path. `rendering.md` §11's "must match this config exactly" is rejected **for this one flag**, on security grounds; the other four keys are matched. Fix any future scheme complaint with a narrow allowlist, not a blanket re-enable](addenda/2026-10-03-rendering-sanitiser-deliberate-divergence.md)
- [2026-10-03 — **CORRECTION + NEW (leaf-utilities):** **Heading rename matches with `stripHeading` but writes with `stripHeadingForLink`** — asymmetrically, in adjacent lines (`app/own/1312-hR.js:205-206`). The match side strips `* ! [ ] % ` + backtick that the write side **keeps**, so matching is permanently looser than the bytes written. The ref set is `frontmatterLinks ∪ links ∪ embeds`, so **renames rewrite YAML property values**, and the applier **re-dumps the whole frontmatter block through js-yaml** — silently keeping body edits but dropping frontmatter edits if the YAML won't parse, and silently no-opping on dotted keys. Block-id rename uses a third rule entirely. `SecretStorage` **extends `Events`** and fires `changed` 3x (refuting §4); `Scope.handleKey` lets a specific binding returning `undefined` **swallow** the event; `requestUrl` **drops `opts.headers`** on the web path.](addenda/2026-10-03-leaf-utilities-subpath-rename-and-leaf-bundle-corrections.md)
- [2026-10-03 — **CORRECTION + NEW (ui-bundle):** **The menu section protocol is a hierarchical `a.b.c` namespace** (`app/own/0751-my.js:62-81`): separators key on the id's *first dot-segment*, and `submenuConfigs` matching is **prefix-based**, so `info` and `info.copy` render contiguously with no separator — the doc's flat list cannot produce this. **The shipped Lucide table has 1408 entries, not 125** and is *generated* by `getIconIds()`, not hand-listed. `PopoverState` is a **string enum with reversed member order** (so §2, §8's `Hidden === 0` and §13 Q1 are all wrong); `SliderComponent.setDynamicTooltip`/`showTooltip` are empty stubs; `Modal.open` listens for `pagehide`, not `beforeunload`; `openSubmenuSoon` is a leading-edge throttle. **Web→desktop UI translation is intended, so DOM-widget choices are not reported as gaps** — only observable-behaviour and data-contract differences.](addenda/2026-10-03-ui-bundle-widgets-and-overlays-source-proof.md)
- [2026-10-03 — **CORRECTION + NEW (core):** **`.obsidian/hotkeys.json` is override-only** (`app/own/0833-$w.js:57-76`): `bake()` walks custom keys first and falls back to defaults only when `!hasOwnProperty` — so `"some:command": []` **unbinds** the command, the only way Obsidian expresses an unbind. **115 command ids, not "30+"**; implementing only the doc's 25 silently drops every `editor:*` override in a real vault. `Modal` allocates a **parentless** `Scope`, so the root scope and its hotkeys are unreachable while a plain modal is open. `Events.off` deletes *all* listeners on a no-match. 21 NEW + 8 corrections; §13 Q1-Q6 answered, Q7 refuted.](addenda/2026-10-03-core-command-and-hotkey-contracts.md)
- [2026-10-03 — **CORRECTION + NEW (plugin):** **Corbomite deliberately does not mirror this domain's API** — a C++ plugin is not a web plugin. What *is* transferable is documented as a 15-item capability surface. Load-bearing facts: **there is no permission model at all** (zero permission hits; the only gate is `Ab.plugins`); enable-set order *is* load order; `minAppVersion` is a real install gate; a throw from `onload` **leaks a half-loaded instance that is never retried**; `loadCSS()` is not opt-in (§12 is backwards); `addCommand` **silently overwrites** on duplicate id; `saveData` **cannot report a failed write**. §10's enumeration is complete (26 members). 8 corrections + 17 catalogued defects. Web-delivery surface (`require` shim, `window.eval`, inline-sourcemap stripping, hot-reload) is explicitly marked **not applicable by design**.](addenda/2026-10-03-plugin-loader-lifecycle-and-capability-surface.md)
- [2026-10-03 — **CORRECTION + NEW (settings):** **Definitive tab -> config-key -> on-disk-file table.** §3's table is wrong in 3 of 7 rows: **4 keys it names do not exist** (proven exhaustively — 55 unique `getConfig`/`setConfig` keys, all keys of the defaults table `ME` at `app/own/0920-eE.js:1174-1237`), and **17 real keys are missing**. **`.obsidian/<corePluginId>.json` is a whole file class the doc omits** — core-plugin data goes through `readConfigJson(pluginId)`, *not* `plugins/<id>/data.json`. Encodings that corrupt a vault if missed: `openBehavior` is `"file:" + path`, not an enum; the three font keys are comma-joined **strings**. `display()` is no longer the contract (declarative `getSettingDefinitions()`), so a `display()`-only tab is **invisible to global settings search**. Safe mode is a `localStorage` flag, not absence of `community-plugins.json`. Cross-checked and **confirmed** the 16-key `app.json`/`appearance.json` partition.](addenda/2026-10-03-settings-config-file-ownership-and-definitions.md)
- [2026-10-03 — **CORRECTION + NEW (search):** **The DSL grammar is in-tree and readable** (`app/own/1175-SP.js:148-221`, a ~60-line operator table) — refuting §13 Q1's "out-of-tree, reverse-engineer from documentation". Real flags are `match-case` + **`ignore-case`**; `whole-word` **does not exist**. Missing from the doc: `content:`, `task:`, `task-todo:`, `task-done:`, `[key:value]`, `TRUE`/`FALSE`/`EMPTY`, `>`/`<`. **`PARITY-MATRIX.md` §5 overclaims**: quick-switcher fuzzy constants match but the *penalty rule* does not (two worked examples).](addenda/2026-10-03-search-dsl-grammar-and-quick-switcher-scoring.md)
- [2026-10-03 — **CORRECTION + NEW (views):** **There is no three-way merge** (`app/own/1810-bZ.js:2-8`): `diff_main(base, view)` -> `patch_make` -> `patch_apply(patches, disk)[0]`, with per-patch success discarded. Demonstrated by execution against the extracted code: a mid-paragraph user edit is **discarded and its text relocated onto the preceding line**, with only a notice. Refutes §1/§12's "does not lose in-progress edits". Also: a **second registry `app.embedRegistry`** exists besides `viewRegistry`, and `_empty` is a per-leaf singleton, not global.](addenda/2026-10-03-views-leaf-viewstate-and-deferred-restore.md)
- [2026-10-03 — **CORRECTION + NEW (editor-markdown):** **The async preview worker is single-flight and contention degrades to a blocking main-thread parse** (`app/own/1500-wW.js:11-13` returns `null` when in flight; `parseAsync` doesn't check, so the `catch` runs `parseSync()`). `TW` is module-level, so contention is app-wide — a Corbomite thread-pool-with-queue would diverge. **`commonmark: true` is hard-coded with no user escape hatch**; `breaks` is a process-global singleton. 8 corrections (save throttle, Ctrl+Wheel, double-tap, sortOrder, rAF-vs-timeout) + full citation remap: every `MarkdownView.js:*` style reference in the frozen doc is dead.](addenda/2026-10-03-editor-markdown-view-modes-and-preview-pipeline.md)
- [2026-10-03 — **CORRECTION + NEW (editor):** **`Editor` is half-documented.** The public export binds the *mixin*; the concrete `EditorView`-owning subclass (`app/own/1823-RJ.js:885`) adds **~45 undocumented methods** — the 7-entry format-markup table, a ~48-binding base keymap, undo-history snapshotting, fold-restore line-count gate, clipboard hooks. §14's sizing basis is off ~2x. `exec` is a fixed **18-key** table that **throws** on unknown names. `insertCallout()` selects `NOTE` in both branches. Suggesters are link / tag / **footnote** (no headers suggester). **Audit-wide: `debounce` (`Ml`, `app/own/0355-Ml.js:22-27`) is a leading-edge throttle, not a trailing debounce** — repeat calls inside the window do not reset the timer, so every debounce/timing claim needs re-reading.](addenda/2026-10-03-editor-wrapper-commands-and-keymaps.md)
- [2026-10-03 — **CORRECTION + NEW (canvas):** the `.canvas` serializer is **`$d`/`Zd`** (`0563-$d.js`/`0564-Zd.js`), **not** `dc`/`fc` — tab indent, all-primitive arrays/objects collapse to one compact line, `undefined` keys dropped, no space after colon, no trailing newline — so §3 item 6 and §13 Q4 name the wrong function and byte parity is cheap. **`nodes` is re-sorted by `zIndex` on every save** (`edges` are not) and file order *is* z-order. **Three silent-loss paths**: unknown node `type`s and endpoint-dangling edges are destroyed on the next save, while **explicit defaults** (`fromEnd:"none"`, `toEnd:"arrow"`, `color:""`, `backgroundStyle:"cover"`) are *preserved* through `unknownData` and `text`/`file`/`url` are *always* written. §13 Q2/Q3 answered (`mW` = YouTube + Twitter only; `backgroundStyle` closed at three). Also: `canvas.md`'s symbol map is **misleading** in the split corpus (`dc`/`fc` are tree-sitter, `V5`/`H5`/`j5` are Bases, `A3` is an embed widget) — replacement map supplied. Full `canvas/data.json` schema, the align/arrange/distribute/justify family (§9.5 has none), all viewport/snap/gesture constants, the link index's 5000-unit threshold, export padding **100** + hard **2×** ceiling. §11.1 is **stale**: its four interop fixes and its bezier/containment criticisms are already done](addenda/2026-10-03-canvas-disk-contract-and-viewport-settings.md)
- [2026-10-03 — **CORRECTION + NEW (rendering):** the DOMPurify config is **five keys** (`ALLOW_UNKNOWN_PROTOCOLS: true`, `RETURN_DOM_FRAGMENT`, `FORBID_TAGS:["style"]`, `ADD_TAGS:["iframe"]`, `ADD_ATTR`) and is called **`FF`**, not the `SL` §13 spent an open question hunting (`SL` is the CLI handler registry, `EL` a Levenshtein helper). **No `ALLOWED_TAGS`/`ALLOWED_ATTR`**, so stock 3.0.1 defaults apply — and `ALLOW_UNKNOWN_PROTOCOLS: true` means the URI-scheme check never runs. Plus three `afterSanitizeAttributes` hooks with two different iframe-sandbox allowlists and an internal-link `nofollow` exemption. §13 Q1 answered](addenda/2026-10-03-rendering-sanitiser-policy.md)
- [2026-10-03 — **CORRECTION + NEW (rendering):** `htmlToMarkdown`'s Turndown service is `MO` (not `hP`; Turndown is a global) with six options and **nine custom rules** verbatim — incl. `highlight` → `==…==` (Obsidian-only) and `highlightedCodeBlock` (`div.highlight-(text|source)-<lang> > pre` → fenced block). **`MO.escape = function(e) { return e; }` — output escaping is switched off**, refuting §8 invariant 11's "sanitisation is on the input". §13 Q2 answered](addenda/2026-10-03-rendering-html-to-markdown-turndown.md)
- [2026-10-03 — **CORRECTION + NEW (rendering):** math is a `.math:not(.is-loaded)` markdown post-processor; display mode is read off the **`math-block` class**; only `&amp; &lt; &gt; &quot;` are entity-decoded; **invalid LaTeX falls back to the literal source text**, not a "MathJax error span" (§9 wrong); and the 100 ms batch **never calls `typeset`** (`startup.typeset: !1`) — it swaps `MathJax.chtmlStylesheet()` into `<head>` and **toggles `data-change` between `"1"` and `"2"`**; recycling transplants already-typeset math nodes](addenda/2026-10-03-rendering-math-pipeline.md)
- [2026-10-03 — **NEW + CORRECTION (rendering):** mermaid is behind a **`mermaid-vault-trust` vault gate** whose guarded DOM and "Allow" button are now specified verbatim — granting trust fires **`post-processor-change`** (a full re-render). The post-`render` pass **namespaces SVG marker-IDs**, re-arms `<a>` hrefs and grafts `<a class="internal-link">` onto mermaid foreignObject labels; errors are **plain text**, not an error pane (§9 wrong). Mermaid is **11.13.0** and the documented `break-spaces` local patch is verified applied](addenda/2026-10-03-rendering-mermaid-pipeline.md)
- [2026-10-03 — **NEW + CORRECTION (rendering):** the post-processor registry has an undocumented sibling **`registerRecycler`** list; `codeBlockPostProcessors` is a **plain object, not a `Map`** (§7 wrong) and double registration **throws**; code-block registration is **dual** (wrapper for reading view, raw fn for live preview) and the two plugin verbs **return different things**; `ctx.replaceCode`/`ctx.promises` specified. Plus the `gm()` loader verbatim (bundled assets, **retryable on failure**), `Prism.manual = true`, the fixed PDF.js viewer options, and `.is-flashing` for 3 s](addenda/2026-10-03-rendering-postprocessor-recycler-registries.md)
- [2026-10-03 — **NEW + CORRECTION (rendering):** the callout DOM is specified verbatim (4 `data-callout-*` attributes, **`--callout-color` only for 6-digit hex**, `callout-icon` before `callout-title-inner`), the fold post-processor's `setTimeout(…,0)` + `defaultPrevented` re-check is what lets a link inside a callout title win the click race, and icons accept three forms including **sanitised inline SVG**. **Reading view sets `dir` per table cell** from a **non-UBA** first-strong regex, so §11's "Qt is per-paragraph only" row mislocates the bidi gap; the doc's `mW` bidi table is **dead code** (live twin is `Rp`)](addenda/2026-10-03-rendering-callout-and-bidi-dom.md)
- [2026-10-03 — **CORRECTION + NEW (rendering):** tooltips are a **delegated `document.body`** listener pair on `[aria-label]` with a **1000 ms default hover delay** (not 0), a 100 ms sweep cooldown whose mechanism is now explained, a global `pointerup` dismiss and a `--no-tooltip` opt-out — so `QToolTip` cannot reproduce them. `renderResults` is `lb` and the highlight class is exactly **`suggestion-highlight`** (§13 Q5 answered, the doc's hedged "or analogous" confirmed)](addenda/2026-10-03-rendering-tooltip-and-match-highlight.md)
- [2026-10-03 — **CORRECTION + NEW (rendering):** `data-href` is the **resolved path** for `TFile` targets, not always the linktext (§1 wrong), and the label fallback is string-vs-`TFile` with no "raw linktext for unresolved" branch; `nE`=`QE` is a **`>`-breadcrumb joiner**, `Ic`=`Av` suppresses the middle-click default, `Lc`=`Dv` is an **enters-element test** (not a modifier predicate), `Yc`=`$v` is **`new URL()` parseability** (not a scheme allowlist) — §13 Q3/Q4/Q6/Q7 answered. **`NumberValue` renders `NaN` as `"NaN"`**; only ±Infinity is `"∞"` (§8 invariant 5 wrong); plus the image dimension-spec carve-out and the extension-family table](addenda/2026-10-03-rendering-rendercontext-value-and-symbol-map.md)
- [2026-10-03 — **Re-verified + CORRECTION (rendering):** coverage verdict — **no `_internal.js` exists in 1.13.7**, so `editor.md`'s mislocation defect does **not** transfer; `rendering.md`'s is that §13 filed two *extractable* facts as un-extractable. Every load-bearing citation remapped; the real markdown→HTML chain is `WT → KT → GT(allowDangerousHtml: true) → RF(DOMPurify) → appendChild → postProcessors`, i.e. **Obsidian's own rendered markdown is sanitised**; `ctx.replace`/`getSectionInfo` are `return null` stubs in the one-shot path; and §3's "No on-disk contracts" is refuted by three `localStorage` keys](addenda/2026-10-03-rendering-citation-remap-and-coverage.md)
- [2026-10-03 — **CORRECTION + NEW (parsing):** `getFrontMatterInfo`'s `from`/`to`/`contentStart`/`frontmatter` are **UTF-16 code units, not bytes** (§1/§2 and `VAULT-FORMAT.md` §4.1 say "byte offset"); existing `---` delimiters are **preserved verbatim** — §3's "CRLF-original files get LF delimiters after write-back" is wrong; and Obsidian has **two** frontmatter scanners (regexes vs a hand-rolled block rule) that **disagree on CRLF** (writer sees it, indexer doesn't) and on **fences with trailing text/spaces** (the reverse). Plus: `parseFrontMatterAliases` returns `[]` not `null`, an undocumented fifth `cssclasses` helper, and `VAULT-FORMAT.md` §4.1's "P0 correctness bug in Corbomite" does not exist](addenda/2026-10-03-parsing-frontmatter-info-offsets-and-parser-divergence.md)
- [2026-10-03 — **CORRECTION + NEW (parsing):** the wikilink parser returns `{href, title, isAlias}` (not `{path, subpath}`) and does five steps before the `path`/`subpath` split — alias split, trailing-`\` strip, **NFC normalise**, NBSP→space, trim. **The resolver never folds** (`bD`/`KF`/`getLinkpathDest` are case-insensitive but not Unicode-normalising), so `VAULT-FORMAT.md` §9 puts NFC in the wrong layer. Plus: the default display text for `[[Note#H1#H2]]` is the breadcrumb **`"Note > H1 > H2"`** and lands in `CachedMetadata.links[].displayText` (written unconditionally); `[[|foo]]` targets a file named `|foo`; an interior containing `[[` is not a link](addenda/2026-10-03-parsing-wikilink-grammar-nfc-and-breadcrumb.md)
- [2026-10-03 — **NEW + CORRECTION (parsing):** the body-tag grammar is one explicit character class — `/` and `#` are **not** legal inside a tag (so subtags are a search-time `^(regex)($|\/)` convention, not lexical), a `#`+digits-only token is not a tag, and a tag must follow whitespace; frontmatter tags instead use a **literal U+0020**-only filter, so the two sources disagree. `getAllTags` is frontmatter-first with **no de-dup**, while the properties `getTags()` is inline-first **with** de-dup. Block ids have **two** rules and are grafted onto the **preceding sibling** node (nested sub-lists are not searched)](addenda/2026-10-03-parsing-tag-and-blockid-grammar.md)
- [2026-10-03 — **CORRECTION (parsing):** `resolveSubpath`'s block and footnote branches have **no early return on a miss** — both fall through into the heading walk; footnote ids match **case-sensitively** via an unguarded `slice(2,-1)`, while block ids are lowercased. Confirms §8's `stripHeading`/`stripHeadingForLink` regexes byte-identical (they are `SD`/`TD` and `xD`/`DD`), flags §15's dead `AT`/`PT` citations (those names are webpack module ids in 1.13.7), and records a **third** heading normaliser (`AD`) used for display text](addenda/2026-10-03-parsing-resolve-subpath-fallthrough.md)
- [2026-10-03 — **NEW (parsing):** footnote ids may not contain space/tab/LF, `[^a] : text` is not a definition, and `[^` is carved out of markdown-it's own reference syntax; a **dangling `[^x]` renders as the bare text `x`** and creates no cache entry (invisible to unresolved-link machinery); an unreferenced definition is not rendered; repeat labels are **`[1]`, `[1-1]`, `[1-2]`**; and an inline footnote's cache id is the literal string **`"[inline" + N`** — addressable by the otherwise-ungeneratable subpath `#[^[inline0]`. Rendered footnote `listItem`s are synthetic and never enter `CachedMetadata.listItems`](addenda/2026-10-03-parsing-footnote-grammar-and-inline-ids.md)
- [2026-10-03 — **NEW + CONFIRMED (workspace):** split `direction` divider-line convention has **six** independent source proofs in 1.13.7 (incl. `workspace:split-vertical` = "Split right", icon `lucide-separator-vertical`); corrects the *attribution* in the 2026-10-01 addendum — `workspace.md:79`'s gloss matches source. Plus a `WorkspaceSplit`/`Sidedock` confirmation table](addenda/2026-10-03-workspace-split-direction-source-proof.md)
- [2026-10-03 — **NEW + CORRECTION (workspace):** exact per-node and root key order (`main, left, right, left-ribbon, floating, active, lastOpenFiles`); `mobile-drawer.pinnedSize` (new field); window `direction` hard-coded `"vertical"` and **stale bounds saved when maximized**; `saveLayout` early-returns when `!layoutReady`; `hiddenItems` writes explicit `false` per item and the **right ribbon is never persisted**; `dimension` written on truthiness with an inclusive clamp; `recomputeChildrenDimensions` is all-or-nothing. Re-verifies all five 2026-06-10 claims as still holding](addenda/2026-10-03-workspace-node-schemas-and-key-order.md)
- [2026-10-03 — **NEW (workspace):** `deserializeLayout` **mutates the input JSON** (`children.splice`), wraps a bare `leaf` under a `split` in a synthesised tab group, gates popouts on desktop **and Electron major ≥ 13**, drops `stacked: true` without `canStackTabs`, flattens the root tree without `canSplit`, and resolves `active` by leaf id only if its dock is not collapsed](addenda/2026-10-03-workspace-deserialize-and-platform-gating.md)
- [2026-10-03 — **NEW (workspace):** `lastOpenFiles` is quota-capped per category — 25 md / 10 canvas / 10 image / 10 other, **≤ 55** on disk — while `getLastOpenFiles()` returns at most **10**; every vault `create` event also adds an entry; `load()` does no validation](addenda/2026-10-03-workspace-last-open-files-quota.md)
- [2026-10-03 — **CORRECTION (workspace):** **there is no `workspaces.json`** — named workspaces live in `.obsidian/plugins/workspaces/data.json` as `{workspaces: {<name>: LayoutJson & {mtime}}, active}`; refutes `workspace.md` §3 and the Pass-1 on-disk-contracts line](addenda/2026-10-03-workspace-named-workspaces-data-json.md)
- [2026-10-03 — **NEW + CONFIRMED (workspace):** the missing **`hover-link-sources-changed`** event (an 18th); `focusLeaf` closes the Settings tab in the target window; `registerEditorExtension` also rebuilds the spellcheck dictionary; `setViewState`'s completion order (`onLayoutChange`→`updateHeader`→`recordHistory`→`done`); tab-group menu has a **trailing** `""` section; `pushUndoHistory` skips empty leaves. Plus a batch confirmation of every debounce/history constant in §1, §5 and §8](addenda/2026-10-03-workspace-missing-event-and-small-items.md)
- [2026-10-03 — **CORRECTION + NEW (vault):** `app.json`/`appearance.json` split is an **18-key** allow-list, not 6 — 10 keys `vault.md` §2 files under `app.json` are written to `appearance.json`; full `PC` defaults table; `types.json`; Sync allow-list (`workspace.json` never syncs)](addenda/2026-10-03-vault-config-split-and-defaults.md)
- [2026-10-03 — **CORRECTION (vault):** hidden-path predicate is a pure dot-segment walk with **no** `configDir` special case (`vault.md` §3 has the rule inverted); `userIgnoreFilters` are **regexes**, not globs, and gate search/graph/property-edits only — not the tree or the link graph](addenda/2026-10-03-vault-hidden-paths-and-userignore.md)
- [2026-10-03 — **NEW (vault):** exact `checkPath` character sets (platform-conditional; Windows-only reserved names and trailing-dot/space), dotfiles have `extension === ""`, attachment basenames sanitised by `/([:#|^\\\r\n]|%%|\[\[|]])/g`, markdown-link encoder set, `createNewFile` ext-strip, linktext-with-`/` bypasses `newFileLocation`](addenda/2026-10-03-vault-path-validation-and-filenames.md)
- [2026-10-03 — **CORRECTION + NEW (vault):** `TFile.saving` is a nesting **counter** not a boolean; `getNewPathAfterRename` replaces control chars with a space; watcher-seen ENOENT gets a **100 ms grace** before deletion; mobile `.trash/` naming **does** preserve the extension (refutes `vault.md` §1/§3); `.trash/` is never read by core (refutes §13 Q7's speculation)](addenda/2026-10-03-vault-adapter-reconcile-and-trash.md)
- [2026-10-03 — **NEW (vault):** frontmatter serialiser pins `lineWidth: 0` / `nullStr: ""` / `aliasDuplicateObjects: false`; `processFrontMatter` deletes the block when the mutator empties it; `insertIntoFile`'s three-way newline rule; `generateMarkdownLink` same-file collapse + empty display text for non-`.md`; orphan-attachment sweep is **two-phase** with a `not-in {md,canvas,base}` filter](addenda/2026-10-03-vault-filemanager-content-rewrites.md)
- [2026-10-03 — **Re-verified (vault):** all four `2026-06-10` path/naming corrections hold unchanged in 1.13.7, re-anchored to the restored corpus; plus a citation map for `vault.md` §4's dead 1.12.7 `Vault.js:*` refs](addenda/2026-10-03-vault-prior-corrections-re-verified.md)
- [2026-10-03 — **CORRECTION + NEW (bases):** the `.base` writer **canonicalises property IDs** — `hQ` strips the `note.` prefix from `order`/`sort`/`groupBy`/`summaries` and shortens `file.file` to `file`, while `properties:` and `data.columnSize` are emitted **raw**, so one file carries two key conventions (`bases.md` §3's schema shows the wrong form); `BasesViewConfig` has **no** `unrecognizedData` (refutes §8's `[CRIT]` "at every level"); `summaries: {}` is emitted when empty but `formulas: {}` is not; `limit: 0` is dropped at parse; a non-array `sort` is silently ignored while a non-array `order` throws. **`not:` is NOR not NAND** (refutes the 2026-04-17 §10); **`html()` returns the `Markdown` value and nothing returns `HTML`** — `labelGlobalMarkdown` has zero code references (§13 Q9 answered); **`duration("5ms")` can never parse** (the unit regex has no `ms` alternative — three dead branches); `date()` takes `YYYYMMDDHHmm`/`YYYYMMDD` and returns `NullValue`; `link("https://…")` returns a `UrlValue`; `tagMatches` is one-directional (§1/§11 backwards); `list.sort()` **has** a comparator and `stddev()` is population (§14's TBDs resolved). Plus the entire per-view-type `data:` key inventory (§3 has none of it), the fenced `base` code-block language with its tab→4-space rewrite and 100 ms `replaceCode` write-back, `![[X.base#View]]` view selection (§13 Q6), six commands (§6 says none), and `QueryController` at `1813-CZ.js:1893` with the refresh gate shown to be a `JSON.stringify` fingerprint (§13 Q2/Q3). **§14's promote-to-`VAULT-FORMAT.md` instruction is blocked until §A/§F are absorbed**](addenda/2026-10-03-bases-base-file-serialization-and-dsl-corrections.md)
- [2026-10-01 — **CORRECTION:** `EphemeralState` persistence + `eState.scroll` semantics are self-contradictory in the corpus (`views.md` says "not persisted" + line number; `workspace.md` embeds it in serialized records)](addenda/2026-10-01-ephemeral-state-persistence-contradiction.md)
- [2026-10-01 — **CORRECTION:** workspace split `direction` names the divider *line*, not the stacking axis (refutes `workspace.md` §2)](addenda/2026-10-01-workspace-split-direction-correction.md)
- [2026-06-10 — Editor timing + coverage corrections (`expandText` debounce; `lP` regex redundancy; live-preview engine mislocated by `editor.md` §14)](addenda/2026-06-10-editor-timing-and-coverage-corrections.md)
- [2026-06-10 — Taxonomy + Bases corrections (`QueryController` does not parse the search DSL; Bases `summaries:` double-handling)](addenda/2026-06-10-taxonomy-and-bases-corrections.md)
- [2026-06-10 — Vault path + naming corrections (`getAvailablePath` starts at ` 1`; UTF-16-unit offsets; `normalizePath` behavior)](addenda/2026-06-10-vault-path-and-naming-corrections.md)
- [2026-06-10 — Workspace serialization corrections (flat window bounds; mobile-drawer node; `pinned` at two levels; `group` sibling of `state`)](addenda/2026-06-10-workspace-serialization-corrections.md)
- [2026-04-19 — File Recovery core plugin: backup store + Version History modal](addenda/2026-04-19-file-recovery-plugin.md)
- [2026-04-19 — "Add file property" hamburger entry into frontmatter editor](addenda/2026-04-19-add-file-property-menu.md)
- [2026-04-19 — Graph internal plugin: "Copy screenshot" menu action + preamble](addenda/2026-04-19-graph-screenshot.md)
- [2026-04-19 — Canvas internal plugin: "Export as image" menu action + preamble](addenda/2026-04-19-canvas-export-as-image.md)
- [2026-04-19 — "Merge entire file with…" modal UX](addenda/2026-04-19-merge-file-modal.md)
- [2026-04-19 — `Platform.shell.showItemInFolder` — "Show in system explorer" menu action](addenda/2026-04-19-show-in-folder.md)
- [2026-04-19 — `app.openWithDefaultApp` — "Open in default app" menu action](addenda/2026-04-19-open-with-default-app.md)
- [2026-04-19 — `fileManager.promptForDeletion` — delete-confirm modal](addenda/2026-04-19-delete-confirm-modal.md)
- [2026-04-19 — `fileManager.promptForFileRename` / `promptForMove` modals](addenda/2026-04-19-rename-move-modals.md)
- [2026-04-19 — Bookmarks core plugin: on-disk format, panel, commands, modal](addenda/2026-04-19-bookmarks-core-plugin.md)
- [2026-04-17 — Bases formula / filter DSL: grammar, operator semantics, function catalog](addenda/2026-04-17-bases-formula-dsl.md)
- [2026-04-15 — Markoff strips footnote-def lines before tree-sitter (offset-shift)](addenda/2026-04-15-markoff-footnote-def-offset-shift.md)
- [2026-04-15 — Daily Notes + Templates JSON schemas](addenda/2026-04-15-daily-notes-templates-schemas.md)
