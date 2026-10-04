# The wikilink parser returns `{href, title, isAlias}`, NFC-folds every target, and derives a `>`-separated breadcrumb as the default display text

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/parsing.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/parsing.md` §1 (`parseLinktext`), §8; `VAULT-FORMAT.md` §4.4 (wikilink grammar) and §9 (NFC placement)
**Relevant cluster plans:** `libs/storage` link resolution / `LinkResolver`, `SQLiteIndex` wikilink extraction, `libs/vault/FileManager` rename refactoring

## Finding

### CORRECTION 1 — `parseLinktext` is two different functions, and neither is the one that parses a wikilink

§1 describes `parseLinktext(linktext)` as returning `{ path, subpath }`. That function exists and
§1's description of it is exactly right:

```js
// app/own/1113-wD.js:2-8
function wD(e) {
  var t = e.indexOf("#");
  return {
    path: -1 === t ? e : e.substring(0, t),
    subpath: -1 === t ? "" : e.substring(t)
  };
}
```

But `wD` is not what the markdown parser calls on a `[[…]]`. That is `$E`, and it returns a
completely different shape:

```js
// app/own/0937-$E.js:2-11
function $E(e) {
  var t = "",
    n = e.indexOf("|"),
    i = n > 0;
  return i ? (t = e.substring(n + 1).trim(), e = e.substring(0, n).trim()) : t = QE(e = e.trim()), e.endsWith("\\") && (e = e.substring(0, e.length - 1)), {
    href: e = GE(e),
    title: t,
    isAlias: i
  };
}
```

called from the inline `ilink` rule:

```js
// app/own/1083-expressionstatement.js:35-46   (worker twin: worker/own/0157-expressionstatement.js:35-46)
  OE(e, "ilink", "link", function(e, t, i) {
    var r = n.exec(t);
    if (r) {
      var o = r[2].trim();
      if (-1 !== o.indexOf("[[")) return !1;
      if (i) return !0;
      var a = ("0" + o).substring(1),
        s = "!" === r[1],
        l = $E(a),
        c = l.href,
        u = l.title,
        h = l.isAlias;
```

So §1's claim — *"`parseLinktext` does NOT split `[[target|display]]` — the `|` is stripped upstream
by the markdown parser before `parseLinktext` is called"* — is literally true of `wD` and
substantively misleading: the `|` split **is** the wikilink parser's job, and everything after it
(alias trimming, the trailing-backslash escape, NFC folding, the display-text default) is
invisible from `wD`'s signature. A port that implements only `wD` and calls it from the wikilink
rule loses all five behaviours below. `VAULT-FORMAT.md` §4.4 inherits the same framing ("The `|`
alias-split happens in the markdown parser before `parseLinktext` sees the text").

### CORRECTION 2 (HIGH — affects what resolves, and what lands in the cache) — every link target is NFC-normalised and NBSP-folded **at parse time**

```js
// app/own/0933-GE.js:2-4
function GE(e) {
  return (t = e, t.replace(_E, " ")).trim().normalize("NFC");
  var t;
}
// app/own/0931-_E.js:2
var _E = /\u00A0/g;
```

(worker's identical pair: `worker/own/0010-b.js:2-5`, `worker/own/0009-v.js:2`.) Executing the
verbatim expression:

| input | `GE(input)` |
|---|---|
| `"A\u00A0B"` | `"A B"` — U+00A0 becomes a plain space |
| `"cafe\u0301"` (NFD) | `"caf\u00e9"` (NFC) |
| `"A\tB"` | `"A\tB"` — tabs are untouched |

`GE` is applied at **five** sites, i.e. to every link form Obsidian recognises as internal:

- wikilinks and wikilink-embeds — `app/own/0937-$E.js:7`;
- relative markdown links — `app/own/1083-expressionstatement.js:440` (`var o = GE(r)`) after
  `decodeURI`), worker twin `worker/own/0157-expressionstatement.js:440`;
- relative markdown image embeds — `app/own/1083-expressionstatement.js:468` (`GE(KE(i))`), worker
  twin `worker/own/0157-expressionstatement.js:468`;
- frontmatter links — `app/own/1110-gD.js:5` (`e.link = GE(e.link);`);
- embed resolution at render time — `app/own/1796-K$.js:116`, `app/own/2075-Vie.js:1742`.

**But the resolver never folds.** Both halves of linkpath normalisation are fold-free:

```js
// app/own/1112-bD.js:2-5      cut at the first "#"
function bD(e) {
  var t = e.indexOf("#");
  return -1 === t ? e : e.substring(0, t);
}
// app/own/1299-KF.js:2-4      strip ".md" when the basename's extension is md
function KF(e) {
  return "md" === Fl(Al(e)) && (e = e.substring(0, e.length - 3)), e;
}
```

and the destination lookup is case-insensitive but not Unicode-normalising:

```js
// app/own/1306-eR.js:91-97, 111-113
  t.prototype.getLinkpathDest = function(e, t) {
    if ("" === e && t && (f = this.vault.getAbstractFileByPath(t)) instanceof GD) return [f];
    var n = e.toLowerCase(),
      i = Al(n),
      r = null;
    if (i.contains(".") && (r = this.uniqueFileLookup.get(i)), r || (i = Al(n = (e + ".md").toLowerCase()), r = this.uniqueFileLookup.get(i)), !r) return [];
…
    for (var l = 0, c = r; l < c.length; l++) {
      if ((v = (f = c[l]).path.toLowerCase()) === n) return [f];
    }
```

`toLowerCase()` does not change NFD→NFC. So the fold is applied **once, on the way in**, and a
stored path that is not already in NFC cannot match a folded target. Two consequences:

1. `VAULT-FORMAT.md` §9 places NFC normalisation at "every path handed into the vault API". It is
   actually in the **link parser**, and the vault/resolver API does not do it. A linktext supplied
   programmatically (`getLinkSuggestions`, a plugin, the rename refactorer) is never folded.
2. A vault containing an NFD filename on a case-sensitive filesystem will not resolve links to it
   after a parse, because the parser folds the written target but the stored path is not folded.

Corbomite must fold link targets in the parser and **not** assume the resolver will do it for it.

### NEW — the default display text for a subpath link is a `>`-separated breadcrumb, and it lands in `CachedMetadata`

```js
// app/own/0936-QE.js:2-6
function QE(e) {
  return e.split("#").filter(function(e) {
    return !!e;
  }).join(" > ").trim();
}
```

When there is no alias, `$E` uses `QE` as the title (`app/own/0937-$E.js:6`), and the cache builder
copies that title straight into `displayText`:

```js
// worker/own/0169-expressionstatement.js:69-79   (app twin: app/own/1103-uD.js)
          } else if ("ilink" === r.type) {
            var f = e.substring(r.position.start.offset, r.position.end.offset),
              h = r.href || "";
            !(m = r.title || "") && r.converted && (m = gr(r));
            var d = {
              position: vr(r.position),
              link: h,
              original: f,
              displayText: m
            };
            t && delete d.original, n.links.push(d);
```

Executed `QE` results: `"Note#H1#H2"` → `"Note > H1 > H2"`; `"#heading"` → `"heading"`;
`"a##b"` → `"a > b"`; `"#"` → `""`. So for `[[Note#H1#H2]]`:

- `CachedMetadata.links[i].link` = `"Note#H1#H2"` (subpath retained),
- `CachedMetadata.links[i].original` = `"[[Note#H1#H2]]"`,
- `CachedMetadata.links[i].displayText` = **`"Note > H1 > H2"`**.

`displayText` is written **unconditionally** — §2's `displayText?` optionality is wrong; the only
way to get `""` is a genuinely empty title (`[[Note|]]`, or `[[#]]`). The `gr(r)` fallback fires
only for `converted` nodes, i.e. converted markdown links with no title
(`worker/own/0169-expressionstatement.js:72`).

For an **aliased** link the breadcrumb is not the display text but the ARIA label, computed from the
target rather than the alias:

```js
// app/own/1083-expressionstatement.js:69
      return h && (p["aria-label"] = QE(c), p["data-tooltip-position"] = "top"), e(r[0])({
```

### NEW — three wikilink-interior edge cases the grammar in `VAULT-FORMAT.md` §4.4 does not cover

All verified by executing `$E` verbatim:

| Written | `href` | `title` | `isAlias` | Note |
|---|---|---|---|---|
| `[[Note\|Alias\|Extra]]` | `Note` | `Alias\|Extra` | `true` | only the **first** `\|` splits |
| `[[\|foo]]` | `\|foo` | `\|foo` | `false` | `indexOf("\|") > 0` is **strictly** greater, so a leading `\|` is part of the target and the link points at a file literally named `\|foo` |
| `[[Note\]]` | `Note` | `Note\` | `false` | a single trailing `\` is stripped from the href only (the title keeps it) — the escape hatch for a target ending in `]` |

Plus one rejection rule: **a wikilink whose interior contains `[[` is not a link at all**, it falls
through to ordinary text (`app/own/1083-expressionstatement.js:39`,
`if (-1 !== o.indexOf("[[")) return !1;`).

The full inline rule, for reference:

```js
// app/own/1083-expressionstatement.js:33-34
  var t = /!?\[\[/g,
    n = /^(!?)\[\[(.+?)]]/;
```

`(.+?)` is lazy and the pattern is **not** `$`-anchored, so `]]` closes the link at the first
possible position; the locator (`app/own/1083-expressionstatement.js:83-87`) uses the global
`/!?\[\[/g` to skip ahead to the next opener.

### CONFIRMED

| Claim (`domains/parsing.md` / `VAULT-FORMAT.md` §4.4) | 1.13.7 location | Verdict |
|---|---|---|
| `wD`-style split at the **first** `#`, `subpath` keeps the `#`, `"Note"` ⇒ `subpath: ""` | `app/own/1113-wD.js:2-8` | **CONFIRMED** |
| Wikilink and embed share one interior parser; only the leading `!` differs | `app/own/1083-expressionstatement.js:42-63` (`s = "!" === r[1]` → `iembed` node with `className: "internal-embed"`) | **CONFIRMED** |
| A wikilink produces an `<a class="internal-link">` with `href` and `data-href` set to the target | `app/own/1083-expressionstatement.js:64-68` | **CONFIRMED** |
| The alias split trims both sides | `app/own/0937-$E.js:6` | **CONFIRMED**, and it is `$E`, not "upstream of `parseLinktext`" |
| `[[#Heading]]` is a pure subpath with empty target | `app/own/1306-eR.js:92` — `if ("" === e && t && …) return [f];` resolves the empty linkpath to the source file itself | **CONFIRMED** |
| Markdown links are only internalised when relative; absolute URLs (anything containing `:`) are left alone | `app/own/0928-WE.js:2-4` (worker twin `worker/own/0008-y.js:2-4`) | **CONFIRMED** — already recorded by `2026-10-03-metadata-worker-cached-metadata-shape.md`; re-verified here because `$E`'s input path depends on it |
| A markdown link whose href fails `decodeURI` is **not** converted (stays a plain link, absent from the cache) | `app/own/1083-expressionstatement.js:435-440` — `try { r = decodeURI(i); } catch (e) { return; }` | **CONFIRMED** for links; note the *image* transformer uses the swallowing `KE` instead (`app/own/0932-KE.js:2-8`), so a bad percent-encoding drops a link but not an image |

## Why noticed now

The wikilink grammar is the one parsing contract that decides **which file a link points at**, and
it is the one §14 of the audit itself nominated for promotion to `VAULT-FORMAT.md` as a normative
BNF. Reading the actual rule showed the grammar has three pre-normalisation steps (alias split,
backslash escape, Unicode fold) that sit *before* the `path`/`subpath` split the doc documents, and
that one of them is not idempotent with respect to the filesystem. A BNF written from `parsing.md`
alone would have omitted all of it.

## Action taken

No Corbomite code touched. Recorded as requirements for the wikilink/embed extractor and the link
resolver:

- Parse the interior in this order: `indexOf("|") > 0` alias split (trim both sides) → trailing
  single-`\` strip → NBSP-fold + trim + **NFC** → *then* the `wD` `path`/`subpath` split. The
  resolver must not be relied on to fold.
- Reject an interior containing `[[` outright rather than producing a link.
- Store `displayText` unconditionally, defaulting to the `>`-joined breadcrumb
  (`QE`) for non-aliased links, and the link's own child text only for converted markdown links.
- `[[|foo]]` targets a file named `|foo`; `[[a|b|c]]` displays `b|c`.

`VAULT-FORMAT.md` §4.4's grammar block needs the three edge cases and the five-step normalisation
order; §9's NFC placement is wrong (parser, not vault API). Both are Pass-3 docs and are left
unedited per the append-only rule.
