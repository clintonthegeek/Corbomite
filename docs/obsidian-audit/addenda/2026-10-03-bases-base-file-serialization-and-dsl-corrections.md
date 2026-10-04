# Bases `.base` serialisation **rewrites what you wrote**: property IDs are canonicalised on write, `not:` is NOR not NAND, `html()` returns a *Markdown* value, and `duration("5ms")` can never parse

**Date:** 2026-10-03
**Discovered during:** `bases.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/bases.md` §1, §2, §3, §6, §7, §8, §13 (Q2/Q3/Q5/Q6/Q9/Q10 answered, Q4 still open); re-verifies §2 of `addenda/2026-06-10-taxonomy-and-bases-corrections.md`; corrects `addenda/2026-04-17-bases-formula-dsl.md` §6.1, §8.2, §8.6, §8.7, §10, §14 (whose provenance paths are dead — see §D)
**Relevant cluster plans:** none yet. `bases.md` §14 item 1 proposes promoting the `.base` schema to `VAULT-FORMAT.md`; **do that only after absorbing §A and §F below**, because the schema as written in §3 does not match the writer.

**Source:** `RE/obsidian/1.13.7/renamed/src/app/own/` (one statement per file). Every citation below is `app/own/<file>:<line>`. Names are post-rename; string literals are verbatim from source.

---

## Finding

The Bases engine is fully in the corpus and mostly matches the audit — but the two documents describing it were written from a corpus that was **not** the code that writes `.base` files, and both are wrong in ways that matter for a vault-format port.

The headline is §A: **the writer canonicalises property IDs.** `order`, `sort`, `groupBy` and `summaries` all run their property IDs through `hQ`, which strips the `note.` prefix and shortens `file.file` to `file` — while the `properties:` map and the newly-discovered `columnSize` map are emitted with **raw** keys. So a single `.base` file carries **two different key conventions for the same logical property**, and `bases.md` §3's schema block shows the pre-canonicalisation form for `order` and the post-canonicalisation form for `properties`, which is the opposite of what the writer does. §F then adds the *entire* per-view-type `data:` key inventory, which §3 gestures at with two examples — `image:` is a real cards key, but `rowHeight: 120` is not a legal value (it is a four-way dropdown string) and the table's `columnSize` map, which persists per-column widths, is missing altogether.

The second headline is §B–§C: two outright semantic errors. **`not:` is NOR, not NAND** (refuting the 2026-04-17 addendum §10), and **`html()` returns the `Markdown` value while nothing at all returns the `HTML` value** — `labelGlobalMarkdown` survives in the locale tables (`main/own/0033-Ke.js:2622`) but no code references it, which answers `bases.md` §13 Q9 in the negative. §D then shows `duration("5ms")` is unreachable because the unit regex has no `ms` alternative, that `date()` accepts two undocumented numeric forms, and that `link("https://…")` returns a **URL** value.

§G–§J answer the four open questions the domain doc could not (`QueryController` is at `1813-CZ.js:1893`; the incremental-refresh gate is a `JSON.stringify` fingerprint; the view-type registry verb is `registerView` with a `factory`; `.base` embeds select a named view via `#`).

---

## A. CORRECTION — the `.base` writer **canonicalises property IDs**; §3's schema shows the wrong form

There are four property-ID helpers, not the `parsePropertyId`/`nY`/`xY`/`SY` cluster §15 lists: `qX` (parse), `pQ` (normalise, `app/own/1769-pQ.js`), `hQ` (serialise) and `WX` (join, `app/own/1763-WX.js:2-4`, used by `removeFormula` at `app/own/1766-lQ.js:282`). Two of them matter enormously for round-tripping:

```js
// app/own/1762-qX.js:2-16  — PARSE
function qX(e) {
  var t = e.indexOf(".");
  if (-1 === t) return { type: "note", name: e };
  var n = e.slice(0, t);
  return "note" !== n && "formula" !== n && "file" !== n
    ? { type: "note", name: e }          // <-- WHOLE string, dot included
    : { type: n, name: e.slice(t + 1) };
}
```

```js
// app/own/1768-hQ.js:2-9  — SERIALISE  (the one §3 never mentions)
function hQ(e) {
  if (e.length > 5 && e.startsWith("note.")) {
    var t = e.substring(5), n = t.split(".")[0];
    return ["file", "formula", "note"].contains(n) ? e : t;
  }
  return "file.file" === e ? "file" : e;
}
```

So on write: `note.status` → **`status`**, `note.note.x` → `note.note.x` (kept, because the next segment is a reserved word), `formula.ppux` → `formula.ppux`, `file.file` → **`file`**, `file.name` → `file.name`.

`hQ` is applied to exactly four places:

| Site | Line |
|---|---|
| `views[].groupBy.property` | `app/own/1766-lQ.js:15` |
| `views[].order[]` | `app/own/1766-lQ.js:17` |
| `views[].sort[].property` | `app/own/1766-lQ.js:17-22` |
| `views[].summaries` keys | `app/own/1766-lQ.js:22-26` |

…and to **none** of these:

| Site | Line | Key form on disk |
|---|---|---|
| `properties:` map | `app/own/1766-lQ.js:223-227` | raw, whatever the user typed |
| `views[].data.columnSize` keys | `app/own/1972-Z5.js:1021-1029` | **canonical** (`note.x`) |
| legacy `display:` map | consumed at `app/own/1766-lQ.js:134-146` → routed into `properties` | raw |

**Consequences a port must reproduce:**

1. **Key order moves on rewrite.** `order:` is emitted before `sort`, `limit`, `summaries`, and then the free-form `data` keys *last* (`app/own/1766-lQ.js:9-32`). A hand-written `views: [{image: …, order: […]}]` comes back with `image:` after `order:`. There is no `unrecognizedData` on `BasesViewConfig` at all — unknown view keys land in `.data` via `app/own/1770-dQ.js:96-97` and are re-emitted verbatim, just relocated. **This refutes `bases.md` §8's `[CRIT]` "Unknown keys round-trip verbatim. `unrecognizedData` at every level (`BasesQuery`, `BasesViewConfig`, `PropertyConfig`)":** there are two levels with it (`BasesQuery`, `PropertyConfig`) and one without.
2. **`properties:` keys are provenance-dependent.** Parse stores under the *raw* YAML key but gives the config a *canonical* `propertyId` (`Object.hasOwn` check, then `h.properties[A] = iQ.deserialize(h, pQ(A), b)` — `app/own/1766-lQ.js:127-132`). But `getPropertyConfig(e)` **writes through into the map** under the canonical key (`app/own/1766-lQ.js:276-278`). So a file edited only in Obsidian accumulates `note.foo:` keys, while a file authored by hand keeps `foo:`, and one file can end up with both.
3. **The `properties:` block is omitted entirely when every config serialises empty**, and `PropertyConfig.serialize()` returns `null` when it has nothing (`app/own/1764-jX.js:267-269`, consumed at `app/own/1766-lQ.js:223-227`). A non-string `displayName` is *not* an error — it falls into `unrecognizedData` and round-trips (`app/own/1764-jX.js:259-266`).
4. **`qX` does not split on an unrecognised prefix**, so a frontmatter key containing a dot (`my.key`) is addressable as `note.my.key` → `{type:"note", name:"my.key"}`. §2's "`note.<key>` — frontmatter property" gloss hides this. Correspondingly `pQ("file")` special-cases to `"file.file"` (`app/own/1769-pQ.js:2-4`).

**Precision note on §3's "`Root must be an object`":** the guard is `"object" != typeof t` (`app/own/1766-lQ.js:110`), which **`null` and arrays pass** — a `null` root then dies with a `TypeError` on `n.views` rather than `msgErrorInvalidQueryFormat`.

---

## B. CORRECTION — `not:` is **NOR**, not NAND (refutes 2026-04-17 §10)

2026-04-17 §10 states: *"**`not`**: logical-NOT applied to the conjunction of its children (effectively `!(child[0] && child[1] && ...)`)"*. The code returns false as soon as **any** child passes:

```js
// app/own/1764-jX.js:232-237
t.prototype.test = function(e) {
  for (var t = 0, n = this.filters; t < n.length; t++) {
    if (n[t].test(e)) return !1;
  }
  return !0;
}
```

So `not: [a, b]` is `!(a || b)`. The filter-builder UI agrees: a `not` group is labelled **"None of the following are true"** with rows joined by **`labelOr`** (`app/own/1803-cZ.js:48-66`, `:136-137` — `o = "and" === this.conjunction ? labelAnd() : labelOr()`).

Rest of the filter tree, which §1 gets structurally right but leaves behaviourally undescribed:

- **Parse does not optimise.** `uQ` returns `new JX(...)` / `new eQ(...)` / `new tQ(...)` directly (`app/own/1767-uQ.js:14,20,26`). `.optimize()` is only reached via the `Filter.and/or/not` statics (`app/own/1764-jX.js:128-139`) and the UI. **So the authored tree shape is what round-trips** — `{and: [x]}` stays a one-child `and`, it does not collapse to `x`.
- **A conjunction object must have exactly one key** — `1 !== n.length` throws `msgErrorFilterAllowedKeys` ("*\"filters\" may only have one of an \"and\", \"or\", or \"not\" keys.*"), and an unknown key throws the same error from the `default` arm (`app/own/1767-uQ.js:6-29`). §3 says only "one of".
- **`optimize()` can return `null`**, and `Filter.and()` collapses a single argument to that argument itself (`app/own/1764-jX.js:128-139`). `Filter.not()` has no such collapse at the static level, but `NotFilter.optimize` implements **double-negation elimination**: a single child that is itself a `NotFilter` becomes `new JX(o.filters).optimize()` — a *different class*, so `not: [not: [a]]` serialises as `{and: [a]}` (`app/own/1764-jX.js:221-231`).
- **NEW, undocumented: the introspection API.** `getFilterRules()` / `getFilterRulesWithNegation(negated)` walk the tree producing `[{rule, negated}]`, flipping the flag inside a `NotFilter` (`this instanceof tQ` → `e !== n`, `app/own/1764-jX.js:113-127`); and `hasError()` on both `FilterRule` (`this.rule.formula instanceof CX`, `:146-147`) and conjunctions (`:159-162`). `hasError` is what distinguishes a syntactically broken filter from a merely empty one, and it is the only reason the invalid sentinel `CX` needs to be reachable from the filter layer.

---

## C. CORRECTION — `html()` returns the **`Markdown`** value; the `HTML` value is unreachable

`bases.md` §1 says `HTMLValue` is "Produced by formulas", and 2026-04-17 §8.2 says `html(str) → HTMLValue`. Neither is true. `bd.plugins.bases`'s pair of HTML-ish values is declared together, Markdown first:

```js
// app/own/1649-dK.js:460-478
SK = (function(e) {
    function t() { … t.icon = "lucide-baseline" … }
    v(t, e), t.prototype.renderTo = function(e, t) {
      e.addClass("markdown-rendered");
      var n = GT(WT(this.data));
      e.appendChild(RF(n)), t.app.fixFileLinks(e, "");
    }, t.type = "Markdown";
  }(lK), function(e) {
    function t() { … t.icon = "lucide-code-2" … }
    v(t, e), t.prototype.renderTo = function(e, t) {
      e.appendChild(RF(this.data)), t.app.fixFileLinks(e, "");
    }, t.type = "HTML";
  }(lK));
```

and the only registration constructs the **first** one:

```js
// app/own/1657-expressionstatement.js:168-171
return t.name = "html", t.params = [DK("input", lK)], t.docString = TK.labelGlobalHtml, t;
…
t.prototype.apply = function(e) { return new SK(e.data); },
```

The docstring table still carries the *other* function's string — `labelGlobalMarkdown`: *"Converts a string into a code snippet that renders as Markdown."* — but `grep -rl labelGlobalMarkdown` over the whole corpus returns **only the locale tables** (`main/own/0033-Ke.js:2622`, `main/all.js:3570`, `i18n/i18n.js`, `i18n/tree.js`). No code path reads it. **So: `html(x)` → `type "Markdown"`, and nothing constructs `type "HTML"`.** `bases.md` §13 Q9 answered: there is no `markdown()` and no `richtext()`.

Corroborating detail: the Markdown value's `renderTo` runs the **full sanitising render chain** (`GT(WT(data))` → `RF` → `fixFileLinks`) and adds the `markdown-rendered` class, i.e. it is the same path as reading-view markdown — not a raw-HTML sink.

---

## D. CORRECTION — the duration unit list has **no `ms`**, the ISO regex is **unanchored**, `date()` takes numerics, and `link()` can return a URL

```js
// app/own/1649-dK.js:245-267
t.parseFromString = function(e) {
  var n = e.match(/P(?:([.,\d]+)Y)?(?:([.,\d]+)M)?(?:([.,\d]+)W)?(?:([.,\d]+)D)?T(?:([.,\d]+)H)?(?:([.,\d]+)M)?(?:([.,\d]+)S)?/);
  if (n) { … return new t(i(n[1]), i(n[2]), 7 * i(n[3]) + i(n[4]), i(n[5]), i(n[6]), i(n[7]), 0); }
  var r = e.match(/^(-?\d+) ?([dhmswMy]|(?:second|minute|hour|day|week|month|year)[s]?)$/);
  …
    if ("ms" === a || "millisecond" === a || "milliseconds" === a) return new t(0, 0, 0, 0, 0, 0, s);
```

**The three `ms` branches at the end are dead code.** The single-letter class is `[dhmswMy]` — no `ms` alternative — and the word alternation is `(?:second|minute|hour|day|week|month|year)[s]?`, which does not include `millisecond`. So `duration("5ms")`, `duration("5 millisecond")` and `duration("5 milliseconds")` all fail to match, `parseFromString` returns `null`, and the global returns `oK.value` (`app/own/1657-expressionstatement.js:128-131`). **This refutes 2026-04-17 §6.1's unit list and §14's "Also accepts `ms`, `millisecond`, `milliseconds`".**

Also in that same function, both `DurationValue` unit claims in §6.1 hold otherwise, and three new facts fall out:

- The **ISO branch is unanchored** — no `^`/`$` — so `"foo P1D bar"` parses; it accepts `,` as a decimal separator; it **requires the literal `T`**; and it **always sets `milliseconds` to 0** (`i(n[7])` is passed in the seconds slot, ms is literal `0`).
- **`addToDate` upgrades the result's `time` flag.** `r = e.time` and then `0 !== this.hours && (…, r = !0)` for hours/minutes/seconds/milliseconds (`app/own/1649-dK.js:219-223`) — so `date-only + duration("2h")` yields a value with `time === true`.
- **`getMilliseconds()` is relative to *now***: `this.addToDate(new gK(new Date())).date.getTime() - e.date.getTime()` (`:224-226`). Since `looseEquals` and the `file` sort comparator both use it, duration equality and ordering depend on wall-clock/DST/month-length at evaluation time.

### `date()` — undocumented numeric forms, and `NullValue` not an error Value

```js
// app/own/1657-expressionstatement.js:21-46  (excerpt)
t.name = "date", t.params = [DK("input", lK, gK)], …
t.prototype.apply = function(e) {
  if (e instanceof gK) return e;
  var t = gK.parseFromString(e.data);
  if (t) return t;
  var n = function(e) { if (/^\d{12}$/.test(e)) { … return new Date(t, n, i, r, o); } return null; }(e.data);
  if (n) return new gK(n, !0);
  var i = function(e) { if (/^\d{8}$/.test(e)) { … return new Date(t, n, i); } return null; }(e.data);
  return i ? new gK(i, !1) : oK.value;
}
```

`date("202401011230")` → a `DateValue` with `time: true` built from **local** components; `date("20240101")` → `time: false`. **Failure is `oK.value` (NullValue), not an error Value** — 2026-04-17 §8.2's "Returns error Value on parse failure" is wrong. `date()` also accepts a `DateValue` and returns it unchanged, which the doc's `(string) → DateValue` signature omits.

### `link()` — a URL-shaped string yields a `UrlValue`

```js
// app/own/1657-expressionstatement.js:101-106
t.prototype.apply = function(e, t) {
  if (e instanceof CK || e instanceof lK && $v(e.data)) return new CK(e.data, t);
  if (e instanceof kK) return new EK(this.ctx.app, e.file.path, "", t);
  var n = EK.parseFromString(this.ctx.app, e.data, "");
  return n || new EK(this.ctx.app, e.data, "", t);
}
```

`$v` is the `new URL()` parseability test (`app/own/1649-dK.js` uses it as `renderExternalLink`'s guard), so `link("https://x.dev")` → **`UrlValue`**, not `LinkValue`. Also note `sourcePath` is the literal `""` in both branches. `image()` by contrast *resolves* a `LinkValue` and yields NullValue if unresolved (`:137-141`).

`escapeHTML`'s charset is exactly `/[&<>"]/` → `&amp; &lt; &gt; &quot;` — **the apostrophe is not escaped** (`app/own/0546-Od.js:2-9`).

---

## E. CORRECTION — `list.sort()` **has** a comparator; `unique()` uses `equals`; `stddev()` is population; `mean()` divides by the *full* length

2026-04-17 §14 flags `list.sort()` as "*uses `.sort()` without a comparator … **Unit-test at Cluster K impl time.***" It has one, and it is numeric-aware:

```js
// app/own/1647-tK.js:304-310
t.prototype.sort = function() {
  var e = this.data.slice();
  return e.sort(function(e, t) {
    var n = e instanceof sK ? e.data : e instanceof iK ? e.toString() : e,
      i = t instanceof sK ? t.data : t instanceof iK ? t.toString() : t;
    return Number.isNumber(n) && Number.isNumber(i) ? n - i : Sw(n, i);
  }), new t(e);
}
```

(`Sw` = `localeCompare`; `Tw` = the numeric comparator, used only for group ordering — `app/own/0819-ww.js:233,237`.)

Four more precise semantics, all in `ListValue`:

| Method | Line | Actual behaviour |
|---|---|---|
| `unique()` | `:287-303` | Buckets by **`toString()`**, then keeps the first element in each bucket that **`iK.equals`** the newcomer. So dedup is **strict**, not loose — 2026-04-17 §8.7's "dedupe via element-wise `Value.looseEquals`" is wrong — and two values that stringify alike but don't `equals` both survive. |
| `mean()` | `:343-345` | `sum().data / this.length()` — divides by the **whole list length**, not the count of numbers. `[1,2,"x"].mean()` is `1`, not `1.5`. |
| `stddev()` | `:352-365` | `Math.sqrt(mean of squared deviations)` — **population** standard deviation. This settles §14's "sample-vs-population TBD": population. |
| `looseEquals()` | `:267-268` | `e instanceof List ? compare(e, iK.looseEquals) : 1 === this.data.length && iK.looseEquals(this.get(0), e)` — **a one-element list loose-equals its own element**, which is why `note.tags == "x"` matches a single-tag list. |
| `get()` | `:271-273` | Mutates `this.data[i]` in place with the lazy-evaluated value. (Cross-confirms the 2026-10-03 rendering addendum.) |
| `empty` aggregates | `:313-365` | `earliest/latest/sum/median/min/max/stddev` all return `oK.value` on empty input. |

Two indexing rules from the evaluator, both new:

- **Negative list indices wrap:** `i < 0 && (i += t.length())` (`app/own/1759-kX.js:406`). `l[-1]` is the last element.
- **Strings are indexable and yield a `StringValue`:** `new lK(t.data.charAt(n.data))` (`app/own/1759-kX.js:412-415`), with **no** negative wrap (out-of-range gives `""`). And `x["name"]` on any `Value` routes to `objectAccess`, while `x[0]` on a non-list throws `msgErrorInvalidArrayAccess` (`:416-426`).

---

## F. NEW — the per-view-type `data:` key inventory (§3's schema section is missing all of it)

The three built-in view types register a **static `options`** descriptor list (`app/own/1972-Z5.js:1510`, `:1517`, `:1524` — `options: Z5.getViewOptions` etc.), and each descriptor's `key` is a `views[].data` key. These are therefore **`.base` file-format keys**, and `bases.md` §3 mentions none of them.

### `table` — `app/own/1972-Z5.js:1033-1044`, `:833-846`, `:1021-1030`

| `data` key | Type | Values / default | Read at |
|---|---|---|---|
| `rowHeight` | dropdown | `""`=Short, `medium`, `tall`, `extra` | `:833-846` → multiplier `1 / 2 / 4 / 8`; base row height from CSS `--bases-table-row-height`, fallback `30` |
| `columnSize` | `Record<canonicalPropertyId, number>` | written by `saveColumnSizes()` | `:808-825` |

```js
// app/own/1972-Z5.js:1021-1029
t.prototype.saveColumnSizes = function() {
  for (var e = this.columnInfo, t = {}, n = 0, i = this.data.properties; n < i.length; n++) {
    var r = i[n];
    if (e.hasOwnProperty(r)) { var o = e[r]; o.customWidth && (t[r] = o.customWidth); }
  }
  this.config.set("columnSize", t);
}
```

`data.properties` is canonical (`pQ`-normalised), so **`columnSize` keys keep the `note.` prefix while `order` entries lose it** — the asymmetry flagged in §A, now with a concrete victim. Note the write is unconditional: resizing back to default still persists `{}`, and `config.set` calls `query.save()` (`app/own/1766-lQ.js:57-58`).

Table DOM classes, for style parity: `bases-table`, `bases-tbody`, `bases-tr`, `bases-td`, `bases-table-group-summary-row`.

### `cards` — `app/own/1967-M5.js:213-249`, `:118-129`

| `data` key | Type | Values / default |
|---|---|---|
| `cardSize` | slider | min `50`, max `800`, step `10`, **default `200`**; read as `Number.isNumber(g) && g > 0`, then multiplied by CSS `--bases-cards-scale` |
| `image` | property-picker | `filter: e => "file.file" === e \|\| !e.startsWith("file.")`, read as `pQ(d)` → canonical `note.x` |
| `imageFit` | dropdown | `""`=Cover, `contain`=Contain → `this.imageFitContain = "contain" === i.get("imageFit")` |
| `imageAspectRatio` | slider | min `.25`, max `2.5`, step `.05`, default `1` |

Classes: `bases-cards-group`, `bases-group-heading`, `bases-cards-item`, `bases-cards-label`, `bases-cards-line`.

### `list` — `app/own/1969-L5.js:42-45`, `:46-67`

| `data` key | Type | Values / default |
|---|---|---|
| `markers` | dropdown | `bullet` \| `number` \| `none`, default `"bullet"`; `getMarkers` coerces **any other value** back to `"bullet"` |
| `indentProperties` | toggle | default `false` |
| `separator` | text | default `", "`; `String(i.get(R5) \|\| ", ")`; the option is **hidden in the UI** while `indentProperties` is on (`shouldHide`) |

List is virtualised (`bases-list-container is-loading`, `tabIndex: 0`), with a **60 px** assumed row height until something measures, a 10 ms render debounce, and `show-indentation-guide` bound to `vault.getConfig("showIndentGuide")` (`:72-81`, `:96-99`).

### Defaults §3 does not state

- **Default `order` is `["file.name"]`** — `getOrder()` returns `this.order || oQ.slice()` (`app/own/1766-lQ.js:59-60`, `oQ` at `app/own/1764-jX.js:282`).
- **Default sort is `[{property:"file.name", direction:"ASC"}]`** — applied when `getSort()` yields nothing *after* filtering to known properties (`rQ` at `app/own/1764-jX.js:278-281`, consumed at `app/own/1813-CZ.js:1505-1512`).
- **The default table view's name is the localised `labelViewTypeTable()`**, pushed by both `fromString("")` and a zero-length `views:` array (`app/own/1766-lQ.js:103-108`, `:125`).

---

## G. NEW — parse-side validation is much stricter and much looser than §3 says

`dQ` (the per-view builder, `app/own/1770-dQ.js:2-101`) is where §3's "Validation errors" list should have pointed. Six *new* error keys, all with the full English text (`i18n/tree.js`, `bd.plugins.bases`):

| Key | English | Trigger |
|---|---|---|
| `msgErrorViewInvalidFormat` | `"view" items must be a YAML object, however item {{ index }} is not` | `dQ:3-5` |
| `msgErrorViewNameInvalid` | `Missing or invalid "name" in view {{ index }}` | `dQ:8-10` — **`!r \|\| "string" != typeof r`**, i.e. non-empty string only |
| `msgErrorViewMissingRequiredKey` | `"{{ key }}" is required in view "{{ viewName }}"` | `dQ:12-15` |
| `msgErrorViewIncorrectType` | `"{{ key }}" must be a {{ type }} in view "{{ viewName }}"` | `dQ:16-20, 39-43, 53-57, 71-75` |
| `msgErrorViewIncorrectValueType` | `"{{ key }}" values must all be of type {{ type }} in view "{{ viewName }}"` | `dQ:62-66` |
| `msgErrorViewParsingFilters` | `Unable to parse filters in view "{{ viewName }}": {{ message }}` | `dQ:28-35` — re-wraps the inner filter error |

Three asymmetries a port must mirror:

1. **`sort` that is not an array is silently ignored; `order` that is not an array throws.** `case "sort": if (Array.isArray(i.sort)) {…}` has no `else` (`dQ:78-91`), while `case "order": if (!Array.isArray(i.order)) throw` (`dQ:70-76`).
2. **`sort[].column` is a legacy alias**: `k.column && (k.property = k.column)` (`dQ:83`).
3. **`groupBy` requires *both* `property` and `direction` as own keys or throws**; a non-string `property`, or a direction outside `{ASC, DESC}`, is then **silently dropped** (`dQ:37-51`). `sort` entries are filtered the same silent way (`dQ:86-90`).

Plus: **`limit: 0` is never stored.** `Number.isNumber(i.limit) && i.limit > 0 && (a.limit = i.limit)` (`dQ:93-94`) — so a hand-written `limit: 0` is dropped at parse and absent on rewrite. §8's "`limit: 0` means unlimited" is right about the *meaning* and silent about the fact that the key itself does not survive.

And: **`getEvaluatedFormula` has an explicit invalid-sentinel bail-out** that did not exist in 1.12.7 — `if (r.formula instanceof CX) return oK.value` before constructing the evaluation entry (`app/own/1766-lQ.js:44-56`).

---

## H. NEW — the `summaries` asymmetry: `formulas: {}` is suppressed, `summaries: {}` is emitted

This **re-verifies 2026-06-10 §2 unchanged** (the double-handling is still there) and adds a new fact next to it.

The recognized-key switch still omits `"summaries"` from its case list, so a top-level `summaries:` block is *also* copied into `unrecognizedData`:

```js
// app/own/1766-lQ.js:188-202
for (var x = t, T = 0, D = Object.keys(t); T < D.length; T++) {
  var A;
  switch (A = D[T]) {
    case "views": case "filters": case "display": case "properties":
    case "formulas": case "newItemFolder": case "newItemTemplate":
      continue;
    default:
      h.unrecognizedData[A] = x[A];
  }
}
```

`getSerializable` still starts from `Object.assign({}, this.unrecognizedData)` (`app/own/1766-lQ.js:205`) and then overwrites `summaries` from the live `summaryFormulas` (`:216-222`). **But the two write sites are guarded differently:**

```js
// app/own/1766-lQ.js:206-222
if (this.formulas) {
  var t = Object.keys(this.formulas);
  if (t.length > 0) {            // <-- length-guarded
    e.formulas = {};
    for (var n = 0, i = t; n < i.length; n++) { var r = i[n]; e.formulas[r] = this.formulas[r].toString(); }
  }
}
if (this.summaryFormulas) {      // <-- NOT length-guarded
  e.summaries = {};
  for (var o = 0, a = Object.keys(this.summaryFormulas); o < a.length; o++) { r = a[o]; e.summaries[r] = this.summaryFormulas[r].toString(); }
}
```

So an emptied `formulas` map **disappears** from the output while an emptied `summaryFormulas` map **persists as `summaries: {}`**. `deleteSummaryFormula` deletes the key from `summaryFormulas` but never deletes the map (`app/own/1766-lQ.js:249-252`), which is how that state is reached. Order of writes is `unrecognized → filters → formulas → summaries → properties → views → newItemFolder → newItemTemplate` (`:204-230`).

Two more `summaryFormulas` facts §1's API list omits: `deleteSummaryFormula` and `renameSummaryFormula` both **walk every view** and rewrite `views[].summaries` values that referenced the name (`:249-269`) — `summaries` maps *property id → summary name*, not property id → formula text. And `getSummaryFormula(name, valueClass)` returns a **freshly parsed** `fX` from the default table on every call (`:270-275`).

---

## I. NEW — the default-summary catalogue is keyed by **`Value` class**, and `isEmpty()` exists on only six types

`bases.md` §9 says "optional summary cells (sum/count/mean)". There is no `count`. The defaults are a `Map` from **`Value` constructor** to a name→source table:

```js
// app/own/1759-kX.js:509-533
NX = new Map([
  [cK, { Average: "values.mean().round(2)", Min: "values.min()", Max: "values.max()",
         Sum: "values.sum().round(2)", Range: "values.max() - values.min()",
         Median: "values.median()", Stddev: "values.stddev().round(2)" }],
  [gK, { Earliest: "values.earliest()", Latest: "values.latest()", Range: "(values.latest() - values.earliest())" }],
  [uK, { Checked: "values.filter(value == true).length", Unchecked: "values.filter(value == false).length" }],
  [iK, { Empty: 'values.filter(value.isType("null")).length',
         Filled: 'values.filter(!value.isType("null")).length', Unique: "values.unique().length" }]
]);
```

`BX` walks the constructor chain with **derived-class-wins** accumulation (`app/own/1760-BX.js:2-9`), so the available defaults depend on the column's inferred class:

| Column value class | Defaults offered |
|---|---|
| `NumberValue` (+ Value) | Average, Min, Max, Sum, Range, Median, Stddev, Empty, Filled, Unique |
| `DateValue` (+ Value) | Earliest, Latest, Range, Empty, Filled, Unique |
| `BooleanValue` (+ Value) | Checked, Unchecked, Empty, Filled, Unique |
| everything else | Empty, Filled, Unique |

The class is sniffed from **the first non-`NullValue` entry's value** (`app/own/1813-CZ.js:1551-1557`), then the summary formula is evaluated in a context that exposes **only** `values` — `keys()` returns `["values"]` and every other identifier is `oK.value` (`app/own/1761-VX.js:2-13`). So a custom summary cannot reference `note.*`, `file.*` or `this`. (`HX`, `:14-17`, is the same class with `values = []`.)

**The type string itself is a *static, inherited* property**, and `isType()` reads it through the constructor chain — worth spelling out because it looks broken and is not:

```js
// app/own/1647-tK.js:104-106        — static toString on the Value base
e.toString = function() { return this.type; }
// app/own/1657-expressionstatement.js:203-210  — isType
for (var n = t.data.toLowerCase(), i = e.type; i;) {
  if (i.toString().toLowerCase() === n) return new uK(!0);
  if (i === iK) break;
  i = Object.getPrototypeOf(i);
}
```

`e.type` is the *constructor* (instance getter, `app/own/1647-tK.js:118-123`); statics are inherited through `Object.setPrototypeOf` by the `__extends` helper (`app/own/0026-f.js:2-11`, `app/own/0027-v.js:2-8`), so `oK.toString()` is `"Null"` and `dK.toString()` (TagValue, which declares no static `type`) is `"String"`. **Verified by execution**, because the obvious reading (`Function.prototype.toString()` on a class) would make `isType` permanently false.

**`isEmpty()` is registered on exactly six classes** — `oK` (always `true`), `lK`, `cK` (always `false`), `hK`, `vK`, `gK` (always `false`). It is *not* on `Value`, so **`file.isEmpty()`, `file.ctime.isEmpty()`, `note.dur.isEmpty()`, a `LinkValue`'s `.isEmpty()` etc. all throw `msgErrorInvalidInstanceFunction`.** §1's `Value`-subclass table implies it's universal; §3 of 2026-04-17 correctly enumerates six types but does not say the others throw.

---

## J. NEW — `.base` is also a **code-block language**, and the embed selects a view by `#`

This answers `bases.md` §13 Q6 and §9's "likely renders the first view inline" — which is wrong.

### ```base code blocks

```js
// app/own/1972-Z5.js:1398-1420 (excerpt)
var i = function(t, i, r) {
  t = t.replace(/^(\t+)/gm, function(e, t) { return "    ".repeat(t.length); });   // tabs -> 4 spaces
  var o = e.vault.getFileByPath(r.sourcePath),
    a = new nJ(n.app, n, i, o);                                                    // a QueryController
  i.addClass("bases-embed", "interactive-child");
  var s = new EW(i); r.addChild(s); s.addChild(a);
  var l = cQ.fromString(t),
    c = Ml(function(e) { return r.replaceCode(e.toString()); }, 100);              // 100 ms debounce
  l.saveFn = function() { a.setQuery(l), c(l); };
  a.setQuery(l);
}, r = AW.createCodeBlockPostProcessor(u8, i);
AW.registerPostProcessor(r), AW.registerCodeBlockPostProcessor(u8, i),
this.app.workspace.trigger("post-processor-change"),
```

So: leading **tabs are rewritten to four spaces** before YAML parse; the block is parsed as a full query; the controller is live; and any mutation of the query is **written back into the code block** via `ctx.replaceCode` after a **100 ms** debounce. Registration is dual (`registerPostProcessor` + `registerCodeBlockPostProcessor`), matching the dual-registration pattern the 2026-10-03 rendering addendum documents, and it fires `post-processor-change` so existing reading views re-render. `CodeMirror.modes.base = CodeMirror.modes.yaml` (`:1446`) — there is no bespoke grammar.

### `![[My Base#View]]`

```js
// app/own/1972-Z5.js:1446-1449
e.embedRegistry.registerExtension(u8, function(e, t, i) {
  var r = (null == i ? void 0 : i.startsWith("#")) ? i.substring(1) : "";
  return new iJ(n.app, n, e, t, r);
}),
```

and the embed host (`app/own/1813-CZ.js:2253-2270`):

```js
a.requestSave = Ml(function(e) { … this.app.vault.modify(this.file, e.toString()); }, 1e3),
…
a.viewName = o, a.linkText = i.linktext,
…
var s = a.containingFile = t.vault.getFileByPath(i.sourcePath),
  l = a.controller = new nJ(a.app, a.plugin, a.containerEl, s);
```

Facts: the **`#` fragment names the view**; the embed gets its own `QueryController` (`nJ`) with `containingFile` as the `this` context, not the `.base` file; it reads with **`vault.read`**, not `cachedRead` (`:2321`); and it writes back with **`vault.modify` after a 1000 ms debounce** — a *different* debounce and a *different* channel from the standalone view (2000 ms, `app/own/1813-CZ.js:6-7`) and from the code block (100 ms, `replaceCode`). Classes added: `bases-embed interactive-child`.

Also `fileManager.registerFileParentCreator("base", e => app.fileManager.getNewFileParent("path", "base"))` (`app/own/1972-Z5.js:1449-1451`), and `workspace.registerHoverLinkSource("bases", {display: this.name, defaultMod: !0})` (`:1454-1457`) — the `defaultMod: true` is why Bases file-link hovers need no modifier, so §9's "(with modifier)" is wrong for Bases cells.

---

## K. NEW — commands, the view-type registry, the operator-func table, and four CLI handlers

`bases.md` §6 ("No commands registered here … the likely source of any `bases:`-prefixed commands") and §7 (`{name(), icon, options(viewConfig), create(controller)}` — "verb inferred") are both now settled.

### Six global commands + a ribbon item — `app/own/1972-Z5.js:1347-1397`

| id | name key | icon | gate |
|---|---|---|---|
| `bases:new-file` | `commandCreateNew` | `lucide-layout-list` | — |
| `bases:insert` | `commandInsertNew` | `lucide-layout-list` | `editorCheckCallback`, refused inside a table cell (`!t.inTableCell`) |
| `bases:copy-table` | `commandCopyTable` | `lucide-copy` | needs an active `MZ` **and** an active `Z5` (table) view |
| `bases:change-view` | `commandChangeView` | `lucide-shapes` | only when `controller.getQueryViewNames().length > 1` |
| `bases:add-view` | `commandAddView` | `lucide-shapes` | active `MZ` |
| `bases:add-item` | `commandAddItem` | `lucide-shapes` | opens `newItemMenu` |

Plus `t.registerRibbonItem(c8.commandCreateNew(), "lucide-layout-list", …)` and two workspace menu hooks — `file-menu` (adds an item under `"file-explorer-context-menu"` / section `"action-primary"`) and `editor-menu` (section `"selection.insert.advanced"`).

### The view-type registry — §13 Q10 answered

```js
// app/own/1972-Z5.js:1505-1527
this.registerView(G5, { name: c8.table.name(), icon: "lucide-table",
                        factory: function(e, t) { return new Z5(e, t); }, options: Z5.getViewOptions }),
this.registerView(M5, { name: c8.cards.name(), icon: "lucide-layout-grid",
                        factory: function(e, t) { return new T5(e, t); }, options: T5.getViewOptions }),
this.registerView(O5, { name: c8.list.name(),  icon: "lucide-list",
                        factory: function(e, t) { return new H5(e, t); }, options: H5.getViewOptions }),
```

```js
// app/own/1972-Z5.js:1744-1758
BasesPlugin.prototype.registerView = function(e, t) {
  this.registrations.hasOwnProperty(e) ? new Aw(c8.msgErrorRegisterView({ viewId: e })) : this.registrations[e] = t;
},
BasesPlugin.prototype.deregisterView = function(e) { delete this.registrations[e]; },
BasesPlugin.prototype.getViewFactory = function(e) { return Object.hasOwn(this.registrations, e) ? this.registrations[e].factory : null; },
```

The verb is **`registerView(typeKey, {name, icon, factory(controller, el), options})** — `factory`, not `create`, and `options` is a **static** function of no arguments. Duplicate registration raises a `Notice`, it does not throw. `onDisable` calls `deregisterView` for all three (`:1689-1691`). The `.base` extension itself is bound by `viewRegistry.registerViewWithExtensions(["base"], "bases", e => new MZ(e, n))` (`:1500-1502`).

### `registerOperatorFuncConfigs("bases", …)` — 11 entries, verbatim — `app/own/1972-Z5.js:1456-1500`

`hasProperty` / `isEmpty` / `startsWith` / `endsWith` / `contains` / `containsAny` / `containsAll` / `matches` / `hasLink` / `inFolder` / `hasTag`, each with a `display` and an `inverseDisplay` ("has property" / "does not have property", "links to" / "does not link to", …). This is the exact set the simple-filter builder can emit, which is why the builder recognises only those method names (see §M).

### Four CLI handlers — a **third** query pipeline

`base:query` (`:1536-1596`), `bases` (`:1597-1606`), `base:views` (`:1606-1620`), `base:create` (`:1621-1700`). `base:query` takes `{file, path, view, format: "json|csv|tsv|md|paths"}` and re-implements the query outside the controller:

```js
// app/own/1972-Z5.js:1568-1585 (excerpt)
for (h = QX.and(s.filters, c.filters), p = new jX(e, h, s.formulas, null), d = [], f = 0, v = e.vault.getMarkdownFiles(); f < v.length; f++)
  if (m = v[f], !e.metadataCache.isUserIgnored(m.path)) try {
    g = new _X(p, m), (!p.filter || p.filter.test(g)) && d.push(g);
  } catch (e) {}
return d.sort(function(e, t) { return Sw(e.file.path, t.file.path); }), …
```

Row semantics that differ from the in-app view and are worth knowing before porting either:

- **Candidate set is `vault.getMarkdownFiles()`** — non-markdown vault files can never be rows.
- **`metadataCache.isUserIgnored(path)` is the exclusion filter.** §9 never mentions the "excluded files" setting.
- **A row whose evaluation throws is silently dropped** (bare `catch (e) {}`), whereas the controller collects the message into a banner (`msgErrorFilterFailedToEvaluate`).
- **`local` is `null`** → `this` is `NullValue` in every CLI query.
- **Default ordering is `localeCompare` on `file.path` and replaces the view's sort** — `applySort` is never called.
- **Cell text is `value.toString()`**, so a Link renders `[[x|y]]`, an Object renders JSON, a List renders comma-joined. And **JSON keys are display names** (`k = y.map(propId => c.getDisplayName(propId))`, `:1581`), so two properties sharing a display name collide.

`base:create` is the CLI twin of "+ New": default name **`"Untitled"`**, `content` with `\\n`/`\\t` unescaped, frontmatter applied via `processFrontMatter` **only if non-empty** (`:1687-1689`), and it reuses the same defaults computer as the UI (`:1671`, `c = wQ(app, query, viewConfig)`).

One dead branch worth not replicating: all four handlers guard with `if ((q = cQ.fromString(text)) instanceof Error)`, but `fromString` either returns a `cQ` or **throws** (`app/own/1766-lQ.js:103-108`). The `"Failed to parse base file: …"` message can never appear.

---

## L. NEW — `QueryController` (`nJ`) is in the corpus, and the refresh gate is a `JSON.stringify` fingerprint

`bases.md` §13 Q2 is stale; the class is `nJ` at `app/own/1813-CZ.js:1893-2251`, extending a `Component`, holding `events = new tJ()` (an `Events` emitter, `:1880-1895`), constructed `(app, plugin, containerEl, currentFile)`. **The same class backs the standalone view, the `#`-selected embed and the ```base code block**, which is why §11's "QueryController source not extracted — incremental refresh opaque" was written from a partial corpus.

### Q3 answered: the query re-runs only when the *shape* of the query changes

```js
// app/own/1813-CZ.js:2076-2081
var u = this.ctx = this.buildBasesContext(r.filters),
  h = JSON.stringify({ filter: u.filter, formulas: l });     // l = {name: formula.toString()}
h !== this.queryState ? (this.queryState = h, this.runQuery(u)) : this.requestNotifyView();
```

`runQuery` is reachable from exactly two places: that fingerprint change, and `updateCurrentFile`. Global + view filters are merged by `QX.and(global, viewFilters)` (`app/own/1813-CZ.js:2083-2089`), which collapses a lone filter to itself and returns **`null`** for "no filters" — hence the `!p.filter ||` short-circuits.

### There is **no** per-entry invalidation, and the result set is rebuilt wholesale

The controller subscribes to exactly two data-ish events:

```js
// app/own/1813-CZ.js:2000-2002
this.registerEvent(this.app.metadataTypeManager.on("changed", this.requestNotifyView.bind(this))),
this.registerEvent(this.app.vault.on("config-changed", this.onConfigChanged, this)),
```

`requestNotifyView` is a **50 ms** debounce (`:1898`). **No `MetadataCache.changed`, no `resolved`, no per-file invalidation.** A frontmatter edit therefore re-renders the existing `results` map; it does not re-test the filter. Whatever re-tests it is the metadata-type manager's own change event reaching a *render*.

`runQuery` itself is a batched generator:

```js
// app/own/1813-CZ.js:2146-2156 (excerpt)
this.queue.stop(), this.errors.clear();
var i = this.queue.start(), r = OA(i.generator(), { batchSize: 10, duration: 50, maxDelay: 16 });
this.initialScan = !0, …
```

— 10 items per 50 ms slice, capped at 16 ms of synchronous work, yielding to `onNodeInserted` while the container is hidden. `notifyView` early-returns while `initialScan` is set (`:2109`), so the initial scan renders **once** at the end via `stopLoader()` (`:2210-2212`; `startLoader()` is a **no-op**). If the query reads `this`, a mid-scan active-file change **restarts the whole scan**: `if (e.localUsed) return this.runQuery(e.regenerateLocal())` (`:2174-2175`), where `localUsed` is set by the `local` getter's side effect (`app/own/1764-jX.js:6-11`).

### `view-changed` fires three times, not once — §4 needs widening

`app/own/1813-CZ.js:2054-2074` triggers it (a) when the named view is missing (alongside a `msgErrorViewNotFound` banner, `viewContainerEl.hide()` and both menus disabled — `:2094-2102`), (b) when `viewName` was empty and gets adopted from the config, and (c) **when the view *type* changes**, which also tears down and rebuilds the layout child. `BasesView` subscribes with `requestSaveLayout()` + `leaf.updateHeader()` (`app/own/1813-CZ.js:188`). Two more facts for CSS parity: `viewContainerEl` carries `dataset.viewType` / `dataset.viewName` and `toggleClass("is-grouped", !!r.groupBy)` (`:2072-2074`).

The 2 s save debounce is confirmed verbatim: `Ml(n.save.bind(n), 2e3)` (`app/own/1813-CZ.js:6-7`). And **§1's `lastData` note needs a correction**: `setViewData` overwrites `lastData` with `query.toString()`, i.e. the *re-serialised* form, not the text as read (`:184`) — so a non-canonical on-disk file is normalised into `lastData` the moment it loads.

### Search — 2026-06-10 §1 re-verified, with the exact scope

```js
// app/own/1813-CZ.js:2235-2252 (excerpt)
var i = $y(this.searchQuery).tokens;
return 0 === i.length ? e : e.filter(function(e) {
  for (var n = i.slice(), r = 0, o = t; r < o.length; r++) {
    var a = o[r];
    try { var s = e.getValue(a);
      if (!s || s instanceof oK) continue;
      for (var l = s.toString().toLowerCase(), c = n.length - 1; c >= 0; c--) l.includes(n[c]) && n.splice(c, 1);
      if (0 === n.length) return !0;
    } catch (e) {}
  }
```

Tokens are consumed **across all cells** (not per-cell), the haystack is `value.toString()` lowercased, `NullValue` cells are skipped, and per-cell throws are swallowed. `t` is the view's property list, so search covers exactly the visible columns.

---

## M. NEW — how the inline-edit widget is chosen, and what "+ New" actually seeds

### The type-introspection context is a *different* class from `BasesEntry`

`fQ` (`app/own/1771-fQ.js:2-40`) looks like an entry but is not one. Its `cacheProps()` maps **every property name known to the metadata type manager** to a *default `Value`* for that property's declared widget type:

```js
// app/own/1771-fQ.js:6-15
for (… Object.keys(this.app.metadataTypeManager.getAllProperties()); …) {
  var r = i[n], o = e.getPropertyInfo(r);
  t[o.name] = vQ(o.widget);
}
```

```js
// app/own/1772-vQ.js:2-23
"file" | "folder" | "text"   → new lK("")
"number"                       → new cK(0)
"checkbox"                     → new uK(!0)
"date"                         → new gK(new Date(), !1)
"datetime"                     → new gK(new Date(), !0)
"tags"                         → new fK([""])
"aliases" | "multitext"        → new hK([""])
default                        → oK.value
```

`getWidgetForIdent` then turns a property ID into an editor key:

```js
// app/own/1813-CZ.js:2217-2231
switch (… ) {
  case "file.file": case "file.path": return "file";
  case "file.tags":                 return "tags";
  case "file.ctime": case "file.mtime": return "date";
  case "file.folder":               return "folder";
  default:
    var t = this.mockContext.evaluateIdent(e);          // mockContext = new fQ(app, {})
    return t instanceof gK ? t.time ? "datetime" : "date" : bQ(t.type);
}
```

```js
// app/own/1776-bQ.js:2-7
var t = function(t) { return e === t || t.isPrototypeOf(e); };
return t(lK) || t(oK) ? "text" : t(cK) ? "number" : t(hK) ? "multitext"
     : t(uK) ? "checkbox" : t(gK) ? "date" : "unknown";
```

**So the inline editor is picked from the property's *declared* type, not the value's inferred type** — and `bQ`'s ordering means every `StringValue` subclass (`TagValue`, `LinkValue`, `UrlValue`, `IconValue`, `ImageValue`, `MarkdownValue`, `HTMLValue`) maps to `"text"`, every `ListValue` subclass (incl. the tag list) to `"multitext"`, and anything unrecognised to `"unknown"`. `mockContext` is also what feeds the CodeMirror completion source (`getEditorLanguageSupport()` → `new mZ(mockContext)`, `:2233-2235`).

Note this is **not** what `BasesViewConfig.getEvaluatedFormula` uses — that one builds a real `new _X(controller.ctx, currentFile)` entry (`app/own/1766-lQ.js:50-51`), so `note.*`/`file.*` resolve properly there.

### `wQ` — the "+ New" defaults computer, and the exact rule grammar it can read

`app/own/1777-wQ.js:2-59` merges global + view filters (`new JX([l, n.filters].filter(Boolean)).optimize()`), then walks `getFilterRules()` and honours **only non-negated** rules. Recognised shapes:

| Rule shape | Effect |
|---|---|
| `not <prop>.isEmpty()` | `<prop>: ""` |
| `<prop> >= \| <= \| == <literal>` | `<prop>: <evaluated literal>` |
| `file.folder == "<str>"` | the new item's **folder** |
| `<prop>.startsWith\|endsWith\|contains\|containsAll(x)` | `<prop>: x` — or `[x]` if the inferred type is `multitext` |
| `<prop>.containsAny(x)` | as above (a list arg takes its first element) |
| `file.hasTag(x)` | push `x` onto `frontmatter.tags` |
| `file.inFolder(x)` | the folder |
| `file.hasProperty(x)` | `<x>: null` |
| `<something>.tags.contains\|containsAll\|containsAny(x)` | `tags: [x]` |

Finally **every `note.*` property in `viewConfig.getOrder()` that wasn't already seeded is set to `null`**, and `{frontmatter, folder}` is returned. So "+ New" pre-populates *every visible note column* with `null`, not just the filtered ones — §9's "filter-satisfying frontmatter" understates it.

The extractor it pattern-matches on is `kQ` (`app/own/1778-kQ.js:2-39`), which peels **one** `object_access`/`array_access` off an identifier chain, resolves `note`/`file`/`formula` prefixes, defaults anything unprefixed to `note`, throws `"Unsupported left-hand side of formula"` for a bare `true`/`false`/`null` (the 1.12.7 `NY`), **unwraps a `list(...)` wrapper** (`if (e instanceof DX && "list" === e.name && 1 === e.args.length) return kQ(e.args[0])`, `:36`), and throws `"Formula cannot be deserialized"` for anything else. Bare-identifier method calls (`status.contains("x")` with no `note.` prefix) mis-parse: the name becomes `"status.contains"` (`:9-16`).

### Non-groupable properties — `aQ` finally identified

§15 lists `kY` as "is this property groupable?" without saying what it excludes. It is:

```js
// app/own/1765-sQ.js:2-9
if (aQ.includes(e)) return !1;
var n = qX(e), i = n.type, r = n.name;
if ("note" === i && "aliases" === ctx.app.metadataTypeManager.getTypeInfo(r).expected.type) return !1;
return !0;
```

`aQ = ["file.file", "file.path", "file.ctime", "file.mtime", "file.name", "file.fullname", "file.basename"]` (`app/own/1764-jX.js:283`) — **seven of the fourteen `file.*` members cannot be grouped by**, and neither can a `note.*` property whose declared type is `aliases`.

---

## N. NEW — smaller `Value`-level facts with no coverage anywhere

- **`toString()` shapes** (they leak into CSV/TSV/clipboard exports, so they are format, not cosmetics): `ListValue` → `this.join(", ").data` (`app/own/1647-tK.js:247-248`); `ObjectValue` → **`JSON.stringify` of `{key: value.toString()}` for every key** (`app/own/1649-dK.js:40-44`); `ImageValue` → `![[path]]` or `![](path)` depending on the wikilink predicate (`:444-445`); `RegExpValue` → `regexp.toString()`; `DurationValue` → `moment.duration({...}).humanize()`; `DateValue` → `YYYY-MM-DD` or `YYYY-MM-DDTHH:mm:ss` — **local time, no offset** (`:114-116`), which is what `date("…Z")` degrades to on write-back.
- **`DateValue.equals` is stricter than `looseEquals`**: `this.time === e.time && this.date.getTime() === e.date.getTime()` (`:127-128`), so the same instant with a differing `time` flag is `equals`-**false** but `looseEquals`-true.
- **`Value.toString()` of a `DateValue` fed through the DSL** is `local`, but `objectAccess("timestamp")` is UTC-epoch (`getTime()`), and `year`/`month`/… use **local** getters (`:137-156`) with `month` 1-based. Fine, but Corbomite must pick local-vs-UTC deliberately rather than inheriting it by accident.
- **`file.backlinks` linktexts are basenames, not paths**: `new EK(app, r, "", new lK(Vl(r)))` where `Vl` = basename minus `.md` (`app/own/1649-dK.js:303-310`; `Vl`/`Al`/`Ll` at `app/own/0369-Vl.js:2-5`, `app/own/0360-Al.js:2-5`, `app/own/0363-Ll.js:2-5`) and `sourcePath` is `""`.
- **`file.tags` is body tags then frontmatter tags, deduped**: `cache.tags` then `ZT(cache.frontmatter)`, then `new fK(Ud(n))` (`Ud` = `Array.from(new Set(...))`) (`:323-342`).
- **`frontmatter.tags` matching is case-insensitive on the key**: `String.isString(i) && "tags" === i.toLowerCase()` (`:80`) — so `Tags:` and `TAGS:` also become a tag list. Non-string values under a `tags` key fall through to the generic coercer.
- **Frontmatter coercion order is Link → URL → Date**, with the URL test being `!s.contains(" ") && $v(s)` (`app/own/0646-Zv.js:2-4`) — so a URL-shaped frontmatter string containing a space stays a `StringValue`. Nested lists/objects get the same lazy evaluator re-attached (`:91-92`).
- **`file.inFolder("/")` is true for every file** (`"/" === n || path.startsWith(n + "/")`, `app/own/1659-expressionstatement.js:364-366`), and the folder is NFC-normalised first (`app/own/0367-Nl.js:2-4`).
- **`file.hasProperty(x)` is case-sensitive** — it is `props.keys().contains(x)` on raw frontmatter keys (`:395-398`), unlike `objectAccess`, which lowercases. `file.hasProperty("Status") !== note.status`.
- **`tagMatches` is one-directional.** `this` must be the *more specific* tag (`n.startsWith(i) && (n.length === i.length || "/" === n.charAt(i.length))`, `app/own/1649-dK.js:10-16`), and the argument is normalised through `pK` (`"#"-prefix if absent`, `app/own/1648-pK.js:2-4`). So `child.tagMatches(parent)` is **true** while `parent.tagMatches(child)` is **false** — the reverse of §1's "`#parent` matches `#parent/child`" and §11's "`#parent` matches `#parent/child`". The public method is **`tag.matches(tag)`**, registered per-`TagValue` (`app/own/1659-expressionstatement.js:421`) and absent from 2026-04-17 §8 entirely.
- **Regexp literals keep their flags**: `pattern = src.slice(1, src.lastIndexOf("/"))`, `flags = src.slice(lastIndexOf("/") + 1)` (`app/own/1758-wX.js:95-103`), and `RealNumber` literals go through **`parseFloat`** (`:104-110`), so `0x10` would silently become `0` if the lexer ever admitted it.
- **`o.map`'s registry stub is not identity** — it returns `new hK(e.valuesRaw())` (a **ListValue of raw values**), while `o.filter`'s stub *is* identity (`app/own/1659-expressionstatement.js:267-273, 275-281`). 2026-04-17 §8.8 describes `o.map` as "returns a list (not an object)" without noting the stub differs from `o.filter`'s.
- **`o.values()` also returns raw values** (`:259-265`) — they are coerced lazily by `ListValue.lazyEvaluator` on access.
- **The function registry's `removeForType` is broken**: `delete n[t.toLowerCase()]` instead of `delete n[t.name.toLowerCase()]` (`app/own/1656-LK.js:40-42`). It only works if a caller passes `fn.name`; passing the descriptor deletes the key `"undefined"`. Nothing in the built-in registrations removes anything.
- **A `NullValue` argument to a non-`optional` parameter silently null-propagates the whole call** — `if (e === oK.value) return !1` inside the binder, and the caller does `if (!D(arg)) return oK.value` (`app/own/1759-kX.js:353-386`). So `note.foo.contains(file.missing)` is `NullValue` (falsy), not an error badge. Optional parameters instead receive **JS `null`**, which is why the implementations write `n || oK.value` / `i.data` / `e.filter(Boolean)`.
- **`if` is intercepted before the registry** (`app/own/1759-kX.js:229-238`), so the registered `if` — whose `condition` is typed `BooleanValue`, `app/own/1657-expressionstatement.js:53` — is **dead code**, and the real one uses `isTruthy()` on any value. It also **short-circuits**: the untaken branch is never evaluated. Arity is 2–3 counting a `this.if(...)` subject.
- **`min()`/`max()` return JS `null`**, not `NullValue`, for an all-empty argument list (`app/own/1657-expressionstatement.js:69-88`) — a genuine asymmetry against every other aggregate in the file.
- **`list.map`/`list.filter` always return a plain `ListValue`** (`new hK(f)`, `app/own/1759-kX.js:290`), so `file.tags.map(…)` loses `tagMatches`.
- **`round(0)` and `round(-1)` round to integer** — the guard is `t && t.data > 0` (`app/own/1659-expressionstatement.js:5-13`).
- **`createTransaction` drops no-op edits and clears the redo stack**: it pushes onto `undoStack` only if some change has `JSON.stringify(start) !== JSON.stringify(end)`, and unconditionally resets `redoStack` (`app/own/1813-CZ.js:1590-1608`). `startLoader()` is a no-op (`:2210`).
- **Group ordering**: `NumberValue` groups compare numerically (`Tw`), everything else by `localeCompare` on `toString()` (`Sw`), and **a key-less group is always last regardless of direction** — it is moved there after sorting (`app/own/1813-CZ.js:1459-1466`).

---

## O. Re-verifications (compact CONFIRMED table)

| Claim | Verdict | Anchor |
|---|---|---|
| 2026-06-10 §2: top-level `summaries:` is copied into `unrecognizedData` **and** overwritten at serialise | **still true**, and `getSerializable` still starts from `Object.assign({}, unrecognizedData)` | `app/own/1766-lQ.js:188-202`, `:205`, `:216-222` |
| 2026-06-10 §2 / bases.md §13 Q2: `QueryController` has been extracted | **true**, and it is `nJ` at `app/own/1813-CZ.js:1893` | `app/own/1813-CZ.js:1893-2251` |
| 2026-06-10 §1: Bases search is lowercased substring, AND across tokens, no DSL | **true**, scoped to the visible columns | `app/own/1813-CZ.js:2235-2252` |
| 2026-04-17 §1/§12: Lezer pipeline; `⚠` walker; leading/trailing-noise rejection; invalid sentinel; `test()` = `isTruthy()` | **all true**, byte-identical node-name table, `version: 14`, `maxTerm: 52` | `app/own/1752-hX.js:2-32`, `1754-mX.js:2-32`, `1757-bX.js:2-10`, `1759-kX.js:2-79` |
| 2026-04-17 §7: single-quoted-string escape rewriting | **true**, verbatim | `app/own/1758-wX.js:76-94` |
| 2026-04-17 §5.2: shadow contexts bind `index`/`value`/`acc`; `map`/`filter`/`reduce` registry stubs are dead | **true**; keys are exactly `["index","value","acc"]` and `["index","value"]`/`["key","value"]` | `app/own/1759-kX.js:251-338`, `1659-expressionstatement.js:219-241, 267-281` |
| 2026-04-17 §4.1/§4.2/§4.4/§4.5: null propagation (`!Null`→Null, `Null <op> x`→Null, arithmetic with Null→Null), `parseFloat` in `number()`, constant-folded `-<literal>` | **all true** | `app/own/1759-kX.js:25, 108-109, 178, 191-193, 205` |
| 2026-04-17 §4.3: `Du * N` works but `N * Du` throws; `D + D` throws | **true** | `app/own/1759-kX.js:159-170, 179` |
| 2026-04-17 §9: the 15 default summary formulas | **verbatim correct**; keyed by Value class not by a type-name string | `app/own/1759-kX.js:509-533` |
| 2026-04-17 §8.3: `isType` compares the (inherited static) type string | **true**; inheritance is via `Object.setPrototypeOf` in the `__extends` helper | `app/own/1647-tK.js:104-106`, `0026-f.js:2-11` |
| `FILE_PROPERTIES` has 14 entries while `FileValue.keys()` has 15 (adds `properties`) | **true** | `app/own/1764-jX.js:59`, `1649-dK.js:349-350` |
| Save debounce 2 s; `saveImmediately` on close | 2 s confirmed (`Ml(save, 2e3)`); 1 s for embeds | `app/own/1813-CZ.js:6-7`, `:2256` |
| `getValue` returns JS `null` for an unrecognised property-ID type | **true** | `app/own/1764-jX.js:25-37` |
| `hover-link` payload is `{source:"bases"}` | **true but incomplete** — it is a 5-key object with `event`, `hoverParent`, `targetEl`, `linktext` | `app/own/1647-tK.js:56-64` |
| Lezer keyword tokens `null`/`true`/`false` (`true`/`false` share term 58) | **new, previously unstated** | `app/own/1752-hX.js:2-6` |

---

## P. Unverifiable / not read

- **`bases.md` §13 Q4** (does Bases silently discard unsaved local edits on external `modify`?) — not answered. `onModify` is registered at `app/own/1813-CZ.js:14` and `requestSave` is at `:6-7`, but I did not read the `onModify`/`loadFileInternal` interaction in `TextFileView` this pass.
- The **table/cards/list virtualiser internals** (viewport maths, row pooling, drag-resize) were read only where they touch persisted state (§F) and `createTransaction` (§N).
- The **toolbar families** (`GX`/`KX`/`YX`/`QX`/`e$`/`t$`/`n$`/`i$`/`s$`/`a$`/`l$`/`ZX`/`$X` in the 1.12.7 naming) were not re-mapped; §1's list of them is neither confirmed nor corrected here.
- **`.base` new-file content**: `createNewBasesFile` delegates straight to `fileManager.createNewFile(parent, content, "base")` (`app/own/1972-Z5.js:1739-1743`) and I did not read `FileManager`'s new-file template path, so whether Obsidian seeds a `.base` with anything other than empty text is unestablished.
- **Plugin-facing Bases APIs** (`getBasesView`, `bases:` command registration on a `Plugin`, `registerGlobalFunc`) — `FK` is the registry and `registerGlobalFunc`/`removeGlobal` exist (`app/own/1656-LK.js:43-46`), but I did not trace the `Plugin` methods this pass. 2026-04-17 §13's claim that `registerInstanceFunc`'s teardown works is **contradicted in spirit** by the broken `removeForType` (§N) but the plugin-side call site was not read.

---

## Why noticed now

The rendering/parsing/metadata verification passes against the restored 1.13.7 corpus turned up `app.asar`'s `own/` statements as one-file-per-statement, which makes the Bases engine — previously visible only as five de-duplicated extraction windows — fully enumerable. Bases is the largest single item in `bases.md` §11's Missing table and the `.base` format is its only HIGH-severity contract, so the on-disk writer was the first thing to audit. It did not survive contact: §3's schema block does not match `hQ`, and four of the format's most consequential behaviours (`columnSize`, the tab rewrite in code blocks, the `#view` embed selector, the two-convention key split) are absent from both the domain doc and the formula addendum.

## Action taken

- No corpus or vault code changed. `libs/bases/` untouched.
- `bases.md` §14's instruction to promote the §3 schema into `VAULT-FORMAT.md` is **blocked** pending §A and §F: `note.status` does not survive a write, `columnSize` does, and neither fact is in the doc. Any Corbomite `.base` writer must apply `hQ` to `order`/`sort`/`groupBy`/`summaries` keys and *not* to `properties:`/`columnSize`, emit `summaries: {}` but not `formulas: {}` when empty, drop `limit: 0`, ignore a non-array `sort`, and relocate free-form view keys after the recognised ones.
- `libs/bases` planning should treat as spec: NOR `not:`; `html()` as Markdown; no `ms` duration unit; `date()`'s two numeric forms; `link()`'s URL branch; the null-propagating argument binder; the six-class `isEmpty()` surface; and the `group`/`sort` silent-drop rules.
- Nothing added to `01-markoff-gaps.md` — nothing here is a rendering/editor gap. `02-extension-surfaces.md` is the better home for §K's registry shapes if a plugin surface is ever planned.