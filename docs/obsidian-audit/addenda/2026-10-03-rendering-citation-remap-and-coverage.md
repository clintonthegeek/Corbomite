# Citation remap, coverage verdict, and the real markdown→HTML chain (which turns out to run its own output through DOMPurify)

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/rendering.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/rendering.md` §1 header, §3, §4, §5, §6, §10, §11 (`MarkdownRenderer.render` row), §13 (Q1, Q2, Q8, Q10 status), §15 (whole short-symbol table)
**Relevant cluster plans:** `libs/core/.../MarkoffRenderEngine.{h,cpp}`, `MarkdownRenderer.{h,cpp}`, `Corbomite::RenderedDocument`; `superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md`; `docs/PARITY-MATRIX.md`

## Finding

### Coverage verdict — `rendering.md` does **not** have the `_internal.js` problem, but it has a worse one

The brief asked whether `addenda/2026-06-10-editor-timing-and-coverage-corrections.md`'s claim (that `editor.md` §14 mislocated live-preview/table-editor/vim/DnD/find-replace into unread `_internal.js` territory) applies to `rendering.md` too. It does not, and the reason is worth recording:

- **There is no `_internal.js` anywhere in the 1.13.7 corpus.** The bundles are `app/own` (2077 files), `help/own` (84), `main/own` (61), `starter/own` (161), `worker/own` (170); `enhance`, `i18n` and `sim` have no `own` statements. `ls app/own | grep -i internal` returns nothing.
- `rendering.md`'s equivalent defect is different and specific: **§13 Q1 and Q2 filed the two highest-value facts in the domain as un-extractable** ("The `EL`/`SL` configuration … lives in the bundle outside this directory and could not be inspected during this audit"; "The Turndown rule list is configured outside scope"). Both are five-to-twenty-line statements in the corpus (`1293-FF.js`, `1265-MO.js` + `1267`/`1269`/`1275`). The gap was an artefact of the Pass-2 directory slicing (`.../tree/obsidian/rendering/`, eleven tiny "public API symbol" extracts), not of the source.
- **Every citation in the doc is dead.** The header points at `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/rendering/`, which does not exist on disk. All §-internal cites are `rendering/<file>.js:<line>` and all §10/§11 cites are `editor/markdown/*.js` / `plugin/Plugin.js` paths in that same destroyed tree. Remap below.

### Remap of the load-bearing citations

| §doc cite | real 1.13.7 cite | note |
|---|---|---|
| §1 `RenderContext.js:31-153` | `app/own/1647-tK.js:28-99` | class is `nK`; the doc's 153-line window is 72 lines here |
| §1 `RenderContext.js:5-30` (bidi table `mW`) | `app/own/1647-tK.js:2-27` (table `tK`, **zero references**) / live twin `app/own/0506-Rp.js:2-27` (`Rp`) | see the callout/bidi addendum |
| §1 `RenderContext.js:154-624` (`Value` chain) | `app/own/1647-tK.js:100-385` | |
| §1 `renderMath.js:5-9` | `app/own/1492-lW.js:2-5` (`lW`) | |
| §1 `finishRenderMath.js:5-12` | `app/own/1494-dW.js:2-3` (`dW`) + `app/own/1493-cW.js:2-8` (`pW`) | |
| §1 `loadMathJax.js:5-16` | `app/own/1490-aW.js:2` (`aW = gm("/lib/mathjax/tex-chtml-full.js", …)`) | |
| §1 `loadMermaid.js:5-16` | `app/own/1486-tW.js:3-44` (`nW = gm("/lib/mermaid.min.js", …)`) | |
| §1 `loadPdfJs.js:5-16` | `app/own/1483-$q.js:2-6` (`$q`) + `app/own/1484-Jq.js:2-13` (`Jq`) | a second loader `eW` → `window.pdfjsViewer` also exists (`1485-eW.js`) |
| §1 `loadPrism.js:5-16` | `app/own/1488-rW.js:2-6` (`rW`) | |
| §1 `htmlToMarkdown.js:5-7` | `app/own/1274-IO.js:2-3` (`IO`); service `MO` at `1265-MO.js:2-9` | |
| §1 `sanitizeHTMLToDom.js:5-7` | `app/own/1294-RF.js:2-3` (`RF`); config `FF` at `1293-FF.js:2-8`; instance `LF` at `1290-LF.js:2` | |
| §1 `renderResults.js:5-7` | `app/own/0791-lb.js:2-21` (`lb`) | no `renderMatches` exists |
| §1 `displayTooltip.js` | `app/own/0735-Xg.js:2-52` (`Xg`) + `setTooltip` `app/own/0738-Zg.js:2-4` (`Zg`) | |
| §7 `MarkdownPreviewRenderer.postProcessors` | `app/own/1506-TW.js:723-729`, statics at `:823` (`AW`) | |
| §7 `codeBlockPostProcessors: Map<lang,Function>` | `app/own/1506-TW.js:736-742`; declared `= {}` at `:823` — **plain object** | |
| §10 `editor/markdown/MarkdownPreviewRenderer.js:1167-1173` | `app/own/1506-TW.js:723-769` | |
| §5/§10 `plugin/Plugin.js:144/147/162/166` | `app/own/1938-c4.js:88-97` (`Plugin.prototype.registerMarkdownPostProcessor` / `…CodeBlockProcessor`) | |
| §5 `editor/markdown/MarkdownPreviewView.js:253` | `app/own/1531-YW.js:95-176` (`MarkdownRenderer.postProcess` / `.render`) | |
| §10 `registerHoverLinkSource` (Page Preview) | `app/own/1932-r4.js:2009-2013` (`Workspace`), `:2039-vee.js:427` (consumer) | |
| §15 `bz`/`mz`/`uz`/`gz` (library deferreds) | one loader, `gm` at `app/own/0665-gm.js:2-35`; the four handles are `aW`/`nW`/`$q`/`rW` | |
| §15 `Cz`/`Ez`/`Sz` | `cW`/`uW`/`pW` at `app/own/1493-cW.js:2-4`; `dW` at `1494-dW.js:2`; no `Ez`/`Sz` | |
| §15 `EL`/`SL` | `EL` = Levenshtein helper (`1223-EL.js:2`); `SL` = CLI handler registry (`1225-SL.js:2`). Neither is DOMPurify | |
| §15 `hP` | `MO` (`1265-MO.js:2`); Turndown itself is the global `window.TurndownService` | |
| §15 `gb` (deferred factory) | `Lw` (`0822-Lw.js:2-8`) | |
| §15 `ub` (natural compare) | `Sw` (used as `sort(Sw)` / `Sw(a,b)` at `1225-SL.js:107`, `1647-tK.js:309`) | |
| §15 `zW` (lazy coercion) | `xK` (`1647-tK.js:230-232`) | |

### NEW — the real markdown→HTML chain, and it ends in DOMPurify

§11's first row is built on "`MarkdownRenderer.render(md, el, sourcePath, component)` (sibling-domain entry)". That entry point is `MarkdownRenderer` (`1531-YW`) and its body is short enough to quote:

```js
// app/own/1531-YW.js:135-176  (render, abridged)
var l = WT(t),
  c = KT(l),
  u = RF(GT(l));
n.appendChild(u);
for (var h = [], p = {
    docId: Gd(16), sourcePath: i, frontmatter: c, promises: h,
    addChild: function(e) { return r && r.addChild(e); },
    getSectionInfo: function() { return null; },
    replace: function() { return null; },
    containerEl: n, el: n, displayMode: !0
  }, d = 0, f = AW.postProcessors; d < f.length; d++) {
  var v = (0, f[d])(n, p);
  v && v.then && h.push(v);
}
```

So the pipeline is:

| step | function | what it does |
|---|---|---|
| 1 | `WT(md)` — `1082-WT.js:2-10` | `iM()(md)` (markdown-it) → `new VT(String(t), t)` with `VT.globalOptions` → `.parse()` → mdast root, then runs **`HT.transformers`** (`1082-WT.js:6`) over the tree |
| 2 | `KT(mdast)` — `1086-KT.js:2-10` | if `mdast.children[0].type === "yaml"`, JSON-parse `t.value` via `wT(value, undefined, {})` (`1086-KT.js:5`); **`catch → return null`** |
| 3 | `GT(mdast, opts)` — `1087-GT.js:2-9` | sets `opts.allowDangerousHtml = true` and `opts.handlers = qT`; `OT(e, t)` (mdast→hast), runs **`zT.transformers`**, then `JE()(n, {allowDangerousHtml: !0})` (hast→**HTML string**) |
| 4 | **`RF(html)`** — `1294-RF.js:2-3` | `document.importNode(DOMPurify.sanitize(html, FF), true)` |
| 5 | `n.appendChild(u)` | |
| 6 | `AW.postProcessors[]` | each `(el, ctx)`; a returned thenable is pushed onto `ctx.promises` |

Two consequences Corbomite cannot skip:

1. **`allowDangerousHtml: true` is set on both the mdast→hast and hast→html steps.** Raw HTML in a note survives into the HTML string intact, and **DOMPurify is the only thing that removes it**. So `sanitizeHTMLToDom` is not a plugin-only chokepoint — it is on the main render path for every note. A `MarkoffRenderEngine` that renders raw HTML without a sanitiser pass, or that never passes raw HTML through at all, diverges from Obsidian in both directions.
2. **In the one-shot `renderMarkdown` path, `ctx.getSectionInfo` and `ctx.replace` are literal `return null` stubs.** `ctx.replaceCode` therefore does nothing there; source-rewriting from a rendered view only works in the incremental section path (`1506-TW.js:752-766`). The `MarkdownPostProcessorContext` shape §10 describes is real but its capabilities are **path-dependent**.

The incremental section path adds the embed pass:

```js
// app/own/1531-YW.js:95-125  (postProcess, abridged)
for (var n = t.sourcePath, i = t.promises, r = t.el, o = t.displayMode, a = 0, s = r.findAll("code.language-query"); a < s.length; a++) {
  var l = s[a], c = FW(l).trim(), u = l.parentElement;
  t.addChild(XW(e, c, u, n));
}
for (var h = 0, p = AW.postProcessors; h < p.length; h++) { var d = (0, p[h])(r, t); d && d.then && i.push(d); }
var f = F1(t.containerEl), v = r.findAll(".internal-embed:not(.is-loaded)");
if (v.length > 0) for (…) { … w = W1.load({ app: e, linktext: b, sourcePath: n, containerEl: y, displayMode: o, showInline: !0, depth: f }); w && (t.addChild(w), i.push(w.loadFile())); }
```

Ordering is fixed: **Bases `code.language-query` embeds → post-processors → `.internal-embed:not(.is-loaded)` embeds**, with each embed's `loadFile()` promise pushed onto `ctx.promises`. `.is-loaded` is the *same* idempotence gate as `.math.is-loaded`, so embed loading is uniformly lazy-once. `F1` (`app/own/1843-F1.js:2-7`) is the **embed-depth** resolver: it walks `parentElement` until it finds a node present in the `O1` WeakMap and returns that node's recorded depth, else `0`.

### NEW — the "you forgot the Component" runtime guard

```js
// app/own/1531-YW.js:126-135
if (!(r && r instanceof mg)) {
  var o = new Error().stack;
  if ("string" == typeof o) {
    var a = o.match(/plugin:([^:]+)/);
    if (a) {
      var s = a[1];
      UW.hasOwnProperty(s) || (UW[s] = !0, console.error(new Error('Plugin "'.concat(s, '" is not passing Component in renderMarkdown. This is needed to avoid memory leaks when embedded contents register global event handlers.'))));
    }
  }
}
```

If the 4th argument is not a `Component` (`mg`), Obsidian walks the stack for `plugin:<id>`, de-duplicates per plugin via the `UW` set, and logs a **one-per-plugin** console error telling the author to pass a Component "to avoid memory leaks when embedded contents register global event handlers." §10 asserts the `addChild`/`unload()` lifecycle but never mentions that Obsidian actively detects and complains about the failure mode.

### CORRECTION — §3 "No on-disk contracts" is wrong; there are three localStorage contracts in this domain

`rendering.md` §3 says "No on-disk contracts." No *vault* file is involved (that part holds), but three `localStorage` keys are written or read by rendering code:

| key | written/read by | cite |
|---|---|---|
| `"mermaid-vault-trust"` | `saveLocalStorage` on Allow, `loadLocalStorage` as a strict `=== true` gate | `1844-R1.js:449`, `1845-_1.js:2`, `1846-K1.js:2` |
| `"pdfjs-search-settings"` | PDF viewer search settings `{highlightAll, caseSensitive, matchDiacritics, …}` | `1835-t1.js:285`, `:292-296` |
| `"MathJax-Menu-Settings"` | **`removeItem`** at MathJax load — actively cleared, not read | `1490-aW.js:35` |

The callout collapse state (`Zf`/`$f` via `Jf`, `0604-Jf.js`) is a fourth, per-callout persistence path whose key was not read.

Also §3's "whether those scripts come from a bundled asset or a CDN fetch is invisible from inside the audit scope" is answered: `/lib/mermaid.min.js`, `/lib/mathjax/tex-chtml-full.js`, `/lib/prism.min.js`, `/lib/pdfjs/pdf.min.mjs` — all bundled asar assets injected as `<script>` (`0665-gm.js:2-35`).

### CORRECTION — §4's event table is missing one event, and §6 is right for the wrong reason

§4 lists only `hover-link`. Two more exist in this domain's reach:

- **`hover-link-sources-changed`**, on `app.workspace`, triggered by `registerHoverLinkSource`/`unregisterHoverLinkSource` (`1932-r4.js:2010`, `:2012`) and consumed by Page Preview with a 100 ms debounce (`2039-vee.js:479`). (Already recorded in the workspace addendum; noted here because `rendering.md` §10's hover-source row depends on it.)
- **`post-processor-change`** *is* listed in §5 as a sibling-domain concern and is correctly attributed as register+unregister — only the line cites are dead (remapped above).

§6 "No commands registered here" is confirmed as a statement about this domain, but it is worth noting `editor:insert-mathblock` (`app/own/0754-wy.js:407`) and `editor:toggle-inline-math` (`:525`) exist and land in this domain's renderers.

### CONFIRMED

| §claim | verdict | cite |
|---|---|---|
| `Value` subclasses emit their documented class names verbatim (`bases-formula-error`, `warning-icon`, `bases-formula-error-message`, `value-list-container`, `value-list-element`, `value-list-gap`) | confirmed, all four | `1647-tK.js:141-147`, `:381-382` |
| `sanitizeHTMLToDom` deep-`importNode`s into the current document (invariant 10) | confirmed, and it is on the main render path, not just the plugin path | `1294-RF.js:3`, `1531-YW.js:138` |
| Tooltip is a single-slot mutex with a cooldown (invariant 12) | confirmed, `Bg = 100` | `0725-Og.js:6`, `0735-Xg.js:9` |
| `renderResults(el, text, null, offset?)` renders plain text | confirmed — an empty/absent match list appends a bare text node | `0791-lb.js:4-6` |
| `renderResults`' offset is added to both match ends | confirmed | `0791-lb.js:11-12` |
| §1 "no `Component` mixin on `RenderContext`; ownership is GC-only" | confirmed — the class has no `load`/`unload` and extends nothing | `1647-tK.js:28-99` |
| §1 `renderExternalLink` label falls back to the raw href | confirmed | `1647-tK.js:75` |
| §1 `renderTag` no-ops when global-search is disabled | confirmed | `1647-tK.js:95-96` |
| §8 invariant 8: the four `load*` shims are idempotent | confirmed, plus retryable-on-failure | `0665-gm.js:2-35` |
| §13 Q8 (`handleLinkContextMenu` built-in items) / Q10 (`BooleanValue` editability) | **still open** — workspace- and Bases-domain respectively; not re-derived here | — |

### Still open after this pass

`rendering.md` §13's Q8 and Q10 remain open, plus the following, none of which were resolvable from this domain's code:

- `handleLinkContextMenu` / `handleExternalLinkContextMenu` item enumeration (workspace domain).
- Whether any context makes `BooleanValue` editable (Bases domain).
- `jv`'s `Vv`/`zv`/`Hv`/`qv` constants; `yv`'s length parsing.
- The class identity of the view whose constructor registers a DOMPurify hook at `1802-rZ.js:18`.
- Whether `Zg`'s "displayed" flag ever reaches a Corbomite analogue (`ErrorValue` is the only in-corpus caller of `Xg` besides the tooltip machinery).

## Why noticed now

Every other addendum in the 2026-10-03 batch had to re-anchor dead citations before it could say anything; this file is that bookkeeping for the rendering domain, and it is where the `MarkoffRenderEngine` row in `PARITY-MATRIX.md` should be re-pointed. The `allowDangerousHtml` → `RF` finding is the substantive one: it moves "sanitisation is a plugin-HTML concern" (which is how §10 and §11 read) to "sanitisation is on the main render path", which is a different security posture and a different Corbomite work item.

## Action taken

None yet. `docs/PARITY-MATRIX.md` should gain a row for the six-step render chain and drop the "sibling domain" framing of `MarkdownRenderer.render`; `rendering.md` §3's "No on-disk contracts" needs a localStorage carve-out.

## Unverifiable

- `iM()` (the markdown-it instance) and `VT` (the mdast wrapper) — `WT`'s collaborators; the parse options in `VT.globalOptions` were not read, so the markdown dialect's option set is not claimed here.
- `JE`, `OT`, `qT`, `wT` (hast→html, mdast→hast, handlers, YAML) are vendored library bindings whose bodies are not in this corpus.
- `mg` (the `Component` base class) — used at `1531-YW.js:126` only as an `instanceof` guard.