# Audit addendum — vault path/naming corrections (collision suffix, unresolvedLinks casing, normalizePath, 250-unit truncation, UTF-16 offsets)

**Corrects:** `domains/vault.md` §1/§8, `domains/metadata.md` §2/§8, `VAULT-FORMAT.md` §1/§2/§4.9/§7/§8.

**Date:** 2026-06-10
**Discovered during:** verification pass of audit claims against the decompiled source.
**Source:** decompiled Obsidian 1.12.7 corpus at `/home/clinton/bin/ObsidianRAW/audit/` (paths below are relative to `renamed/obsidian/` unless noted). All claims below were re-checked against that corpus on 2026-06-10.

These are **refutations** of main-doc claims, not additive facts. Main docs stay frozen; read this file before implementing from the sections named above.

## 1. `getAvailablePath` collision suffix starts at ` 1`, not ` 2`

**Wrong claim:** vault.md §1 — "`getAvailablePath(pathNoExt, ext)` — appends ` 2`, ` 3`, … until `getAbstractFileByPathInsensitive` is null." Repeated in VAULT-FORMAT.md §2 — "`getAvailablePath(pathNoExt, ext)` appends ` 2`, ` 3`, … on collision."

**Verified reality:** the counter initialises to **1** and is used before increment:

```js
// tree/obsidian/vault/Vault.js:884-890
(t.prototype.getAvailablePath = function (e, t) {
  for (var n = 1, i = lu(e, t); this.getAbstractFileByPathInsensitive(i); )
    ((i = lu(e + " " + n, t)), n++);
  return i;
}),
```

First collision produces `Untitled 1.md`, then `Untitled 2.md`, etc.

Only the **`.trash/` collision naming** starts at 2: `trashLocal` (`tree/obsidian/vault/FileSystemAdapter.js:185-225`) initialises `a = 1` (`:208`) but increments **before** building the candidate name (`a++` then `r + " " + a + i`, `:213-215`), so the first trash collision gets suffix ` 2`. vault.md §8 / VAULT-FORMAT.md's `.trash/` description (`""`, ` 2`, ` 3`, …) is therefore correct — the error is only in `getAvailablePath`.

**Implementation impact:** Corbomite's `collisionFreeName` (`libs/vault/src/FileManager.cpp:591`) starts at `int n = 2` — **divergent**. "Untitled 1" is the Obsidian-parity behaviour for new-file/copy/attachment naming; ` 2`-first is correct only for `.trash/`.

## 2. `unresolvedLinks` keys are NOT lowercased

**Wrong claim:** metadata.md §2 — `BL(linkpath)` described as "lowercase-normalise + strip heading … expect lowercase keys"; metadata.md §8 — "`unresolvedLinks` keys are normalised via `BL(getLinkpath(linktext))` — lowercased, heading/block-id stripped. Do not expect original casing to survive."

**Verified reality:** `BL` only strips a trailing `.md` extension; it never lowercases:

```js
// src/_internal.js:261141-261143
function BL(e) {
  return ("md" === su(nu(e)) && (e = e.substring(0, e.length - 3)), e);
}
```

(`su` lowercases the extension **only for the comparison**; the returned string is the original.) Call path: `resolveLinks` (`tree/obsidian/metadata/MetadataCache.js:646-662`) does `getLinkpath(r.link)` (strips `#subpath`) then `var s = BL(o); i[s] = (i[s] || 0) + 1` (`:657`) and assigns `this.unresolvedLinks[e] = i` (`:661`). Original casing survives: `[[Foo Bar]]` → key `"Foo Bar"`, not `"foo bar"`.

**Implementation impact:** a port that lowercases these keys diverges from the plugin-visible contract (`app.metadataCache.unresolvedLinks`) and from graph-view/orphan semantics — two unresolved links differing only in case are **distinct** keys in Obsidian.

## 3. `normalizePath` internals: no `./` stripping; leading slashes ARE stripped; `tu` is NBSP-only

**Wrong claims:** vault.md §1 — "`uu(s)` … trims trailing slashes except at root, **removes leading `./`**" and "`tu(s)` — strips some low-ASCII / line-separator chars (inferred from callers)". vault.md §8 — "NFC + `\\`→`/` + collapse-slashes + trim + strip-leading-`./`". VAULT-FORMAT.md §1 — "trailing-slash trim (except root), leading `./` strip".

**Verified reality:**

```js
// src/_internal.js:41373-41380
function uu(e) {
  return (
    "" === (e = e.replace(/([\\/])+/g, "/").replace(/(^\/+|\/+$)/g, "")) &&
      (e = "/"),
    e
  );
}
// src/_internal.js:41309, 41313-41315
var eu = / | /g;
function tu(e) {
  return e.replace(eu, " ");
}
```

- `uu` has **no `./` handling at all** — `normalizePath("./foo")` returns `"./foo"` (the leading `.` is not a slash, so neither regex touches it).
- `uu` **does strip leading slash runs** (`^\/+` branch) in addition to trailing ones — `"/foo/bar/"` → `"foo/bar"`. The docs describe only the trailing trim.
- `tu` replaces exactly two characters — NBSP `U+00A0` and narrow NBSP `U+202F` — with a regular space. It does **not** touch low-ASCII or line-separator characters.

**Implementation impact:** Corbomite's `normalizePath` port must *not* strip `./` — this is load-bearing because `getAvailablePathForAttachments` detects the `attachmentFolderPath` form `"./sub"` by prefix check **before** normalisation (`tree/obsidian/vault/Vault.js:902`); a normaliser that eats `./` would corrupt that dispatch if applied upstream. Leading-slash stripping matters when normalising rooted link inputs (`[[/Note]]`).

## 4. Attachment-name truncation is 250 UTF-16 code units, not 250 bytes

**Wrong claim:** VAULT-FORMAT.md §4.9 — "truncates the basename to **250 bytes** (UTF-8)"; repeated in §8 ("truncated to 250 bytes") and sourced from vault.md §13 Q11. (VAULT-FORMAT.md §2 says "250 chars", which is closer but ambiguous.)

**Verified reality:** `getAvailablePathForAttachments` (`tree/obsidian/vault/Vault.js:892-922`) does

```js
// tree/obsidian/vault/Vault.js:908-911
(e = stripHeadingForLink(normalizePath(e)).slice(0, 250)),
```

`String.prototype.slice(0, 250)` on a JS string counts **UTF-16 code units**. A 100-character CJK basename (300 UTF-8 bytes) is *not* truncated; an emoji at index 249/250 is split mid-surrogate-pair.

**Implementation impact:** the C++ equivalent is `QString::left(250)` (QString is also UTF-16 code-unit indexed) — *not* a UTF-8 byte-length cap. Matching the surrogate-split edge exactly is what byte-faithful parity means here, ugly as it is.

## 5. General note — ALL CachedMetadata/`loc` offsets are UTF-16 code units, not bytes

Several docs describe `CachedMetadata` position fields as byte offsets: metadata.md §2 ("Offsets are 0-based **byte** offsets; lines/cols 0-based"), VAULT-FORMAT.md §4.1 (`FrontMatterInfo.from/to/contentStart` annotated "byte offset of …").

**Verified reality:** the metadata worker receives raw bytes but immediately decodes them — `new TextDecoder().decode(e)` (`unbundled/worker/deobfuscated.js:18128`) — and every subsequent offset is advanced by JS string indexing (`o.offset++` per code unit, e.g. `deobfuscated.js:186`). JS strings are UTF-16; therefore **every `loc`/`pos`/`from`/`to`/`offset` in `CachedMetadata`, `FrontMatterInfo`, and the section/heading/link/block position records is a UTF-16 code-unit index into the decoded document**. For pure-ASCII files these coincide with byte offsets, which is why the audit's spot checks never caught it; any non-ASCII character (≥ U+0080) breaks the equivalence, and astral-plane characters (emoji) count as 2 units.

**Implementation impact:** load-bearing for all C++ offset arithmetic. Corbomite code that indexes file bytes (`QByteArray`) with cached offsets corrupts positions on any non-ASCII note; offsets must be applied to the decoded `QString` (same UTF-16 unit space) or converted explicitly. Every audit-doc occurrence of "byte offset" should be read as "UTF-16 code-unit offset".
