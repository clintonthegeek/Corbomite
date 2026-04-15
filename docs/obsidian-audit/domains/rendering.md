# `obsidian/rendering` — markdown→HTML, lazy renderers, sanitiser

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/rendering/`
**File count:** 11
**Files:** `displayTooltip.js`, `finishRenderMath.js`, `htmlToMarkdown.js`, `loadMathJax.js`, `loadMermaid.js`, `loadPdfJs.js`, `loadPrism.js`, `RenderContext.js`, `renderMath.js`, `renderResults.js`, `sanitizeHTMLToDom.js`.

**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> Rendering primitives shared across preview, embeds, hover popovers, search-result snippets, callouts, and Bases cells. `RenderContext` is the central object that knows how to render an internal link, a tag, a date, an image, a file embed; passed into every `Value.renderTo`. The `load*` family lazy-loads heavyweight engines (MathJax, Mermaid, PDF.js, Prism syntax-highlighting). `htmlToMarkdown` (Turndown) reverses paste-from-browser. `sanitizeHTMLToDom` is the DOMPurify wrapper for any plugin-supplied HTML.

**De-minifier artifact note:** All eleven files have unique `md5sum` hashes — there are no extracted duplicates here. The file-line-range comments are intact and tell a clear story: ten of the eleven files are tiny "public API symbol" extracts (8 to 90 lines) covering one function each, and `RenderContext.js` is the single substantive module at 625 lines, declared `// source: app.js lines 123910-124529`. The four `load*.js` files (`loadMathJax`, `loadMermaid`, `loadPdfJs`, `loadPrism`) are *lazy-loader shims* — each is a 12-line `await singleton.promise` body that returns the global library handle (`MathJax`, `mermaid`, `window.pdfjsLib`, `Prism`). The actual library bootstrap (the resolver of `bz`/`mz`/`uz`/`gz` deferred promises) lives in another extraction window outside this directory, so this domain documents *the public-facing wait-for-loader API*, not the loader lifecycle. Similarly, `htmlToMarkdown.js` and `sanitizeHTMLToDom.js` delegate to in-bundle Turndown (`hP`) and DOMPurify (`EL`/`SL`) singletons that are configured elsewhere — Pass 2 cannot inspect the rule lists or allowlist from inside the assigned directory; flagged in OPEN QUESTIONS. The 619-line `RenderContext.js` extraction window also contains the foundation `Value`/`PrimitiveValue`/`StringValue`/`NumberValue`/`BooleanValue`/`ListValue`/`NullValue` typed-value class chain (lines 154–625). Those classes are the *base* of the Bases-domain typed-value system and are extended by `bases/StringValue.js` etc.; the de-minifier put them in `rendering/` because they implement `renderTo(el, ctx)` and so logically belong with `RenderContext`. They are documented here only insofar as they form the *render-target contract* — full type-system semantics belong to the `bases` audit.

---

## 1. Public API surface

In declaration order across the eleven files. Most are static functions; only `RenderContext` and the `Value` type tower are classes.

### `RenderContext` (class, `RenderContext.js:31-153`)

- **Kind:** class
- **Exported as:** `RenderContext` (per `// public API symbol: RenderContext`)
- **Constructor:** `new RenderContext(app)`. Stores `this.app = app` and initialises `this.hoverPopover = null`. The `hoverPopover` slot is populated by callers that want a `HoverParent` reference (see §10) — `RenderContext` itself never assigns it.
- **Purpose:** Ambient renderer that knows how to materialise three Obsidian-domain inline primitives into DOM: internal wikilinks (`renderFileLink`), external URLs (`renderExternalLink`), and tags (`renderTag`). It is *not* a markdown renderer in the CommonMark sense — block-level markdown rendering lives in `editor/markdown/MarkdownRenderer` (sibling agent's domain). `RenderContext` is what `Value.renderTo(el, ctx)` receives so that any Bases cell, search-result row, embed, or hover popover renders links/tags/external URLs *consistently with the rest of the app* — same context-menu sections, same drag affordances, same `hover-link` event emission.
- **Lifecycle:** Constructed lazily by callers that need a render target. Bases creates a fresh `RenderContext(app)` per cell-render call. There is no central registry of contexts — each Bases view, hover popover, or search-result widget owns its own. No `Component` mixin (no `load`/`unload`); ownership is GC-only.
- **Mixes in:** neither `Component` nor `Events` — pure value object.

#### Methods

- **`renderFileLink(target, displayValue, parentEl)`** (`RenderContext.js:36-100`)
  - **Inputs:** `target` is a `TFile` (resolved) or a `string` linktext; in the string branch, `i = app.metadataCache.getFirstLinkpathDest(getLinkpath(target), "")` — **empty `sourcePath`** (Bases tables resolve from vault root). `displayValue` optionally writes the label via `.renderTo(span, this)`; fallback label is `nE(target)` for strings, `i.getShortName()` for resolved files, or raw linktext for unresolved.
  - **DOM:** `parentEl.addClass("markdown-rendered")` + `span.internal-link[data-href=<linktext>][class+="is-unresolved"?]`; `Ic(span)` adds internal-link styling. `data-href` is always the linktext, never the resolved path.
  - **Click:** buttons 0/1 → `app.workspace.openLinkText(linktext, "", Keymap.isModEvent(e))` (empty sourcePath).
  - **Context menu:** `Menu.forEvent(t)` with sections `["title","open","action-primary","action","info","info.copy","view","system","","danger"]`; `handleLinkContextMenu(menu, linktext, "")` populates built-ins; if `TFile`, appends a `danger` "Delete file" → `fileManager.promptForDeletion(file)`.
  - **Drag:** `dragManager.handleDrag(span, e => dragManager.dragLink(e, linktext, ""))` — draggable as a wikilink fragment.
  - **Hover:** `mouseover` gated by `Lc(e, span)` (modifier-key predicate) triggers `app.workspace.trigger("hover-link", {event, source:"bases", hoverParent:this, targetEl, linktext})`. **`source` is hard-coded to `"bases"`** regardless of the calling surface — Page-Preview must have `"bases"` registered as a hover source. `hoverParent` is the `RenderContext` itself.
- **`renderExternalLink(href, displayValue, parentEl)`** (`RenderContext.js:101-137`)
  - DOM: `<a class="external-link" href=<href> target="_blank" rel="noopener">`; label is `displayValue.renderTo(a, this)` if present, else raw href.
  - Right-click menu sections `["title","open","action","info","view","","danger"]` via `handleExternalLinkContextMenu(menu, href)`.
  - Click buttons 0/1 → `Yc(href)` safe-scheme guard (likely http(s)/mailto/obsidian); safe → `window.open(href, target)` where `target = Keymap.isModEvent(e)` (string or ""); unsafe → `Notice(i18n.msgFailedToOpenHref)`.
  - No drag, no hover.
- **`renderTag(tagWithHash, parentEl)`** (`RenderContext.js:138-150`)
  - Strips leading `#`, creates `<a class="tag">name</a>`. **Click handler binds to `parentEl`** (not the `<a>`), calling `app.internalPlugins.getEnabledPluginById("global-search").openGlobalSearch("tag:" + name)`. No-op if the plugin is disabled. No context menu, drag, or hover.

Notably **absent** despite Pass 1 hypothesis: no `renderDate`/`renderProperty`/`renderImage`/`renderEmbed` on `RenderContext`. Those live on the typed-value side (`DateValue.renderTo` etc. in `bases/`); `RenderContext` only mediates the three navigable inline primitives.

### Bidi-isolate decoration table (`mW`, `RenderContext.js:5-30`)

Not a public API symbol but lexically scoped to the same IIFE as `RenderContext`. A static record:

```javascript
{
  rtl: Decoration.mark({ class:"cm-iso", inclusive:true,
                          attributes:{dir:"rtl"}, bidiIsolate: Direction.RTL }),
  ltr: Decoration.mark({ class:"cm-iso", inclusive:true,
                          attributes:{dir:"ltr"}, bidiIsolate: Direction.LTR }),
  auto: Decoration.mark({ class:"cm-iso", inclusive:true,
                           attributes:{dir:"auto"}, bidiIsolate: null }),
}
```

These are CodeMirror-6 `Decoration.mark` factories tagged with `bidiIsolate: Direction.RTL/LTR/null`, so the editor **wraps RTL/LTR/auto inline ranges in `<span class="cm-iso" dir="…">`** with a true bidi-isolate at the CM-level. Confirms Pass 1 signal #11 (Editor / live-preview surface, "Bidi / RTL isolates"). The table is consumed by the editor extensions (sibling editor-domain agent should have visibility), but the *declaration* is here so this domain owns the spec.

### `Value` (base class, `RenderContext.js:154-202`)

- **Constructor:** `new Value()` → sets `icon = "lucide-file-question"`.
- **Static helpers:** `Value.equals(a, b)` (strict identity *or* matching constructor + `a.equals(b)`; both-null-safe); `Value.looseEquals(a, b)` (tries `a.equals`, then `a.looseEquals`, then `b.looseEquals` — symmetric).
- **Instance contract:** `equals(other)` default `false`; `looseEquals(other)` default `false`; **`renderTo(el, ctx)`** default `el.setText(toString())` — *this is the render contract subclasses override*; `keys()` default `[]`; `objectAccess(key)` default `null`; `type` getter returns constructor. `static type = "Any"` — subclasses override with the string name of their type.

### `Value` concrete subclasses (compact table)

The `Value` chain in `RenderContext.js` consists of ten classes. Only the `renderTo` DOM contracts are load-bearing for this domain; the full method surfaces belong to the `bases` audit. Summary:

| Class | Source lines | `type` | Icon | `renderTo(el, ctx)` DOM |
|---|---|---|---|---|
| `ErrorValue` (`yW`) | 203-245 | `"Error"` | `lucide-alert-triangle` | `<div class="bases-formula-error">` wrapping `<div class="warning-icon">` (Lucide icon) + `<div class="bases-formula-error-message">{msg}</div>`; attaches `setTooltip(el, msg, {delay:sv})` and a click handler that fires `displayTooltip(el, msg)`. |
| `NullValue` | 246-271 | `"Null"` | inherits | Singleton (`NullValue.value`); constructor throws on re-instantiation. `renderTo` is a no-op — null cells are visually empty. |
| `NotNullValue` | 272-277 | — | inherits | Trivial marker subclass; Bases uses `instanceof NotNullValue` as a null-filter. |
| `PrimitiveValue<T>` | 278-299 | — | inherits | Holds `data: T`. Strict `equals` on `data`, loose `==` for `looseEquals`. |
| `StringValue` | 300-324 | `"String"` | `lucide-text` | `el.setText(data)`. Exposes `length` via `objectAccess`. |
| `NumberValue` | 325-340 | `"Number"` | `lucide-binary` | `setText(toString())`, **except** non-finite/NaN writes literal `"∞"` (U+221E). |
| `BooleanValue` | 341-362 | `"Boolean"` | `lucide-check-square` | `<input type="checkbox" disabled checked=…>` — a non-interactive checkbox, not `true`/`false` text. |
| `ListValue` | 363-624 | `"List"` | `lucide-list` | `<div class="value-list-container">` containing `<span class="value-list-element">` (item's `renderTo`) and `<span class="value-list-gap">\n</span>` between items. The **literal `"\n"` text node** in `value-list-gap` is whitespace-preserved by CSS. |

`ListValue` additionally carries a full aggregation API (`length`, `get(i)` with lazy `zW`-coercion, `slice`/`reverse`/`concat`/`flatten`/`unique`/`sort`, `includes`/`compare`/`equals`/`looseEquals`, statistics `sum`/`mean`/`median`/`min`/`max`/`stddev`, temporal `earliest`/`latest`, and `join(sep)`) — all load-bearing for Bases formulas, not for rendering per se. See the future `bases` audit for the complete aggregation-method spec.

### `displayTooltip(el, text, opts?)` (`displayTooltip.js`)

- **Signature:** `displayTooltip(targetEl, text, opts?: { placement?: "top"|"right"|"bottom"|"left", classes?: string[], gap?: number, horizontalParent?: Element, delay?: number })`. Default placement `"bottom"`.
- **Purpose:** Materialises a tooltip immediately (or after `delay` ms) at the target. Companion to `setTooltip(el, text, opts)` (defined elsewhere) which sets `aria-label`-style attributes that `displayTooltip` later reads on hover. Flow: delay-dispatch via `setTimeout` if a delay is set and no recent tooltip is live; otherwise reuse the singleton `.tooltip` div if it's already on this target, else tear down and re-create a body-appended `<div class="tooltip">` + `.tooltip-arrow` child. Placement arithmetic adds `mod-{top,right,left}` classes and clamps to viewport; arrow offset adjusts when the tooltip is shifted to fit. `horizontalParent` lets a child-cell tooltip hug its column's left edge.
- **Module-static state (file-scoped):** `uv` (active div), `hv` (active target), `dv` (delay timeout), `wv`/`cv` (recently-dismissed cooldown timestamps), `fv` (clear-pending), `Cv` (destroy), `ov` (default gap), `av` (top-arrow offset), `sv` (default delay constant, also used by `ErrorValue.renderTo`).

### `htmlToMarkdown(html)` (`htmlToMarkdown.js`)

- **Kind:** function
- **Exported as:** `htmlToMarkdown`
- **Signature:** `htmlToMarkdown(htmlString: string): string`
- **Purpose:** Single-line wrapper: `return hP.turndown(html);`. `hP` is the Turndown service singleton configured elsewhere (out of audit scope — flagged in §13). Used on paste-from-browser to convert clipboard HTML to Markdown source before insertion. Plugins also call `htmlToMarkdown` to convert arbitrary HTML to MD (e.g. for "Save to vault" web-clipper plugins).

### `sanitizeHTMLToDom(html)` (`sanitizeHTMLToDom.js`)

- **Kind:** function
- **Exported as:** `sanitizeHTMLToDom`
- **Signature:** `sanitizeHTMLToDom(htmlString: string): DocumentFragment`
- **Body:** `return document.importNode(EL.sanitize(html, SL), true);`. `EL` is DOMPurify; `SL` is the rule object (allowlists + flags). The `document.importNode(…, true)` deep-clones the result into the *current document* — important when the call is made from a popped-out window (different `document`).
- **Purpose:** The single chokepoint for any *plugin-supplied* HTML that needs to enter the live DOM. Plugins should `el.appendChild(sanitizeHTMLToDom(plugin.html))` rather than `el.innerHTML = plugin.html`. The `EL`/`SL` configuration (allowlist of tags + attrs) lives in the bundle outside this directory and could not be inspected during this audit; flagged in §13.

### `loadMathJax()` (`loadMathJax.js`)

- **Signature:** `loadMathJax(): Promise<void>`
- **Body:** `await bz.promise; return;` — awaits the in-bundle `bz` deferred and resolves with no value. The MathJax library, once loaded, attaches itself to `window.MathJax`; calling `loadMathJax()` is the contract for "be sure MathJax is ready before I call `renderMath`/`finishRenderMath`".
- The `bz` deferred resolution lives outside this directory; the *first* `loadMathJax` call observed by the bootstrap is what kicks off the actual script-tag injection. Subsequent calls cheaply re-await the same resolved promise.

### `renderMath(latex, displayMode)` (`renderMath.js`)

- **Signature:** `renderMath(latex: string, displayMode: boolean): HTMLElement`
- **Body:** `return MathJax.tex2chtml(latex, { display: displayMode });` — synchronous, returns a DOM node. **`displayMode === true`** means *display math* (block-level, larger, centred); **`false`** means *inline math*. **Caller responsibility:** call `loadMathJax()` first, then `renderMath()` per equation, then `finishRenderMath()` once at end of pass.
- The output is CHTML (Common HTML); MathJax v3 nodes that need to be `finishRenderMath`-typeset before they become visually correct.

### `finishRenderMath()` (`finishRenderMath.js`)

- **Signature:** `finishRenderMath(): Promise<void>`
- **Body:** Lazy-creates a deferred (`Cz || (Cz = gb())`), debounces a 100 ms `setTimeout(Sz, 100)` (calling `Sz`, the typeset+resolve worker), and returns `Cz.promise`. Any caller that issued `renderMath()` calls during the same tick joins the same batch: all of them await the *same* `Cz` deferred, the timer trails 100 ms after the last `finishRenderMath()` call, and `Sz` runs MathJax's global typeset/font-load+layout pass. After resolve, `Cz` is cleared and the next batch starts fresh.
- **Implication for Markoff:** Obsidian batches typesetting per ~100 ms window across an entire markdown render. JKQTMathText doesn't have a typeset queue (each `MathRenderer::render` is independent), so Corbomite gets per-equation rendering "for free" but has no equivalent batching savings — relevant for very-equation-dense notes.

### `loadMermaid()` (`loadMermaid.js`)

- **Signature:** `loadMermaid(): Promise<MermaidLib>`
- **Body:** `await mz.promise; return mermaid;` — returns the global `mermaid` library (note: not `window.mermaid`, but a bundle-scoped binding). Plugins that want to render diagrams call `await loadMermaid()` then `mermaid.render(id, source)`.

### `loadPdfJs()` (`loadPdfJs.js`)

- **Signature:** `loadPdfJs(): Promise<typeof window.pdfjsLib>`
- **Body:** `await uz.promise; return window.pdfjsLib;` — returns the PDF.js global. Used by the PDF view (sibling `views` domain) to lazy-init the worker thread before opening any document.

### `loadPrism()` (`loadPrism.js`)

- **Signature:** `loadPrism(): Promise<typeof Prism>`
- **Body:** `await gz.promise; return Prism;`. Plugins that need syntax-highlighting outside Obsidian's automatic code-block path (e.g. a custom code-block processor) call this before `Prism.highlight(code, grammar, "language")`.

### `renderResults(el, text, search, offset?)` (`renderResults.js`)

- **Kind:** function
- **Exported as:** `renderResults`
- **Signature:** `renderResults(el: HTMLElement, text: string, searchResult: { matches: [start, end][] } | null, offset: number = 0): void`
- **Body:** One-liner: `renderMatches(el, text, search ? search.matches : null, offset)`.
- The actual `renderMatches` implementation lives outside this directory (it is referenced in `ui/popups/AbstractInputSuggest.js`, `ui/popups/PopoverSuggest.js`, and `utils/apiVersion.js` — meaning it is a global utility). Its contract, inferred from call sites: walks `text`, splits at every `[start,end]` match range from `searchResult.matches`, wrapping in-match runs with `<span class="suggestion-highlight">` (or analogous) and emitting plain text between. The `offset` is added to all match offsets — useful when `text` is a slice of a larger string and `matches` are absolute.
- **Purpose:** Single canonical helper used by **search-result rows**, **quick-switcher list**, **command-palette list**, and **every `SuggestModal`/`AbstractInputSuggest`**. Markoff currently lacks an equivalent — flagged in §12.

---

## 2. Data structures

### `RenderContext` (instance shape)

```typescript
{
  app: App;                 // ambient DI root
  hoverPopover: HoverPopover | null;  // optional HoverParent slot
}
```

No additional state. The class is intentionally dataless beyond the app reference.

### `BidiDecorationTable` (`mW`)

```typescript
{
  rtl: Decoration;   // CM6 mark, attrs={dir:"rtl"},  bidiIsolate: Direction.RTL
  ltr: Decoration;   // CM6 mark, attrs={dir:"ltr"},  bidiIsolate: Direction.LTR
  auto: Decoration;  // CM6 mark, attrs={dir:"auto"}, bidiIsolate: null
}
```

All three carry `inclusive: true` (the mark extends to inserted text at its boundaries) and `class: "cm-iso"`. Consumed by the editor-domain extension code that walks document inline runs and assigns one of the three based on Unicode direction analysis.

### `Value` chain (abbreviated — full spec belongs to `bases`)

`Value { icon, type }` base. `PrimitiveValue<T> extends NotNullValue { data: T }` → `StringValue<string>` / `NumberValue<number>` / `BooleanValue<boolean>`. `ListValue extends NotNullValue { data: unknown[], lazyEvaluator: (i, raw) => Value }`. `NullValue` is a singleton (constructor throws on re-instantiation). `ErrorValue extends Value { message: string }`. `static type` override strings: `"Any" | "String" | "Number" | "Boolean" | "List" | "Null" | "Error"` (Bases adds `"Date" | "Tag" | "Link" | …`). All `renderTo(el, ctx)` produce DOM in-place (no return). Equality: strict on `data` for primitives; element-wise (with `Value.equals`) for lists. `looseEquals` collapses single-element lists to scalar comparison.

### `displayTooltip` options

```typescript
{
  placement?: "top" | "right" | "bottom" | "left";  // default "bottom"
  classes?: string[];                                // appended to .tooltip
  gap?: number;                                       // px from anchor; default ov constant
  horizontalParent?: Element;                         // overrides horizontal anchor rect
  delay?: number;                                     // ms; 0 = synchronous
}
```

### `renderResults` search-input shape

```typescript
{
  matches: Array<[startIndex: number, endIndex: number]>  // half-open [start, end)
}
```

The `searchResult` argument matches the shape returned by `prepareFuzzySearch(query)(text)` from the `search` domain. `null` is allowed → renders plain text without highlights.

---

## 3. On-disk contracts

`No on-disk contracts.`

This domain reads no vault file directly, writes no vault file directly, and persists no `.obsidian/*.json` config. It does, however, **read CSS variables** (e.g. `.tooltip` styling, `cm-iso` direction styling, `internal-link` underline colour, `value-list-container` flex rules) from the active theme — so theme CSS is an *implicit input contract*. Themes that omit `.tooltip`, `.callout-icon`, `.suggestion-highlight`, `.value-list-container` etc. produce visually-broken renders.

The lazy-loaders (`loadMathJax`/`loadMermaid`/`loadPdfJs`/`loadPrism`) trigger script-tag injection *outside this directory*; whether those scripts come from a bundled asset or a CDN fetch is invisible from inside the audit scope (Pass 1 reasoning suggests *bundled*, since Obsidian works offline by design).

---

## 4. Events emitted

### `RenderContext` (does *not* extend `Events`)

`RenderContext` itself never `.trigger`s. It does, however, *cause* the workspace to `.trigger("hover-link", …)` from `renderFileLink`'s `mouseover` handler:

| Event name | Payload (inferred) | Triggered when | Typical consumers |
|---|---|---|---|
| `hover-link` (on `app.workspace`) | `{event: MouseEvent, source: "bases", hoverParent: RenderContext, targetEl: HTMLSpanElement, linktext: string}` | mouseover on a `RenderContext.renderFileLink` span, when `Lc(e, span)` returns truthy (modifier-or-default-source predicate) | Page-Preview internal plugin matches the `source`-registered modifier-key requirement (see `workspace` doc §7), then opens a `HoverPopover` with `linktext` resolved against `app.workspace.getActiveFile().path` |

Cite: `RenderContext.js:90-99`.

`displayTooltip`, `htmlToMarkdown`, `sanitizeHTMLToDom`, `loadMathJax`, `renderMath`, `finishRenderMath`, `loadMermaid`, `loadPdfJs`, `loadPrism`, `renderResults` — none `.trigger` events; they are all pure functions or `Promise`-returning utilities.

---

## 5. Events consumed

This domain subscribes to nothing directly. The render-context's `RenderContext` constructor does not call `app.on(...)`. The lazy-loaders return promises; they don't subscribe to anything. The `Value` chain is event-free.

| Listener file | Subscribes to | Why |
|---|---|---|
| _(none)_ | _(none)_ | Rendering-domain code is purely event-emitting and pure-function; consumers (e.g. `MarkdownPreviewView`) subscribe to `post-processor-change` themselves. |

The post-processor-change lifecycle is a sibling-domain concern: `plugin/Plugin.js:144/147/162/166` triggers it; `editor/markdown/MarkdownPreviewView.js:253` re-renders on it.

---

## 6. Commands registered

`No commands registered here.`

The rendering domain is invoked *by* commands (e.g. `editor:toggle-source` reaches `loadMathJax`/`renderMath`) but does not declare any `app.commands.addCommand({...})` itself.

---

## 7. Registries owned

This domain owns no plugin-registered registries. The post-processor and code-block-processor registries (which would conceptually belong to "rendering") are owned by **`editor/markdown/MarkdownPreviewRenderer`** (`MarkdownPreviewRenderer.postProcessors: Function[]`, `MarkdownPreviewRenderer.codeBlockPostProcessors: Map<lang, Function>`) — sibling domain.

The only *registry-shaped* state in this domain:

### `mW` bidi-decoration map (`RenderContext.js:5-30`)

- **Stores:** three `Decoration` factories, keyed by `"rtl" | "ltr" | "auto"`.
- **Populated by:** static initialiser only — **never extended at runtime**, never plugin-registerable.
- **Read by:** the editor-extension code that decides per-inline-run direction (sibling editor domain).
- **Persistence:** in-memory module-static.
- **Lifecycle:** lives forever, no add/remove API.

(Internal-only constant; not a public registry. Listed for completeness because Pass 1 specifically called out bidi as a Markoff gap signal.)

### Tooltip singleton state (`uv`/`hv`/`dv`/`wv`/`cv`)

- File-static module variables shared across all `displayTooltip` invocations. Acts as a "one tooltip at a time" gate. Not a registry — single-slot mutex.

---

## 8. Invariants

Enumerated. Corbomite must uphold these for compatibility with Obsidian-trained user expectations and any plugin-API surface that ports over.

1. `RenderContext.renderFileLink` always resolves with `sourcePath = ""` (vault-root). Corbomite's analogue must accept empty source-path and not collapse to `null`.
2. `renderFileLink` always emits `source: "bases"` on `hover-link`. Page-Preview must have `"bases"` registered.
3. `renderTag`'s click handler binds to `parentEl`, not the `<a>` child — clicking cell padding still triggers tag-search.
4. `BooleanValue.renderTo` produces `<input type="checkbox" disabled>`, not text.
5. `NumberValue.renderTo` writes literal `"∞"` (U+221E) for non-finite/NaN.
6. `ListValue.renderTo` separates items with a literal `"\n"` text node inside `<span class="value-list-gap">` (whitespace-preserved). Do not substitute `<br>`.
7. `finishRenderMath` is 100 ms debounce-batched: all per-tick `renderMath` outputs typeset in one pass.
8. `loadMathJax`/`loadMermaid`/`loadPdfJs`/`loadPrism` are idempotent and cheap on re-await. Plugins call them ad-hoc without check-then-load.
9. `renderMath(src, true)` = display mode; `false` = inline.
10. `sanitizeHTMLToDom` returns a `DocumentFragment` deep-`importNode`'d into the *current* `document` — safe across popout-window boundaries.
11. `htmlToMarkdown` accepts arbitrary (untrusted) HTML; sanitisation is on the input. Callers must not double-escape.
12. `displayTooltip` is single-slot; `wv + cv` cooldown skips the delay when sweeping across tooltipped icons.
13. `renderResults(el, text, null, offset?)` with null search renders plain text without highlights.
14. `Value.equals`/`Value.looseEquals` are *static* helpers that dispatch to *instance* overrides with constructor-identity guards.
15. `Value.icon` default is `"lucide-file-question"` — the diagnostic fallback. Do not silently render nothing when a type forgets to override.

---

## 9. Observable user features

User-visible behaviours powered by this domain. These feed into `FEATURE-MATRIX.md`.

- **Wikilinks in Bases cells / search rows / suggesters:** click opens note; middle-click opens new tab; modifier-click opens new pane/window (Keymap rules); right-click → context menu `[title|open|action-primary|action|info|info.copy|view|system|""|danger]` with "Delete file" when resolved; hover fires `hover-link` (source `"bases"`) → Page-Preview popover; draggable via `dragManager.dragLink`.
- **External links in Bases cells:** click opens default browser (safe-scheme guard `Yc`; fail → `Notice`); right-click menu `[title|open|action|info|view|""|danger]`.
- **Tags in Bases cells:** click anywhere in cell runs `openGlobalSearch("tag:<tag>")`; no-op if global-search plugin disabled.
- **Math equations:** typeset inline and display via MathJax; invalid LaTeX renders as a MathJax error span.
- **Fenced `mermaid` code blocks:** rendered as SVG; invalid diagrams render Mermaid's error pane.
- **`.pdf` files + `![[file.pdf]]` embeds:** render via PDF.js.
- **Fenced code blocks with language tag:** Prism syntax-highlighted.
- **Paste HTML from browser:** converted to Markdown by Turndown (`htmlToMarkdown`).
- **Plugin-supplied HTML:** sanitised via DOMPurify (`sanitizeHTMLToDom`); safe to `appendChild` even across popout-window documents.
- **Tooltips on hover:** appear via `displayTooltip`; placement adapts to viewport edges; sweeping across multiple tooltipped icons skips the configured delay (recently-dismissed cooldown).
- **Search / quick-switcher / command-palette / suggester rows:** show match-highlighted snippets via `renderResults`.
- **Typed Bases cells:** string → text; number → text (literal `"∞"` for infinity/NaN); boolean → disabled checkbox; list → wrapping flex container with whitespace-preserved newline separators; null → empty; error → alert-triangle icon + message + tooltip + click-expand.
- **Editor bidi-isolate:** RTL/LTR/auto inline runs wrapped in `<span class="cm-iso" dir="…">` with CM6 `bidiIsolate: Direction.RTL/LTR/null`.

---

## 10. Extension surfaces exposed

The rendering domain *itself* exposes no plugin registration verbs — but it is *consumed* by every plugin-registered renderer in the editor-markdown sibling domain. The plugin-facing registration verbs that hand DOM to this domain are:

| Surface | Registration verb | Consumer call site | What plugins supply |
|---|---|---|---|
| Markdown post-processor (DOM walker) | `Plugin.registerMarkdownPostProcessor(fn, sortOrder?)` | `editor/markdown/MarkdownPreviewRenderer.js:1167-1173` (push + ascending stable sort) | `(el: HTMLElement, ctx: MarkdownPostProcessorContext) => void \| Promise<void>` |
| Code-block processor | `Plugin.registerMarkdownCodeBlockProcessor(lang, fn, sortOrder?)` | `editor/markdown/MarkdownPreviewRenderer.js` (registerCodeBlockPostProcessor) | `(source: string, el: HTMLElement, ctx: MarkdownPostProcessorContext) => void \| Promise<void>` |
| `MarkdownRenderChild` subclassing | `ctx.addChild(child)` from inside a post-processor | `editor/markdown/MarkdownRenderChild.js` | A `Component` subclass whose `unload()` runs when the rendered section is recycled |
| Hover-link source | `Plugin.registerHoverLinkSource(id, {display, defaultMod})` | `workspace.hoverLinkSources` (read by Page-Preview) | A friendly source-name string + a default-modifier-required boolean |
| HTML sanitisation chokepoint | `sanitizeHTMLToDom(html)` (no registration — just call it) | `rendering/sanitizeHTMLToDom.js` | _N/A_ — pure utility |
| Lazy-load awaiters | `loadMathJax()` / `loadMermaid()` / `loadPdfJs()` / `loadPrism()` | `rendering/load*.js` | _N/A_ — pure utility |
| Tooltip primitive | `displayTooltip(el, text, opts)` (immediate) and `setTooltip(el, text)` (deferred-on-hover) | `rendering/displayTooltip.js` | _N/A_ — pure utility |
| HTML→MD converter | `htmlToMarkdown(html)` | `rendering/htmlToMarkdown.js` | _N/A_ — pure utility |
| Search snippet renderer | `renderResults(el, text, search, offset?)` | `rendering/renderResults.js` | _N/A_ — pure utility |
| `Value` subclass + `renderTo(el, ctx)` | _Bases-internal_ — extending `Value` lets a plugin add a typed cell value | `RenderContext.js:154-624` chain | A `Value` subclass with `renderTo(el, ctx)`, `equals`, `toString`, optional `keys`/`objectAccess` |

The `MarkdownRenderChild` subclass + the `ctx.addChild(child)` registration is the lifecycle bridge that ties **renderer-produced DOM** to **plugin-side cleanup**. When a `MarkdownPreviewSection` is recycled (because the user scrolls past, or the file changes), every registered child's `unload()` fires — letting plugins detach DOM event listeners, cancel async work, and free resources. Corbomite's reading-view recycle pool (when implemented) MUST honour this: any plugin-supplied DOM fragment registered via `addChild` gets `unload`-callback parity.

---

## 11. Corbomite mapping

Corbomite-side files: `libs/markoff/src/MathRenderer.{cpp,h}` (LaTeX → QImage via JKQTMathText), `libs/mmdr/` (Rust Mermaid bridge — static-linked archive `libmermaid_rs_renderer.a` with FFI header `mmdr_ffi.h`), `libs/markoff-parser/` (tree-sitter parser producing the AST; rendering happens elsewhere), `libs/jkqtmathtext/` (vendored math typesetter), `libs/core/include/corbomite/core/MarkoffRenderEngine.h` + `.cpp` (markdown → HTML wrapper), `libs/core/include/corbomite/core/MarkdownRenderer.h` + `.cpp` (older regex-based renderer used for canvas cards). Markoff's reading view is currently being rebuilt; the previous `ReadingView.{cpp,h}` exist only as build artifacts/clangd indices in the working tree — flagged in §13.

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `MarkdownRenderer.render(md, el, sourcePath, component)` (sibling-domain entry) | `Corbomite::MarkoffRenderEngine::render(md, opts) → RenderedDocument` | Partial | Returns HTML + metadata, not live-DOM mutation; `sourcePath`/`component` semantics not plumbed. Old regex-based `Corbomite::MarkdownRenderer` is canvas-card-only and TODO-marked for replacement (`MarkdownRenderer.h:12-16`). |
| `RenderContext` (inline-primitive ambient renderer) | _Missing — wiring entangled in `libs/markoff/src/Editor.cpp` and the deleted reading-view_ | Missing | Extract `Markoff::LinkRenderer` exposing `renderFileLink`/`renderExternalLink`/`renderTag` with same context-menu-sections / drag / hover-link-emission contract. Drives Bases cells when Bases lands. |
| Bidi `mW.rtl/.ltr/.auto` `cm-iso` decorations | _N/A_ | Missing | Qt inherits `QTextOption::TextDirection` per-paragraph only; no per-inline isolate spans. Mixed-script notes diverge visually. Long-term fix: per-inline `QTextCharFormat::setLayoutDirection` on detected RTL runs. |
| Markdown post-processor pipeline (`postProcessors[]` ascending sortOrder) | _N/A_ | Missing | No post-processor registry. Introduce `Markoff::PostProcessorRegistry` keyed by integer sort-order; each processor receives `(QWidget* sectionEl, RenderContext* ctx)`; awaits `QFuture<void>` before section-complete. |
| Code-block processor (per-language dispatch) | Mermaid hard-wired via `libs/mmdr/`; Math via inline detection | Partial | Plugins cannot claim a fenced language. Introduce `Markoff::CodeBlockProcessorRegistry::registerLanguage("lang", fn)`; default handler is syntax-highlight. |
| `loadMathJax()` lazy promise | JKQT statically linked | Different | No lazy load needed. First-call cost (font load) nonzero — pre-warm if cold-start scrolling is sluggish. Shim must exist as resolved-future for plugin compat. |
| `renderMath(latex, display)` → DOM | `Markoff::MathRenderer::render(latex, display, fontSize, dpr) → QImage` | Partial | `QImage` (DPR=3), not live DOM. Cached process-wide by `(latex, display, fontSize, dpr)`. No batching needed. Under-amortised vs MathJax only for equation-dense notes. |
| `finishRenderMath()` 100 ms batch | _N/A_ | Different | Shim must exist as `QFuture<void>::makeReady()` for plugin compat. |
| `loadMermaid()` + `mermaid.render` | `libs/mmdr/` Rust FFI bridge | Partial | Static-linked; no lazy contract. Verify KDE theme integration, error-pane on invalid diagrams, version parity. |
| `loadPdfJs()` + `pdfjsLib` | _Missing_ | Missing | No PDF rendering today. Long-term: Poppler-Qt6 or Okular kpart. Plugin-API shim returns a Poppler facade. |
| `loadPrism()` + `Prism.highlight` | KDE `KSyntaxHighlighting` (reading-view reach unconfirmed) | Partial | Coverage acceptable (~300 languages) but names differ ("py" vs "python"); shim must translate. |
| `htmlToMarkdown` (Turndown) | _Missing_ | Missing | Paste-from-browser inserts HTML literally today. Add `Markoff::htmlToMarkdown(QString)` — high-priority for web-clipper plugin compat. |
| `sanitizeHTMLToDom` (DOMPurify) | _Implicit via `QTextDocument::setHtml` subset_ | Partial | `QTextDocument`'s XHTML-1.0 subset silently strips `<details>`/`<svg>`/etc. Add explicit `Markoff::sanitizeHtml(QString, QTextDocument* targetDoc=nullptr)` matching Obsidian's `SL` allowlist (§13). |
| `displayTooltip` / `setTooltip` | `QToolTip::showText` / `QWidget::setToolTip` | Have | Cooldown-skip-on-sweep differs from Qt semantics; minor UX divergence. |
| `renderResults` highlight spans | _Missing_ | Missing | Extract `Markoff::renderHighlightedRuns(QString text, QList<QPair<int,int>> matches, int offset=0)` as the single chokepoint. |
| `Value` typed-cell tower | _N/A — Bases not yet implemented_ | Missing | Belongs to future Bases audit; mirror render-contract shape for plugin portability. |
| Callout chrome (`callout/callout-title/callout-icon/callout-content`) | `libs/markoff/src/MarkdownTextItem.cpp` (parser); reading-view DOM unconfirmed | Partial | Verify Qt-side class parity by rendering `> [!info]` test note. |

---

## 12. Markoff gap confirmations / discoveries

Confirmation of Pass-1 signals from `01-markoff-gaps.md` "Rendering / preview surface", plus new signals discovered during the audit. Citations use short form `rendering/<file>.js:<line>`.

**Confirmed Pass-1 signals:**

- **MathJax integration (typeset + finishRenderMath batch).** `loadMathJax.js:5-16` + `renderMath.js:5-9` + `finishRenderMath.js:5-12`. Confirmed: `renderMath` synchronous DOM return; `finishRenderMath` 100 ms debounce-batches all per-tick renders into one typeset pass. JKQTMathText is per-call synchronous — functional parity without batching. Plugin-API shim must expose a no-op resolved future so ported plugins don't break.
- **Mermaid lazy init.** `loadMermaid.js:5-16`. Corbomite's `libs/mmdr/` is static-linked — no lazy load. Parity gaps to verify: KDE colour-scheme theme integration, error-pane rendering for invalid diagrams, interactive node features, Mermaid version parity.
- **PDF.js embeds.** `loadPdfJs.js:5-16`. Corbomite has no PDF capability today. High-priority: decide Poppler-Qt6 vs Okular kpart vs PDF.js-via-WebEngine.
- **Prism syntax highlighting.** `loadPrism.js:5-16`. KSyntaxHighlighting is the KDE analogue; per-language name mapping and theme adaptation need auditing.
- **`renderResults` highlight spans.** `renderResults.js:5-7` → in-bundle `renderMatches`. Contract: `(el, text, {matches:[start,end][]}, offset)`. Corbomite has no standalone helper.
- **`htmlToMarkdown` (Turndown).** `htmlToMarkdown.js:5-7` → `hP.turndown(html)`. The Turndown rule list is configured outside this directory — §13 filed.
- **`sanitizeHTMLToDom` (DOMPurify).** `sanitizeHTMLToDom.js:5-7` → `EL.sanitize(html, SL)` with deep `importNode`. The `SL` allowlist is configured outside this directory — §13 filed. Security-boundary critical: Corbomite's sanitisation must match this config exactly.
- **Callout chrome.** Not produced by anything in `rendering/` — `editor/markdown/MarkdownRenderer` (sibling domain) builds the `callout/callout-title/callout-icon/callout-content` DOM from the blockquote AST. Parser produces the AST node, renderer produces the DOM — same split in Markoff.

**New signals** (full details in §8 Invariants and §11 Corbomite mapping; one-line bullets here):

- `RenderContext.renderFileLink` hard-codes `source: "bases"` on every `hover-link` emission.
- `renderTag`'s click handler binds to the parent element, not the `<a>`.
- `renderFileLink` resolves with empty `sourcePath` (vault-root).
- Bidi-isolate decorations `mW.rtl/.ltr/.auto` apply CM6 `bidiIsolate: Direction.RTL/LTR/null` — Markoff inherits Qt's per-paragraph direction but not per-inline isolate spans.
- `BooleanValue.renderTo` = disabled checkbox, not text.
- `NumberValue.renderTo` writes literal `"∞"` for non-finite/NaN.
- `ListValue.renderTo` uses literal `"\n"` text node inside `<span class="value-list-gap">` — do not substitute `<br>`.
- `ErrorValue.renderTo` pattern: `bases-formula-error` div + `warning-icon` (alert-triangle) + message div + tooltip + click-to-show.
- `displayTooltip` has single-slot state + recently-dismissed cooldown that skips delay on icon-sweep.
- `finishRenderMath` batch contract must exist as a no-op for plugin compatibility even on static-linked math.
- All four `load*()` shims are idempotent and cheap on re-await.
- `sanitizeHTMLToDom` returns a `DocumentFragment` `importNode`'d into the *current* document — matters for popout windows.
- The DOMPurify `SL` config and Turndown `hP` rule list live outside `rendering/` — must be extracted in Pass 3.
- `renderResults` is shared across search / quick-switcher / command-palette / every suggester; Markoff lacks a standalone equivalent.

All findings appended to `01-markoff-gaps.md` under `## Pass 2 additions — rendering` per the agent rules.

---

## 13. Open questions

1. **Exact DOMPurify `SL` config** (sanitizer allowlist of tags + attrs) — defined outside `rendering/`. Critical for security parity; Pass 3 grep bundle for `SL = { ALLOWED_TAGS: …, ALLOWED_ATTR: … }`.
2. **Exact Turndown rule set** — `hP` configuration outside scope. Pass 3 grep for `new TurndownService` / `.addRule(`; enumerate GFM extensions enabled, HTML→MD tag mapping, custom rules.
3. **`Lc(event, span)` semantics** in `renderFileLink` mouseover. Likely "modifier held *or* preview-on-hover-without-modifier enabled". Gates `hover-link` emission.
4. **`Yc(href)` safety predicate**. Likely scheme allowlist (`http`/`https`/`mailto`/`obsidian`/`tel`/`sms`). Markoff must apply the same list.
5. **`renderMatches` definition** — called by `renderResults.js:6` but defined elsewhere; likely `ui/components/` or `utils/`.
6. **`nE(linktext)` behaviour** (`RenderContext.js:51`) — basename-stripper or `#`-stripper.
7. **`Ic(span)` behaviour** (`RenderContext.js:55`) — probably the `internal-link` class/attribute applicator.
8. **`app.workspace.handleLinkContextMenu` built-in items** — enumerate for Corbomite context-menu parity (workspace-domain follow-up).
9. **Is `RenderContext.hoverPopover` ever populated by a caller?** Ctor sets `null`; Bases callers may set before `renderFileLink`. Needs Bases-domain grep.
10. **Does the `BooleanValue` checkbox become editable in any context?** Constructor `disabled=true` suggests read-only; editable variants likely override `renderTo`. Bases-domain clarifies.
11. **Bundled MathJax / Mermaid / PDF.js / Prism versions.** Affects feature parity; Pass 3 grep `package.json` if accessible.

---

## 14. Recommended Pass 3 synthesis input

1. **`RenderContext` triplet is an ambient inline-primitive renderer for *every* Bases cell / search row / suggester / plugin-supplied surface.** Promote into `FEATURE-MATRIX.md` as "Ambient inline-element renderer (link/tag/external-link with click+drag+contextmenu+hover-link wiring)". Corbomite needs `Markoff::LinkRenderer` extracted early.
2. **Four-promise lazy-load contract** (`loadMathJax`/`loadMermaid`/`loadPdfJs`/`loadPrism`) + `finishRenderMath` debounce-batch. All five must exist in the Corbomite plugin-API surface even as resolved-future no-ops. Add `GAP-ANALYSIS.md` row for PDF.js-equivalent-via-Poppler.
3. **HTML sanitisation (`sanitizeHTMLToDom`) and HTML→Markdown (`htmlToMarkdown`) are security/compatibility chokepoints with configs *outside* this directory.** Pass 3 must extract the DOMPurify `SL` allowlist and the Turndown rule list and add to `VAULT-FORMAT.md` ("Plugin-HTML and Paste compatibility"). A sanitiser-allowlist divergence is a security-bug class.

---

## 15. Cross-domain references

Other Pass 2 domains this doc touches and why.

| Other domain | Reference type | Brief description |
|---|---|---|
| `metadata` | consumer (this domain reads from) | `RenderContext.renderFileLink` calls `app.metadataCache.getFirstLinkpathDest(getLinkpath(linktext), "")`. Sees `metadata.md` §3 (link resolution algorithm) — the empty `sourcePath` invocation matters for the 10-step resolver. |
| `vault` | consumer (this domain depends on) | `RenderContext.renderFileLink` checks `target instanceof TFile` — the `TFile` type is owned by `vault`. The unresolved-link branch returns `i.getShortName()` where `i` is a `TFile`, again from `vault`. |
| `workspace` | emitter target + consumer | Emits `hover-link` on `app.workspace` (consumed by Page-Preview). Calls `app.workspace.openLinkText` (open-link), `handleLinkContextMenu` / `handleExternalLinkContextMenu` (built-in context-menu items). Subscribes (indirectly via the editor-markdown sibling) to `post-processor-change` for re-render triggers. |
| `editor/markdown` | sibling (Markoff-overlap zone) | This domain provides primitives; sibling domain owns the actual `MarkdownRenderer.render(...)` entry point + the `MarkdownPreviewRenderer.postProcessors[]` registry + the `MarkdownPreviewSection` recycling. Callouts, embeds, checkbox-click round-trip, and progressive section rendering all live in the sibling. The `MarkdownRenderChild` lifecycle (consumed by post-processors registered in the sibling) ties directly to this domain's render outputs. |
| `bases` | sibling (consumer of `Value`/`RenderContext`) | All `Value` subclasses (`StringValue`, `NumberValue`, `BooleanValue`, `ListValue`, `NullValue`, `ErrorValue`) declared in `RenderContext.js` are extended by `bases/StringValue.js` etc. Bases cell rendering invokes `value.renderTo(el, ctx)` where `ctx: RenderContext`. The `bases-formula-error` DOM class (from `ErrorValue.renderTo`) is consumed by Bases CSS. |
| `core` | dependency | The `RenderContext` constructor takes an `App` instance (from `core/App.js`). |
| `ui/menu` | consumer | Both `renderFileLink` and `renderExternalLink` build a `Menu.forEvent(t).addSections([…]).addItem(…)`. Section names align with the Pass 2 `workspace.md` enumeration. |
| `ui/popups` | consumer (`displayTooltip` shares state) | The `setTooltip` companion (defined elsewhere in `ui/popups/` likely) sets attributes that `displayTooltip` later reads. The hover-link emission feeds `HoverPopover` (in `ui/popups/`) via Page-Preview. `renderResults` is consumed by `AbstractInputSuggest` and `PopoverSuggest`. |
| `search` | consumer (`renderResults` input shape) | The `searchResult.matches` shape matches `prepareFuzzySearch(query)(text)` output from the `search` domain. |
| `plugin` | consumer (registration verbs) | `Plugin.registerMarkdownPostProcessor` / `registerMarkdownCodeBlockProcessor` / `registerHoverLinkSource` all flow into rendering primitives but live in `plugin/Plugin.js`. The `post-processor-change` event is triggered by `plugin/Plugin.js:144/147/162/166` on every register/unregister. |
| `parsing` | sibling | The Markdown→AST parsing (parse → render) lives in `parsing/` for inline syntax; `editor/markdown/` does the AST→DOM. Bidi-isolate decisions in `mW` likely consume per-inline direction analysis from `parsing/` or the editor-domain bidi pass. |
| `utils` | dependency | `getLinkpath`, `nE`, `Ic`, `Lc`, `Yc`, `ub`, `zW`, `setTooltip`, `setIcon`, `Notice`, `Keymap.isModEvent`, `Direction.RTL/LTR` — all from `utils/` or stand-alone helpers. |

Short symbols from other domains referenced by name (Pass 3 symbol-table input):

| Short symbol | Defined in | Used here for |
|---|---|---|
| `bz` / `mz` / `uz` / `gz` | `core` (library-bootstrap bundle) | Deferred promises awaited by `loadMathJax` / `loadMermaid` / `loadPdfJs` / `loadPrism` respectively |
| `Cz` / `Ez` / `Sz` | `finishRenderMath` module-static + `core` | typeset-batch deferred / debounce timeout id / MathJax typeset-resolve worker |
| `EL` / `SL` | `core` (DOMPurify singleton + config) | HTML sanitiser and allowlist used by `sanitizeHTMLToDom` |
| `hP` | `core` (Turndown service singleton) | HTML→MD converter used by `htmlToMarkdown` |
| `nE` / `Ic` / `Lc` / `Yc` / `ub` / `gb` | `utils` (likely) | Linktext display-name extractor, internal-link styling, hover-link predicate, external-URL safety predicate, natural-sort comparator, deferred-promise factory |
| `zW` | `bases` (likely) | Lazy `Value`-coercion used by `ListValue.lazyEvaluator` |
| `mW` | this domain | Bidi-isolate decoration table consumed by editor extensions |
| `uv`/`hv`/`dv`/`wv`/`cv`/`fv`/`Cv`/`ov`/`av`/`sv` | `displayTooltip` module-static | Tooltip singleton state; not exported |
| `renderMatches` | `utils` or `ui/components` | Backing implementation of `renderResults`; also called by suggesters |
| `Direction.RTL`/`LTR`, `Decoration.mark(spec)` | `vendor/codemirror/view` | bidi-isolate enum and CM6 decoration factory in `mW` |
| `MathJax.tex2chtml` / `mermaid.render` / `Prism.highlight` / `pdfjsLib.getDocument` | bundled libraries | core call sites after each respective `load*()` |
| `TFile` | `vault` | wikilink target discriminator in `renderFileLink` |
| `Menu.forEvent`, `Keymap.isModEvent`, `Notice`, `setIcon`, `setTooltip`, `getLinkpath`, `gm.interface.*` | `ui/menu`, `core`, `ui/components`, `ui/icons`, `ui/popups`, `parsing`/`metadata`, i18n bundle | context-menu, modifier detection, toast, Lucide icon, deferred tooltip, linktext stripper, i18n strings |
