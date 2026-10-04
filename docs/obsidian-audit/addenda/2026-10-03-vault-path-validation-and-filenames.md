# Exact path-validation character sets, filename splitting, and attachment-name sanitisation

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/vault.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`).
**Supersedes / extends:** `domains/vault.md` §1 (`normalizePath`, `TFile`, `getAvailablePathForAttachments`, `generateMarkdownLink`), §3 (`.obsidian/`), §8, §13 Q3 + Q10; `VAULT-FORMAT.md` §1.
**Relevant cluster plans:** `libs/core/PathNormalize`, `libs/vault/FileManager`, attachment-import / paste-image work.

## Finding

### NEW (audit open question 3, answered): `checkPath` / `validateConfigDir` character sets are **platform-dependent**, and much narrower than "illegal filename chars" implies

The audit asked for "Regexes `UT` / `WT` / `GT` / `KT` … Exact character sets are required for cross-tool path compat." The real definitions:

```js
// app/own/1126-FD.js:51-56
RD = "#^[]|",
ND = Jp ? '*"\\/<>:|?' : "\\/:" + (nd ? '*?<>"' : ""),
BD = ND.split("").join(" "),
VD = RD.split("").join(" "),
HD = new RegExp("[" + jd(ND) + "]"),      // illegal filename characters
zD = new RegExp("[" + jd(RD) + "]"),      // characters unsafe in links
qD = /^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$/i;   // Windows reserved device names
```

with `Jp = "win" === platform` and `nd = /android/i.test(userAgent)` (`app/own/0519-Qp.js:13`, `:16`).

```js
// app/own/1128-jD.js:2-13   ← Vault.checkPath
function jD(e) {
  if (Jp) {
    var t = e.charAt(e.length - 1);
    if ("." === t || " " === t) throw new Error("File names cannot end with a dot or a space.");
    var n = Ll(e);
    if (qD.test(n)) throw new Error("File name is forbidden: " + n);
  }
  if (e.split("/").some(function (e) { return HD.test(e); }))
    throw new Error("File name cannot contain any of the following characters: " + BD);
}
```

Effective rules:

| | Linux / macOS | Windows | Android |
|---|---|---|---|
| Illegal path characters | `\ / :` | `* " \ / < > : \| ?` | `\ / : * ? < > "` |
| Trailing `.` or space | allowed | rejected | allowed |
| Reserved device name as final segment | allowed | rejected (`CON PRN AUX NUL COM1-9 LPT1-9`, case-insensitive, whole-segment only) | allowed |

Two consequences worth stating explicitly:

- The reserved-name and trailing-dot/space checks are **Windows-only**. A vault authored on Linux can legitimately contain `Note ` (trailing space) or a file literally named `CON`; Obsidian on Windows will refuse to touch it, and a cross-tool scanner must not reject it.
- The illegal-character test is applied **per `/`-delimited segment**, so the `/` in the character class is never actually exercised after `normalizePath` has run — `checkPath` is always called on a normalised path (`app/own/1129-UD.js:308`, `:323`, `:338`, `:429`, `:540`). In practice the load-bearing characters are `\ :` on POSIX and `: * ? " < > |` on Windows.

`validateConfigDir` tests the **whole** configDir name against the same `HD`, not per segment:

```js
// app/own/1129-UD.js:105-108
t.validateConfigDir = function (e) { return "." !== e && e.startsWith(".") && !HD.test(e); };
t.prototype.setConfigDir = function (e) { t.validateConfigDir(e) || (e = UD), this.configDir = e; };
```

so a custom config dir must be a single dot-prefixed segment with no `\` or `:` (POSIX) — it cannot be nested. Invalid values silently revert to `.obsidian`.

> Naming trap for future readers: the audit's `UT` is, in 1.13.7, the *frontmatter terminator* regex `/---(\r?\n|$)/g` (`app/own/1084-jT.js:3`) used by the frontmatter locator `app/own/1085-_T.js:2`. The configDir/path-safety regex the audit meant is `HD`. Also, `qD` is anchored (`/^(…)$/i`) so only an exact final-segment match is reserved — `COM0`, `COM10`, and `CON.txt` are not.

The rename/folder-creation modals layer two more rules on top of `checkPath`, neither of which is enforced by `Vault.create`:

```js
// app/own/1312-hR.js:76-88, validate at `:82`   (new-folder modal)
return HD.test(e) ? msgInvalidCharacters() + BD
     : e.startsWith(".") ? msgBadDotfile()
     : zD.test(e) ? msgUnsafeCharacters() + VD : "";
// app/own/1312-hR.js:92-124, validate at `:98-108`  (rename-file modal) additionally rejects "" and checkForDuplicate(file, name)
```

`zD` (`[#^[]|]`) is the "unsafe in a link" set: these characters are rejected in a *rename* because the name must remain linkable. `FileManager.createNewFile` does **not** apply either the dot-prefix or `zD` rule — only `createNewMarkdownFileFromLinktext` runs `checkPath` (`app/own/1312-hR.js:314`).

### NEW: `basename` / `extension` splitting — dotfiles have **no** extension, and a trailing dot yields none either

```js
// app/own/0365-Fl.js:2   extension
function Fl(e) { var t = e.lastIndexOf("."); return -1 === t || t === e.length - 1 || 0 === t ? "" : e.substr(t + 1).toLowerCase(); }
// app/own/0363-Ll.js:2   basename
function Ll(e) { var t = Al(e), n = t.lastIndexOf("."); return -1 === n || n === t.length - 1 || 0 === n ? t : t.substr(0, n); }
// app/own/0364-Ol.js:2   strip-extension (same guards)
function Ol(e) { var t = e.lastIndexOf("."); return -1 === t || t === e.length - 1 || 0 === t ? e : e.substr(0, t); }
```

The `0 === t` guard means a leading dot yields `""` for `Fl` and the **whole name** for `Ll`. So `.gitignore` → `basename === ".gitignore"`, `extension === ""`, and `getShortName()` returns the full name (`app/own/1129-UD.js:21-23`). A trailing dot (`Note.`) likewise gives `extension === ""`.

This closes audit open question 10 for the reverse direction: `Note.md.bak` → `basename "Note.md"`, `extension "bak"` (confirmed by reading `Fl`/`Ll`), and additionally `.hidden` → `basename ".hidden"`, `extension ""`.

`TFile.getNewPathAfterRename` re-appends the extension from `this.extension`, not from the user's input:

```js
// app/own/1129-UD.js:23-24
t.prototype.getNewPathAfterRename = function (t) { return e.prototype.getNewPathAfterRename.call(this, Rl(t, this.extension)); }
```

(`Rl(name, ext) = ext ? name + "." + ext : name`, `app/own/0366-Rl.js:2`.) The rename modal seeds its textarea with `file.basename` (`app/own/1312-hR.js:94`, textarea seeded at `:46-48`), so the user never types the extension.

### CORRECTION: `getNewPathAfterRename` **replaces** control characters with a space; it does not strip them

`domains/vault.md` §1 says "Strips control chars `[\x00-\x1F]` and trims."

```js
// app/own/1129-UD.js:9-10
e.prototype.getNewPathAfterRename = function (e) {
  return e = e.replace(/[\x00-\x1F]/g, " ").trim(), this.parent ? this.parent.isRoot() ? e : this.parent.path + "/" + e : "";
};
```

`\t`, `\n` and friends become U+0020, then the whole string is trimmed. Stripping would fuse `a\tb` into `ab`; Obsidian produces `a b`. The audit also omits that root-level renames return the bare name (no parent prefix) and that a detached node returns `""` — the latter *is* in the doc, correctly.

### NEW: attachment basenames are sanitised by a specific character class before the 250-unit cut

```js
// app/own/1129-UD.js:575  (getAvailablePathForAttachments)
e = DD(Nl(e)).slice(0, 250)
// app/own/1120-DD.js:2
function DD(e) { return e.replace(xD, " ").replace(/\s+/g, " ").trim(); }
// app/own/1118-SD.js:3
xD = /([:#|^\\\r\n]|%%|\[\[|]])/g;
```

So `: # | ^ \ CR LF`, the pair `%%`, the pair `[[` and the pair `]]` are each replaced with a space, then runs of whitespace collapse and the result is trimmed, then the first 250 UTF-16 code units are kept. Note `%%` is Obsidian comment syntax and `|` is the wiki alias separator, so this pass exists to strip link syntax out of a pasted linktext.

The folder half of the same function, also newly citable:

```js
// app/own/1129-UD.js:575
i = this.getConfig("attachmentFolderPath"), r = "." === i || "./" === i, o = null,
i.startsWith("./") && (o = i.slice(2)),
r ? i = n ? n.parent.path : ""
  : o && (i = (n ? n.parent.getParentPrefix() : "") + o),
i = Nl(i), ...
```

The `.`/`./` branch uses `n.parent.path` (so a root-level note yields `"/"`, which `normalizePath` preserves and which then resolves to the root folder's `getParentPrefix() === ""`), while the `./sub` branch uses `getParentPrefix()`. With the **default** `attachmentFolderPath` of `"/"` neither branch applies and attachments land in the vault root.

### NEW: `createNewFile` strips a redundant extension; a linktext containing `/` bypasses `newFileLocation`

```js
// app/own/1312-hR.js:330
r = this.vault, e || (e = r.getRoot()), n || (n = t && Fl(t) || "md"),
this.fileParentCreatorByType.hasOwnProperty(n) || (n = "md"),
o = e.getParentPrefix(),
t ? (t = t.trim(), p = n, Fl(t) === p && (t = Ol(t)), t = Nl(t), a = r.getAvailablePath(o + t, n))
  : (s = labelUntitledFile(), a = r.getAvailablePath(o + s, n)),
l = Pl(a), c = Al(a), [4, this.getOrCreateFolder(l)];
```

```js
// app/own/1312-hR.js:313-315   (createNewMarkdownFileFromLinktext)
if (!WD(e)) throw new Error(msgInvalidCharacters() + BD);
return n = null, e.contains("/") || (n = this.getNewFileParent(t, e)), [2, this.createNewMarkdownFile(n, e)];
```

- Passing `"Note.md"` with `ext = "md"` produces `Note.md`, not `Note.md.md` — the extension is stripped when it already matches.
- `createNewMarkdownFileFromLinktext` leaves `parent` **null** (→ vault root) whenever the linktext contains a `/`, so `newFileLocation` / `newFileFolderPath` are ignored for nested linktexts; the intermediate folders are then created by `getOrCreateFolder` (`app/own/1312-hR.js:337-342`, which resolves case-insensitively via `getAbstractFileByPathInsensitive`).
- `getNewFileParent(hint, filename)` derives the extension from the **filename**, not from a separate argument, and falls back to the `md` creator with a console error if unregistered (`app/own/1312-hR.js:300-305`, error text at `:303`); `createNewFile` instead coerces an unregistered ext to `"md"` **silently** (`:330`).

### NEW: the markdown-link percent-encoding set is small and specific

```js
// app/own/0934-YE.js:2
function YE(e) { return e.replace(/[\\\x00\x08\x0B\x0C\x0E-\x1F ]/g, function (e) { return encodeURIComponent(e); }); }
```

Only backslash, NUL, BS, VT, FF, SO–US, and the **space** are encoded. `#`, `?`, `%`, `[`, `]`, `(`, `)`, `;` and every non-ASCII character pass through untouched — so a subpath-bearing link becomes `[Note](Note#Heading)` with a literal `#`, and a space becomes `%20`. This backs the audit's "percent-encoded" claim with a citable set.

## Why noticed now

Open question 3 has been open since the audit was written and blocks any claim that a Corbomite path layer is format-compatible. Restoring the corpus made `HD`/`ND`/`RD`/`qD` readable — and the answer is platform-conditional, which no amount of black-box probing on a single Linux machine would have revealed. The `saving`-counter and `.gitignore`-extension facts fell out of the same read and are recorded in the adapter addendum; the filename-splitting rules were needed to verify `getShortName` and `createNewFile`'s extension handling.

## Action taken

- Open questions 3 and 10 closed, with the platform-conditional table above as the normative statement of `checkPath`/`validateConfigDir`.
- `Il`-independent path rules (illegal chars, reserved names, trailing dot/space, configDir single-segment) recorded for `libs/core/PathNormalize` and the Corbomite rename/new-folder dialogs.
- The `/…/`-vs-bare two-form grammar for rename rejection recorded so the Corbomite rename dialog does not silently accept a name Obsidian will refuse.
- Recorded that `createNewFile`/`createNewMarkdownFileFromLinktext` do **not** enforce the dot-prefix or `zD` rules, so a Corbomite implementation adding those to the *creation* path would be stricter than Obsidian.
