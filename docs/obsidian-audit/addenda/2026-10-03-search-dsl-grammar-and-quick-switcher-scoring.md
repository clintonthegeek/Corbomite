# Search: the real DSL grammar is in-tree, and the quick switcher scores differently than the audit thinks

**Date:** 2026-10-03
**Discovered during:** exploratory spike — verifying `docs/obsidian-audit/domains/search.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/renamed/src/app/own/`)
**Supersedes / extends:** `domains/search.md` §1, §2, §3, §9, §10, §11, §13 Q1/Q4/Q5, §14 items 2–3, §15
**Relevant cluster plans:** none currently; `libs/search` (SearchDSL/FuzzyMatcher) parity work should cite this

All citations are `bundle/rel:line` against `RE/obsidian/1.13.7/renamed/src/`. Every `_internal.js:*` and
`obsidian/search/*.js:*` reference in the domain doc is dead — see the remap in §G.

---

## Finding

`domains/search.md` filed the entire search DSL as out-of-tree (§9 "Features NOT powered by this
domain", §13 Q1 "the parser lives in the internal `global-search` plugin, not in `obsidian/search/`",
§14 item 2 "DSL parser — source unavailable; reverse-engineer from user documentation"). **It is in
the corpus.** The whole pipeline is a run of small statements at `app/own/1160`–`app/own/1178`,
immediately below the fuzzy matcher (`app/own/0778`–`0789`) that the doc *does* cover, and the
`global-search` plugin that hosts the UI is at `app/own/1848-Y1.js:209`. §13 Q1 and §14 item 2 are
answered; nothing needs reverse-engineering from documentation.

---

## §A — NEW: the grammar

### A.1 Tokenizer — `app/own/1173-fP.js:2` (`fP(query) → Token[]`)

Four character classes, all `{type, content, pos}`:

| Trigger | Token types emitted | Lines |
|---|---|---|
| `"` | `quote` (backslash-escapes the next char) | `:130`, `:141` |
| `/` | `regex` (only `\/` and `\\` are unescaped) | `:95`, `:115` |
| `-` at a token start | `not` | `:84` |
| anything else | accumulated until one of `[ ] ( ) : > <` or a space | `:38`–`:70` |

A `-` is only a `not` token when the scan **starts** on it; inside an accumulating token it is an
ordinary character. That is why `task-todo:` is a single `text` token, not `task` `not` `todo`.

The accumulator is post-classified (`app/own/1173-fP.js:4-22`):

```js
"TRUE" === e  ? {type:"true"}  : "FALSE" === e ? {type:"false"} :
"EMPTY" === e ? {type:"empty"} : "OR" === e    ? {type:"or"}    : {type:"text"}
```

All four comparisons are **case-sensitive and exact** — `or`, `True`, `empty` are ordinary `text`.

### A.2 Parser — three layers

| Layer | Site | Shape |
|---|---|---|
| `lP(tokens, operators)` | `app/own/1167-lP.js:2` | seeds `{tokens, pos:0, operatorStack:[], operators}` |
| `cP` — **OR list** | `app/own/1168-cP.js:2-18` | `uP` results split on `or` tokens, wrapped in `rP` if >1 |
| `uP` — **AND list** | `app/own/1169-uP.js:2-5` | `hP` results back-to-back, wrapped in `iP` if >1 |
| `hP` — **one term** | `app/own/1170-hP.js:2-73` | see below |

**There is no `AND` keyword.** Whitespace is the AND. Precedence is therefore exactly
`OR < AND < term`, and an operator call, `-`, `>` and `<` each bind to a **single `hP`** — one
token, not a sub-expression. `path:a b` parses as `AND(path:a, text("b"))`; `path:a OR b` as
`OR(path:a, text("b"))`. Only `(` (`:52-64`) and the property body (`:28`, `:34`) recurse back into `cP`.

`hP` branches, in evaluation order:

| Branch | Line | Produces |
|---|---|---|
| `text` + `colon` → operator | `:7-22` | `a.operate(hP(e))` |
| `bracket [` → property | `:24-39` | `JA(key, value?)` |
| `bracket ]` / `parenthesis )` / `or` / `colon` | `:41`, `:66`, `:67`, `:69` | `null` (terminator) |
| `text` | `:42` | `tP` |
| `quote` | `:43` | `eP` |
| `regex` | `:44-51` | `ZA`, after `new RegExp(i)` validation |
| `parenthesis (` | `:52-64` | the inner `cP` node **verbatim — there is no Group node in Obsidian's AST** |
| `true`/`false`/`empty` | `:68` | `_A` |
| `not` | `:70` | `sP(hP(e))` |
| `greaterthan`/`lessthan` | `:71` | `aP(type, hP(e))` |
| anything else | `:72` | `throw new Error("Failed to parse, error at " + pos + ' "' + content + '"')` |

Errors are surfaced verbatim in the search-info bar (`app/own/1848-Y1.js:606-613`) and thrown as
CLI errors (`app/own/1848-Y1.js:247`: `throw "Invalid search query."`).

### A.3 Operator table — `app/own/1175-SP.js:148-221` (`DP`)

This is the authoritative list, verbatim keys and flags:

```js
"match-case":  {operate: e => new vP(e, !0)}      // not exclusive
"ignore-case": {operate: e => new vP(e, !1)}      // not exclusive
path:     {exclusive:!0, operate: e => new gP(e)}  //  filepath
file:     {exclusive:!0, operate: e => new yP(e)}  //  e.name (basename+ext)
content:  {exclusive:!0, operate: e => new bP(e)}  //  explicit content:
line:     {exclusive:!0, operate: e => new kP(e)}  //  per-line
block:    {exclusive:!0, operate: e => new CP(e)}  //  block-boundary
section:  {exclusive:!0, allowSelf:!0, operate: e => new SP(e)}
task:     {exclusive:!0, operate: e => new xP(e, void 0)}
task-todo:{exclusive:!0, operate: e => new xP(e, !1)}
task-done:{exclusive:!0, operate: e => new xP(e, !0)}
tag:      {exclusive:!0, operate: e => { if (!(e instanceof tP))
              throw new Error('Operator "tag" can only be followed by text');
              return new TP(e); }}
```

Eleven operators. The two flags `match-case` / `ignore-case` are **not exclusive** — they wrap the
rest of the query rather than taking an operand (`app/own/1174-vP.js:7-13` clones the input and
overwrites `caseSensitive`), so they nest inside anything.

**Exclusivity** (`app/own/1170-hP.js:12-17`) walks the operator stack and throws
`'Operator "X" cannot be nested within "Y"'` for any `exclusive` outer operator, unless the outer
sets `allowSelf` and names the same operator — which only `section:` does, so `section:foo` nests
but `section:foo path:x` throws. A `[` anywhere on the stack disables operator recognition
altogether (`:7`), and a second `[` throws `"Property cannot be nested within a property."` (`:25`).

### A.4 Property syntax — `[key]` / `[key: value]`

`app/own/1170-hP.js:24-39` builds `JA(key, value)` where `key` is a full `cP` (so `["a" OR "b"]`
works) and `value` is a `cP` after an optional `:`. `value === null` is an existence check
(`app/own/1166-ZA.js:69-71`). Matching (`app/own/1166-ZA.js:29-76`):

- iterates `e.cache.frontmatter` keys, testing each key name through `this.key.match` with
  `o.keys = ["propertyName"]` — **case-insensitive equality**
  (`app/own/1166-ZA.js:102-104`: `e.strings[o].toLowerCase() === this.text.toLowerCase()`);
- array-valued properties are matched **element-wise** and each hit records
  `{key, subkey:[h], pos}` (`:44-56`), scalars record `{key, pos}` (`:57-68`);
- an absent frontmatter returns `null` outright (`:31-33`).

Frontmatter key lookup elsewhere is also case-insensitive: `IA(frontmatter, "tags")`
(`app/own/1156-IA.js:2-9`) returns the object's own spelling, so `Tags:` in YAML still matches
`tag:`. Corbomite's own `docs/obsidian-audit/addenda/2026-10-03-parsing-frontmatter-info-offsets-and-parser-divergence.md`
records the same `IA` behaviour from the frontmatter side.

### A.5 Bare text is a substring regex; quotes are whole-word — but only on `content`

- `tP` (bare text) **extends the regex matcher** (`app/own/1166-ZA.js:116-128`), storing
  `regex = jd(text)`. `jd` escapes exactly
  `/[.?*+^$[\]\\(){}|-]/g` (`app/own/0553-Wd.js:2`, applied at `app/own/0554-jd.js:3`). `foo` therefore
  matches `foobar`; `foo.md` matches `fooXmd`; `-` and `|` are escaped, `/` is not.
- `eP` (quoted) keeps **two** regexes (`app/own/1166-ZA.js:90`):
  `wholeWordRegex = VA(text)` and `partialMatchRegex = jd(text)`. At match time
  (`app/own/1166-ZA.js:96-101`) the key decides: `content` uses the whole-word regex with the
  capture-group flag on; **every other key (`filename`, `filepath`, `tag`) uses the plain substring
  regex**, and `propertyName` uses case-insensitive equality.

`VA` (`app/own/1159-FA.js:6-11`) is verbatim:

```js
function VA(e) {
  if (0 === e.length) return "";
  var t = e.charAt(0), n = e.charAt(e.length - 1),
      i = BA.test(t), r = BA.test(n);
  return (i ? "(?:^|[^" + NA + "])" : "") + "(" + jd(e) + ")" + (r ? "(?![ " + NA + "])" : "()");
}
```

`NA` is the Unicode letter set (ASCII + Latin-1/Extended-A/B + Greek + Cyrillic + Hebrew + Arabic +
Devanagari … + **CJK** + Hangul, `app/own/1159-FA.js:2`) concatenated with `0-9` and ~30 other digit
ranges (`:4`). So `"foo bar"` is a whole-word *phrase* against file content but a bare substring
against a filename or tag.

### A.6 `TRUE` / `FALSE` / `EMPTY`, `>` and `<`

`_A` (`app/own/1163-UA.js:21-38`) and `aP` (`app/own/1166-ZA.js:190-212`) both read a *numeric* value
off the input object via `getData(MP)`. `MP` is only ever populated by
`zA.cloneForPropertyContent` (`app/own/1160-HA.js:12`), which is called **only** from the property
matcher (`app/own/1166-ZA.js:47`, `:59`). Consequences:

- `>`, `<` and the three keywords only ever compare **frontmatter property values**.
- `aP` requires a text operand and otherwise throws
  `'" + type + " operator cannot be applied to matcher " + n.getInfo()'`
  (`app/own/1166-ZA.js:195`); the comparison is a JS relational on `number < string`, so
  `[rating: >4]` works numerically for a YAML number.
- **A bare top-level `EMPTY` matches every file.** `_A`'s `expected` stays `null` for `"EMPTY"`
  (`app/own/1163-UA.js:24`) and the root input has no `MP` data, so `getData` returns `null` and
  `null === null` is true (`app/own/1163-UA.js:27-28`). `TRUE`/`FALSE` at top level match nothing
  (`null !== true`).

### A.7 Per-operator search space

| Op | Matcher | Input string(s) searched |
|---|---|---|
| `path:` | `gP` `app/own/1174-vP.js:15-27` | `filepath` (`e.path`) |
| `file:` | `yP` `:30-42` | `filename` (`e.name`) |
| `content:` | `bP` `:45-57` | `content` |
| `line:` | `kP` `:93-113` | `content` **split on `"\n"`**, each line matched separately (`:101`) |
| `block:` | `CP` `:115-148` | each `cache.sections` entry with `type !== "list"` (`app/own/1174-vP.js:125-133`) **plus every `cache.listItems` entry** (`app/own/1174-vP.js:134-140`) |
| `section:` | `SP` `app/own/1175-SP.js:2-70` | a flat section list derived from `cache.headings` (`:32-49`) |
| `task:`/`task-todo:`/`task-done:` | `xP` `:71-103` | every `cache.listItems` entry that has a `.task`, gated on `" " !== s.task` |
| `tag:` | `TP` `:104-147` | frontmatter `tags` array **and** `cache.tags[].tag` |

Four consequences worth having in a port:

1. **`block:` is a block-boundary-scoped *content* search, not a block-ID lookup.** `CP.match`
   never compares `section.id`; it just narrows the searchable text to each block's substring. There
   is no `^block-id` path in the DSL matcher. (`blockCache` is a *display-text* index with a
   completely separate lifecycle — see the 2026-10-03 metadata addendum.)
2. **`line:` cannot match across a newline**, by construction (`:101`). A phrase spanning two lines
   matches under `content:`/`/regex/` and never under `line:`.
3. **`section:` is a derived flat list, not `CachedMetadata.sections`.** From
   `app/own/1175-SP.js:32-49`: for each heading it emits `{start, end, children}` where `end` is the
   *next* heading's start (or EOF) and `children` is the count of following deeper headings. A
   preamble entry `{start:0, end:firstHeadingStart, children:0}` is emitted **only when the first
   heading does not start at offset 0** (`:37-41`). A leaf with `children === 0` returns `null` on
   re-entry (`:30`), and the resumable index is stashed in the input object's data map under `EP`
   (`app/own/1174-vP.js:150`, `app/own/1175-SP.js:14-19`, `:56-59`).
4. **`tag:` is always case-insensitive.** `TP.match` hardcodes the `i` flag:
   `new RegExp("^" + this.matcher.regex + "($|\\/)", "i")` (`app/own/1175-SP.js:110`). `match-case`
   does not affect it. The `($|\/)` tail is what makes `#work` match `#work/sub`.
   `TP`'s constructor also auto-prefixes: `"#" !== t.text.charAt(0) && t.setText("#" + t.text)`
   (`:107`), so `tag:foo` and `tag:#foo` are the same query — but `tag:"foo"` **throws**, because a
   `quote` token yields `eP`, and `eP` is a sibling of `tP` (`app/own/1166-ZA.js:87` vs `:116`),
   not a subclass.

### A.8 Which files get searched at all — `app/own/1175-SP.js:229-269`

```js
"canvas" === e.extension && canvasEnabled ? … JSON.parse, filter nodes to 'text' === e.type …
  each text node matched as {filename, filepath, content: node.text} with cache from
    canvas.index.get(file).caches[node.id]
  then filename matched separately                      // app/own/1175-SP.js:231-259
…
kb(e.extension) || g.push("filename"), "md" === e.extension && g.push("content")
```

with `kb = ext => !["md","canvas","base"].includes(ext)` (`app/own/0795-kb.js:2`, table at
`app/own/0794-hb.js:5-8`). So the searchable key set is:

| Extension | `keys` |
|---|---|
| `.md` | `["content"]` |
| `.canvas` | `["content"]` — plus the per-text-node pass above |
| `.base` | `["content"]` (raw file text) |
| everything else | `["filename"]` |

A `.canvas` file whose JSON has no `nodes` array (or fails to parse) falls through to the generic
path and is content-searched as raw JSON (`app/own/1175-SP.js:231-236` guards with
`Array.isArray(i.nodes)`).

### A.9 The search panel's file iteration — `app/own/1176-PP.js:2`

`PP(app, requiredInputs, queue, cb, throttleOpts)`, called from `app/own/1848-Y1.js:586`:

- **skips** `metadataCache.isUserIgnored(path)` (`:15`) and `!metadataCache.isSupportedFile(file)` (`:16`);
- reads content with `vault.cachedRead` **only when `requiredInputs.content` is set and the
  extension is `md` or `canvas`** (`:17`) — `.base` content is never read;
- cancels via `queue.runnable.isCancelled()` (`:26`);
- batches through `OA` (`app/own/1158-OA.js:2`) with `batchSize: 10`, `duration: 5`,
  `maxDelay: 100` (defaults at `app/own/1158-OA.js:8`), i.e. ~10 files per 5 ms tick, forced at 100 ms.

---

## §B — NEW: the search view's persisted and local state

`domains/search.md` §3 says "**No on-disk contracts.**" True for the six pure functions, false for
the domain as a whole — the `search` view writes six keys into the **workspace layout** and one key
into **localStorage**.

### B.1 Workspace-layout state — view type `"search"` (`app/own/1848-Y1.js:416`, `:500-501`)

`getState` (`:502-504`) returns exactly:

```js
t.query = this.getQuery(),
t.matchingCase = this.matchingCase,
t.explainSearch = this.explainSearch,
t.collapseAll = this.dom.collapseAll,
t.extraContext = this.dom.extraContext,
t.sortOrder = this.dom.sortOrder
```

`setState` (`:505-516`) restores each independently with a type guard (`String.isString` for
`query`/`sortOrder`, `isBoolean` for the four toggles) and re-runs `setQuery` → `startSearch`.
Ephemeral state is `{focus: true}` only (`:517-522`), which selects the whole input.

`matchingCase` is not just an operator — it is a **toolbar button** whose state is passed straight
into the matcher constructor: `new AP(app, query, this.matchingCase)`
(`app/own/1848-Y1.js:574`, consumed at `app/own/1175-SP.js:224` and threaded to `zA` at `:247`,
`:268`). Toggling it re-runs the search only when the query is non-empty (`:524`).

`openGlobalSearch(query)` is `workspace.ensureSideLeaf("search", "left", {active, reveal:true,
state:{query}})` (`:400-407`) — i.e. the tag-pane's `openGlobalSearch("tag:" + name)` opens a
**left side leaf**, not a modal.

### B.2 Sort orders — `app/own/1177-IP.js:2-33`

Six orders in three dropdown pairs (`IP`), labels in `LP`, comparators in `OP`:

```
["alphabetical","alphabeticalReverse"], ["byModifiedTime","byModifiedTimeReverse"],
["byCreatedTime","byCreatedTimeReverse"]
```

`alphabetical` compares **`basename`**, not path (`OP.alphabetical = (a,b) => Sw(a.basename, b.basename)`,
`:20-22`); the two time orders are `b.stat.mtime - a.stat.mtime` / `b.stat.ctime - a.stat.ctime`
(`:26-34`). Default is `"alphabetical"` (`app/own/1848-Y1.js:481`). `FP(order)` falls back to
alphabetical for an unknown key (`app/own/1178-FP.js:2-4`).

`Sw` is `Mw` = `new Intl.Collator(undefined, {usage:"sort", sensitivity:"base", numeric:true}).compare`
(`app/own/0819-ww.js:228-233`) — **base** sensitivity (case- and accent-insensitive) and
**numeric: true**, so `note2` sorts before `note10`.

### B.3 `recent-searches` in localStorage

`saveSearch` (`app/own/1848-Y1.js:492-497`) reads `app.loadLocalStorage("recent-searches")`,
`remove(e)` then `unshift(e)` (LIFO), `slice(0, 20)`, and writes back. It is wired as
`Ml(saveSearch, 5000, !0)` (`:420`) — `Ml` is lodash-style `debounce(fn, wait, trailing)`
(`app/own/0355-Ml.js:2-24`), so the third argument `true` means the write lands **5 s after the last
keystroke**, not on a leading edge. The same key feeds the suggestion popup's History group
(`app/own/1848-Y1.js:158-168`), whose clear button calls
`app.saveLocalStorage("recent-searches", null)`.

### B.4 Compiled-regex cache with a 60 s trailing reset

`QA(source, caseSensitive)` (`app/own/1164-QA.js:2-6`) memoises `RegExp` objects in `GA`
(case-sensitive → flags `"gm"`) and `YA` (case-insensitive → `"gmi"`). `XA` is
`Ml(() => { GA = {}; YA = {}; }, 6e4, !0)` (`app/own/1163-UA.js:59-61`) and is invoked at the top of
every `AP._match` (`app/own/1175-SP.js:287`), so the cache survives active searching and is dropped
60 s after it stops. Note the `m` flag is always on — `^`/`$` in a `/regex/` are **per line** even
against `filepath`.

### B.5 CLI handlers

Three, all registered by `GlobalSearchPlugin.onEnable` (`app/own/1848-Y1.js:273-357`):

- `search` — `{query*, path, limit, total, case, format: text|json}` (`:284-322`). Iterates
  `vault.getMarkdownFiles()` only (`:248`), skips ignored files (`:252`), and applies the `path`
  argument as a **directory prefix with the slash forced on**:
  `a = n.endsWith("/") ? n : n + "/"` then `e.path.startsWith(a)` (`:248-251`) — so `path=notes`
  matches `notes/…` but never a file literally named `notes`. Results are unsorted; `limit` slices (`:310`).
- `search:context` — same, but emits `path:line: text` (`:314-353`); a file with zero match ranges
  is emitted as a bare path (`:347`).
- `search:open` — `app/own/1848-Y1.js:354-357`.

A fourth, `__files`, is registered by the CLI registry (`app/own/1225-SL.js:163-192`) and is the
place where **recency is a candidate-ordering pre-sort, not a score bonus**:

```js
o = new Set(t.workspace.getLastOpenFiles()),
a = r.getFiles().map(e => e.path),
c = [...a.filter(p => o.has(p)), ...a.filter(p => !o.has(p))];   // opened first
…
h.sort((a,b) => b.score - a.score)                                 // stable ⇒ recency wins ties
```

---

## §C — NEW: the quick switcher

`kte` (`app/own/2040-Vee.js:2999-3060`) is the Quick Switcher modal; it subclasses the generic file
suggester `e3` (`app/own/1951-e3.js:2`). Plugin id `"switcher"`, `defaultOn: true`
(`app/own/2040-Vee.js:2958`), settings `{showExistingOnly:false, showAttachments:true,
showAllFileTypes:false}` (`:2951-2955`), and `showAllFileTypes` is only offered when
`vault.getConfig("showUnsupportedFiles")` is set (`app/own/2040-Vee.js:3075-3077`).

### C.1 The candidate string differs from what `file:` searches

`Z4` (`app/own/1949-Z4.js:2`): `"md" === e.extension ? Ol(e.path) : e.path` — for a markdown file
the candidate is the basename **without** extension (`Ol`, `app/own/0364-Ol.js:2`), for everything
else it is the **full path**. `file:` searches `e.name` (`app/own/1175-SP.js:262`), i.e. basename
*with* extension. Two surfaces, two different strings.

### C.2 Basename-preferred scoring with a −1 fallback

`XR(prepared, candidate)` (`app/own/1333-XR.js:2-6`):

```js
n = Al(t),                                   // basename incl. extension  (app/own/0360-Al.js:2)
i = e(n);
return i ? (ib(i.matches, t.length - n.length), i)   // shift offsets back into full-path space
         : ((i = e(t)) && (i.score -= 1), i);        // full string, −1
```

So a hit on the basename outranks an equally-good hit on the full path by exactly **1 point**
(≈ the entire start-bias term), and match offsets are translated (`ib`,
`app/own/0786-ib.js:2-8`) into the displayed string's coordinates.

### C.3 ≥ 10 000 files switches to a *different* matcher

`app/own/1951-e3.js:89`:

```js
v = m.length < 1e4 ? Zy(e) : ab(e);
```

`Zy` is `prepareFuzzySearch` (the two-pass token-then-char matcher); `ab` is
**`prepareSimpleSearch`** — the same function the plugin API exports
(`app/own/0025-expressionstatement.js:145`). `ab` (`app/own/0789-ab.js:2-7`) splits only on spaces
and delegates to `rb` (`app/own/0787-rb.js:2-11`), which is an ordered substring-AND with **no
char-fuzzy fallback, no punctuation/CJK tokenisation, and the case-fidelity penalty hardcoded to
`0`** (`app/own/0788-ob.js:2-8`: `Jy(i, t.length, n.length, 0)`). Token boundaries still advance
left-to-right (`l += c + 1` after each hit) and offsets are merged (`Gy`), but the ranking of a
large vault is materially different from a small one.

### C.4 Row types, caps, and ignored files

`getSuggestions` (`app/own/1951-e3.js:70-156`) emits four row kinds, each scored independently and
sorted together by `nb` (= `sortSearchResults`, `:155`):

| `type` | Source | Scored text | Lines |
|---|---|---|---|
| `file` | `vault.getFiles().filter(h)` | `XR(v, Z4(b))` | `:89-97` |
| `alias` | each entry of `parseFrontMatterAliases` (`$T`) | the alias string | `:98-115` |
| `unresolved` | deduped targets of `metadataCache.unresolvedLinks` | the linktext | `:117-134` |
| `bookmark` | `bookmarks` plugin items (file+subpath resolved to a real file first) | breadcrumb + item title | `:135-154` |

- **Cap:** `limit = 20` (`app/own/1951-e3.js:5`), sliced in `SuggestModal.updateSuggestions`
  **after** `getSuggestions` has sorted (`app/own/0819-ww.js:176-185`, `:182`).
- **Empty query** returns `workspace.getRecentFiles(...)` unsorted, and *excludes* ignored files
  (`app/own/1951-e3.js:70-84`, exclusion at `:77`) — no fuzzy pass runs at all.
- **Non-empty query downranks, it does not exclude:** `w = a.isUserIgnored(b.path)` then
  `F.score -= 10` and `downranked: true` (`:91-97`, alias rows too at `:106-112`). The row renders
  with `mod-downranked` (`:172`). This is a different policy from the search panel's, which skips
  ignored files outright (`app/own/1176-PP.js:15`).
- `parseFrontMatterAliases` (`$T`, exported at `app/own/0025-expressionstatement.js:136`) reads the frontmatter key by `/^aliases$/i` and trims/filters empties
  (`app/own/1091-$T.js:2-8`).
- Extension admission (`:75-79`): `md` → `shouldShowMarkdown`; `canvas`/`base` →
  `shouldShowNonAttachments`; `["bmp","png","jpg","jpeg","gif","svg","webp","avif"]`
  (`app/own/0794-hb.js:2`) → `shouldShowImages`; else `shouldShowAllTypes ||
  (shouldShowNonImageAttachments && registry.isExtensionRegistered(ext))` where the registry is
  `viewRegistry` for `context === "view"` and `embedRegistry` otherwise.
- Tab-completion: the first suggestion's link text is recomputed and, if it already equals the
  input, a `/` is appended (`app/own/1951-e3.js:5-19`).

---

## §D — CORRECTIONS to `domains/search.md`

| # | Doc says | Source says |
|---|---|---|
| **D1** | §1/§9/§13 Q1 list the case flags as `match-case` **/`whole-word`** | `whole-word` is **not** a DSL operator. The only flags are `match-case` and **`ignore-case`** (`app/own/1175-SP.js:148-158`). The `whole-word` strings in the corpus are the *editor find/replace* `wholeWord` toggle (`app/own/1426-nz.js:47`, `app/own/1433-fz.js:10`, `app/own/1449-Wz.js:105`). Whole-word semantics exist, but they are what **quotes** do (§A.5), not a flag. |
| **D2** | §9/§13 operator set = `tag:`, `path:`, `file:`, `line:`, `block:`, `section:`, regex, quote, `-`, flags | Also `content:`, `task:`, `task-todo:`, `task-done:` (`app/own/1175-SP.js:171-213`), plus the `[key]`/`[key:value]` property syntax (`:24-39` of `1170-hP.js`), the `TRUE`/`FALSE`/`EMPTY` keywords, and `>`/`<`. `OR` is a reserved literal keyword (§A.1). |
| **D3** | §1 says `prepareSimpleSearch` "delegates to internal `Iy(tokens, query, candidate)` which runs `Ly` … and scores via `xy`" | In 1.13.7 it is `ab` → `rb`, and `rb` is its own matcher; `ob` is the scorer wrapper. `Iy`/`Ly`/`xy` do not exist under those names. See the remap in §G. |
| **D4** | §13 Q5 "prepareSimpleSearch has zero call sites in the audited tree. Confirm this is intentional." | It has an in-app call site and it is load-bearing: it is the **≥ 10 000-file quick-switcher matcher** (`app/own/1951-e3.js:89`). Not an oversight. |
| **D5** | §3 "**No on-disk contracts.**" | The search view writes six keys into the workspace layout (`query`, `matchingCase`, `explainSearch`, `collapseAll`, `extraContext`, `sortOrder` — §B.1) and `recent-searches` into localStorage (§B.3). |
| **D6** | §13 Q4 "FuzzySuggestModal base class never caps … Corbomite should adopt 100 as the default" | `SuggestModal` sets `limit = 100` explicitly (`app/own/0819-ww.js:145`) and `AbstractInputSuggest` sets `limit = 100` (`app/own/1319-xR.js:75`), so the base *does* cap — at 100. But the **file suggester** (quick switcher, link autocomplete, embed picker) overrides it to **20** (`app/own/1951-e3.js:5`). 20 is the number that governs note-finding surfaces. |
| **D7** | §1/§10 "Plugin-reachable public API count: 5" | Runtime exports are indeed 5 (`app/own/0025-expressionstatement.js:117` `fuzzySearch: () => tb`, `:143` `prepareFuzzySearch: () => Zy`, `:144` `prepareQuery: () => $y`, `:145` `prepareSimpleSearch: () => ab`, `:157` `sortSearchResults: () => nb`) — but only **three** are in the published typings. The npm `obsidian` typings (package version **1.12.3**, `testvaults/obsidian-api/package.json`) declare `prepareFuzzySearch` (`:5080`), `prepareSimpleSearch` (`:5088`) and `sortSearchResults` (`:5821`) and **not** `fuzzySearch` or `prepareQuery`. Caveat: this d.ts is not part of the 1.13.7 extraction, so treat it as "the published typings package" rather than "1.13.7's own surface". |
| **D8** | §10 "QueryController is **not a plugin-facing surface** (not part of the published `obsidian` typings…)" | It is declared: `export class QueryController extends Component` at `testvaults/obsidian-api/obsidian.d.ts:5143`, with `BasesViewFactory` taking one (`:1248`). Same 1.12.3 caveat as D7. (Its *behaviour* claim — that it does not parse the search DSL — remains correct; see `app/own/1813-CZ.js:2235` for `applySearchQuery`.) |
| **D9** | §11 "Corbomite's C++ port must reproduce *ordering*… `sortSearchResults`" as the only sorter | There is a second, path-level ranking layer the doc never mentions: basename-first with a −1 fallback (`app/own/1333-XR.js:2-6`, §C.2). Two candidates can tie on `match.score` and still be ordered by that layer. |

---

## §E — CONFIRMED (audit-doc claims re-verified in 1.13.7, with live citations)

| Doc claim (location) | 1.13.7 source | Verdict |
|---|---|---|
| `prepareQuery` return shape `{query, tokens, fuzzy}`, `fuzzy` = char array with spaces removed (`§1`, `§2`) | `app/own/0780-$y.js:2-14` | exact |
| Tokenizer classes: whitespace / ASCII+general-punct / Tibetan+CJK+half-width-katakana (`§1`) | `app/own/0779-Yy.js:2-4` (`Xy`/`Yy`/`Qy`) | exact |
| Two-tier `fuzzySearch`: word-token pass, then strict-adjacent char-fuzzy fallback (`§1`) | `app/own/0784-tb.js:2-9` (`eb(tokens,…,false)` then `eb(fuzzy,…,true)`) | exact |
| Empty query ⇒ `{score: 0, matches: []}` (`§1`, `§8`) | `app/own/0784-tb.js:3-6` | exact |
| `matches` are `[start,end)` half-open, merge-sorted, non-overlapping (`§2`, `§8`) | pushed at `app/own/0783-eb.js:19-23`; merge in `Gy` at `app/own/0778-Gy.js:2-9` | exact |
| Scoring formula: `-(runs-1)`, `-penalty/10`, `-span/100`, `-first/1e3`, `-len/1e4` (`§2`) | `app/own/0782-Jy.js:2-8` | exact, all five constants |
| `sortSearchResults` sorts by `match.score` descending, in place (`§1`) | `app/own/0785-nb.js:2-6` | exact |
| `FuzzySuggestModal` = trim query, `$y` + `tb` per item, sort via `sortSearchResults` (`§1`, `§10`) | `app/own/0819-ww.js:209-221` | exact |
| `QueryController` does **not** parse the search DSL; `applySearchQuery` is a substring-AND over `prepareQuery(...).tokens` (`§8`, and the 2026-06-10 addendum) | `app/own/1813-CZ.js:2235` | exact |
| `renderMatches` is the public highlight renderer (`§13 Q2`) | `renderMatches: () => lb` (`app/own/0025-expressionstatement.js:147`), `renderResults: () => sb` (`:149`); `lb` is `app/own/0791-lb.js:2` (spans `cls:"suggestion-highlight"`) and `sb` (`app/own/0790-sb.js:2`) is `lb(el, text, n?.matches, offset)` | answered. Note for the rendering addendum: the **public `renderResults` is `sb`, and `lb` is the public `renderMatches`** — `2026-10-03-rendering-tooltip-and-match-highlight.md` §"renderResults is `lb`" has the two swapped; the span markup it documents is `lb`'s. |

---

## §F — Corbomite divergences (`libs/search`, measured against the above)

`docs/PARITY-MATRIX.md` §5 currently reads *"Search DSL parse (path/file/content/tag, phrases,
regex, OR, -, parens, case) | ✅ | `SearchDSL.cpp`; quirk-faithful"*. The grammar really is
faithful — and now verifiable, which it was not before this addendum. Six concrete deltas:

**F1 — `SearchDSL.cpp` lexes `<`, `>`, `TRUE`, `FALSE`, `EMPTY` but the parser has no cases for
them.** `Tok::LessThan/GreaterThan/True/False/Empty` are produced
(`libs/search/src/SearchDSL.cpp:53-58`, `:183-186`) and then fall through
`parsePrimary`'s `default: return nullptr` (`:309-310`). `parseAnd` sees no token consumption and
breaks (`:250-260`, `:256`), so `>5` or a stray `TRUE` **silently truncates the conjunct list**. Obsidian
builds `aP`/`_A` nodes for both (`app/own/1170-hP.js:68`, `:71`). Not user-visible until property
support lands, but the AST shape is already decided and it will bite then.

**F2 — trailing-hyphen tokenisation differs.** `readText` only keeps an internal `-` when the
next character is not a delimiter (`libs/search/src/SearchDSL.cpp:171-177`), so `foo-` tokenises to
`[Text("foo"), Not]`. Obsidian's accumulator takes `-` unconditionally once the scan has started
(`app/own/1173-fP.js:24-78`), yielding `text("foo-")`. `match-case` is unaffected (the hyphen is
interior there), but `foo-` and `foo -` differ.

**F3 — Corbomite has a `Group` AST node; Obsidian does not.** Parens return the inner node
directly, pushing only the `(` token onto `matchedTokens`
(`app/own/1170-hP.js:52-64`; compare `libs/search/include/corbomite/search/SearchAst.h:36`). Harmless
for matching, visible in any `toString` round-trip and in any node-count assertion.

**F4 — `[property]` is a null-returning stub.** `parsePropertyStub`
(`libs/search/src/SearchDSL.cpp:371-384`) skips the bracketed body and reports the term as absent,
so `[author:x]` is *silently dropped* rather than rejected. Obsidian builds a `JA` node with
case-insensitive key matching and element-wise array handling (§A.4). Worth noting the stub also
loses the operator-recognition guard: Obsidian refuses to recognise `foo:` as an operator while a
`[` is on the stack (`app/own/1170-hP.js:7`).

**F5 — `PARITY-MATRIX` "Quick-switcher fuzzy scoring parity ✅ constants match audit exactly" is
half true.** The five constants do match (`libs/search/src/FuzzyMatcher.cpp:127-142` vs
`app/own/0782-Jy.js:2-8`). The **penalty accounting** does not, in two places.

> **(a) The word-boundary predicate.** Obsidian's test (`app/own/0783-eb.js:22-30`) is two
> clauses over the *candidate's* original-case characters `h = n.charAt(u)` (the matched char) and
> `p = n.charAt(u-1)`:
>
> ```js
> if (h.toLowerCase() !== h && p.toLowerCase() !== p          // A: h cased-upper, p NOT cased-upper
>  || h.toUpperCase() !== h && !Yy.test(p) && !Xy.test(p) && !Qy.test(p))   // B: h not already upper, p not ws/punct/CJK
> ```
>
> Corbomite's `isWordBoundary` (`libs/search/src/FuzzyMatcher.cpp:52-59`) is
> `i == 0 || prev ∈ {ws,punct,CJK} || (prev.isLower() && curr.isUpper())`. Clause B lines up.
> Clause A does not: it is *"p is not an uppercase letter"*, whereas Corbomite tests *"p is a
> lowercase letter"*. The two disagree whenever `h` is cased-upper and `p` is **not lowercase** —
> an all-caps run, or a digit/symbol before an upper-case letter.
>
> Worked example — query `bc`, candidate `ABc`: the lowercased candidate is `abc`, the token matches
> at offset 1, so `h = n.charAt(1) = "B"` and `p = n.charAt(0) = "A"`. Obsidian's clause A fails
> (`p.toLowerCase() === p`), clause B fails (`h.toUpperCase() === h`) → **`o` stays 0**.
> Corbomite: `prev.isLower()` is false for `"A"` → not a boundary → **penalty 1**. Identical match
> ranges `[[1,3]]`, but the scores are `-0.0113` (Obsidian) vs `-0.1113` (Corbomite) — a whole
> case-fidelity unit apart, enough to reorder candidates against any `bc` match with one boundary hit.

> **(b) The strict (char-fuzzy) pass is a different algorithm.** Obsidian forces **exact
> adjacency**: `if (u !== a) { a += c.length; l--; continue; }` (`app/own/0783-eb.js:12-17`) — it
> retries the *same* token starting at `cursor + tokenLength` and accepts only
> `indexOf(token, cursor) === cursor`, **without charging a penalty**. Corbomite instead scans ahead
> for a word-boundary occurrence and, failing that, accepts the first hit **with** a penalty
> (`libs/search/src/FuzzyMatcher.cpp:86-96`). Worked example — query `md`, candidate `m1d`: the
> word-token pass fails, so pass 2 runs. Obsidian: `'m'`@0 → cursor 1; `'d'` found at 2 ≠ 1, retry
> from 2, accept, **penalty 0**, `matches = [[0,1],[2,3]]`, score
> `-(2-1) - 0 - (3-0+1-2)/100 - 0 - 3/10000 = **-1.0203**`. Corbomite: `'d'` first hit 2, not a
> boundary, no later probe → **penalty 1**, same `matches`, score
> `-1 - 0.1 - 0.02 - 0 - 0.0003 = **-1.1303**`.

**F6 — no quick-switcher candidate pre-ranking exists.** `XR`'s basename-first/−1-fallback
(§C.2), the `ab`-above-10 000-files matcher switch (§C.3), the `score -= 10` user-ignore
downrank (§C.4) and the recency pre-sort (§B.5) have no counterpart in
`libs/search/src/FuzzyMatcher.cpp`. `FuzzyMatcher.cpp` exposes the two-pass scorer but nothing
composes it per-candidate the way `XR`/`Z4` do.

---

## §G — Symbol remap (every name in `domains/search.md` §15 and §1, re-anchored to 1.13.7)

The doc's §15 "Short symbols" table and all `obsidian/search/*.js:*` / `_internal.js:*` citations
are dead — there is no `_internal.js` in the 1.13.7 extraction, and the `obsidian/search/` directory
was a 1.12.7-era extraction layout. 1.13.7 names:

| Doc name / public name | 1.13.7 site | Role |
|---|---|---|
| `prepareQuery` | `app/own/0780-$y.js:2` (`$y`) | tokenizer |
| `prepareFuzzySearch` | `app/own/0781-Zy.js:2` (`Zy`) | curried two-pass matcher |
| `fuzzySearch` | `app/own/0784-tb.js:2` (`tb`) | one candidate against a prepared query |
| `prepareSimpleSearch` | `app/own/0789-ab.js:2` (`ab`) → `app/own/0787-rb.js:2` (`rb`) → `app/own/0788-ob.js:2` (`ob`) | simple substring matcher |
| `sortSearchResults` | `app/own/0785-nb.js:2` (`nb`) | in-place score sort |
| `Cy` / `ky` / `Ey` | `app/own/0779-Yy.js:2-4` → `Xy` (ws) / `Yy` (punct) / `Qy` (CJK) | char classes |
| `Ty` (matcher) | `app/own/0783-eb.js:2` (`eb`) | two-mode token matcher |
| `xy` (scoring) | `app/own/0782-Jy.js:2` (`Jy`) | five-term scorer |
| `wy` (merge) | `app/own/0778-Gy.js:2` (`Gy`) | sort + coalesce ranges |
| `Ly` (range collector) | `app/own/1165-$A.js:2` (`$A`) | `RegExp` → `[[start,end) …]` |
| `Iy` (result merge) | `app/own/1162-jA.js:2` (`jA`) | merge per-key range arrays |
| `ub` (collator) | `app/own/0819-ww.js:228-233` (`Mw`/`Sw`) | `Intl.Collator(undefined,{usage:"sort",sensitivity:"base",numeric:true})` |
| `debounce` | `app/own/0355-Ml.js:2` (`Ml`) | lodash-style, 3rd arg = trailing |
| `Cb` / `cb` result offsets shift | `app/own/0786-ib.js:2` (`ib`) | translate `[[s,e]]` by an offset |
| — *(not in the doc)* `jl`/`Ol`/`Al` | `app/own/0364-Ol.js:2`, `app/own/0360-Al.js:2` | strip extension / basename |
| — *(not in the doc)* DSL tokenizer | `app/own/1173-fP.js:2` (`fP`) | query → tokens |
| — *(not in the doc)* DSL parser | `app/own/1167-lP.js:2`, `1168-cP.js:2`, `1169-uP.js:2`, `1170-hP.js:2`, `1171-pP.js:2`, `1172-dP.js:2` | `lP`→`cP`→`uP`→`hP`, token-end + peek helpers |
| — *(not in the doc)* matcher AST | `app/own/1163-UA.js:2` (`UA` base), `app/own/1166-ZA.js` (`ZA` regex, `eP` quote, `tP` text, `nP` group, `iP` and, `rP` or, `oP` op-wrapper, `aP` compare, `sP` not), `app/own/1163-UA.js:21` (`_A`), `:39` (`KA`) | |
| — *(not in the doc)* operator classes | `app/own/1174-vP.js` (`vP` case, `gP` path, `yP` file, `bP` content, `wP` splitter, `kP` line, `CP` block), `app/own/1175-SP.js:2` (`SP` section), `:71` (`xP` task), `:104` (`TP` tag), `:148` (`DP` table), `:222` (`AP`) | |
| — *(not in the doc)* matcher input | `app/own/1160-HA.js:3` (`zA`), `:21` (`qA` key order), `app/own/1164-QA.js:2` (`QA` regex cache) | |
| — *(not in the doc)* quick-switcher | `app/own/1951-e3.js:2` (`e3` file suggester), `app/own/2040-Vee.js:2999` (`kte` modal), `app/own/1333-XR.js:2` (`XR`), `app/own/1949-Z4.js:2` (`Z4`) | |
| — *(not in the doc)* search view | `app/own/1848-Y1.js:209` (`GlobalSearchPlugin`), `:417` (`e0` view), `:3-33` (`X1` operator suggestion table), `:34` (`Q1` suggestion popup) | |
| — *(not in the doc)* `renderMatches` / `renderResults` | `app/own/0025-expressionstatement.js:147` / `:149` → `lb` (`app/own/0791-lb.js:2`) / `sb` (`app/own/0790-sb.js:2`) | matches §13 Q2 |

---

## Unverifiable / out of scope

- `domains/leaf-utilities.md` has **no quick-switcher section** — §1/§10 list
  `debounce`/`moment`/`Platform`/`Keymap`/`SecretStorage`/`network` and nothing about switcher
  scoring, so there was nothing there to check. `ML`/`Ml` (the real debounce, with a `trailing`
  third argument) is documented in `app/own/0355-Ml.js:2-24` and belongs to that domain's follow-up,
  not this one.
- The **English** operator descriptions (`labelPathOptionDescription` and friends) live in
  `RE/obsidian/1.13.7/asar/obsidian/i18n` and were not needed — every claim here quotes ASCII
  literals from the code, never a localised string.
- D7/D8 rely on `testvaults/obsidian-api/obsidian.d.ts`, which is the npm `obsidian` typings
  package at **1.12.3**, not the 1.13.7 asar. The 1.13.7 extraction ships no `obsidian.d.ts`
  (`RE/obsidian/1.13.7/asar/obsidian/` has only `lib/readability.d.ts`), so "which symbols are in
  the typings for 1.13.7 specifically" cannot be settled from this corpus.
- Plugin-facing DSL extensions (whether a plugin can register an operator) were not investigated:
  `AP`'s constructor takes an `operators` argument (`app/own/1175-SP.js:223-224`) defaulting to
  `DP`, and `lP` threads it through, but no public API surface for supplying it was found.

## Why noticed now

The corpus was restored (`RE/obsidian/1.13.7/`), which turned `search.md` §13 Q1 —
"the DSL parser is out-of-tree, reverse-engineer from user documentation" — into a
falsifiable claim. It is false: the whole grammar is a ~60-line operator table at
`app/own/1175-SP.js:148-221`. An implementer following §13 would have hand-built a
parser from documentation and shipped the wrong grammar, including a `whole-word` flag
that does not exist.

The second finding surfaced only because the domain agent was told to check
`docs/PARITY-MATRIX.md` against source: its §5 row claimed quick-switcher fuzzy-scoring
parity was complete, and it is half true.

## Action taken

- `docs/PARITY-MATRIX.md` §5 downgraded ✅ → 🟡 with the specific gap named (constants
  match, penalty rule does not).
- Punched as **P1** in `docs/punch-list.md`, covering both the grammar defects and the
  `FuzzyMatcher` penalty-rule divergence, with the two worked examples.
- §F measures `libs/search` against the recovered grammar so the implementation gap is
  explicit rather than assumed; §G re-anchors every symbol `search.md` cites.
- The `whole-word` / `ignore-case` correction should be checked before any further
  search work — `docs/PARITY-MATRIX.md` does not currently mention either flag.
