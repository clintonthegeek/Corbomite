# Content-rewrite contracts: frontmatter serialisation options, `insertIntoFile` newlines, link generation, and the two-phase orphan-attachment sweep

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/vault.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`).
**Supersedes / extends:** `domains/vault.md` §1 (`FileManager` → *Property edits*, *Deletion UI*, *Attachments*, *Link generation*, *Content merge*, *Frontmatter public API*), §8 (`processFrontMatter` bullet), §9; `VAULT-FORMAT.md` frontmatter/link-generation sections.
**Relevant cluster plans:** `libs/vault/FileManager`, frontmatter/property-editing work, Cluster N rich clipboard (paste-merge), attachment-import.

## Finding

### NEW (HIGH — note content format): the frontmatter serialiser pins three js-yaml options

Every frontmatter write in the app goes through one wrapper:

```js
// app/own/1219-bL.js:2-11  (bL = the only YAML dumper used for frontmatter)
… (e, null, { nullStr: "", lineWidth: 0, aliasDuplicateObjects: !1 });
```

- **`lineWidth: 0` — no line wrapping.** js-yaml's default is 80 columns; Obsidian disables folding entirely. A long paragraph in a frontmatter string is emitted on one line. A port using a wrapping emitter will reformat notes on the first property edit.
- **`nullStr: ""`** — a YAML `null` serialises as an *empty scalar*, not `null`, `~`, or an empty value with a trailing marker.
- **`aliasDuplicateObjects: false`** — repeated references to the same object are **expanded**, never emitted as `&anchor` / `*alias`. A note whose frontmatter holds the same sub-object twice will grow on each Obsidian-saved edit if a port aliases them.

### NEW: `processFrontMatter` is a whole-text rewrite, and it *deletes* the block when the mutator empties it

```js
// app/own/1313-wR.js:2-5
function wR(e, t) {
  var n = _T(e), i = n.exists ? yL(n.frontmatter) : {};
  if (i && "object" == typeof i || (i = {}), t(i),
      0 === Object.keys(i).length) return n.exists ? e.slice(n.contentStart) : e;
  var r = bL(i);
  return n.exists ? e.slice(0, n.from) + r + e.slice(n.to) : "---\n" + r + "---\n" + e;
}
// app/own/1312-hR.js:888-896  (the public wrapper)
e.prototype.processFrontMatter = function (e, t, n) {
  return "md" !== e.extension ? [2] : [4, this.vault.process(e, function (e) { return wR(e, t); }, n)];
};
```

- Non-`.md` files are a **silent no-op** (confirmed).
- Empty result ⇒ the `---` block is removed entirely and the body resumes at `contentStart`. The audit describes the op as "rewrites the YAML block preserving key order" and never mentions the deletion case.
- No pre-existing block ⇒ `"---\n" + yaml + "---\n" + content`; because `bL` output ends in a newline this is well-formed, but note there is **no** blank line between the closing `---` and the body.
- The whole thing runs inside `vault.process`, i.e. the adapter's write is skipped when the mutator is a no-op (`app/own/0376-_l.js:318`).

### NEW: `insertIntoFile`'s newline contract is exact, and the frontmatter merge is right-biased

```js
// app/own/1312-hR.js:857-887   (separator at `:877`, frontmatter merge at `:878-880`)
o = ER(t).content, a = ER(t).frontmatter;     // t = the incoming content
s = ER(e).content, l = ER(e).frontmatter;     // e = the target file
var c, u = "prepend" === n ? o : s,           // u = first part
         h = "prepend" === n ? s : o;         // h = second part
c = u && h && !(u.slice(-2) + h.slice(0, 2)).contains("\n\n")
      ? (u.endsWith("\n") || h.startsWith("\n") ? u + "\n" + h : u + "\n\n" + h)
      : u + h;
CR(l, a);
if (Object.keys(l).length > 0) { var p = bL(l).trim(); c = "---\n".concat(p, "\n---\n") + c; }
```

- **Body order**: `append` → `[existing, incoming]`; `prepend` → `[incoming, existing]`.
- **Separator**: if the last two chars of `u` plus the first two chars of `h` already contain `\n\n`, join with nothing; else if `u` ends with `\n` **or** `h` starts with `\n`, join with a single `\n`; else join with `\n\n`. So a merge never invents or destroys a blank line, and never produces a triple.
- Note the extra `.trim()` on the serialised YAML here, which `wR` does **not** apply — the two frontmatter emitters differ by a trim, so their output can differ in trailing whitespace.
- Frontmatter merge is `CR(target, incoming)` (`app/own/1315-CR.js:2`): scalars/objects take the **incoming** value, arrays `concat(...).unique()`, objects recurse. The audit's "right-biased by mode" is wrong twice — it is always right-biased (incoming wins), and `mode` only controls body order.

### NEW: `generateMarkdownLink` display-text rules, and same-file links collapse to the bare subpath

```js
// app/own/1312-hR.js:824-835
a = o.vault.getConfig("useMarkdownLinks"), s = !a,
l = o.metadataCache.fileToLinktext(e, t, s) + (n || "");
if (e.path === t && n) l = n;
if (a) {
  var c = YE(l);
  r = "md" === e.extension ? "[".concat(i || e.basename, "](").concat(c, ")")
                          : "[".concat(i || "", "](").concat(c, ")");
} else i && i.toLowerCase() === l.toLowerCase() && (l = i, i = null),
     r = i ? "[[".concat(l, "|").concat(i, "]]" : "[[".concat(l, "]]");
```

- **Same-file link** (`e.path === t`) with a subpath ⇒ `l = n`, i.e. `[[#Heading]]` / `[Heading](#Heading)`, never `[[Note#Heading]]` from inside `Note.md`.
- In markdown-link mode, a **non-`.md` target gets an empty display text** (`i || ""`), while an `.md` target falls back to `e.basename`. So `[](image.png)` is the shape Obsidian writes for an attachment, not `[image.png](image.png)`.
- The wiki alias elision the audit describes is real but **case-insensitive**: `i.toLowerCase() === l.toLowerCase()`.
- The third argument to `fileToLinktext` is not "use markdown" — it is **strip the `.md` extension**, and it is passed `!useMarkdownLinks`. See `app/own/1306-eR.js:204-217`: `n` defaults to `true`, and `r = "md" === e.extension && n ? Ol(e.path) : e.path`. In wiki mode the extension is dropped; in markdown-link mode it is kept because the target needs it.

### NEW: link rewriting preserves more syntax than the audit implies

The rewriter dispatches on two anchored regexes:

```js
// app/own/1101-sD.js:2-3
sD = /^(!?\[\[)(.*?)(\|(.*))?(]])$/,
lD = /^(!?\[)(.*?)(]\(\s*)((<[^>]*?>|[^ "]+?)(\s+([^ ]+|"[^"]+"|'[^']+'|\([^']+\)))?)?(\s*\))$/;
```

(`app/own/1305-JF.js:2-13`.) Capture groups mean a rewrite preserves: the leading `!` embed marker, the existing display text, the angle-bracket `<…>` target form, an optional **link title** (bare, `"…"`, `'…'` or `(…)`), and trailing whitespace before `)`. If neither pattern matches, the fallback is `"[](" + encoded + ")"` with a leading `!` re-added if the original had one — so a malformed link is **normalised to an empty display text**, not left alone.

The rename path's own gate is `kR` (`app/own/1314-kR.js:2-16`): it re-resolves the *old* linktext against post-mutation state, compares the resulting path array to the pre-snapshot `resolvedPaths` element-wise, and only then emits `fileToLinktext(resolvedFile, sourcePath, sD.test(reference.original)) + subpath`. So the **subpath is carried over verbatim**, and the link *style* is preserved from the original text (a `[[…]]` stays `[[…]]`, a `[…](…)` stays `[…](…)`), independent of `useMarkdownLinks`.

### NEW (HIGH — vault safety): the orphan-attachment sweep is two-phase, and its candidate filter is "not a note file", not "is an attachment"

```js
// app/own/1312-hR.js:594-601  (promptForDeletion)
u = {}, h = t.metadataCache.resolvedLinks, e instanceof GD && Object.hasOwn(h, e.path)
  && [for (f in d = h[e.path]) Object.hasOwn(d, f) && Cb(f) && (E = t.vault.getFileByPath(f)) && (u[E.path] = E)];
return [4, this.trashFile(e)];            // ← phase 1: snapshot candidates, THEN trash
…
for (p in h) if (Object.hasOwn(h, p)) for (f in d = h[p]) Object.hasOwn(d, f) && delete u[f];   // ← phase 2: prune
```

- `resolvedLinks[sourcePath]` maps **destination path → { sourcePath: count }**. Candidates are the *sources* of every outgoing link from the doomed file, filtered by `Cb`.
- Candidates are snapshotted **before** the file is trashed and pruned **after** — pruning walks every remaining file's `resolvedLinks` and removes any candidate still referenced from anywhere. So the check is "became unreferenced as a result of this deletion", not "is unreferenced right now". A naive pre-check would delete attachments that other notes embed; a naive post-check would miss the ones this deletion orphaned.
- The filter is **not** an attachment whitelist: `Cb(e) = kb(Fl(e))` and `kb(e) = !bb.includes(e)` with `bb = ["md","canvas","base"]` (`app/own/0796-Cb.js:2`, `app/own/0795-kb.js:2`, `app/own/0794-hb.js:10`). Anything that is not `.md`, `.canvas` or `.base` is a candidate — `.pdf`, `.json`, `.txt`, arbitrary binaries. A port that whitelists image extensions will under-delete relative to Obsidian.

### NEW: downloaded-attachment frontmatter properties are always rewritten as **wiki** links

```js
// app/own/1312-hR.js:727-736  (inside downloadAttachmentsForNote's vault.process)
t.length > 0 && (i = wR(i, function (e) { … 
  var c = a[l], u = s.app.metadataCache.fileToLinktext(c, "", !1);
  e[o] = "[[".concat(u, "]]");
}));
```

Body embeds go through `generateMarkdownLink` and therefore honour `useMarkdownLinks`; frontmatter image properties are **hard-coded** to `[[…]]` with `fileToLinktext(file, "", false)` (extension kept). So a vault with `useMarkdownLinks: true` ends up with a mixed convention after an attachment download. Corbomite must reproduce that asymmetry rather than "fixing" it.

### CONFIRMED

| Claim (`domains/vault.md`) | 1.13.7 location |
|---|---|
| `getMarkdownNewFileParent` honours `folder`+`newFileFolderPath`, `current`→dirname of hint, else root; falls back to root when the configured folder is absent | `app/own/1312-hR.js:288-298` |
| `runAsyncLinkUpdate` awaits `metadataCache.onCleanCache`, snapshots via `iterateAllRefs`, mutates, then replays | `app/own/1312-hR.js:370-429` |
| "Always update" persists `alwaysUpdateLinks`; the prompt is skipped entirely when it is already on | `app/own/1312-hR.js:430-501` (`alwaysUpdateLinks` read at `:463`, persisted at `:476`) |
| `deleteProperty`/`renameProperty` skip user-ignored files | `app/own/1312-hR.js:509`, `:538` |
| `trashFile` router: `system`→`trash(_, true)`, `local`→`trash(_, false)`, `none`→`delete(_, true)` | `app/own/1312-hR.js:649-670` |
| `storeTextFileBackup` calls `file-recovery`'s `forceAdd` and swallows errors | `app/own/1312-hR.js:901-913` |
| `notifyForBulkUndo` defaults to 30 000 ms and restores content **and mtime** | `app/own/1312-hR.js:926-928`, `:939-944`, Notice at `:947` |
| Bulk undo runs through `updateQueue`, i.e. serialised against rename link-updates | `app/own/1312-hR.js:930`, `:931` |
| `getAllLinkResolutions()` returns `[]` | `app/own/1312-hR.js:554-556` |
| `createAndOpenMarkdownFile` opens with `state.mode = "source"`, `eState.rename = "all"` | `app/own/1312-hR.js:835-856` |
| `getAvailablePathForAttachment` splits the linktext with `Ll`/`Fl`; an unresolvable `sourcePathHint` falls back to `null`, **not** to the active file | `app/own/1312-hR.js:919-925` |
| `promptForDeletion` returns `false` on cancel and `true` otherwise; "don't ask again" writes `promptDelete: false` | `app/own/1312-hR.js:591`, `:592`; dialog copy from `:562-589` |

## Why noticed now

These are the operations that **write into note bodies and frontmatter** — the highest-blast-radius surface in the whole vault domain, because a divergence re-serialises a user's notes rather than merely mis-rendering them. The audit documented their *shape* (from `FileManager.js` line refs in a tree that no longer exists) but not their serialisation parameters, which is exactly the layer that determines whether a Corbomite edit is byte-identical to Obsidian's. The `lineWidth: 0` finding in particular is the kind of thing that passes every functional test and then reformats half a vault in production.

The two-phase orphan sweep was found while checking the audit's claim that `"always"` "silent-trashes orphan attachments" — the word *orphan* is doing more work than the source supports, and the algorithm is only correct with both phases.

## Action taken

- The three js-yaml options (`nullStr: ""`, `lineWidth: 0`, `aliasDuplicateObjects: false`) are recorded as the normative frontmatter-serialisation contract for any Corbomite writer that must not reformat existing notes.
- `processFrontMatter`'s empty-result deletion semantics and the `insertIntoFile` three-way separator rule are recorded as testable contracts; the `wR` vs `insertIntoFile` trim asymmetry is called out so the two emitters are not unified by accident.
- `generateMarkdownLink`'s same-file collapse, non-`.md` empty display text, and case-insensitive alias elision are recorded as byte-level requirements for link generation.
- The orphan sweep is recorded as a mandatory two-phase algorithm with a `not-in {md, canvas, base}` candidate filter, and the frontmatter-wiki-link asymmetry is flagged so it is reproduced rather than corrected.
