# `obsidian/editor/markdown` — MarkdownView, the three-mode state machine, and the preview-render pipeline

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/editor/markdown/`
**File count:** 6
**Files:** `MarkdownPreviewRenderer.js`, `MarkdownPreviewSection.js`, `MarkdownPreviewView.js`, `MarkdownRenderChild.js`, `MarkdownRenderer.js`, `MarkdownView.js`

**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> **What it does:** The marquee feature. `MarkdownView` is the `TextFileView` subclass for `.md` files; it owns the editor, the preview pane, the toggle between "source / live-preview / preview" modes, the rendered markdown sections (`MarkdownPreviewSection`), and section recycling. `MarkdownPreviewRenderer` does progressive async section-by-section rendering with a worker. `MarkdownRenderer` is the public static API plugins call to render markdown into a DOM element. `MarkdownRenderChild` is the lifecycle wrapper for plugin-rendered fragments (so they unload when the section is recycled).
> **Key exports / primitives:**
> - `MarkdownView` — view registered for `.md`; manages mode switching, search-in-document, scroll sync, breadcrumbs, sort buttons.
> - `MarkdownPreviewView` — preview-only view (used inside hover popovers, embeds).
> - `MarkdownPreviewRenderer` — section-recycling, progressive-render engine; tracks `sections`, `asyncSections`, `recycledSections`.
> - `MarkdownPreviewSection` — one HTML chunk + heading-collapse state + frontmatter flag + height cache.
> - `MarkdownRenderer` — static `render(app, markdown, el, sourcePath, component)`; what plugins call.
> - `MarkdownRenderChild` — `Component` subclass for plugin DOM children (unload-on-recycle).

**De-minifier artifact note:** `MarkdownPreviewSection.js` and `MarkdownRenderChild.js` are byte-identical except their API-symbol comment; both extract `app.js:112290-112346` which contains **both** classes. Canonical: `MarkdownPreviewSection.js`. `MarkdownView.js` (`app.js:175263-177910`) contains five adjacent classes: `R6` nav-header helper, Backlinks internal plugin `V6`/`H6`/`z6`/`q6` (out of scope — belongs to `plugin/internal-plugins/backlinks`), `j6` MarkdownEditView (`type="source"`), `MarkdownView` at `:1582`, PDF-export modal `G6`. External symbols used but not defined here listed in §15.

---

## 1. Public API surface

### `MarkdownView`

- **Kind:** class, extends `TextFileView` (`MarkdownView.js:2314` — `})(TextFileView)`).
- **Exported as:** `MarkdownView` (the file-header comment); `VIEW_TYPE` static is `"markdown"` (`MarkdownView.js:1581` + `:2311`).
- **Signature:** constructor `new MarkdownView(leaf)`. The `WorkspaceLeaf` factory in `ViewRegistry` is `(leaf) => new MarkdownView(leaf)`.
- **Purpose:** The marquee view. Owns a set of **mode objects** (`modes: Record<string, Mode>`), the editor `Scope`, the inline-title element, the frontmatter `metadataEditor`, the scroll position, the backlinks side-panel toggle, and PDF-print plumbing. Delegates all data I/O to the currently-selected mode via `getViewData()`/`setViewData(e,clear)`/`clear()` (the `TextFileView` contract) which in turn calls `mode.get()` / `mode.set(data, clear)` / `mode.clear()`.
- **Lifecycle:** constructed by `WorkspaceLeaf.openFile` (after `ViewRegistry` looks up `.md` → `"markdown"` → factory). `onload` subscribes to `workspace.quick-preview`, `workspace.resize`, `workspace.css-change`, and `vault.config-changed`. `onUnloadFile` saves fold info, clears the metadata editor, clears CSS classes. `onClose` awaits super and calls `editMode.destroy()`. Instance is destroyed when the leaf detaches.
- **Mixes in:** `Component` + `Events` (via `View` → `ItemView` → `FileView` → `EditableFileView` → `TextFileView` → `MarkdownView`).

**Method groups (33 prototype methods, grouped by role):**

- **Mode state machine.** `getMode(): "source"|"preview"` (`:1869`); `registerMode(mode)` stores by `mode.type` (`:1866`); `setMode(mode)` (`:1910`) is the async primitive: early-returns if already-current; `await save()` if leaving source; captures outgoing `getFoldInfo()` → `app.foldManager.save`; hides outgoing, `show`/`set(data, false)`/`onResize` on incoming; restores scroll via `setEphemeralState({scroll})`; `applyFoldInfo(r)`; updates metadata-editor collapse + mode-button icon + `containerEl.data-mode` attr; returns `true` on change. `toggleMode()` (`:1875`) flips `state.mode` via `leaf.setViewState({focus:true})`. `onSwitchView(e)` (`:1975`) is the mode-button click: `Mod+click` splits the leaf with opposite mode; else in-place swap. `updateButtons()` swaps icon (`lucide-book-open` source → `lucide-edit-3` preview).
- **State serialisation.** `getState()` → `{mode, source: editMode.sourceMode, backlinks, backlinkOpts?}` (`:1764`). `setState(state, eState)` applies mode via `setMode`, then toggles `source` via `editMode.toggleSource()` if different, then backlinks; sets `eState.layout=true` on change so workspace schedules a layout-change event (`:1774`).
- **Ephemeral state.** `getEphemeralState()` adds `scroll`. `setEphemeralState(e)` handles `rename: "start"|"end"` (inline-title focus only if scroll < 0.5), resolves `state.subpath` via `resolveSubpath(metadataCache.getFileCache(file), subpath)` into `{line, startLoc, endLoc}`, delegates to `currentMode.setEphemeralState` (`:1824`).
- **Scroll sync.** `syncScroll()` captures `currentMode.getScroll()`, emits `workspace.trigger("markdown-scroll", this)`, calls `syncState()` for linked-pane-group peers (`:2090`). `receiveSyncState(other)` — different file reopens leaf; same file applies ephemeral state.
- **Data I/O (TextFileView overrides).** `getViewData()` → `currentMode.get()` (preview's `.get()` returns `renderer.text`). `setViewData(data, clear)` — when `clear=true`, nulls scroll, calls `.set(data, true)` on every mode (so offscreen modes stay current), reloads fold info from `foldManager`. Unconditionally calls `loadFrontmatter(data)` (`:2231`). `clear()` iterates all modes, nulls scroll.
- **Change propagation.** `onInternalDataChange()` — fires from editor updates; if differs from `this.data`, updates `data`, calls `workspace.onQuickPreview(file, data)` (per-keystroke cross-pane sync), reloads frontmatter (`:2221`). `onExternalDataChange(file, data)` — subscribed to `quick-preview`; if other pane edited same file, `setData(data, false)` syncs. `saveFrontmatter(obj)` — edits YAML via `kI(viewData, obj)` helper, full round-trip through `setViewData + onInternalDataChange + save` (`:1675`).
- **Frontmatter.** `loadFrontmatter(text)` — diffs `rawFrontmatter` against `getFrontMatterInfo(text).frontmatter`; if changed, `parseYaml`, rejects arrays, calls `editMode.setCssClass(rT(parsed))` and `metadataEditor.synchronize(parsed)` (`:2200`). `canShowProperties()` — properties shown iff not raw-source mode AND `propertiesInDocument === "visible"` (`:2261`).
- **Editor passthrough.** `editor` getter → `editMode.editor`; `undo()`/`redo()` → the editor; `getSelection()` → `currentMode.getSelection()`; `triggerClickableToken(token, newLeaf?)` routes `internal-link`/`external-link`/`tag` clicks (100 ms timeout for internal-link, `window.open` for external, global-search for tag).
- **Clipboard & inline-title.** Cut/copy/paste route to `metadataEditor` when it has focus or clipboard contains `obsidian/properties` MIME. `inlineTitleEl` is `contentEditable` with focus/blur/input/paste/keydown handlers from `EditableFileView`.
- **Backlinks panel.** `canToggleBacklinks()` — true if preview mode or live-preview edit mode. `toggleBacklinks()` flips `state.backlinks`. `updateShowBacklinks()` creates/destroys the inline backlinks child (`z6`) under `backlinksEl`.
- **Misc.** `onResize`/`onCssChange`/`updateOptions` all delegate to modes. `showSearch(replace?)` → `currentMode.showSearch(replace)`. `printToPdf()` → `new G6(app, file).open()`. `shiftFocusBefore/After` move focus through inline-title ↔ frontmatter ↔ body.

**Ctrl+Wheel zoom** — passive `wheel` listener on `containerEl`: when `ctrlKey` and no click in the last 500 ms, adjusts `vault.baseFontSize` clamped `[10, 30]` (`:1637-1660`).

### `MarkdownPreviewView`

- **Kind:** class, extends `MarkdownRenderer`. Registered in `MarkdownView.modes` under `type = "preview"`.
- **Constructor:** `new MarkdownPreviewView(view)`. Builds `contentEl.createDiv("markdown-reading-view")`, attaches a `MarkdownPreviewRenderer` (via super), sets `addBottomPadding = true`, runs 7 config-update methods (`updateReadableLineLength`, `updateFoldHeading`, `updateFoldIndent`, `updateIndentGuide`, `updateRTL`, `updatePropertiesInDocument`, `updateStrictLineBreaks`), adds header (for inline-title + metadata editor) and footer (for embedded backlinks). Binds `onFoldChange` debounced 500 ms to `view.onMarkdownFold()`.
- **Mode-specific DOM handlers.** `keydown`: `Mod+A` intercepted with a Notice (explains `Mod+C` copy-all); `Mod+C` with empty selection → `Ec(get())` clipboards the full markdown source. `pointerdown`/`pointerend` with `touch` pointerType: double-tap within 300 ms / <30 px travel → `view.toggleMode()`. `contextmenu` (not touch) emits `workspace.trigger("markdown-viewport-menu", menu, view, "preview", "gutter")`.
- **Public methods:** `get`/`set`/`clear` / `rerender(fully?)` (re-runs postprocessors; `fully` wipes rendered HTML first) / `getScroll`/`applyScroll` / `getFoldInfo`/`applyFoldInfo` / `onResize` / `showSearch()` attaches `lz` search widget / `foldAll`/`unfoldAll` (respecting `foldHeading`/`foldIndent` config) / `getSelection()` → `window.getSelection().toString()`.
- **`setEphemeralState(e)`** (`:299-361`) handles `focusMetadata`, `focus` (preview-el focus with `preventScroll`), `scroll`/`line` (with optional highlight flash), `propertyMatches`, `match.content/matches`.
- **Config-change reactivity.** `onConfigChanged(key)` switches on 7 keys (listed above) — each calls a named update. `strictLineBreaks` sets `Ux.globalOptions.breaks = !strict` (a **global** parser option) and `rerender()`s.

### `MarkdownPreviewRenderer`

- **Kind:** plain class (NOT a `Component`). Owned by each `MarkdownRenderer` instance.
- **Constructor:** `new MarkdownPreviewRenderer(owner, eventBus, containerEl, workerPath, listenForInsertion=true)` (`MarkdownPreviewRenderer.js:12`). Creates the DOM: `previewEl` (`.markdown-preview-view.markdown-rendered`), `sizerEl` (`.markdown-preview-sizer.markdown-preview-section`), `pusherEl` (1×0.1 px placeholder used to offset the rendered content via `margin-bottom` — the virtual-scroll anchor).
- **Load-bearing fields:** `sections: MarkdownPreviewSection[]` (full ordered list incl. header/footer); `asyncSections` (post-processors pending); `recycledSections` (pool matched by **exact HTML string equality**); `text` / `lastText` (guards redundant parse); `frontmatter` (last parsed, JSON-compared for diff); `queued`/`parsing`/`scrolling` flags; `header`/`footer` pinned chrome sections; `renderExtra = 1` / `renderExtraMinPx = 500` (virtual-scroll overscan); `addBottomPadding` (+viewport/2 padding).

**Methods (38 total, grouped by role):**

- **Text set / render queue.** `set(text)` → `queueRender()`. `rerender(clearHtml)` clears non-chrome `.el` and re-queues. `queueRender()` schedules `onRender` via `Pc(fn, delay)` (idle-callback-ish): `0 ms` if not parsing, else `200 ms`.
- **Parse pipeline.** `parseSync()` → `cz(text)` → `{sections, frontmatter}`. `parseAsync()` uses a **worker** (lazy singleton `Wz` via `Hz(workerPath)`; posts `{parseSections, options: Ux.globalOptions}`). **Async fires iff `_z.asyncParse && text.length >= 10240`.** Worker returns sectioned HTML; render on main thread.
- **Recycle algorithm** — `parseFinish(text, {sections, frontmatter})`:
  1. Snapshot existing sections into a multimap `html → MarkdownPreviewSection[]` (skipping header/footer).
  2. For each new section: claim unused match by HTML-string equality, else `new MarkdownPreviewSection(html)`. Copy `lines`, `start`, `end`, `level`.
  3. **Frontmatter diff** via `JSON.stringify` equality; if changed, every section with `usesFrontMatter=true` is force-re-rendered (`rendered=false`, `.el.empty()`).
  4. Apply/remove CSS-class frontmatter key (`rT(frontmatter)`) on `previewEl`.
  5. Unshift header, push footer, commit `this.sections = newList`. Filter `asyncSections` to only those still present; `recycledSections` receives prior sections with `rendered && !used`.
- **Render loop — `onRender()` (`:374`), the progressive pipeline:**
  1. Parse if text changed.
  2. For each unrendered section: `section.render()` (sanitizes HTML via `sanitizeHTMLToDom`, appends, adds `el-<tagName>` class); run every `recyclers` static; inject `heading-collapse-indicator` chrome (`jz`) and `list-collapse-indicator` chrome (`Uz`); inject `list-bullet` spans. Run `owner.postProcess(section, promises, frontmatter)` — async ones go into `asyncSections` + `Promise.all(...).then(remove+resetCompute+queueRender)`. **Time-budgeted: every 10 sections, if `performance.now()-start > 5 ms`, break and `queueRender()`.**
  3. Assemble DOM; measure heights via `measureSection`.
  4. `updateVirtualDisplay(scrollTop?)` — viewport `[scrollTop - overscan, scrollTop + height + overscan]`; walks sections accumulating cumulative top; marks sections intersecting viewport as visible `[M, S]`; **extends window to include any Selection range** (so selection doesn't tear across virtualisation); detaches sections outside; sets `pusherEl.marginBottom = startOffset` and `sizerEl.minHeight = totalHeight - 1 px`. A 10000-line note ends up with ~20 sections in the DOM at a time.
- **Fold state.** `foldAllHeadings`/`unfoldAllHeadings`/`foldAllLists`/`unfoldAllLists` — toggle `headingCollapsed` / `.is-collapsed` per `li`. `getFoldInfo(): {folds: [{from, to}], lines}` scans sections; `applyFoldInfo(info)` replays. Frontmatter-collapse is `{from: 0, to: 0}`.
- **Scroll (visual-line float).** `getScroll()` returns `floor(line) + fraction_within_line` interpolated over `li`; `null` while parsing. `applyScroll(visualLine, {highlight?, center?})` inverse; `applyScrollDelayed(line, opts, cb)` retries via `onRendered`; `applyScrollSection(section)` jumps to top. `showSection(section)` walks backwards un-collapsing ancestor-headings (`[[Note#SubHeading]]` nav into collapsed parent).
- **Section lookup.** `getSectionInfo(el): {text, lineStart, lineEnd} | null` (plugins use this to know which lines a code-block occupies). `belongsToMe(el)` climbs but **stops at `.markdown-preview-view` or `.bases-view`** — so hover-popovers and Bases views don't steal events from the outer preview.
- **DOM event delegation** — static `registerDomEvents(sizerEl, eventBus, detachFilter?)` wires `a.internal-link` click/auxclick/dragstart/contextmenu/mouseover to `eventBus.on*`; `a.footnote-link` mouseover synthesises `linktext = "#[^" + footnoteId + "]"`; `a.external-link`/`.tag`; `img,video` click/contextmenu on mobile → pinch-zoom image viewer.
- **Checkbox bridge.** `onCheckboxClick(e, checkboxEl)` extracts `data-line`, adds `section.start.line`, forwards to `owner.onCheckboxClick(e, el, absLine)` (algorithm in Section 8).
- **Heading-collapse click.** `onHeadingCollapseClick` → `section.setCollapsed(!headingCollapsed)` + clear selection + queueRender + `owner.onFoldChange()`.
- **Search-highlight rendering.** `renderHighlights(startIdx, endIdx)` uses text-walker helpers to project highlight ranges to DOM `Range.getClientRects()`; creates absolute-positioned `div`s inside a `.search-highlight` overlay in `sizerEl`.
- **Headers/footers.** `addHeader`/`removeHeader`/`addFooter`/`removeFooter` manage pinned `.mod-header`/`.mod-footer`/`.mod-ui` sections (preview uses header for inline-title+metadata; footer for backlinks).
- **Static plugin registries** — `postProcessors: Function[]` (sort-ordered), `codeBlockPostProcessors: Record<lang, fn>` (throws on duplicate lang), `recyclers: Function[]`. `createCodeBlockPostProcessor(lang, fn)` generates a post-processor that finds `code.language-<lang>`, wraps in `div.block-language-<lang>`, provides `ctx.replaceCode(newSrc)` that rewrites source at the code-block's line range.

### `MarkdownPreviewSection`

- **Kind:** plain class. One per parsed block.
- **Fields:** `html` (immutable post-parse; the recycle key), `el` (lazily populated by `render()`), `rendered`, `computed` (height cached), `height`, `lines`, `used` (recycle marker), `highlightRanges`, `level` (0 for non-heading, 1–6), `headingCollapsed`, `shown` (hidden by ancestor-heading collapse), `usesFrontMatter`, `start`/`end` (`{line, col, offset}`).
- **Methods:** `render()` runs `sanitizeHTMLToDom(html)` + appends + adds `el-<tagName>` class; `resetCompute()` invalidates height; `setCollapsed(b)` toggles `headingCollapsed` + `.is-collapsed`.
- **Recycling key:** two sections are "the same" iff HTML byte-equal. A one-char paragraph edit re-runs all that paragraph's post-processors; unrelated sections reuse DOM verbatim.

### `MarkdownRenderer`

- **Kind:** class, extends `MarkdownRenderChild` (a `Component`). Public static API is `MarkdownRenderer.render(app, markdown, el, sourcePath, component)`.
- **Constructor:** `new MarkdownRenderer(app, containerEl, listenForInsertion=true)`. Creates `docId = cc(16)` (per-render-child plugin state keying) and an internal `MarkdownPreviewRenderer(workerPath="worker.js")`.
- **Lifecycle (Component):** `onload()` subscribes to `vault.modify`, `vault.delete`, `metadataCache.changed` — all three fire `onFileChange` → `requestUpdateLinks` (debounced 500 ms) which walks every rendered section and `resolveLinks`-updates `.internal-link` classes.
- **Instance methods:** `onCheckboxClick(e, el, line)` (§8), `postProcess(section, promises, frontmatter)` (delegates to static), `resolveLinks(el)` (via `uq(app, el, path, hrefResolver)`), `onScroll`/`onFoldChange`/`onRenderComplete` (no-op hooks).
- **Hooks subclasses override:** `onInternalLinkClick`/`onInternalLinkRightClick`/`onInternalLinkDrag`/`onInternalLinkMouseover`/`onExternalLinkClick`/`onExternalLinkRightClick`/`onTagClick` — default implementations in `MarkdownEditView` / `MarkdownPreviewView` call `workspace.openLinkText` / `workspace.trigger("hover-link", …)` / `workspace.trigger("file-menu", …)`.
- **Statics:**
  - `toggleCheckbox(text, line): {text, char} | null` — **pure function** that locates `[.]` on `line`, flips `" "` ↔ `"x"`, returns new full-source + new char.
  - `postProcess(app, ctx)` — runs `postProcessors` array over `ctx.el`; finds unloaded `.internal-embed` and delegates each to `sJ.load({app, linktext, sourcePath, containerEl, displayMode, showInline, depth})` (`sJ` = `EmbedRegistry`); pushes each `loadFile()` promise into `ctx.promises`. `depth` comes from `JZ(containerEl)` — walks ancestors counting `.internal-embed` for the **infinite-recursion guard**.
  - `render(app, markdown, el, sourcePath, component)` — parses via `Kx`, extracts frontmatter via `$x`, renders body via `Zx`, sanitises via `sanitizeHTMLToDom`, runs post-processors, resolves embeds; returns `Promise<void>`. **Logs `console.error` with plugin-stack detection if `component` is missing** (memory-leak guard).
  - `renderMarkdown(markdown, el, sourcePath, component)` — deprecated alias calling `render(null, ...)`.

### `MarkdownRenderChild`

- **Kind:** class, extends `Component`. Constructor stores `containerEl`; that's it.
- **Purpose:** lifecycle wrapper plugins return from post-processors. When `MarkdownPreviewRenderer` recycles (or view closes), descendants get `unload()`. Pattern: plugin code-block processor does `ctx.addChild(new MyWidget(el))` where `MyWidget extends MarkdownRenderChild` with `onunload() { cleanup }`.

---

## 2. Data structures

### `ViewState.state` for `MarkdownView`

```typescript
{
  file: string;            // vault-relative, '/'-separated
  mode: "source" | "preview";
  source?: boolean;        // within "source": true = raw source, false = live-preview
  backlinks?: boolean;     // per-document backlinks panel
  backlinkOpts?: { collapseAll?, extraContext?, sortOrder?, isShowingSearch?, searchQuery?, backlinkCollapsed?, unlinkedCollapsed? };
}
```

Three modes encoded as `{mode: "preview"} | {mode: "source", source: true} | {mode: "source", source: false /* live-preview */}`.

### `EphemeralState` keys consumed by `MarkdownView`

`scroll?: number` (visual-line float), `line?: number`, `startLoc`/`endLoc: {line, col}`, `subpath?: string` (resolved to startLoc/endLoc), `cursor?: {from, to?}`, `focus`/`focusOnMobile`/`focusMetadata: boolean`, `rename: "start"|"end"`, `match: {content, matches: [number, number][]}`, `propertyMatches: [{key, …}]`.

### `FoldInfo` (persisted via `app.foldManager`)

```typescript
{
  folds: Array<{ from: number; to: number; }>;  // {0,0} = frontmatter collapsed
  lines: number;  // total line count; applyFoldInfo bails if mismatch — line-count-changing external edits silently drop folds
}
```

### `MarkdownPostProcessorContext`

```typescript
{
  docId: string;                          // cc(16) per render
  sourcePath: string; frontmatter: unknown;
  promises: Promise<void>[];
  addChild(child: MarkdownRenderChild): void;
  getSectionInfo(el): {text, lineStart, lineEnd} | null;
  replace(newText: string, rerenderOwnSection?: boolean): void;
  containerEl: HTMLElement; el: HTMLElement;   // sizerEl / section el
  displayMode?: boolean;
}
```

`ctx.replace(newText, rerender)` rewrites source at the section's offset range, writes back via `owner.edit(fullNewText)`; if `rerender=true` AND the new section renders as one block, in-place HTML swap skips a full re-parse.

### `toggleCheckbox` return — `{text: string; char: string;} | null`.

---

## 3. On-disk contracts

No direct filesystem writes. All persistence via:

1. **`.md` file** — `TextFileView.save()` → `vault.modify(file, text)` (2000 ms debounce). Mode switch `await`s save before leaving source.
2. **Fold state** — `app.foldManager.save(file, foldInfo)` / `.load(file)`. `foldManager` lives in `core/App` or `internal-plugins/folds`; persistence details not in this domain.
3. **`pdfExportSettings`** — `vault.setConfig("pdfExportSettings", …)` at `MarkdownView.js:2406`. Schema:
   ```typescript
   { includeName: boolean;                                  // prepend h1 with basename
     pageSize: "A3" | "A4" | "A5" | "Legal" | "Letter" | "Tabloid";
     landscape: boolean;
     margin: "0" | "1" | "2";                               // "0"=default / "1"=none / "2"=minimal
     downscalePercent: number; }                            // 10–100
   ```
4. **`baseFontSize`** — Ctrl+Wheel handler writes via `vault.setConfig("baseFontSize", n)`, clamp [10, 30]; gated on `baseFontSizeAction` config; 500 ms debounce.
5. **`backlinkInDocument`** (read only) — mirror of the backlinks internal-plugin config; read in constructor at `:1634`.

No vault I/O outside `.md` + `vault.setConfig`. Worker-parse loads `worker.js` from the app bundle (not vault).

---

## 4. Events emitted

All emissions go through `workspace.trigger(...)`, not on this domain's own objects.

| Event | Payload | When | Emitter |
|---|---|---|---|
| `markdown-scroll` | `(view)` | `syncScroll()` on every scroll-sync | `MarkdownView.js:2094` |
| `markdown-viewport-menu` | `(menu, view, "source"\|"preview", "gutter")` | Right-click source gutter or preview empty viewport | `:1296` (source), `MarkdownPreviewView.js:195` (preview) |
| `quick-preview` | `(file, data)` | `onInternalDataChange()` via `workspace.onQuickPreview(file, data)` on every edit | `MarkdownView.js:2225` |
| `hover-link` | `{event, source: "editor"\|"preview", hoverParent, targetEl, linktext, sourcePath?}` | `mouseover` on `a.internal-link`/`a.footnote-link`/`.tag` | delegated from `MarkdownPreviewRenderer.js:1400-1423` via `owner.onInternalLinkMouseover` |
| `editor-menu` | `(menu, editor, view)` | Not emitted here — comes from sibling `editor/` `Editor` | — |
| `post-processor-change` | `()` | Not emitted here; consumed | — |

`registerPostProcessor(fn, sortOrder)` does NOT itself fire `post-processor-change`; that happens at `workspace.registerEditorExtension`-style call elsewhere.

### Owner-method callbacks (not events)

`owner.onCheckboxClick(e, el, line)` / `owner.onFoldChange()` / `owner.onScroll()` / `owner.onRenderComplete()` plus the `owner.on{Internal,External}Link{Click,RightClick,Drag,Mouseover}` / `onTagClick` / `onFootnoteLinkClick` hooks — direct method calls, not `Events`-based. Subclasses override.

---

## 5. Events consumed

| Listener | Subscribes to | Why |
|---|---|---|
| `MarkdownView.js:1724` | `workspace.quick-preview` → `onExternalDataChange` | Cross-pane same-file sync without disk save |
| `:1731, :1734, :1737` | `workspace.resize`, `workspace.css-change`, `vault.config-changed` | Resize delegate to mode; css-change → editor re-measure; config-change → only `"spellcheck"` acted on |
| `MarkdownPreviewView.js:252` | `workspace.post-processor-change` | Full `rerender(true)` on plugin (un)register |
| `MarkdownPreviewView.js:258` | `vault.config-changed` | 7 render-affecting keys (`readableLineLength`, `foldHeading`, `foldIndent`, `showIndentGuide`, `rightToLeft`, `propertiesInDocument`, `strictLineBreaks`) |
| `MarkdownRenderer.js:28-30` | `vault.modify`/`vault.delete`/`metadataCache.changed` | All three fire `requestUpdateLinks` (500 ms debounced link-class re-resolve) |

MarkdownView **does not** subscribe directly to `metadataCache` or to `workspace.active-leaf-change`/`file-open`. Link-awareness comes through `MarkdownRenderer.onload`. Backlinks child (`z6`) subscribes separately to `metadataCache.resolved`. Incoming file navigation driven by `FileView.loadFile` (see `views.md`).

---

## 6. Commands registered

No commands registered in this domain directly. Mode-toggle is exposed via:

1. **Mode button** — `addAction("lucide-book-open"|"lucide-edit-3", tooltip, onSwitchView)` on the view's action bar (`:1621`); tooltip includes `Mod+click → open in new tab`.
2. **Pane-menu items** (via `onPaneMenu`): "Toggle reading view" (section `pane`), "Toggle source mode" (`pane`, only shown when in source), "Add property" (`action`, disabled if not `canShowProperties`), "Find"/"Replace" (`find`, Replace disabled in preview), "Export to PDF" (`action`, gated on `Platform.canExportPdf`).
3. `editor-menu` contributions from sibling `Editor` add "Rename heading"/"Rename block ID".

Canonical Obsidian command IDs are **registered in the internal-markdown-plugin domain, not here**: candidates referenced by plugins and docs: `markdown:toggle-preview`, `editor:toggle-source`, `workspace:export-pdf`.

**Corbomite status:** `view_editing_mode`/`view_reading_mode` only (two modes, `MainWindow.cpp:272,280`). Recommended future three-mode command IDs: `editor_toggle_source` / `editor_toggle_live_preview` / `editor_toggle_reading_view`.

---

## 7. Registries owned

### `MarkdownPreviewRenderer.postProcessors: Function[]` (static; plugin-facing)

Plugin-supplied `(el, ctx) => void | Promise<void>` callbacks, sort-order-ranked. Populated by `registerPostProcessor(fn, sortOrder)` (plugin-wrapped as `Plugin.registerMarkdownPostProcessor`). Read by `MarkdownRenderer.postProcess(app, ctx)` walking in sort order; promise returns pushed into `ctx.promises`. In-memory only. Plugin (un)register fires `post-processor-change` on workspace (from the plugin-manager, not here); every live `MarkdownPreviewView` rerenders. Also in §10.

### `MarkdownPreviewRenderer.codeBlockPostProcessors: Record<lang, fn>` (static; plugin-facing)

One processor per fenced-code language; `registerCodeBlockPostProcessor(lang, fn)` **throws on duplicate**. Read at render time via `createCodeBlockPostProcessor`. Also in §10.

### `MarkdownPreviewRenderer.recyclers: Function[]` (static; internal)

`(sectionEl, recycledPool) => void`. Called for every freshly-rendered section; hook for transplanting per-section state (layout/scroll/etc.) across parse cycles. `registerRecycler(fn)` / `unregisterRecycler(fn)`. Not plugin-facing.

### `MarkdownView.modes: Record<"source"|"preview", Mode>` (per-view; internal)

Populated by `registerMode(mode)` in constructor; consumed by `setMode`/`setState`/`toggleMode`/`setViewData` (the `clear=true` branch fans out to every mode).

---

## 8. Invariants

1. `MarkdownView.VIEW_TYPE === "markdown"`. Callers must `await leaf.loadIfDeferred()` before accessing `view.editor`/`getViewData()` (deferred-view placeholder).
2. `getMode()` returns `"source"` or `"preview"`. **Live-preview is NOT a separate mode**: it's `{mode: "source", source: false}` (`editMode.sourceMode === false`). `source: true` = raw source (`**bold**` visible); `source: false` = live-preview. **Single most important compat convention.**
3. `setMode(new)` early-returns `false` if target is already current (toggle-idempotence).
4. **Before leaving `"source"`, `setMode` awaits `save()`.** Switching source → preview on a dirty doc triggers a disk write.
5. `loadFrontmatter` is called on every `setViewData`. Invalid YAML → `metadataEditor.synchronize(null)`; file still displays.
6. `canShowProperties()` is **false** for raw-source mode and **true** elsewhere when `propertiesInDocument === "visible"`.
7. **Section recycling is HTML-string-equality based.** One-char paragraph edits force all that paragraph's post-processors to re-run; unrelated sections reuse DOM. Stable headings preserve fold state.
8. `asyncParse` fires iff `text.length >= 10240 && _z.asyncParse`. Worker singleton `Wz` lazy-loaded. Worker error → sync fallback.
9. `updateVirtualDisplay` always includes sections containing any Selection-range endpoint (no selection tear across virtualisation).
10. `owner.onRenderComplete()` fires exactly once per full render cycle, after async post-processors resolve.
11. `fold.lines !== text.lineCount` silently drops foldInfo on apply (external edits changing line count reset folds).
12. **`MarkdownRenderer.onCheckboxClick` preserves non-standard task characters** (`:34-73`):
    1. `e.preventDefault()`; `navigator.vibrate(100)` on mobile.
    2. `getSectionContainer(checkboxEl)` → section; bail if none.
    3. `MarkdownRenderer.toggleCheckbox(fullText, absoluteLine)` — pure helper: finds the line, searches for `/\[.\]/`, flips `" "` ↔ `"x"` (any other char treated as "checked" → reverts to `" "`). Returns `{text, char}`.
    4. **Rewrite rendered HTML** without re-parse, regex:
       ```regex
       /<li class="task-list-item( is-checked)?" data-task="(.)" data-line="(\d+)"><input class="task-list-item-checkbox" type="checkbox"( checked)? data-line="(\d+)">/g
       ```
       Replacer acts only when both `data-line` values equal `String(clickedLineOffset)`; rewrites `is-checked` class, `data-task="<newChar>"`, `checked` attribute — preserves line numbers and any non-standard task marker.
    5. `setTimeout(0)` mutates live DOM (`checkboxEl.checked`, parent-`<li>.data-task`, `.is-checked` class) to avoid the contentEditable glitch frame.
    6. `this.edit(newFullSource)` → `renderer.set(text)` + `view.onInternalDataChange()` + `await view.save()`. **Disk write on every click.**
13. `onInternalDataChange()` only emits `quick-preview` when `this.data !== currentMode.get()` (no-op on stale calls).
14. `setEphemeralState({scroll: float})` with fractional value interpolates within-section (virtual-scroll); integer = line-top position.
15. `MarkdownRenderer.render(...)` without `Component` logs `console.error` with plugin-stack detection (passive warning, not throw); plugins predating the param continue to work but leak event handlers on reload.

---

## 9. Observable user features

- Toggle reading view ↔ editing view from action-bar icon, tab context menu, or `Ctrl+E` hotkey. `Mod+click` on the toggle-icon opens the opposite mode in a new split.
- Toggle live-preview ↔ raw source (only available in editing view) via tab menu "Toggle source mode". Live-preview hides syntax and draws widgets; raw source shows `**bold**` etc.
- Click any checkbox in reading view to flip `[ ]`/`[x]`; disk write on every click; non-standard markers `[?]`/`[/]` preserved.
- Heading-collapse triangle on any heading in reading view; list-item collapse triangle on parent list items. State persists via `foldInfo`.
- Find in document (tab menu or shortcut); Find+Replace available only in editing view.
- Export current file to PDF via tab menu "Export to PDF" modal (page size / landscape / margin / downscale slider); settings persist in `vault.pdfExportSettings`.
- Embedded backlinks panel at the bottom of the document (toggled per-leaf).
- `Ctrl+Wheel` zoom (10–30 px base font), guarded on `baseFontSizeAction` config.
- Scroll-sync across linked-pane-group peers via `markdown-scroll` event.
- Quick-preview cross-pane: typing in an edit pane instantly refreshes a sister preview pane via per-keystroke-debounced `quick-preview` event (no save required).
- Click internal-link → open target (`Mod+click` new tab / `Shift+Mod+click` split / `Mod+Alt+click` window); hover-for-500 ms → preview popover.
- Footnote-link click → scroll-to-target in same view with flash highlight.
- Progressive section-by-section render on notes >10 KB (worker parse + ~20 sections in DOM at a time); scrollbar geometry preserved via `pusherEl` + `sizerEl.minHeight`.
- Mobile: checkbox haptic vibrate (100 ms); double-tap (touch) to toggle mode; pinch-zoom image viewer on embedded images in preview.
- Inline-title rename at top of document; `Enter`/`Esc`/`Tab`/`ArrowDown` navigate.
- Properties editor at top of document (editable YAML frontmatter key-value rows); toggled by `propertiesInDocument` config.
- `Mod+A` in reading view shows Notice about `Mod+C` copy-all; `Mod+C` with empty selection copies entire markdown source.
- Right-click on gutter/empty viewport → `markdown-viewport-menu` event fires (plugins + built-in Readable-line-length / Line-numbers / Inline-title toggles).

---

## 10. Extension surfaces exposed

| Surface | Plugin-facing verb | Consumer | Plugin supplies |
|---|---|---|---|
| Markdown post-processor | `Plugin.registerMarkdownPostProcessor(fn, sortOrder)` | `MarkdownRenderer.js:160, 234` (`postProcessors` loop in `postProcess`) | `(el, ctx) => void \| Promise<void>` |
| Code-block processor | `Plugin.registerMarkdownCodeBlockProcessor(lang, fn)` → `createCodeBlockPostProcessor` + `registerPostProcessor(fn, -100)` | `MarkdownPreviewRenderer.js:1198` | `(source, containerEl, ctx & {replaceCode}) => Promise<void>` |
| `MarkdownRenderChild` subclass | Returned from a post-processor via `ctx.addChild(new X(el))` | `MarkdownRenderChild.js` + `MarkdownPreviewRenderer.js:1153-1166` (`cleanupParentComponents`) | `class extends MarkdownRenderChild { onload, onunload }` |
| `MarkdownRenderer.render(app, text, el, sourcePath, component)` | Direct static call | `MarkdownRenderer.js:185` | Plugins render ad-hoc markdown with full post-processor chain |
| `hover-link` event | `workspace.on("hover-link", cb)` + `Plugin.registerHoverLinkSource(id, info)` | `MarkdownPreviewRenderer.registerDomEvents` → `owner.onInternalLinkMouseover` | Source providers; `"preview"` + `"editor"` pre-registered |
| `markdown-viewport-menu` | `workspace.on("markdown-viewport-menu", cb)` | `MarkdownView.js:1296` / `MarkdownPreviewView.js:195` | `(menu, view, mode, "gutter")` |
| `markdown-scroll` | `workspace.on("markdown-scroll", cb)` | `MarkdownView.js:2094` per syncScroll | `(view)` — "user is reading line N" |
| `quick-preview` | `workspace.on("quick-preview", cb)` | `MarkdownView.js:2225` via `workspace.onQuickPreview` | `(file, data)` — per-keystroke |
| `post-processor-change` | `workspace.on("post-processor-change", cb)` | Subscribed at `MarkdownPreviewView.js:252` | `()` — fires after (un)register |

---

## 11. Corbomite mapping

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `MarkdownView` | `src/editor/NoteEditorWidget` | Partial | No `TextFileView`-style debounce+3-way-merge yet (see `views.md`). Not a `WorkspaceLeaf::ItemView`. |
| **Three-mode state machine** | `NoteEditorWidget::ViewMode { Editing, Reading }` (`src/editor/NoteEditorWidget.h:21`) | Missing | **Two modes only.** No live-preview. `docs/superpowers/plans/2026-04-03-editor-three-modes.md` is the planning doc. |
| `{mode: "preview"}` | `ViewMode::Reading` backed by `libs/markoff/ReadingView` (build artefacts present; source currently lives outside `libs/markoff/src/` — verify) | Partial | Confirm `ReadingView` is the canonical widget. |
| `{mode: "source", source: true}` raw | `ViewMode::Editing` + `MarkdownHighlighter` | Partial | Corbomite renders `**bold**` with colourised syntax literally. |
| `{mode: "source", source: false}` live-preview | **Missing** | Missing | Per-block widget swap in `MarkdownTextItem`: leave-block replaces text with rendered sub-widget; enter-block reveals source. |
| Progressive section pipeline (`MarkdownPreviewRenderer`) | `libs/core/src/MarkdownRenderEngine` (canvas-card only; whole-doc) | Missing | Section-recycle + virtual-scroll + worker-parse absent. Large-note scaling risk. |
| `MarkdownPreviewSection` (HTML-equality recycle) | N/A | Missing | Candidate: per-block `MarkdownTextItem` hashable on source + frontmatter. |
| `MarkdownRenderChild` lifecycle | N/A (no plugin system) | Missing | Future `QObject`-rooted lifecycle wrapper. |
| `MarkdownRenderer.render(...)` static | `MarkdownRenderEngine::render` | Partial | No post-processor chain / embed resolution / link-resolution callback. |
| Checkbox round-trip + `toggleCheckbox` helper | `libs/markoff/src/CheckboxTextObject.cpp` (Apr 14) | Partial | Audit source-rewrite path; `data-task="?"`-preserving regex is the spec to implement verbatim. |
| `![[Note]]` embed (`EmbedRegistry sJ`) | N/A | Missing | Inline vs block embed classes, depth guard (`JZ`, limit ~8), self-embed prevention — all missing. |
| Heading/list collapse | N/A | Missing | `QGraphicsItem` hide/show possible. |
| Virtual section detach | N/A (QGraphicsView culls off-screen but no size-cache) | Missing | |
| Find/replace in document | Partial via `QTextDocument::find` | Partial | No regex/case/whole-word UI. |
| Properties/frontmatter editor in document | N/A | Missing | |
| Fold persistence | N/A | Missing | Consider source-hash or block-ID keyed to survive line-count changes. |
| Mode actions | `view_editing_mode`/`view_reading_mode` in `MainWindow.cpp:272,280` | Partial | Not Obsidian-command-ID-compatible; missing the third mode. Candidate IDs: `editor_toggle_source`, `editor_toggle_live_preview`, `editor_toggle_reading_view`. |
| `Ctrl+E` toggle | `MainWindow.cpp:283` bound to `view_reading_mode` | Present | Matches Obsidian. |
| `Ctrl+Wheel` zoom | N/A | Missing | Easy: `NoteEditorWidget::wheelEvent`. |
| `strictLineBreaks` | Markoff parser setting | Unknown | Audit `libs/markoff-parser/`. |
| `pdfExportSettings` dialog | N/A | Missing | `QPrinter` + `QPrintDialog` + `ReadingView` output. |
| `markdown-scroll`/`markdown-viewport-menu`/`quick-preview` events | N/A | Missing | Postpone until plugin API exists. |

---

## 12. Markoff gap confirmations / discoveries

**Confirmed Pass-1 signals from `01-markoff-gaps.md`:**

- **Three modes (source/live-preview/reading).** CONFIRMED. Encoding: `{mode: "source", source: true|false}` + `{mode: "preview"}`. Transition: `setMode(new)` → `await save()` on leave-source → capture-fold → hide outgoing → `show/set/onResize` on incoming → restore scroll → apply fold → update buttons + `data-mode`. Corbomite has two modes only. Split-region QGraphicsView is the right architecture; critical: cursor-reveal granularity is per-**block** (paragraph, list-item, heading, code-block, math-block), not per-line.
- **Progressive section render + recycle.** CONFIRMED. Recycle key is **exact HTML-string equality**. Async-parse threshold 10240 bytes. 5 ms / 10-section time budget. Worker returns already-sectioned HTML. Corbomite whole-doc only — **missing**. Target architecture: `libs/markoff-parser/` section-array API + `MarkdownTextItem` per-section hashable + `SceneCoordinator` recycle pool.
- **Heading collapse.** CONFIRMED. Per-section `headingCollapsed` + `level`. `updateShownSections` walks linearly: sections with level > current-collapse-level → `shown=false`. Corbomite missing entirely.
- **Checkbox round-trip.** CONFIRMED. Regex and algorithm in §8 item 12. `data-task="?"` preservation is central to Tasks/Todo plugins. Corbomite has `CheckboxTextObject` (Apr 14); audit the regex mirror.
- **`![[Note]]` inline embed.** CONFIRMED. Classes `markdown-embed` + `inline-embed` + `markdown-embed-title` + `markdown-embed-content`. Per-embed `MarkdownPreviewView` (mini-renderer). Subpath resolution `resolveSubpath(cache, subpath) + NT(text, cache, subpath).content`. Depth guard `JZ(containerEl)` passed as `sJ.load({depth})`. Corbomite `EmbedRegistry` equivalent **missing**.
- **Find/replace in document.** CONFIRMED. `currentMode.showSearch(replace?)`; replace disabled in preview mode.
- **Hover-link preview.** CONFIRMED. `mouseover` on `a.internal-link` → `onInternalLinkMouseover` → `workspace.trigger("hover-link", {event, source: "preview"|"editor", hoverParent, targetEl, linktext, sourcePath})`. Footnote mouseover synthesises `linktext = "#[^" + footnoteId + "]"`.
- **Bidi/RTL.** Partial — only the `.rtl` class; per-inline `cm-iso` is an `editor/` concern.
- **MathJax/Mermaid.** Out of scope (in `rendering/`).

**Newly discovered gaps (Pass 2):**

- **Section recycling by HTML-string equality — not AST hash.** Any whitespace-only change to a paragraph's surrounding text rewrites the paragraph's HTML and forces a full post-processor re-run (including Dataview queries, MathJax typeset, Mermaid render). Corbomite's `MarkdownTextItem` should key on rendered-HTML or AST-hash similarly.
- **Frontmatter-diff re-render.** Any change to frontmatter triggers re-render of every section where `usesFrontMatter === true` (Dataview, Templater that depends on `{{frontmatter.*}}`). This is a two-level invalidation — source hash AND frontmatter hash.
- **Per-keystroke `quick-preview` emission.** `onInternalDataChange` fires on every edit (via `editor.updateListener` + 10 ms debounce on the `requestOnInternalDataChange`). A linked preview pane re-renders ~100 times while typing a paragraph. Corbomite's same-file-in-two-leaves case (if supported) must match this behaviour or live-preview will feel laggy.
- **Mode-switch triggers save.** `setMode` always calls `await this.save()` before leaving `"source"`. Mode-switch in a dirty document causes a synchronous disk write. Corbomite must match this: switching `ViewMode::Editing` → `Reading` on a dirty note should save.
- **`MarkdownRenderer.render` without `Component` leaks.** Plugins that render ad-hoc markdown must pass a `Component` for lifecycle. Corbomite's future plugin API needs the same contract — any async render whose result outlives its caller must bind to a parent object for cleanup.
- **Heading-collapse affects the virtual-scroll anchor.** `showSection(section)` walks *backwards* through sections expanding any ancestor-heading that's collapsed until the target section is visible. On a subpath-link click (`[[Note#Sub-heading]]`), this auto-reveals the target even if the user had collapsed its parent. Corbomite must mirror: clicking a link that resolves to a collapsed target needs to un-collapse first.
- **Fold info invalidates on line-count change.** External edits that add/remove lines silently drop all folds. Corbomite's fold persistence (when added) should use block-IDs or source-hashes instead of line numbers to survive external edits.
- **Ctrl+A in reading view shows a Notice, not a select-all.** Users accidentally hit `Mod+A` expecting select-all; Obsidian intercepts and tells them to use `Mod+C`-all-source instead. Corbomite's reading view should either support select-all natively or match this notification.
- **Touch double-tap toggles mode.** Obsidian wires a 300 ms / 30 px threshold double-tap in preview mode to `view.toggleMode()`. Corbomite is desktop-first so this is low priority, but note for future mobile.
- **`strictLineBreaks` toggle force-rerenders.** Changing the config key triggers `Ux.globalOptions.breaks = !strict` and `this.rerender()`. The breaks setting is **global** (on the shared parser options object), not per-view. Corbomite's markdown-parser settings are per-engine instance today; if global, must match.
- **Search-highlight rendering uses `Range.getClientRects`.** Obsidian draws highlight boxes as absolute-positioned `div`s atop the rendered DOM; positions are computed from DOM ranges. Corbomite's `QGraphicsView` approach can use `QGraphicsRectItem` overlays, but needs a DOM-range-equivalent to locate match spans within `MarkdownTextItem`.
- **Scroll position is a **visual-line float**, not pixel offset.** `getScroll()` returns e.g. `42.73` meaning "line 42, 73% of the way through". `applyScroll` interpolates using per-section heights and `li`-element offsets. Pixel-based scroll survives font-size/zoom changes; line-based scroll survives reflow. Obsidian chose line-based. Corbomite should match so `.obsidian/workspace.json`'s stored scroll state round-trips on version upgrade.
- **Selection is preserved across virtual-scroll.** `updateVirtualDisplay` extends its section window to include any selected range. Without this, selecting across a section boundary and scrolling would tear the selection.
- **`worker.js` parser protocol.** The worker handles `{parseSections: text, options: Ux.globalOptions}` messages and returns `{sections: [{html, level, pos: {start, end}}], frontmatter: unknown}`. Loaded lazily on first big note. `new Worker("worker.js")` is relative to the app bundle. Corbomite's `libs/markoff-parser/` would need a thread-pool or `QThread` worker if it goes async.
- **`docId` for render-child state.** Every render creates a `cc(16)` id; plugins can use it to persist state per-render-pass. Corbomite's plugin API should expose an equivalent.
- **Print-export rewrites links.** `MarkdownView.printToPdf` removes `href` from every `.internal-link` before printing (so Ctrl+click-to-open doesn't accidentally trigger during printer preview). Corbomite's PDF export should do the same.

**Appended to `01-markoff-gaps.md` under `## Pass 2 additions — editor-markdown`.**

---

## 13. Open questions

1. `strictLineBreaks` mutates `Ux.globalOptions.breaks` — is `Ux` a module-local singleton? If so, changing it in one view affects every preview. Confirm in `parsing/`.
2. `MarkdownRenderer.render` — is the public param order `(app, markdown, el, sourcePath, component)` per Obsidian TS `.d.ts`? Minified source is `render(e, t, n, i, r)`.
3. The worker singleton `Wz` is shared app-wide. If 20 preview panes need parsing simultaneously, does the worker serialise? Confirm the preview-pipeline queue behaviour.
4. `MarkdownPreviewSection.highlightRanges` — mutated by external search widgets or only by the renderer's own search? Trace through `search.updateQuery` in `editor/`.
5. Embed-depth threshold: `JZ(containerEl)` counts `.internal-embed` ancestors. What's the numeric limit, and how does `sJ.load` fail when exceeded — placeholder or silent skip? Confirm in the embed-registry domain.
6. `foldManager` API surface — lives in `core/App` or `internal-plugins/folds`? Storage format?
7. The `tO` metadata/properties editor — own domain audit, or inside `internal-plugins/properties`?
8. Does source-mode right-click **on actual text** (not gutter) emit `editor-menu` rather than `markdown-viewport-menu`? (Likely yes via `Editor`'s own context menu in sibling `editor/` domain — confirm.)

---

## 14. Recommended Pass 3 synthesis input

1. **`FEATURE-MATRIX.md`: Three-mode state machine semantics.** The `{mode, source}` encoding and the per-block cursor-in-reveal contract are the single largest "Markoff missing" item. Document the state diagram (3 states, 4 transitions), the fold/scroll/selection persistence across transitions, and the save-on-leave-source invariant. Corbomite currently has a 2-mode state; this is the highest-leverage gap to plan.
2. **`FEATURE-MATRIX.md`: Progressive section-recycle pipeline.** The HTML-string-equality recycle key, the 10240-byte async-parse threshold, the 5 ms/10-section time budget, the virtual-scroll + selection-preservation machinery, the async-post-processor promise pool. Corbomite's `MarkdownRenderEngine` is whole-document; for notes > 10 KB this will not scale. `libs/markoff-parser/` + `SceneCoordinator` architecture.
3. **`FEATURE-MATRIX.md`: Checkbox round-trip regex + `toggleCheckbox` pure helper.** The exact regex (`/<li class="task-list-item( is-checked)?" data-task="(.)" data-line="(\d+)"><input class="task-list-item-checkbox" type="checkbox"( checked)? data-line="(\d+)">/g`), the `data-task="?"` preservation contract, and the `{text, char}` return value of the pure helper. Corbomite's `CheckboxTextObject` needs this verbatim for task-plugin compatibility.
4. **`GAP-ANALYSIS.md`: 15-item new-Markoff-gap cluster from Section 12.** Feed the bullets directly.

---

## 15. Cross-domain references

| Other domain | Reference type | Brief description |
|---|---|---|
| `views` | parent | `MarkdownView extends TextFileView`: inherits 2000 ms save-debounce, 3-way-merge, `getViewData/setViewData/clear` contract. |
| `workspace` | emits+consumes | Emits `markdown-scroll`, `markdown-viewport-menu`, `quick-preview`, `hover-link`. Subscribes to `quick-preview`, `resize`, `css-change`, `post-processor-change`. `activeEditor` setter rejects MarkdownView (see `workspace.md`). |
| `vault` | consumes+writes | Subscribes to `modify`/`delete`/`config-changed`. Writes via `TextFileView.save` + `setConfig("pdfExportSettings")` / `setConfig("baseFontSize")`. |
| `metadata` | consumer | `MarkdownRenderer` subscribes to `metadataCache.changed`. `loadFrontmatter` uses `parseYaml`; subpath nav uses `metadataCache.getFileCache(file) + resolveSubpath`. Backlinks child subscribes to `metadataCache.resolved`. |
| `editor` | sibling | `MarkdownView.editor` passthrough to the CM6-backed `Editor`. `j6` / MarkdownEditView extends `aZ` (base in `editor/` domain). |
| `rendering` | dependency | `sanitizeHTMLToDom`, `Kx`/`$x`/`Zx` render chain, MathJax/Mermaid/Prism loaders via post-processors. |
| `parsing` | dependency | `parseYaml`, `getFrontMatterInfo`, `cz` (section-splitter), `NT` (subpath content range). |
| `plugin` | consumer | `registerMarkdownPostProcessor`/`registerMarkdownCodeBlockProcessor`/`registerHoverLinkSource` flow into this domain. |
| `ui/popups` | dependency | `HoverPopover` created on this domain's `hover-link` event. |
| `ui/menu` | dependency | `Menu.forEvent(...).addSections([...])` for context menus. |
| `plugin/internal-plugins/backlinks` | co-located | `V6`/`H6`/`z6`/`q6` classes physically live in `MarkdownView.js` (de-minifier artifact) but belong to that plugin. |

### Short-symbol references (others of interest)

| Short symbol | Defined in | Used here for |
|---|---|---|
| `aZ` | `editor` (base MarkdownEditView) | `j6` extends it; carries `cm`/`sizerEl`/`editor`/`editorEl`/`onUpdate`/`getDynamicExtensions`/`search`/`handleScroll` |
| `sJ` | `core/App.embedRegistry` (`App.js` assigns `embedRegistry = new aJ()`) | `sJ.load({app, linktext, sourcePath, containerEl, …})` — embed render-child factory |
| `JZ` | `parsing`/`rendering` | Embed-depth guard (walks `.internal-embed` ancestors) |
| `lz` | `editor`/`ui` | In-document search dialog `new lz(scope, renderer, containerEl, onClose)` |
| `Kx`/`$x`/`Zx` | `parsing` | tokenise / extract-frontmatter / render-to-html pipeline |
| `cz` | `parsing` | Synchronous section-splitter (used by `parseSync`) |
| `sanitizeHTMLToDom` | `rendering/sanitizeHTMLToDom.js` | DOMPurify wrapper |
| `tO`/`eO` | `plugin/internal-plugins/properties` (likely) | Properties editor / view lookup |
| `nb` | `ui/popups/Modal.js` | Base class for `G6` PDF modal |
| `Y0` | `settings/PluginSettingTab.js` | Base class for `q6` settings tab |
| `foldManager` | `core/App.foldManager` | Fold-state persistence |
| `resolveSubpath` | `metadata` | Subpath → CachedMetadataLocation |
| `getFrontMatterInfo`/`parseYaml`/`rT`/`NT`/`Ux.globalOptions` | `parsing` | Frontmatter/YAML helpers; `rT`=frontmatter-CSS-class extract; `NT`=subpath content range; `Ux.globalOptions.breaks` is the **global** parser line-break flag |
| `cc` | `utils` | Random-ID (used for `docId`) |
| `Pc`/`Hz`/`Bz`/`Wz` | `utils`/`metadata` | Deferred scheduler; worker loader/wrapper/singleton |
| `Dv`/`$z`/`OA` | `utils`/`editor` | Line-to-offset / codeEl-text-extractor / offset→`{line, ch}` |
| `uq` | `rendering/RenderContext.js` | Link-resolution walker |
| `KL`/`jz`/`Uz` | `rendering`/`utils` | Heading/list collapse-indicator helpers |
| `VA`/`foldEffect`/`unfoldEffect` | `editor` (CM) | Transaction-contains-effect predicate + CM state effects |
| `Cg`/`Fl`/`Bl`/`Ec` | `utils`/`ui` | Touch-gesture helpers / clipboard copy |
| `Av`/`Cv`/`mg`/`vg`/`IX`/`LX` | `views/EditableFileView.js` | Inline-title focus/validation |
| `gm` | `core` (i18n) | Localised strings |
| `Scope`/`Menu`/`Keymap`/`Platform`/`Component`/`TFile` | `core/App` / `ui/menu` | Base classes and ambient types |
