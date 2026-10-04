# Re-verification of the 2026-06-10 path/naming corrections against Obsidian **1.13.7** (all four hold) + citation map

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/vault.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`, rebuilt 2026-10-03 from `obsidian_1.13.7_amd64.deb`; previous audit baseline was 1.12.7).
**Supersedes / extends:** [`2026-06-10-vault-path-and-naming-corrections.md`](2026-06-10-vault-path-and-naming-corrections.md) — re-baselines every `file:line` in it and confirms none of its five claims regressed.
**Relevant cluster plans:** `libs/core/PathNormalize`, `libs/vault/FileManager`, `libs/storage/MetadataCache*` (offset arithmetic).

## Finding

**All four corrections in the 2026-06-10 addendum are still correct in 1.13.7, unchanged.** Two of them moved to different symbols/files; nothing changed semantically. A fifth, previously unverifiable, general claim (all `CachedMetadata` offsets are UTF-16 code units) is unaffected — the decode-then-index structure is intact.

| 2026-06-10 claim | Still true in 1.13.7? | 1.13.7 location (was 1.12.7) |
|---|---|---|
| §1 `getAvailablePath` collision suffix starts at ` 1` | **Yes** | `app/own/1129-UD.js:566-569` (was `tree/obsidian/vault/Vault.js:884-890`) |
| §1 only `.trash/` starts at ` 2` (pre-increment) | **Yes** | desktop `app/own/0376-_l.js:117-131` (was `FileSystemAdapter.js:185-225`); mobile `app/own/0887-hC.js:143-166` |
| §2 `unresolvedLinks` keys are **not** lowercased | **Yes** | `app/own/1299-KF.js:2` (`KF`), called at `app/own/1306-eR.js:408` (was `BL` at `MetadataCache.js:646-662`) |
| §3 `normalizePath` never strips `./`, and **does** strip leading slashes | **Yes** | `Nl` = `Dl(Bl(e)).normalize("NFC")` at `app/own/0367-Nl.js:2`; `Bl` at `app/own/0368-Bl.js:2`; `Dl` at `app/own/0359-Dl.js:2` (was `_internal.js:41373`, `:41309`) |
| §3 `tu`/`Dl` replaces exactly NBSP + narrow NBSP | **Yes** | `Tl = /\u00A0|\u202F/g` at `app/own/0358-Tl.js:2` |
| §4 attachment truncation is 250 **UTF-16 code units** | **Yes** | `e = DD(Nl(e)).slice(0, 250)` at `app/own/1129-UD.js:575` (was `Vault.js:908-911`) |
| §5 all `CachedMetadata` offsets are UTF-16 code units | **Yes** (structure intact) | decode-then-index in the metadata worker; `TextDecoder().decode` also used on the read path at `app/own/1129-UD.js:375` |

Verbatim, for the two most load-bearing:

```js
// app/own/1129-UD.js:566-569
t.prototype.getAvailablePath = function (e, t) {
  for (var n = 1, i = Rl(e, t); this.getAbstractFileByPathInsensitive(i);) i = Rl(e + " " + n, t), n++;
  return i;
},
```

```js
// app/own/1299-KF.js:2-4
function KF(e) {
  return "md" === Fl(Al(e)) && (e = e.substring(0, e.length - 3)), e;
}
```

Note `KF`'s guard changed shape without changing behaviour: 1.12.7 compared `"md" === su(nu(e))` (extension-of-whole-string), 1.13.7 compares `"md" === Fl(Al(e))` (extension-of-basename). The 1.13.7 form is stricter for a path like `folder.md/Note` — but `resolveLinks` feeds it `getLinkpath(reference.link)` (`app/own/1306-eR.js:404`), which has already had any `#subpath` removed, so in practice the two agree.

Also unchanged, and worth keeping in the same breath: the leading-`./` case that §3 called "load-bearing" still dispatches on the **raw** config value, before normalisation.

```js
// app/own/1129-UD.js:575
i = this.getConfig("attachmentFolderPath"), r = "." === i || "./" === i, o = null,
i.startsWith("./") && (o = i.slice(2)), …          // ← prefix test on the raw string, then Nl(i) later
```

### Citation map for `domains/vault.md` §4's stale `Vault.js:*` / `FileSystemAdapter.js:*` references

The frozen doc cites a 1.12.7 tree that no longer exists. The event-table rows in §4 map as follows, so a future reader can find each claim:

| `domains/vault.md` §4 claim | 1.12.7 ref (dead) | 1.13.7 location |
|---|---|---|
| `create` from `folder-created` | `Vault.js:440` | `app/own/1129-UD.js:256-258` |
| `create` from `file-created` | `Vault.js:446` | `app/own/1129-UD.js:259-261` |
| `modify`; skipped cache flush when `file.saving` | `Vault.js:451` | `app/own/1129-UD.js:262-264` |
| `delete` from `file-removed`/`folder-removed` | `Vault.js:458` | `app/own/1129-UD.js:265-269` |
| `rename` from `renamed` | `Vault.js:468` | `app/own/1129-UD.js:270-274` |
| `closed` | `Vault.js:472` | `app/own/1129-UD.js:275` |
| `raw` | `Vault.js:474` | `app/own/1129-UD.js:275` |
| `config-changed` on `setConfig` | `Vault.js:290` | `app/own/1129-UD.js:156-157` |
| `config-changed` on reload (changed / deleted) | `Vault.js:163` / `:167` | `app/own/1129-UD.js:87` / `:88` |
| `renamed` per-child on folder rename | `FileSystemAdapter.js:595`, `:617` | `app/own/0376-_l.js:408`, `:413` |
| `getNewFileParent` reads `registerFileParentCreator` | `FileManager.js:510` | `app/own/1312-hR.js:304-305` |
| `processFrontMatter` | `FileManager.js:1622` | `app/own/1312-hR.js:888` |
| `notifyForBulkUndo` | `FileManager.js:1693` | `app/own/1312-hR.js:926` |
| `setFileCacheLimit` | `Vault.js:188` | `app/own/1129-UD.js:97` |

Two other dead references worth flagging for whoever maintains the doc: §1's "Source: `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/vault/`" and §12's "`MarkdownRenderer.js:28-29`" both point outside the corpus that now exists.

## Why noticed now

The 2026-06-10 addendum was written against a 1.12.7 corpus that has since been destroyed, so its citations were themselves unverifiable — the addendum that exists specifically to restore citability had lost it. Re-verifying was a precondition for trusting the `getAvailablePath` and 250-unit claims while implementing against them, and the symbol renames (`BL` → `KF`, `uu`/`tu` → `Bl`/`Dl`, `lu` → `Rl`) needed recording so nobody re-derives them from scratch.

## Action taken

- Every claim in the 2026-06-10 addendum re-checked against 1.13.7 and re-anchored to the restored corpus; nothing regressed, so no new correction was needed.
- Corbomite's side was spot-checked and needs no change: `collisionFreeName` already starts at `int n = 1` with a comment citing the correction (`libs/vault/src/FileManager.cpp:600-617`), resolving the "divergent" note in the 2026-06-10 addendum's §1 implementation-impact paragraph.
- One residual Corbomite divergence found while verifying §3 of that addendum, recorded as an Obsidian-side fact in
  [`2026-10-03-vault-path-validation-and-filenames.md`](2026-10-03-vault-path-validation-and-filenames.md): `createNewMarkdownFileFromLinktext` leaves the parent null (⇒ vault root) when the linktext contains a `/`, so `newFileLocation` is bypassed for nested linktexts. `libs/vault/src/FileManager.cpp:648-653` currently always consults `getNewFileParent`.
