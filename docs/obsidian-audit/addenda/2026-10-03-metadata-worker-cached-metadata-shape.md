# The real `CachedMetadata` shape: an undocumented `referenceLinks` field, `footnoteRefs`/`footnotes` reassignment, `blocks` keyed lowercase, `listItems.parent` is a **line number**, and offsets are UTF-16 code units

**Date:** 2026-10-03
**Discovered during:** `metadata.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/metadata.md` §2 (the whole `CachedMetadata` block and its
"Offsets are 0-based **byte** offsets" comment), §2 `FileCacheEntry` invariants, §2 "Worker message
protocol", §13 Q4 (`frontmatterPosition.start.offset === 0`?), Q5 (footnote shape); re-verifies
§5 of `addenda/2026-06-10-vault-path-and-naming-corrections.md`
**Relevant cluster plans:** none yet — `metadata.md` §11 lists `CachedMetadata` as the biggest
Missing item and §14 item 2 proposes promoting it to `VAULT-FORMAT.md`.

## Finding

The worker bundle **is** in the corpus (`RE/obsidian/1.13.7/renamed/src/worker/`), and the entire
`CachedMetadata` construction is one readable statement: `worker/own/0169-expressionstatement.js:1-127`.
§2's interface block is a reconstruction; here is the source. Every point below is quoted.

### CORRECTION 1 — offsets are UTF-16 **code units**, not bytes

§2's comment reads `// Offsets are 0-based byte offsets; lines/cols 0-based.` The chain is
unambiguous. Bytes arrive, are decoded once, and every offset indexes the resulting JS string:

```js
// worker/own/0169-expressionstatement.js:4-6, 122-124
xr(function(e, t) { … }(new TextDecoder().decode(e)));
```

`yr` subtracts 1 from `line`/`column` but passes `offset` through **untouched**:

```js
// worker/own/0163-yr.js:2-6
function yr(e) {
  return {
    line: e.line - 1,
    col: e.column - 1,
    offset: e.offset
  };
}
```

and the offsets are used to slice that very string to produce the user-visible `original` text:

```js
// worker/own/0169-expressionstatement.js:70-71  (ilink)  …  :81  (iembed)
var f = e.substring(r.position.start.offset, r.position.end.offset), h = r.href || "";
```

A byte offset would make `original` garbage on any non-ASCII file. Two independent in-app consumers
confirm the unit: `app/own/1481-_q.js:391` computes `s.offset - s.col` to get back to a **line
start** (so `col` and `offset` share a unit), and `app/own/1481-_q.js:402` walks lines with
`10 !== e.charCodeAt(p - 1)` (LF detection over a JS string). For pure-ASCII files the two units
coincide, which is why the audit never caught it.

**This re-verifies §5 of the 2026-06-10 addendum unchanged**, and extends it: the async path is
`Md(text) = Ed(new TextEncoder().encode(e))` (`app/own/0537-Md.js:2-4`) → worker →
`new TextDecoder().decode`. In Corbomite, apply cached offsets to the decoded `QString` (also
UTF-16-unit indexed), never to a `QByteArray`.

### CORRECTION 2 — `referenceLinks` exists and is completely undocumented

§2's `CachedMetadata` lists nine fields. There are **ten**:

```js
// worker/own/0169-expressionstatement.js:8-18
var n = {
    links: [], embeds: [], tags: [], headings: [], footnotes: [], footnoteRefs: [],
    referenceLinks: [], sections: [], listItems: [], blocks: {}
  },
```

fed from markdown link-*reference* definitions:

```js
// worker/own/0169-expressionstatement.js:55-62
} else if ("definition" === r.type && r.url) {
  var c = r.label || r.identifier;
  n.referenceLinks.push({ position: vr(r.position), id: c || "", link: r.url || "" });
}
```

So `[text][ref]` / `[ref]: url` produce `{position, id, link}` where `id` is the **label** if present
else the identifier, and `link` is the raw URL. It has **no `position`-flattening on disk** either
(see the persisted-shape addendum) and **no consumer** in the app corpus — it is exposed to plugins
only. `iterateCacheRefs` does not walk it (see Confirmed §below), so it does not affect the link graph.

### CORRECTION 3 — `footnoteRefs` vs `footnotes` is *not* "refs vs definitions"

§2 says `footnoteRefs?: {id, position}[]; // ^1, ^2 refs` and
`footnotes?: {id, position}[]; // [^1]: definitions`, and §13 Q5 says the shape is
*"inferred from general Obsidian plugin lore"*. The actual rule: an `fnRef` node goes into
**`footnotes`** when the footnote's *content* is present, and into **`footnoteRefs`** when it is not:

```js
// worker/own/0169-expressionstatement.js:41-53
if ("fnRef" === r.type) {
  var l = null === (s = r.data) || void 0 === s ? void 0 : s.contentPosition;
  l ? n.footnotes.push({ position: vr(l), id: r.identifier || "" })
    : n.footnoteRefs.push({ position: vr(r.position), id: r.identifier || "" });
} else if ("footnoteDefinition" === r.type) n.footnotes.push({ position: vr(r.position), id: r.identifier || "" });
```

Note `id: r.identifier || ""` — **the empty string is a legal `id`**, so both arrays can hold several
entries with `id: ""`. `footnoteRefs` positions are the *reference mark's* position;
`footnotes` positions are either the *definition's* position or the reference's **content**
position, depending on which branch fired. A port that assumes "`footnotes` = definitions" will
mis-locate footnote bodies for single-line footnotes.

### NEW — `blocks` is lowercased-keyed and fed from **two** sources, not just headings

```js
// worker/own/0169-expressionstatement.js:27-39   (top-level sections with an id)
f = l.id;
f && "string" == typeof f && (u.id = f, n.blocks[f.toLowerCase()] = { position: c, id: f }), n.sections.push(u);
```

```js
// worker/own/0169-expressionstatement.js:117-121  (list items with an id)
(c = i.id) && "string" == typeof c && (x.id = c, n.blocks[c.toLowerCase()] = { position: m, id: c }),
n.listItems.push(x);
```

So: keys are `id.toLowerCase()`, values keep the **original-cased** `id`, and a `^block-id` on a
**list item** or on a top-level section registers a `blocks` entry just as a heading would.
§2's `blocks?: Record<string, {id, position}>` is right in shape and silent on all three points.
The block-id charset is `/^\^([a-zA-Z0-9\-]+)$/` (`worker/own/0157-expressionstatement.js:398`).

### NEW — `listItems.parent` is a **0-based line number** of the parent list item

§2 writes `parent: number` with no unit, which makes the field unusable. The parser sets it to the
parent item's start line:

```js
// worker/own/0169-expressionstatement.js:111-121
if (g && g.length > 1)
  for (var y = g.length - 1; y >= 0; y--) {
    var b = g[y];
    if ("list" !== b.type) { m.end = oD(b.position.end); break; }
    b.listParentPosition = i.position;
  }
var w = 0;
"list" === o.type && (o.listParentPosition ? w = o.listParentPosition.start.line - 1
  : 0 === (w = -(o.position.start.line - 1)) && (w = -1));
var k = { position: m, parent: w }, C = i.checklist;
```

and a consumer compares it directly against other items' `position.start.line`:

```js
// app/own/1481-_q.js:137-144
var a = o, s = u + 1, l = new Set();
for (l.add(a.position.start.line); s < i.length;) {
  var c = i[s];
  if (!l.has(c.parent)) break;
  l.add(c.position.start.line), s++, a = c;
}
```

`parent` is a **line number (0-based)** or `-1`. The fallback branch (`-(parentListPosition.start.line - 1)`,
with `0` remapped to `-1`) is emitted verbatim above; I have not established what case reaches it.

Also in that block, and load-bearing for `[[Note#^abc]]` ranges: **a list item's `position.end` is
extended past nested lists** — children are walked backwards and the first non-`list` child's end
becomes the item's end (`worker/own/0169-expressionstatement.js:102-110`). And `task` is the raw
checklist string, present only when the AST exposes one: `"string" == typeof A && (x.task = A)`
(`:117-118`).

### NEW — empty collections are **deleted**, not returned empty

```js
// worker/own/0169-expressionstatement.js:123
0 === n.links.length && delete n.links, … 0 === n.referenceLinks.length && delete n.referenceLinks,
(0 === n.sections.length || t) && delete n.sections,
0 === n.listItems.length && delete n.listItems,
0 === Object.keys(n.blocks).length && delete n.blocks, n;
```

§2 marks the fields optional (`?`) but never says they are *actively removed*. Consequence:
`Object.keys(cache.blocks)` throws `TypeError` for a file with no block ids, and `cache.sections` is
absent (not `[]`) for an empty document. The second parameter `t` forces `sections` off entirely.

### NEW — there is a **second** worker message, and the parser is duplicated on the main thread

```js
// worker/own/0169-expressionstatement.js:5, 125-126
if (t.hasOwnProperty("metadataCache")) return void … (t.metadataCache);
…
if (t.hasOwnProperty("parseSections")) return n = t.parseSections, r = t.options,
  lr.globalOptions = r || {}, void xr(wr(n));
```

The first branch `return`s, so a message carries exactly one of the two keys. §2's "Worker message
protocol" documents only `{metadataCache: ArrayBuffer}`. Note `parseSections` replies with `wr(...)`,
a *different* serialiser, and `options` is installed into a **module-global** (`lr.globalOptions`,
default `{breaks: true, commonmark: true}` at `worker/own/0154-expressionstatement.js:2-5`) — so
concurrent callers share it.

More importantly, the exact same construction also runs **on the main thread**, statement-for-statement
identical, with renamed helpers:

```js
// app/own/1103-uD.js:1-16, 95-118   (cf. worker/own/0169-expressionstatement.js:7-18, 95-121)
function uD(e, t) { var n = { links: [], embeds: [], …, blocks: {} }, i = WT(e), r = i.children[0]; … }
```

`WT`↦`hr`, `KT`↦`pr`, `aD`↦`vr`, `oD`↦`yr`, `cD`↦`kr`. The split is by size:

```js
// app/own/2028-n9.js:447
return e.length < 5e3 ? [2, uD(e)] : (t = Md(e), [4, this.app.metadataCache.computeMetadataAsync(t)]);
```

(canvas text nodes; `uD` is also called directly at `app/own/1795-_$.js:4`). **Any Corbomite
`NoteMetadata` struct must be produced identically by both paths**, exactly as Obsidian has to.

### ANSWER to §13 Q4 — `frontmatterPosition.start.offset` is always 0, and a BOM breaks frontmatter entirely

§13 Q4 asks whether the frontmatter position always starts at 0 and flags "files with only trailing
whitespace before frontmatter, or files with a **BOM**" as a corner case worth a test. The block rule
is `onlyAtStart`:

```js
// worker/own/0157-expressionstatement.js:384-394
var t = "---",
  n = function(e, n, r) {
    if (n.slice(0, 3) === t && "\n" === n.charAt(3)) {
      for (var i = n.indexOf(t, 3); -1 !== i && "\n" !== n.charAt(i - 1);) i = n.indexOf(t, i + 3);
      return -1 !== i ? !!r || e(n.slice(0, i + 3))({ type: "yaml", value: n.slice(4, i - 1) }) : void 0;
    }
  };
n.onlyAtStart = !0;
```

`onlyAtStart` is the markdown-it flag that restricts a block rule to line 0 — `this.atStart = !0`
in the state constructor (`worker/vendor/1528.js:13`), cleared by `exitStart`
(`worker/vendor/1528.js:24`), and gated at `worker/vendor/7269.js:35`
(`l.onlyAtStart && !f.atStart`). Therefore:

- **`frontmatterPosition.start` is always `{line: 0, col: 0, offset: 0}`.** The question is answered
  affirmatively.
- **A UTF-8 BOM defeats detection.** `\uFEFF` decodes to a non-`-` first character, `onlyAtStart`
  fires at line 0, the rule returns `void 0`, and there is no `yaml` node — so `frontmatter`,
  `frontmatterPosition` **and** `frontmatterLinks` are all absent and the `---` lines are tokenised as
  ordinary content. Same for any leading blank line or whitespace.
- The closing fence must be `---` preceded by a newline (the `"\n" !== n.charAt(i - 1)` guard skips
  mid-line occurrences), and the block value is `n.slice(4, i - 1)` — one line dropped at each end.

### NEW — `frontmatter` is omitted unless it parses to a **plain object**

```js
// worker/own/0158-pr.js:2-10   (main-thread twin: app/own/1086-KT.js:2-10)
function pr(e) {
  var t = e.children[0];
  if (t && "yaml" === t.type) try {
    var n = Vn(t.value, void 0, {});
    if (n && "object" == typeof n && !Array.isArray(n)) return n || void 0;
  } catch (e) { return null; }
}
```

`Vn` is the `yaml` package's `parse` wrapper (`worker/own/0147-Vn.js:2-29`); it **throws** on any
document error unless `logLevel === "silent"`, and the caller swallows that into `return null`. So
malformed YAML, multi-document YAML, frontmatter that parses to a **scalar**, to an **array**, or to
`null` all produce a `CachedMetadata` with **no `frontmatter` key at all**. §2's
`frontmatter?: Record<string, unknown>` does not convey that the field is a lost cause rather than an
empty object. There are **no per-key positions** inside `frontmatter`.

### CONFIRMED — §2 claims that survive, with live locations

| §2 claim | Live location | Verdict |
|---|---|---|
| `links`/`embeds` entries are `{link, original, displayText?, position}` | `worker/own/0169-expressionstatement.js:69-79`, `:80-88` | **CONFIRMED** — `link` is `r.href \|\| ""`; `displayText` is `r.title \|\| ""`, falling back to `gr(r)` when the node is `converted` and untitled (`:72`) |
| `TagCache.tag` includes the leading `#` | `worker/own/0169-expressionstatement.js:89-93` — `{position, tag: r.tag \|\| ""}` | **CONFIRMED** |
| `HeadingCache` is `{heading, level, position}` | `worker/own/0169-expressionstatement.js:62-68` — `{position, heading: p(e, r), level: r.depth \|\| 1}` | **CONFIRMED** — `p` (`worker/own/0005-p.js:2-7`) slices from the first child's start to the last child's end and trims, so `heading` is the **markup-stripped inner text**; `level` is the AST's own `depth`, defaulting to `1`. Headings are only collected when their ancestor is the tree root (`worker/own/0169-expressionstatement.js:62`, `"root" === o.type`) |
| `SectionCache` is `{type, position, id?}` with `type` = the top-level node type | `worker/own/0169-expressionstatement.js:27-39` — `{type: l.type, position: c}` plus `u.id` | **CONFIRMED**, including the `id?` field |
| Only **top-level** children become `sections` | `worker/own/0169-expressionstatement.js:27` — `for (var s = 0, a = r.children; s < a.length; s++)` | **CONFIRMED** (a nested blockquote is not a section; a top-level `blockquote` with a callout marker becomes `type: "callout"`, `worker/own/0157-expressionstatement.js:739-741`) |
| `Footnote`/`footnoteRef` records are `{id, position}` | `worker/own/0169-expressionstatement.js:44-52` | **CONFIRMED** as to shape, refuted as to which array (Correction 3) |
| `frontmatterLinks` entries are `{key, link, original, displayText?}` with dotted / index-interleaved `key` | `worker/own/0166-kr.js:19-24`, `:33-38` (`o = e ? e + "." + n : n`, `n(e, String(r), i)`) | **CONFIRMED**, e.g. `"related.0"` |
| `frontmatterLinks` only captures string leaves that look like `[[…]]` or `[…](…)` | `worker/own/0166-kr.js:16-40` | **CONFIRMED** — and **new**: markdown links are gated by `WE` = `(startsWith("./") \|\| startsWith("../")) \|\| indexOf(":") === -1` (`app/own/0928-WE.js:2-4`), so **absolute URLs (`https://…`, `mailto:…`) are excluded** and only plain relative linkpaths are indexed. `<…>` hrefs are unwrapped and trimmed, then `decodeURI` in a `try`/`catch` (`:31-34`). The app-side twin `cD` (`app/own/1102-cD.js:2-47`) is algorithmically identical. |
| The `metadataCache` message transfers the buffer | `app/own/1306-eR.js:541-543` — `t.worker.postMessage({ metadataCache: e }, [e])` | **CONFIRMED** |
| Only one parse is in flight; `work` throws on overlap | `app/own/1306-eR.js:539` — `if (this.workerResolve) throw new Error("Work queue must be sequential!");` | **CONFIRMED** |
| Concurrency comes from a serial promise chain | `app/own/0821-Pw.js:38-46` — `Iw.queue = function(e) { var t = this.promise.then(e, e); this.promise = t; return t; }` | **CONFIRMED** — note `.then(e, e)` passes the task as *both* handlers, so a rejection does not poison the chain |
| Content is read as **bytes**; YAML parsing is worker-side | `app/own/1306-eR.js:473` (`vault.readBinary`), `worker/own/0158-pr.js:5` | **CONFIRMED** |
| Binary content is read via `readBinary` so the worker gets bytes, not decoded text | `app/own/1306-eR.js:473`, `app/own/0537-Md.js:2-4` | **CONFIRMED** |
| Frontmatter has no position info | `worker/own/0158-pr.js:2-10` returns the bare loader result | **CONFIRMED** |

### CONFIRMED — §2 `FileCacheEntry` invariants

| Claim | Live location | Verdict |
|---|---|---|
| Entry shape is `{mtime, size, hash}` | `app/own/1306-eR.js:456-460`, `:504-508` | **CONFIRMED** |
| A file seen for the first time gets a **placeholder row persisted before parsing** | `app/own/1306-eR.js:456-460` — `r = {mtime: 0, size: 0, hash: ""}, this.saveFileCache(i, r)` | **CONFIRMED** (undocumented) — this is how `fileCache` learns about a file at all |
| Non-`.md` files are stored with `hash: ""` | `app/own/1306-eR.js:504-508` — `} else this.saveFileCache(i, {mtime: n.mtime, size: n.size, hash: ""});` | **CONFIRMED** — and note this branch is keyed on `"md" === e.extension` at `:449`, **not** on `isSupportedFile` |
| The stat tuple is updated **before** the parse completes | `app/own/1306-eR.js:477` — `r.mtime = n.mtime, r.size = n.size, r.hash = a, this.saveFileCache(i, r)` | **CONFIRMED** |
| A hash hit reuses the existing metadata and skips the worker, but still emits `changed` | `app/own/1306-eR.js:477` | **CONFIRMED** |
| Hash dedup is checked as `r.hash && metadataCache.hasOwnProperty(r.hash)` **in addition to** the stat equality | `app/own/1306-eR.js:453-455` | **CONFIRMED** |
| A failed parse logs `"Metadata failed to parse"` and writes no cache entry | `app/own/1306-eR.js:488` — `s ? (this.saveMetaCache(a, s), …) : (console.log("Metadata failed to parse", e), [2])` | **CONFIRMED** |
| A 10-second stall raises a user-visible notice | `app/own/1306-eR.js:478-479` — `setTimeout(function() { new Aw("Indexing taking a long time for " + e.path); }, 1e4)` | **CONFIRMED** (undocumented — a per-file "Indexing taking a long time for &lt;path&gt;" toast) |

## Why noticed now

The prompt's starting point flagged `worker.js` as out of Pass-2 scope, and `metadata.md` §13 Q5
says the footnote shape is *"inferred from general Obsidian plugin lore — not referenced in this
file's code"*. With the worker bundle restored, the whole construction is one readable statement, so
Q5 and Q4 are answered outright and four fields §2 does not model at all (`referenceLinks`, the
`blocks` lowercasing, the `parent` unit, the deleted-empty-arrays rule) turned up in the same read.

## Action taken

No Corbomite code touched — `metadata.md` §11 records `CachedMetadata` as entirely Missing, so
there is nothing to fix, only something to build correctly later. Concretely, when `NoteMetadata`
is introduced it must: (a) treat offsets as UTF-16 code units against the decoded `QString`;
(b) model `referenceLinks`; (c) key `blocks` lowercase with original-cased `id`s and include
list-item and section block-ids; (d) make `listItems.parent` a `qint64` line number defaulting to
`-1`; (e) omit empty collections rather than emitting empties; (f) tolerate frontmatter that
vanishes entirely on BOM, leading blank line, malformed YAML, or non-object YAML; and (g) be produced
identically by Corbomite's sync and async parse paths, as Obsidian's `uD`/`0169` twins are.
