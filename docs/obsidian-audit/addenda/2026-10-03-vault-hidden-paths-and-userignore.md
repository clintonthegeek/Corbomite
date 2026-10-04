# Hidden-path predicate: dot-segments only, no `configDir` special case — and `userIgnoreFilters` are regexes, not globs

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/vault.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`).
**Supersedes / extends:** `domains/vault.md` §3 (*Ignored names*), §8 (last bullet), §13 Q4; `VAULT-FORMAT.md` §1.
**Relevant cluster plans:** `libs/storage/VaultScanner`, `libs/core/NoteMeta`, any plan implementing `Corbomite::Vault`'s file tree or a `MetadataCache` analogue.

## Finding

### CORRECTION (HIGH — vault format): the hidden-path predicate has *no* `configDir` parameterisation, and the audit states the rule backwards

`domains/vault.md` §3 says: "**Ignored names:** `ru(path)` … filters reserved names: everything under `configDir` is excluded from the public `fileMap`, and most dotfiles are hidden **except under `configDir`**."

The second clause is inverted, and the first is an accident of the naming rather than a rule. The predicate is entirely generic:

```js
// app/own/0362-Il.js:2-9
function Il(e) {
  for (; e;) {
    if (Al(e).startsWith(".")) return !0;
    e = Pl(e);
  }
  return !1;
}
```

(`Al` = basename, `app/own/0360-Al.js:2`; `Pl` = dirname, `app/own/0361-Pl.js:2`.) It walks **every ancestor segment of the vault-relative path** and returns true as soon as one starts with `.`. There is no `configDir` argument, no reserved-name list, no exception for anything. `.obsidian/` is hidden for exactly the same reason `.git/` and `.trash/` are: its own name starts with a dot.

Call sites — every one of them routes a dot-path to `reconcileDeletion` instead of `reconcileFileInternal`, so a dot-path **never enters `adapter.files`** and therefore never enters `Vault.fileMap`:

```js
// app/own/0376-_l.js:63  (listRecursiveChild — initial scan)
// Il(i) ? [4, this.reconcileDeletion(n, i)] : [3, 2]; → reconcileFileInternal(n, i)
// app/own/0376-_l.js:685 (reconcileFile — watcher-driven)
this.trigger("raw", t), Il(t) ? [4, this.reconcileDeletion(e, t, n)] : [3, 2];
```

So **audit open question 4 is answered: the predicate does *not* parameterise on `configDir`**, and the consequences are stronger than the audit implies:

- Any user dotfolder (`.johns-notes/`, `.obs/`) is invisible in the file explorer, invisible to `getFiles()`/`getMarkdownFiles()`/`recurseChildren`, invisible to the graph, and excluded from link resolution — not just `.obsidian/` and `.git/`.
- `.trash/` is hidden by the same rule, which is why the trash needs no separate exclusion anywhere.
- `.obsidian/` contents are *not* special-cased at the fileMap layer at all; they are excluded purely by the leading dot. The `Vault.onChange` raw handler's `t.startsWith(this.configDir + "/")` test (`app/own/1129-UD.js:275`) exists only to re-install hidden watchers and to trigger `reloadConfig` — it plays no part in deciding what is visible.

### CORRECTION (HIGH — vault settings format): `userIgnoreFilters` entries are **regular expressions**, not glob patterns

`domains/vault.md` §2 types them as `userIgnoreFilters?: string[]; // glob patterns`.

```js
// app/own/1306-eR.js:588
o.length > 2 && o.startsWith("/") && o.endsWith("/")
  ? n.push(new RegExp(o.substring(1, o.length - 1), "i"))   // "/re/"  → unanchored regex
  : n.push(new RegExp("^" + jd(o), "i"))                     // bare   → escaped prefix match
```

- Each entry is **trimmed**; empty entries are skipped (`:586-587`).
- `/…/` (length > 2, leading and trailing slash) → the inner text is used as a **raw, unanchored, case-insensitive** regex.
- Anything else → regex-escaped (`jd`, `app/own/0554-jd.js:2`, escaping set `/[.?*+^$[\]\\(){}|-]/g` at `app/own/0553-Wd.js:2`) and anchored with `^`, i.e. a **case-insensitive literal prefix match** on the path.
- A malformed regex is skipped with `console.error("Bad regex for user ignore filter", e)` (`:589-590`).
- Matching target is the **full vault-relative path**, memoised per path in `userIgnoreFilterCache` (`isUserIgnored`, `app/own/1306-eR.js:596-604`).
- Recompiled only when `JSON.stringify(getConfig("userIgnoreFilters"))` changes, and only in response to a `config-changed` for that exact key (`onConfigChanged`, `app/own/1306-eR.js:578-579`).

So a filter entry `Archive*` means the literal prefix `Archive*` (the `*` is escaped), and `/^Archive/` is required for glob-ish intent. A Corbomite UI presenting these as globs would produce filters Obsidian silently misreads.

### NEW: `userIgnoreFilters` does **not** affect the file tree, the link graph, or metadata indexing

The natural assumption is that an "ignore" filter hides the file. It does not. `computeFileMetadataAsync` (`app/own/1306-eR.js:442`) has no `isUserIgnored` check — ignored `.md` files are still parsed, still enter `resolvedLinks`/`unresolvedLinks`, and still participate in rename-with-refactor. The gates that *do* consult it:

- search results are dropped (`app/own/1951-e3.js:77`)
- graph-view nodes are dropped (`app/own/1851-h0.js:162`)
- `FileManager.deleteProperty` / `renameProperty` skip the file (`app/own/1312-hR.js:509`, `:538`)

File-tree visibility is a *different* mechanism entirely: `isSupportedFile` = `showUnsupportedFiles || viewRegistry.isExtensionRegistered(ext)` (`app/own/1306-eR.js:593-595`), which keeps unsupported files in `fileMap` but hides them from listings.

## Why noticed now

Audit open question 4 explicitly asked whether `ru(path)` parameterises on `configDir`, and phrased the §3 invariant with the polarity reversed — the kind of error that survives because "dotfiles are hidden and `.obsidian` is hidden" is observationally indistinguishable from "dotfiles are hidden *except* under `configDir`" for anyone testing with the default config dir name. The 1.13.7 restore made the predicate itself readable for the first time, and it turned out to be four lines with no parameters at all.

The `userIgnoreFilters` glob-vs-regex error is the more expensive of the two: it is a `.obsidian/app.json` value contract, and the audit's `// glob patterns` annotation would have been copied straight into a Corbomite settings UI.

## Action taken

- Open question 4 closed: no `configDir` parameterisation. Recorded `Il` verbatim as the normative visibility predicate; a Corbomite `Vault` file tree must apply it to every ancestor segment of every path, not just the basename, and must not add a `.obsidian`-specific exemption.
- `userIgnoreFilters` documented as a regex-or-anchored-literal-prefix list with the exact two-form grammar, trimming, escaping and error handling. A Corbomite settings UI must label these as regular expressions (with the `/…/` hint) or translate globs to the anchored form deliberately.
- Recorded that ignore filters are a *search/graph/property-edit* gate, not a tree-visibility gate, to prevent the two mechanisms being conflated during implementation.
