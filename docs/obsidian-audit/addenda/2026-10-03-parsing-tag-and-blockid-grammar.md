# Tag grammar (an explicit character class, `#`+digits excluded, frontmatter-then-inline with no de-dup) and block-id grammar (two rules, and the id is attached to the *preceding* node)

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/parsing.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/parsing.md` §1 `parseFrontMatterTags` / §15 (no tag grammar at all); extends `addenda/2026-10-03-metadata-worker-cached-metadata-shape.md`'s block-id note (it cites only `worker/own/0157-expressionstatement.js:398`); `VAULT-FORMAT.md` §4.3
**Relevant cluster plans:** `superpowers/plans/2026-10-01-cluster-q-tags-pane.md` (its §8 cites `parsing.md` line 337 + §`parseFrontMatterTags` directly, and it must implement the subtag decomposition this file specifies), `libs/storage` `SQLiteIndex` tag population

## Finding

### NEW — there is a real, explicit inline-tag grammar, and it is not "anything up to whitespace"

`parsing.md` gives the *frontmatter* tag rules but no body-tag grammar at all. There is one, a
single character class, duplicated verbatim in both bundles (`app/own/1078-BT.js:2` and its worker
twin `worker/own/0152-ar.js:2`):

```js
// app/own/1078-BT.js:2
var BT = /^#[^\u2000-\u206F\u2E00-\u2E7F'!"#$%&()*+,.:;<=>?@^`{|}~\/\[\]\\\s]+/;
```

A tag is `#` plus one-or-more characters that are **not** any of:

- General Punctuation, U+2000–U+206F;
- Supplemental Punctuation, U+2E00–U+2E7F;
- the 30 ASCII punctuation characters in the class body — apostrophe, `!`, `"`, `#`, `$`, `%`, `&`,
  `(`, `)`, `*`, `+`, `,`, `.`, `:`, `;`, `<`, `=`, `>`, `?`, `@`, `^`, backtick, `{`, `|`, `}`, `~`,
  `/`, `[`, `]`, backslash;
- whitespace (`\s`).

Letters, digits, `_`, `-`, and non-Latin scripts (CJK, emoji) **are** allowed. Executed against the
verbatim regex:

| input | tag matched | why |
|---|---|---|
| `#work` | `#work` | |
| `#work/sub` | `#work` | **terminates at the first excluded char** — `/` is punctuation, so `#work/sub` is *not* one tag |
| `#a-b_c` | `#a-b_c` | `-` and `_` are allowed |
| `#日本語` | `#日本語` | allowed (not in either punctuation block) |
| `#tag,comma` | `#tag` | terminates at `,` |
| `#tag!` / `#tag.` / `#tag%` | `#tag` | terminates at the punctuation |
| `#a#b` | `#a` | terminates at `#` |
| `# tag` | *no match* | the space is excluded, so `#` alone cannot satisfy the `+` |
| `##h` | *no match* | `#` is excluded from the class |

The `#a#b` row matters most: **`#` is not a legal character inside a tag**, so nested tags are
*not* expressed as `#parent/child` in the body — `/` is excluded too. A body tag is a single flat
token; Obsidian's subtag descent is a *search-time* convention, not a lexical one (see the matcher
below).

### NEW — a tag that is nothing but digits is not a tag

```js
// app/own/1083-expressionstatement.js:90-96   (worker twin: worker/own/0157-expressionstatement.js:90-96)
  var t = /^#\d+$/,
    n = /\s/;
  OE(e, "tag", "image", function(e, n, i) {
    var r = BT.exec(n);
    if (r) {
      var o = r[0];
      return !t.test(o) && (!!i || e(o)({
        type: "tag",
        tag: o,
```

`#123`, `#42`, `#0` are excluded outright; `#1a` is a tag (the exclusion is anchored on both
sides). Presumably so GitHub/Linear issue references and heading anchors are not indexed as tags.

### NEW — a tag must be preceded by whitespace or sit at the very start of the inline run

```js
// app/own/1083-expressionstatement.js:112-119
  }, function(e, t) {
    for (var i = t; - 1 !== (i = e.indexOf("#", i));) {
      if (i <= 0) return i;
      var r = e.charAt(i - 1);
      if (n.test(r)) return i;
      i++;
    }
  });
```

The locator walks forward for `#`, requires `/\s/` immediately before it, and returns immediately at
a `#` found at index `≤ 0`. So `a#b` is not a tag, while `#tag` at the very start of a paragraph
is (that is exactly what the `i <= 0` branch is for). Note the block-id locator has the same shape
but returns `-1` in that branch (`app/own/1083-expressionstatement.js:409-416`) — see below.

### CORRECTION — frontmatter tag filtering rejects the literal space U+0020 and nothing else

§1 says *"Filters out blank strings and tags containing a space character (tags with spaces are
invalid in Obsidian)"*. Precisely:

```js
// app/own/1092-ZT.js:6-8
    t = t.filter(function(e) {
      return !!e && !e.includes(" ")
    });
```

One `String.prototype.includes(" ")` — U+0020. A tag written `tags: ["a\tb"]` or
`tags: ["a\u00A0b"]` in frontmatter **survives** and becomes `#a\tb` / `#a\u00A0b`, even though the
identical text in the body would be truncated by the grammar above at the tab/NBSP. Two different
rules for two different sources, and they disagree. The same literal-space filter is reused for
`cssclasses` (`app/own/1093-JT.js:5-7`).

### CORRECTION — `getAllTags` concatenates frontmatter-then-inline with **no de-duplication**, and a second tag list does the opposite

§1's cross-reference says `getAllTags` "calls this function and appends inline `#tag` occurrences to
the result". True but incomplete in three ways that matter for any tag-count UI:

```js
// app/own/1117-MD.js:2-18
function MD(e) {
  if (!e) return null;
  var t = [],
    n = ZT(e.frontmatter);
  if (n)
    for (var i = 0, r = n; i < r.length; i++) {
      var o = r[i];
      t.push(o);
    }
  var a = e.tags;
  if (a)
    for (var s = 0, l = a; s < l.length; s++) {
      o = l[s];
      t.push(o.tag);
    }
  return t;
}
```

- **No de-dup.** `tags: [work]` plus a body `#work` yields `["#work", "#work"]`.
- **No case folding.** frontmatter `tags: [Work]` yields `#Work`; a body `#work` yields `#work`.
  Both land in the cache, both are returned.
- **Frontmatter first, inline second** — and there is a *second* function that reverses it and
  *does* de-dup:

```js
// app/own/1649-dK.js:323-343
    }, t.prototype.getTags = function() {
      if (this._cachedTags) return this._cachedTags;
      var e = this.app,
        t = this.file,
        n = [],
        i = e.metadataCache.getFileCache(t);
      if (i) {
        if (i.tags)
          for (var r = 0, o = i.tags; r < o.length; r++) {
            var a = o[r];
            n.push(a.tag);
          }
        var s = ZT(i.frontmatter);
        if (s)
          for (var l = 0, c = s; l < c.length; l++) {
            var u = c[l];
            n.push(u);
          }
      }
      return this._cachedTags = new fK(Ud(n));
    },
```

with `Ud = Array.from(new Set(e))` (`app/own/0555-Ud.js:2-4`). So a file's *properties* tag list
(`getTags`) is inline-first, de-duplicated and order-stable; the *plugin-facing* `getAllTags` is
frontmatter-first and duplicated. Any Corbomite "tags on this note" UI that picks one of these
silently disagrees with Obsidian's own two surfaces.

### NEW — the search-time tag matcher is what makes subtags work, and it reports the frontmatter key as spelled

```js
// app/own/1175-SP.js:105-137
  TP = function(e) {
    function t(t) {
      var n = e.call(this) || this;
      return "#" !== t.text.charAt(0) && t.setText("#" + t.text), n.matcher = t, n;
    }
    return v(t, e), t.prototype.match = function(e) {
      var t = new RegExp("^" + this.matcher.regex + "($|\\/)", "i");
      if (e.strings.hasOwnProperty("tag") && t.test(e.strings.tag)) return {};
      if (e.strings.hasOwnProperty("content")) {
        var n = e.cache;
        if (!n) return null;
        var i = void 0,
          r = [],
          o = !1,
          a = ZT(n.frontmatter);
        if (a)
          for (var s = IA(n.frontmatter, "tags"), l = 0, c = a; l < c.length; l++) {
            var u = c[l];
            t.test(u) && (o = !0, i = [{
              key: s
            }]);
          }
        var h = n.tags;
        if (h)
          for (var p = 0, d = h; p < d.length; p++) {
            u = d[p];
            t.test(u.tag) && (r.push([u.position.start.offset, u.position.end.offset]), o = !0);
          }
        return o ? {
          content: r,
          properties: i
        } : null
```

- The query text is forced to start with `#` (`t.setText("#" + t.text)`).
- The matcher is `^(regex)($|\/)` — anchored at the start, case-insensitive, and matching either the
  whole tag or a `/`-delimited **prefix**. This is the entire subtag mechanism: searching `#a`
  matches `#a/b`. (Consistent with the grammar above — `/` can never be *inside* a lexical tag, so
  the `/` in `^($|\/)` can only ever be matching a separator the user typed.)
- Frontmatter hits are reported as a **property key** (via `IA`, so `Tags:` is reported as `Tags`),
  while inline hits are reported as an **offset range**. A note matching both yields both shapes in
  one result. `IA` itself is quoted in `2026-10-03-parsing-frontmatter-info-offsets-and-parser-divergence.md`.

### NEW — block ids have **two** rules, and the id is grafted onto the preceding node

The metadata addendum records the inline charset `/^\^([a-zA-Z0-9\-]+)$/`
(`worker/own/0157-expressionstatement.js:398`) but not the second, block-level rule beside it:

```js
// app/own/1083-expressionstatement.js:397-416   (worker twin: worker/own/0157-expressionstatement.js:397-416)
function(e) {
  var t = /^\^([a-zA-Z0-9\-]+)$/,
    n = /\s/;
  OE(e, "blockid", "text", function(e, n, i) {
    var r = t.exec(n);
    if (r) {
      var o = r[1];
      return !!i || e(r[0])({
        type: "blockid",
        id: o
      });
    }
  }, function(e, t) {
    for (var i = t; - 1 !== (i = e.indexOf("^", i));) {
      if (i <= 0) return -1;
      var r = e.charAt(i - 1);
      if (n.test(r)) return i;
      i++;
    }
  });
```

followed by

```js
  var i = /^\^([a-zA-Z0-9\-]+)(?=$|\n$|\n\n)/;
  FE(e, "blockid", "paragraph", function(e, t, n) {
    var r = i.exec(t);
    if (r) {
      var o = r[1];
      return !!n || e(r[0])({
        type: "blockid",
        id: o
      });
    }
  });
```

The paragraph-level rule anchors at the **start of a paragraph** and requires the id to be followed
by end-of-paragraph, one newline, or a blank line (`(?=$|\n$|\n\n)`). Because it consumes only
`r[0]`, it **terminates the paragraph at the id**: `^abc` followed by a blank line and then text
yields a `blockid` token and two paragraphs, whereas `^abc` followed directly by text on the next
line matches nothing at all (the lookahead fails) and stays literal text. This is why the id must
be on its own line or at the end of the paragraph.

Then a transformer decides what the id names:

```js
// worker/own/0157-expressionstatement.js:675-698   (app twin: app/own/1083-expressionstatement.js:669-692)
function(e, t) {
  t.blockid = function() {
    return null;
  }, e.addTransformer(function(e) {
    for (var t, n = e.children, r = function(e) {
        var r = n[e];
        if ("blockid" === r.type) {
          if (e > 0) n[e - 1].id = r.id;
          return n.splice(e, 1), e--, t = e, "continue";
        }
        if ("list" === r.type) return h(r, "listItem", function(e) {
          var t = null;
          h(e, ["list", "blockid"], function(e) {
            return "list" === e.type ? "skip" : (t = e.id, !1);
          }), t && (e.id = t);
        }), t = e, "continue";
```

- The `blockid` **node is removed from the tree** and its `id` is stamped onto `n[e - 1]`, the
  **preceding sibling**. (At index 0 there is no sibling, so a leading `^id` is silently dropped —
  which is why the block rule needs the blank line after it.)
- Because the node is spliced out, the `blocks` position recorded later is the *host* node's
  position, never the id's own offset — matching what
  `2026-10-03-metadata-worker-cached-metadata-shape.md` observed but never explained.
- The host need not be a heading: any sibling node type takes the id (`if (i && (r.id = i), t = e)`
  in the same function), which is why the metadata addendum found `blocks` fed from "sections and
  list items" as well as headings.
- For a list, each `listItem` takes the first `blockid` among its children, and `"skip"` on a
  `list` node (a `unist-util-visit-parents` SKIP, `worker/vendor/2888.js:9,31`) means **nested
  sub-lists are not searched** — a `^id` inside a sub-list cannot name the parent item.

### CONFIRMED

| Claim | Location | Verdict |
|---|---|---|
| `TagCache.tag` includes the leading `#` | `app/own/1083-expressionstatement.js:97-102` — `tag: o` where `o` is `r[0]` from `BT`, which starts at `#` | **CONFIRMED** |
| Tags are collected for any nesting depth, not only top level | the rule is registered on `image` (i.e. inside the inline chain at every level) — `app/own/1083-expressionstatement.js:92` | **CONFIRMED** |
| Frontmatter tag values never store the `#`; it is injected at read | `app/own/1092-ZT.js:11` — `"#" !== i.charAt(0) && (t[n] = "#" + i)` (mutates a copy from `QT`, never the cached frontmatter object) | **CONFIRMED** |
| Only top-level nodes become `sections` | `worker/own/0169-expressionstatement.js:27` | **CONFIRMED** (already recorded) |
| Block-id charset is `[a-zA-Z0-9-]` | `app/own/1083-expressionstatement.js:398,417`; `worker/own/0157-expressionstatement.js:398,417` | **CONFIRMED** — but there are **two** regexes, and the prior addendum cites only the first |

## Why noticed now

Cluster Q (tags pane) is live and cites `parsing.md` line 337 and §`parseFrontMatterTags` as its
normative spec for tag extraction. Reading the actual grammar showed that spec describes neither the
body-tag grammar (there is none in the doc) nor the subtag mechanism (which is a search-time
regex, not lexical), and that the two Obsidian tag lists disagree on both order and de-duplication —
the kind of difference a tag-count badge exposes on the first note that has the same tag in
frontmatter and in the body.

## Action taken

No Corbomite code touched. Recorded as the normative tag/block-id contract:

- Lexical tag rule: `#` followed by one or more characters outside
  `[U+2000–U+206F] [U+2E00–U+2E7F] '!"#$%&()*+,.:;<=>?@^\`{|}~\/\[\]\\\s]`, terminated by the first
  excluded character; preceded by whitespace or at the start of the inline run; rejected outright if
  the whole match is `#` + digits only.
- Frontmatter tags use a *separate*, weaker rule: trim, drop blanks, drop values containing U+0020,
  prepend `#`. Do not apply the body grammar to frontmatter values.
- `/` cannot appear in a lexical tag; subtag matching is the search-time `^(regex)($|\/)` regex.
- Two tag lists exist with different orderings; pick one deliberately and note which Obsidian
  surface it mirrors.
- Block ids: implement both the whole-line and the paragraph-anchored rule, then attach the id to
  the preceding sibling node (dropping it at index 0), and do not search nested lists for an
  item's id.

Cluster Q's §8 audit-reference list should cite this addendum alongside `parsing.md`, and its
subtag-decomposition task should be specified against the matcher regex rather than against a
lexical rule.
