# `blockCache` is a display-text index rebuilt by a *re-parse*, not a persisted `^block-id` resolver; `linkUpdaters` is keyed by bare extension; `isSupportedFile` gates only suggestions and link rewriting

**Date:** 2026-10-03
**Discovered during:** `metadata.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/metadata.md` §3 (the `showUnsupportedFiles` bullet), §7 (all three
subsections), §10 (`linkUpdaters` row), §11 (`blockCache` row); answers §13 Q3 and Q7
**Relevant cluster plans:** none yet

## Finding

### CORRECTION 1 — `MetadataCache.isSupportedFile` does **not** gate indexing

§3 says: *"`"showUnsupportedFiles"` — `boolean`. If true, every extension is indexed; otherwise
`viewRegistry.isExtensionRegistered(ext)` gates inclusion."* Nothing gates indexing.
`computeFileMetadataAsync` branches on the literal extension, not on support:

```js
// app/own/1306-eR.js:449, 504-508
if ("md" === e.extension) { … } else this.saveFileCache(i, { mtime: n.mtime, size: n.size, hash: "" });
```

and `initialize()` feeds it every `TFile` from `vault.getAllLoadedFiles()` with no filter
(`app/own/1306-eR.js:298`), as do `onCreate` (`:441`) and `vault.on("modify")` (`:220`). **Non-markdown
files are always in `fileCache` with `hash: ""` and always in `uniqueFileLookup`, whatever
`showUnsupportedFiles` says** — and because `getCache` returns `{}` (truthy) for them, they also get
`resolvedLinks`/`unresolvedLinks` entries once their `linkUpdaters` handler resolves refs.

`isSupportedFile` has exactly **two** call sites in the whole app bundle, and neither is indexing:

```js
// app/own/1306-eR.js:593-596
t.prototype.isSupportedFile = function(e) {
    var t = this.app;
    return !!t.vault.getConfig("showUnsupportedFiles") || t.viewRegistry.isExtensionRegistered(e.extension);
},
```

1. `app/own/1306-eR.js:14` — the **link-suggestion** filter inside `getLinkSuggestions`.
2. `app/own/1176-PP.js:16` — the **link-rewriting** filter inside the rename/move link-fixup pass
   (`if (e.metadataCache.isUserIgnored(c.path)) return [3,9]; if (!e.metadataCache.isSupportedFile(c)) return [3,9];`).

So the observable meaning is: `showUnsupportedFiles` controls whether canvas/base/image files appear
in the `[[` autocomplete popup and whether rename rewrites links inside them. §11's mapping row for
`linkUpdaters` ("Defer until canvas / bases land") and §9's feature list are unaffected.

### CORRECTION 2 — `linkUpdaters` is keyed by **bare extension**, and its interface has a fourth method

§7/§10/§15 write `linkUpdaters[".canvas"]` / `linkUpdaters[ext]`. The key is `TFile.extension`, which
carries no dot:

```js
// app/own/1961-Q3.js:377
return "md" === e.extension || Object.hasOwn(r.metadataCache.linkUpdaters, e.extension);
```

```js
// app/own/1306-eR.js:171  — dispatch inside updateInternalLinks
(r = i.vault.getAbstractFileByPath(n)) instanceof GD ? (o = r.extension, a = e.get(n),
  Object.hasOwn(t, o) ? [4, t[o].applyUpdates(r, a)] : [3, 2]) : [2, "continue"];
```

```js
// app/own/1306-eR.js:154  — dispatch inside iterateRefsForFile
var n = this.linkUpdaters[e.extension];
```

The canvas plugin registers under the constant `h9 = "canvas"` (`app/own/2028-n9.js:378`) and
**deletes** the entry on unload (`app/own/2028-n9.js:719-721`) — confirming §7's "populated by direct
property assignment" and "removed when that plugin unloads".

§7's `LinkUpdaterPlugin` interface is missing `renameSubpath`, which rename-refactor calls on every
registered updater:

```js
// app/own/1312-hR.js:180-182
m = a.linkUpdaters, g = 0, y = Object.values(m), b.label = 7;
case 7:
  return g < y.length ? (w = y[g], k = this.getCustomReplacements(e), C = k.oldSubpath, E = k.newSubpath,
    [4, w.renameSubpath(this.file, C, E)]) : [3, 10];
```

The real interface, all four methods, from the canvas implementation `f9`
(`app/own/2028-n9.js:455-560`):

| Member | Signature | Canvas behaviour |
|---|---|---|
| `iterateReferences` | `(cb(sourcePath, ref)) => void` | `:459-468` — iterates the canvas index's paths, calls `iterateReferencesForFile(path, e)` |
| `iterateReferencesForFile` | `(path, cb) => void` | `:469-483` — walks each **text** node's `CachedMetadata` via `CD` (frontmatterLinks → links → embeds), then emits a synthetic `{link: file + (subpath ?? ""), original: ""}` per **`file` node** and per **group node with a `background`** |
| `applyUpdates` | `(file: TFile, updates) => Promise<void>` | `:484-515` — maps each update through `refNodeIds.get(update.reference)`, buckets by node id, then `ZF(text, patches)` per text node and re-serialises the JSON |
| `renameSubpath` | `(file, oldSubpath, newSubpath) => Promise<void>` | `:516-560` — scans all canvases for embeds into `file` whose subpath matches (block-ids compared `toLowerCase()`, headings via `TD(...)`), shows `new Aw("Canvas detected file renames affecting " + r.length + " canvas files, updating...")`, and rewrites `node.subpath = "#" + newSubpath` |

**This answers §13 Q3**: the patch element carries a **`reference`** property — it is the ref object
that was yielded by `iterateReferences`, and it is used as the key into the `WeakMap` of ref → node id
(`app/own/2028-n9.js:383`, `:490`, `:495-496`). It is the same ref shape `CD` yields.

### NEW — canvas ref discovery only walks **top-level** nodes

```js
// app/own/2028-n9.js:396, 426-428
if (!(i = n.nodes) || !Array.isArray(i)) return [2, null];
…
c = 0, u = i; … c < u.length ? (h = u[c], [5, s(h)]) : [3, 5]
```

`s(h)` handles `file`, `group` (only when it has a `background`), and `text` nodes — and it never
descends into `children`. So a `file` node nested inside a group inside a group contributes **no**
reference to the link graph. Recorded because `metadata.md` §13 Q7 asks whether non-`.md` sources can
appear in `resolvedLinks`; the answer is yes, but with this blind spot.

### CORRECTION 3 — `blockCache` is a **display-text index over a fresh markdown parse**, and it is never persisted

§7 says `blockCache` is an "Opaque sub-index (`qL(app)`) handling resolution of `^block-id` anchors
across files… Constructed but never exported directly via prototype methods", and §11 says it is
"Required for `![[Note#^abc]]` resolution. Persist `blocks` and `headings` tables with offsets." Both
are wrong. `QF` (`app/own/1302-QF.js:2-90`; the class spans lines 2-104) builds, per markdown file, a list of
`{display, node}` pairs over **block-level nodes**, from a **separate parse of the file's text**:

```js
// app/own/1302-QF.js:14  (getForFile, abridged)
return "md" !== t.extension ? [2, null] : (n = t.stat.mtime,
  (i = this.cache).hasOwnProperty(t.path) && (r = i[t.path]).file === t && r.mtime === n ? [2, r]
    : [4, this.app.vault.cachedRead(t)]);
…
a = WT(o), s = [], …
```

```js
// app/own/1302-QF.js:27-38  (the per-node rules)
if ("yaml" === h.type) return [3, 5];
if ("list" === h.type) return NE(h, "listItem", function(e) { var t = nD(e).trim(); t && s.push({ display: t, node: e }); }), [3, 5];
if ("" === (p = nD(h).trim())) return [3, 5];
"heading" === h.type && (p = Array(h.depth || 1).fill("#").join("") + " " + p), s.push({ display: p, node: h }),
```

```js
// app/own/1302-QF.js:62-66  (the stored record)
f = { file: t, content: o, mtime: n, blocks: s }, i[t.path] = f, [2, f];
```

Points that matter for a port:

- **Not persisted.** `this.cache = {}` is a plain in-memory map (`app/own/1302-QF.js:4`); the only
  IndexedDB write in this class is `clear()` (`app/own/1302-QF.js:6-8`). It is *not* derived from
  `CachedMetadata.blocks`/`headings` either — it is built from `vault.cachedRead` plus a full
  re-parse via `WT` (the same parser `uD` uses).
- **Validity key is `mtime` *and* `TFile` object identity** (`app/own/1302-QF.js:14`) — not `mtime`+`size` like
  `fileCache`, so a same-mtime in-place edit is not detected here.
- **Non-markdown returns `null`**, and a cancelled token returns `null` too (`app/own/1302-QF.js:16`, `:26`).
- `display` for a heading is prefixed with `"#".repeat(depth || 1) + " "`; for a `list` node it is the
  **per-`listItem`** text, so a bullet produces one entry per item rather than one per list.
- `yaml` blocks are skipped; empty-text blocks are skipped.

The only two consumers are the **heading/block jump picker** and a **cursor-block reveal** helper:

```js
// app/own/2020-O7.js:80-82
return [4, this.app.metadataCache.blockCache.getForFile(e, this.file)];
case 1:  return n = i.sent(), [2, UR(e, this.file, this.file.path, n, t)];
```

```js
// app/own/1972-Z5.js:2383-2386
i = null != i ? i : Gd(6), r = t.editor, o = t.file, [4, t.app.metadataCache.blockCache.getForFile(e, o)];
case 1:  for (a = d.sent(), s = 0, l = a.blocks; s < l.length; s++) if (c = l[s],
  (u = c.node.position) && u.start.line - 1 <= n.line && u.end.line - 1 >= n.line) return h = { content: a.content, node: c.node }, …
```

`getAll(token)` (`app/own/1302-QF.js:69-88`) walks `vault.getMarkdownFiles()` sequentially to warm the
cache and returns nothing.

So `^block-id` **resolution** goes through `CachedMetadata.blocks` (populated by the parser, lowercased
keys — see the worker-shape addendum), while `blockCache` is purely a **display-text index for the
jump picker's fuzzy labels and for cursor-context commands**. They are separate mechanisms.

### CONFIRMED — the other §7 registry claims

| Claim | Live location | Verdict |
|---|---|---|
| `linkUpdaters` is in-memory only, not subject to `clear()` | `app/own/1306-eR.js:7` initialises it; `clear()` touches only the db (`app/own/1306-eR.js:307-322`) | **CONFIRMED** |
| `iterateAllRefs(cb)` walks `fileCache` via the ref walker, then every `linkUpdaters[*].iterateReferences` | `app/own/1306-eR.js:149-152` | **CONFIRMED** |
| `iterateAllRefs` **skips** files absent from `fileCache` for the walker but not for the updaters | `app/own/1306-eR.js:141-149` — the inner guard is `if (!Object.hasOwn(n.fileCache, t)) return "continue";`, the outer loop is `for (var i in this.fileCache)` | **CONFIRMED** — with the caveat that a path can appear in the canvas index without a `fileCache` row, and the updater loop is unguarded |
| `iterateRefsForFile` prefers `linkUpdaters[ext]` and falls back to the walker over `getFileCache` | `app/own/1306-eR.js:153-159` | **CONFIRMED** — the fallback passes `getFileCache(e)`, which is `{}` for non-`.md`, so a registered-but-absent updater silently yields nothing |
| `updateInternalLinks` falls back to `vault.process(file, text => ZF(text, patches))` for un-registered extensions | `app/own/1306-eR.js:175-177` | **CONFIRMED** (`ZF` = `app/own/1304-ZF.js:2`) |
| `updateInternalLinks` iterates the **keys** of the supplied map, sequentially | `app/own/1306-eR.js:184-186` | **CONFIRMED** |
| `getBacklinksForFile` returns a `MultiMap` of `source → refs`, built by re-resolving every ref through `getFirstLinkpathDest(bD(ref.link), source)` | `app/own/1306-eR.js:196-203` | **CONFIRMED** — it is an **O(all refs)** scan on every call, and `MultiMap.add` de-duplicates identical ref objects (`app/own/0556-_d.js:6-9`) |
| `getLinks()` returns `Record<src, (LinkCache\|EmbedCache)[]>` with **no** `linkUpdaters` contribution | `app/own/1306-eR.js:119-128` — iterates `fileCache` only and concats `r.links` + `r.embeds` | **CONFIRMED** — so canvas/base links are **absent** from `getLinks()` even when `iterateAllRefs` sees them |
| `getCachedFiles()` is `Object.keys(fileCache)` | `app/own/1306-eR.js:138-140` | **CONFIRMED** |
| `getFileInfo(path)` returns the raw `fileCache` row (or `undefined`) | `app/own/1306-eR.js:136-137` | **CONFIRMED** |
| `getCache(path)` returns `{}` for a non-`.md` path present in `fileCache`, `null` for an absent path or a `.md` whose hash has no row | `app/own/1306-eR.js:131-135` | **CONFIRMED** |
| `getFileCache(file)` is `getCache(file.path)` | `app/own/1306-eR.js:129-130` | **CONFIRMED** |
| `getFileCache` never returns `null` for an existing `.md` file unless the metadata row is missing | `app/own/1306-eR.js:135` — `return this.metadataCache[t] \|\| null;` | **CONFIRMED**, including §8's "parse failed → `null`" |

## Why noticed now

§13 Q3 explicitly parked the `linkUpdaters` protocol as *"inferred, not proven. Need to confirm
against the canvas domain audit"*, and the canvas plugin's updater class turned out to be fully
readable in the app bundle. That single read closes Q3 and Q7 and simultaneously refuted the
`linkUpdaters[".canvas"]` key format three separate places in the doc. Reading `QF` in the same pass
showed that §7's and §11's descriptions of `blockCache` describe a mechanism that does not exist.

## Action taken

No Corbomite code touched. Recorded for the eventual linkUpdaters-equivalent and for
`metadata.md` §11's "Persist `blocks` and `headings` tables with offsets" row — that row is still
correct as a *goal*, but the justification (`blockCache` does `^block-id` resolution) is not; the
resolution path is `CachedMetadata.blocks`, and `blockCache` is an unpersisted display-text index
that Corbomite may legitimately not implement at all unless it wants the jump picker.
