# `getFrontMatterInfo` offsets are UTF-16 code units, the writer and the indexer run **different** frontmatter scanners, and existing `---` delimiters are never re-normalised

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/parsing.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/parsing.md` §1 (`getFrontMatterInfo` bullets, `parseFrontMatterAliases`, `processFrontMatter`), §2 `FrontMatterInfo`, §3 (frontmatter wire format), §8; `VAULT-FORMAT.md` §4.1 `FrontMatterInfo` block and §4.2 "CRLF" paragraph and §4.3; re-verifies §5 of `addenda/2026-06-10-vault-path-and-naming-corrections.md`
**Relevant cluster plans:** `superpowers/plans/2026-08-20-cluster-p-shared-libraries.md` (a shared frontmatter reader/writer is the natural home), `libs/vault/FileManager` property-edit work; any future `processFrontMatter` port

## Finding

### CORRECTION 1 (HIGH — implement against this) — the four offsets are UTF-16 **code units**, not bytes

§1 calls `from`/`to`/`contentStart` "byte offset" three times each, and §2's `FrontMatterInfo`
typescript block repeats it in three comments. `VAULT-FORMAT.md` §4.1 inherited the same wording.
They are UTF-16 code units, because they index a JS string:

```js
// app/own/1085-_T.js:2-29   (getFrontMatterInfo)
function _T(e) {
  if (jT.lastIndex = 0, !jT.exec(e)) return {
    exists: !1, contentStart: 0, from: 0, to: 0, frontmatter: ""
  };
  var t = jT.lastIndex;
  UT.lastIndex = t;
  for (var n = UT.exec(e); n && "\n" !== e.charAt(n.index - 1);) n = UT.exec(e);
  if (!n) return {
    exists: !1, contentStart: 0, from: 0, to: 0, frontmatter: ""
  };
  var i = n.index,
    r = UT.lastIndex;
  return {
    exists: !0,
    frontmatter: e.slice(t, i),
    from: t,
    to: i,
    contentStart: r
  };
}
```

Every value comes from a `RegExp` match index or `String.prototype.lastIndex`/`slice` on `e`,
which is the decoded document text. The same unit is used by the splice that consumes them:

```js
// app/own/1313-wR.js:2-7   (the body of processFrontMatter)
function wR(e, t) {
  var n = _T(e),
    i = n.exists ? yL(n.frontmatter) : {};
  if (i && "object" == typeof i || (i = {}), t(i), 0 === Object.keys(i).length) return n.exists ? e.slice(n.contentStart) : e;
  var r = bL(i);
  return n.exists ? e.slice(0, n.from) + r + e.slice(n.to) : "---\n" + r + "---\n" + e;
}
```

This **re-verifies unchanged** §5 of the 2026-06-10 addendum ("all CachedMetadata offsets are
UTF-16 code units") and extends it to the four frontmatter offsets the audit never covered. For
ASCII files the two units coincide, which is why the audit's wording survived. In Corbomite,
`QString` is UTF-16-unit indexed, so these offsets apply directly to it — never to a `QByteArray`.

### CORRECTION 2 (HIGH — note content format) — existing `---` delimiters are **preserved verbatim**, not normalised to LF

§3 states: *"Delimiter format (write): Always `---\n` (LF only; CRLF-original files get LF
delimiters after write-back — CRLF preservation is lost)"*, and `VAULT-FORMAT.md` §4.2 repeats it
("Writing always uses `\n` — CRLF-originated files round-trip to LF on first write"). That is
wrong for a file that **already has** frontmatter. `from` and `to` bound the YAML payload only —
both delimiter lines sit outside the splice, so their bytes (including `\r\n`) survive untouched.
Only the re-emitted payload is LF, because the `yaml` stringifier joins with `\n`:

```js
// app/own/1027-Ex.js:248   (yaml Document#toString → the payload text bL returns)
      return n.join("\n") + "\n";
```

Executing the verbatim `_T` + `wR` expressions on a CRLF document (mutator = no-op, dumper
stubbed to emit `key: value\n` since only its line ending is at issue):

```
in : "---\r\ntitle: x\r\n---\r\nbody\r\n"
out: "---\r\ntitle: x\n---\r\nbody\r\n"      ← both delimiters keep \r\n; only the payload line flips
in : "---\ntitle: x\n---\nbody\n"
out: "---\ntitle: x\n---\nbody\n"            ← byte-identical
```

So the accurate rule is: **pre-existing delimiters are never rewritten; only the YAML between
them is re-serialised with LF line endings.** The `---\n`-always form in §3 is correct *only* for
the "no frontmatter before mutation" branch (`app/own/1313-wR.js:7`). A port that normalises
delimiters will rewrite every CRLF note in the vault on its first property edit.

### CORRECTION 3 (HIGH — parser behaviour changes what the indexer sees) — Obsidian has **two different frontmatter scanners**, and they disagree

`getFrontMatterInfo` uses the two regexes §1 documents (verified byte-identical):

```js
// app/own/1084-jT.js:2-3
var jT = /^---(\r?\n)/g,
  UT = /---(\r?\n|$)/g;
```

But the **markdown parser** — the one that builds `CachedMetadata`, i.e. everything the index,
tags, links, properties and the outline see — does not use them at all. It uses a hand-rolled
scanner, duplicated statement-for-statement in the app bundle and the worker bundle:

```js
// app/own/1083-expressionstatement.js:382-396   (identical at worker/own/0157-expressionstatement.js:382-396)
function(e) {
  var t = "---",
    n = function(e, n, i) {
      if (n.slice(0, 3) === t && "\n" === n.charAt(3)) {
        for (var r = n.indexOf(t, 3); - 1 !== r && "\n" !== n.charAt(r - 1);) r = n.indexOf(t, r + 3);
        return - 1 !== r ? !!i || e(n.slice(0, r + 3))({
          type: "yaml",
          value: n.slice(4, r - 1)
        }) : void 0;
      }
    };
  n.onlyAtStart = !0;
  var i = e.prototype;
  i.blockMethods.unshift("frontmatter"), i.blockTokenizers.frontmatter = n;
}
```

The differences are not cosmetic. Running both implementations side by side on the same inputs:

| Input | `_T` (`getFrontMatterInfo`, writer path) | AST rule (`type: "yaml"`, indexer path) |
|---|---|---|
| `---\ntitle: x\n---\nbody\n` | `exists:true`, payload `"title: x\n"` | `value:"title: x"` |
| `---\r\ntitle: x\r\n---\r\nbody\r\n` | **`exists:true`** | **`undefined` — not frontmatter** |
| `---\ntitle: x\n---   \nbody\n` (trailing spaces on the closing fence) | **`exists:false`** | **`value:"title: x"`** |
| `---\na: 1\n---b\n` (body glued to the closing fence) | **`exists:false`** | **`value:"a: 1"`**, body becomes `b` |
| `---\ntitle: x\n---` (closing fence at EOF, no trailing newline) | `exists:true`, `contentStart:16` | `value:"title: x"` |
| `---\n---\nbody` (empty block) | `exists:true`, `frontmatter:""`, `from===to===4` | `value:""` |
| `\n---\n…`, `\uFEFF---\n…` | `exists:false` | `undefined` |

Three consequences a port must reproduce:

1. **A CRLF note's frontmatter is invisible to the indexer but visible to the property editor.**
   `n.charAt(3) === "\n"` rejects `---\r\n` outright, while `jT`'s `\r?\n` accepts it. So on a
   CRLF vault, `CachedMetadata` has no `frontmatter` and no `frontmatterPosition`, while
   `processFrontMatter` happily rewrites the block. The first property edit therefore *creates*
   frontmatter that the indexer had never seen.
2. **A closing fence with trailing text or spaces is the mirror image**: the indexer accepts it,
   the writer does not, so a property edit on such a file *prepends a second frontmatter block*
   (`"---\n" + r + "---\n" + e`, `app/own/1313-wR.js:7`) in front of the one the parser found.
3. **The payload strings differ.** `_T`'s `frontmatter` retains the newline that precedes the
   closing fence (`"title: x\n"`); the AST `value` drops it (`n.slice(4, r - 1)`). Both parse to
   the same YAML, but a byte-comparison test against either will fail.

§8's closing-fence invariant ("requires that the character immediately before the match is a
newline") and the all-zero `exists === false` invariant are **confirmed** for both.

### CORRECTION 4 — `parseFrontMatterAliases` returns `[]`, not `null`, for an all-blank array

§1 claims `aliases: ["", "  "]` yields `null`. It yields `[]`:

```js
// app/own/1091-$T.js:2-9
function $T(e) {
  var t = QT(e, /^aliases$/i);
  return t ? t.map(function(e) {
    return e.trim();
  }).filter(function(e) {
    return !!e;
  }) : null;
}
```

The `? :` guard tests the array from `parseFrontMatterStringArray`, **not** the filtered result, and
`[]` is truthy. `null` comes back only when the raw value is absent or itself falsy (`aliases: ""`
→ `XT` returns `""` → `QT` returns `null` → `$T` returns `null`). `parseFrontMatterTags` has the
same shape and therefore also returns `[]` for `tags: []` or `tags: [""]`. Anything that
distinguishes "no aliases" from "aliases that were all blank" must test `length`, not truthiness.

### NEW — there is a fourth typed frontmatter helper, `cssclasses`, and a key-spelling-preserving reader

`parsing.md` §1 lists four `parseFrontMatter*` readers. There is a fifth, undocumented, with the
same shape as the tags reader minus the `#` prefixing:

```js
// app/own/1093-JT.js:2-8
function JT(e) {
  if (!e) return null;
  var t = QT(e, /^cssclasses$/i);
  return t ? t.filter(function(e) {
    return !!e && !e.includes(" ")
  }) : null;
}
```

Consumed by the live-preview and reading-mode renderers to set the note's CSS classes
(`app/own/1844-R1.js:234`, `app/own/1961-Q3.js:1185`, `:1381`) and by the properties context
(`app/own/1506-TW.js:199`). Note the space filter is `!e.includes(" ")` — **U+0020 only**, so a
class containing a tab or NBSP survives. Same for tags: §1's "tags containing a space character
(tags with spaces are invalid in Obsidian)" is precisely a literal-space test
(`app/own/1092-ZT.js:7`), not a whitespace test.

Separately, §1's `parseFrontMatterEntry` matches keys case-insensitively but always reports the
canonical lower-case name; the code that surfaces a frontmatter hit to the user re-derives the key
**as actually spelled**:

```js
// app/own/1156-IA.js:2-9
function IA(e, t) {
  if (Object.hasOwn(e, t)) return t;
  for (var n = t.toLowerCase(), i = 0, r = Object.keys(e); i < r.length; i++) {
    var o = r[i];
    if (o.toLowerCase() === n) return o;
  }
  return t;
}
```

So a search hit on a `Tags:` property is reported as `key: "Tags"` (see
`2026-10-03-parsing-tag-and-blockid-grammar.md`).

### CONFIRMED — §1's public surface, with live locations

| §1 claim (`domains/parsing.md`) | 1.13.7 location | Verdict |
|---|---|---|
| `getFrontMatterInfo` regexes `Yx = /^---(\r?\n)/g` (opening, from offset 0) and `Qx = /---(\r?\n|$)/g` (closing, must be newline-preceded) | `app/own/1084-jT.js:2-3`, used at `app/own/1085-_T.js:3,11,12` | **CONFIRMED, verbatim** — they are `jT`/`UT`, not `Yx`/`Qx`, and they belong to `getFrontMatterInfo` **only** (Correction 3) |
| `getFrontMatterInfo` requires the closing `---` to begin at a line start; mid-line `---` is skipped | `app/own/1085-_T.js:12` — `for (var n = UT.exec(e); n && "\n" !== e.charAt(n.index - 1);) n = UT.exec(e);` | **CONFIRMED** |
| `exists === false` ⇒ all numerics `0`, `frontmatter === ""` | `app/own/1085-_T.js:3-9, 13-19` | **CONFIRMED** (two identical literals) |
| `frontmatter` excludes both delimiter lines; `from`/`to`/`contentStart` bracket them | `app/own/1085-_T.js:24-27` (`e.slice(t, i)`, `from:t`, `to:i`, `contentStart:r`) | **CONFIRMED** as structure; **offsets are UTF-16 units, not bytes** (Correction 1) |
| `parseYaml` delegates with an empty options object, no custom schema, no reviver | `app/own/1218-yL.js:2-3` — `return wT(e, null, {});` | **CONFIRMED** — the YAML entry point is `wT` (`app/own/1072-wT.js:2-30`), identified as eemeli/yaml v2 by its `MULTIPLE_DOCS` error and `toJS({reviver, …})` tail |
| `parseYaml` returns `null` for empty input and throws on errors in non-silent mode | `app/own/1072-wT.js:29-32` (`if (!r) return null;`) and `:26-28` (`if ("silent" !== r.options.logLevel) throw r.errors[0];`) | **CONFIRMED** |
| YAML 1.2 by default (`yes`/`no`/`on`/`off` are not booleans) | `app/own/1054-$x.js:11-12` — `this.directives = new PM({ version: e.version \|\| "1.2" })`, and `yL` passes `{}` so nothing overrides it | **CONFIRMED** (§13 Q6 answered) |
| `stringifyYaml` returns `undefined` when the input is `undefined` and `keepUndefined` is unset | `app/own/1219-bL.js:11-16` — `if (void 0 === e) { const { keepUndefined: e } = n ?? t ?? {}; if (!e) return; }` | **CONFIRMED** |
| `stringifyYaml` options `nullStr: ""`, `lineWidth: 0`, `aliasDuplicateObjects: false` | `app/own/1219-bL.js:19-23` | **CONFIRMED, verbatim** — already recorded by `2026-10-03-vault-filemanager-content-rewrites.md`; note that addendum's phrase "js-yaml options" is a misnomer, these are eemeli/yaml v2's |
| `parseFrontMatterEntry` returns `null` on falsy `fm`, uses `hasOwnProperty`, and for a `RegExp` returns the first matching own key's value | `app/own/1089-XT.js:2-9`, with `var YT = Object.prototype.hasOwnProperty;` at `app/own/1088-YT.js:2` | **CONFIRMED** |
| `parseFrontMatterStringArray` wraps a bare string in a 1-element array, filters non-string members, `.trim()`s, and returns `null` for anything else | `app/own/1090-QT.js:2-9` | **CONFIRMED** — note it does **not** filter empty strings; that happens in the callers |
| `parseFrontMatterTags` matches `/^tags$/i` (case-insensitive, no singular fallback), drops blanks, prepends `#` | `app/own/1092-ZT.js:2-15` | **CONFIRMED** — plus NEW: the space test is U+0020 only |
| `parseFrontMatterAliases` matches `/^aliases$/i`, trims, filters empties | `app/own/1091-$T.js:3-8` | **CORRECTED** (Correction 4: `[]`, not `null`) |
| `parsePropertyId` splits at the first `.`; an unrecognised prefix makes the whole key a `note` name; no dot ⇒ `{type:"note", name:key}` | `app/own/1762-qX.js:2-13` | **CONFIRMED**, including all four worked examples |
| `parseFrontMatterTags` result is consumed by a `getAllTags` that appends inline `#tag` occurrences | `app/own/1117-MD.js:2-18` | **CONFIRMED, but order and dedupe are wrong in the doc's favour** — see `2026-10-03-parsing-tag-and-blockid-grammar.md` |
| `processFrontMatter` is a no-op for non-`.md` files | `app/own/1312-hR.js:888-896` (cited by `2026-10-03-vault-filemanager-content-rewrites.md`) | **CONFIRMED** |
| `processFrontMatter` deletes the block when the mutator empties the object, and prepends `---\n…---\n` when there was none | `app/own/1313-wR.js:5,7` | **CONFIRMED** |
| Comments in frontmatter are lost on any non-empty `processFrontMatter` | `app/own/1027-Ex.js:201` — `if (this.errors.length > 0) throw …` / no comment round-trip in `toString`; the stringifier only re-emits comments it is handed | **CONFIRMED** in substance (the dump is built fresh from the JS object at `app/own/1313-wR.js:6`) |

### Answers to §13 open questions

- **Q3 (closing `---` at EOF).** Yes — both scanners accept it: `_T` via `UT`'s `$` alternative
  (`app/own/1084-jT.js:3`) and the AST rule because the fence is still newline-preceded
  (`app/own/1083-expressionstatement.js:386`). Verified by execution: `---\ntitle: x\n---` yields
  `exists:true, contentStart:16` and `value:"title: x"`. §11's "main gap" and
  `VAULT-FORMAT.md` §4.1's **"P0 correctness bug in Corbomite"** are therefore both unfounded —
  see the next subsection.
- **Q4 (tag key casing).** CONFIRMED case-insensitive at `/^tags$/i` (`app/own/1092-ZT.js:4`).
  NEW: the reported key preserves the user's spelling via `IA` (`app/own/1156-IA.js:2-9`).
- **Q6 (YAML 1.1 vs 1.2).** CONFIRMED 1.2, with the default coming from
  `app/own/1054-$x.js:11-12` and no override anywhere on the frontmatter path.

### The "P0 correctness bug in Corbomite" does not exist

`VAULT-FORMAT.md` §4.1 flags a **P0**: *"Corbomite's `Markoff::Document::fromMarkdown` uses
`indexOf("\n---", 3)` … A file ending with bare `---` (no trailing newline) is not matched. See
GAP-ANALYSIS §P0."* That was `parsing.md` §11's guess about a `Document.cpp` line range the audit
never read, and it is wrong. Markoff's extractor already handles every shape in the table above:

```cpp
// libs/markoff-family/libs/markoff-parser/src/Document.cpp:38-64
    if (source.startsWith(QStringLiteral("---\n")) || source.startsWith(QStringLiteral("---\r\n"))) {
        int endPos = source.indexOf(QStringLiteral("\n---"), 3);
        if (endPos >= 0) { /* … */
        out.frontmatterEofClose = (afterFm >= source.size());   // :54
        /* … */
    } else if (source.endsWith(QStringLiteral("\n---"))) {
        // Opening --- with closing --- at EOF, no trailing newline.
```

It accepts both opening line endings, walks past a `\r\n` closing fence
(`if (afterFm < source.size() && source[afterFm] == QLatin1Char('\r')) ++afterFm;`), and has a
dedicated EOF-close branch (`Document.cpp:56-64`), consumed by `fromMarkdown`
(`Document.cpp:134-148`). Two things it does *not* do, which are the genuine remaining gaps, both
from Correction 3: it has only **one** scanner (so it cannot reproduce the CRLF
writer/indexer disagreement — it accepts CRLF for both, which matches the *writer* and not the
indexer), and it accepts `\n---anything` (matching `_T`, i.e. the writer, again not the indexer).
The GAP-ANALYSIS §P0 item should be closed or re-scoped to those two.

One Corbomite-side note, since it is the same wording bug as Correction 1:
`Document.cpp:13,15` label `frontmatterBlockStart`/`frontmatterBlockEnd` as "byte offset", but both
come from `QString::indexOf`/`mid` and are therefore UTF-16 code units — the values are right and
the comments are wrong.

## Why noticed now

`parsing.md` was written from an `obsidian/parsing/` tree that extracted eight exported helpers and
never opened the markdown parser. The one claim in it that changes bytes on disk — the CRLF
statement in §3 — turns out to be the visible symptom of a structural omission: there are two
frontmatter scanners in Obsidian, not one, and the audit only ever saw the one that writes. That
divergence is invisible to every functional test (frontmatter still round-trips on LF vaults) and
destructive on the two input shapes tabulated above.

## Action taken

No Corbomite code touched — `parsing.md` §11 records `getFrontMatterInfo`, `parseYaml`,
`stringifyYaml` and the whole `parseFrontMatter*` family as **Missing**, so there is nothing to
fix yet. Recorded as build requirements for whoever implements them:

- All four offsets are UTF-16 code units against the decoded `QString`.
- A note-file frontmatter **reader** and an AST frontmatter **block rule** must be implemented as
  two separate scanners matching the tabulated agreement/disagreement, not one shared helper —
  otherwise Corbomite will silently disagree with itself between the property editor and the
  index on CRLF vaults and on fences with trailing characters.
- `processFrontMatter` must splice only `[from, to)` and leave existing delimiters byte-identical.
- `parseFrontMatterAliases`/`parseFrontMatterTags` return `[]` (not `null`) for all-blank input;
  test on `length`.
- `cssclasses` must be parsed alongside `tags`/`aliases`, and the case-insensitive key lookup must
  be able to report the key as spelled.

`VAULT-FORMAT.md` §4.1 (the `FrontMatterInfo` "byte offset" comments), §4.2 (the CRLF paragraph)
and §4.3 (four helpers, not five) all need the corrections above; they are Pass-3 synthesis docs
and are left unedited per the append-only rule. `docs/obsidian-audit/01-markoff-gaps.md` /
`GAP-ANALYSIS.md` §P0 (the `indexOf("\n---", 3)` item) should be re-scoped: the EOF case it names
is fine, the CRLF and trailing-fence cases are the real divergences.
