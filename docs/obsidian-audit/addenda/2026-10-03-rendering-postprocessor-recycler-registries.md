# The post-processor registry has a sibling `registerRecycler` list, `codeBlockPostProcessors` is a plain object (not a `Map`), and the recycling strategy is DOM-node *transplant*

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/rendering.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/rendering.md` §4 (event table incomplete), §7 (`MarkdownPreviewRenderer.postProcessors` / `codeBlockPostProcessors: Map`), §10 (whole table), §11 (`PostProcessorRegistry`, `CodeBlockProcessorRegistry` rows), §12 bullets
**Relevant cluster plans:** `superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md` (audit-driven), any plan adding `Markoff::PostProcessorRegistry`; `docs/PARITY-MATRIX.md`

## Finding

The reading-view renderer's registries live on the module-level class `AW`, declared at `app/own/1506-TW.js:8` (whose other statics are `previewEl`, `sizerEl`, `pusherEl`, `sections`, `asyncSections`, `recycledSections`, `lastText`, `lastRender`, `lastScroll`, `viewportHeight`, `renderedWidth`, `renderExtra`, `renderExtraMinPx`, `addBottomPadding`, `topSpace`, `parsing`, `scrolling`, `workerPath`).

### CORRECTION — `codeBlockPostProcessors` is a plain object, not a `Map`, and registration *throws* on collision

§7 states: "`MarkdownPreviewRenderer.postProcessors: Function[]`, `MarkdownPreviewRenderer.codeBlockPostProcessors: Map<lang, Function>`". Verbatim:

```js
// app/own/1506-TW.js:723-743
e.registerPostProcessor = function(e, t) {
  e.sortOrder = t;
  var n = this.postProcessors;
  n.push(e), n.sort(function(e, t) { return (e.sortOrder || 0) - (t.sortOrder || 0); });
}, e.unregisterPostProcessor = function(e) { this.postProcessors.remove(e); },
e.registerRecycler = function(e) { this.recyclers.push(e); },
e.unregisterRecycler = function(e) { this.recyclers.remove(e); },
e.registerCodeBlockPostProcessor = function(e, t) {
  var n = this.codeBlockPostProcessors;
  if (n.hasOwnProperty(e)) throw new Error("Code block postprocessor for language ".concat(e, " is already registered"));
  n[e] = t;
}, e.unregisterCodeBlockPostProcessor = function(e) { delete this.codeBlockPostProcessors[e]; },
```

- `codeBlockPostProcessors` is a **plain object**, guarded with `hasOwnProperty` and deleted with `delete`. Consumers use `AW.codeBlockPostProcessors.hasOwnProperty(a)` (`app/own/1955-A3.js:87`) and `AW.codeBlockPostProcessors[a]`. Not a `Map`.
- **Double registration throws** rather than replacing. Contrast `registerPostProcessor`, which happily accepts duplicates and re-sorts.
- `unregisterPostProcessor` uses Obsidian's `Array.remove` (reference equality).

### NEW — `registerRecycler` / `unregisterRecycler`: a second, undocumented plugin-adjacent registry

```js
// app/own/1506-TW.js:731-734
e.registerRecycler = function(e) { this.recyclers.push(e); },
e.unregisterRecycler = function(e) { this.recyclers.remove(e); }
```

No `sortOrder`, no sort. This is the **section-recycling hook family** and it is absent from `rendering.md` §7, §9, §10 and §11. Four consumers exist in 1.13.7, all in the math/mermaid/image family:

| recycler | what it transplants |
|---|---|
| `app/own/1511-expressionstatement.js:2-9` | a rendered `div.mermaid` with identical source, replacing a raw `<pre><code class="language-mermaid">` |
| `app/own/1521-expressionstatement.js:2-8` | a `.math.is-loaded` node with identical source **and** identical `math-block`-ness |
| `app/own/1526-expressionstatement.js:2-10` | a loaded `<img>` with identical `src` |
| `app/own/1955-A3.js` region | (live-preview side) |

The image transplant is the most conditional:

```js
// app/own/1524-qW.js:2-7
var qW = function(e, t, n) {
  for (var i = 0, r = e; i < r.length; i++)
    for (var o = 0, a = r[i].el.findAll("img"); o < a.length; o++) {
      var s = a[o];
      if (s.instanceOf(HTMLImageElement) && s.complete && t === s.src) return s.setAttr("width", n.getAttr("width")), s.setAttr("height", n.getAttr("height")), s.setAttr("alt", n.getAttr("alt")), s;
    }
  return null;
};
```

Two contracts: the candidate must be **`complete`** (a still-loading image is never transplanted — you cannot move a node whose bitmap isn't resident), and the `width`/`height`/`alt` of the *incoming* node are **copied onto the reused node** before it is moved, so a recycled section picks up new sizing attributes.

### NEW — the DOM surgery `createCodeBlockPostProcessor` performs, and `ctx.replaceCode`

```js
// app/own/1506-TW.js:741-767  (abridged, structure preserved)
e.createCodeBlockPostProcessor = function(e, t) {
  return function(n, i) {
    var r = n.findAll("code.language-" + e);
    if (0 !== r.length)
      for (var o = function(n) {
          var r = FW(n), o = n.parentElement, a = createDiv("block-language-" + e);
          o.replaceWith(a);
          var s = i;
          s.replaceCode = function(e) {
            var t = i.getSectionInfo(a);
            if (t) {
              var o = t.text.substring(am(t.text, t.lineStart).start, am(t.text, t.lineEnd).end),
                s = parseInt(n.dataset.line, 10);
              isNaN(s) && (s = 0);
              var l = am(o, s + 1).start, c = l + r.length;
              o.substring(l, c) === r && (i.replace(o.substring(0, l) + e + o.substring(c), !0), OW.set(n, e), r = e);
            }
          };
          var l = t(r, a, s);
          l && i.promises.push(l);
        }, a = 0, s = r; a < s.length; a++) { o(s[a]); }
  };
}
```

Observable contract for a registered fenced language in the **reading view**:

- matched by `code.language-<LANG>` (so the `<pre><code class="language-x">` pair is the anchor);
- the **`<pre>` is replaced wholesale** by `<div class="block-language-<LANG>">`, and the processor is called as `fn(source, divEl, ctx)`;
- if the processor returns a thenable it is **pushed onto `ctx.promises`** — section completion waits on it. `rendering.md` §11 says "awaits `QFuture<void>` before section-complete", which is directionally right, but the mechanism is a **`promises` array on the context**, not a return-value await;
- the context gains a **`replaceCode(newSource)`** closure that rewrites the *markdown source* of the section, guarded by an exact substring check (`o.substring(l, c) === r`) and re-keyed into `OW` so `FW` returns the new text. `dataset.line` on the `<code>` gives a 0-based line offset into the section. That is the read→write path for plugins that rewrite source from a rendered view.

### NEW — the two `register*` verbs are asymmetric in what they return, and both fire `post-processor-change`

```js
// app/own/1938-c4.js:88-99   (Plugin.prototype)
registerMarkdownPostProcessor = function(e, t) {
  var n = this;
  return AW.registerPostProcessor(e, t), this.app.workspace.trigger("post-processor-change"), this.register(function() {
    AW.unregisterPostProcessor(e), n.app.workspace.trigger("post-processor-change");
  }), e;
}, registerMarkdownCodeBlockProcessor = function(e, t, n) {
  var i = this, r = AW.createCodeBlockPostProcessor(e, t);
  return AW.registerPostProcessor(r, n), AW.registerCodeBlockPostProcessor(e, t), this.app.workspace.trigger("post-processor-change"), this.register(function() {
    AW.unregisterCodeBlockPostProcessor(e), AW.unregisterPostProcessor(r), i.app.workspace.trigger("post-processor-change");
  }), r;
}
```

- `registerMarkdownPostProcessor` returns **the user's function** (`e`).
- `registerMarkdownCodeBlockProcessor` returns **the generated wrapper** (`r`), *not* the user's `t` — an API asymmetry with no doc coverage.
- A code-block registration touches **two** registries: the wrapper goes into `postProcessors[]` (with the `sortOrder`), the raw function into `codeBlockPostProcessors[lang]`. The raw entry is what the **live-preview** widget uses (`app/own/1955-A3.js:87-95`, which creates `<div class="block-language-LANG">` inside a `cm-preview-code-block` container), while the wrapper is what the **reading view** uses. That reading-view / live-preview split is the real reason both exist.
- `post-processor-change` fires on **register and unregister** for both verbs — confirming §5's one-line claim, but at `app/own/1938-c4.js:88-97`, not the doc's `plugin/Plugin.js:144/147/162/166`.

### NEW — the `gm()` loader behind `loadMathJax`/`loadMermaid`/`loadPdfJs`/`loadPrism`

All four `load*` shims and every engine in this domain await a `gm()` handle. It is fully readable, which settles §3's "whether those scripts come from a bundled asset or a CDN fetch is invisible" (they are **bundled asar assets**, injected as `<script>` into `document.body`) and adds the retry contract:

```js
// app/own/0665-gm.js:2-35
function gm(e, t) {
  var n = !1, i = null;
  return {
    get loaded() { return n; },
    get promise() {
      if (n) return Promise.resolve();
      if (!i) {
        t && t.before && t.before();
        var r = function(e, t) {
          return void 0 === t && (t = "text/javascript"), new Promise(function(n, i) {
            var r = document.createElement("script");
            r.type = t, r.src = e,
            r.addEventListener("load", function() { return n(r); }),
            r.addEventListener("error", function(e) { return i(e); }),
            document.body.appendChild(r);
          });
        }(e, null == t ? void 0 : t.type);
        r.catch(function(e) { e.detach(), i = null; });
        i = r.then(function() { n = !0, i = null, t && t.after && t.after(); });
      }
      return i;
    },
    then: function(e) { return n ? Promise.resolve(e()) : this.promise.then(e); }
  };
}
```

- Injection is triggered by the **first `.promise` or `.then` access**, not at module load.
- `type` defaults to `"text/javascript"`; PDF.js passes `type: "module"` (`app/own/1483-$q.js:4`).
- **Failure is retryable**: on rejection the `<script>` is detached, `i = null`, `loaded` stays `false`, and the next `.promise` access re-injects. A poisoned promise is never memoised — a stronger guarantee than §8 invariant 8's "idempotent and cheap on re-await".
- `before` runs before injection, `after` after load — that is how MathJax (`1490-aW.js:3`), Prism (`1488-rW.js:3-6`, which sets `window.Prism.manual = true`) and mermaid (`1486-tW.js:4`) get configured.

### NEW — Prism is manual, PDF.js is ESM, and the PDF viewer options are fixed

```js
// app/own/1488-rW.js:2-6
var rW = gm("/lib/prism.min.js", {
  before: function() { window.Prism = window.Prism || {}, window.Prism.manual = !0; }
});
```

`Prism.manual = true` — Prism never auto-highlights on DOMContentLoaded; Obsidian drives highlighting per code block. A `KSyntaxHighlighting` port has the same shape by construction, but the `manual` flag matters for the plugin-compat shim (`loadPrism`): a ported plugin calling `Prism.highlightElement` directly must not get a second, document-wide auto-highlight.

```js
// app/own/1483-$q.js:2-13
var $q = gm("/lib/pdfjs/pdf.min.mjs", {
    type: "module",
    after: function() {
      window.pdfjsLib.GlobalWorkerOptions.workerSrc = "/lib/pdfjs/pdf.worker.min.mjs", Bd(window, "pdfjsLib"), Bd(pdfjsLib.GlobalWorkerOptions, "workerSrc");
    }
  }),
  Zq = gm("/lib/pdfjs/pdf.viewer.min.mjs", { type: "module", after: function() { Bd(window, "pdfjsViewer"); } });
```

`Jq()` (`1484-Jq.js:2-13`) resolves `window.pdfjsLib`; `eW()` (`1485-eW.js:2-15`) additionally resolves **`window.pdfjsViewer`** — a second loader `rendering.md` §1 does not list.

```js
// app/own/1835-t1.js:261-283
l1 = {
  enableScripting: !1, externalLinkTarget: 2, imageResourcesPath: "/lib/pdfjs/images/",
  maxCanvasPixels: 16777216, pdfBugEnabled: !1, sidebarViewOnLoad: 0, viewerCssTheme: 0,
  disablePreferences: !1, disableHistory: !0, verbosity: 0, defaultZoomValue: "page-width",
  cMapPacked: !0, cMapUrl: "/lib/pdfjs/cmaps/", standardFontDataUrl: "/lib/pdfjs/standard_fonts/",
  wasmUrl: "/lib/pdfjs/wasm/", iccUrl: "/lib/pdfjs/iccs/", workerSrc: "/lib/pdfjs/pdf.worker.min.js",
  defaultUrl: "", disableTelemetry: !0, isEvalSupported: !1
},
```

Security-relevant defaults for any PDF port: `enableScripting: false`, **`isEvalSupported: false`** (no `eval()` in PDF content), `disableTelemetry: true`, a **16 MP canvas cap**, and `externalLinkTarget: 2` (`LinkTarget.BLANK`). Note `workerSrc` here is the legacy `.min.js` path while `1483-$q.js` sets `.min.mjs` — both assets ship. LocalStorage keys: `"pdfjs-search-settings"` (`1835-t1.js:285`, whose shape is `{highlightAll, caseSensitive, matchDiacritics, …}` from `:292-296`) and the CSS class **`"pdfjs-is-themed"`** (`app/own/1836-f1.js:519`).

### NEW — `registerDomEvents`: the delegated link handling, and `data-href` beats `href`

```js
// app/own/1506-TW.js:768-790  (abridged)
e.registerDomEvents = function(t, n, i, r) {
  function o(n) {
    var r = n.getAttr("data-href") || n.getAttr("href");
    return r && e.belongsToMe(n, t, i) ? { href: r, displayText: n.getText().trim() } : null;
  }
  function a(e, t) { if (0 === e.button || 1 === e.button) { var i = o(t); i && n.onInternalLinkClick(e, t, i.href, i.displayText); } }
  …
  t.on("click", "a.internal-link", a), t.on("auxclick", "a.internal-link", a),
  t.on("dragstart", "a.internal-link", function(e, t) { var i = o(t); i && n.onInternalLinkDrag(e, t, i.href, i.displayText); }),
```

- Delegated listeners are attached to the preview container for `click`, `auxclick`, `dragstart`, `contextmenu` on **`a.internal-link`** only.
- **`data-href` takes precedence over `href`**, and `displayText` is `getText().trim()` — not the `data-href`.
- Every hit is gated on `belongsToMe(el, previewEl, selector)`, which is how post-processors that add their own links are kept from being double-handled by per-node listeners.

### NEW — `.is-flashing`: the navigation-flash class, 3 s

```js
// app/own/1506-TW.js:655-663
e.prototype.highlightEl = function(e) {
  for (var t = 0, n = this.sections; t < n.length; t++)
    for (var i = 0, r = n[t].el.findAllSelf(".is-flashing"); i < r.length; i++) { r[i].removeClass("is-flashing"); }
  e.addClass("is-flashing"), setTimeout(function() { e.removeClass("is-flashing"); }, 3e3);
}
```

`highlightEl(el)` clears `.is-flashing` from every **section root** (`findAllSelf`) then sets it on the target for **3000 ms**. `rendering.md` lists `highlightEl` in the seed map but never documents it; the class name and the 3 s duration are the theme-visible contract.

### CONFIRMED

| §claim | verdict | source |
|---|---|---|
| `postProcessors` is `Function[]` sorted ascending by `sortOrder`, `undefined` → 0 | confirmed | `1506-TW.js:723-729` |
| `post-processor-change` fired on register **and** unregister | confirmed (different line cites) | `1938-c4.js:88-97` |
| code-block processors receive `(source, el, ctx)` and may be async | confirmed, plus `ctx.promises` and `ctx.replaceCode` | `1506-TW.js:761-766` |
| `MarkdownRenderChild.unload()` on recycle is the plugin lifecycle bridge | not contradicted, but the *renderer-side* mechanism (node transplant) is separate and undocumented | `1511`/`1519`/`1521`/`qW` |
| loaders use "bundled assets, not CDN" (§3, inferred) | confirmed | `1483`/`1486`/`1488`/`1490` paths under `/lib/` |
| `codeBlockPostProcessors: Map<lang, Function>` (§7) | **refuted** → plain object | `1506-TW.js:736-742` |
| §10 cites `editor/markdown/MarkdownPreviewRenderer.js:1167-1173` and `plugin/Plugin.js:144/147/162/166` | dead cites; real ones are `1506-TW.js:723-743` and `1938-c4.js:88-99` | — |

## Why noticed now

`rendering.md` §11 tells Corbomite to "Introduce `Markoff::PostProcessorRegistry` keyed by integer sort-order" and a separate "CodeBlockProcessorRegistry keyed by `registerLanguage`". Reading the real thing shows the registry needs a **third** list (recyclers), that the code-block registry must be **dual-registered** across two stores, and that the recycler list implies a node-reuse strategy that has no analogue in the plan. `PARITY-MATRIX.md` has no row for any of it.

## Action taken

None yet. If `Markoff::PostProcessorRegistry` is introduced it needs a sibling `Markoff::RecyclerRegistry`, and `registerCodeBlockProcessor` needs both the wrapper and the raw-function slots plus a `replaceCode` capability if plugin parity is wanted. The `Map`-vs-plain-object distinction matters only if a plugin reads the registry, which the JS API does allow.

## Unverifiable

- Where `ctx.promises` is awaited relative to section completion — `MarkdownPreviewSection` internals were not read.
- Whether `recyclers` are invoked with `(sectionEl, allSections)` in that order; the observed call shape is `(e, t)` where `e` is the section element and `t` is the iterable of sections (`1519`/`1524` both index `t` as an array of `{el}`), which is consistent but the declaring site was not read.