# `getLinkpathDest` corrections: the `.md` retry is unconditional, there is **no** alphabetical tiebreak, and rooted extensionless links can never resolve

**Date:** 2026-10-03
**Discovered during:** `metadata.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/metadata.md` §8 ("Link resolution algorithm (`getLinkpathDest`)",
steps 2, 3, 5, 6 and the closing "Upshot"), §15 (`VL`, `BL` rows)
**Relevant cluster plans:** none yet — but `metadata.md` §11 calls this "**Highest-priority
correctness gap**" and §14 item 1 proposes promoting §8 to `VAULT-FORMAT.md`, so this addendum
should be read *before* that promotion happens.

## Finding

The whole algorithm is 28 lines (`app/own/1306-eR.js:91-118`). Quoted in full because four of
§8's six steps mis-describe it:

```js
// app/own/1306-eR.js:91-118
t.prototype.getLinkpathDest = function(e, t) {
  if ("" === e && t && (f = this.vault.getAbstractFileByPath(t)) instanceof GD) return [f];
  var n = e.toLowerCase(),
    i = Al(n),
    r = null;
  if (i.contains(".") && (r = this.uniqueFileLookup.get(i)), r || (i = Al(n = (e + ".md").toLowerCase()), r = this.uniqueFileLookup.get(i)), !r) return [];
  if (i === n && 1 === r.length) return r.slice();
  var o = Pl(t).toLowerCase();
  if (n.startsWith("./") || n.startsWith("../")) {
    if (n.startsWith("./../") && (n = n.substr(2)), n.startsWith("./")) "" !== o && (o += "/"), n = o + n.substring(2);
    else {
      for (; n.startsWith("../");) n = n.substr(3), o = Pl(o);
      "" !== o && (o += "/"), n = o + n;
    }
    for (var a = 0, s = r; a < s.length; a++) {
      if ((v = (f = s[a]).path.toLowerCase()) === n) return [f];
    }
  }
  n.startsWith("/") && (n = n.substr(1));
  for (var l = 0, c = r; l < c.length; l++) {
    if ((v = (f = c[l]).path.toLowerCase()) === n) return [f];
  }
  if (e.startsWith("/")) return [];
  for (var u = [], h = [], p = 0, d = r; p < d.length; p++) {
    var f, v;
    (v = (f = d[p]).path.toLowerCase()).endsWith(n) && (v.startsWith(o) ? u.push(f) : h.push(f));
  }
  return u.sort(GF), h.sort(GF), u.concat(h);
},
```

### CORRECTION 1 — step 2: the `.md` retry is **unconditional**, not an `else`

§8 step 2 reads *"If it contains `.`, look up `uniqueFileLookup[name]`; **else** retry with
`name + ".md"` lowercased."* The source is a short-circuit `||`, not an `if/else`: the retry runs
whenever the first lookup produced nothing, dot or no dot. So `[[image.png]]` (no such file) also
tries `image.png.md`, and `[[a.b]]` tries `a.b.md`. The doc's `else` reading would skip the retry
for any dotted linktext — the opposite of what happens.

### CORRECTION 2 — step 6 / the "Upshot": the sort comparator is **path length only**

§8 step 6 says candidates are sorted "by `VL` (shortest-path, **alpha tiebreak**)", and the Upshot
restates "shortest-path-wins, with same-folder preference". `VL`'s 1.13.7 counterpart has no
tiebreak at all:

```js
// app/own/1300-GF.js:2-4
var GF = function(e, t) {
    return e.path.length - t.path.length;
  },
```

`Array.prototype.sort` is stable, so equal-length candidates keep the order they were inserted into
`uniqueFileLookup` — which `initialize()` fills from `vault.getAllLoadedFiles()` in listing order
(`app/own/1306-eR.js:298`), later extended by `computeFileMetadataAsync` (`:445`) and `onCreate`
(`:441` has no `add`; `:445` does). **Ambiguity is therefore broken by vault-listing order, not
alphabetically**, and Corbomite's port must reproduce that ordering to break ties identically.

### CORRECTION 3 — step 6: the same-folder bias is a **raw string prefix**, with no `/`

§8 step 6 says `sameFolder` is "candidates that start with source's lowercased folder prefix + `/`".
The test is `v.startsWith(o)` where `o = Pl(t).toLowerCase()` is the source's **dirname**, with no
separator appended (`app/own/1306-eR.js:98`, `:116`). A source at `N/foo.md` therefore treats
`NN/notes.md`, `N2/notes.md` and `Notes2/notes.md` as "same folder" as well as `N/notes.md`. Any
prefix of the folder path counts.

### CORRECTION 4 — step 3: the single-candidate shortcut tests "no `/` in the linktext"

§8 step 3 says "*Exactly one candidate* and the linktext matched with its literal extension →
return it alone." The actual predicate is `i === n && 1 === r.length` where `i` is the **basename**
of `n` and `n` is the current lowercased linktext (already rewritten to `linktext + ".md"` if the
first lookup missed). `i === n` is therefore true exactly when the linktext contains no `/`. A
single-candidate `[[folder/Note]]` skips the shortcut and falls through to the suffix branches.

### NEW — step 5: rooted links **without** an extension can never resolve

Because the `.md` retry (Correction 1) rewrites `n` *before* the rooted branch runs,
`[[/Note]]` becomes `n = "/note.md"`, then `n.substr(1)` → `"note.md"`, which is then compared for
equality against **full** lowercased paths (`app/own/1306-eR.js:110-112`). It matches only a file
literally at the vault root named `note.md`. Otherwise `if (e.startsWith("/")) return []`
(`:113`). So `[[/Note]]` is dead while `[[/Note.md]]` and `[[/folder/Note.md]]` work. Nothing in
`metadata.md` records this, and it is exactly the kind of edge a resolver port gets wrong by
stripping the leading slash first.

### CONFIRMED — the rest of the algorithm

| §8 claim | Live location | Verdict |
|---|---|---|
| Step 1: empty linktext + source path → `[getAbstractFileByPath(sourcePath)]` if it is a `TFile`, else `[]` | `app/own/1306-eR.js:92` | **CONFIRMED** (falls through to `[]` via `:96` when the path is a folder or missing) |
| Step 4: `./` and `../` are resolved against the source's dirname and compared for exact (case-folded) path equality | `app/own/1306-eR.js:99-108`; `./../x` is first rewritten to `../x` (`:100`), `./x` gets `o += "/"` (`:100`), `../` climbs with `Pl` (`:102`) | **CONFIRMED** — and note `./../` is normalised by a `substr(2)`, i.e. the `./` prefix is simply dropped |
| Step 5: a leading `/` suppresses short-name disambiguation | `app/own/1306-eR.js:113` — `if (e.startsWith("/")) return [];` (tested on the **original** `e`) | **CONFIRMED** |
| Step 6: partition by `endsWith(linktext)` into same-folder / other, concatenate | `app/own/1306-eR.js:114-118` | **CONFIRMED** apart from Corrections 2–3 |
| `getFirstLinkpathDest` = first element or `null` | `app/own/1306-eR.js:88-90` | **CONFIRMED** |
| `uniqueFileLookup` is a multi-map keyed by **lowercased filename including extension** | populated at `app/own/1306-eR.js:298` and `:445` — `this.uniqueFileLookup.add(s.name.toLowerCase(), s)`; the container `_d` is a de-duplicating multimap (`app/own/0556-_d.js:6-9`, `i.contains(t) || i.push(t)`) | **CONFIRMED** |
| §2 `LinkSuggestion`: `.md` is stripped for the suggestion path | `app/own/1306-eR.js:16` — `"md" === o.extension && (a = Ol(a))`, with `Ol` = strip-after-last-dot-but-not-dotfile (`app/own/0364-Ol.js:2-5`) | **CONFIRMED** |
| §2: unresolved suggestion paths are truncated to 500 chars | `app/own/1306-eR.js:38` — `f.length > 500 && (f = f.substring(0, 500))` | **CONFIRMED** |
| §8: `fileToLinktext` "shortest" tries the basename and widens to the full path when the basename does not resolve uniquely to this file | `app/own/1306-eR.js:214-217` | **CONFIRMED** |
| §3: `newLinkFormat` is re-read on every `fileToLinktext` call | `app/own/1306-eR.js:206` — `this.vault.getConfig("newLinkFormat")` | **CONFIRMED** |

### NEW — `fileToLinktext`'s `relative` branch walks up while the target is not already prefixed

```js
// app/own/1306-eR.js:209-212
if ("relative" === i) {
  for (var o = "", a = Pl(t); "" !== a && "/" !== a && 0 !== r.indexOf(a + "/");) o = "../", a = Pl(a);
  return 0 === r.indexOf(a + "/") ? o + r.substr(a.length + 1) : o + r;
}
```

The loop climbs parent directories of the source (`Pl`) for as long as the target does **not** start
with `dirname + "/"`, prefixing one `"../"` per level, then emits either the path relative to the
final ancestor or the ancestor-prefixed absolute. Note `r` here is already the `.md`-stripped path
for markdown files when the third argument is true (`app/own/1306-eR.js:207`), so relative links
are emitted without the extension.

## Why noticed now

`metadata.md` §8's algorithm was reconstructed from a 1.12.7 tree that no longer exists and is
flagged by the doc itself as needing promotion to `VAULT-FORMAT.md` (§14 item 1). §11 rates it the
highest-priority correctness gap, and `SQLiteIndex::resolveWikilink` is a flat lowercase-name
lookup. Reading the 28 lines end to end against 1.13.7 turned up four behavioural differences, three
of which change *which file* a link resolves to — i.e. they are not cosmetic.

## Action taken

No Corbomite code touched. This addendum is a prerequisite for the §14 promotion: the `VAULT-FORMAT.md`
text must be written from `app/own/1306-eR.js:91-118`, not from `metadata.md` §8, or Corbomite will
ship a resolver that ties-break alphabetically, skips the `.md` retry for dotted linktexts, applies a
separator-aware folder bias, and resolves `[[/Note]]`.
