# Mermaid is behind a **per-vault trust gate** whose "Allow" button re-runs every markdown post-processor; the rendered SVG gets marker-IDs namespaced and wikilinks grafted on

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/rendering.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/rendering.md` §1 (`loadMermaid`), §8 invariant 8, §9 mermaid bullet, §11 (`loadMermaid` row), §12 mermaid bullet, §13 Q11 (partly answered)
**Relevant cluster plans:** `libs/mmdr/` Rust mermaid bridge; `superpowers/plans/archive/2026-08-15-cluster-k-markoff-canvas-adoption.md` (adjacent render-parity work)

## Finding

### NEW — a whole rendering *feature* is missing from the doc: the mermaid trust gate

Untrusted mermaid does not render an error and does not render nothing — it renders a **guarded source view with an Allow button**, and granting trust fires a workspace-wide re-render.

```js
// app/own/1844-R1.js:449
U1 = "mermaid-vault-trust";
```

```js
// app/own/1845-_1.js:2-4   — the predicate
function _1(e) { return !0 === e.loadLocalStorage(U1); }
```

```js
// app/own/1846-K1.js:2-4   — the grant
function K1(e, t) {
  e.saveLocalStorage(U1, t || null), e.workspace.trigger("post-processor-change");
}
```

`_1` requires **strictly `true`** — `K1(app, false)` writes `false` and also revokes, and `K1(app, false)`'s `t || null` writes `null`. The check is `=== true`, not truthiness.

```js
// app/own/1847-G1.js:2-43   — G1(container, source, ctx)
if (_1(e)) ! function(e, t) {
  var n = this;
  km(e, function() {
    return y(n, void 0, void 0, function() { … return n = e.createEl("pre"), (i = n.createEl("code", "language-mermaid")).setText(t), [4, IW(e, [i])]; … });
  });
}(t, n);
else {
  var i = t.createDiv("mermaid-wrapper is-guarded"),
    r = i.createDiv("mermaid-guard-header");
  r.createDiv("mermaid-guard-text", function(e) {
      e.createDiv({ cls: "mermaid-guard-title",       text: bd.plugins.mermaid.labelGuardTitle() }),
      e.createDiv({ cls: "mermaid-guard-description", text: bd.plugins.mermaid.labelGuardDesc() });
    }),
    r.createDiv("mermaid-guard-actions", function(t) {
      t.createEl("button", { text: bd.dialogue.buttonAllow() }, function(t) {
        t.addEventListener("click", function() { return K1(e, !0); });
      });
    }),
    function(e, t) { e.createEl("pre").createEl("code", "language-mermaid").setText(t); }(i.createDiv("mermaid-guard-source"), n);
}
```

Exact untrusted DOM:

```html
<div class="mermaid-wrapper is-guarded">
  <div class="mermaid-guard-header">
    <div class="mermaid-guard-text">
      <div class="mermaid-guard-title">…labelGuardTitle()</div>
      <div class="mermaid-guard-description">…labelGuardDesc()</div>
    </div>
    <div class="mermaid-guard-actions"><button>…buttonAllow()</button></div>
  </div>
  <div class="mermaid-guard-source"><pre><code class="language-mermaid">SOURCE</code></pre></div>
</div>
```

Three contract points:

1. **The raw source is always in the DOM**, inside `<pre><code class="language-mermaid">`, trusted or not. Only the rendering differs.
2. Clicking Allow calls `K1(app, true)` which **triggers `post-processor-change`** — i.e. every markdown post-processor in every reading view and live preview re-runs. That is the only re-render mechanism; there is no targeted invalidation.
3. `main/own/0033-Ke.js:1682` carries the gate's *prompt* string (`"Display Mermaid diagrams in this vault?"`) in the main-process settings/menu bundle, so the trust decision is also surfaced outside the renderer.

### NEW — the render pipeline is a post-SVG DOM-surgery pass, not just `mermaid.render(id, source)`

```js
// app/own/1508-PW.js  — PW(container, codeEl)
n = FW(t), i = t.parentElement, r = t.doc,
0 === (o = i.clientWidth) && (o = e.clientWidth), 0 === o && (o = e.win.innerWidth),
a = "m" + Gd(16), s = document.body.createDiv("mermaid"), …
s.style.width = o + "px", s.style.visibility = "hidden", s.style.position = "absolute",
[4, mermaid.render(a, n, s)];
… l = b.sent().svg, s.detach(),
(c = createDiv("mermaid")).innerHTML = l, i.replaceWith(c), OW.set(c, n),
(u = c).instanceOf(Element) && …
```

Four behaviours Corbomite has to reproduce:

1. **Width selection is `<pre>`-first**: `t.parentElement.clientWidth`, falling back to the view's `clientWidth`, then `window.innerWidth`. The diagram is laid out in an **absolutely-positioned, `visibility:hidden` `div.mermaid` appended to `document.body`** at that explicit pixel width. This is why Obsidian can set `useMaxWidth` differently per diagram type and still get correct sizing.
2. **The SVG is injected with `innerHTML`, not through the sanitiser.** `RF`/`DOMPurify` is not involved. Mermaid's output is trusted by construction (`securityLevel: "strict"`, below).
3. **`SVGAElement` href re-arming**: `g.instanceOf(SVGAElement) && g.href && !g.href.baseVal && (g.href.baseVal = g.getAttr("href"))` — Chromium drops the `SVGAnimatedString` reflection on `innerHTML` assignment, so every `<a>` inside the SVG is re-pointed at its own `href` attribute.
4. **Marker-ID namespacing** — the part that silently breaks arrowheads when omitted:

```js
if (y = u.find("style")) {
  for (w = y.innerHTML, k = w, C = 0, E = u.findAll("marker"); C < E.length; C++)
    if (M = E[C], S = M.id) {
      for (x = a + "-" + S, M.id = x, T = 0, D = ["marker-start", "marker-end"]; T < D.length; T++)
        for (A = D[T], P = 0, I = u.findAll("[".concat(A, '="url(#').concat(S, ')"]')); P < I.length; P++)(O = I[P]).setAttr(A, "url(#".concat(x, ")"));
      w = w.replace(new RegExp("#".concat(jd(S), "(?!\\w|-)"), "g"), "#" + x);
    }
  w !== k && (y.innerHTML = w);
}
```

Each `<marker>`'s `id` is rewritten from `S` to `"m<random>" + "-" + S`; every `marker-start`/`marker-end` attribute matching `url(#S)` is rewritten; and the inline `<style>` text gets a regex replace of `#S` bounded by `(?!\w|-)` (`jd` is the regex escaper, `app/own/0554-jd.js:2-4`). Without this, two diagrams on one page with the same marker id cross-wire their arrowheads.

5. **Wikilinks inside mermaid become clickable**: `u.findAll('[*|class*="internal-link"] > g.label foreignObject > div')` — for each such div with a `textContent`, the div is emptied and replaced with `<a class="internal-link" href="{text}">{text}</a>`. A mermaid node whose label text matches a wikilink (via mermaid's `classDef`/`class` with `internal-link`) renders as a real Obsidian internal link.

**Error path — CORRECTION to §9.** "invalid diagrams render Mermaid's error pane" is wrong:

```js
L = b.sent(), (O = r.getElementById("d" + a)) && O.detach(), s.detach(), console.error(L),
t.empty(), t.appendText("Error parsing Mermaid diagram!\n\n".concat(L.str || L.message));
```

The message is `console.error`'d, mermaid's own error SVG (`#d<id>`) is removed, and the `<code>` element is filled with the **plain text** `"Error parsing Mermaid diagram!\n\n" + (err.str || err.message)`.

### NEW — laziness, memoisation and dedup

```js
// app/own/1509-IW.js:2-24   — IW(container, codeEls); awaits nW (the mermaid module)
i = function(t) { t.isShown() || e.isShown() ? n.push(PW(e, t)) : t.onNodeInserted(function() { return PW(e, t); }, !0); }
```

A code element that is neither itself shown nor inside a shown container is **deferred via `onNodeInserted`** — mermaid is not rendered for off-screen sections until they are inserted.

```js
// app/own/1513-FW.js:2-6   — cached source text
function FW(e) {
  if (OW.has(e)) return OW.get(e);
  var t = e.getText();
  return t.endsWith("\n") && (t = t.substring(0, t.length - 1)), OW.set(e, t), t;
}
```

`OW = mI()` is a **WeakMap** (`app/own/1512-OW.js:2`). `FW` strips **exactly one** trailing newline, so `graph\n` and `graph` are the same cache key and render identically.

```js
// app/own/1510-LW.js:2-6   — find an already-rendered twin
for (var n = 0, i = e; n < i.length; n++)
  for (var r = 0, o = i[n].el.findAll(".mermaid"); r < o.length; r++) {
    var a = o[r];
    if (t === FW(a)) return a;
  }
return null;
```

Used by the recycler (`app/own/1511-expressionstatement.js:2-9`), which detaches the found `.mermaid` node and replaces the raw `<pre>` in the recycled section with it — the same node-transplant pattern as math (see the math addendum). The reading-view post-processor is `app/own/1797-expressionstatement.js:2-5`; the live-preview path goes through `G1` instead (`app/own/1955-A3.js:85`, which **`detaches the `<pre>`** and hands the bare container to `G1`).

### NEW — the init config, including a per-diagram-type `useMaxWidth` table and a Monday-only gantt axis

```js
// app/own/1486-tW.js:3-44
nW = gm("/lib/mermaid.min.js", {
  after: function() {
    mermaid.initialize({
      startOnLoad: !1,
      securityLevel: "strict",
      themeVariables: { fontFamily: "var(--font-mermaid)" },
      flowchart: { useMaxWidth: !1 },
      sequence:  { useMaxWidth: !1 },
      gantt:     { useMaxWidth: !0, axisFormatter: [["%Y-%m-%d", function(e) { return 1 === e.getDay(); }]] },
      journey:   { useMaxWidth: !0 },
      class:     { useMaxWidth: !0 },
      git:       { useMaxWidth: !1 },
      state:     { useMaxWidth: !0 },
      er:        { useMaxWidth: !1 },
      pie:       { useMaxWidth: !0 }
    });
  }
});
```

| diagram type | `useMaxWidth` |
|---|---|
| `flowchart`, `sequence`, `git`, `er` | `false` (honours the explicit pixel width `PW` sets) |
| `gantt`, `journey`, `class`, `state`, `pie` | `true` (shrinks to fit the container) |

`gantt.axisFormatter` returns `true` only when `getDay() === 1`, i.e. **date-axis ticks are drawn only on Mondays**.

`startOnLoad: false` + `securityLevel: "strict"` are what make the trust gate meaningful: mermaid's HTML labels are inert, and Obsidian separately re-gates the whole thing behind the vault flag.

### CONFIRMED — the documented local mermaid patch is real and applied

`rendering.md` does not mention it; the audit brief does. The bundle header states it, and the patched comparison is present in the shipped file:

```
// RE/obsidian/1.13.7/asar/obsidian/lib/mermaid.min.js:1-2
// Source: https://unpkg.com/mermaid@11.13.0/dist/mermaid.min.js
// Manual patch: Search for `"break-spaces"`, look behind and replace `return f.width===r&&` with `return Math.round(f.width)===Math.round(r)&&`.
```

Verified applied — the shipped minified source contains

```js
…let d=l.node().getBoundingClientRect();return Math.round(d.width)===Math.round(r)&&(l.style("display","table"),l.style("white-space","break-spaces"),l.style("width",r+"px"),…
```

(the minifier renamed the header's `f` to `d`). The patch makes the label-width equality test **rounding-tolerant**, which stops sub-pixel float jitter from re-flowing `break-spaces` labels in sequence diagrams. It is a **single-comparison change**; it is not a fork, so mermaid 11.13.0 behaviour otherwise matches upstream. Version **11.13.0** is thereby confirmed, answering half of §13 Q11.

### CONFIRMED

| §claim | verdict | source |
|---|---|---|
| `loadMermaid()` awaits a deferred and returns the library | confirmed — `nW` is a `gm()` handle | `1486-tW.js:3` |
| lazy load is idempotent and cheap on re-await (§8 invariant 8) | confirmed, plus the failure path is *retryable* (see the registries addendum) | `0665-gm.js:2-35` |
| Mermaid renders as SVG; error pane on failure (§9) | **refuted on the failure half** → plain text in the `<code>` | `1508-PW.js:31` |
| "`mermaid.render(id, source)`" (§1) | correct call, but ~40 lines of post-processing follow | `1508-PW.js:13-30` |

## Why noticed now

Corbomite reaches mermaid through `libs/mmdr/`, a static Rust renderer, and `rendering.md` §11 flags "KDE colour-scheme theme integration, error-pane rendering for invalid diagrams" as things to verify. Both are now answerable, and both of Obsidian's answers are the *non-obvious* option: the trust gate plus `post-processor-change`, and plain-text errors. The marker-ID namespacing in particular is invisible until two diagrams share a page and Corbomite's SVG output does not do it.

## Action taken

None yet. The `mermaid-vault-trust` localStorage flag is a **new on-disk/localStorage contract** for a domain doc whose §3 says "No on-disk contracts" — it should be listed as such, alongside the equivalent for Corbomite's own renderers if it adopts the gate.

### NEW — the container carries an `is-loading` class for the whole async render

`G1`'s trusted branch wraps its body in `km(containerEl, fn)`, which is a promise wrapper that adds and removes a single CSS class — no throttling, no idle-callback:

```js
// app/own/0669-km.js:2-14
function km(e, t) {
  return y(this, void 0, Promise, function() {
    var n;
    return b(this, function(i) {
      switch (i.label) {
        case 0: e.addClass("is-loading"), i.label = 1;
        case 1: return i.trys.push([1, , 3, 4]), [4, t()];
        case 2: return n = i.sent(), [3, 4];
        case 3: return e.removeClass("is-loading"), [7];
        case 4: return [2, n];
      }
    });
  });
}
```

So the observable class contract is `.is-loading` on the container (the live-preview widget's `cm-embed-block` div, `app/own/1955-A3.js:82`) for the whole duration, including the `await nW` on the mermaid module. A theme or plugin can key off it, and so must a Qt port's placeholder styling.

## Unverifiable

- Whether `gm()`'s `before`/`after` hooks (and therefore `mermaid.initialize`) run once or per injection — the loader memoises on `loaded`, so once per successful load; see the registries addendum for the verbatim loader.