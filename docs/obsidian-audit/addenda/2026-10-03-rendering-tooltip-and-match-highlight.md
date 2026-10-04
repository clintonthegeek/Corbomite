# Tooltips are a **delegated `document.body` system** driven by `aria-label`, with a **1000 ms default hover delay**; match highlighting is `span.suggestion-highlight`

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/rendering.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/rendering.md` §1 (`displayTooltip`, `renderResults`), §2 (options block), §8 invariants 12–13, §9 (tooltip + search-row bullets), §11 (`displayTooltip`/`setTooltip`, `renderResults` rows), §12 bullets, **§13 Q5** (answered), §15 (`renderMatches` row)
**Relevant cluster plans:** any plan replacing `QToolTip::showText`/`QWidget::setToolTip`; Cluster O (context-sensitive UI) tooltips; `docs/PARITY-MATRIX.md`

## Finding

### CORRECTION — the default *hover* delay is 1000 ms, not 0

§2's options block says `delay?: number; // ms; 0 = synchronous` and §12 calls the delay a constant with no value. The module-statics table resolves it (the per-name glosses below are mine, not in the source):

```js
// app/own/0725-Og.js:2-8
var Og = 8,
    Fg = 5,
    Rg = 300,
    Ng = 1e3,
    Bg = 100,
    Vg = null,
    Hg = null;
```

`Xg` (i.e. `displayTooltip`) does default `delay` to `0`:

```js
// app/own/0735-Xg.js:4-13
i = (n = n || {}).placement, r = void 0 === i ? "bottom" : i,
o = n.classes, a = void 0 === o ? [] : o,
s = n.gap, l = void 0 === s ? Og : s,
u = n.delay, h = void 0 === u ? 0 : u;
```

…but the **hover path** supplies `Ng`:

```js
// app/own/0733-Gg.js:11-17   (Gg = "show the tooltip for this aria-label element")
var r, o = Ng;
e.hasAttribute("data-tooltip-delay") && (o = parseInt(e.getAttribute("data-tooltip-delay") || ""), isNaN(o) && (o = Ng)),
e.hasAttribute("data-tooltip-gap") && (r = yv(e.getAttribute("data-tooltip-gap"), void 0)),
Xg(e, t, { placement: n, classes: i, delay: o, gap: r });
```

Reading the names against their use sites: `Og` (`:2`) is the default gap, `Fg` (`:3`) the top-placement arrow offset, `Rg` (`:4`) the default delay for *direct* `setTooltip` calls (and the one `ErrorValue.renderTo` passes), `Ng` (`:5`) the default delay for *hover-driven* tooltips, `Bg` (`:6`) the post-dismiss cooldown, `Vg` (`:7`) the live tooltip div, `Hg` (`:8`) its target element.

So **any element carrying `aria-label` gets a one-second hover delay** unless it sets `data-tooltip-delay`. §11's Corbomite row ("Cooldown-skip-on-sweep differs from Qt semantics; minor UX divergence") understates this: the baseline is a deliberate 1 s delay, which is the observable difference.

### NEW — the whole system is one delegated installer on `document.body`

```js
// app/own/0726-zg.js:2-5
function zg(e) {
  e || (e = document);
  var t = e.body;
  t.on("pointerover", "[aria-label]", Ug), t.on("pointerout", "[aria-label]", _g), t.addEventListener("pointerup", Qg);
}
```

```js
// app/own/0730-Ug.js:2-4
function Ug(e, t) {
  jv(e) || Dv(e, t) && (jg(t) || Gg(t));
}
```

```js
// app/own/0731-_g.js:2-13
function _g(e, t) {
  if (!jv(e) && Dv(e, t)) {
    Qg();
    var n = e.relatedTarget;
    if (n && n.matchParent) {
      var i = n.matchParent("[aria-label]");
      if (i && i.instanceOf(HTMLElement)) {
        if (jg(i)) return;
        Gg(i);
      }
    }
  }
}
```

- **One `pointerover` / `pointerout` listener pair on `document.body`,** delegated on the `[aria-label]` attribute selector. There is no per-widget tooltip API and no `QToolTip`-equivalent singleton per view.
- **Sweeping between two tooltipped icons hands off with no delay.** `pointerout` destroys the tooltip and then, if the new `relatedTarget` has an `[aria-label]` ancestor, shows *that* one immediately. The cooldown makes this work: `Qg` sets `Yg = Date.now()` (`app/own/0736-Qg.js:3`, `Yg` declared at `app/own/0734-Yg.js:2`), and `Xg`'s delay gate is

```js
// app/own/0735-Xg.js:9-13
if (h > 0 && (Vg || Date.now() > Yg + Bg)) return Wg(), n.delay = 0, void(qg = window.setTimeout(function() { return Xg(e, t, n); }, h));
```

so immediately after a teardown, `Date.now() > Yg + 100` is **false**, the gate is skipped, and `Gg`'s 1000 ms delay is bypassed. That is §8 invariant 12's "cooldown skips the delay when sweeping across tooltipped icons" — now with the constant (`Bg = 100`) and the exact reason.
- Note the gate **mutates the caller's options object** (`n.delay = 0`) and pushes the `mod-*` class into the caller's `classes` array (`:26-28`), so a shared opts object accumulates classes across repeated calls.
- `Qg` (`app/own/0736-Qg.js:2-4`) is the destroy path: `Wg(), Vg && (Yg = Date.now(), Vg.detach(), Vg = null, Hg = null)` — and it is also bound to a bare `pointerup` on `body`, so clicking anywhere dismisses.

### NEW — the synthetic-event filter `jv`, which is why Qt has no direct equivalent

```js
// app/own/0637-jv.js:2-8
function jv(e) {
  if (!(e instanceof MouseEvent)) return !1;
  if (e instanceof PointerEvent) return "mouse" !== e.pointerType || Vv < 2;
  if (zv > 0) return !0;
  var t = Date.now();
  return t - Hv < 400 || !!(rd.isIosApp && t - qv < 100);
}
```

Three suppression rules: a **non-`"mouse"` `pointerType`** (touch, pen) never triggers a tooltip; a `PointerEvent` with `pointerType === "mouse"` is suppressed while `Vv < 2`; and a plain `MouseEvent` is suppressed if `zv > 0` or if it arrives within **400 ms** of `Hv` (**100 ms** on iOS). Qt delivers neither `pointerType` nor the synthetic-event provenance this guards against, so a port has to decide what replaces them — most plausibly "only react to real hover-enter events", which is what `Dv` already does.

### NEW — opt-out, dynamic labels, and attribute serialisation

```js
// app/own/0729-jg.js:2-4     opt-out via computed custom property
function jg(e) { return "true" === getComputedStyle(e).getPropertyValue("--no-tooltip").trim(); }

// app/own/0739-Jg.js:2-4    dynamic (provider) label
function Jg(e, t, n) { e.setAttribute("aria-label", ""), Kg.set(e, t), $g(e, n), Hg === e && Xg(e, t(e), n); }

// app/own/0737-$g.js:2-8    attribute writer
function $g(e, t) {
  var n = t || {}, i = n.placement, r = n.classes, o = n.delay, a = n.gap;
  i && "bottom" !== i && e.setAttribute("data-tooltip-position", i),
  r && e.setAttribute("data-tooltip-classes", r.join(" ")),
  o && e.setAttribute("data-tooltip-delay", String(o)),
  a && e.setAttribute("data-tooltip-gap", String(a));
}
```

- `setTooltip` is `Zg` (`0738-Zg.js:2-4`): `e.setAttribute("aria-label", t), $g(e, n), Hg === e && Xg(e, t, n)` — it writes `aria-label` plus the four `data-tooltip-*` attributes, and **re-displays immediately if the element is already the hovered target**.
- `$g` **never serialises the default placement** (`i && "bottom" !== i`), so `data-tooltip-position` is absent unless the placement is non-default. Any theme keying off that attribute must handle its absence.
- `Jg(el, providerFn, opts)` writes an **empty** `aria-label` and stores the provider in the `Kg` WeakMap (`0732-Kg.js:2`); `Gg` then resolves the text at hover time: `var t = e.getAttribute("aria-label") || ""; Kg.has(e) && (t = Kg.get(e)(e));`. Note `Jg`'s immediate-refresh path calls `t(e)` while `Zg`'s passes the literal string — two different deferred-label APIs with different arities.

### NEW — placement arithmetic, and `mod-bottom` does not exist

```js
// app/own/0735-Xg.js:27-34  (abridged)
if (Vg && Hg === e) Vg.setText(t); else Qg(), Vg = createDiv({ cls: "tooltip", text: t });
var b = Vg.createDiv("tooltip-arrow"), w = 0, k = 0;
"bottom" === r ? (w = f + g + l, k = v + m / 2)
: "right" === r ? (w = f + g / 2, k = v + m + l, a.push("mod-right"))
: "left"  === r ? (w = f + g / 2, k = v - l,          a.push("mod-left"))
: "top"    === r && (w = f - l - Fg, k = v + m / 2,    a.push("mod-top")),
Vg.addClasses(a), Vg.style.top = "0px", Vg.style.left = "0px", Vg.style.width = "", Vg.style.height = "",
Vg.parentNode || p.body.appendChild(Vg);
```

- **`mod-bottom` is never added** — bottom is the unclassed default. §1's "adds `mod-{top,right,left}` classes" is literally correct; worth stating explicitly because a theme that styles `.tooltip.mod-bottom` will never match.
- `top` placement offsets by `Fg = 5` above the anchor (`w = f - l - Fg`) — the arrow-offset constant.
- **The tooltip is appended to `e.doc.body`**, not to the anchor.
- `horizontalParent` (`c`) replaces the anchor rect's `left` and `width` only: `var y = c.getBoundingClientRect(); v = y.left, m = y.width` (`:17-19`).
- Clamping uses `e.doc.body.clientHeight` / `clientWidth` (`:33-34`), and on horizontal shift the arrow switches from `right` to `left` (or vice versa) with `E - T - l / 2` px offsets (`:36-45`).
- Final assignment **locks the measured size**: `Vg.style.top/left/width/height` are all written in px from `C = Vg.getBoundingClientRect()` measured before clamping (`:48`), so the box cannot reflow while shown.
- **`e.isShown()` gates everything**: a hidden anchor produces no tooltip and, notably, **no re-schedule** — the function returns without arming the timer.
- Re-showing on the *same* element is a text-only `setText`, with no relayout and no class/placement recomputation (`:20`).

### NEW — `renderResults` is `lb`, and the highlight class is exactly `suggestion-highlight`

§13 Q5 asks for `renderMatches`' definition and hedges the class name ("`<span class="suggestion-highlight">` (or analogous)"). There is no `renderMatches` in 1.13.7 — the function the doc was tracking is:

```js
// app/own/0791-lb.js:2-21   (lb(el, text, matches, offset = 0))
void 0 === i && (i = 0), e.appendChild(function(e, t, n) {
  void 0 === n && (n = 0);
  if (!t || 0 === t.length) return document.createTextNode(e);
  for (var i = document.createDocumentFragment(), r = 0, o = 0; r < e.length && o < t.length; o++) {
    var a = t[o], s = a[0] + n, l = a[1] + n;
    if (!(l <= 0)) {
      if (s >= e.length) break;
      s < 0 && (s = 0), s !== r && i.appendText(e.substring(r, s)), i.createSpan({ cls: "suggestion-highlight", text: e.substring(s, l) }), r = l;
    }
  }
  r < e.length && i.appendText(e.substring(r));
  return i;
}(t, n, i));
```

Exact behaviour, worth having because it differs from a naive implementation:

- `matches` are half-open `[start, end)` pairs in the **unoffset** coordinate space; `offset` is added to both ends.
- A match with `end + offset <= 0` is **skipped**, not clamped.
- A match whose `start + offset >= text.length` **breaks the whole loop** — later matches are dropped rather than skipped.
- A negative `start + offset` is clamped to `0`, which can emit an overlapping highlight (`s !== r` guard prevents duplicate text nodes but not overlapping spans).
- The function **always appends**, even for an empty match list (a bare text node for empty input), and it appends to `el` rather than replacing `el`'s content.

## Why noticed now

`rendering.md` §11 marks `displayTooltip`/`setTooltip` as **Have** (via `QToolTip`/`setToolTip`) with "minor UX divergence", and marks `renderResults` as **Missing**. The tooltip file shows the divergence is structural rather than cosmetic — a delegated, attribute-driven system with a 1 s default delay, a 100 ms sweep cooldown, a `pointerup` global dismiss, and a `--no-tooltip` opt-out have no `QToolTip` counterpart at all. The `renderResults` class name, meanwhile, was a one-line confirmation away.

## Action taken

None yet. `QToolTip::showText` will not reproduce the delay/cooldown/handoff semantics; if tooltip timing parity is wanted, Corbomite needs its own single-singleton tooltip layer with a filter (Qt's `underMouse` transitions) rather than `QToolTip`.

## Unverifiable

- `yv` (`data-tooltip-gap` length parsing), `Vv`, `zv`, `Hv`, `qv` — the module constants behind `jv` were not read, so the exact values of the `Vv < 2` and `zv > 0` gates are not claimed.
- Whether `zg()` is called once per window or once per popout window; the `e || (e = document)` parameter implies a second-document call site exists (consistent with the `sanitizeHTMLToDom` popout concern in §8 invariant 10) but the call site was not read.