# UI bundle source proof: the Lucide table is 1408 icons not 125, `PopoverState` is a string enum, the slider tooltip is a stub, and the menu/section protocol is hierarchical

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/ui-bundle.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`, `renamed/src/<bundle>/own/*.js` + `asar/obsidian/app.css`).
**Supersedes / extends:** `domains/ui-bundle.md` §1 (all four sub-dirs), §2 (`Menu` sort algorithm, `PopoverState` enum, `Component` internals), §5, §7 (icon + section registries), §8, §9, §10, §11, §13 (Q1–Q5, Q7, Q8, Q9).
**Relevant cluster plans:** any plan touching `libs/core` icon resolution (`Corbomite::Icons`), `Corbomite::MenuSectionRouter`, `Corbomite::Component`, a `Corbomite::Notice`, `Corbomite::HoverPopover`, `Corbomite::SuggestModal`, the settings dialog (`Setting` / `SettingGroup` / `SettingPage`), or the Moment-format adapter.

> **Read this before implementing any of §11.** Three of the doc's most
> implementable sentences are wrong: the icon count is off by ~15×, the slider
> dynamic tooltip does not exist, and `PopoverState` is not a numeric enum.

---

## Finding

### CORRECTION 1 (HIGH — icons): the shipped Lucide table has **1408** entries, not 125

`ui-bundle.md` §11 ("125 unique Lucide IDs") and §14 item 5 ("**125 unique Lucide IDs.** Canonical list in `getIcon.js`'s resolution chain (`Um`→`Xm`→`Qm`→`Ym`-aliased)") both size the icon surface at 125. The real table is 11× larger, and the "canonical list" is **generated, not hand-listed**:

```js
// app/own/0713-gg.js:2  — `gg` is the Lucide table; 1408 top-level keys,
// first "a-arrow-down", last "github". 336653 B.
const gg = { "a-arrow-down": [ [6,"m14 12 4 4 4-4"], [6,"M18 16V7"], … ], … }
```

```js
// app/own/0724-Lg.js:2 — getIconIds(): no dedup, aliases excluded
function Lg() {
  return [].concat(Object.keys(gg).map(function(e) {
    return "lucide-" + e;
  })).concat(Object.keys(Mg)).concat(Object.keys(Eg));
}
```

> **Reviewer correction.** This section originally claimed 1935. `gg` has exactly
> **1408** keys, and the two concatenated tables are empty at module-init time
> (`Mg = {}` at `app/own/0718-Cg.js:202`; `Eg` at `:156` declares no own keys), so
> `Lg()` yields 1408 ids, not 1935. The correction against the audit's 125 stands
> and is in fact larger than first written, but the exact figure is 1408. Counted by
> enumerating top-level keys of the object literal, not by regex over the file.

Consequences for Corbomite:
- Do **not** treat "125" as a coverage target. A `lucide-*` name Corbomite doesn't resolve is expected, not exceptional.
- `gg` contains **`github`**, so `github` resolves through the Lucide branch, not the alias branch — relevant to the §11 table row that lists `github` under `[M]` bundled-SVG.
- `getIconIds()` returns `lucide-*` keys ++ custom ++ legacy **with no dedup**, so a custom icon whose name collides with a legacy id appears twice.

#### NEW: the Lucide table is a 7-variant binary node encoding, not raw SVG

`kg()` expands each entry through `wg()`, so the stored form is compact tuples, not SVG strings:

```js
// app/own/0717-kg.js:2
function kg(e) { for (var t = bg("svg", yg), n = 0, i = e; n < i.length; n++) t.appendChild(wg(i[n])); return t; }
```

```js
// app/own/0716-wg.js:2-46 — the seven node variants
//   0 line     [0, x1, y1, x2, y2]
//   1 circle   [1, cx, cy, r]        (+ truthy e[4] => fill: currentColor)
//   2 polyline [2, points]
//   3 polygon  [3, points]
//   4 ellipse  [4, cx, cy, rx, ry]
//   5 rect     [5, x, y, w, h, rx?, ry?, transform?]
//   6 path     [6, d]                (+ truthy e[2] => fill: currentColor)
```

A faithful Qt port must either implement this expansion or ship upstream Lucide's
raw SVGs; the in-bundle form cannot be passed through verbatim.

#### NEW: every icon `<svg>` carries two classes, and there are **two** attribute presets

```js
// app/own/0719-Tg.js:2-9 — the memo wrapper
function Tg(e, t) {
  if (!xg.hasOwnProperty(e)) {
    var n = t();
    if (!n) return null;
    n.classList.add("svg-icon", e), xg[e] = n;
  }
  return xg[e].cloneNode(!0);
}
```

Three contracts fall out, none of which the doc states:

1. **The rendered SVG's class list is exactly `"svg-icon <icon-name>"`** — `svg-icon` plus the *bare* name (no `lucide-` prefix). Theme CSS targets this pair (`asar/obsidian/app.css:4784` `body.is-mod-rtl svg.svg-icon:not(…)`, `:4712` callout-icon RTL flip, `:5399` `.setting-hotkey-icon .svg-icon`). Any Corbomite icon-name→theme mapping must preserve both class names for CSS parity.
2. **The cached node is deep-cloned** (`cloneNode(!0)`) per call, so two `getIcon("x")` calls never share a node.
3. **A miss caches nothing and returns `null`** — misses re-run the factory every time, and `xg.hasOwnProperty` (not truthiness) is the memo test.

```js
// app/own/0714-yg.js:2-11 — Lucide + legacy preset (stroke-based, themeable)
var yg = { xmlns:"http://www.w3.org/2000/svg", width:24, height:24,
  viewBox:"0 0 24 24", fill:"none", stroke:"currentColor",
  "stroke-width":2, "stroke-linecap":"round", "stroke-linejoin":"round" };
```
```js
// app/own/0718-Cg.js:203-205 — custom-icon preset: NO stroke attrs, 100×100 viewBox
  Mg = {},
  Sg = { viewBox: "0 0 100 100" },
```

So `addIcon`-registered plugin SVGs get a **100×100 viewBox and no `stroke`/`fill`/`width`/`height`**, while built-ins are 24×24 stroke-based with `stroke: currentColor`. The doc's §1 asserts custom icons are wrapped "via `$m` attr preset" and legacy via `Wm` — the presets are **crossed**: custom → `Sg`, legacy → `yg`.

#### CORRECTION 1b: `getIcon`'s branch order — the `lucide-` branch never consults aliases

```js
// app/own/0720-Dg.js:2-15  (verbatim)
function Dg(e) {
  return e.startsWith("lucide-") ? Tg(e, function() {
    var t = e.substring(7);
    return gg.hasOwnProperty(t) ? kg(gg[t]) : null;
  }) : Mg.hasOwnProperty(e) ? Tg(e, function() {
    var t = bg("svg", Sg); return t.innerHTML = Mg[e], t;
  }) : Eg.hasOwnProperty(e) ? Tg(e, function() {
    var t = bg("svg", yg); return t.innerHTML = Eg[e], t;
  }) : (Cg.hasOwnProperty(e) && (e = Cg[e]), gg.hasOwnProperty(e) ? Tg("lucide-" + e, function() {
    return kg(gg[e]);
  }) : null);
}
```

The doc's §1 four-step order is structurally right, but two consequences it misses:
- **A `lucide-`-prefixed name is looked up *only* in `gg` with the prefix stripped** — the alias map is never reached for that branch.
- **Alias resolution is last-resort and silently loses to the legacy table.** `right-triangle` is present in *both* `Cg` (as an identity alias) and `Eg` (with a real path), so `Eg` wins and `Cg["right-triangle"]` is unreachable. The doc treats the alias map as a uniform fourth tier.

### NEW: §13 Q4 answered — the alias map is 15 entries, two of them identities

```js
// app/own/0718-Cg.js:2-16  (verbatim, complete)
var Cg = {
    "create-new": "edit",           trash: "trash-2",
    search: "search",               "right-triangle": "right-triangle",
    document: "file",               folder: "folder-open",
    pencil: "edit-3",               "left-arrow": "chevron-left",
    "right-arrow": "chevron-right", "three-horizontal-bars": "menu",
    "reading-glasses": "glasses",   "user-manual-filled": "book-open",
    "discord-filled": "discord",    "chat-bubbles-filled": "message-circle",
    "experiment-filled": "experiment"
  },
```

### CORRECTION 1c: a custom icon named `lucide-*` is **unreachable**, and `removeIcon` on a built-in is a cache-bust rather than a no-op

Both fall out of `Dg`'s branch 1 combined with `Pg`/`Ig`:

```js
// app/own/0722-Pg.js:2   (verbatim)
function Pg(e, t) { delete xg[e], Mg[e] = t; }
// app/own/0723-Ig.js:2   (verbatim)
function Ig(e) { delete xg[e], delete Mg[e]; }
```

- `addIcon("lucide-mine", svg)` stores into `Mg`, but `Dg("lucide-mine")` takes branch 1, strips the prefix, finds no `gg["mine"]`, and **returns `null` without ever consulting `Mg`**. The icon is not merely outranked by a built-in (§7/§8's framing) — it is unreachable for its whole lifetime, and `setIcon(el, "lucide-mine")` will *blank* `el` per CORRECTION 2. This is a plugin-visible trap on the extension surface §10 documents.
- `removeIcon("lucide-x")` deletes `xg["lucide-x"]` (the memo) and finds nothing in `Mg`. So it does **not** remove the icon — but §8's "silent no-op" is imprecise: the memo entry is gone, so the next `getIcon("lucide-x")` re-runs `kg(gg["x"])` and returns a fresh `<svg class="svg-icon lucide-x">`. The distinction only bites a caller holding a stale node.

`removeIcon` never touches `gg` or `Eg`, which is the part §7 gets right.

### CORRECTION 2: `setIcon`'s idempotency test is a **class-name test**, and a miss *blanks* the element

```js
// app/own/0721-Ag.js:2-9  (verbatim)
function Ag(e, t) {
  var n = e.firstChild;
  if (!(n && n.instanceOf(SVGSVGElement) && n.classList.contains(t))) {
    n && e.removeChild(n);
    var i = Dg(t);
    i && e.appendChild(i);
  }
}
```

- The guard is `classList.contains(name)` — it requires the caller to have used `setIcon` (which adds the class) rather than injected an SVG by hand.
- **An unknown name removes the existing icon and appends nothing.** `setIcon(el, "lucide-typo")` silently empties `el`. The doc's §8 says only "`getIcon(unknown)` returns `null` (callers must guard)" and does not mention that `setIcon` is destructive on a miss.
- `removeChild`, not `detach` — no other difference.

### CORRECTION 3: `PopoverState` is a **string** enum, and `Hidden` is `"Hidden"` (not `0`)

```js
// app/own/1793-expressionstatement.js:3  (verbatim)
! function(e) {
  e[e.Showing = 0] = "Showing", e[e.Shown = 1] = "Shown", e[e.Hiding = 2] = "Hiding", e[e.Hidden = 3] = "Hidden";
}(M$ || (M$ = {}));
```

This answers §13 Q1 and refutes three places:
- §2's block
  ```ts
  enum PopoverState { Hidden = 0, Showing = 1, Shown = 2, Hiding = 3 }
  ```
  has both the wrong value type and the **reversed** member order. The real reverse-map order is `Showing, Shown, Hiding, Hidden`.
- §8's "`PopoverState.Hidden === 0` (inferred from `!state` truthy-checks)" is wrong — `Hidden === "Hidden"`. Its truthiness argument also does not apply: the ctor sets `state = M$.Showing` immediately (`app/own/1794-N$.js:56`), so `state` is never falsy.
- §2's "Exact numeric values not verified" is now verified.

`PopoverState` is also **plugin-exported** (`app/own/0025-expressionstatement.js:60` `PopoverState: () => M$`), so the string values are part of the public surface a Corbomite shim must reproduce.

### CORRECTION 4: `SliderComponent.setDynamicTooltip()` and `showTooltip()` are **empty stubs** — there is no slider tooltip

```js
// app/own/0817-Yb.js:549-550, :561  (verbatim)
SliderComponent.prototype.setDynamicTooltip = function() { return this; },
…
SliderComponent.prototype.showTooltip = function() {},
```

§1 says `setDynamicTooltip()` "`displayTooltip(el, value, {placement:"top"})` on hover"; §9 says "Sliders show a dynamic tooltip above the thumb while dragging". Neither is implemented. §11's mapping row (`setDynamicTooltip()` → `QToolTip::showText` in `sliderMoved`) is therefore a Corbomite **invention**, not a parity requirement — it should be filed as a deliberate divergence, not a gap.

What the slider *does* do is write two observable things the doc omits:

```js
// app/own/0817-Yb.js:506 — a sibling value span is created BEFORE the input
      n.valueEl = t.createEl("span", "slider-value");
```
```js
// app/own/0817-Yb.js:551-560 — updateDisplay()
SliderComponent.prototype.updateDisplay = function() {
  var e = this.sliderEl;
  this.valueEl.setText(this.getValuePretty());
  var t = yv(e.min, 0), n = yv(e.max, 100), i = this.getValue(),
      r = n > t ? (i - t) / (n - t) : 0;
  e.setCssProps({ "--slider-fill-ratio": r.toString() });
};
```

- A custom property `--slider-fill-ratio` is written **on the `input[type=range].slider` element** (not a wrapper) and consumed by
  ```css
  /* asar/obsidian/app.css:8420-8422 */
  input[type='range'].slider {
    --slider-fill-position: calc(var(--slider-thumb-width) / 2 + (100% - var(--slider-thumb-width)) * var(--slider-fill-ratio, 0));
    background-image: linear-gradient(to var(--inset-end), var(--slider-fill-background) var(--slider-fill-position), transparent var(--slider-fill-position));
  }
  ```
- `.slider-value` is styled and **`:empty { display: none }`** (`asar/obsidian/app.css:8471-8479`), so the span disappears rather than showing a gap when unset. `font-variant-numeric: tabular-nums` at `:8476`.
- `getValuePretty()` has **three** branches, not the doc's one:
  ```js
  // app/own/0817-Yb.js:542-546  (verbatim)
  SliderComponent.prototype.getValuePretty = function() {
    var e = this.sliderEl, t = this.displayFormat, n = this.getValue();
    return t ? t(n) : "any" === e.step || parseFloat(e.step) < 1 ? n.toFixed(2) : n.toString();
  };
  ```
  i.e. `setDisplayFormat(fn)` wins; else `step === "any"` **or** `step < 1` ⇒ `toFixed(2)`; else `toString()`. `setDisplayFormat` is a public method the doc omits.
- `setValue()` **fires `changeCallback`** when the value actually changes (`:536-540`), like `ToggleComponent.setValue`.
- `navigator.vibrate(100)` fires on **every `input` event** (`:512`), i.e. on every drag tick, not once per interaction.

### NEW: `DropdownComponent` auto-sizes itself with a hidden probe `<select>` and one custom property

```js
// app/own/0817-Yb.js:448  (verbatim)
  n.selectEl = t.createEl("select", "dropdown"), n.measureEl = t.createEl("select", "dropdown is-measuring"),
  n.measureEl.setAttribute("aria-hidden", "true"), n.measureEl.tabIndex = -1,
  n.measureOptionEl = n.measureEl.createEl("option"),
  n.selectEl.onNodeInserted(function() { return n.resizeToFit(); }, !0),
```
```js
// app/own/0817-Yb.js:460-466  (verbatim)
DropdownComponent.prototype.resizeToFit = function() {
  var e = this, t = e.selectEl, n = e.measureEl, i = e.measureOptionEl, r = t.options[t.selectedIndex];
  r && (i.text = r.text, t.style.setProperty("--dropdown-fitted-width", n.offsetWidth + "px"));
};
```

So the observable contract is: a second, `aria-hidden`, `tabindex=-1` `<select class="dropdown is-measuring">` sibling holds a single `<option>` mirroring the selected label, and `--dropdown-fitted-width` is set on the real select. The property is consumed **only** inside a settings row:

```css
/* asar/obsidian/app.css:5338-5340 */
.setting-item-control select.dropdown { width: var(--dropdown-fitted-width, inherit); }
```

Also new: `mousedown` adds class `mouse-focus` to the select and `blur` removes it (`:454-457`), and `resizeToFit` re-runs on every inserted node (`onNodeInserted(..., true)`), so `addOption` after construction resizes.

### NEW: `setTooltip` writes `aria-label`, not `title` or `data-tooltip`

Every `setTooltip` in this domain — on `ButtonComponent` (`0817-Yb.js:249-250`), `ExtraButtonComponent` (`:283-284`), `ToggleComponent` (`:330-331`), and `Setting` (`:31-32`) — routes through one helper:

```js
// app/own/0738-Zg.js:2  (verbatim)
function Zg(e, t, n) { e.setAttribute("aria-label", t), $g(e, n), Hg === e && Xg(e, t, n); }
```

`aria-label` is exactly the hook the delegated `[aria-label]` tooltip system reads (per `2026-10-03-rendering-tooltip-and-match-highlight.md`), and the third clause means **re-calling `setTooltip` on the currently-hovered element re-renders the live tooltip immediately**. §1 lists `setTooltip(s, opts?)` on four components without ever saying which attribute it writes; that attribute is the whole integration contract.

### CORRECTION 5: the section protocol is **hierarchical (`a.b.c`)** and `sort()` keys separators on the first dot-segment

§2's five-step sort description is materially incomplete. The live `sort()`:

```js
// app/own/0751-my.js:47-98 — the two rules §2 does not have
      for (var e = this, t = e.items, n = e.sections, i = e.submenuConfigs, r = { "": [] }, …) {
        var s = "";
        (w = a[o]) instanceof py && (s = w.section),
        "" === s || n.contains(s) || n.push(s);           // ← "" is never pushed onto sections[]
        …
      }
      for (var c = null, u = null, h = "", p = 0, d = n; p < d.length; p++) {
        s = d[p];
        if (r.hasOwnProperty(s)) {
          var f = "";
          for (var v in i)
            if (i.hasOwnProperty(v) && (s === v || s.substring(0, v.length + 1) === v + ".")) { f = v; break; }
          var m = s.split("."), g = m[0], y = m.length > 0 ? m.slice(1).join(".") : s;
          if (f) {
            if (u && f === h) u.items.push(new dy(this));
            else { c && c.split(".")[0] !== g && (null !== c && t.push(new dy(this)), c = s); … }
            for (var k = 0, C = r[s]; k < C.length; k++) { (w = C[k]) instanceof py && w.setSection(y), u.items.push(w); }
          } else {
            c !== g && (null !== c && t.push(new dy(this)), c = s);   // ← separator keyed on g = first segment
            …
```

Four load-bearing behaviours §2 misses:

1. **Separators are keyed on the section id's first dot-segment** (`g`), not on the section itself. `info` and `info.copy` are therefore rendered **contiguously with no separator between them** — `info.copy` is a *subsection* of `info`, not a peer. §2 lists `info.copy` as an ordinary section id alongside `info`.
2. **`submenuConfigs` matching is prefix-based**: `s === v || s.substring(0, v.length + 1) === v + "."`. `setSectionSubmenu("info")` therefore swallows `info.copy` too. §2 says "`submenuConfigs[id]` present → wrap **that** section in a child `Menu`".
3. **Items inside a submenu-configured section get their `section` rewritten** to the remainder after the first segment (`w.setSection(y)` where `y === m.slice(1).join(".")`). So an item declared `info.copy` ends up with `section === "copy"` and `data-section="copy"` in the DOM. This also refutes §8's "`setSection` post-sort mutates the DOM attribute only" — `sort()` calls it.
4. **Successive sections mapping to the same submenu config get an intra-submenu separator** (`if (u && f === h) u.items.push(new dy(this))`), so `info` + `info.copy` under one config become two separator-delimited groups *inside* the submenu.

§2's remaining claims are confirmed: unknown non-empty sections auto-push onto `sections[]` (line 52), the `""` bucket is emitted only when `""` is explicit in `sections[]` (line 89), trailing separators are popped (line 94), and items between separators are wrapped in a `.menu-group` div (line 97).

### NEW: the real section-ID inventory is ~40 ids, three levels deep — §14 item 2's canonical list is missing most of them

Every `addSections([...])` array literal in `app/own/`:

| Site | Sections |
|---|---|
| `app/own/1204-gI.js:233, 906` (×2) | `title, open, action-primary, action, info, info.copy, view, system, "", danger` |
| `app/own/0754-wy.js:170` | `close, pane, open, action, find, info, info.copy, view, view.linked, system, "", danger` |
| `app/own/1209-jI.js:825` | `title, close, pane, open, action, find, info, info.copy, view, view.linked, system, "", danger` |
| `app/own/1209-jI.js:42` | `action, close, **""**, tablist, **""**` |
| `app/own/1823-RJ.js` (editor context menu) | `title, correction, spellcheck, open, selection-link, selection, selection.format, selection.paragraph, selection.paragraph.list, selection.paragraph.heading, selection.paragraph.block, selection.insert.basic, selection.insert.advanced, insert, clipboard, info, info.copy, action, view, "", danger` |
| `app/own/1478-Wq.js:3` (formatting) | `<prefix>.basic, <prefix>.advanced, <prefix>.danger` — `prefix` is `"selection"` (`1535-ej.js:1427`) or `"selection.format"` (`1823-RJ.js:1593`) |
| `app/own/1341-oN.js:114, 417` | `title, action-primary, action, action.sort, "", danger` / `title, action, action.changeType, selection, clipboard, "", danger` |
| `app/own/1836-f1.js` | `zoom, spread, appearance` / `view, action` |
| `app/own/2044-sne.js:719` | `status, selection, "", global` |
| `app/own/1840-x1.js` | `action, selection, annotation` |
| `app/own/1955-A3.js` | `edit, clipboard, type, "", danger` |
| `app/own/1972-Z5.js:2180` | `action, resize, sort, "", danger` |
| `app/own/2020-O7.js` | `title, canvas, open, action-primary, action, info, view, system, "", danger` / `action, file, ""` |
| `app/own/1963-s5.js` | `title, options, danger` / `open, danger` |
| `app/own/1533-QW.js`, `1961-Q3.js` | `view, ""` |
| `app/own/1939-m4.js:1832`, `1813-CZ.js:723, 1403` | `action, danger` / `action, "", danger` |
| `app/own/1969-L5.js` | `danger` (alone) |
| `app/own/1951-e3.js:31` | `title, image` |
| `app/own/1336-JR.js:314`, `1481-_q.js:202`, `1647-tK.js:48, 76`, `1851-h0.js`, `1967-M5.js`, `2043-Xte.js` | the file-explorer / vault / tab family |
| `app/own/1336-JR.js:314` | `title, open, action-primary, action, info, view, system, "", danger` |

Ids §14 item 2 never lists: `correction`, `spellcheck`, `selection-link`, `insert`, `clipboard`, `selection.format`, `selection.paragraph{,.list,.heading,.block}`, `selection.insert.{basic,advanced}`, `action.sort`, `action.changeType`, `zoom`, `spread`, `appearance`, `annotation`, `edit`, `type`, `image`, `options`, `status`, `global`, `file`, `resize`, `sort`, `canvas`, `find`. Max depth is **three** segments.

The two §2 rows that hold: `1204-gI.js` matches §2's folder list exactly, and `view, ""` matches §2's markdown-preview row (two sites, `1533-QW.js` / `1961-Q3.js`).

### CORRECTION 6: the leaf-tab menu **does** call `addSections`, and the call passes `""` twice

```js
// app/own/1209-jI.js:42  (verbatim)
              n.dom.addClass("mod-tab-list"), n.setParentElement(e), n.addSections(["action", "close", "", "tablist", ""]), …
```

§2's row ("Leaf tab (`WorkspaceLeaf.js:80-128`): `action`, `close`, `tablist` (no explicit `addSections`)") is wrong on both counts. The `""` duplication is not cosmetic — it proves:

### CORRECTION 7: `addSections` does **not** dedupe within its own argument

```js
// app/own/0751-my.js:99-105  (verbatim)
t.prototype.addSections = function(e) {
  var t = this.sections;
  e = e.filter(function(e) { return !t.contains(e); });
  var n = t.indexOf("");
  return -1 === n && (n = t.length), t.splice.apply(t, k([n, 0], e, !1)), this;
};
```

The filter tests against `this.sections`, which is not mutated during the pass, so an id repeated *inside the argument* survives. `sections[]` then contains `""` twice and `sort()` walks the `r[""]` bucket **twice** (`0751-my.js:57-88`), appending every uncategorised item twice to `this.items`. Because `appendChild` moves an existing node, the DOM looks right but arrow navigation visits each uncategorised row twice and `selected` indices are off. §7's "(idempotent, dedupes)" is true only across *successive calls*.

`addSections` also inserts the whole batch **at the index of the existing `""`** (or at the end), which §1 gets right.

### CORRECTION 8: `openSubmenuSoon` is a leading-edge **throttle**, not a debounce

```js
// app/own/0751-my.js:5  (verbatim)
n.unloading = !1, … n.openSubmenuSoon = Ml(n.openSubmenu.bind(n), 250, !0), n.scope = new ly(uy.global.getRootScope());
```

The third argument is `immediate = true`, so the first hover of a burst arms the deadline and later hovers only push the deadline out (`app/own/0355-Ml.js:2`, `h = function() { … return i ? n ? a = s = p + t : … : h; }`). The observable consequence §9's "250 ms hover" gloss hides: **sweeping the pointer across a menu opens each submenu after the pointer has already moved on**, rather than only after it settles. `Ml` also carries `.cancel()` and `.run()`, used at `0751-my.js:267`, `:292`, `:317`.

### NEW: `Menu.forEvent` shows **asynchronously** and the two Menu backends diverge on two behaviours

```js
// app/own/0751-my.js:320-325  (verbatim)
t.forEvent = function(e) {
  e.preventDefault();
  var n = gy.get(e);
  return n || (n = new t(), gy.set(e, n), e.win.setTimeout(function() { return n.showAtMouseEvent(e); })), n;
};
```

Three things the doc's §1 "lazy per-event singleton via internal `WeakMap<Event, Menu>`" omits:
1. `forEvent` calls `e.preventDefault()` on the caller's event.
2. **It returns an unshown menu and shows it on the next tick** via `e.win.setTimeout`. Callers populate via `addItem` *after* `forEvent` returns and before the timer fires; the menu is sorted at show time, so items added synchronously are fine and items added in a later microtask are not.
3. The `WeakMap` is keyed by event and never deleted.

**Backend divergence.** The DOM path and the Electron-native path (`useNativeMenu && isDesktop`, `:142`) do not agree on two points:

| | DOM path | Native path (`app/own/0751-my.js:171-205`) |
|---|---|---|
| Selecting a label row | `onArrowUp/Down` skip only `r.disabled` (`:237`), **not** `.is-label` — a label is arrow-reachable and Enter fires its handler | `enabled: !e.disabled && !e.dom.hasClass("is-label")` (`:177`) — labels are inert |
| Choosing a leaf item in a **submenu** | `this.menu.hide()` (`app/own/0749-py.js:47`) hides only that submenu; `Menu.hide` never walks `parentMenu` (`:208-228`), so **the root menu stays open** | Electron closes the whole native menu, and `menu-will-close` fires `hide()` on the root (`:195-197`) |

A Corbomite `QMenu` implementation has to pick one; Obsidian itself does not.

Also new in `showAtPosition`: **an empty menu shows nothing at all** (`if (this.unload(), 0 === this.items.length) return this;`, `:131`) — no dim layer, no positioning. `setParentElement` adds class `has-active-menu` and arms a **500 ms auto-hide** (`fm(this.parentEl, 500, () => n.hide())`, `:140-142`); `setNoIcon()` adds `mod-no-icon` (`:108-109`). The dim layer is a separate `createDiv("suggestion-bg")` appended alongside the menu with a hard-coded `"0.85"` opacity (`:9`, `:167`).

### NEW: the menu-construction handshake order (plugin event fires **before** `sort()`)

```js
// app/own/0754-wy.js:168-186  (verbatim, the "…" button on a leaf tab header)
}, t.prototype.onMoreOptions = function(e) {
  e.preventDefault();
  var t = new my().addSections(["close", "pane", "open", "action", "find", "info", "info.copy", "view", "view.linked", "system", "", "danger"]);
  t.setSectionSubmenu("info.copy", { title: bd.interface.menu.copyPath(), icon: "lucide-clipboard" }),
  t.setSectionSubmenu("view.linked", { title: bd.interface.menu.openLinkedView(), icon: "lucide-link" }),
  this.onPaneMenu(t, "more-options"), this.onMoreOptionsMenu(t),
  this.app.workspace.trigger("leaf-menu", t, this.leaf);
  var n = e.target, i = n.getBoundingClientRect();
  t.setParentElement(n).showAtPosition({ x: i.x, y: i.bottom, …
```

So the canonical order is `addSections` → `setSectionSubmenu` → view callbacks → `workspace.trigger(…)` → `setParentElement` → `showAtPosition` (which calls `sort()`). **Plugin-added items are sorted like everything else**, because the event fires before `showAtPosition`. §10's "New menu section" row and §14 item 2 should state that.

§10 also says `setSectionSubmenu` is "rarely used by plugins" — first-party code uses it at `0754-wy.js:171, 174` and `1823-RJ.js:1593` (`setSectionSubmenu("selection.paragraph", …)`), so the dotted-prefix rule is exercised in production, not an edge case.

### CORRECTION 9: `Modal.open` listens for `pagehide`, and `Modal.close()` has **no** idempotency guard

§5's row says "`Modal.open` (`Modal.js:156-157`) | `beforeunload` on popped-out windows". The real event and guard:

```js
// app/own/0817-Yb.js:720 / :773  (verbatim)
n !== window && n.addEventListener("pagehide", this.onWindowClose);
…
n && n !== window && n.removeEventListener("pagehide", this.onWindowClose);
```

And:

```js
// app/own/0817-Yb.js:688-691 — open()'s guard
e.prototype.open = function() {
  var e = this.app, t = this.containerEl;
  if (!t.parentNode) { … }
```
```js
// app/own/0817-Yb.js:748-758 — close(): no guard at all
e.prototype.close = function() {
  var e = this, t = this.app, n = this.win;
  t.keymap.popScope(this.scope), this.win = null, vy(), Qg();
  var i = this.getRootEl(),
    r = function() { var t; i.detach(), e.onClose(), null === (t = e.onCloseCallback) || void 0 === t || t.call(e); };
  rd.isPhone ? this.animateClose().then(r) : r(), vg.pop(this), this.shouldRestoreSelection && …
```

§8's "`Modal.open()` / `close()` are both idempotent via a `containerEl.parentNode` guard" is **half wrong**: only `open()` guards. `close()` unconditionally pops the scope, detaches, and calls `onClose()` + `onCloseCallback` on **every** invocation — a subclass `onClose()` with side effects runs twice on a double `close()`.

Two more orderings that matter:

- **`onClose()` is asynchronous on phone.** `close()` runs `animateClose().then(r)`, and `r` is what calls `detach()` + `onClose()` + `onCloseCallback`. On desktop `r()` is synchronous. So a Corbomite port cannot assume `onClose()` has run when `close()` returns.
- **`onClose()` runs before selection restore**, which itself runs before the `pagehide` listener is removed (`:758` → `:773`).

### NEW: Modal's platform- and event-gated dismissal

```js
// app/own/0817-Yb.js:634-642  (verbatim)
if (i.addEventListener("click", this.close.bind(this)), rd.isDesktopApp && rd.isMacOS) {
  var r = null, o = null;
  n.addEventListener("click", function(e) { Math.hypot(e.screenX - r, e.screenY - o) < 5 && t.close(); }),
  rd.isMacOS && n.addEventListener("mousedown", function(e) { r = e.screenX, o = e.screenY; });
} else n.addEventListener("click", this.onClickOutside.bind(this));
```

```js
// app/own/0817-Yb.js:776-779  (verbatim)
e.prototype.onClickOutside = function(e) { e.defaultPrevented || this.close(); },
e.prototype.onEscapeKey = function(e) { e.defaultPrevented || (e.preventDefault(), this.close()); },
```

Three facts §1/§9's "close on Esc / click-outside" does not carry:
- **On macOS desktop the background only closes the modal if the pointer moved < 5 *screen* px between mousedown and click** (`screenX`/`screenY`, not client). A drag that starts inside the modal and ends on the background leaves it open.
- **On macOS desktop `onClickOutside` is not registered at all** — the 5 px rule *replaces* it, so a plugin overriding `onClickOutside` gets no effect on that platform.
- **Both dismissal paths are gated on `defaultPrevented`** (and Escape `preventDefault()`s on the way out).

### NEW: focus is moved into the modal only when a physical keyboard is present

```js
// app/own/0817-Yb.js:718  (verbatim, tail of open())
…, vg.push(this, n), rd.hasPhysicalKeyboard && this.hasInitialInputFocus && Fm(r, { preventScroll: !0 }),
```

plus `this.scope.setTabFocusContainerEl(this.containerEl)` (`:634`) and the undocumented field `hasInitialInputFocus = !0` (`:630`). So on a touch-only device a modal opens **without stealing focus**, and `ConfirmationModal.open()` sets `hasInitialInputFocus = !1` when a button asked for initial focus and focuses that button by hand instead (`:854-861`).

### NEW: `setDimBackground` / `setBackgroundOpacity` are field-only and inert after `open()`

```js
// app/own/0817-Yb.js:784-789  (verbatim)
e.prototype.setBackgroundOpacity = function(e) { return this.bgOpacity = e, this; },
…
e.prototype.setDimBackground = function(e) { return this.dimBackground = e, this; },
```

`open()` reads them once (`:717-718`, `t.toggleClass("mod-dim", a), … o.style.opacity = a ? this.bgOpacity : "0"`). Calling either after `open()` changes nothing visible.

### CONFIRMED: the rest of the Modal contract, with citations

| Doc claim | 1.13.7 |
|---|---|
| parentless `Scope`, pushed/popped manually | `:630` `this.scope = new ly()`, `:713` `pushScope`, `:752` `popScope` (childless; contrast `Menu`'s `new ly(uy.global.getRootScope())` at `0751-my.js:5` and `PopoverSuggest`'s `new ly(t \|\| e.scope)` at `1319-xR.js:29`) |
| DOM: `containerEl > bgEl + modal` | `:631-632` `modal-container > .modal-bg + .modal` |
| fields `selection`, `win`, `shouldAnimate`, `dimBackground`, `bgOpacity:"0.85"` | `:630` all present with those defaults |
| `getRootEl()` returns `containerEl`; `doc` getter | `:680-687` (`doc` is a **non-enumerable** `defineProperty`) |
| selection restore skips CodeMirror `.cm-content` | `:764` `n.instanceOf(HTMLElement) && n.hasClass("cm-content")`; range restore also requires both endpoints still in the same document (`:765` `hm(t, i.startContainer) && hm(t, i.endContainer)`) |
| selection snapshot descends into iframes, nulls `body` | `:693-707` |
| focus restore accepts `HTMLElement` **or** `SVGElement` | `:769` |
| Esc/`onEscapeKey`, `onClickOutside`, `setTitle`, `setContent`, `setCloseCallback`, phone swipe-down-to-dismiss on the title bar | `:778-779`, `:776-777`, `:780-781`, `:782-783`, `:786-787`, `:643-678` |
| modal close button is a `.modal-header-button.mod-raised.clickable-icon` created **before** the header | `:633-634` — and the icon is the legacy id **`"x"`**, not `lucide-x` |

### NEW: `SuggestModal` throws away the whole `Modal` chrome, and its default empty state is a *plugin-settings* string

```js
// app/own/0819-ww.js:143-165  (verbatim, abridged only where marked)
    function SuggestModal(t) {
      var n = e.call(this, t) || this;
      n.limit = 100,
      n.emptyStateText = bd.setting.thirdPartyPlugin.labelNoResultsFound(),
      n.isOpen = !1, n.inputEl = null, n.clearButtonEl = null, n.ctaEl = null,
      n.resultContainerEl = null, n.instructionsEl = createDiv("prompt-instructions");
      var i = n.modalEl;
      return i.removeClass("modal"), i.addClass("prompt"), i.empty(), i.createDiv("prompt-input-container", function(e) {
        rd.isPhone && e.addClass("mod-raised");
        var t = n.inputEl = e.createEl("input", { cls:"prompt-input", type:"text", attr:{
          autocapitalize:"off", spellcheck:"false",
          enterkeyhint: rd.isAndroidApp ? "enter" : "done" } });
        n.ctaEl = e.createDiv("prompt-input-cta"),
        n.clearButtonEl = e.createDiv("search-input-clear-button", function(e) {
          e.addEventListener("mousedown", function(e) { return e.preventDefault(); }),
          e.addEventListener("click", function() {
            "" !== t.value ? (t.value = "", t.trigger("input")) : n.close();
          });
        });
      }),
      n.resultContainerEl = i.createDiv("prompt-results"),
      n.chooser = new ww(n, n.resultContainerEl, n.scope),
      n.inputEl.addEventListener("input", n.onInput.bind(n)), n;
```

- **`modalEl` loses `modal`, gains `prompt`, and is `empty()`ed** — the close `×` button and `.modal-header`/`.modal-title` built by `Modal`'s constructor are destroyed. A `SuggestModal` therefore has **no close button and no title**; Esc, the background click, and the clear button are its only exits.
- **The default no-results text is `bd.setting.thirdPartyPlugin.labelNoResultsFound`** = `"No results found."` (`src/i18n/tree.js:443`, path `setting` `:2` → `thirdPartyPlugin` `:395` → `:443`). A Corbomite suggester that ships a different string diverges on a visible string. (A second identical string exists at `i18n/tree.js:270` under `setting.hotkeys`; that is a different key.)
- **The clear button closes the modal when the input is already empty**; otherwise it clears and synthesises an `input` event via `t.trigger("input")`, which re-runs `onInput` (`:162`, verbatim `"" !== t.value ? (t.value = "", t.trigger("input")) : n.close();`).
- `.prompt-input-cta` is **created and exposed as `ctaEl` but no `addEventListener` is attached to it anywhere in `app/own/`** — a dangling, plugin-facing hook. It has CSS only under `.is-mobile` (`asar/obsidian/app.css:20949`).
- `.prompt-input-container` gets `mod-raised` on phone. `instructionsEl` (`.prompt-instructions`) is created **detached** and only appended by `setInstructions` when the array is non-empty (`:169-170`).
- `onOpen` clears the input, focuses it, then populates (`:171-173`).

### NEW: `SuggestModal` and `AbstractInputSuggest` treat an empty result set **oppositely**

```js
// app/own/0819-ww.js:197-198  — modal: empty ⇒ render a message row
SuggestModal.prototype.onNoSuggestion = function() {
  this.chooser.setSuggestions(null), this.chooser.addMessage(this.emptyStateText);
};
```
```js
// app/own/1319-xR.js:79-86  (verbatim) — popover: empty ⇒ close
t.prototype.showSuggestions = function(e) {
  var t = this.textInputEl;
  if (0 !== e.length) {
    if (t.isShown()) {
      var n = this.limit;
      n > 0 && e.length > n && (e = e.slice(0, n)), this.suggestions.setSuggestions(e), this.open(), this.setAutoDestroy(t);
    }
  } else this.close();
};
```

`showSuggestions` is doubly gated: results are **discarded** (not cached) if the input is hidden, and an empty array closes the popover. There is no "no results" row for the popover family.

Other new `AbstractInputSuggest` behaviour, all at `1319-xR.js`:
- **mousedown on a suggestion is `preventDefault`ed** (`:75-77`) so the anchor input never blurs before the click lands — the reason `blur → close` (`:75`) is safe.
- **`getSuggestions` only runs while the anchor has focus** (`:98` `if (this.textInputEl.isActiveElement())`).
- **On a device with no physical keyboard the initial fetch is deferred** (`:93` `rd.hasPhysicalKeyboard ? this.onInputChange() : lv(function() { return e.onInputChange(); })`).
- `getValue`/`setValue` branch on `instanceOf(HTMLInputElement)` and otherwise use **`innerText`** (`:88, 90`) — the contenteditable path.
- The scroll listener is `{capture: !0, passive: !0}` (`:130-133`) and `autoReposition` only repositions when the rect actually changed (`:134-138`).
- File suggester iterates **`app.vault.getAllLoadedFiles()`**, not `getFiles()` (`:150`), and its rows get class **`mod-nowrap`** (`:146`).
- Folder suggester constructor sets `includeRoot` twice — `o.includeRoot = !1, o.allowNullSelection = i, o.includeRoot = r` (`:189`) — and its null-selection row is appended **last**, rendering as `"+ " + this.getValue()` (`:193`, `:206`).
- **`OR` (folder + predicate) cannot set `includeRoot` and shifts its 3rd argument**: `e.call(this, t, n, r)` passes the caller's 4th argument into the base's `allowNullSelection` slot (`:216`). So `new OR(app, el, predicate, true)` yields `allowNullSelection === true, includeRoot === false`.

### NEW: the list scaffold (`ob`/`ab` → `ww`/`kw`) — three behaviours worth porting and one latent crash

```js
// app/own/0819-ww.js:23-32  (verbatim)
Object.defineProperty(e.prototype, "rowHeight", { get: function() {
  return this.suggestions[this.selectedItem].clientHeight; }, enumerable: !1, configurable: !0 }),
Object.defineProperty(e.prototype, "numVisibleItems", { get: function() {
  return Math.floor(this.containerEl.clientHeight / this.rowHeight); }, enumerable: !1, configurable: !0 }),
```

- **`rowHeight` reads the *currently selected* row's `clientHeight`** — so if the selected row wraps, page-up/down jump by the wrong amount.
- **`PageUp`/`PageDown` throw on an empty list.** `onNoSuggestion()` leaves `suggestions === []` (via `setSuggestions(null)` → `renderSuggestions`), and neither `pageUp` (`:35-45`) nor `pageDown` (`:47-58`) guards for it, while `rowHeight` dereferences `suggestions[0].clientHeight`. Arrow/Home/End/`Enter` *are* guarded (`setSelectedItem` checks `0 !== n.length` at `:90`; `useSelectedItem` checks `!this.values` at `:113`). A Corbomite list view must not copy the unguarded read.
- **Keyboard navigation deliberately does not scroll**: `a && (a.addClass("is-selected"), t && !t.instanceOf(KeyboardEvent) || a.scrollIntoView({ block: "nearest" }))` (`:98-99`) — note the custom `instanceOf` helper, and that `scrollIntoView` is skipped when the trigger *is* a `KeyboardEvent`.
- `selectOnHover` defaults to **true**, with a public `shouldSelectOnHover(flag)` opt-out (`:86-88`). Hover-select is a first-party default a desktop port should match.
- The delegated bindings use a selector-helper: `t.on("click", ".suggestion-item", …)`, `t.on("auxclick", …)`, `t.on("mousemove", …)` (`:5`) — **middle-click (`auxclick`) also chooses** (`:101-106`, guarded by `!e.defaultPrevented`).
- Keyboard nav is registered on the **Modal's own scope** (`:165` `new ww(n, n.resultContainerEl, n.scope)`), so a `SuggestModal`'s list keys are unreachable from anywhere the modal's parentless scope isn't active.

### CORRECTION 10: `FuzzySuggestModal` **does** support an async `getSuggestions`

§8 says "`FuzzySuggestModal.getSuggestions` handles async only if `getItems()` is sync; full-async plugins must override `getSuggestions` directly" — read as "async is unsupported", it is wrong. The async handling lives one level up and is generic:

```js
// app/own/0819-ww.js:176-196  (verbatim, abridged)
SuggestModal.prototype.updateSuggestions = function() {
  var e = this, t = this.inputEl.value, n = this.getSuggestions(t),
    i = function(t) { if (t && 0 !== t.length) { var n = e.limit; n && n > 0 && (t = t.slice(0, n)), e.chooser.setSuggestions(t); } else e.onNoSuggestion(); };
  Array.isArray(n) ? i(n) : y(e, void 0, void 0, function() { … e = i, [4, n] … });
};
```

Any subclass may return a promise from `getSuggestions`; `updateSuggestions` awaits it. The real limitation is narrower: the **default** `FuzzySuggestModal.getSuggestions` is synchronous because `getItems()` is (`:211-221`). Also new in that default:
- the query is **`trim()`ed** first (`:212`),
- `sortSuggestions(results)` is an **overridable public method** the doc omits (`:209-211`), and
- the sort is a bare descending score with **no tiebreak**:
  ```js
  // app/own/0785-nb.js:2-6  (verbatim)
  function nb(e) { e.sort(function(e, t) { return t.match.score - e.match.score; }); }
  ```
  so ties keep `getItems()` order (stable sort). A Corbomite quick-switcher must use a *stable* descending sort or first-result ranking diverges on ties.

Also new: `selectSuggestion` updates the keymap's modifier state **from the choosing event, before the handler runs, and closes before calling back**:
```js
// app/own/0819-ww.js:199-200  (verbatim)
SuggestModal.prototype.selectSuggestion = function(e, t) {
  this.app.keymap.updateModifiers(t), this.close(), this.isOpen = !1, this.onChooseSuggestion(e, t);
};
```

### CONFIRMED: `Notice` — with four corrections

| Doc claim | 1.13.7 |
|---|---|
| `new Notice(msg, durationMs = 4000)` | `app/own/0819-ww.js:243` |
| `durationMs = 0` disables auto-hide | `:264` `if (e)` |
| per-window `.notice-container` singleton in `WeakMap`, detached when empty | `:240` `Dw = new WeakMap()`, `:247-248`, `:292-293` |
| `.notice` child div, `.notice-cta` button container | `:249`, `:278-280` |
| desktop slide-in `translateX(350px)` → `""`; mobile `translateY` | `:253` |
| hover pauses auto-hide; leave re-arms +1 s | `:269-273` — `mouseenter` sets a flag, `mouseleave` arms `win.setTimeout(r, 1e3)` |
| click anywhere hides | `:253-255` |
| **new-at-bottom stacking (§13 Q7)** | **answered: `r.createDiv("notice")` appends, so new notices land at the bottom** |
| **exit animation (new)** | desktop: 120 ms, `opacity` 1→0 plus `margin-top` 0→`-(offsetHeight + marginBottom)` (`:299-306`); mobile: 150 ms `scale(1,1)`→`scale(0.85,0.85)` + fade (`:296-298`) |
| **`addButton` order (correction)** | `:281-283` `n.hide(), t(e)` — the notice is hidden **before** the callback runs, and the callback's own click bubbles to `.notice`'s handler, so `hide()` is entered twice |
| **`setMessage` (new)** | `:259-260`; `messageEl === noticeEl` are the same node (`:250`) |
| **`setAutoHide` leak (new)** | `:269, 271` attach a **fresh** `mouseenter`/`mouseleave` pair on every call; only the timeout is cleared |
| phone swipe-dismiss (new) | `:255-257` `yy(o, o, null, …)` |

CSS contract worth reproducing: `.notice-container` is `position: fixed; inset-inline-end: 0; top: 22px; z-index: var(--layer-notice); pointer-events: none` (`asar/obsidian/app.css:8704-8712`) — children re-enable with `pointer-events: auto`, which is why only `.notice` is clickable. `.notice` is `max-width: 300px; white-space: pre-wrap; cursor: var(--cursor)` (`:8713-8731`), which is why the JS's `margin-bottom` compensation exists (it matches `.notice { margin-bottom: 14px }` at `:8729`).

Dead CSS, for completeness: `.notice.is-loading` and `.notice progress` (`asar/obsidian/app.css:8732-8741`) have **no producer in `app/own/`** — the only two `addClass("is-loading")` sites target a modal `containerEl` (`app/own/0669-km.js:8`, `app/own/0829-Ww.js:363`).

### NEW: `HoverPopover` — the third `shouldShowSelf` term, the forced incumbent hide, and a shared 10-shot resize budget

`HoverPopover` is `U$` at `app/own/1794-N$.js:52-186`. Confirmed from the doc: `extends Component` (`:187`), ctor `(parent, targetEl, waitTime = 300, staticPos = null)` (`:54`), starts `Showing` (`:56`), 80 ms `var(--anim-motion-swing)` fade on `show()` (`:103-106`), `hide()` cascades (`:113-116`), the 500 ms `window.setInterval` poll with `elementFromPoint` (`:37-39`, `:50`), capture-phase `click`/`contextmenu` outside-hide (`:30-31`, `:46-49`), `setIsFocused` pinning (`:92-93`), and `childHovers` as a live filter over the visible list (`:79-87`).

New:

1. **`shouldShowSelf()` has a third term the doc omits — focus containment:**
   ```js
   // app/own/1794-N$.js:78  (verbatim)
   return this.onTarget || this.onHover || this.isFocused || e.contains(e.doc.activeElement);
   ```
   So a popover whose `hoverEl` contains `document.activeElement` stays open. This is what makes "click a link inside the preview" viable, and it is *independent* of `setIsFocused`.
2. **`onShow()` force-hides the incumbent on the same parent:**
   ```js
   // app/own/1794-N$.js:118-120  (verbatim)
   t.prototype.onShow = function() {
     var e = this.parent;
     e.hoverPopover && e.hoverPopover.hide(), e.hoverPopover = this, Qg();
   };
   ```
   `parent.hoverPopover` is a **single slot**; the previous occupant is hidden, not merely displaced. That is the mechanism behind §8's "at most one popover per `parent.hoverPopover`", and it partially answers §13 Q8: the contract is one `hoverPopover` property on any `Component`, assigned by `onShow` and cleared by `onHide` (`:121-123`).
3. **The outside-click hide has a 5 ms grace and is two-phase:** `z$` (`:6-21`) hides all *pending* popovers immediately, then schedules `win.setTimeout(…, 5)` for the visible ones, skipping any that are `isFocused` or whose `hoverEl`/`childHovers` contain the click target. The grace exists so a click that opens a *new* popover settles first.
4. **`position()`'s mouse anchor is a zero-width, 20 px-tall box** (`top: H$.y - 10, bottom: H$.y + 10, left: H$.x, right: H$.x`, `:132-137`), the `offsetHeight > 300` test lives in `position()` not `show()`, and **the decision is sticky** — it assigns `this.staticPos = H$` (`:132`). The gap is **10** for `HoverPopover` (`:167`) versus **5** for `PopoverSuggest` (`1319-xR.js:57`), and `horizontalAlignment` comes from the *target's* computed `direction` (`:161`).
5. **`watchResize`'s 10-call cap is cumulative across every observed element and then permanent:**
   ```js
   // app/own/1794-N$.js:170-182  (verbatim)
   t.prototype.watchResize = function(e) {
     var t = this;
     if (!this.observer) {
       var n = 0;
       this.observer = new ResizeObserver(function() {
         var e;
         if (t.state === M$.Shown) {
           if (++n >= 10) return null === (e = t.observer) || void 0 === e || e.disconnect(), void(t.observer = null);
           t.position();
         }
       });
     }
     this.observer.observe(e);
   };
   ```
   §12 calls this "trivial". The sharp version: one observer per popover, counter shared across all observed elements, and after the tenth resize callback the observer is dropped for good — repositioning silently stops for the rest of the popover's life. `onunload` disconnects (`:183-185`).
6. `hoverEl` listeners are `mouseover`/`mouseout` **guarded by the enters-element test** `Dv(e, a)` (`:60-64`, `:66-69`) — the same helper the rendering addendum identified, so moving between popover children is not a leave.

### NEW: `PopoverSuggest`'s scope parent and the missing detach

```js
// app/own/1319-xR.js:29-33  (verbatim)
      this.isOpen = !1, this.win = null, this.app = e, this.scope = new ly(t || e.scope),
      this.suggestEl = createDiv("suggestion-container");
      var i = this.suggestInnerEl = this.suggestEl.createDiv("suggestion");
      this.suggestions = new ww(this, i, this.scope),
      this.scope.register([], "Escape", function(e) { return n.onEscapeKey(e), !1; });
```

So the three scope parents in this domain are **three different things** — a fact the doc's single "menu/modal/popover keyboard scope" phrasing hides and that decides shortcut arbitration in Corbomite:

| Class | Scope construction | Effect |
|---|---|---|
| `Menu` | `new ly(uy.global.getRootScope())` (`0751-my.js:5`) | child of the **global root** |
| `Modal` | `new ly()` (`0817-Yb.js:630`) | **parentless** — root-scope hotkeys unreachable while open |
| `PopoverSuggest` | `new ly(t \|\| e.scope)` (`1319-xR.js:29`) | child of **`app.scope`** (or a caller-supplied scope) |

Also new: `attachDom()` fades in over **80 ms** with `var(--anim-motion-swing)` (`:39-44`) — the doc mentions this only for `HoverPopover`. `reposition`'s second argument is a **tri-state** (`"ltr" | "rtl" | "auto"`), with `"auto"` resolved from `getComputedStyle(suggestEl).direction` (`:53-64`). `close()` runs `autoDestroy` first, pops the scope, and only then does the `isOpen` work — and it **clears the list** via `suggestions.setSuggestions([])` (`:50-52`). `onHistoryBack()` also closes (`:37-38`), as on `Modal` and `Menu`.

### NEW: `vg` is not a menu stack — it is a `history.back/forward` overlay stack

The doc's §15 lists `Wg` as the "current-menu singleton". Both halves are wrong. `vg` is an instance of a per-window overlay stack that **monkey-patches `window.history`**:

```js
// app/own/0712-fg.js:6-14  (verbatim)
e.prototype.patchWindow = function(e) {
  var t = this;
  e.history.back = function() { return t.back(e); },
  e.history.forward = function() { return t.forward(e); },
  e.history.go = function(n) { return t.go(e, n); };
};
```
```js
// app/own/0712-fg.js:23-28  (verbatim)
e.prototype.back = function(e) { var t; null === (t = null === (e = this.getStack(e)).last()) || void 0 === t || t.onHistoryBack(); },
e.prototype.forward = function(e) { … t.onHistoryForward … },
```

So `Menu`, `Modal` and `PopoverSuggest` all implement `onHistoryBack()` → close, and that is how **browser/Electron Back closes an overlay** instead of navigating. `onHistoryForward` is implemented by **exactly one** participant in the whole app bundle — `Workspace` itself (`app/own/1932-r4.js:62-70`, desktop only), which forwards to `activeLeaf.history.forward()`:

```js
// app/own/1932-r4.js:62-72  (verbatim, abridged)
rd.isMobile || vg.push({ onHistoryBack: function() { … i.activeLeaf?.history.back(); },
                         onHistoryForward: function() { … i.activeLeaf?.history.forward(); } }, window),
rd.isLinux || window.addEventListener("mousedown", function(e) {
  3 !== e.button && 4 !== e.button || (e.preventDefault(), e.stopPropagation(),
    3 === e.button ? window.history.back() : window.history.forward());
}, { capture: !0 }),
```

Note also `window.addEventListener("popstate", e => { vy(), e.preventDefault(); })` (`:60-61`) — popstate is swallowed and its work deferred to `vy()`, and on **Linux desktop, X-buttons 3/4 are mapped to `history.back()`/`history.forward()`**. This is a cross-cutting contract Corbomite's back/forward stack needs for overlays, and §15's "current-menu singleton" is actually **`fy`** (`app/own/0749-py.js:59`).

`Wg` is instead a one-line timer cancel feeding `Qg()`, the global "abort any in-flight drag / drop the drag tooltip" call that appears in `Menu.showAtPosition` (`0751-my.js:142`), `Modal.open`/`close` (`0817-Yb.js:718`, `:752`), `ConfirmationModal.onClose` (`:863`), and `HoverPopover.onShow` (`1794-N$.js:120`):

```js
// app/own/0736-Qg.js:2-4  (verbatim)
function Qg() { Wg(), Vg && (Yg = Date.now(), Vg.detach(), Vg = null, Hg = null); }
// app/own/0728-Wg.js:2-4  (verbatim)
function Wg() { qg && (clearTimeout(qg), qg = null); }
```

### NEW: `Component.load()` is async-aware and returns a promise

§1 describes `load()` as "recurses into children and calls `onload()`". It also **collects and awaits promises**:

```js
// app/own/0712-fg.js:44-55  (verbatim)
Component.prototype.load = function() {
  if (!this._loaded) {
    this._loaded = !0;
    var e = [], t = this.onload();
    t && e.push(t);
    for (var n = 0, i = this._children.slice(); n < i.length; n++) {
      var r = i[n].load();
      r && e.push(r);
    }
    return e.length > 0 ? Promise.all(e).then() : void 0;
  }
};
```

- `onload()` may return a promise; `load()` returns `Promise<void> | undefined`. The doc's §2 invariant block omits this, and any Corbomite `Corbomite::Component` that mirrors only the sync behaviour will drop async teardown ordering. `View.open()` consumes it (`0754-wy.js:13`).
- `_loaded` is set **before** `onload()` runs, and children are iterated over a `.slice()` copy.
- `registerScopeEvent(e)` cleans up via **`e.scope.unregister(e)`** (`:83-86`) — the `EventRef` must carry its own `.scope`. §1 lists the method; §2 does not give the cleanup.

§2's remaining invariants hold verbatim: `_children` LIFO before `_events` LIFO before `onunload()` (`:56-66`), `registerDomEvent` stores the same `opts` for add and remove (`:79-82`), `registerEvent` → `ref.e.offref(ref)` (`:75-78`), `registerInterval` → `clearInterval` and returns `id` (`:87-90`), `addChild` auto-loads when already loaded and returns the child (`:67-68`), `removeChild` unloads and returns the child (`:69-72`).

### NEW: `Setting` / `SettingGroup` / `SettingPage` — the settings-dialog keyboard contract

§1 says `Setting` is "Out-of-domain; see `obsidian/settings/`". It is a first-class plugin export (`app/own/0025-expressionstatement.js:72-74` `Setting: () => Yb, SettingGroup: () => Xb, SettingPage: () => Qb`) and it carries behaviour Corbomite's settings dialog would otherwise miss. `Setting` is `Yb` at `app/own/0817-Yb.js:3-104`:

- Row DOM: `settingEl.setting-item[tabindex=-1] > .setting-item-info > (.setting-item-name, .setting-item-description)` + sibling `.setting-item-control` (`:5-12`).
- `addToggle` adds class **`mod-toggle`** to the row (`:51`); `setHeading` adds `setting-item-heading` (`:33-34`).
- `setErrorMessage(msg)` lazily creates **`.setting-item-error` inside `controlEl`** and toggles **`is-invalid`** on the row (`:14-16`) — Corbomite's error styling hook.
- `setNavigable(cb)` adds `mod-navigable tappable` **and a `.setting-item-chevron` div with `lucide-chevron-right`** (`:91-94`); `setAction(cb)` adds `mod-action tappable` (`:95-96`).
- `setRowClick` registers the listener **once** and checks `e.defaultPrevented || t.disabled`, so a disabled row swallows the click rather than letting it through (`:97-102`).
- `setClass(e)` **splits on whitespace and `addClass`es each token** (`:28-30`) — additive, never removes.
- `setTooltip` targets **`nameEl`**, not the row (`:31-32`).
- **`addText` on a device without a physical keyboard blurs the input on Enter**: `!rd.hasPhysicalKeyboard && n.addEventListener("keydown", e => { e.isComposing || e.defaultPrevented || "Enter" !== e.key || n.blur(); })` (`:54-59`). A Corbomite `QLineEdit` must not steal Enter for a "submit" the web build doesn't have.

`SettingGroup` is `Xb` at `:106-181`:

- DOM: `.setting-group` containing a **detached-by-default** `.setting-item.setting-item-heading` header (`.setting-item-name` + `.setting-item-control`), then `.setting-group-search[tabindex=-1]`, then `.setting-items` (`:108-115`). `setHeading(e)` prepends or detaches the header based on `!!e || this.controlEl.childElementCount > 0` (`:117-121`).
- **Two keyboard reorder verbs, both real:**
  - `onDeleteItem(cb)` — `Delete`/`Backspace` on a focused `.setting-item` inside `listEl` calls `cb(index)` then `focusNearestItem(index)` (`:137-149`, `:168-180`).
  - `onReorderItem(cb)` — **`Alt`+`ArrowUp`/`ArrowDown`** on a focused `.setting-item` moves it, clamped to `[0, length-1]`, calling `cb(from, to)` (`:150-167`).
  Both check `e.isComposing || e.defaultPrevented`. A Corbomite settings list that wants parity needs both, plus the "focus the nearest *visible* row afterwards, forwards first then backwards" rule from `focusNearestItem`.
- `addExtraButton` forces the header visible and sets `tabIndex = 0` on the button (`:133-136`).

Two widgets the doc never mentions:
- **`SettingPage`** (`Qb`, `:183-188`) — `.setting-page.vertical-tab-content` > `.setting-page-titlebar` + `.setting-page-content`, with an **empty `hide()`** (the tab container hides it). Plugin-exported.
- **The display-value widget** (`tw`, `:291-301`) — `addDisplayValue(cb)` hands you a component over `.setting-item-value` with `setValue` and `setStatus(status)`; `"warning"` creates `.setting-item-status.mod-warning` with a `lucide-alert-triangle`.

### NEW: widget details §1 gets wrong or omits

| Widget | Fact | Cite |
|---|---|---|
| `ButtonComponent` | **`setWarning()` === `setDestructive().setCta()`** — it applies *two* classes, `mod-destructive` **and** `mod-cta`. `setDestructive`/`removeDestructive` are separate public methods. | `:243-248` |
| `ButtonComponent` | `setClass(e)` is `addClass` — **additive, never removes** | `:255-256` |
| `ButtonComponent` | `mod-loading` is added **only if there is a clickCallback and not disabled**, and removed in a `finally` | `:214-233` |
| `ExtraButtonComponent` | **`setDisabled(true)` removes the `tabindex` attribute entirely** (not `aria-disabled`), so a disabled extra button leaves the tab order | `:282` |
| `ExtraButtonComponent` | DOM is `clickable-icon extra-setting-button[tabindex=0]`, default icon `lucide-settings`, activated by `Enter`/`Space` | `:264-279` |
| `ToggleComponent` | the `<label>` carries `tabIndex = 0` with a `Space`/`Enter` handler; the inner `<input>` is `tabIndex = -1`. `setValue` toggles **`is-enabled`** on the label | `:305-318`, `:327` |
| `AbstractTextComponent` | `setValue` assigns **only if `String.isString(e)`** — `setValue(null)` is a silent no-op | `:347-349` |
| `TextComponent.autoSelect` | the shared helper is **gated on `rd.hasPhysicalKeyboard`**: `(t \|\| rd.hasPhysicalKeyboard) && $m(e, () => { e.focus(), e.select(); })` — a no-op on touch-only devices unless `selectAll` is passed | `app/own/0656-lm.js:2` |
| `SearchComponent` | the clear button `preventDefault`s **mousedown** (so the input keeps focus), then on click clears, calls `onChanged()` **and** refocuses; it respects `disabled` | `:379-385` |
| `MomentFormatComponent` | `updateSample` writes `""` (not the previous sample) when the effective format is falsy; the sample is refreshed by `setValue` and `setDefaultFormat` too, not only by input events | `:426-443` |
| `ProgressBarComponent` | `setValue` **clamps to `[0, 100]`**; `setVisibility` sets the `hidden` **property**, not a class | `:497-500` |
| `ColorComponent` | `getValueRgb()` falls back to `{r:0,g:0,b:0}` on an unparseable value | `:579-584` |
| `ButtonComponent` (confirm modal subclass, `vw`) | `setInitialFocus()`, `setSecondary()` → `mod-secondary`, `setCancel()` → `mod-cancel`; async click resolves → **close iff the awaited result is falsy** (`n.sent() \|\| this.handleClose()`), so a plain sync handler closes and one returning `true` deliberately stays open | `:792-822` |

`ConfirmationModal` is `mw` at `0817-Yb.js:823-865` and `PromptModal` is its subclass at `:866-898` (both plugin-exported as `ConfirmationModal`, `app/own/0025-expressionstatement.js:18`):

- `mw` adds `mod-confirmation` to `containerEl` and a `.modal-button-container` (`:826`), and **`addCheckbox(label, cb)` takes two arguments**, rendering `<label class="mod-checkbox"><input type="checkbox" tabindex="-1">` inside the *button container* (`:830-843`).
- `mw.open()` scans for `isInitialFocus`, and if found sets `hasInitialInputFocus = false` so the modal's own autofocus is suppressed, then focuses the button by hand (`:854-861`).
- **`PromptModal.onClose()` calls `cancel()`** (`:895-897`) — so Esc / × / background all resolve the promise to `false`, and `accept()` resolves `true` **before** the button closes (`:877-879` then `:891`).
- `PromptModal` adds exactly two buttons: Cancel (`bd.dialogue.buttonCancel()`) and an Accept that is `setCta().setInitialFocus()` with an optional `setAcceptButton(cb)` hook (`:886-891`).

§11's `nb`/`ib` names are dead; §11's "Buttons auto-close unless handler returns truthy" is **confirmed** by `vw.onClick`.

### NEW: an undocumented `Modal` subclass — the form sheet — and the form-field builder

```js
// app/own/0817-Yb.js:902  (verbatim, abridged)
return n.modalEl.addClass("mod-lg", "mod-form-sheet"), rd.isPhone ? (… header gets a `mod-start` × button and a `mod-cta` check button that calls `formSubmit()` …)
  : (n.buttonContainerEl = n.modalEl.createDiv("modal-button-container"),
     n.ctaButton = new Jb(n.buttonContainerEl).setCta().setButtonText(bd.dialogue.buttonDone()).onClick(function() { return n.formSubmit(); }),
     n.ctaEl = n.ctaButton.buttonEl,
     new Jb(n.buttonContainerEl).setButtonText(bd.dialogue.buttonCancel()).onClick(function() { return n.close(); })), n;
```

`extends Modal` directly, `.modal.mod-lg.mod-form-sheet`, with `addField(cb)` returning a field object over `<p class="form-field">` (`dw` at `:606-626`, exposing `setName`/`setHelp`/`addText`/`addDropdown`/`addToggle`/`addButton`) plus `setCtaText`, `setCtaDisabled`, `setCtaLoading`, `setCtaClass`, `formSubmit()` → `onSubmit()`. Default CTA label is `bd.dialogue.buttonDone()` = `"Done"` (`src/i18n/tree.js:1117`). On phone the CTA is a header icon instead of a button. **Note `mod-form-sheet` has zero rules in `asar/obsidian/app.css`** (only `mod-lg` is styled), so it is a JS-only class.

### NEW: `SecretComponent` — three details §1 misses

`v6` at `app/own/1921-l6.js:288-330` (plugin-exported, `app/own/0025-expressionstatement.js:70`):

- The warning is `div.clickable-icon.mod-warning` with `lucide-alert-triangle` and `aria-label = bd.setting.keychain.msgSecretsNotEncryptedShort()`, toggled by `secretStorage.isEncryptionAvailable()` (`:291-295`) — it opens a modal on click.
- The reveal toggle uses the **hard-coded 7-bullet string `"•••••••"`** in a `span.setting-password` inside `span.setting-hotkey` (`:312-318`) — not a CSS mask.
- **The `<button>` label is state-dependent**: `bd.setting.keychain.buttonLink()` before linking, `bd.dialogue.buttonChange()` after (`:296`, `:323`, `:324`).
- **There is an unlink affordance** the doc omits: `span.setting-hotkey-icon` with `lucide-x` whose click does `setValue(null)` + `changeCallback(null)` (`:319-323`).
- **`setValue` does not fire `changeCallback`** — only the picker's own callback closure does (`e = function(e) { i.setValue(e), null === (t = i.changeCallback) || void 0 === t || t.call(i, e); }`, `:299-302`). A plugin calling `setValue` directly changes the field silently.
- The picker choice is `listSecrets().length > 0 ? new f6(app, settingKey, cb) : new h6(app, "", cb)` (`:303`) — §1's `$1`/`Y1` are dead names.
- Adjacent persistence keys, for the §11 "QtKeychain adapter" row: `localStorage` keys `secrets-encryption-warning-dismissed`, `secrets`, `secrets-encrypted`, `secrets-meta`, with a one-shot `migrate()` that reads `secrets` and deletes it (`:331-334`, `:339-342`).

### NEW: §13 Q9 — the i18n namespace for this domain is `bd`, and its keys are enumerable

§13 Q9 and §15 both say `gm.*`. The live namespace is **`bd`**. The complete set of `bd.*` keys referenced by this domain's own files: `dialogue.buttonCancel`, `dialogue.buttonDone`, `interface.sidebarExpand`, `interface.labelNewTab`, `interface.manageVaults`, `interface.noFile`, `interface.msgTabBusy`, `interface.msgFailedToCloseView`, `interface.emptyState.empty`, `interface.menu.{close,closeOthers,closeAfter,closeAll,copyPath,linkTab,moreOptions,moveToNewWindow,openLinkedView,pin,stackTabs,unlinkTab,unpin,unstackTabs}`, `interface.formatting.{clear,labelFormatting,toggleBold,toggleItalics,toggleCode,toggleComments,toggleHighlight,toggleMath,toggleStrikethrough}`, `interface.dragAndDrop.{openAsTab,openInThisTab}`, `nouns.{fileWithCount,folderWithCount}`, `setting.thirdPartyPlugin.labelNoResultsFound`, `setting.keychain.{msgSecretsNotEncryptedShort,buttonLink,buttonChange}`. English values live in `src/i18n/tree.js` (`buttonCancel` `:1113`, `buttonDone` `:1117`, `labelNoResultsFound` `:443`).

### Citation remap — every `*.js:*` path in `ui-bundle.md` is dead

The corpus is split per statement; the doc's `obsidian/ui/{components,icons,menu,popups}/*.js:NN` coordinates do not exist in 1.13.7. The authoritative membership list is the plugin export map at `app/own/0025-expressionstatement.js:1-161`.

| Doc symbol | 1.13.7 | Notes |
|---|---|---|
| `Component` | `app/own/0712-fg.js:40-92` (`mg`) | |
| `BaseComponent` | `app/own/0817-Yb.js:189-198` (`$b`) | |
| `ValueComponent` | `:199-209` (`Zb`) | |
| `AbstractTextComponent` | `:338-357` (`iw`) | |
| `ButtonComponent` | `:210-260` (`Jb`) | |
| `ExtraButtonComponent` | `:261-290` (`ew`) | |
| `ToggleComponent` | `:302-337` (`nw`) | |
| `TextComponent` | `:358-368` (`rw`) | |
| `SearchComponent` | `:369-398` (`ow`) | |
| `TextAreaComponent` | `:399-404` (`aw`) | |
| Number component (`$k`) | `:405-421` (`sw`) | **not exported** → §13 Q2 answered **no** |
| `MomentFormatComponent` | `:422-444` (`lw`) | |
| `DropdownComponent` | `:445-487` (`cw`) | |
| `ProgressBarComponent` | `:488-502` (`uw`) | |
| `SliderComponent` | `:503-564` (`hw`) | |
| `ColorComponent` | `:565-605` (`pw`) | |
| `SecretComponent` | `app/own/1921-l6.js:288-330` (`v6`) | |
| `Setting` | `app/own/0817-Yb.js:3-104` (`Yb`) | |
| `SettingGroup` | `:106-181` (`Xb`) | |
| `SettingPage` | `:183-188` (`Qb`) | undocumented in the doc |
| form-field (`dw`) / form sheet (`yw`) | `:606-626` / `:899-933` | undocumented |
| `setIcon` | `app/own/0721-Ag.js:2-9` (`Ag`) | |
| `getIcon` | `app/own/0720-Dg.js:2-15` (`Dg`) | |
| `addIcon` / `removeIcon` / `getIconIds` | `app/own/0722-Pg.js:2` / `0723-Ig.js:2` / `0724-Lg.js:2` | |
| memo wrapper (`Jm`) | `app/own/0719-Tg.js:2-9` (`Tg`) | |
| Lucide table (`Um`) | `app/own/0713-gg.js:2` (`gg`, 1408 keys) | |
| legacy table (`Qm`) / custom table (`Xm`) / alias map (`Ym`) / cache (`Zm`) | `app/own/0718-Cg.js:156` (`Eg`) / `:202` (`Mg`) / `:2` (`Cg`) / `app/own/0718-Cg.js:205` (`xg`) | presets `yg` `:0714-yg.js:2` and `Sg` `:0718-Cg.js:203` |
| `Menu` | `app/own/0751-my.js:2-328` (`my`) | |
| `MenuItem` | `app/own/0749-py.js:2-55` (`py`) | |
| `MenuSeparator` | `app/own/0749-py.js:56-58` (`dy`) | |
| current-menu singleton (§15 says `Wg`) | `app/own/0749-py.js:59` (`fy`) | `Wg` is a timer cancel at `app/own/0728-Wg.js:2` |
| `Modal` | `app/own/0817-Yb.js:627-791` (`fw`) | |
| confirm/prompt (`nb`/`ib`) | `:823-865` (`mw`, `ConfirmationModal`) / `:866-898` | |
| `SuggestModal` | `app/own/0819-ww.js:142-204` (`Cw`) | |
| `FuzzySuggestModal` | `:205-227` (`Ew`) | second, unrelated `FuzzySuggestModal` at `app/own/2032-E9.js:70` |
| `Notice` | `app/own/0819-ww.js:241-308` (`Aw`) | |
| list scaffold (`ob`/`ab`) | `:2-117` (`ww`) / `:118-141` (`kw`) | |
| Notice container map (`db`) | `:240` (`Dw`) | |
| `PopoverSuggest` | `app/own/1319-xR.js:26-71` (`TR`) | |
| `AbstractInputSuggest` | `:72-140` (`DR`) | |
| concrete suggesters (`xI`/`TI`/`DI`/`AI`/`PI`) | `:141-168` (`AR`) / `:169-176` (`PR`) / `:177-185` (`IR`) / `:186-212` (`LR`) / `:213-222` (`OR`) | |
| `HoverPopover` + globals | `app/own/1794-N$.js:52-186` (`U$`); `N$`/`B$`/`V$`/`H$`/`z$`/`q$`/`W$`/`j$` at `:2-51` | |
| `PopoverState` | `app/own/1793-expressionstatement.js:3` (`M$`) | **string** enum |
| `vg` (§15 mis-scoped) | `app/own/0712-fg.js:2-38` (`fg`) | `history.back/forward` overlay stack |
| `Ml` ("debounce") | `app/own/0355-Ml.js:2` | leading-edge throttle, `.cancel()`/`.run()` |
| `Qg` (drag/tooltip abort) | `app/own/0736-Qg.js:2-4` | |
| `setTooltip` | `app/own/0738-Zg.js:2` (`Zg`) | writes `aria-label` |
| `autoSelect` | `app/own/0656-lm.js:2` (`lm`) | |
| `sortSearchResults` | `app/own/0785-nb.js:2-6` (`nb`) | stable, no tiebreak |
| `renderResults` / `renderMatches` | `app/own/0791-lb.js:2-20` (`lb`) / `app/own/0790-sb.js:2` (`sb`) | |
| `prepareQuery` / `fuzzySearch` | `app/own/0780-$y.js:2` (`$y`) / `app/own/0784-tb.js:2` (`tb`) | |
| placement (`Nv`) / auto-destroy (`Rv`) | `app/own/0661-dm.js:2` (`dm`) / `app/own/0662-fm.js:2` (`fm`) | gaps 5 (popover) / 10 (hover) |
| `gv` (§5: "beforeunload") | `pagehide` — `app/own/0817-Yb.js:720`, `:773` | |

---

## Why noticed now

The UI bundle is the last remaining primitive domain that Corbomite has no code for, and `ui-bundle.md` §11 is the table an implementer would port from directly. Three of its most concrete instructions are wrong in ways that would have been expensive to discover late: the icon table is 15× the documented size (so a "port the 125 icons" plan is wrong), `setDynamicTooltip` is a stub (so a "fill this gap" plan is wrong), and `PopoverState`'s numeric values are string enum members with a different order (so any `static_cast<PopoverState>(0)` port is wrong). Separately, the menu section protocol turns out to be a **two-level hierarchical namespace** with prefix-matched submenu configs — which is the one piece §11 flags as **Missing** and needs for `Corbomite::MenuSectionRouter`, and which the doc's flat section-ID list would not have produced.

## Action taken

No Corbomite code changed as part of this verification; this is a facts-only addendum. Recommended follow-ups for whoever picks up the UI primitives:

- Delete the "125 Lucide IDs" target from any plan that copied §11/§14. The Corbomite icon layer should be a **resolver** (theme lookup → bundled SVG → null), not a fixed port list.
- File `SliderComponent.setDynamicTooltip` in Corbomite's divergence list, not its gap list, and re-use the `--slider-fill-ratio` + `.slider-value` contracts as the design spec for `Corbomite::Slider`.
- Build `Corbomite::MenuSectionRouter` against the **dotted** namespace with first-segment-keyed separators and prefix-matched `setSectionSubmenu`; add a regression test for `addSections(["a","b","","c",""])`, which Obsidian itself gets wrong (duplicate `""` emission).
- When implementing `Corbomite::Component`, make `load()` return a promise and aggregate children's returns.
- When implementing back/forward handling, mirror `vg`: an overlay stack that intercepts Back and calls `onHistoryBack()` → close. Corbomite's KDDW/window back handling does not currently do this for overlays.
- When implementing `Corbomite::HoverPopover`, port `shouldShowSelf`'s **focus-containment** term (not just `isFocused`), and treat the 10-shot `ResizeObserver` budget as a known-fidelity wart rather than a bug to fix.
- Three scope parents (Menu → global root, Modal → parentless, PopoverSuggest → `app.scope`) must be modelled explicitly in whatever shortcut-arbitration layer Corbomite adds, or modal-open will not shadow root hotkeys the way Obsidian's does.

**Not covered / unverifiable from this corpus:** the doc's §11 Corbomite-side status column ("Have" / "Missing" / "Partial") is a claim about Corbomite's own tree, not Obsidian's, and is out of scope here. The `SettingPage` / `SettingTab` / `PluginSettingTab` hierarchy and the `class="vertical-tab-content"` settings-tab shell were located but not audited. The CSS was read only where a JS fact demanded a class or custom property; `app.css` has not been swept for the full `.menu-item` / `.modal` / `.prompt` rule sets. `Mg` (the `addIcon` table) is empty in this chunk, so the custom-icon preset is confirmed from `Dg`'s branch and `Sg` but no live custom icon was observed.
