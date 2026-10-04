# Footnote grammar: ids may not contain spaces, `[^` is carved out of markdown-it's reference syntax, dangling references degrade to bare text, repeat labels are `[1-1]`, and an inline footnote's cache id is the literal string `[inline0`

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/parsing.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/parsing.md` has no footnote coverage at all (its §5/§8/§15 do not mention footnotes despite `metadata.md` modelling them); extends `addenda/2026-10-03-metadata-worker-cached-metadata-shape.md` Correction 3, which establishes *which array* an `fnRef` lands in but not how the ids in those arrays are formed
**Relevant cluster plans:** `libs/storage` `MetadataCache`/`NoteMetadata`, Markoff footnote rendering and editing, `docs/PARITY-MATRIX.md` footnote rows

## Finding

### NEW — the footnote id grammar is "anything but space, tab, LF", and `[^` is reserved

Two inline rules and one block rule, all in the markdown-it plugin bundle
(`app/own/1083-expressionstatement.js`, worker twin `worker/own/0157-expressionstatement.js`):

```js
// app/own/1083-expressionstatement.js:329-353
  OE(e, "footnoteCall", "reference", function(e, t, n) {
    var i,
      r,
      o,
      a,
      s = t.length + 1,
      l = 0;
    if (t.charCodeAt(l++) === HE && 94 === t.charCodeAt(l++)) {
      for (r = l; l < s;) {
        if ((a = t.charCodeAt(l)) != a || 10 === a || 9 === a || a === VE) return;
        if (93 === a) {
          o = l, l++;
          break;
        }
        l++;
      }
      if (void 0 !== o && r !== o) return !!n || (i = t.slice(r, o), e(t.slice(0, l))({
        type: "footnoteReference",
        identifier: i.toLowerCase(),
        label: i
      }));
    }
  }, function(e, t) {
    return e.indexOf("[", t);
  });
```

with `VE = 32` (space) and `HE = 91` (`[`) at `app/own/0926-VE.js:2-3`. So:

- `[^a b]`, `[^a\tb]` and anything spanning a newline are **not footnotes at all** — the rule bails
  and the text stays literal. The id charset is "any char except space, tab, LF, `]`", i.e. `]` is
  the terminator and whitespace is forbidden.
- `[^]` is rejected (`r !== o`, an empty id).
- **`identifier` is lowercased; `label` keeps the author's casing** (`app/own/1083-expressionstatement.js:347-348`). The cache records
  the lowercased form, which is why `resolveSubpath`'s footnote branch can compare with `===`
  (`app/own/1122-PD.js:39`) and why a mixed-case link can never resolve — see
  `2026-10-03-parsing-resolve-subpath-fallthrough.md`.

The block definition rule requires the colon to be **immediately** after `]`, and does the same
charset check:

```js
// app/own/1083-expressionstatement.js:291-304, 320-326
  FE(e, "footnoteDefinition", "definition", function(e, t, n) {
    for (var i, r, o, s, l, c, u, h, p, d, f, v, m, g = this, y = g.interruptFootnoteDefinition, b = g.offset, w = t.length + 1, k = 0, C = []; k < w && (9 === (s = t.charCodeAt(k)) || s === VE);) k++;
    if (t.charCodeAt(k++) === HE && 94 === t.charCodeAt(k++)) {
      for (r = k; k < w;) {
        if ((s = t.charCodeAt(k)) != s || 10 === s || 9 === s || s === VE) return;
        if (93 === s) {
          o = k, k++;
          break;
        }
        k++;
      }
      if (void 0 !== o && r !== o && 58 === t.charCodeAt(k++)) {
        if (n) return !0;
```

`58` is `:`. `[^a] : text` (space before the colon) is **not** a definition. Leading spaces/tabs
before `[^` are allowed. The body is then re-tokenised as a block with a 4-column tab-stop model
(`:304-317` — each body line gets its own `{start, contentStart, contentEnd, end}`,
with `contentStart` advanced past the indent), and trailing content-free lines are
dropped (`:319`,
`for (k = -1, w = v.length; w > 0 && v[w - 1].contentStart === v[w - 1].contentEnd;) w--;`).
The dedented lines are then re-tokenised as a block (`C.join("")` at `:320`) instead of being kept
as raw text, which is why a definition body can itself contain block syntax.

`[^` is additionally **carved out of markdown-it's own reference-link syntax**, so a footnote
reference is never mistaken for a markdown reference link:

```js
// app/own/1083-expressionstatement.js:360-366
  function u(e, t, n) {
    var i = 0;
    if (33 === t.charCodeAt(i) && i++, t.charCodeAt(i) === HE && 94 !== t.charCodeAt(i + 1)) return c.call(this, e, t, n);
  }
  a.definition = function(e, t, n) {
    for (var i = 0, r = t.charCodeAt(i); r === VE || 9 === r;) r = t.charCodeAt(++i);
    if (r === HE && 94 !== t.charCodeAt(i + 1)) return l.call(this, e, t, n);
  }, s.reference = u, u.locator = c.locator;
```

`33` is `!` and `94` is `^`. The stock rules therefore keep handling `![alt][ref]` and `[text][ref]`,
and only the `[^` form is diverted to the footnote rules.

Finally, a footnote definition can be interrupted by anything that can interrupt a paragraph,
because the interrupt list is *derived* rather than written out:

```js
// app/own/1083-expressionstatement.js:248-252
function(e) {
  for (var t = e.prototype, n = t.blockMethods, i = [], r = 0; r < n.length; r++) {
    var o = n[r];
    "newline" !== o && "indentedCode" !== o && "paragraph" !== o && "footnoteDefinition" !== o && i.push([o]);
  }
  i.push(["footnoteDefinition"]), t.interruptFootnoteDefinition = i, OE(e, "inlineNote", "reference", function(e, t, n) {
```


i.e. every registered block method except those four, plus `footnoteDefinition` itself (so a run of
consecutive definitions works).

### NEW — a dangling footnote reference renders as its **bare id** and produces no cache entry

```js
// worker/own/0157-expressionstatement.js:556-561   (app twin: app/own/1083-expressionstatement.js)
      } else {
        s = String(e.identifier);
        if (!r.hasOwnProperty(":" + s)) return e.type = "text", void(e.value = s); - 1 === (o = r[":" + s]) && (o = t.length, r[":" + s] = o, t.push({
          count: 0,
          identifier: ":" + s
        }));
```

`[^missing]` is **rewritten to the plain text `missing`** — the `[^` and `]` are gone from the
rendered output, and because the node is no longer an `fnRef` it never reaches `CachedMetadata`
(`worker/own/0169-expressionstatement.js:42-50` only handles `"fnRef"`). This also means a typo'd
footnote reference is **invisible to the unresolved-link machinery**.

The mirror case: a footnote **definition with no reference** is registered in `n` but never pushed
onto the render list `t` (the `t.push` only happens from the reference branch at `:557-561`), so
the final loop at `worker/own/0157-expressionstatement.js:567-570` never visits it and it is not rendered at all.

### NEW — footnote labels are 1-based with a dash suffix for repeats

```js
// worker/own/0157-expressionstatement.js:500-513  (the label itself is built at :504-505)
    function i(e, t, n) {
      var r,
        i,
        o,
        s,
        a = String(t + 1),
        l = a;
      if (n > 0 && (l += "-" + n), e.type = "fnRef", e.data = {
          hName: "sup",
          hProperties: {
            className: "footnote-ref",
            "data-footnote-id": "fnref-" + l
          }
        }, !e.identifier && e.hasOwnProperty("children")) {
```

and the call site for a repeat:

```js
// worker/own/0157-expressionstatement.js:562-564
        var a = t[o],
          l = a.count;
        a.count++, i(e, o, l);
```

The displayed label is `String(index + 1)`, and for the *n*-th repeat of the same footnote
(`n > 0`) it becomes `` `${label}-${n}` ``. So one definition referenced three times renders as
`[1]`, `[1-1]`, `[1-2]` — three distinct anchors, back-referenced as `#fnref-1`, `#fnref-1-1`,
`#fnref-1-2` (`worker/own/0157-expressionstatement.js:511`, `:581-583`). Inline footnotes (`^[…]`)
are numbered by document order and never get a suffix (they pass `n = 0`).

### NEW — an inline footnote's `CachedMetadata` id is the literal string `"[inline" + N`

```js
// worker/own/0157-expressionstatement.js:513-521
        !e.identifier && e.hasOwnProperty("children") {
          var c = e.children,
            u = null === (i = null === (r = c[0]) || void 0 === r ? void 0 === r.position) || void 0 === i ? void 0 === i.start,
            f = null === (s = null === (o = c[c.length - 1]) || void 0 === o ? void 0 === o.position) || void 0 === s ? void 0 === s.end;
          u && f && (e.data.contentPosition = {
            start: u,
            end: f
          }, e.identifier = "[inline".concat(t));
        }
```

`"[inline".concat(t)` — note the **missing closing bracket**. `t` is the 0-based ordinal of the
footnote in document order (`worker/own/0157-expressionstatement.js:546-555`,
`var o = t.length; … i(e, o, 0)`), so the first inline footnote in a note gets the id `[inline0`.

Two things follow:

1. This is the `id` recorded in `CachedMetadata.footnotes[i].id`, because the branch that sets
   `contentPosition` is exactly the branch the metadata addendum identified as "inline footnote"
   (`worker/own/0169-expressionstatement.js:43-46`). The other addendum correctly reports that the
   position comes from the *content* rather than the mark; it does not report the id shape.
2. **Inline footnotes are addressable, by accident.** `resolveSubpath` computes
   `p = subpath.slice(2, -1)` after seeing `[^`
   (`app/own/1122-PD.js:34-39`), so the subpath `#[^[inline0]` yields `p === "[inline0"` and matches.
   Nothing generates that link, but a hand-written one resolves.

Also worth recording: the inline-footnote rule requires **balanced `[]` inside the note body**, skips
backslash escapes, and skips bracketed code spans:

```js
// app/own/1083-expressionstatement.js:263-288   (rule registered at :252)
      if (94 === t.charCodeAt(h++) && t.charCodeAt(h++) === HE) {
        for (o = h; h < u;) {
          if ((r = t.charCodeAt(h)) != r) return;
          if (void 0 === l) {
            if (92 === r) h += 2;
            else if (r === HE) p++, h++;
            else if (93 === r) {
              if (0 === p) {
                a = h, h++;
                break;
              }
              p--, h++;
            } else if (96 === r) {
              for (s = h, l = 1; 96 === t.charCodeAt(s + l);) l++;
              h += l;
            } else h++;
          } else if (96 === r) {
            for (s = h, c = 1; 96 === t.charCodeAt(s + c);) c++;
            h += c, l === c && (l = void 0), c = void 0;
          } else h++;
        }
        if (void 0 !== a) return !!n || ((i = e.now()).column += 2, i.offset += 2, e(t.slice(0, h))({
          type: "footnote",
          children: this.tokenizeInline(t.slice(o, a), i)
        }));
```

(`92` is `\`, `96` is `` ` ``, `93` is `]`.) An unbalanced `[` inside `^[…]` therefore swallows the
closing `]`. The token's inline state is `e.now()` with `column` and `offset` advanced by exactly 2 —
the length of the consumed `^[` prefix — and the position the cache records for an inline footnote
comes from its children (`contentPosition`, above), so neither is the offset of the `^` itself.

### NEW — the rendered footnote `listItem`s are synthetic and never reach `CachedMetadata`

```js
// worker/own/0157-expressionstatement.js:591-598
        o.push({
          type: "listItem",
          synthetic: !0,
          data: {
            hProperties: {
              "data-footnote-id": "fn-" + f
            },
            isInline: l.isInline
```

and the cache builder drops both flavours:

```js
// worker/own/0169-expressionstatement.js:95-99
          } else if ("listItem" === r.type) {
            if (r.synthetic) {
              if (null === (a = r.data) || void 0 === a ? void 0 === a.isInline) return;
              return "skip";
            }
```

So no synthetic item appears in `CachedMetadata.listItems` (inline ones `return` — a plain CONTINUE,
so their children are still walked; block ones return the `unist-util-visit-parents` SKIP sentinel,
`worker/vendor/2888.js:9,31`). Neither flavour reaches the cache, but the *list* they form also does
not, so the footnotes section of a note contributes no `listItems`.

### CONFIRMED

| Claim | Location | Verdict |
|---|---|---|
| `footnoteRefs` / `footnotes` entries are `{id, position}` | `worker/own/0169-expressionstatement.js:42-54` | **CONFIRMED** as to shape (see the other addendum for which array) |
| Footnote nodes carry a lowercased `identifier` and an original-case `label` | `app/own/1083-expressionstatement.js:322-325, 345-349` | **CONFIRMED** |
| The inline-footnote branch is selected by "no `identifier` and has children" | `worker/own/0157-expressionstatement.js:513` | **CONFIRMED** |
| `[^` is intercepted before markdown-it's reference rule | `app/own/1083-expressionstatement.js:360-366` | **CONFIRMED** |

### Corbomite divergence (for the implementer, not an Obsidian claim)

Markoff's extractor uses different rules for the same syntax:

```cpp
// libs/markoff-family/libs/markoff-parser/src/Document.cpp:68-71, 76
    static const QRegularExpression footnoteDef(
        QStringLiteral(R"(^\[\^([^\]]+)\]:\s*(.+)$)"),
        QRegularExpression::MultilineOption);
…
    static const QRegularExpression footnoteRef(QStringLiteral(R"(\[\^([^\]]+)\])"));
```

`[^\]]+` **admits spaces and tabs** in the id (Obsidian rejects the whole construct), the `:` may be
preceded by whitespace (`\s*`, Obsidian requires adjacency), and the definition body is captured to
end-of-line only, so a multi-line footnote definition body is not captured at all. Obsidian's
behaviour is the three rules in this addendum.

## Why noticed now

`parsing.md` never covered footnotes at all, yet footnote ids are exactly the kind of value that
ends up in a cache key and in a link-resolution table, and `metadata.md` §2's footnote model is
"inferred from general Obsidian plugin lore". The dangling-reference demotion and the `[inline0` id
in particular are invisible to any test written from the audit, because both produce *plausible*
output rather than an error.

## Action taken

No Corbomite code touched. Recorded as the footnote contract:

- Ids: any character except space, tab, LF and `]`; lowercased for `identifier`, original case kept
  only as `label`; empty id rejected; `[^a] : text` is not a definition; `[^` is reserved ahead of
  markdown-it's reference syntax.
- A dangling `[^x]` renders as the bare text `x` and creates no cache entry — do not surface it as
  an unresolved link.
- An unreferenced `[^x]: …` definition is not rendered.
- Labels: 1-based, `${label}-${n}` for the *n*-th repeat (0-based suffix), so repeats are `[1]`,
  `[1-1]`, `[1-2]`.
- Inline footnotes: `CachedMetadata.footnotes[i].id == "[inline" + <0-based ordinal>`; body offsets
  come from the first/last child, not from the `^[` mark; brackets inside must balance; backslash
  escapes and code spans are skipped.
- Rendered footnote `listItem`s are synthetic and never enter `CachedMetadata.listItems`.

Markoff's `Document::extract` footnote regexes should be tightened to match the three rules above
(§"Corbomite divergence"); that is a Markoff change, not an Obsidian-compat doc change.
