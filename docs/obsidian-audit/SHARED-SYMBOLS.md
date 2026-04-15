# Shared-symbol reconciliation

Obsidian's JavaScript is minified: most module-internal identifiers are two- to three-character tokens (`Y6`, `sc`, `xT`, …). These short names appear in many Pass 2 domain docs — usually because a consumer grep turned up a name the auditor couldn't define in-tree. This document is the merged cross-reference: for every short symbol cited in two or more domain docs, it records the canonical home domain, the underlying identifier kind, what the symbol does, and which consumers reference it.

The primary source is `leaf-utilities.md §15`, which resolved 20+ symbols during the audit; the tables below fold in additional entries from `metadata.md §15`, `workspace.md §15`, `vault.md §11`, `core.md §15`, `editor.md §15`, `editor-markdown.md §15`, `rendering.md §15`, `bases.md §15`, `plugin.md §15`, `search.md §15`, `ui-bundle.md §15`, `settings.md §15`, and `parsing.md §15`.

Entries marked `unknown` could not be located inside the Pass 2 extraction windows and are logged as follow-ups in the Unresolved section below.

## Resolved short symbols

| Short symbol | Defined in (domain + file:line) | Kind | Purpose | Used by (cross-domain) |
|---|---|---|---|---|
| `ru` | `vault` — app.js ~35442 | function | Hidden-path predicate: returns `true` if any path component starts with `.`; walks path via `iu` | `vault/Vault.js` (fileMap filter) |
| `nu` | `vault` — app.js ~35434 | function | `basename(path)` — last path segment after `/` | `vault/Vault.js`, `BL`, `resolveSubpath` |
| `iu` | `vault` — app.js ~35438 | function | `dirname(path)` — all before last `/` | `vault/Vault.js`, `ru` |
| `su` | `vault` — app.js ~35458 | function | `extension(path)` — lowercased extension after last `.` | `BL`, `MetadataCache`, resolve helpers |
| `au` | `vault` — internal | function | Linktext helper (path/heading splitter) cited by `metadata.md §15` | `metadata.md` link resolution |
| `pu` | `vault` — internal | function | Path-manipulation helper cited by `workspace.md §15` | `workspace` protocol handlers |
| `BL` | `metadata` — app.js ~95373 | function | Linkpath normaliser: strips `.md` extension via `su(nu(path))` | `MetadataCache.unresolvedLinks` keys |
| `VL` | `metadata` — app.js ~95376 | function | Path-length comparator (shortest-path wins) for link disambiguation | `MetadataCache.getFirstLinkpathDest` sort |
| `HL` | `metadata` — app.js ~95381 | function | Set-contains check: does `cache.tags` include any of a given tag set | `MetadataCache` tag intersection |
| `zL` | `metadata` — app.js ~95387 | function | Valid-tag predicate: non-empty, no newlines, ≤ 100 chars or no spaces | Tag validation |
| `$A` | `metadata` — internal | function | Non-empty-string predicate cited by `metadata.md §15` | `MetadataCache` |
| `ub` | `utils` — internal | function | Locale-aware string comparator | `metadata`, `settings`, `bases` (sort) |
| `sc` | `metadata` — app.js ~34630 | class | Multi-map `Map<string, T[]>` — `add/remove/get/keys/contains/count` | `MetadataCache.unresolvedLinks`, backlinks |
| `vb` | `metadata` — app.js ~64083 | class | Serial promise-chain queue — `queue(fn)` appends to a running `Promise.resolve()` chain | `MetadataCache.workQueue` |
| `wb` | `metadata` — app.js ~64184 | class | Generator-backed async queue (`addList/add/remove/clear`), backed by `mb` (FIFO deque) + `bb` (cancellable runnable) | `MetadataCache.linkResolverQueue` |
| `Cb` | `metadata` — internal | class | Runnable wrapper companion to `wb`/`bb` | `MetadataCache` |
| `Mb` | `utils` — internal | function | Adapter-promise barrier used by `TextFileView.save` to drain the serialisation queue | `views/TextFileView.js`, `bases/BasesView.js` |
| `Zw` | `utils/platform` — app.js ~66774 | function | `idb-open(name, version, {upgrade, blocked})` — wraps `indexedDB.open` as a promise | `MetadataCache._preload` |
| `GL` | `utils` — app.js ~96726 | function | Chunked IDB cursor walker: `getAllKeys` then `getAll` in slices of `n`, invokes `cb(keys, values)` per chunk | `MetadataCache._preload` |
| `Sf` | `utils` — app.js ~42917 | function (async) | SHA-256 via `crypto.subtle.digest`, returned as hex via `arrayBufferToHex` | `MetadataCache` content-hash |
| `kf` | `utils` — app.js ~42897 | function | `TextDecoder().decode(new Uint8Array(buffer))` — binary → UTF-8 string | `MetadataCache` |
| `Mf` | `utils` — app.js ~42912 | function | Byte-by-byte `String.fromCharCode` + `btoa` — base64 encoder inside `arrayBufferToBase64` | `arrayBufferToBase64` |
| `oc` | `utils` — internal | function | `escapeRegExp(str)` | `MetadataCache.userIgnoreFilters` |
| `AT` | `utils` — app.js ~79797 | const (regex) | `/[!"#$%&()*+,.:;<=>?@^\`{\|}~\/\[\]\\\r\n]/g` — wide punctuation strip for display headings | `stripHeading` |
| `PT` | `utils` — app.js ~79798 | const (regex) | `/([:#\|^\\\r\n]\|%%\|\[\[\|]])/g` — narrow strip for link-fragment generation | `stripHeadingForLink` |
| `Yx` | `parsing` — app.js ~45995 | const (regex) | `/^---(\r?\n)/g` — opening frontmatter delimiter | `getFrontMatterInfo` |
| `Qx` | `parsing` — app.js ~45996 | const (regex) | `/---(\r?\n\|$)/g` — closing frontmatter delimiter (EOF-tolerant) | `getFrontMatterInfo` |
| `Jx` | `parsing` — app.js ~46043 | function | `Object.prototype.hasOwnProperty` alias | `parseFrontMatterEntry` |
| `xx` | `parsing` — app.js ~45024 | function | `yaml.parse(text, null, {})` from bundled eemeli/yaml v2 | `parseYaml` |
| `yE` / `AS` | `parsing` (bundled `yaml` v2) | class/function | `yaml.Document` type guard and constructor | `stringifyYaml` |
| `vI` | `vault/FileManager` — app.js ~56958 | function | Inner frontmatter splice helper used by `processFrontMatter` | `FileManager.processFrontMatter` |
| `yI` | `vault/FileManager` — app.js ~56998 | function | Ordered-assign merge helper | `FileManager.insertIntoFile` |
| `UL` | `parsing`/`editor` | function | Applies link-rewrite patches to raw markdown text | `MetadataCache.updateInternalLinks` fallback |
| `xT` | `core` (or adjacent) | function | Ref walker — walks `links`/`embeds`/`frontmatterLinks`/`blocks` | `MetadataCache.iterateAllRefs` |
| `qL` | `core` or adjacent | class | `BlockCache` constructor (`blockCache = new qL(app)`) | `MetadataCache` |
| `NL` | `core`/`ui` | function | Widget-type inference for `getAllPropertyInfos` | `MetadataCache` |
| `RL` | `metadata` (unextracted) | class | `MetadataTypeManager` — property/widget registry | `App.metadataTypeManager`, `bases/` |
| `vT`/`wT`/`kT`/`mT`/`gT`/`yc` | `utils` | functions | Persist/hydrate filters and position normalisers for `saveMetaCache`/`_preload` | `MetadataCache` persistence |
| `Ty` | `utils` — `_internal.js:83097` | function | Two-mode word/char matcher (core of `fuzzySearch`) | `search` domain |
| `Iy` | `utils` — `_internal.js:83199` | function | Simple substring-AND scorer for `prepareSimpleSearch` | `search` domain |
| `Ly` | `utils` — `_internal.js:83199` | function | Tokens-all-present finder | `search` domain |
| `wy` | `utils` | function | Range-merger for match ranges | `search` |
| `xy` | `utils` — `_internal.js:83133` | function | Score formula: contiguity > case > span > start > length | `fuzzySearch` |
| `Cy` / `ky` / `Ey` | `utils` — `_internal.js:83109-83113` | const (regex) | Char-class regexes: whitespace / ASCII+punctuation / CJK+Tibetan+Japanese | `prepareQuery`, `Ty` |
| `sy` | `network` — app.js ~62496 | class | `RequestUrlError extends Error { status: number; headers: Record<string,string> }` | `requestUrl` error path |
| `B0` | `vault/` or `core/` | class | `recentFileTracker` — load/serialize/onFileOpen/getLastOpenFiles/getRecentFiles/addRecentFile | `Workspace.recentFileTracker`, `.obsidian/workspace.json` `lastOpenFiles` |
| `R0` | `core/` | function | Pane-type normaliser mapping `"tab"`/`"split"`/`"window"` to `getLeaf` mode | Workspace protocol handlers |
| `aJ` | `views` — app.js ~153862 | class | `EmbedRegistry` — extension→embed factory registry; image/audio/video/pdf/markdown pre-registered | `App.embedRegistry` |
| `sJ` | `core` / `views` | class | Embed-render-child factory (`sJ.load({app, linktext, sourcePath, containerEl, …})`) | `editor/markdown` preview |
| `JZ` | `parsing`/`rendering` | function | Embed-depth guard: walks `.internal-embed` ancestors, caps nesting | `MarkdownPreviewRenderer` |
| `Y6` | `core`/plugin (unextracted) | class | `Commands` — `addCommand`/`removeCommand`/`findCommand`/`executeCommandById` | `App.commands` |
| `zb` | `core`/plugin (unextracted) | class | `HotkeyManager` — reads `.obsidian/hotkeys.json`; binds by command id | `App.hotkeyManager` |
| `xP` | `plugin` (unextracted) | class | `DragManager` | `App.dragManager` |
| `Ib` | `plugin` (unextracted) | class | `customCss` — loads `.obsidian/snippets/*.css` | `App.customCss` |
| `Eb` | `utils`/`ui` (unextracted) | class | Quit-tasks collector (`isEmpty()`, `promise()`) | `App.registerQuitHook` |
| `$0` | `plugin` (unextracted) | class | Community-plugin loader | `App.plugins` |
| `o2` | `plugin` (unextracted) | class | `InternalPlugins` — 31 built-in plugin registry | `App.internalPlugins` |
| `Nee` | `ui` (unextracted) | class | `StatusBar` | `App.statusBar` |
| `wte` | `ui` (unextracted) | class | DOM layout wrapper (`appContainerEl`, `workspaceEl`, `statusBarEl`, `horizontalMainContainerEl`) | `App.dom` |
| `t4` | `platform` (unextracted) | class | `shareReceiver` (mobile) | `App.shareReceiver` |
| `CA` | `platform` (unextracted) | class | `CLI` | `App.cli` |
| `vte` | `settings` (unextracted) | class | `SettingModal` | `App.setting` |
| `Q6` | `editor` (unextracted) | class | `FoldManager` | `App.foldManager` |
| `Rg` | `core/Scope.js` | class | `Scope` subclass with dynamic child-delegate; `new Rg(parent, cb)` forwards to `cb()`-returned scope | `Workspace.scope` |
| `pD` | `ui` (unextracted) | class | Load-progress UI singleton (`pD.instance`) | Boot |
| `gm` | i18n bundle | object | Translation tree (`gm.plugins.bases`, `gm.interface`, `gm.nouns`, `gm.dialogue`) | Every user-facing string |
| `Fb` | `platform/Keymap.js` | function | Hotkey-binding constructor `(modifiers, key) => KeyBinding` | Every command registration |
| `KZ` / `xZ` / `MZ` / `XZ` / `h0` | internal plugins | class | `PdfView` / `ImageView` / `AudioView` / `VideoView` / `ReleaseNotesView` | `ViewRegistry` built-ins |
| `tD` / `eD` / `nD` | `views` (extracted into `vault/Vault.js`) | class | Empty-view placeholder / deferred-load stub / unknown-view fallback | `WorkspaceLeaf._empty`, `setViewState` |
| `HX` / `VX` | `bases` / `views/TextFileView.js` | class / const | `BasesView` class / `"bases"` view-type string | `.base` registration |
| `hP` | `rendering` (bundled Turndown) | object | Turndown service singleton | `htmlToMarkdown` |
| `EL` / `SL` | `rendering` (bundled DOMPurify) | object | DOMPurify singleton / allowlist config | `sanitizeHTMLToDom` |
| `bz` / `mz` / `uz` / `gz` | `rendering` (library bootstrap) | deferred | MathJax / Mermaid / PDF.js / Prism lazy-load promises | `loadMathJax`/`loadMermaid`/`loadPdfJs`/`loadPrism` |
| `Cz` / `Ez` / `Sz` | `rendering/finishRenderMath.js` | deferred/timeout/function | Typeset batch deferred / debounce id / typeset-resolve worker | `finishRenderMath` |
| `mW` | `rendering/RenderContext.js` | object | Bidi-isolate decoration table `{rtl, ltr, auto}` (CM6 `Decoration.mark` factories) | editor bidi extensions |
| `PopoverState` | `ui/popups` (defined in `_internal.js`) | enum | `{ Hidden=0, Showing, Shown, Hiding }` | `HoverPopover.state` — **reconstructed from use-sites** (mis-extracted `PopoverState.js` was Publish code) |
| `db` | `ui/popups/SuggestModal.js` (module-static) | WeakMap | `WeakMap<Window, HTMLDivElement>` — per-window `.notice-container` | `Notice` stacking |
| `XQ` / `$Q` | `ui/popups/HoverPopover.js` (module-static) | set | Pending / visible `HoverPopover` sets | `HoverPopover` global poll |
| `nX` / `iX` / `eX` / `tX` | `ui/popups/HoverPopover.js` (module-static) | function/timeout | 500 ms poll function / interval id / click-outside / mousemove handlers | `HoverPopover` lifecycle |
| `ob` / `ab` / `nb` / `ib` | `ui/popups/SuggestModal.js`, `Modal.js` | class | List scaffolding / grouped list / confirm modal / prompt modal | `SuggestModal` internals |
| `Y0` | `settings/PluginSettingTab.js` | class | Internal-plugin settings tab (sibling of `PluginSettingTab` for core plugins) | `app.setting` |
| `Ym` / `Um` / `Xm` / `Qm` / `Zm` / `$m` / `Wm` / `Jm` / `Km` / `_m` | `ui/icons` (bundle internals) | tables/functions | Alias map / Lucide paths / custom `addIcon` store / legacy built-ins / cache / SVG attr presets | `getIcon` resolution chain |
| `Nv` | `utils` | function | Placement helper (`{gap, preventOverlap, horizontalAlignment}`) | `PopoverSuggest.reposition` |
| `Rv` | `utils` | function | Auto-destroy watcher (`Rv(el, 500ms, cb)`) | `PopoverSuggest.setAutoDestroy` |
| `Cc` | `utils` | function | Idle-callback / `requestIdleCallback` wrapper | `Workspace.requestUpdateLayout` |
| `Vm` / `_g` | `utils` | function | Swipe gesture recogniser | `Workspace.onSwipe` |
| `Gc` | `utils` | function | Drag-to-reorder helper (Sortable) | `WorkspaceRibbon.reorder` |
| `cc` | `utils` | function | `cc(16)` — random 16-char id generator | `WorkspaceItem.id`, per-render `docId` |
| `Sv` | `utils` (likely) | function | `setTooltip(el, text, opts)` — attribute-setting companion to `displayTooltip` | Menu items, links |
| `LI` | `utils` | function | Bidi-direction inference | `EditorSuggest.updatePosition` |
| `Lc` | `utils` | function | Modifier-key-or-default-source predicate (gates `hover-link` emission) | `RenderContext.renderFileLink` |
| `Yc` / `Qc` | `utils` | function | Safe-URL-scheme predicate / URL validator | `RenderContext.renderExternalLink` |
| `YC` | `utils` | function | Wikilink predicate | `bases/` |
| `nE` | `utils` | function | Linktext → display-name (basename / `#`-stripper) | `RenderContext.renderFileLink` |
| `Ic` | `utils` | function | Internal-link styling applicator (adds `cm-iso` or equivalent class) | `RenderContext.renderFileLink` |
| `hu` / `ou` | `utils` | function | Path → shortname / filename-safe-fy | `bases/` |
| `zW` | `bases`-adjacent `utils` | function | Lazy `Value`-subclass coercer for heterogeneous values | `ListValue.lazyEvaluator`, `ObjectValue.fromFrontMatter` |
| `xW` | `utils` | function | Tag normaliser (strip `#`, validate) | `metadata`, `bases` |
| `HV` / `zV` | `utils` (date) | function | `YYYY-MM-DD` / `HH:MM:SS` formatters | `bases/DateValue.renderTo` |
| `FX` | `utils` — `_internal.js:461203` | function | Three-way text merge (`diffMain → patchMake → patchApply`) | `TextFileView.loadFileInternal` |
| `UT` / `WT` / `GT` / `KT` | `vault`/`utils` | const (regex) | Illegal-filename chars / reserved names / linktext validation / path-safety | `Vault.checkPath`, `EditableFileView` rename validation |
| `AC` | `settings` (outside domain) | set | Appearance-keys allow-list partitioning `app.json` vs `appearance.json` at `saveConfig` time | `Vault.saveConfig` |
| `PC` | `settings` (outside domain) | object | Vault-config defaults table returned by `vault.getConfig(key)` on missing keys | `Vault.getConfig` fallback |
| `AD` / `PD` / `UD` | `workspace/WorkspaceSplit.js` (module-local) | const | Minimum pane size (200 px) / sidedock animation descriptor / mobile-drawer duration (200 ms) | `WorkspaceSidedock` |
| `F0` / `N0` | `workspace/WorkspaceFloating.js` (module-local) | const (string) | `"workspace.json"` / `"workspace-mobile.json"` filenames | `Workspace.saveLayout` |
| `qD` | `workspace/WorkspaceLeaf.js` (module-local) | class | `LeafHistory` — per-leaf back/forward stacks (cap 20) | `WorkspaceLeaf.history` |
| `WD` | `workspace/WorkspaceLeaf.js` (module-local) | class | `MobileDrawer` — mobile sidedock replacement | Mobile layout |
| `cD` | `utils` | function | Boot-timing helper | `App` init |
| `DK` / `RK` / `JK` / `PX` / `IY` | `editor`-adjacent (Bases formula) | class/function | Formula container / error type / language-support / extension wrapper / default extensions | Bases filter/formula parsing |
| `ZK` | `editor`-adjacent | class | Aggregate-formula context (`new ZK(app, values)`) | `bases/` aggregate formulas |
| `QW` | `plugin/internal-plugins/bases/` (not extracted) | object | Bases formula globals/instance-methods registry | `Plugin.registerGlobalFunc` / `registerInstanceFunc` |
| `aq` | `bases` / `search/QueryController.js` | class | Bases entry queue | `QueryController` |
| `iY` / `nY` / `xY` / `SY` | `parsing`-adjacent / `utils` | function | `PropertyId` parse / build / case-fold / serialise | Bases |
| `OY` / `LY` | `utils`-adjacent | function | Property-type label / property-id → widget-key | Bases property renderer |
| `kY` | `utils`-adjacent | function | Groupable-property predicate | Bases groupBy |
| `PY` | `utils`-adjacent | function | Unique-name suffixer | Bases view naming |
| `MY` / `TY` / `FY` | `utils`-adjacent | function | Filter-parser / view-config-builder / new-item-defaults | Bases |
| `k$` | `search/QueryController.js` | class | Nested `Events`-derived emitter used as `QueryController.events` | `BasesView.on('view-changed')` |
| `lz` | `editor`/`ui` | class | In-document search dialog (`new lz(scope, renderer, containerEl, onClose)`) | `MarkdownView.currentMode.showSearch` |
| `Kx` / `$x` / `Zx` | `parsing`/`rendering` | function | Tokenise / extract-frontmatter / render-to-html pipeline | `MarkdownRenderer.render` |
| `cz` | `parsing` | function | Synchronous section-splitter used by `parseSync` | `MarkdownPreviewRenderer` |
| `NT` / `rT` | `parsing` | function | Subpath content-range extractor / frontmatter-CSS-class extractor | `editor/markdown` |
| `Ux.globalOptions` | `parsing` (module singleton) | object | Global parser options (`breaks`, etc.); `strictLineBreaks` toggle mutates this | `MarkdownPreviewRenderer` worker |
| `JD` | `plugin` (constant, undefined in extracted tree) | const (string) | Plugin styles filename — conventionally `"styles.css"` | `Plugin.loadCSS` |
| `Wz` | `editor-markdown` | Worker (module singleton) | `worker.js` instance for progressive-parse | `MarkdownPreviewRenderer` |

## Unresolved (follow-up for controller / Pass-3 extraction out-of-tree)

The following are cited in one or more Pass 2 docs but their definitions live outside the extracted `obsidian/` subtree (either in `_internal.js`, `app.js` bundle, or internal-plugin source). They are flagged for a future pass that grep the full bundle:

- **`Y6`** (`App.commands` class) — cited in `core.md §13 Q3` as needing extraction.
- **`zb`** (`App.hotkeyManager` class) — cited in `core.md §13 Q4`.
- **`o2` / `$0`** (internal/community plugin loaders) — cited in `core.md §13 Q5`; also names the 31 internal-plugin ctors (`P8`, `dJ`, `g9`, `bJ`, `V6`, `i8`, `r7`, `dee`, `q8`, `k7`, `f7`, `v8`, `wee`, `Z8`, `u8`, `k9`, `b8`, `l3`, `U8`, `See`, `w9`, `p7`, `Pee`, `S9`, `a4`, `Iee`, `O8`, `f9`, `uee`, `Z4`, `M2`).
- **`QW`** — Bases formula globals registry; cited in `plugin.md §13 Q4`.
- **`app.cli`** (`CA` class) — cited in `plugin.md §13 Q5`.
- **`NL`** (widget-type inference) — cited in `metadata.md §13 Q1`.
- **`Eb`** (quit-tasks collector) — cited in `core.md §13 Q1`.
- **`wte`** (DOM layout wrapper) — cited in `core.md §13 Q2`.
- **`EL` / `SL`** (DOMPurify config) — cited in `rendering.md §13 Q1`. Security-critical.
- **`hP`** (Turndown rule set) — cited in `rendering.md §13 Q2`. Web-clipper-compat-critical.
- **`DK` / `RK` / `JK` / `PX`** (Bases formula/filter DSL parser) — cited in `bases.md §13 Q1`.
- **`QueryController`** (Bases view controller — the one actually in `obsidian/search/`, but its incremental-refresh strategy references helpers outside the audit) — cited in `bases.md §13 Q2-Q3`. Note the Pass 1 error: the Pass 1 taxonomy attributed `QueryController` to the search-panel DSL parser; the real owner of the DSL is the internal `global-search` plugin (see `search.md`, de-minifier artifact note).
- **Internal-plugin manifest IDs** — the 31 constructors enumerated in `core.md §7` need `.manifest.id` extracted.
- **`AC` / `PC` contents** — appearance-keys allow-list and vault-config defaults; cited in `vault.md §13 Q1-Q2` and `settings.md §13 Q4`.
- **Icon-alias table `Ym`** — cited in `ui-bundle.md §13 Q4`.
- **Regex sets `UT` / `WT` / `GT` / `KT`** — illegal filename / reserved-name / linktext / path-safety regexes cited in `vault.md §13 Q3`.
- **`Rg` scope-delegate exact semantics** — largely answered in `core.md §13 Q8` (forward-OR-fallback confirmed) but the predicate-filter edge cases need a live trace.
