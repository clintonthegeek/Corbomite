# The persisted CachedMetadata shape is flat 6-number position arrays, carries a `v` schema field, and is re-migrated on every launch that follows a re-parse

**Date:** 2026-10-03
**Discovered during:** `metadata.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/metadata.md` §2 ("Normalisation for persistence" — the
`vT`/`mT`/`gT` sentence), §3 (the `metadata` object-store value description and the durability note)
**Relevant cluster plans:** none yet

## Finding

`metadata.md` §2 says only that `saveMetaCache` "renames `frontmatterPosition → frontmatterPos` via
`vT(...)` and recursively prunes uninteresting position objects via `mT(cache, gT)`". The actual
persist/hydrate pair is four functions plus a JSON round-trip, and the on-disk shape is not the
in-memory shape at all.

### 1. Positions are flattened to `[sl, sc, so, el, ec, eo]`

```js
// app/own/1105-pD.js:2-6   — Position  →  flat array   (persist)
function pD(e) {
  var t = e.start,
    n = e.end;
  return [t.line, t.col, t.offset, n.line, n.col, n.offset];
}
```

```js
// app/own/1107-fD.js:2-13  — flat array  →  Position   (hydrate)
function fD(e) {
  return {
    start: { line: e[0], col: e[1], offset: e[2] },
    end: { line: e[3], col: e[4], offset: e[5] }
  };
}
```

```js
// app/own/1106-dD.js:2-5   — per-ref `position`  →  per-ref `pos`
function dD(e) {
  var t = e.position;
  t && (delete e.position, e.pos = pD(t));
}
```

```js
// app/own/1108-vD.js:2-4   — the inverse, guarded on length
function vD(e) {
  var t = e.pos;
  t && Array.isArray(t) && 6 === t.length && (delete e.pos, e.position = fD(t));
}
```

So on disk: `frontmatterPos` is a **6-element array**, and every entry of `links`/`embeds`/`tags`/
`headings`/`sections`/`listItems`/`blocks` carries **`pos`**, not `position`.

### 2. The walker skips `footnotes`, `footnoteRefs` and `frontmatterLinks`

`hD(e, t)` visits exactly seven collections — `links`, `embeds`, `tags`, `headings`, `sections`,
`listItems`, `blocks` (`app/own/1104-hD.js:2-31`) — and returns. Consequently **footnote and
reference-link records keep their nested `{start:{…},end:{…}}` form on disk**, while everything else
is flattened. A reader that assumes one uniform persisted position shape will mis-parse footnote rows.

### 3. There is a `v` schema field, and `saveMetaCache` never writes it

```js
// app/own/1306-eR.js:529-536
t.prototype.saveMetaCache = function(e, t) {
    if (t) {
      if (this.metadataCache[e] = t, !this.transactionSave) return;
      var n = rf(t);
      return (i = n).frontmatterPosition && (i.frontmatterPos = pD(i.frontmatterPosition), delete i.frontmatterPosition), hD(i, dD), this.transactionSave("metadata", e, n);
    }
    var i;
    if (delete this.metadataCache[e], this.transactionSave) return this.transactionSave("metadata", e, null);
},
```

```js
// app/own/0569-rf.js:2-4
var rf = function(e) {
  return JSON.parse(JSON.stringify(e));
};
```

No `v` is written here. The migrator is `gD`, and it both migrates and *reports* whether it did
anything, by comparing against a `v` it stamps itself:

```js
// app/own/1110-gD.js:2-7
function gD(e) {
  var t = e.v || 0;
  return t < 1 && (e.frontmatter && Array.isArray(e.frontmatter) && (e.frontmatter = null, delete e.frontmatterPos, delete e.frontmatterLinks),
    e.frontmatter && (!e.frontmatterPos && e.frontmatter.pos && (e.frontmatterPos = e.frontmatter.pos, delete e.frontmatter.pos),
      e.frontmatterLinks || (e.frontmatterLinks = cD(e.frontmatter))),
    CD(e, function(e) { e.link = GE(e.link); }), e.v = 1), e.v !== t;
}
```

and `_preload` acts on that return value by **writing the row back**:

```js
// app/own/1306-eR.js:262-270
return [4, tR(a.objectStore("metadata"), 300, function(e, t) {
  for (var r = 0; r < e.length; r++) {
    var o = e[r], a = t[r];
    o && a && (gD(a) && i.transaction("metadata", "readwrite", { durability: "relaxed" }).store.put(rf(a), o), n[o] = mD(a));
  }
})];
```

Because `saveMetaCache` never stamps `v`, a row that 1.13.7 wrote itself comes back with
`e.v === undefined`, so `t = e.v || 0` is `0`, `t < 1` fires, `e.v = 1` is assigned, and
`e.v !== t` returns `true` — **every such row is re-`put` on the next launch**, opening a
`readwrite` transaction per batch. The rewrite is only avoided for rows already stamped `v: 1`,
which after the first hydrate happens only until the file is re-parsed and `saveMetaCache` strips
the stamp again.

The `v < 1` migration itself is four steps, all worth knowing because they describe historical
on-disk shapes: an **array** `frontmatter` is discarded wholesale (`frontmatter = null`,
`frontmatterPos`/`frontmatterLinks` deleted); a `frontmatter.pos` key is renamed up to
`frontmatterPos`; missing `frontmatterLinks` are synthesised from the frontmatter tree by `cD`; and
**every** `link` on `frontmatterLinks`/`links`/`embeds` is rewritten through
`GE` = `e.replace(/\u00A0/g, " ").trim().normalize("NFC")` (`app/own/0933-GE.js:2-4`,
`app/own/0931-_E.js:2`).

### 4. Hydration mutates the row, then re-reads it into memory

`mD` (`app/own/1109-mD.js:2-4`) applies `fD` to `frontmatterPos` and `hD(e, vD)` to expand `pos` back
to `position` — on the **same object** `gD` just mutated, before `rf(a)` snapshots it for the
re-write. In-memory is therefore always the nested form, and `getCache()` never exposes `pos` or
`frontmatterPos`.

### 5. Writes are coalesced, and the coalescing window is a microtask

`transactionSave` is a closure over the open db, shared by both stores:

```js
// app/own/1306-eR.js:237-253
i = r.db = d.sent(), this.transactionSave = function(e, t) {
  var n = null;
  return function(i, r, o) {
    if (n) try {
      var a = n.objectStore(i);
      return o ? a.put(o, r) : a.delete(r);
    } catch (e) {}
    var s = n = e.transaction(t, "readwrite", { durability: "relaxed" });
    Sl(function() { n === s && (n = null); });
    var l = n.objectStore(i);
    return o ? l.put(o, r) : l.delete(r);
  };
}(i, ["file", "metadata"]);
```

with `Sl = queueMicrotask` (`app/own/0356-Sl.js:2`). So the reuse window is **one microtask**, not
"the same tick" as §3 says, and the transaction spans `["file", "metadata"]` regardless of which
store the call targeted — a `saveFileCache` can join a metadata write's transaction and vice versa.
Note the silent `catch (e) {}` around the reuse attempt: if the previous transaction has already
closed, the write is dropped on the floor and **not** retried.

### 6. §3's durability note is half right

§3: *"transactions are opened with `{ durability: "relaxed" }`"*. True for `transactionSave`
(`app/own/1306-eR.js:245`) and for the migration re-write (`:267`). But the **hydrate** reads use a
plain `"readonly"` transaction with no durability option (`app/own/1306-eR.js:257`), and `clear()`
takes neither (`app/own/1306-eR.js:313-316` — `e.clear("metadata")` then `e.clear("file")`).

### 7. The `file` store is loaded unbounded; only `metadata` is chunked

```js
// app/own/1306-eR.js:257-259
[4, (a = i.transaction(["file", "metadata"], "readonly")).objectStore("file").getAllKeys()];
case 5:  return [4, a.objectStore("file").getAll()];
```

Two full `getAll` passes over the `file` store, zipped by **index** (`app/own/1306-eR.js:261`:
`u = s[c], h = l[c], u && h && (t[u] = h)`), so a falsy key or value silently drops the row. The
`metadata` store alone goes through the 300-row chunker `tR`
(`app/own/1307-tR.js:2-12`, `store.getAll(IDBKeyRange.lowerBound(o), t)` in a loop until a short
batch), matching §3's "chunked cursor walker" gloss.

### 8. CONFIRMED — IndexedDB facts that survive unchanged

| §3 claim | Live location | Verdict |
|---|---|---|
| DB name is `app.appId + "-cache"` | `app/own/1306-eR.js:231` | **CONFIRMED** |
| Version is **19** | `app/own/1306-eR.js:231` | **CONFIRMED** |
| Stores are exactly `file` and `metadata` | `app/own/1306-eR.js:233` | **CONFIRMED** |
| Version bump **drops and recreates** both stores — no migration | `app/own/1306-eR.js:232-234` — `e.objectStoreNames.contains("file") && e.deleteObjectStore("file"), e.objectStoreNames.contains("metadata") && e.deleteObjectStore("metadata"), e.createObjectStore("file"), e.createObjectStore("metadata")` | **CONFIRMED, verbatim** |
| `file` is keyed by relative path, `metadata` by content hash | `app/own/1306-eR.js:528` / `:533` — `transactionSave("file", e, t)` / `transactionSave("metadata", e, n)` | **CONFIRMED** |
| Failure to open logs `"Failed to load cache, unable to open IndexedDB"` and continues unpersisted | `app/own/1306-eR.js:255` | **CONFIRMED** (a *hydrate* failure logs the shorter `"Failed to load cache"`, `app/own/1306-eR.js:274`) |
| `hash` is 64-char lowercase hex SHA-256 | `app/own/0905-FC.js:5` — `DC.digest("SHA-256", new Uint8Array(e))`, hexified by `app/own/0543-Pd.js:2-7` | **CONFIRMED** |
| No vault-disk contract for the metadata cache | no filesystem path appears in `app/own/1306-eR.js`; the only I/O is `vault.readBinary` (`:473`) | **CONFIRMED** |
| Only four `appId`-keyed IndexedDB databases exist, so stores cannot be confused | `<appId>-cache` v19 (`file`,`metadata`) `app/own/1306-eR.js:231`; `<appId>-webview` v1 (`icons`, `history`) `app/own/1939-m4.js:781-786`; `<appId>-sync` v1 (`data`) `app/own/2044-sne.js:756-759`; `<appId>-backup` v1 (`backups`, indexes `path`/`ts`) `app/own/2036-I9.js:295-303` | **CONFIRMED** — plus a **new** fourth database (§13 Q9's "how many real version bumps" is unanswerable, but the sibling DB list is now closed) |

## Why noticed now

`metadata.md` §2's one-sentence normalisation note names four short symbols (`vT`, `wT`, `mT`, `gT`)
whose 1.12.7 citations are dead, and §13 Q2 explicitly says *"Without seeing `gT` we can't replicate
the on-disk shape byte-compatibly, but that's moot for us (SQLite)."* All four are visible in
1.13.7 (`rf`/`pD`/`hD`/`dD` persist, `mD`/`fD`/`vD` hydrate, `gD` migrates), so the "moot for us"
hedge no longer holds — and the `v`-stamp asymmetry plus the microtask coalescing window are the
kind of thing that decides whether a cache survives an unclean shutdown.

## Action taken

No Corbomite code touched: `SQLiteIndex` uses SQLite and `metadata.md` §11 already plans a
destructive `PRAGMA user_version` migration, so none of this needs mirroring. What changes is the
§14 item 2 proposal ("Promote the `CachedMetadata` + … shapes to `VAULT-FORMAT.md` … must reproduce
them byte-for-byte"): that promotion should now specify the **flat-array persisted form, the
unflattened footnote exception, and the `v: 1` stamp**, or state explicitly that the on-disk cache
form is not a compat contract (which is the correct call for a desktop app with its own store).
