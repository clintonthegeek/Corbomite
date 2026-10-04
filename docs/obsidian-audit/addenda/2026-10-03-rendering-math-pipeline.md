# Math is a `.math:not(.is-loaded)` post-processor, and the 100 ms batch swaps a `<style>` element and toggles `data-change` — it does not call `typeset` at all

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/rendering.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/rendering.md` §1 (`renderMath`, `finishRenderMath`), §8 invariants 7 and 9, §9 math bullet, §11 (`renderMath`, `finishRenderMath`, `loadMathJax` rows), §12 MathJax bullet, **§13 Q11** (partly answered), §15 (`bz`/`Cz`/`Ez`/`Sz` rows)
**Relevant cluster plans:** `libs/markoff/src/MathRenderer.{cpp,h}`; the plugin-API shim obligations in `rendering.md` §14 item 2

## Finding

### CORRECTION — there is no public `renderMath`/`finishRenderMath` in 1.13.7, and `renderMath`'s function is dead outside the post-processor

§1 documents two public API symbols, `renderMath(latex, displayMode)` and `finishRenderMath()`. The functions exist but are **private module bindings with zero references**:

- `renderMath` → `lW`, defined at `app/own/1492-lW.js:2-5`. The index shows exactly one `bindef` and one `decl`, both at `1492-lW.js:2` — **no call site anywhere in the corpus** except the math post-processor, which calls it as a bare identifier:

```js
// app/own/1518-expressionstatement.js:2-16  — the whole math pipeline
AW.registerPostProcessor(function(e) {
  var t = e.findAll(".math:not(.is-loaded)");
  if (0 !== t.length) return aW.then(function() {
    for (var e = 0, n = t; e < n.length; e++) {
      var i = n[e],
        r = qd(i.innerHTML);
      i.empty();
      try {
        var o = lW(r, i.classList.contains("math-block"));
        i.appendChild(o);
      } catch (e) {
        i.appendText(r);
      }
      i.addClass("is-loaded"), BW.set(i, r);
    }
    return dW();
  });
});
```

- `finishRenderMath` → the batching function is `dW` (`app/own/1494-dW.js:2-3`):

```js
function dW() {
  return cW || (cW = Lw()), clearTimeout(uW), uW = window.setTimeout(pW, 100), cW.promise;
}
```

`Lw` is the deferred factory (`app/own/0822-Lw.js:2-8`, a `{promise, resolve, reject}` object built from a `Promise`). So the *100 ms debounce* in §1/§8 invariant 7 is real, but the doc's description of its shape — "Any caller that issued `renderMath()` calls during the same tick joins the same batch: all of them await the *same* `Cz` deferred" — describes a **public per-caller API**. In 1.13.7 it is a private function called **once per section-render post-processor pass**, after all `.math` nodes in that section have been converted. There is no caller-visible batching handle.

### NEW — the batch worker never calls `MathJax.typeset*`; it swaps the CHTML stylesheet and flips `data-change`

```js
// app/own/1493-cW.js:2-8
var cW = null,
  uW = 0,
  hW = null,
  pW = function() {
    var e = MathJax.chtmlStylesheet();
    hW && e === hW || (hW && hW.detach(), hW = e, document.head.appendChild(e)),
    hW.dataset.change = "1" === hW.dataset.change ? "2" : "1",
    cW.resolve(), cW = null;
  };
```

Line by line, after the 100 ms timer fires:

1. `MathJax.chtmlStylesheet()` returns the `<style>` element carrying MathJax's **adaptive-measurement CSS rules** for the glyphs used so far.
2. If it is a *different* element from the currently-attached one, the old one is detached and the new one appended to `document.head`.
3. **`hW.dataset.change` is toggled between `"1"` and `"2"`.** That attribute is not a MathJax API — it exists to invalidate Chromium's style-resource cache so the freshly-inserted measurement CSS takes effect and previously-measured CHTML re-measures. Without it, a second batch of equations in the same view keeps the first batch's widths.
4. `cW.resolve(); cW = null;` — resolve the shared deferred and null it so the next pass allocates a fresh one.

This is only coherent because Obsidian disables MathJax's own startup typesetting:

```js
// app/own/1490-aW.js:13
startup: { typeset: !1 },
```

A grep for `typeset` / `typesetPromise` / `fontCache` over the whole corpus returns nothing outside that config. **MathJax is never asked to typeset anything**; `tex2chtml` produces already-laid-out CHTML and Obsidian only manages the stylesheet.

### NEW — display mode is read off a DOM class, and invalid LaTeX degrades to raw source text

From `1518-expressionstatement.js:9`: `lW(r, i.classList.contains("math-block"))` (`1518-expressionstatement.js:10`). So

- `$x$` → `<span class="math">` → `display: false` → `MathJax.tex2chtml(x, {display:false})`
- `$$x$$` → `<span class="math math-block">` → `display: true`

The class list is the contract, not a parameter threaded from the parser. (§8 invariant 9 — "`renderMath(src, true)` = display mode; `false` = inline" — is true of `lW` but the *observable* contract is the `math-block` class.)

The `catch` branch is the other half: `i.appendText(r)` puts the **original source string** back as a text node inside the `.math` span, then still marks it `is-loaded`. With `processEnvironments: false` and `processEscapes: false` (below), `tex2chtml` throws on malformed input rather than emitting MathJax's own error markup.

> **CORRECTION to §9:** "invalid LaTeX renders as a MathJax error span" is wrong. It renders as **the literal LaTeX source text**, with no error styling of its own.

The source is HTML-decoded first — but only four entities:

```js
// app/own/0550-Vd.js:2-7
var Vd = /&(amp|lt|gt|quot);/g,
  Hd = { "&amp;": "&", "&lt;": "<", "&gt;": ">", "&quot;": '"' };

// app/own/0551-zd.js:2-4
function zd(e) { return Hd[e]; }

// app/own/0552-qd.js:2-4
function qd(e) { return e.replace(Vd, zd); }
```

So `$a && b$` works (the `&amp;&amp;` is decoded) but a numeric entity such as `&#38;` or `&nbsp;` is **not**. `BW = mI()` (`app/own/1517-BW.js:2`) is the WeakMap holding the memoised source per element.

### NEW — recycling transplants already-typeset math nodes

```js
// app/own/1519-VW.js:2-9  (find a loaded twin)
for (var i = 0, r = e; i < r.length; i++)
  for (var o = 0, a = r[i].el.findAll(".math.is-loaded"); o < a.length; o++) {
    var s = a[o];
    if (t === BW.get(s) && s.classList.contains("math-block") === n) return s;
  }
return null;

// app/own/1521-expressionstatement.js:2-8  (the recycler)
AW.registerRecycler(function(e, t) {
  for (var n = 0, i = e.findAll(".math:not(.is-loaded)"); n < i.length; n++) {
    var r = i[n], o = qd(r.innerHTML), a = VW(t, o, r.classList.contains("math-block"));
    a && (a.detach(), r.replaceWith(a));
  }
});
```

When a reading-view section is recycled, every not-yet-loaded `.math` node is replaced by an **existing, already-typeset node with identical source and identical block-ness**, by detaching the found node and moving it. The transplant avoids re-running `tex2chtml` for equations the user has already scrolled past. `rendering.md` §10 discusses `MarkdownRenderChild.unload()` as the recycling lifecycle but never mentions node transplantation — see the registries addendum, where the same pattern is shown for mermaid and images.

### NEW — the full MathJax configuration

```js
// app/own/1490-aW.js:4-35
window.MathJax = {
  tex: { inlineMath: [], displayMath: [], processEscapes: !1, processEnvironments: !1, processRefs: !1 },
  startup: { typeset: !1 },
  options: {
    enableMenu: !1,
    menuOptions: { settings: { renderer: "CHTML" } },
    renderActions: { assistiveMml: [] },
    safeOptions: { safeProtocols: { http: !0, https: !0, file: !0, javascript: !1, data: !1 } }
  }
}, window.publish && delete window.MathJax.options.renderActions, localStorage.removeItem("MathJax-Menu-Settings");
```

Points that matter for parity:

- **`inlineMath`/`displayMath` are both empty arrays.** Obsidian does not let MathJax find delimiters; `$`/`$$` are tokenised by Obsidian's own markdown layer (the same layer that emits `.math` / `.math-block`, cf. `app/own/0506-Rp.js:55-58` where the inline formatting table maps `math` → `{nodeType: "math", surroundingChars: "$"}`). A Corbomite port must not hand delimiter discovery to its math engine.
- `processEscapes`, `processEnvironments`, `processRefs` all **false** — `\$`, `\begin{}`, `\ref{}` are not interpreted by MathJax.
- `renderActions.assistiveMml: []` — **no MathML assistive output is generated**; a screen reader sees the CHTML only.
- **`safeOptions.safeProtocols` is a real scheme allowlist** — `{http, https, file: true; javascript, data: false}` — for `\href{}`/`\url{}` inside math. This is *not* the same thing as `renderExternalLink`'s guard (`$v`, which only tests `new URL()` parseability; see the RenderContext addendum).
- `localStorage.removeItem("MathJax-Menu-Settings")` runs on load, and in Publish mode `options.renderActions` is deleted outright.
- The library is loaded from `/lib/mathjax/tex-chtml-full.js` as a **classic script** (`gm("/lib/mathjax/tex-chtml-full.js", {before: …})`, `1490-aW.js:2`).

### CONFIRMED / CORRECTED small items

| §claim | verdict | source |
|---|---|---|
| 100 ms debounce batching (§8 invariant 7) | confirmed, but private and called once per section pass | `1494-dW.js:2-3` |
| `renderMath` is synchronous and returns a DOM node | confirmed | `1492-lW.js:2-5` |
| `loadMathJax()` awaits a deferred, idempotent | confirmed — `aW` is a `gm()` handle (§see the loader addendum) | `1490-aW.js:2` |
| "invalid LaTeX renders as a MathJax error span" (§9) | **refuted** → literal source text | `1518-expressionstatement.js:12-13` |
| "MathJax nodes that need to be `finishRenderMath`-typeset before they become visually correct" (§1) | **refuted** → nothing is typeset; a CHTML stylesheet swap + `data-change` toggle happens instead | `1493-cW.js:4-8` |
| §13 Q11 "bundled MathJax version" | still open (the file is an asar asset, not a corpus statement) | — |
| `ErrorValue.renderTo` uses the "default delay constant `sv`" | `sv` is `Rg = 300` ms | `1647-tK.js:146`, `0725-Og.js:4` |

## Why noticed now

`rendering.md` §11 maps `renderMath`/`finishRenderMath`/`loadMathJax` onto JKQTMathText and calls the whole thing "functional parity without batching". That conclusion was drawn from a doc that describes a `tex2chtml` + `typeset` pipeline. 1.13.7 does not run a typeset pass at all, and the `is-loaded` / `math-block` / `.math` class contract plus the `data-change` stylesheet trick are the parts a Qt port actually has to reproduce to get sizing and incremental-append behaviour right.

## Action taken

None yet. If Corbomite's reading view ever mirrors the batch contract, the equivalent of `data-change` is a stylesheet-invalidation problem (Qt: invalidate the layout of just-typeset blocks, not the whole document), and the equivalent of the recycler transplant is an equation cache keyed on `(source, block-ness)`.

## Unverifiable

- Whether `MathJax.chtmlStylesheet()` returns a new element per call or a cached one — the code's `e === hW` guard handles both, and MathJax internals are not in the corpus.
- MathJax's own version.