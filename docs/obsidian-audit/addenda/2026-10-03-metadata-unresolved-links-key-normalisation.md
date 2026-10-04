# `unresolvedLinks` keys are `#`-stripped then `.md`-stripped — never lowercased; and re-queueing on create/rename/delete is a **substring** test

**Date:** 2026-10-03
**Discovered during:** `metadata.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/metadata.md` §2 (`UnresolvedLinkMap`), §5 (`watchVaultChanges`
rows), §8 (`unresolvedLinks` key invariant); re-verifies §2 of
`addenda/2026-06-10-vault-path-and-naming-corrections.md`
**Relevant cluster plans:** none yet

## Finding

### 1. The two-stage key pipeline, exactly

§2 says *"The linkpath is passed through `BL(linkpath)` before being used as the key (lowercase-normalise
+ strip heading — see §15)"* and §8 says *"`unresolvedLinks` keys are normalised via
`BL(getLinkpath(linktext))` — lowercased, heading/block-id stripped. Do not expect original casing
to survive."* Both halves of that are wrong. There is **no lowercasing anywhere in the pipeline**,
and there are **two** functions, applied in this order:

```js
// app/own/1306-eR.js:403-411   (resolveLinks)
//   this.iterateRefsForFile(r, function(r) {
//     var o = bD(r.link), a = t.getFirstLinkpathDest(o, e);
//     if (a) n[a.path] = (n[a.path] || 0) + 1;
//     else { var s = KF(o); i[s] = (i[s] || 0) + 1; }
//   }),
//   this.resolvedLinks[e] = n, this.unresolvedLinks[e] = i;
```

```js
// app/own/1112-bD.js:2-5   — "getLinkpath": cut at the FIRST '#'
function bD(e) {
  var t = e.indexOf("#");
  return -1 === t ? e : e.substring(0, t);
}
```

```js
// app/own/1299-KF.js:2-4
function KF(e) {
  return "md" === Fl(Al(e)) && (e = e.substring(0, e.length - 3)), e;
}
```

So the key is: `ref.link` → cut at the first `#` (killing `[[Note#Heading]]`, `[[Note#^abc]]` and
any `#` inside the path) → if the **basename's** extension is `md` (compared case-insensitively,
because `Fl` lowercases), drop the trailing 3 characters. Nothing else. Case, subfolders, and
non-`.md` extensions survive verbatim: `[[Foo Bar]]` → key `"Foo Bar"`; `[[notes.d/Foo]]` →
`"notes.d/Foo"`; `[[a/b.canvas]]` → `"a/b.canvas"`.

The `.md` strip is basename-scoped, which is behaviourally identical to a whole-string strip for
any path whose extension lives in the basename — and correctly does *not* strip from a
dot-directory like `[[v1.2/Note]]` (there `Al` yields `Note`, `Fl` yields `""`, nothing is removed).

### 2. Re-verification of the 2026-06-10 correction — **holds unchanged**

`addenda/2026-06-10-vault-path-and-naming-corrections.md` §2 already corrected `metadata.md` on the
casing point ("`unresolvedLinks` keys are NOT lowercased … `BL` only strips a trailing `.md`
extension"), citing 1.12.7. In 1.13.7 the same two behaviours are present under new symbols:
`BL` → `KF`, `getLinkpath` → `bD`. **`BL` ↦ `KF` and `getLinkpath` ↦ `bD`; nothing else moved.**
The 1.12.7 claim stands; only the citations needed re-anchoring.

### 3. `updateRelatedLinks` matches by **substring containment**, on lowercased basenames

§5 and §8 both say rename/create/delete "re-queue link resolution for files that pointed here"
without saying how "pointed here" is decided. It is a `contains` test, not equality, and the
needle set is built by stripping a trailing `.md` off each changed name:

```js
// app/own/1306-eR.js:426-439
t.prototype.updateRelatedLinks = function(e) {
  for (var t = [], n = 0, i = e = e.map(function(e) { return e.toLowerCase(); }); n < i.length; n++) {
    var r = i[n];
    r.endsWith(".md") && t.push(r.substring(0, r.length - 3)), t.push(r);
  }
  for (var o = this.resolvedLinks, a = this.unresolvedLinks, s = 0, l = this.getCachedFiles(); s < l.length; s++) {
    var c = l[s];
    if (YF(e, o[c]) || YF(t, a[c])) {
      var u = this.vault.getFileByPath(c);
      u && this.queueFileForLinkResolution(u);
    }
  }
},
```

```js
// app/own/1300-GF.js:5-12
YF = function(e, t) {
  if (!t) return !1;
  for (var n = 0, i = Object.keys(t); n < i.length; n++) {
    var r = i[n];
    if (e.contains(Al(r).toLowerCase())) return !0;
  }
  return !1;
};
```

Consequences worth writing down:

- Against `resolvedLinks[src]`, the keys are full destination paths; `Al(r)` takes the basename and
  lowercases it, then `e.contains(...)` is a **substring** test against the *original* (lowercased)
  name list. So renaming `a.md` re-queues every source whose resolved dest basename merely
  *contains* `"a"` — `data.md`, `alpha.md`, `Notes/a.md` all match.
- Against `unresolvedLinks[src]`, keys are linkpaths (possibly with subfolders, since `KF` only
  strips `.md`); `Al(r)` takes the last `/`-segment, so `[[folder/note]]` matches a rename of
  `note.md` even from a different folder.
- Each changed name contributes **both** its bare and its `.md`-stripped form to the needle set
  (`t.push` runs before the unconditional `t.push(r)`), so the `.md` strip only widens matching
  against `unresolvedLinks` keys — which is exactly where `.md`-less keys live.
- `YF` returns `false` for a missing per-source record. A file that has never been link-resolved is
  therefore **never** re-queued by this path, even if it contains a matching linktext.
- The callers pass different granularities, all basenames-with-extension:
  `onCreate` → `[e.name]` (`app/own/1306-eR.js:441`), `deletePath` → `[Al(e)]` (`app/own/1306-eR.js:513`),
  `onRename` → `[Al(oldPath)]` **and** `[Al(newPath)]` if the basename changed, else only the old one
  (`app/own/1306-eR.js:523-525`).

### 4. CONFIRMED — the resolved-link map invariants

| Claim | Live location | Verdict |
|---|---|---|
| `resolvedLinks[src][dst]` is a **count**, incremented once per ref | `app/own/1306-eR.js:406` — `n[a.path] = (n[a.path] || 0) + 1` | **CONFIRMED** |
| `resolvedLinks`/`unresolvedLinks` keys are the **vault-relative paths with extensions** | `app/own/1306-eR.js:406` uses `a.path` (the `TFile`'s own path) | **CONFIRMED** |
| `resolveLinks` only writes when `getCache(path)` is truthy | `app/own/1306-eR.js:399` — `if (this.getCache(e)) {` … | **CONFIRMED**, with a caveat: `getCache` returns `{}` (truthy) for non-`.md`, so canvas/base sources *do* get entries — see `2026-10-03-metadata-blockcache-linkupdaters-and-issupportedfile.md` |
| Both maps are **wholesale replaced** per source, never merged | `app/own/1306-eR.js:400-401` build fresh `n`/`i` locals, assigned at `:411` | **CONFIRMED** |
| `onRename` remaps `oldPath → newPath` immediately in both maps | `app/own/1306-eR.js:522` — `o.hasOwnProperty(t) && (o[n] = o[t], delete o[t]), a.hasOwnProperty(t) && (a[n] = a[t], delete a[t])` | **CONFIRMED, verbatim** |
| `onDelete` removes both map entries | `app/own/1306-eR.js:513` — `delete this.resolvedLinks[e], delete this.unresolvedLinks[e]` | **CONFIRMED** |
| `isUnresolved(linktext, source)` is `initialized && !getFirstLinkpathDest(...)` | `app/own/1306-eR.js:86-87` — `return !!this.initialized && !this.getFirstLinkpathDest(e, t);` | **CONFIRMED** (the `initialized` gate is undocumented in the doc) |

## Why noticed now

`metadata.md` §2/§8 both name `BL` and describe a lowercase step that the source does not contain.
The 2026-06-10 addendum had already refuted the lowercase half against 1.12.7, but its citations
were dead with the old corpus, so the domain doc's version was still the one an implementer would
read. Re-anchoring to 1.13.7 gave the exact two-function pipeline, and reading `resolveLinks` +
`updateRelatedLinks` side by side surfaced the `contains` matching, which nothing in the audit
records.

## Action taken

No Corbomite code touched (no `unresolvedLinks` equivalent exists yet — `metadata.md` §11 lists it
as Missing). Recorded so the eventual port: (a) never lowercase `unresolvedLinks` keys, (b) cut at
the first `#` *before* resolving, (c) strip `.md` only when the basename's extension is `md`,
(d) implement create/rename/delete invalidation as a lowercased-basename **substring** test, or
accept extra re-queues (a superset is safe; a stricter test is not).
