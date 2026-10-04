# `RenderContext`: `data-href` is the resolved path for `TFile` targets, `NumberValue` renders `NaN` as `"NaN"` (only ±Infinity is `"∞"`), and three of §13's four guessed helpers are wrong

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/rendering.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/rendering.md` §1 (`RenderContext`, `Value` subclasses, bidi table), §2, §8 invariants 1–5 and 14–15, §11 (`RenderContext`, `Bidi` rows), §12 bullets, **§13 Q3/Q4/Q6/Q7** (answered), §15 (symbol table)
**Relevant cluster plans:** `libs/markoff/src/Editor.cpp` (the `Markoff::LinkRenderer` extraction `rendering.md` §11 asks for); Cluster O (context-sensitive UI) work that greps Bases cell rendering

## Finding

`RenderContext` and the whole `Value` tower live in **one** extraction window, `app/own/1647-tK.js` (bundle offsets 3329883–3345295, 385 lines). Symbol map, which replaces most of §15's `RenderContext` rows:

| §15 name | 1.13.7 name | what it actually is | cite |
|---|---|---|---|
| `mW` | **`tK`** | the (dead) bidi decoration table | `1647-tK.js:2` |
| `RenderContext` | **`nK`** | | `1647-tK.js:28` |
| `Value` | **`iK`** | | `1647-tK.js:100` |
| `ErrorValue` | **`rK`** | | `1647-tK.js:128` |
| `NullValue` | **`oK`** | | `1647-tK.js:152` |
| `NotNullValue` | **`aK`** | | `1647-tK.js:166` |
| `PrimitiveValue` | **`sK`** | | `1647-tK.js:172` |
| `StringValue` | **`lK`** | | `1647-tK.js:187` |
| `NumberValue` | **`cK`** | | `1647-tK.js:202` |
| `BooleanValue` | **`uK`** | | `1647-tK.js:211` |
| `ListValue` | **`hK`** | | `1647-tK.js:227` |
| `nE` (linktext display) | **`QE`** | **breadcrumb joiner, not a stripper** | `0936-QE.js:2` |
| `Ic` (internal-link styling) | **`Av`** | **middle-click `preventDefault`er** | `0631-Av.js:2` |
| `Lc` (hover-link predicate) | **`Dv`** | **`mouseover`-enters-element test** | `0630-Dv.js:2` |
| `Yc` (external-URL safety) | **`$v`** | **`new URL()` parseability** | `0645-$v.js:2` |

(The `Value` class in `renames.json` at offset 2174342 is a *different* declaration — the public Bases formula API. The Bases-domain `Value` base is `iK` and was not renamed.)

### CORRECTION — `data-href` is **not** always the linktext

§1 states "`data-href` is always the linktext, never the resolved path." Verbatim:

```js
// app/own/1647-tK.js:37-46  (renderFileLink, abridged)
if (e instanceof GD) r = (i = e).path;
else { var s = bD(e); i = a.metadataCache.getFirstLinkpathDest(s, ""), r = e; }
n.addClass("markdown-rendered");
var l = n.createSpan("internal-link"),
  c = t && i && hb.contains(i.extension) && FT(t.toString());
t && !c ? t.renderTo(l, this) : String.isString(e) ? l.setText(QE(e)) : l.setText(i ? i.getShortName() : r),
l.setAttr("data-href", r), l.toggleClass("is-unresolved", !i), Av(l),
l.onClickEvent(function(e) { 0 !== e.button && 1 !== e.button || (e.preventDefault(), a.workspace.openLinkText(r, "", uy.isModEvent(e))); }),
```

- **String target** (`else` branch): `r` is the **raw linktext** and `data-href` is that linktext.
- **`TFile` target** (`if` branch): `r = i.path` — the **resolved vault-relative path**, and that is what lands in `data-href`, what `openLinkText` is called with, what `handleLinkContextMenu` receives, what `dragManager.dragLink` receives, and what the `hover-link` payload's `linktext` field carries.

So `data-href` is the linktext **only** for the string overload. A Corbomite `renderFileLink` that always writes the linktext will emit different `data-href` values than Obsidian for every `TFile`-targeted call, which changes what `registerDomEvents`' `data-href || href` read (`app/own/1506-TW.js:770`) resolves to for plugin-generated links.

### CORRECTION — the label fallback is keyed on string-vs-`TFile`, not resolved-vs-unresolved

§1: "fallback label is `nE(target)` for strings, `i.getShortName()` for resolved files, **or raw linktext for unresolved**." The third branch does not exist. The expression is

```js
String.isString(e) ? l.setText(QE(e)) : l.setText(i ? i.getShortName() : r)
```

so a **string** target always gets `QE(e)` — the breadcrumb — *whether or not it resolved*; a `TFile` target always gets `getShortName()` (and `i` is unconditionally assigned in that branch, so the `r` arm is unreachable for `TFile`).

### NEW — `QE` is the `>`-breadcrumb joiner (§13 Q6 answered, and §1's guess was wrong)

```js
// app/own/0936-QE.js:2-6
function QE(e) {
  return e.split("#").filter(function(e) {
    return !!e;
  }).join(" > ").trim();
}
```

```js
// app/own/0936-QE.js:2-6
function QE(e) {
  return e.split("#").filter(function(e) {
    return !!e;
  }).join(" > ").trim();
}
```

It does **not** strip `#`; it splits on `#`, drops empty segments, joins with `" > "` and trims. So `[[Note#H1#H2]]` renders its label as `Note > H1 > H2`. §13 Q6 asks whether `nE` is a "basename-stripper or `#`-stripper" — it is neither. (This corroborates the parsing addendum's finding that the `>`-breadcrumb display text is written unconditionally at parse time; `RenderContext` computes the same string independently for the un-parsed path.)

### NEW — `Av` suppresses the middle-click default, it is not a styling helper (§13 Q7 answered)

```js
// app/own/0631-Av.js:2-6
function Av(e) {
  e.addEventListener("mousedown", function(e) { 1 === e.button && e.preventDefault(); });
}
```

`Av(l)` is called *after* the span exists and *before* `onClickEvent`. It exists so that the middle-click branch of the click handler (`1 === e.button` → `openLinkText`) actually fires: without the `mousedown` `preventDefault`, the browser's default auxclick/navigation would win. §13 Q7's guess ("probably the `internal-link` class/attribute applicator") is wrong, and `internal-link` styling comes from `n.createSpan("internal-link")` at `:43`.

### CORRECTION — `Dv` is not a modifier predicate; the modifier logic is in Page Preview (§13 Q3 answered)

```js
// app/own/0630-Dv.js:2-5
function Dv(e, t) {
  var n = e.relatedTarget;
  return !n || !t.contains(n);
}
```

That is a pure *enters-element* test: true when the pointer came from outside `t`. §1 calls it a "modifier-key predicate" and §13 Q3 guesses "modifier held *or* preview-on-hover-without-modifier enabled" — both wrong. **`hover-link` fires on every mouseover that enters the span**, with no modifier test at all.

The modifier gate is in the consumer, keyed off the hover-source registry:

```js
// app/own/2039-vee.js:427  (PagePreviewPlugin)
if (Object.hasOwn(this.options, i) ? h = this.options[i]
  : this.app.workspace.hoverLinkSources.hasOwnProperty(i) && (h = this.app.workspace.hoverLinkSources[i].defaultMod),
  q$(n), h && !uy.isModifier(n, "Mod")) { … }
```

and §8 invariant 2 ("`renderFileLink` always emits `source: "bases"` … Page-Preview must have `"bases"` registered") is now **sourced**: the Bases plugin registers it.

```js
// app/own/1972-Z5.js:1453-1456
e.workspace.registerHoverLinkSource("bases", { display: this.name, defaultMod: !0 }),
```

The other two in-app registrations are `"graph"` (`app/own/1848-Y1.js:789-793`) and `"bookmarks"` (`v8 = "bookmarks"` at `app/own/1972-Z5.js:1794`, registered at `:2579`), all with `defaultMod: true`.

> **Also a §4/§7 gap:** `hoverLinkSources` is a **plain object**, and `registerHoverLinkSource`/`unregisterHoverLinkSource` fire a workspace event **`hover-link-sources-changed`** (`app/own/1932-r4.js:2009-2013`) which `rendering.md` §4's event table omits. Its consumer re-derives Page Preview's settings UI from the registry:

```js
// app/own/2039-vee.js:475-492
r.icon = "scan-eye", r.instance = i,
n.registerEvent(t.workspace.on("hover-link-sources-changed", Ml(function() { return r.update(); }, 100))), r;
…
var e = this.app.workspace.hoverLinkSources, t = [];
for (var n in e) { var i = e[n]; t.push({ name: i.display, control: { type: "toggle", key: n, … } }); }
```

(the `hover-link-sources-changed` consumer is debounced by **100 ms**.)

### CORRECTION — `$v` is a URL-parseability test, not a scheme allowlist (§13 Q4 answered)

```js
// app/own/0645-$v.js:2-10
function $v(e) {
  if (!e) return !1;
  try { new URL(e); } catch (e) { return !1; }
  return !0;
}
```

§13 Q4 guesses a scheme allowlist ("likely `http`/`https`/`mailto`/`obsidian`/`tel`/`sms`"). It is not. `$v` returns true for **any** string `new URL()` accepts — including `javascript:` and `data:` — and false for empty strings and relative hrefs. So `renderExternalLink`'s guard (`1647-tK.js:80-86`) rejects *relative and empty* hrefs and admits everything else, then `window.open`s it. §11's "safe-scheme guard `Yc`" is a misnomer; the load-bearing property is **absolute-URL-ness**, not safety.

The one real scheme allowlist in the neighbourhood is MathJax's, `safeProtocols: {http, https, file: true; javascript, data: false}` (`app/own/1490-aW.js:26-33`) — for `\href{}` inside math, not for anchors. And `renderExternalLink` opens with `window.open(href, "boolean" == typeof n ? "" : n)` where `n = Keymap.isModEvent(e)`, so a mod-click gets a **named** window, not a new tab (`1647-tK.js:81-83`).

### CORRECTION — `NumberValue.renderTo`: `NaN` renders as `"NaN"`; only ±Infinity renders `"∞"`

§8 invariant 5 and the §12 bullet both say `NumberValue.renderTo` "writes literal `"∞"` (U+221E) for **non-finite/NaN**". The condition is:

```js
// app/own/1647-tK.js:207-209
t.prototype.renderTo = function(e, t) {
  isFinite(this.data) || isNaN(this.data) ? e.setText(this.toString()) : e.setText("∞");
}, t.type = "Number", t;
```

`isFinite(x) || isNaN(x)` → normal path (`toString()` = `String(this.data)`). The `"∞"` fallback is reached only when `!isFinite && !isNaN`, i.e. genuinely `Infinity` or `-Infinity`. **`NaN` takes the normal path and renders the text `NaN`.** (`-Infinity` renders `∞` too — the glyph has no sign.) §8 invariant 5 must be split into two cases.

### NEW — an image dimension-spec carve-out in `renderFileLink`, and the extension-family table

The `c` flag at `:44` is `t && i && hb.contains(i.extension) && FT(t.toString())`, and the label logic is `t && !c ? t.renderTo(...) : <fallback>`. So when a `displayValue` is supplied, the target resolves to an **image**, and the display value's string parses as a size spec, the display value is **discarded** and the fallback label renders. `FT` is the size parser:

```js
// app/own/1075-FT.js:2-8
function FT(e) {
  var t = e.match(/^\s*([0-9]+)\s*(?:x\s*([0-9]+)\s*)?$/);
  return t ? { x: parseInt(t[1]), y: t[2] ? parseInt(t[2]) : 0 } : null;
}
```

so `300` → `{x:300, y:0}` and `300 x 200` → `{x:300, y:200}`; anything else is `null` (display value used normally). This is the `![[image.png|300]]` width-spec path. The image-extension set and its siblings:

```js
// app/own/0794-hb.js:2-11
var hb = ["bmp", "png", "jpg", "jpeg", "gif", "svg", "webp", "avif"],
  pb = ["mp3", "wav", "m4a", "3gp", "flac", "ogg", "oga", "opus"],
  db = ["mp4", "webm", "ogv", "mov", "mkv"],
  fb = ["pdf"],
  vb = ["md"],
  mb = ["canvas"],
  gb = ["base"],
  yb = ["json", "css", "js"],
  bb = vb.concat(mb, gb),
  wb = [].concat(hb, pb, db, fb, vb, mb);
```

Note `bb = ["md","canvas","base"]` — the markdown-family set — and `wb` = images + audio + video + pdf + md + canvas (no `.base`). Bare extensions, no leading dot.

### NEW — `isTruthy()` exists on the entire `Value` tower and is absent from the doc

§1's "Instance contract" list (`equals`, `looseEquals`, `renderTo`, `keys`, `objectAccess`, `type`) is missing `isTruthy`, which every class implements:

| class | `isTruthy` | cite |
|---|---|---|
| `ErrorValue` | `!1` | `1647-tK.js:135-137` |
| `NullValue` | `!1` | `1647-tK.js:160-162` |
| `PrimitiveValue<T>` | `!!this.data` | `1647-tK.js:179-181` |
| `BooleanValue` | `this.data` (no coercion) | `1647-tK.js:216-218` |
| `ListValue` | `this.data.length > 0` | `1647-tK.js:249-251` |

`NotNullValue` inherits `Value`'s — and `Value` declares none, so `iK.prototype.isTruthy` does not exist.

### NEW — three more `Value` facts

- **Base `Value.toString()` returns the constructor.** `e.toString = function() { return this.type; }` and `type`'s getter is `return this.constructor` (`:104-105`, `:118-123`). So an un-overridden `Value` stringifies to a function object. Every concrete class except `NotNullValue` overrides `toString`.
- **`ErrorValue.toString()` is `"Error: " + message`** (`:133-135`), and the constructor accepts an `Error`, another `ErrorValue`, or anything (`n instanceof Error || n instanceof t ? i.message = n.message : i.message = n.toString()`), defaulting to `""`.
- **`ListValue.get(i)` mutates its own backing array**: `this.data[e] = this.lazyEvaluator(e, t)` (`:273`). Rendering a list therefore has the side effect of replacing raw elements with coerced `Value`s.
- `NullValue`'s guard message is verbatim `"Use NullValue.value instead of creating a new NullValue."` and `NullValue.value` is constructed eagerly at class-definition time (`:155`, `:164`).

### CONFIRMED

| §claim | verdict | cite |
|---|---|---|
| `parentEl.addClass("markdown-rendered")` before creating the span | confirmed | `1647-tK.js:42` |
| `<a class="external-link" target="_blank" rel="noopener">`, right-click sections `["title","open","action","info","view","","danger"]` | confirmed verbatim | `:67-77` |
| internal-link right-click sections `["title","open","action-primary","action","info","info.copy","view","system","","danger"]` + danger "Delete file" (`lucide-trash-2`, `setWarning(true)`) | confirmed verbatim | `:48-53` |
| `renderTag` click binds to `parentEl`, strips one leading `#`, no-ops if global-search is disabled | confirmed verbatim | `:88-97` |
| `BooleanValue.renderTo` = `<input type="checkbox" disabled>` with `.checked = data` | confirmed | `:218-224` |
| `ListValue.renderTo` = `div.value-list-container` > `span.value-list-element` + `span.value-list-gap` containing a literal `"\n"`, gap only **between** items | confirmed | `:380-383` |
| `StringValue` exposes `length` via `objectAccess` (case-insensitive key) | confirmed | `:196-197` |
| `Value.icon` default `"lucide-file-question"`; `type` getter returns the constructor; `static type = "Any"` | confirmed | `:102`, `:118-126` |
| `renderExternalLink` has no drag and no hover | confirmed (only `contextmenu` + `onClickEvent`) | `:65-87` |
| `RenderContext` never assigns `hoverPopover` | not contradicted — constructor sets `null` and no in-file writer exists; §13 Q9 remains open | `:30` |
| `ListValue` aggregation API (`sum`/`mean`/`median`/`min`/`max`/`stddev`/`earliest`/`latest`/`join`/`sort`/`unique`/`flatten`/…) | confirmed, incl. `stddev` = `sqrt(E[(x-mean)^2])` (population, not sample) | `:234-379` |

## Why noticed now

`rendering.md` §11 nominates a `Markoff::LinkRenderer` exposing `renderFileLink`/`renderExternalLink`/`renderTag` "with same context-menu-sections / drag / hover-link-emission contract". That contract is now fully specified, and three of its parts were documented wrong — the `data-href` value, the label fallback branch, and the meaning of the hover gate. The `NaN`/`∞` split additionally matters because `rendering.md` §11's Corbomite row for number cells points at `Markoff::MathRenderer`-adjacent behaviour where a NaN showing as `∞` would be a visible parity bug.

## Action taken

None yet. The `Markoff::LinkRenderer` extraction should take `renderFileLink`'s two-target signature literally (`TFile` → `data-href = path`; string → `data-href = linktext`) and should keep the `Av`-equivalent middle-click suppression as a distinct step. Any Bases-cell direction logic should use the `rm`-based detector described in the callout/bidi addendum rather than Qt auto-detection.

## Unverifiable

- `bD` = `getLinkpath` is confirmed (`app/own/1112-bD.js:2-4`: cuts at the **first** `#`), but whether `renderFileLink` ever receives a linktext still containing an alias (`|`) is decided upstream in the parser and was not re-verified here.
- `xK` (the `lazyEvaluator` coercion at `:230-232`) and `gK` (`DateValue`) were not read; `getDates`/`earliest`/`latest` depend on `gK.parseFromString`.