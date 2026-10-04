# `resolveSubpath` falls through to heading resolution when a block or footnote lookup misses, and footnote ids match case-sensitively while block ids do not

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/parsing.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/parsing.md` §1 (`resolveSubpath`), §2 `ResolvedSubpath`, §8 (`resolveSubpath` invariants); `VAULT-FORMAT.md` §4.4 "Subpath resolution"
**Relevant cluster plans:** `libs/core` heading/subpath navigation, `MarkdownView` `setEphemeralState`, embed scroll-to-heading, `[[Note#^block]]` link resolution

## Finding

### CORRECTION 1 — the three-way dispatch has **no early return** on a block/footnote miss

§1 presents `resolveSubpath` as a clean three-way dispatch whose failure modes return `null`:
*"If `subpath` is a single segment starting with `^` (block ID), searches `cache.blocks` … Returns
`{type:"block", …}`"*. The real function only returns `null` from **two** places — an empty segment
list and an empty/missing `headings` array — and the block and footnote branches **fall through** to
the heading walk when they miss:

```js
// app/own/1122-PD.js:2-47
function PD(e, t) {
  if (!e || !t) return null;
  var n = t.split("#").filter(function(e) {
    return !!e;
  });
  if (!n || 0 === n.length) return null;
  if (1 === n.length) {
    var i = n[0];
    if (i.startsWith("^")) {
      var r = i.substr(1).toLowerCase(),
        o = e.blocks;
      if (o && o.hasOwnProperty(r))
        for (var a in o)
          if (a.toLowerCase() === r && o.hasOwnProperty(a)) {
            var s = o[a],
              l = null;
            if (e.listItems)
              for (var c = 0, u = e.listItems; c < u.length; c++) {
                var h = u[c];
                if (h.id && h.id.toLowerCase() === r) {
                  l = h;
                  break;
                }
              }
            return {
              type: "block",
              block: s,
              list: l,
              start: s.position.start,
              end: s.position.end
            };
          }
    } else if (i.startsWith("[^")) {
      var p = i.slice(2, -1);
      if (e.footnotes)
        for (var d = 0, f = e.footnotes; d < f.length; d++) {
          var v = f[d];
          if (v.id === p) return {
            type: "footnote",
            footnote: v,
            start: v.position.start,
            end: v.position.end
          };
        }
    }
  }
```

Both `if` blocks simply end. Control reaches `var m = e.headings; if (!m || 0 === m.length) return null;`
(`app/own/1122-PD.js:48-49`) and the segment is then matched against headings like any other text.

Consequences:

- `[[Note#^typo]]` in a file whose heading list is non-empty does **not** fail cleanly — it tries to
  match the literal string `"^typo"` against every heading's stripped text. It returns `null` in
  practice, but only after walking the headings, and a note with a heading literally named `^typo`
  would resolve to that heading.
- `[[Note#^[missing]]]` behaves identically.
- Only the **miss** path is affected: a hit in either branch returns early, exactly as §1 says.
- Because `cache.headings` is `delete`d when empty (`worker/own/0169-expressionstatement.js:123`, per
  the metadata addendum), `!m` is the common case for a heading-less note — and there the fall-through
  terminates at `app/own/1122-PD.js:49` immediately. In a note that *does* have headings, a missed
  block or footnote reference runs the full heading walk first. Both must be ported: the observable
  difference is which malformed links resolve, and whether a `[[Note#^typo]]` click scrolls to a
  heading literally named `^typo`.

### CORRECTION 2 — footnote ids match **exactly** (case-sensitive), and the `]` is mandatory

§8 records only that block comparison is case-insensitive. The footnote branch is the opposite, and
it also strips the closing bracket unconditionally:

```js
// app/own/1122-PD.js:35
      var p = i.slice(2, -1);
      if (e.footnotes)
        for (var d = 0, f = e.footnotes; d < f.length; d++) {
          var v = f[d];
          if (v.id === p) return {
```

- `slice(2, -1)` drops the leading `[^` **and the last character whatever it is**. `#[^1]` → `"1"`
  (correct), `#[^1` → `""`, `#[^1]]` → `"1]"`. There is no validation that the segment ends with `]`.
- `v.id === p` is `===`, so `#[^Note]` does not match a footnote with id `"note"`. This is
  consistent with the parser, which lowercases footnote identifiers when emitting the node
  (`app/own/1083-expressionstatement.js:349`,
  `identifier: i.toLowerCase(), label: i`), so `CachedMetadata` ids are always lower-case and a
  mixed-case link can never resolve. See `2026-10-03-parsing-footnote-grammar-and-inline-ids.md`.
- By contrast the block branch lowercases the query (`i.substr(1).toLowerCase()`) *and* lowercases
  each candidate key (`a.toLowerCase() === r`), and it pre-tests
  `o.hasOwnProperty(r)` — which succeeds directly because `blocks` keys are already lowercased
  (`2026-10-03-metadata-worker-cached-metadata-shape.md`).

### CONFIRMED — the heading walk and both strip helpers, verbatim

```js
// app/own/1122-PD.js:48-63
  var m = e.headings;
  if (!m || 0 === m.length) return null;
  for (var g = 0, y = 0, b = null, w = null, k = 0, C = m; k < C.length; k++) {
    var E = C[k];
    if (b && E.level <= y) {
      w = E;
      break;
    }!b && E.level > y && TD(E.heading).toLowerCase() === TD(n[g]).toLowerCase() && (g++, y = E.level, g === n.length && (b = E));
  }
  return b ? {
    type: "heading",
    current: b,
    next: w,
    start: b.position.start,
    end: w ? w.position.start : null
  } : null;
```

Both regexes verbatim, for anyone comparing character by character (note the **literal backtick**
inside the first class — the frozen doc's rendering of it is ambiguous about that):

```js
// app/own/1118-SD.js:2-3
var SD = /[!"#$%&()*+,.:;<=>?@^`{|}~\/\[\]\\\r\n]/g,
  xD = /([:#|^\\\r\n]|%%|\[\[|]])/g;
```

| §1 / §8 claim | Location | Verdict |
|---|---|---|
| Splits on `#`, filters empties, `null` if nothing is left | `app/own/1122-PD.js:4-7` | **CONFIRMED** |
| Block: case-insensitive key comparison | `app/own/1122-PD.js:11,15` | **CONFIRMED** — plus an extra `o.hasOwnProperty(r)` fast path at `:13` that only works because `blocks` keys are pre-lowercased |
| Block: also searches `listItems` for a matching id, returned as `list` | `app/own/1122-PD.js:18-25` | **CONFIRMED** — first match wins, and `list` is `null` when the block is not a list item |
| Footnote: searches `cache.footnotes` by id, returns `{type:"footnote", footnote, start, end}` | `app/own/1122-PD.js:34-45` | **CONFIRMED** as to shape; **case-sensitive** and **unguarded `slice(2,-1)`** (Correction 2) |
| Heading: source-order walk, each segment must match at a **strictly deeper** level (`y` starts at 0) | `app/own/1122-PD.js:50-56` | **CONFIRMED** |
| Heading: `end` is the **next same-or-lower-level** heading's start, `null` at EOF | `app/own/1122-PD.js:52-55,62` | **CONFIRMED** — note the `break` on finding `w`, so `next` is the first such heading, not the last |
| Heading: `stripHeading` applied to **both** sides, then lowercased | `app/own/1122-PD.js:55` — `TD(E.heading).toLowerCase() === TD(n[g]).toLowerCase()` | **CONFIRMED** |
| `stripHeading` replacement class (audit calls it `AT`), then collapse + trim | `app/own/1118-SD.js:2` (`SD`) + `app/own/1119-TD.js:2-4` (`TD`) | **CONFIRMED, byte-identical** |
| `stripHeadingForLink` replacement class (audit calls it `PT`), then collapse + trim | `app/own/1118-SD.js:3` (`xD`) + `app/own/1120-DD.js:2-4` (`DD`) | **CONFIRMED, byte-identical** — both regexes live in the *same* two-line module (`app/own/1118-SD.js`) |
| §15's "AT/PT defined in `utils/` at `app.renamed.js:46404-46405`" | — | **DEAD CITATION.** Worse, `AT` and `PT` *do* exist in 1.13.7 at `app/own/1073-kT.js:10-11` — as webpack module ids (`AT = n(7583)`, `PT = n(942)`), i.e. unrelated code that happens to share the minified names. Anyone grepping for `AT`/`PT` in this corpus will land on the wrong thing |
| `resolveSubpath` returns `null` when `cache` or `subpath` is falsy | `app/own/1122-PD.js:3` | **CONFIRMED** |

### NEW — heading *display* text is a third, separate normaliser

The subpath walk uses `TD` (`stripHeading`). Elsewhere the same job is done by parsing the heading's
markdown and flattening it:

```js
// app/own/1121-AD.js:2-6
function AD(e) {
  var t = nD(WT(e.heading));
  return t || (t = e.heading), t;
}
```

i.e. re-parse `heading` with markdown-it (`WT`, `app/own/1082-WT.js:2-9`) and flatten the resulting
hast tree to plain text with `nD` (`app/own/1096-nD.js:2-14`, which drops `html` and `comment`
subtrees, renders an `iembed` as its `href`, a `tag` as its `tag`, and appends `\n` for
paragraph/break/heading/code), falling back to the raw stored string when that yields nothing. This is what feeds heading
suggestions and link generation (`app/own/1332-GR.js:82` builds `subpath: "#" + DD(f.name)` — note
`DD`, `stripHeadingForLink`, not `AD`). Three heading normalisers exist: `TD` for comparison,
`DD` for link fragments, `AD` for display. §1 and `VAULT-FORMAT.md` §4.4 only document two.

## Why noticed now

`resolveSubpath` is on the hot path for every `[[Note#…]]` click, every `![[Note#…]]` embed, and
`setEphemeralState` on file open, and it is the function that decides what "scroll to heading" means.
The audit's version describes a dispatch table; the source has two missing early returns, and one of
its three branches uses a different comparison rule from the other two. That combination is exactly
the kind of detail that makes a partial port pass every happy-path test and then mis-scroll on
malformed or renamed-target links.

## Action taken

No Corbomite code touched — `parsing.md` §11 lists `resolveSubpath` as only Partial
(`Markoff::Document::extractSubpath`, whose implementation the audit admits it never read).
Recorded as the port contract:

- Implement the block/footnote branches as **fall-through**, not early-return-on-miss.
- Footnote lookup: exact match on `slice(2, -1)`, case-sensitive, no `]` validation.
- Block lookup: lowercase the query, compare against lowercased keys, plus the `hasOwnProperty`
  fast path (valid only because `blocks` is keyed lowercase).
- Heading walk: `y` starts at 0, strictly-deeper levels only, `next` = first same-or-lower heading.
- Port all three heading normalisers (`TD`, `DD`, `AD`) or consciously document which two you have.

`VAULT-FORMAT.md` §4.4's numbered "Subpath resolution" list needs the fall-through and the
case-sensitivity asymmetry; its `stripHeading` regexes are confirmed correct as written.
