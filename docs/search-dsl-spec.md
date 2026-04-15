# Obsidian Global-Search DSL — Grammar & FTS5 Translation

Reverse-engineered from `_internal.js` (Obsidian 1.9.x de-minified bundle) and
`testvaults/obsidian-help/en/Plugins/Search.md`. All JS line refs are
`_internal.js:<line>`. Help docs are `help/Search.md:<line>`.

The search plugin registers itself under `gm.plugins.search` and exposes
`openGlobalSearch(query)` — 348 call sites exist in the bundle; **every one of
them emits `"tag:" + X`** (spot-checked `_internal.js:290927`,
`_internal.js:344256`, `_internal.js:561034`). Obsidian itself never programmatically
emits any other operator, so user input is the only source of grammatical
diversity.

---

## 1. Grammar (EBNF)

```
query       = [ or_expr ]                       // empty query is legal → matches nothing
or_expr     = and_expr { "OR" and_expr }        // left-assoc; see gH  @328590
and_expr    = term { term }                     // whitespace-separated; implicit AND. see yH @328610
term        = unary | primary
unary       = "-" term                          // NOT; bH "not" branch @328723
            | ("<" | ">") term                  // comparator; bH @328728
primary     = operator_call
            | property_call
            | group
            | atom
            | literal_const
operator_call = operator_name ":" term          // token "name" + "colon" + recursive term. bH @328626
operator_name = "path" | "file" | "content"
              | "tag"                           // body must be plain text (cH); "tag" rejects quotes/regex @331260
              | "line" | "block" | "section"
              | "task" | "task-todo" | "task-done"
              | "match-case" | "ignore-case"
property_call = "[" [ or_expr ] [ ":" or_expr ] "]"    // bH @328659
group       = "(" or_expr ")"                   // bH @328702
atom        = TEXT                              // cH: partial-match regex against all keys @325341
            | QUOTE                             // lH: whole-word on content, exact on props @325298
            | REGEX                             // aH: JS RegExp, validated at parse @328689
literal_const = "TRUE" | "FALSE" | "EMPTY"      // CH @328760 + JV @324604. Obscure; used for
                                                //   property-value predicates (e.g. [aliases:EMPTY]).
```

### Tokenization (`CH`, `_internal.js:328754-328926`)

Single-pass scanner, no lookahead. Character classes:

| Trigger | Token type | Notes |
|--|--|--|
| `"` | `quote` | Reads until next unescaped `"`. Backslash escapes next char (`\"`, `\\`). `_internal.js:328897-328923` |
| `/` | `regex` | Reads until next unescaped `/`. `\/`→`/`, `\\`→`\\\\`. Unterminated regex is still accepted at EOF. `_internal.js:328862-328896` |
| `-` | `not` | Single char, always a prefix unary. `_internal.js:328853-328861` |
| `[` `]` | `bracket` | Property boundary. `_internal.js:328802` |
| `(` `)` | `parenthesis` | Grouping. `_internal.js:328811` |
| `:` | `colon` | Operator separator. `_internal.js:328820` |
| `<` `>` | `lessthan`/`greaterthan` | Property-value comparators. `_internal.js:328829-328846` |
| space | (skip) | Token separator. `_internal.js:328847` |
| `OR` (bareword) | `or` | Case-sensitive; `or` is plain text. `_internal.js:328778` |
| `TRUE` `FALSE` `EMPTY` | `true`/`false`/`empty` | Case-sensitive barewords. `_internal.js:328760-328777` |
| else | `text` | Runs up to the next delimiter listed above. |

### Parser (`vH`→`gH`→`yH`→`bH`, `_internal.js:328579-328736`)

- **AND** is implicit concatenation of terms; built by `hH` (`_internal.js:325387`).
- **OR** is left-associative, lower precedence than AND; built by `pH` (`_internal.js:325411`).
- **NOT** (`-`) binds tighter than AND/OR, binds to the next single term; `mH` (`_internal.js:325491`).
- **Precedence (low → high):** `OR` < implicit-AND < `-` unary < `<`/`>` unary < operator-call < atom / group / property.
- **Operators ≠ flags:** operator syntax is `name:term` — `term` can be any primary (text, quote, regex, group, or nested operator — subject to `exclusive` rules).

### `exclusive` / `allowSelf` (`RH` table, `_internal.js:331191-331265`)

All of `path`, `file`, `content`, `line`, `block`, `section`, `task`,
`task-todo`, `task-done`, `tag` are **exclusive** — they may not be nested
inside another exclusive operator. `section` sets `allowSelf:true`, so
`section:(section:foo)` is legal. Violation throws `Operator "X" cannot be
nested within "Y"` at parse time (`_internal.js:328640`).

`tag:` additionally requires the body to be `cH` (plain text) — quotes, regex,
or groups throw `Operator "tag" can only be followed by text`
(`_internal.js:331260`).

`match-case` / `ignore-case` are **not** exclusive — they're case-flag wrappers
(`EH`, `_internal.js:328930-328951`) and compose freely with other operators.

### Flags *not* in the DSL

- **`whole-word:`** does not exist as a DSL operator. It's a per-match-regex
  behaviour: `lH` (quote) uses the whole-word regex for `content` keys
  (`_internal.js:325317`); `cH` (text) uses the partial-match regex everywhere. So
  `"foo"` behaves as whole-word-on-content but partial-on-path/filename.
  The `whole-word` token at `_internal.js:76693` is a CodeMirror6 in-editor find
  extension, unrelated to the global-search DSL.
- **Match-case toolbar button** sets `caseSensitive` on the top-level
  `BH` constructor (`_internal.js:331266`); the DSL `match-case:`/`ignore-case:`
  operators override per-subtree.

---

## 2. Worked examples

| Query | Meaning |
|--|--|
| `meeting work` | files where both `meeting` AND `work` appear somewhere indexed (content, path, filename). `help/Search.md:30`. |
| `meeting OR work` | union — files with either. `help/Search.md:31`. |
| `meeting (work OR meetup) personal` | `meeting ∧ (work ∨ meetup) ∧ personal`. `help/Search.md:39`. |
| `meeting -work` | `meeting ∧ ¬work`. `help/Search.md:43`. |
| `path:"Daily notes/2022-07"` | filename+filepath key restricted to paths containing the quoted substring, whole-word on content (irrelevant here since path key). `help/Search.md:70`. |
| `tag:#work` | exact tag match; does NOT match `#myjob/work` (nested tags). `help/Search.md:74`. |
| `section:(dog cat)` | both `dog` and `cat` appear within the same heading-delimited section. `help/Search.md:77`. |
| `/\d{4}-\d{2}-\d{2}/` | JS regex match against content. `help/Search.md:136`. |
| `path:/\d{4}-\d{2}-\d{2}/` | regex, but restricted to the filepath key. `help/Search.md:140`. |
| `task-todo:call` | uncompleted `- [ ]` task lines containing `call`. `help/Search.md:79`. |
| `[aliases]` | files whose frontmatter has an `aliases` property (any value). `help/Search.md:88`. |
| `[status:Draft OR Published]` | property `status` with value `Draft` or `Published` — sub-query inside the value position. `help/Search.md:103`. |
| `meeting [duration:>5]` | `meeting` appears AND property `duration` > 5 (numeric comparator). `help/Search.md:56`. |
| `[aliases:EMPTY]` | property exists but value is empty. `_internal.js:328772` (EMPTY token) + `help/Search.md:96` (`null` is the user-doc alias, but the scanner keyword is `EMPTY`). |
| `match-case:HappyCat` | case-sensitive subtree. `help/Search.md:72`. |

---

## 3. FTS5 Translatability Matrix

Assume existing schema: FTS5 virtual table `notes_fts(path, title, content)` +
side-tables `note_tags(note_path, tag)` and `links(...)`. A new side-table
`note_properties(note_path, key, value_text, value_num)` is required for the
`[prop:val]` operator; not in scope here but flagged.

| DSL construct | FTS5 strategy | Class |
|--|--|--|
| bare `TEXT` | `notes_fts MATCH 'content:foo OR path:foo OR title:foo'` | (a) trivial |
| `"quoted phrase"` | FTS5 phrase `"foo bar"` in MATCH | (a) |
| `A B` (AND) | `MATCH 'A AND B'` | (a) |
| `A OR B` | `MATCH 'A OR B'` | (a) |
| `-A` / `NOT A` | `MATCH 'X NOT A'` with a candidate set X. FTS5 NOT is binary; bare `NOT A` needs to be rewritten as `(something) NOT A`. For pure `-A` at top level: full-table minus `MATCH 'A'`. | (a) with rewrite |
| `(…)` grouping | FTS5 supports `(...)` in MATCH | (a) |
| `path:foo` | `MATCH 'path:foo'` (indexed column) | (a) |
| `file:foo` | `MATCH 'title:foo'` (our `title` ≈ filename) | (a) |
| `content:foo` | `MATCH 'content:foo'` | (a) |
| `tag:#work` | `SELECT note_path FROM note_tags WHERE tag = 'work'` then INTERSECT with other clauses | (b) extra schema |
| `match-case:X` | FTS5 tokenizer is case-folding; cannot do case-sensitive MATCH. Candidate via case-insensitive FTS, then post-filter with `LIKE BINARY` / Qt `QString::contains(…, Qt::CaseSensitive)` | (c) post-filter |
| `ignore-case:X` | default (no-op unless outer scope is match-case) | (a) |
| `/regex/` | FTS5 has no regex. Use FTS5 candidates from literal bigram fallback if any, else full-content scan. Post-filter with `QRegularExpression`. | (c) post-filter |
| `path:/regex/` | candidate = all rows; post-filter `path` column via regex | (c) |
| `line:(A B)` | FTS5 gives document hits; need per-line post-filter over document content | (c) |
| `block:(A B)` | requires markdown block parsing (tree-sitter-markoff); post-filter over AST | (c) |
| `section:(A B)` | requires heading parse; post-filter over AST slices | (c) |
| `task:X` / `task-todo:X` / `task-done:X` | requires task-block AST + completion state; post-filter | (c) |
| `[prop]` / `[prop:val]` | needs `note_properties` table; trivial SQL once present | (b) |
| `[prop:>5]` / `[prop:<5]` | needs numeric column `value_num`; `WHERE value_num > 5` | (b) |
| `[prop:/re/]` | property table lookup + regex post-filter on `value_text` | (b)+(c) |

Pipeline: **(a) into a single FTS5 MATCH → INTERSECT with (b) side-table
subqueries → post-filter (c) in Qt/C++ over the candidate note set.** `NOT`
at the top level is the one trapdoor that forces a "universe" query.

---

## 4. Edge cases

- **Empty query.** `CH("")` → `[]`; `vH([])` → `gH` returns `null` because `t.length===0` (`_internal.js:328602`). Callers treat null matcher as "match nothing" (`BH` constructor stores null; `match` short-circuits). Implementer must not throw.
- **Whitespace-only query.** Same as empty (scanner consumes spaces as separators, no tokens emitted).
- **Unterminated quote:** `"foo` → emits `{type:quote, content:"foo"}` at EOF (`_internal.js:328901`). No error. Same for regex.
- **Escaped quote inside quote:** `"they said \"hi\""` → content is `they said "hi"` (`_internal.js:328910`).
- **Trailing `OR`:** `foo OR` → `yH` returns `foo` as first operand, `gH` pushes the `or` token and loops; second `yH` call returns `null` → loop breaks, returns `foo`. Silent discard of trailing OR.
- **Trailing operator-colon:** `path:` → `bH` sees text+colon, recurses, gets `null`, substitutes `new cH("")` (`_internal.js:328653`). Empty-text matcher short-circuits to null in `cH.match` (`_internal.js:325357`). Net effect: whole query matches nothing.
- **Unrecognized operator:** throws `Operator "X" not recognized` (`_internal.js:328635`). Parser surfaces as a user-facing error.
- **Invalid regex:** throws `Failed to parse regular expression` (`_internal.js:328695`).
- **Nested exclusive operator:** throws `Operator "X" cannot be nested within "Y"` (`_internal.js:328641`).
- **Tag operand not plain text:** `tag:"x"`, `tag:/x/`, `tag:(a b)` → throws (`_internal.js:331261`).
- **Unbalanced parens/brackets:** extra `)` returns `null` from `bH` which terminates the AND run; extra `(` keeps consuming until EOF (`_internal.js:328706` swallows missing close). No error.
- **Property inside property:** `[a:[b]]` → throws `Property cannot be nested within a property` (`_internal.js:328661`).
- **`OR` is case-sensitive.** `or` (lowercase) tokenizes as `text` and participates in AND. Surprising for users; preserve.
- **`TRUE`/`FALSE`/`EMPTY` are case-sensitive barewords** (`_internal.js:328760-328777`). The help doc says `null` (`help/Search.md:94`) — that is a user-docs-only alias; **the scanner only recognises `EMPTY`**. Flagging this as an audit discrepancy; the docs call it `null` but the code looks for `EMPTY`. Implement both if you want doc-compat.

---

## 5. Summary of citations

Grammar & parser:
- Tokenizer `CH`: `_internal.js:328754-328926`
- Parser entry `vH`/`gH`/`yH`/`bH`: `_internal.js:328579-328736`
- Matcher classes (`aH` regex, `cH` text, `lH` quote, `hH` AND, `pH` OR, `mH` NOT, `sH` property, `EH` case, `JV` const): `_internal.js:325169-325510`
- Operator table `RH`: `_internal.js:331191-331265`
- Top-level `BH`: `_internal.js:331266-331387`

User-facing docs (authoritative for semantics of `task:*`, regex flavour, `null`/`EMPTY` equivalence, `Explain search term`): `testvaults/obsidian-help/en/Plugins/Search.md`.

TypeScript API (`testvaults/obsidian-api/obsidian.d.ts`) is **silent** on the DSL — `prepareSimpleSearch` / `prepareFuzzySearch` live there but neither parses this grammar.
