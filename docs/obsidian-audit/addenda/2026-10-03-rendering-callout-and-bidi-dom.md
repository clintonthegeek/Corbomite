# Callout DOM contract (4 attributes, a 3→6 hex expansion, and three icon forms), plus reading-view RTL that is *per table cell* — not the per-paragraph-only gap §11 claims

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/rendering.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/rendering.md` §8 (no callout/bidi invariants), §9 (no callout bullet), §11 rows `Callout chrome` and `Bidi mW.rtl/.ltr/.auto cm-iso decorations`, §12 "Callout chrome" bullet
**Relevant cluster plans:** `libs/markoff/src/MarkdownTextItem.cpp` (callout parsing); `superpowers/plans/2026-08-15-cluster-k-markoff-canvas-adoption.md` (callouts frozen on Markoff E3 — this file records what E3 has to match); `docs/PARITY-MATRIX.md`

## Finding

### NEW — the callout DOM, verbatim, with the full attribute set

Callouts are **not** built by the renderer from a blockquote AST directly; they are produced by a remark-style *transformer* that rewrites `blockquote` nodes carrying a `callout` property, then three node-type builders emit the DOM:

```js
// app/own/1083-expressionstatement.js:705-737
t.callout = function(e, t) {
  var n = t.callout,
    i = n.color,
    r = null;
  return i && (i = i.replace(/^#([a-f\d])([a-f\d])([a-f\d])$/i, function(e, t, n, i) {
    return "#" + t + t + n + n + i + i;
  }), /^#[a-f\d]{6}$/i.test(i) && (r = "--callout-color: " + i + ";")), e(t, "div", {
    className: "callout",
    dataCallout: n.type,
    dataCalloutIcon: n.icon,
    dataCalloutFold: n.fold,
    dataCalloutMetadata: n.data,
    style: r
  }, TT(e, t));
}, t["callout-title"] = function(e, t) {
  return e(t, "div", { className: "callout-title" }, [
    e({}, "div", { className: "callout-icon" }),
    e({}, "div", { className: "callout-title-inner" }, TT(e, t))
  ]);
}, t["callout-content"] = function(e, t) {
  return e(t, "div", { className: "callout-content" }, DT(TT(e, t), !0));
}, e.addTransformer(function(e) {
  NE(e, "blockquote", function(e) { var t = e.callout; if (t) { e.type = "callout";
```

Resulting shape:

```html
<div class="callout"
     data-callout="TYPE"
     data-callout-icon="ICON"
     data-callout-fold="FOLD"
     data-callout-metadata="DATA"
     style="--callout-color: #RRGGBB;">
  <div class="callout-title">
    <div class="callout-icon"></div>
    <div class="callout-title-inner">TITLE</div>
  </div>
  <div class="callout-content">…</div>
</div>
```

Exact rules:

- **`--callout-color` is set only for a 6-digit hex.** A 3-digit hex is first expanded (`#abc` → `#aabbcc`), then the 6-digit test runs; anything else (named colours, `rgb()`, 8-digit) yields `r = null` → **no inline style at all**, so the callout falls back to the `data-callout` type colour.
- `data-callout-metadata` carries the callout's metadata payload as a string.
- `callout-icon` is the **first** child of `callout-title`, before `callout-title-inner`. (The patch-notes/changelog callout at `app/own/1923-D6.js:97-116` uses a *different* order — `callout-title-inner` then `callout-fold`, and no `callout-icon` — so a theme cannot assume one order globally.)
- `callout-content`'s children go through `DT(…, true)`, i.e. a nested transform pass.
- `rendering.md` §11's action item — "Verify Qt-side class parity by rendering `> [!info]` test note" — is now unnecessary as a *discovery* exercise: the class names, the attribute names and the colour rule are all fixed.

### NEW — the fold/collapse post-processor, including a click race that only a `setTimeout` can win

```js
// app/own/1526-expressionstatement.js:11-56  (abridged, structure preserved)
AW.registerPostProcessor(function(e, t) {
  for (var n = function(e) {
      ! function(e) {
        var t = e.getAttr("data-callout-fold");
        if ("+" === t || "-" === t) {
          e.addClass("is-collapsible");
          var n = e.find(".callout-title"), i = e.find(".callout-content");
          if (n && i && !n.find(".callout-fold")) {
            var r = !1,
              o = function() { return a(!r, !0); },
              a = function(t, n) { r = t, e.toggleClass("is-collapsed", r), s.toggleClass("is-collapsed", r), Jf(i, r, n); },
              s = n.createDiv("callout-fold");
            s.addEventListener("mousedown", function(e) { return e.preventDefault(); }),
            s.addEventListener("click", function(e) { 0 === e.button && (e.preventDefault(), o()); }),
            Ag(s, "lucide-chevron-down"),
            n.addEventListener("mousedown", function(e) { e.detail > 1 && e.preventDefault(); }),
            n.addEventListener("click", function(e) {
              setTimeout(function() { 0 !== e.button || e.defaultPrevented || (e.preventDefault(), o()); }, 0);
            }),
            "-" === t && a(!0, !1);
          }
        }
      }(e);
      …
```

Contracts:

- Collapsibility is gated on `data-callout-fold` being **exactly `"+"` or `"-"`** — any other value (including a number, `"true"`, or absent) means non-collapsible. So the fold marker is `+`/`-` in the source, not a boolean.
- `is-collapsible` goes on the callout root; `.callout-fold` is created **inside `.callout-title`**, icon **`lucide-chevron-down`**, and gets its own `mousedown` `preventDefault` plus a button-0-only click.
- `is-collapsed` is toggled on **both** the callout root **and** the `.callout-fold` button — two elements carry the class.
- The title's own click handler defers through `setTimeout(…, 0)` and then re-checks `e.defaultPrevented`. That is deliberate: a link or button inside a callout title calls `preventDefault()` synchronously and thereby wins the race instead of the title collapsing underneath it. Any Qt port that toggles on the title's press handler must reproduce the deferral or it will collapse when the user clicks a link in the title.
- The title's `mousedown` handler checks `e.detail > 1` — i.e. it suppresses only **double**-click (to stop text selection), not single click.
- `"-"` means *start collapsed* and calls `a(true, false)` — **`persist = false`**, so merely rendering a `-` callout does not overwrite the user's stored collapse state.

The toggle itself:

```js
// app/own/0604-Jf.js:2-16
function Jf(e, t, n) {
  return y(this, void 0, Promise, function() {
    return b(this, function(i) {
      switch (i.label) {
        case 0: return Yf(e), t ? n ? [4, Zf(e)] : [3, 2] : [3, 3];
        case 1: i.sent(), i.label = 2;
        case 2: return e.hide(), [3, 5];
        case 3: return e.show(), n ? [4, $f(e)] : [3, 5];
        case 4: i.sent(), i.label = 5;
        case 5: return [2];
      }
    });
  });
}
```

Collapse/expand uses Obsidian's `Component.hide()`/`show()` on `.callout-content` — **not** a CSS-only toggle — with asynchronous persistence (`Zf`/`$f`) only when `persist` is set. So the observable state is both the `is-collapsed` classes *and* the content element's visibility.

### NEW — the callout icon accepts three distinct forms, and inline SVG is sanitised

```js
// app/own/1525-WW.js:2-16
function WW(e, t) {
  if (t.startsWith("'") && t.endsWith("'")) t = t.substring(1, t.length - 1).replace(/\\'/g, "'");
  else if (t.startsWith('"')) try { var n = JSON.parse(t); String.isString(n) && (t = n); } catch (e) {}
  if (t.startsWith("<svg")) {
    var i = RF(t).firstChild;
    i && i.instanceOf(SVGSVGElement) && (i.setAttrs({ width: 16, height: 16, fill: "currentColor", stroke: "currentColor" }), e.empty(), e.appendChild(i));
  } else Ag(e, t);
}
```

Resolution order, from the `data-callout-icon` attribute then the `--callout-icon` CSS custom property (`:42-53`):

1. `'lucide-name'` — single-quoted, `\'` unescaped;
2. `"lucide-name"` — JSON-parsed, used only if the parse yields a string;
3. a value **starting with `<svg`** — sanitised through the DOMPurify chokepoint `RF` (see the sanitiser addendum), forced to `16×16`, `fill:currentColor`, `stroke:currentColor`, and the icon div emptied first;
4. otherwise — `setIcon` with the value treated as a **Lucide icon name**.

Also from `:42-53`: if `.callout-icon` already has a child the pass returns early, and if the section is not currently shown the icon fill is deferred via `onNodeInserted` (`1526-expressionstatement.js:50`) so `--callout-icon` is read **after** insertion (theme may not have applied yet).

### CORRECTION — reading view sets `dir` per table cell, so §11's bidi row is wrong

§11's `Bidi mW.rtl/.ltr/.auto cm-iso decorations` row reads:

> `_N/A_` | Missing | Qt inherits `QTextOption::TextDirection` per-paragraph only; no per-inline isolate spans. Mixed-script notes diverge visually.

There is a third bidi mechanism the doc missed entirely, and it *is* per-node:

```js
// app/own/1526-expressionstatement.js:57-69
AW.registerPostProcessor(function(e, t) {
  for (var n = 0, i = e.findAll("table"); n < i.length; n++) {
    var r = i[n], o = r.parentNode;
    if (o && o.instanceOf(HTMLElement) && r === o.firstChild && r === o.lastChild) {
      o.style.overflowX = "auto";
      for (var a = r.findAll("th, td"), s = 0, l = a.length; s < l; s++) { var c = a[s], u = ag(c); c.dir = u, 0 === s && (o.dir = u); }
    }
  }
});
```

Two contracts in one pass:

1. **Horizontal scrolling**: the *parent* gets `style.overflowX = "auto"`, and only when the table is the **sole child** of that parent (`firstChild === lastChild`). A table followed by a paragraph does not get a scroll container.
2. **`dir` is written on every `th`/`td` individually**, and the first cell's value is also written on the scroll parent. The value comes from first-strong-character detection over the cell's text nodes:

```js
// app/own/0704-ag.js:2-8
function ag(e) {
  for (var t, n = e.doc.createNodeIterator(e, NodeFilter.SHOW_TEXT); t = n.nextNode();) {
    var i = om(t.nodeValue);
    if ("auto" !== i) return i;
  }
  return "auto";
}
```

```js
// app/own/0653-om.js:2-7
function om(e) {
  var t = e.match(rm);
  if (t) {
    if (t[1]) return "rtl";
    if (t[2]) return "ltr";
  }
  return "auto";
}
```

`ag` walks the cell's text nodes and returns the **first non-`"auto"`** result, else `"auto"`. `om` runs one alternation regex:

```js
// app/own/0652-rm.js:2
var rm = /(?:([‏\p{sc=Arabic}\p{sc=Hebrew}\p{sc=Syriac}\p{sc=Thaana}])|([‎\p{sc=Armenian}\p{sc=Bengali}\p{sc=Bopomofo}\p{sc=Braille}\p{sc=Buhid}\p{sc=Canadian_Aboriginal}\p{sc=Cherokee}\p{sc=Cyrillic}\p{sc=Devanagari}\p{sc=Ethiopic}\p{sc=Georgian}\p{sc=Greek}\p{sc=Gujarati}\p{sc=Gurmukhi}\p{sc=Han}\p{sc=Hangul}\p{sc=Hanunoo}\p{sc=Hiragana}\p{sc=Kannada}\p{sc=Katakana}\p{sc=Khmer}\p{sc=Lao}\p{sc=Latin}\p{sc=Limbu}\p{sc=Malayalam}\p{sc=Mongolian}\p{sc=Myanmar}\p{sc=Ogham}\p{sc=Oriya}\p{sc=Runic}\p{sc=Sinhala}\p{sc=Tagalog}\p{sc=Tagbanwa}\p{sc=Tamil}\p{sc=Telugu}\p{sc=Thai}\p{sc=Tibetan}\p{sc=Yi}]))/u;
```

(group 1 = `U+200F` + Arabic/Hebrew/Syriac/Thaana ⇒ `rtl`; group 2 = `U+200E` + the 41 listed LTR scripts ⇒ `ltr`.)

**This is not the Unicode Bidirectional Algorithm.** Because only strong RTL/LTR characters are in the regex, a cell whose first strong character is preceded by digits, punctuation or whitespace-only text yields `"auto"`, not the direction of the first strong character found later in the cell:

| cell text | `ag` result | UBA would give |
|---|---|---|
| `שלום` | `rtl` | `rtl` |
| `hello שלום` | `ltr` | `ltr` |
| `2024 שלום` | **`auto`** | `rtl` |
| `(שלום)` | **`auto`** | `rtl` |
| `123` | `auto` | `auto` |

A Qt port that uses `QTextOption`/`QTextDocument` auto-detection will get UBA behaviour and will therefore render mixed tables differently from Obsidian on exactly the digit-prefixed and bracket-wrapped cases. That is a rendering divergence, and it is *cheap* to match: implement `rm` + the two-step walk verbatim.

### CORRECTION — the `cm-iso` decoration table the doc documents is dead code

§1 and §7 present `mW.rtl/.ltr/.auto` as a live table "consumed by the editor extensions". In 1.13.7 the table is `tK` at `app/own/1647-tK.js:2-27` and it has **zero references** — `grep -rno "tK\.\(rtl\|ltr\|auto\)"` over `RE/obsidian/1.13.7/renamed/src/` returns nothing.

The live table is a **byte-identical copy** at `app/own/0506-Rp.js:2-27` (same `class: "cm-iso"`, same `inclusive: !0`, same `bidiIsolate: Rn.RTL/LTR/null`, same `dir` attributes). The CSS that makes it work is one rule:

```js
// app/own/0264-Ko.js:133-135
".cm-iso": { unicodeBidi: "isolate" }
```

So the *behaviour* §1 describes is real (live preview wraps per-direction inline runs in `<span class="cm-iso" dir="…">` with a real bidi isolate), but the doc documented a duplicate, and its §7 "Read by: the editor-extension code" claim is unsupported for the specific symbol named. The same file also carries the inline formatting-mark table:

```js
// app/own/0506-Rp.js:28-60
  Np = {
  bold:         { nodeType: "strong",      surroundingChars: "**", altSurroundingChars: "__" },
  italic:       { nodeType: "em",          surroundingChars: "*",  altSurroundingChars: "_" },
  code:         { nodeType: "inline-code", surroundingChars: "`" },
  highlight:    { nodeType: "highlight",   surroundingChars: "==" },
  strikethrough:{ nodeType: "strikethrough", surroundingChars: "~~" },
  comment:      { nodeType: "comment",     surroundingChars: "%%", isBlock: !0 },
  math:         { nodeType: "math",        surroundingChars: "$" }
};
```

`nodeType` is the exact CSS class each inline mark renders as, and `math` → `math` is where the `.math` element the math post-processor looks for comes from.

## Why noticed now

Callouts are frozen on Markoff E3 per the project banner, so "verify by rendering a test note" has been the standing action item and cannot be checked without a running Obsidian. These are now five files' worth of source. The bidi row in §11, by contrast, has been read as a known-accepted divergence on a technicality ("Qt is per-paragraph") that turns out to misdescribe Obsidian: reading view does per-cell direction, and its detector is a specific non-UBA regex.

## Action taken

None yet. `libs/markoff`'s callout model should gain the four `data-callout-*` attributes (they are observable and theme-addressable) and the three-form icon resolution; the `+`/`-` fold marker and the deferred title click should be reproduced verbatim in `MarkdownTextItem.cpp`'s consumer. The `rm`-based direction detector is a ~10-line function to add wherever per-node direction is decided.

## Unverifiable

- `Ag` (`app/own/0721-Ag.js`) is `setIcon`; its body was not read, so the exact Lucide-name resolution/fallback behaviour is not claimed.
- The i18n strings behind `bd.plugins.mermaid.labelGuardTitle/Desc` and `bd.dialogue.buttonAllow` (the callout strings are in `app/own/1528-jW.js` / the i18n bundle and were not extracted for this pass).
- `Yf`, `Zf`, `$f` (`Jf`'s three helpers) — collapse-state persistence keys were not read.