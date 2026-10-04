# `lastOpenFiles` is quota-capped per file category (≤ 55 entries), not a plain recent list — and the on-disk list is not what `getLastOpenFiles()` returns

**Date:** 2026-10-03
**Discovered during:** `workspace.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/workspace.md` §2 (`LayoutJson.lastOpenFiles`) and
§13 Q1 (`B0` / recent-file tracker)
**Relevant cluster plans:** `superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md`
(Phase L2 — `workspace.json` persistence)

## Finding

`workspace.md` describes `lastOpenFiles` only as "recentFileTracker — most-recent-first
vault-relative paths" and §13 Q1 lists `B0`'s methods without semantics. In 1.13.7 the
tracker (class `n4`, `app/own/1930-n4.js`) keeps **four independent per-category quotas**,
so the array written into `workspace.json` has a hard ceiling of **55 entries**, not 10:

```js
// app/own/1930-n4.js:10-28  (n4.collect)
// e.prototype.collect = function(e) {
//   if (this.lastOpenFiles[0] !== e.path) {
//     for (var t = {
//         md: 25,
//         canvas: 10,
//         image: 10,
//         other: 10
//       }, n = [e.path], i = 0, r = this.lastOpenFiles; i < r.length; i++) {
//       var o = r[i];
//       if (o !== e.path) {
//         var a = Fl(o),
//           s = "other";
//         vb.contains(a) && (s = "md"), mb.contains(a) && (s = "canvas"), hb.contains(a) && (s = "image");
//         var l = t[s];
//         l <= 0 || (n.push(o), t[s] = l - 1);
//       }
//     }
//     this.lastOpenFiles = n;
//   }
// },
```

| Category | Extension set | Quota |
|---|---|---:|
| `md` | `vb` | 25 |
| `canvas` | `mb` | 10 |
| `image` | `hb` | 10 |
| `other` | everything else — **including `.base`, `.pdf`, audio and video** | 10 |

`25 + 10 + 10 + 10 = 55`. Because the quotas are per-category and the loop walks the old
list in order, the surviving array is *interleaved* by recency across categories, not
grouped. `collect()` is a no-op when the path is already at index 0.

### Three further behaviours

1. **The on-disk list is `lastOpenFiles` raw; `getLastOpenFiles()` is filtered and capped at
   10.** `serialize()` returns the array itself (`app/own/1930-n4.js:8-9`), while:

   ```js
   // app/own/1930-n4.js:40-46  (n4.getLastOpenFiles)
   // e.prototype.getLastOpenFiles = function() {
   //   return this.getRecentFiles({
   //     showMarkdown: !0,
   //     showNonAttachments: !0,
   //     showNonImageAttachments: !0,
   //     showImages: !0
   //   });
   // },
   ```

   `getRecentFiles` merges over `{showMarkdown: true, showNonAttachments: true,
   showNonImageAttachments: false, showImages: false, maxCount: 10}`
   (`app/own/1930-n4.js:47-60`) and breaks on `n.length >= t.maxCount`. The caller above
   sets only the four flags, so **`maxCount` stays 10**. Two different lengths therefore
   exist for the same data, and the public API reports the short one. `workspace.md` §13 Q1
   does not distinguish them.

   Note the two non-`.md` branches differ: `"canvas" === a || "base" === a` is gated by
   `showNonAttachments`, while images are gated by `showImages` and all remaining
   extensions by `showNonImageAttachments`.

2. **Every newly created file enters the list, whether or not it was ever opened.**

   ```js
   // app/own/1930-n4.js:3-4  (n4 constructor)
   // this.workspace = e, this.vault = t, this.lastOpenFiles = [], t.on("create", this.addRecentFile, this), t.on("rename", this.onRename, this);
   ```

   ```js
   // app/own/1930-n4.js:29-39
   // e.prototype.onFileOpen = function(e, t) {
   //   this.workspace.layoutReady && t && (this.collect(t), this.workspace.requestSaveLayout());
   // }, e.prototype.addRecentFile = function(e) {
   //   this.workspace.layoutReady && (this.collect(e), this.workspace.requestSaveLayout());
   // }, e.prototype.onRename = function(e, t) {
   //   if (this.workspace.layoutReady) {
   //     var n = e.path;
   //     this.lastOpenFiles = this.lastOpenFiles.map(function(e) { return e === t ? n : e; }),
   //       this.workspace.requestSaveLayout();
   //   }
   // },
   ```

   `onFileOpen(new, old)` collects the **outgoing** file (`t`), not the incoming one, and
   requires `old` to be truthy — so a file only enters the list when something is navigated
   away from it, or when it is created. All three paths are gated on `layoutReady`, which
   is why `load()` during `setLayout()` cannot trigger a save.

3. **`load()` performs no validation, capping, dedup, or existence check.**

   ```js
   // app/own/1930-n4.js:6-7
   // e.prototype.load = function(e) { this.lastOpenFiles = e; },
   ```

   called from `loadLayout` as
   `e.hasOwnProperty("lastOpenFiles") && this.recentFileTracker.load(e.lastOpenFiles)`
   (`app/own/1932-r4.js:943`). A hand-edited `workspace.json` with 500 entries, duplicate
   paths, or paths that no longer exist loads verbatim; the quota only reasserts itself on
   the next `collect()`.

## Why noticed now

Cluster L2 made `workspace.json` persistence full-fidelity and split Corbomite's
private keys out of it. Getting the *field set* right is not enough for a shared data
file — a writer that emits `lastOpenFiles` as "the 10 most recent files" (the natural
reading of the name, and of `getLastOpenFiles()`) will truncate a real Obsidian-authored
list on the first save, and the diff is user-visible round-trip damage in a file Obsidian
reads and rewrites.

## Action taken

- No code change yet. `WorkspaceSerializer.cpp` currently round-trips whatever
  `lastOpenFiles` it reads, which is correct-by-construction for the read path and needs
  a decision only for the write path: a Corbomite-side recent-file list must apply the
  same four quotas, or it must not claim to write the field at all. Flagged here so the
  choice is explicit when `SessionManager`'s native state is finalised.
- Linked from `addenda/README.md` and `00-taxonomy.md` §Addenda.

## Confirmed (table)

| Claim in `workspace.md` | Verdict | Location |
|---|---|---|
| `lastOpenFiles` is written by `saveLayout()` from `recentFileTracker.serialize()` | confirmed | `app/own/1932-r4.js:1173` |
| `loadLayout()` calls `recentFileTracker.load(lastOpenFiles)` before `setLayout` | confirmed | `app/own/1932-r4.js:943` |
| `Workspace.getLastOpenFiles()` / `getRecentFiles(n)` / `addRecentFile(file)` delegate to the tracker | confirmed | `app/own/1932-r4.js:1796-1802` |
| `lastOpenFiles` is most-recent-first | confirmed (`n = [e.path]` then old entries in order) | `app/own/1930-n4.js:12-27` |