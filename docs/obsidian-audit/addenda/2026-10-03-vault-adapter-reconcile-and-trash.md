# Adapter reconciliation, the 100 ms ENOENT grace, `saving` as a counter, and `.trash/` naming parity

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/vault.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`).
**Supersedes / extends:** `domains/vault.md` §1 (`TAbstractFile`, `TFile`, `Vault` write-path, `FileSystemAdapter`, `CapacitorAdapter`), §3 (`.trash/`), §4 (event table — old `Vault.js:*` line refs), §8, §13 Q6 + Q7.
**Relevant cluster plans:** `libs/storage/FileSystemAdapter`, `libs/core/Vault` event fan-out, `libs/vault/FileManager` trash routing.

## Finding

### CORRECTION: `TFile.saving` is a nesting **counter**, not a boolean

`domains/vault.md` §1 types it as `saving: boolean` and §8 says "`Vault.modify(file, text)` sets `file.saving = true` around the adapter call."

```js
// app/own/1129-UD.js:17
i.saving = 0, i.stat = null, i;
// app/own/1129-UD.js:435-451  (modify — same shape in modifyBinary/append/appendBinary/process)
e.saving++, ...
catch (i = r.sent(), e.cache(null), i)          // modify only: invalidate cache on throw
finally { e.saving-- }
// app/own/1129-UD.js:264  (onChange 'modified')
a.stat = i, a.saving || a.cache(null), this.trigger("modify", a);
```

Five write paths each `++` on entry and `--` in a `finally` (`app/own/1129-UD.js:441/451`, `462/470`, `481/489`, `500/508`, `520/528`). The cache-invalidation guard is `a.saving || …`, i.e. **falsy**, so it fires only when the counter has returned to 0. A boolean would break the moment two writes overlap on one `TFile` — the second `modify` to complete would clear the cache that the first one's self-caused `modified` event was supposed to preserve. Corbomite needs an equivalent depth counter (or a token/generation scheme), not a flag; `Vault.process` re-entrancy is the common case, since frontmatter edits nest inside larger `process` calls.

`Vault.modify` also **overwrites** any caller-supplied `immediate` hook (`(n = n || {}).immediate = function () { return e.cache(t); }`, `app/own/1129-UD.js:443`), so a caller cannot supply its own; `modifyBinary`/`append`/`appendBinary`/`process` all set it to `() => e.cache(null)` (invalidate) instead. The audit's `DataWriteOptions` (§2) lists `immediate` as if it were caller-supplied for every write path.

### CORRECTION: `getNewPathAfterRename` replaces control chars with a space

Recorded in full in the path-validation addendum; repeated here only because the audit's §1 wording ("Strips control chars `[\x00-\x1F]`") is wrong and the two claims sit in the same list. Source: `app/own/1129-UD.js:10`.

### NEW: a watcher-reported deletion gets a **100 ms grace re-check** before the node is removed

This is the single most consequential undocumented behaviour in the reconciler, and it is invisible unless you are chasing phantom file deletions.

```js
// app/own/0376-_l.js:827-833  (reconcileDeletion(realPath, vaultPath, isDelete = true))
if ("/" === t) return this.trigger("closed", t), [2];
if (this.stopWatchPath(t), !(i = this.files[t])) return [2];
if (!n) return setTimeout(function () { o.queue(function () { return o.reconcileFile(e, t); }); }, 100), [2];
```

The third parameter `isDelete` defaults to `true` and is threaded from the two entry points:

- `reconcileInternalFile` (adapter-initiated, e.g. after a write) → `reconcileFile(real, path)` with the default `true` (`app/own/0376-_l.js:673-675`).
- `onFileChange` (inotify/`fs.watch`-driven) → `reconcileFile(e, n, !1)` — explicitly `false` (`app/own/0376-_l.js:665-672`).

So an ENOENT seen by the **watcher** does not delete anything on the first miss; it schedules one full reconcile 100 ms later, and only that second pass (running with `isDelete = true`) removes the entry. Any editor or sync client doing write-to-temp-then-rename, or unlink-then-recreate, therefore produces no spurious `delete`/`create` event pair. A reconciler that deletes on first ENOENT will emit phantom deletions and re-additions for every atomic save — the same class of bug Corbomite hit with `.worktrees`-style KDDW teardown, but on the vault side.

Confirmed corollary: `'/' === t` fires `closed` **unconditionally** on reconcile, with no grace (`app/own/0376-_l.js:827`), which is the whole of audit open question 9's mechanism. Vault's own `closed` event carries no payload (`app/own/1129-UD.js:275`).

### CONFIRMED: audit open question 6 — folder rename fires **N+1** `renamed` events, parent first, children in `for…in` insertion order

```js
// app/own/0376-_l.js:408   (folder itself)
delete this.files[e], a = this.getRealPath(t), r.realpath = a, this.files[t] = r, this.trigger("renamed", t, e),
"folder" === r.type && this.watchers.hasOwnProperty(e) ? (this.stopWatchPath(e), startWatchPath(t)) : …
// app/own/0376-_l.js:413   (each descendant)
for (s in this.files) this.files.hasOwnProperty(s) && s.startsWith(e + "/") &&
  (l = s.slice(e.length), c = t + l, u = this.files[s], delete this.files[s], h = u.realpath.slice(o.length), u.realpath = a + h, this.files[c] = u, this.trigger("renamed", c, s));
```

Not sorted — `for…in` over `adapter.files`, i.e. insertion order from the initial scan. `Vault.onChange` maps each to `rename(file, oldPath)` after mutating `fileMap` (`app/own/1129-UD.js:270-274`), so `MetadataCache.onRename` runs once per descendant (`app/own/1306-eR.js:514-526`). Consumers must be idempotent, exactly as the audit's Q6 suspected.

### NEW: on case-insensitive filesystems the reconciler normalises sibling dirents before matching

```js
// app/own/0376-_l.js:693-697
this.insensitive && "/" !== e
  ? (o = this.path.dirname(r), a = this.path.basename(t), [4, this.fsPromises.readdir(o)])
  : [3, 7];
-1 !== l.sent().map(function (e) { return Dl(e).normalize("NFC"); }).indexOf(a) ? [3, 7] : [4, this.reconcileDeletion(e, t, n)];
```

Every sibling name is put through `Dl` (NBSP U+00A0 + narrow NBSP U+202F → space, `app/own/0358-Tl.js:2`) then `.normalize("NFC")` before the case-sensitive `indexOf` against the incoming basename. Net effect on macOS/Windows: **a file created on disk under an NFD name (or with NBSP in it) is registered in the vault tree under its NFC, NBSP-folded form**, and the reconciler will consider the on-disk name "missing" only if neither form matches. Linux takes the `insensitive = false` branch and does no such normalisation, so tree contents can differ across platforms for the same directory.

### NEW: symlinks resolving into an already-watched part of the vault are silently skipped

```js
// app/own/0376-_l.js:799-802  (reconcileSymbolicLinkCreation)
r = this.path.sep, (o = this.watchers).hasOwnProperty(t) ? o[t].resolvedPath = i
  : for (a in o) if (o.hasOwnProperty(a) && a !== t && (s = o[a].resolvedPath,
        i === s || s.startsWith(i + r) || i.startsWith(s + r))) return [2];
```

If the symlink's `realpath` equals, contains, or is contained by any existing watcher's `resolvedPath`, the reconciler returns without creating any `files` entry and without emitting `file-created`/`folder-created`. This is a deliberate de-duplication (a symlink into an already-indexed subtree would otherwise duplicate the whole subtree in the tree), and it means **symlink aliases are not modelled at all** — only the canonical location appears. The audit's §9 "Symlinked subfolders inside the vault resolve transparently via `getRealPath`" is right about `getRealPath` (`app/own/0376-_l.js:841-849`) but does not mention that aliased entries are dropped.

### NEW: every adapter operation races a 60-second watchdog that **rejects** the pending op

```js
// app/own/0376-_l.js:9
this.thingsHappening = Ml(this.kill.bind(this), 6e4, !0)
// app/own/0376-_l.js:855-863
e.prototype.kill = function () { this.killLastAction && (this.killLastAction(new Error("File system operation timed out.")), this.killLastAction = null); };
e.prototype.queue = function (e) {
  var n = function () { var n = new Promise(function (e, n) { return t.killLastAction = n; }); return t.thingsHappening(), Promise.race([n, e()]); },
      i = this.promise.then(n, n);
  return this.promise = i, i;
};
```

`killLastAction` is the **reject** function of a pending `Promise`. Any 60-second gap in queue activity (network mount wedged, `fs.watch` storm, a hung `btime` call) rejects whichever adapter operation is in flight with `Error("File system operation timed out.")` — and because `queue` chains off `this.promise`, subsequent operations still run. The audit lists `thingsHappening` as an instance field with no explanation; this is what it does. `fs.watch` is also installed with `persistent: false` (`app/own/0376-_l.js:645-650`), so watchers never hold the process open.

### NEW: `readBinary` gates the cache on **UTF-8 byte length**, `read`/`cache()` on **UTF-16 unit length**

```js
// app/own/1129-UD.js:365-376  (readBinary)
t = n.sent();
try { "md" === e.extension && t.byteLength <= this.cacheLimit && e.cache(new TextDecoder().decode(t)); } catch (e) {}
// app/own/1129-UD.js:25-29  (TFile.cache — the gate every other path goes through)
t.prototype.cache = function (e) { if (null != e) { var t = this.vault.cacheLimit; e.length <= t ? KD.set(this, e) : KD.delete(this); } else KD.delete(this); };
```

`byteLength` (UTF-8 bytes) and `e.length` (UTF-16 code units) are equal only for pure-ASCII files. For a note heavy in CJK or emoji, `readBinary` declines to populate the cache while `read` would accept it — a real inconsistency, and the audit's open question 8 ("Corbomite cache policy should match explicitly") now has a concrete answer. `TextDecoder().decode` with default options also strips a leading BOM, so the two cache-fill paths agree on BOM handling. `read` strips the BOM explicitly and unconditionally: `65279 === (t = n.sent()).charCodeAt(0) && (t = t.substring(1))` (`app/own/1129-UD.js:355`), caching the stripped text.

### NEW: `recurseChildren` is a reverse-sibling pre-order DFS

```js
// app/own/1129-UD.js:604-611
t.recurseChildren = function (e, t) {
  for (var n = [e]; n.length > 0;) { var i = n.pop(); if (i && (t(i), i instanceof YD)) { var r = i.children; n = n.concat(r); } }
};
```

`n.pop()` takes the **last** pushed child, so `getMarkdownFiles()` and `getFiles()` (both built on it, `app/own/1129-UD.js:615`, `:621`) enumerate the *last* child first at every level. The audit says "DFS" and is not wrong, but the order is observable: any Corbomite index built by iterating these (SQLite insert order, a bundled `MetadataCache` preload, an export manifest) will come out in reverse-sibling order unless it sorts explicitly.

### CORRECTION: the mobile adapter's `.trash/` naming **does** preserve the extension

`domains/vault.md` §1 (`CapacitorAdapter`) says "`trashLocal` moves to `.trash/` without a preserved extension suffix (name-space-`$n` scheme instead of `<base>_$n.<ext>`)", and §3 repeats it ("Desktop preserves the original extension; mobile appends ` N` without an extension separator").

```js
// app/own/0887-hC.js:160-164  (CapacitorAdapter.trashLocal)
r = Al(t), o = Ol(r), a = Fl(r), s = n + "/" + o, a && (s = s + "." + a), l = 1;
…
return c.sent() ? (l++, s = n + "/" + o + " " + l, a && (s = s + "." + a), [3, 5]) : [3, 7];
```

Mobile produces `.trash/<base>.<ext>` and `.trash/<base> 2.<ext>` — byte-identical naming to desktop (`app/own/0376-_l.js:129-133`). The only real divergences are the mkdir error handling (`"Directory exists"` is swallowed, `app/own/0887-hC.js:153-156`) and the absence of `path.extname`. The audit's `name-space-$n` phrasing appears to be a garbled reading of `" " + a + i`. Both adapters start the counter at 1 and **pre-increment**, so the first collision is ` 2` — confirming the `2026-06-10` addendum's §1 note that only `getAvailablePath` starts at ` 1`.

### CONFIRMED / CLOSED: audit open question 7 — `.trash/` is never read, parsed, restored or purged by core

The audit guessed "Presumably a user-visible cleanup tool lives in a settings tab." There is no such tool. The only Obsidian affordance is a desktop-only command that reveals the folder in the OS file browser:

```js
// app/own/1932-r4.js:764-768
rd.isDesktopApp && l.addCommand({
  id: "workspace:show-trash", name: bd.commands.showTrash(),
  checkCallback: function (e) { if ("local" === t.vault.getConfig("trashOption")) return e || t.showInFolder(".trash"), !0; }
});
```

`checkCallback` returns falsy (command unavailable/hidden) unless `trashOption === "local"`. There is no `empty-trash` command id and no purge path in either adapter. §3's "Read by: nobody in core" and "never purged by Obsidian" are confirmed; only the Q7 speculation is refuted. `.trash/` is additionally never synced (see the config addendum, rule 5).

### CONFIRMED (other §1/§4 claims, now citable)

| Claim | 1.13.7 location |
|---|---|
| `getResourcePath` → `<prefix><file-url>?<mtime>`, Windows UNC → `%5C%5C` prefix, `Date.now()` fallback | `app/own/0376-_l.js:350-357` |
| `rename` rejects on destination collision; case-only rename allowed when `insensitive` | `app/own/0376-_l.js:394-396` |
| `copyRecursive` uses `COPYFILE_EXCL`; `rmdir` uses `maxRetries: 5` | `app/own/0376-_l.js:432`, `:153` |
| `adapter.process` skips the write when `mutator(text) === text` (and therefore skips `applyWriteOptions`, so `{mtime}` is **not** applied on a no-op) | `app/own/0376-_l.js:318` |
| `_exists` readdir double-check only when `caseInsensitiveConfirm && insensitive` | `app/own/0376-_l.js:502-510` |
| `watchHiddenRecursive` is a no-op on macOS/Windows; swallows all errors | `app/own/0376-_l.js:585-591`, `:616-617` |
| `listAll` seeds `files["/"]` with a synthetic folder entry, then `listRecursive("")` | `app/own/0376-_l.js:28-40` |
| `testInsensitive` unlinks a stale `.OBSIDIANTEST` first | `app/own/0376-_l.js:16-24` |
| `getAvailablePathForAttachments` creates the folder if missing | `app/own/1129-UD.js:575` (`getAbstractFileByPathInsensitive` → `createFolder`) |
| `create`/`createBinary`/`createFolder` throw `"File already exists."` / `"Folder already exists."` | `app/own/1129-UD.js:310`, `:325`, `:340` |
| `delete()` and `trash()` are no-ops on `root`; `delete(folder)` defaults `recursive = false` | `app/own/1129-UD.js:390-405`, `:407-421` |
| `generateFiles` iterates `.md` and `.canvas` only (**not** `.base`) | `app/own/1129-UD.js:667` |
| `iterateFiles` iterates `.md` only | `app/own/1129-UD.js:632` |
| Root `TFolder` is materialised by routing the constructor through `onChange("folder-created", "/")` | `app/own/1129-UD.js:93` |
| `getAbstractFileByPathInsensitive` compares only equal-length keys | `app/own/1129-UD.js:238-244` |

## Why noticed now

The audit's line references (`Vault.js:440`, `FileSystemAdapter.js:595`, `:617`, …) all pointed into a destroyed 1.12.7 tree, so none of these claims could be spot-checked. Restoring the corpus made the reconciler readable end-to-end, and the 100 ms grace, the reverse-sibling iteration order, the byte-vs-unit cache gate, and the mobile `.trash` parity were all invisible to the audit's method. The `saving` counter in particular had been described as a boolean in two places and would have been ported as a flag.

## Action taken

- Open questions 6, 7 and 9 closed; question 8 answered (cache-gate asymmetry documented).
- The 100 ms ENOENT grace is recorded as a **hard requirement** for any Corbomite `Vault` reconciler: a first watcher-seen ENOENT must re-check once after 100 ms before removing the node, and the re-check must run in "delete-authorised" mode.
- The `saving` counter is recorded as a required nesting-safe depth counter, with `Vault.process` re-entrancy called out as the case that a boolean would break.
- `.trash/` naming parity between desktop and mobile confirmed, so a Corbomite writer needs one rule, not two; the ` 2`-first counter for trash is confirmed distinct from `getAvailablePath`'s ` 1`-first.
- Recorded that `getFiles()`/`getMarkdownFiles()` order is reverse-sibling, so any Corbomite consumer that wants stable ordering must sort rather than rely on iteration order.
