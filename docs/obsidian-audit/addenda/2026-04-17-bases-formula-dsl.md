# Bases formula / filter DSL — extracted grammar, operator semantics, function catalog

**Date:** 2026-04-17
**Discovered during:** Exploratory spike ahead of Cluster K plan expansion. The Cluster K scouting doc ([`superpowers/plans/2026-04-14-cluster-k-bases-SCOUTING.md`](../../superpowers/plans/archive/2026-04-14-cluster-k-bases-SCOUTING.md)) gates the full plan on this extraction; see "Expand to full plan when" in that doc.
**Supersedes / extends:** Extends `domains/bases.md` §1 (Value hierarchy), §3 (YAML schema), §11 (Missing). No prior coverage of the DSL itself in Pass 2.
**Relevant cluster plans:** [`superpowers/plans/2026-04-14-cluster-k-bases-SCOUTING.md`](../../superpowers/plans/archive/2026-04-14-cluster-k-bases-SCOUTING.md) (expand to full plan citing this addendum).

> **Source provenance.** Obsidian 1.12.7. Primary references: the renamed `_internal.js` tree at `~/src/obsidian-audit/renamed/obsidian/src/` (cited as `renamed/<file>:<line>`) and the formatted single-file view at `~/src/obsidian-audit/formatted/obsidian/app.js` (cited as `formatted/app.js:<line>`). The user-facing help docs consulted were the English pages at `testvaults/obsidian-help/en/Bases/{Bases syntax.md, Formulas.md, Functions.md}`. All line numbers in this addendum are against those two canonical files; they will drift with future Obsidian releases but the surface described should remain stable across 1.x.

---

## 1. Parser architecture

The Bases formula/filter language is parsed by a **Lezer LR parser** (the parser generator used by CodeMirror 6). The filter DSL and the formula DSL are the **same language** — Bases syntax docs explicitly state "the syntax and available functions for filters and formulas are the same". A filter string is simply a formula whose top-level expression is expected to evaluate to a truthy value; filter objects (`and`/`or`/`not`) compose those strings structurally outside the formula grammar (handled in `BasesViewConfig`, not in the DSL parser itself).

The pipeline has four layers:

1. **Lezer parser** (`renamed/_internal.js:387803-387833`, identifier `xK`). Generated from a Lezer `.grammar` specification. Serialized `LRParser.deserialize({ version: 14, states: ..., stateData: ..., goto: ..., nodeNames: ..., tokenData: ..., ... })`. Produces a concrete syntax tree.
2. **Tree-to-AST converter** `FK` (`renamed/_internal.js:388042-388203`, entry `PK`). Walks the Lezer CST and produces a plain-object AST with string `type` tags (`"ident"`, `"paren"`, `"not"`, `"negate"`, `"call"`, `"object_access"`, `"array_access"`, `"math"`, `"compare"`, `"primitive"`, `"regexp"`, `"array"`, plus `"empty"` / `"invalid"` sentinels).
3. **Typed evaluator** — subclasses of `NK` (`renamed/_internal.js:388206-388996`): `BK` (empty), `RK` (invalid), `VK` (comparison/logical), `HK` (arithmetic), `zK` (negate), `qK` (not), `jK` (call), `UK` (array-access), `WK` (object-access), `_K` (paren), `GK` (primitive), `KK` (regexp), `YK` (array), `QK` (ident). Built via `NK.fromParsedResult()` which dispatches on the AST `type` tag.
4. **Formula wrapper** `DK` (`renamed/_internal.js:387838-387858`). Holds the original text + the parsed `NK` tree. `DK.prototype.toString()` returns the source text (used for YAML serialization). `DK.prototype.test(entry)` calls `formula.test(entry)` — `test()` is `this.getValue(e).isTruthy()` on the default `NK` base class. `DK.prototype.getValue(entry)` returns the evaluated `Value`, converting any thrown exception into a `yW` (error-display Value).

A syntactically invalid formula is not a load-time error. `new DK(text)` on a broken formula stores an `RK` (invalid) sentinel as `.formula`; `DK.getValue()` returns `NullValue.value` and `DK.test()` returns `false`. Errors surface through the formula editor's "green checkmark" UI, not through crashes.

---

## 2. EBNF grammar

This grammar is reconstructed from the Lezer `nodeNames` table (`renamed/_internal.js:387810-387811`) and cross-referenced against `FK`'s CST walk. Token shapes from `FK` and the help docs.

```ebnf
Program       = Expression ;

Expression    = LogicalOr ;

LogicalOr     = LogicalAnd { "||" LogicalAnd } ;                 (* left-assoc *)
LogicalAnd    = Equality   { "&&" Equality } ;                   (* left-assoc *)
Equality      = Relational { ("==" | "!=") Relational } ;        (* left-assoc *)
Relational    = Additive   { ("<" | ">" | "<=" | ">=") Additive } ;
Additive      = Multiplicative { ("+" | "-") Multiplicative } ;
Multiplicative = Unary     { ("*" | "/" | "%") Unary } ;

Unary         = ("!" | "-") Unary
              | Postfix ;

Postfix       = Primary { Suffix } ;
Suffix        = "(" [ Expression { "," Expression } ] ")"        (* Call *)
              | "[" Expression "]"                               (* ArrayAccess *)
              | "." Identifier ;                                 (* ObjectAccess *)

Primary       = "(" Expression ")"                               (* GroupedExpression *)
              | Array
              | String
              | RegExp
              | RealNumber
              | BooleanLiteral
              | NullLiteral
              | Identifier ;

Array         = "[" [ Expression { "," Expression } ] "]" ;

(* Terminals *)
Identifier    = ? [A-Za-z_$][A-Za-z_$0-9]* ? ;
NullLiteral   = "null" ;
BooleanLiteral = "true" | "false" ;
RealNumber    = ? JS numeric literal: decimal digits with optional fraction; no exponent or hex observed ? ;
String        = DoubleQuotedString | SingleQuotedString ;
DoubleQuotedString = '"' { <any char except " and \> | Escape } '"' ;
SingleQuotedString = "'" { <any char except ' and \> | Escape } "'" ;
Escape        = '\' <any> ;                                       (* node type Escape, nested under String *)
RegExp        = "/" { <regex body> } "/" { flag } ;               (* JS regex literal syntax *)
```

**Node names from the Lezer grammar (verbatim):**
`⚠, program, LogicalExpression, ||, &&, EqualityExpression, Equality, RelationalExpression, Relation, AdditiveExpression, +, -, MultiplicativeExpression, *, /, %, UnaryExpression, !, Call, (, ), ",", ], ArrayAccess, [, ObjectAccess, ., Identifier, NullLiteral, BooleanLiteral, RealNumber, String, Escape, RegExp, Array, GroupedExpression.`

The `⚠` node is Lezer's error-recovery marker; `OK(node)` (`renamed/_internal.js:388032-388039`) walks the tree for any `⚠` to detect partial parses.

---

## 3. Operator precedence table

From lowest to highest binding. All binary operators are **left-associative** (Lezer LR grammar's default, confirmed by the node nesting in `FK`).

| Precedence | Node type                  | Operators           | Arity   | Associativity | Result type (operand-dependent) |
|------------|----------------------------|---------------------|---------|---------------|---------------------------------|
| 1          | `LogicalExpression`        | `\|\|`              | binary  | left          | `BooleanValue`                  |
| 2          | `LogicalExpression`        | `&&`                | binary  | left          | `BooleanValue`                  |
| 3          | `EqualityExpression`       | `==`, `!=`          | binary  | left          | `BooleanValue`                  |
| 4          | `RelationalExpression`     | `<`, `>`, `<=`, `>=`| binary  | left          | `BooleanValue`                  |
| 5          | `AdditiveExpression`       | `+`, `-`            | binary  | left          | varies (see §4)                 |
| 6          | `MultiplicativeExpression` | `*`, `/`, `%`       | binary  | left          | varies (see §4)                 |
| 7          | `UnaryExpression`          | `!` (not), `-` (neg)| unary   | right         | varies                          |
| 8          | `Call`, `ArrayAccess`, `ObjectAccess` | `()`, `[]`, `.` | postfix | left  | varies                          |
| 9          | `GroupedExpression`, atoms | `(expr)`, literals  | —       | —             | —                               |

> **Note.** `&&` and `||` are handled by the single `LogicalExpression` node (the Lezer grammar lists `LogicalExpression` once with both terminals); precedence between them is encoded in the grammar's state machine. The help doc lists `!` / `&&` / `||` under "Boolean operators" without explicit precedence, but the LR grammar ordering resolves it (`&&` binds tighter than `||`, matching JS/C tradition).
> **Note on `LogicalExpression` short-circuiting.** `VK.getValue` (`renamed/_internal.js:388312-388383`) evaluates `&&`/`||` by calling `left.getValue(e).isTruthy() && right.getValue(e).isTruthy()` — this is JS-level `&&` so it **does short-circuit** (the right side is evaluated only if needed by the surrounding `isTruthy()` call). However, the return value is always a fresh `BooleanValue`, not the last operand (unlike JS). So `a || b` never returns `b` as-is; it returns `true`/`false`.

---

## 4. Type-aware operator semantics

Reconstructed from `VK.prototype.getValue` (comparisons, `renamed/_internal.js:388324-388380`) and `HK.prototype.getValue` (arithmetic, `renamed/_internal.js:388396-388477`).

### 4.1 Equality (`==`, `!=`)

- Delegated to `Value.looseEquals(left, right)`.
- Each `Value` subclass may override `looseEquals` to permit cross-type coercion:
  - `DateValue.looseEquals` tries `DateValue.parseFromString` on a `StringValue` operand before comparing; equality is time-portion-aware (if both dates have time, compare timestamps; otherwise compare date-only).
  - `DurationValue.looseEquals` normalises to `getMilliseconds()` and tries `DurationValue.parseFromString` on a `StringValue` operand.
  - `LinkValue.looseEquals` resolves both sides to a `TFile` (if both resolve → compare file identity; otherwise compare link text). String→Link coercion via `LinkValue.parseFromString`. Link equals `FileValue` if the link resolves to the same file (`renamed/DateValue.js:696-707` — file is misleadingly named, contains multiple Value classes).
  - `FileValue.looseEquals` against `StringValue` compares the file path.
  - Other `Value` types fall back to strict equality.
- Equality is not restricted to any particular type — any pair of Values may be compared.

### 4.2 Relational (`<`, `>`, `<=`, `>=`)

Comparison proceeds in this order (first match wins):

1. If `left` is `DateValue` and `right` is `StringValue` that parses as a `DateValue` → coerce right to `DateValue`. Both sides become millisecond timestamps.
2. If `left` is `DurationValue` and `right` is `StringValue` that parses as a `DurationValue` → coerce. Both sides become millisecond counts.
3. Otherwise, each side becomes: `NumberValue.data` for numbers, else `toString()`.
4. `NullValue` on either side → result is `NullValue.value` (propagates through comparison).

So date↔date, duration↔duration, number↔number are numeric; mixed numeric/string degrades to string comparison (caller's problem).

### 4.3 Arithmetic (`+`, `-`, `*`, `/`, `%`)

Dispatch table (first match wins). `D` = Date, `Du` = Duration, `N` = Number, `L` = List, `S` = String, `Null` = NullValue.

| Left   | Op                  | Right   | Result                                                                       |
|--------|---------------------|---------|------------------------------------------------------------------------------|
| `N`    | `+` `-` `*` `/` `%` | `N`     | `NumberValue` (normal JS math; integer overflow not guarded)                 |
| `D`    | `+` `-`             | `S`     | Coerce `S` → `Du` via `DurationValue.parseFromString`, then see next row.   |
| `D`    | `+` `-`             | `Du`    | `DurationValue.addToDate(date, subtract?)` — adds/subtracts; returns `DateValue` |
| `Du`   | `*` `/`             | `N`     | Scalar multiply/divide on all seven duration components. `*` → componentwise; `/` → componentwise with `1/n`. |
| `Du`   | `+` `-`             | `Du`    | Componentwise add/subtract across `{years, months, days, hours, minutes, seconds, milliseconds}`. |
| `L`    | `+`                 | `L`     | `ListValue.concat` — element-wise concatenation.                             |
| `S` or `N` + `S` or `N`*  | `+`                 | `*`     | If **either** side is `StringValue`, coerce both to strings (via `toString()`) and concatenate. `NullValue` coerces to `""`. |
| `D`    | `-`                 | `D`     | `DurationValue.fromMilliseconds(left.date.getTime() - right.date.getTime())` |
| `Null` | any                 | `*`     | `NullValue.value`                                                            |
| `*`    | any                 | `Null`  | `NullValue.value`                                                            |
| otherwise                       |         |         | `throw new Error("Invalid operator between ...")`                            |

Notable asymmetry: `Du * N` works but `N * Du` **throws** — durations must be on the left for scalar multiply/divide. This matches the help doc's warning: *"When performing arithmetic on durations with scalars, the duration must be on the left."*

Notable divergence from JS: `D + D` is not a date-double-add; it throws. Only `D + Du`, `D - Du`, `D - D` are valid date arithmetic.

### 4.4 Unary

- `!expr` (logical not): if `expr` evaluates to `NullValue` → returns `NullValue` (**null-propagation, not false**); otherwise returns `BooleanValue(!expr.isTruthy())` (`qK`, `renamed/_internal.js:388502-388520`).
- `-expr` (arithmetic negate): if `expr` is a `NumberValue` → `NumberValue(-expr.data)`; if `NullValue` → propagates; otherwise throws "Unable to negate ..." (`zK`, `renamed/_internal.js:388482-388501`). **Constant-folding shortcut:** `NK.fromParsedResult` detects `-<numeric-literal>` at build time and folds it into a single `GK` (primitive) with negated value (`renamed/_internal.js:388234-388239`).

### 4.5 Null propagation summary

- Comparisons: `Null < x` / `Null > x` / etc. → `NullValue`.
- Equality: `Null == x` uses `Value.looseEquals` directly; typically `false` unless the other side is also `Null`.
- Arithmetic: `Null` on either side → `NullValue`.
- Logical: `!Null` → `NullValue`; `&&` and `||` do not propagate `Null` because they test truthiness.
- Function dispatch: if the subject of a `.method()` call is `Null` and the method isn't found on `NullValue`, result is `NullValue` (no error).

---

## 5. Evaluation context & identifier resolution

Evaluators accept a `BasesEntry`-shaped context with three methods (`renamed/BasesEntry.js:70-93`):

- `keys()` — returns the set of resolvable identifiers for auto-complete.
- `getByIdentifier(name)` — resolves bare identifiers.
- `getInsensitive(name)` — case-insensitive object lookup (used by `ObjectValue.objectAccess`).

`BasesEntry.getByIdentifier(name)` dispatches by `name.toLowerCase()`:

| Identifier   | Returns                                                                                      |
|--------------|----------------------------------------------------------------------------------------------|
| `this`       | `sY` wrapper over the **local** (ambient) file — see §5.1                                    |
| `note`       | the frontmatter `ObjectValue` of the current row's file                                      |
| `file`       | the `FileValue` of the current row's file (the "implicit" file)                              |
| `formula`    | an `aY` object exposing the base's formula results (keyed by formula name)                   |
| *anything else* | `note.getInsensitive(name)` — frontmatter property lookup (case-insensitive)              |

So `price` and `note.price` and `note["price"]` all resolve to the same frontmatter property. `file.name`, `file.ctime`, etc. resolve through `FileValue.objectAccess`. `formula.ppu` resolves through `aY.getFormulaValue("ppu")` which **detects cycles** via a sentinel (`rY`) in its `cachedFormulaOutputs` map and throws `msgErrorInfiniteLoop` on re-entry. Formula results are memoised per-entry.

### 5.1 `this`

`this` resolves to an `sY` (subclass of `FileValue`, `type = "ThisFile"`) wrapping a different file depending on where the base is evaluated (`renamed/BasesEntry.js:149-165`, `ctx.local`):

- Base opened in main content area → the `.base` file itself.
- Base embedded in a note or canvas → the embedding file.
- Base in a sidebar → the active file in the main content area (dynamic; re-evaluates on active-file change).

`sY` forwards `objectAccess(name)` into the enclosing entry's `getByIdentifier(name)`, which means `this.file.folder`, `this.file.name`, etc. work by re-entering the dispatch above.

### 5.2 Inline-shadowing contexts (reduce / map / filter)

`jK.getValue` (`renamed/_internal.js:388549-388811`) special-cases `list.reduce`, `list.filter`, `list.map`, `object.filter`, `object.map` by constructing a *shadowing* context that adds identifiers for the lambda body:

- `list.reduce(expr, acc0)` — per iteration: adds `index` (`NumberValue`), `value` (element), `acc` (accumulator).
- `list.filter(predicate)` — per iteration: adds `index`, `value`.
- `list.map(expr)` — per iteration: adds `index`, `value`.
- `object.filter(predicate)` / `object.map(expr)` — per key: adds `key` (`StringValue`), `value`.

The shadowing context's `keys()` returns `ctx.keys().concat(["index", "value", "acc"])` (or similar) so auto-complete surfaces the iteration bindings. `getByIdentifier` checks the iteration-bound names first, then falls through to the outer `ctx.getByIdentifier(name)`.

**Consequence:** the `filter`, `map`, `reduce` built-ins in the function-registry table (§8) are **stubs**. Their `apply` methods return the raw list unchanged. The real semantics live in `jK.getValue`'s dispatch, which pattern-matches on the function name *before* consulting the registry.

Similarly, `if(cond, then, [else])` is hard-cased in `jK.getValue` with its own arity validation — it does **not** go through the global function registry.

---

## 6. Value hierarchy (complete list)

Each class has a `static type` string (used by `isType(name)`) and inherits `objectAccess(key)` for property-like access. Fields are enumerated from `keys()`. Extracted from `renamed/BooleanValue.js`, `NumberValue.js`, `StringValue.js`, `DateValue.js` (which, confusingly, contains several adjacent types), `ListValue.js`, etc.

| Class              | `static type`   | Parent         | Truthy when                               | Key fields / objectAccess                                                                                                 |
|--------------------|-----------------|----------------|-------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| `Value`            | —               | (abstract)     | (abstract)                                | `.isEmpty()`, `.isTruthy()`, `.renderTo(el, ctx)`, `.objectAccess(name)`, `.toString()`, `.keys()`                         |
| `NullValue`        | `"Null"`        | `Value`        | always false                              | singleton `NullValue.value`                                                                                              |
| `NotNullValue`     | —               | `Value`        | (abstract)                                | marker parent                                                                                                            |
| `PrimitiveValue`   | —               | `NotNullValue` | (abstract)                                | parent for `Boolean`/`Number`/`String`                                                                                   |
| `BooleanValue`     | `"Boolean"`     | `PrimitiveValue` | `.data === true`                        | `.data: boolean`                                                                                                         |
| `NumberValue`      | `"Number"`      | `PrimitiveValue` | `.data !== 0 && !NaN`                   | `.data: number`                                                                                                          |
| `StringValue`      | `"String"`      | `PrimitiveValue` | `.data.length > 0`                      | `.data: string`, `length` (via objectAccess → `NumberValue(data.length)`)                                                 |
| `DateValue`        | `"Date"`        | `NotNullValue` | always true                               | `.date: Date`, `.time: boolean`. Fields: `year`, `month` (1–12), `day`, `hour`, `minute`, `second`, `millisecond`, `timestamp` (ms since epoch). |
| `RelativeDateValue` | (inherits `"Date"`) | `DateValue` | always true                             | `toString()` → `moment(date).fromNow()`                                                                                  |
| `DurationValue`    | `"Duration"`    | `NotNullValue` | any non-zero component                    | `.years`, `.months`, `.days`, `.hours`, `.minutes`, `.seconds`, `.milliseconds`. Fields (via objectAccess): `years`, `months`, `weeks`, `days`, `hours`, `minutes`, `seconds`, `milliseconds` — computed as `moment(ref+dur).diff(ref, unit, true)` so `weeks` ≈ `days/7` etc. |
| `ListValue`        | `"List"`        | `NotNullValue` | `.length > 0`                             | `.data: Value[]`. `length` (via objectAccess). Iteration methods: `.get(i)`, `.length()`, `.includes(v)`, `.concat(other)`, `.join(sep)`, `.reverse()`, `.flatten()`, `.unique()`, `.sort()`, `.min()`, `.max()`, `.mean()`, `.median()`, `.sum()`, `.stddev()`, `.earliest()`, `.latest()`. |
| `ObjectValue`      | `"Object"`      | `NotNullValue` | at least one own key                      | `.data: Record<string, Value \| raw>` with lazy evaluation. `.get(key)`, `.getInsensitive(key)`, `.keys()`, `.valuesRaw()`. `objectAccess` uses case-insensitive key lookup. |
| `RegExpValue`      | `"RegExp"`      | `NotNullValue` | always true                               | `.regexp: RegExp`                                                                                                        |
| `FileValue`        | `"File"`        | `NotNullValue` | always true                               | `.app`, `.file: TFile`. Fields: `file` (self), `name`, `basename`, `fullname`, `path`, `folder`, `ext`, `ctime`, `mtime`, `size`, `links` (ListValue of LinkValue), `embeds`, `backlinks` (expensive; see help doc warning), `tags`, `properties`. |
| `sY` ("ThisFile")  | `"ThisFile"`    | `FileValue`    | always true                               | forwards `objectAccess` into the owning `BasesEntry.getByIdentifier`                                                     |
| `TagValue`         | (inherits `"String"`) | `StringValue` | non-empty                           | `.data` (canonicalised), `.lowerTag`. `.tagMatches(other)` → prefix-match with `/` boundary.                              |
| `DW` (TagList)     | (no separate type, inherits `"List"`) | `ListValue` | non-empty               | wraps a list of `TagValue`; `.includes(v)` dispatches through `tagMatches`. Used only by `file.tags`.                   |
| `LinkValue`        | `"Link"`        | `StringValue`  | always true                               | `.data` (link text), `.sourcePath`, `.display`, `.app`. `.resolve()` → `TFile \| null`.                                  |
| `UrlValue`         | `"URL"`         | `StringValue`  | always true                               | `.data` (URL), `.display`.                                                                                               |
| `IconValue`        | (no `type` field observed; labelled `"Icon"` in docs) | `StringValue` | always true | `.data` (lucide icon name); renders as `<icon>`.                                                                         |
| `ImageValue`       | `"Image"`       | `StringValue`  | always true                               | `.data` (path or wikilink-body); renders as `<img>`.                                                                     |
| `HTMLValue`        | `"HTML"`        | `StringValue`  | always true                               | `.data` (sanitised HTML).                                                                                                |
| (Markdown variant) | `"Markdown"`    | `StringValue`  | always true                               | separate class (anonymous in source), `.data` (markdown source); rendered via full markdown renderer.                    |

**Closed hierarchy.** Plugins cannot subclass `Value`. Obsidian's closed hierarchy flows from the per-type function registry (`YW.instance: Map<Class, ...>`) which matches against `value instanceof Class`; an unknown `Value` subclass would have no registered functions.

### 6.1 String parsing coercions

Several `Value` classes have `static parseFromString(text)` methods that are probed opportunistically when a string appears in a typed context:

- `DateValue.parseFromString` — matches `^\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}(:\d{2}(?:\.\d{1,9})?)?(Z|[+-]\d{2}(:?\d{2})?)?$` (date+time) or `^\d{4}-\d{2}-\d{2}$` (date-only).
- `DurationValue.parseFromString` — matches ISO 8601 `P[nY][nM][nW][nD]T[nH][nM][nS]` **or** shorthand `^(-?\d+)\s*(unit)$` where `unit` is one of: `y|year|years`, `M|month|months`, `w|week|weeks`, `d|day|days`, `h|hour|hours`, `m|minute|minutes`, `s|second|seconds`, `ms|millisecond|milliseconds` (the help doc omits `ms`).
- `LinkValue.parseFromString` — matches `[[...]]` with optional `|display`. Internal anchors (`#heading`, `^block-id`) not split out at parse time.

Frontmatter ingestion uses these in the `ObjectValue.fromFrontMatter` lazy evaluator (`renamed/DateValue.js:116-137`): strings that parse as a Link, URL (via `Qc`), or Date are auto-upgraded; anything else falls through to `zW(raw)` (the generic value-wrapper).

---

## 7. String-literal escape rules (from `FK`)

Double-quoted strings are parsed via `JSON.parse(source)` — full JS/JSON escape set (`\n`, `\t`, `\"`, `\\`, `\uXXXX`, etc.).

Single-quoted strings are rewritten before `JSON.parse`:

1. Strip surrounding `'...'`.
2. `\'` → `'` (unescape single-quote).
3. Escape any naked `"` → `\"`, carefully leaving the originally-escaped `\"` sequence alone (via a split-then-rejoin on `\\"`).
4. Re-wrap as `"..."`.
5. `JSON.parse`.

Net effect: `'it\'s "cool"'` → the JS string `it's "cool"`. `'already\\"escaped'` round-trips its escaped quote. The transformer is at `renamed/_internal.js:388132-388155`.

---

## 8. Function catalog

Built-ins register through the global `QW = new YW()` registry. `YW.addGlobal(fn)` adds by `fn.name.toLowerCase()`; `YW.addForType(ValueClass, fn)` adds per-type. Lookup at evaluation:

1. If the call has a subject (e.g. `x.foo(...)`), walk `subject.value.type`'s prototype chain, stopping at `Value` — use the first class whose per-type table has the function; otherwise `findGlobal(name)`.
2. Else, `findGlobal(name)`.
3. Four names are hard-cased in `jK.getValue` and **never** consult the registry even if registered: `if`, `list.reduce`, `list.filter`, `list.map`, `object.filter`, `object.map`. Registrations for `map`/`filter`/`reduce` exist as stubs for auto-complete + type signatures only; their `apply` bodies return the receiver unchanged.

### 8.1 Parameter descriptors

Three helpers build `params` entries (`renamed/_internal.js:369380-369425`):

- `jW(name, ...ValueClass)` → `{name, type: [ValueClass, ...]}` — required; union of allowed `Value` subclasses.
- `UW(name, ...ValueClass)` → `{..., optional: true}` — optional (arg count may stop before this param).
- `WW(name, ...ValueClass)` → **returned as a one-element array** `[{..., variadic: true}]` — the variadic tail (captures all remaining args).
- `_W(name, customWidget, ...ValueClass)` → `{..., customWidget}` — has a dedicated UI editor widget (e.g. property-picker).

### 8.2 Global functions

Extracted from `formatted/app.js:119463-119865`. Names are canonical (lowercased at lookup).

| Function     | Signature                                                  | Semantics                                                                                         |
|--------------|------------------------------------------------------------|---------------------------------------------------------------------------------------------------|
| `now()`      | `() → DateValue`                                           | Current date+time (time=true).                                                                    |
| `today()`    | `() → DateValue`                                           | Current date, time component zeroed (time=false).                                                 |
| `date(str)`  | `(string) → DateValue`                                     | Parse `str` via `DateValue.parseFromString`. Returns error Value on parse failure.                |
| `if(cond, then, else?)` | `(any, any, any?) → any`                        | **Hard-cased in `jK.getValue`.** Evaluates `cond` first; short-circuits. `else` defaults to `null`. |
| `random()`   | `() → NumberValue`                                         | `Math.random()`. Refreshes when view reloads; stable within a view render.                        |
| `min(a, b, ...)` | `(number, number, ...) → NumberValue`                  | Variadic minimum.                                                                                 |
| `max(a, b, ...)` | `(number, number, ...) → NumberValue`                  | Variadic maximum.                                                                                 |
| `list(elem)` | `(any) → ListValue`                                        | If `elem` is a list → return unchanged; else wrap in a 1-element list. Used to normalise single-vs-list frontmatter. |
| `link(path, display?)` | `(string \| FileValue, any?) → LinkValue`         | Build a `LinkValue` pointing at `path`. Second arg overrides display text (string or icon).        |
| `number(x)`  | `(any) → NumberValue`                                      | Coerce: `Date` → ms-since-epoch; `Boolean` → 0/1; `String` → `parseFloat` (error on `NaN`); `Number` → identity. |
| `duration(str)` | `(string) → DurationValue`                              | `DurationValue.parseFromString`. Required when arithmetic-on-durations (e.g. `duration("1d")*2`). |
| `image(path)` | `(string \| FileValue \| UrlValue) → ImageValue`          | Build `ImageValue`. Renders as `<img>` in views.                                                  |
| `icon(name)` | `(string) → IconValue`                                     | Lucide icon name. Renders as `<svg>` in views.                                                    |
| `file(path)` | `(string \| FileValue \| UrlValue) → FileValue`            | Resolve to a `FileValue` (null-propagates if path doesn't resolve in the vault).                   |
| `html(str)`  | `(string) → HTMLValue`                                     | Mark string as HTML; sanitised before render.                                                     |
| `escapeHTML(str)` | `(string) → StringValue`                              | HTML-escape special chars for safe inclusion in an `html(...)` payload.                            |

### 8.3 Any (parent-`Value` methods — applicable to any non-null receiver)

Registered on `Value` class, so inherited by all subclasses. (`formatted/app.js:119869-119937`.)

| Function | Signature | Semantics |
|----------|-----------|-----------|
| `x.toString()` | `any → StringValue` | Delegates to `.toString()` on the receiver. |
| `x.isTruthy()` | `any → BooleanValue` | `x.isTruthy()`. |
| `x.isType(name)` | `any, string → BooleanValue` | Returns `true` if receiver's class has `static type === name`. |
| `x.isEmpty()` | `any → BooleanValue` | Type-specific: `StringValue` = `.length === 0`; `NumberValue` = `!== present` (always false for non-null); `ListValue` = `.length === 0`; `ObjectValue` = no own keys; `DateValue` = always false. Registered separately for each type (see below) but the help docs group them under each type. |

### 8.4 String methods

On `StringValue` (registrations `formatted/app.js:119958-120266`):

| Method | Signature |
|--------|-----------|
| `s.startsWith(q)` | `(string) → boolean` |
| `s.endsWith(q)`   | `(string) → boolean` |
| `s.trim()`        | `() → string` |
| `s.title()`       | `() → string` — title-case (first letter each word). |
| `s.isEmpty()`     | `() → boolean` |
| `s.replace(pattern, replacement)` | `(string \| RegExpValue, string) → string` — plain string replaces all; RegExp honours `g` flag. |
| `s.reverse()`     | `() → string` |
| `s.lower()`       | `() → string` |
| `s.split(sep, n?)` | `(string \| RegExpValue, number?) → ListValue<StringValue>` |
| `s.contains(v)`   | `(string) → boolean` |
| `s.containsAny(...vs)` | variadic `(string...) → boolean` |
| `s.containsAll(...vs)` | variadic `(string...) → boolean` |
| `s.slice(start, end?)` | `(number, number?) → string` |
| `s.repeat(n)`     | `(number) → string` |

**No `s.upper()`, no `s.length()` (length is a *field*, accessed as `s.length`).**

### 8.5 Number methods

On `NumberValue` (registrations `formatted/app.js:120301-120398`):

| Method | Signature |
|--------|-----------|
| `n.round(digits?)` | `(number?) → number` — no arg rounds to integer; `digits` rounds to decimal places. |
| `n.ceil()`  | `() → number` |
| `n.floor()` | `() → number` |
| `n.abs()`   | `() → number` |
| `n.toFixed(precision)` | `(number) → string` (returns `StringValue`, not `NumberValue`). |
| `n.isEmpty()` | `() → boolean` |

### 8.6 Date methods

On `DateValue` (registrations `formatted/app.js:120985-121090`). Fields see §6.

| Method | Signature |
|--------|-----------|
| `d.format(fmt)` | `(string) → string` — Moment.js format string. |
| `d.date()`      | `() → DateValue` — time component removed. |
| `d.time()`      | `() → string` — time-of-day portion as `HH:mm:ss`. |
| `d.relative()`  | `() → string` — `moment(d).fromNow()`, e.g. `"3 days ago"`. |
| `d.isEmpty()`   | `() → boolean` — always false. |

### 8.7 List methods

On `ListValue` (registrations `formatted/app.js:120440-120869`). Fields see §6.

| Method | Signature |
|--------|-----------|
| `l.earliest()` | `() → DateValue` — earliest date in list (expects list-of-dates). |
| `l.latest()`   | `() → DateValue` — latest date. |
| `l.median()`   | `() → NumberValue` — median of numeric list. |
| `l.mean()`     | `() → NumberValue` — arithmetic mean. |
| `l.max()`      | `() → NumberValue` |
| `l.min()`      | `() → NumberValue` |
| `l.sum()`      | `() → NumberValue` |
| `l.stddev()`   | `() → NumberValue` — population std dev (probably; sample-vs-population TBD at impl time — confirm via unit test). |
| `l.join(sep)`  | `(string) → string` |
| `l.reverse()`  | `() → list` |
| `l.flat()`     | `() → list` — single-level flatten (registered as `flat`, calls `.flatten()`). |
| `l.unique()`   | `() → list` — dedupe via element-wise `Value.looseEquals`. |
| `l.contains(v)`     | `(any) → boolean` — uses `ListValue.includes` which dispatches through element-wise `.equals`. For `TagList` (via `DW.includes`), uses `tagMatches` (nested-tag prefix rule). |
| `l.containsAny(...vs)` | variadic |
| `l.containsAll(...vs)` | variadic |
| `l.slice(start, end?)` | `(number, number?) → list` |
| `l.sort()`     | `() → list` — numeric-or-string natural sort, ascending (confirm impl semantics at test time). |
| `l.map(expr)`    | **(special-cased in `jK`; see §5.2)** `(expr: any) → list` — lambda over `value`, `index`. |
| `l.filter(pred)` | **(special-cased)** `(pred: boolean) → list` — lambda over `value`, `index`. |
| `l.reduce(expr, acc0)` | **(special-cased)** `(expr: any, acc: any) → any` — lambda over `value`, `index`, `acc`. |
| `l.isEmpty()`  | `() → boolean` |

> **Important.** The summary-formula helpers in §9 use these same list methods — `values.mean()`, `values.sum()`, etc. — because a summary formula is evaluated with the `values` identifier bound to a `ListValue` of that property's values across the result set.

### 8.8 Object methods

On `ObjectValue` (registrations `formatted/app.js:120880-120964`):

| Method | Signature |
|--------|-----------|
| `o.isEmpty()` | `() → boolean` |
| `o.keys()`    | `() → ListValue<StringValue>` |
| `o.values()`  | `() → ListValue` |
| `o.map(expr)` | **(special-cased in `jK`)** `(expr) → list` — lambda over `key`, `value`; returns a list (not an object). |
| `o.filter(pred)` | **(special-cased)** `(pred: boolean) → object` — lambda over `key`, `value`; returns a filtered object. |

### 8.9 RegExp methods

On `RegExpValue` (registrations `formatted/app.js:120985`):

| Method | Signature |
|--------|-----------|
| `r.matches(s)` | `(string) → boolean` — `r.regexp.test(s)`. |

### 8.10 Link methods

On `LinkValue` (registrations `formatted/app.js:121260-121309`):

| Method | Signature |
|--------|-----------|
| `l.asFile()` | `() → FileValue \| NullValue` — `.resolve()` → TFile → wrap. |
| `l.linksTo(other)` | `(FileValue \| string) → boolean` — target `file.hasLink(other)` lookup. |
| `l.matches(pattern)` | (observed in source; help docs don't list it for Link — likely inherited via RegExp-coerce; treat as unconfirmed surface pending Cluster K test.) |

### 8.11 File methods

On `FileValue` (registrations `formatted/app.js:121111-121286`):

| Method | Signature |
|--------|-----------|
| `f.asLink(display?)` | `(string?) → LinkValue` |
| `f.hasLink(other)`   | `(FileValue \| string) → boolean` — checks metadataCache link entries. |
| `f.inFolder(path)`   | `(string) → boolean` — folder-prefix match, includes sub-folders. |
| `f.hasTag(...tags)`  | variadic `(string...) → boolean` — includes nested tags (prefix match on `/` boundary). |
| `f.hasProperty(name)` | `(string) → boolean` |

### 8.12 Not documented publicly but observed

- The parser accepts `/regex/flags` literals; these become `RegExpValue`. The help docs list them only under `String.replace` / `String.split` contexts, but they're first-class literals in the grammar (node type `RegExp`).
- Identifier `null` / `true` / `false` are reserved — `NY` (`renamed/_internal.js:410168-410170`) explicitly throws if they appear as the LHS of a formula.
- String literals are never identifiers — there is no `x["name with spaces"]` field-access on frontmatter keys via a string expression other than the literal `.["..."]`/`["..."]` forms. `ObjectValue.objectAccess` is case-insensitive (`getInsensitive`), so `note.Author` and `note.author` are equivalent.

---

## 9. Summary formulas

Default summary formulas are expressed **in the same DSL** — they're just formulas with `values` (the list of per-row values) as an implicit identifier. Defined in the `XK` Map at `renamed/_internal.js:388997-389033`:

| Name        | Input type       | Formula source                                         |
|-------------|------------------|--------------------------------------------------------|
| `Average`   | Number           | `values.mean().round(2)`                                |
| `Min`       | Number           | `values.min()`                                          |
| `Max`       | Number           | `values.max()`                                          |
| `Sum`       | Number           | `values.sum().round(2)`                                 |
| `Range`     | Number           | `values.max() - values.min()`                           |
| `Median`    | Number           | `values.median()`                                       |
| `Stddev`    | Number           | `values.stddev().round(2)`                              |
| `Earliest`  | Date             | `values.earliest()`                                     |
| `Latest`    | Date             | `values.latest()`                                       |
| `Range`     | Date             | `(values.latest() - values.earliest())`                 |
| `Checked`   | Boolean          | `values.filter(value == true).length`                   |
| `Unchecked` | Boolean          | `values.filter(value == false).length`                  |
| `Empty`     | Value (any)      | `values.filter(value.isType("null")).length`            |
| `Filled`    | Value (any)      | `values.filter(!value.isType("null")).length`           |
| `Unique`    | Value (any)      | `values.unique().length`                                |

Custom summary formulas go in the `.base`'s top-level `summaries:` map and follow the same conventions (receiving `values: List` in their context).

---

## 10. Filter structure (outside the DSL grammar)

Filters in the `.base` YAML compose DSL expressions structurally. This is **not** part of the DSL grammar — it's the `BasesViewConfig` loader's responsibility. Shape from `domains/bases.md` §3 + `renamed/BasesViewConfig.js:175-265`:

```yaml
filters:
  # Option A: a single filter-string (same grammar as a formula expression)
  "status == 'Done'"
  # Option B: a filter-object with exactly one of and / or / not,
  # whose value is a heterogeneous list of filter-strings or filter-objects.
  or:
    - file.hasTag("book")
    - and:
        - "price > 10"
        - "status == 'open'"
    - not:
        - file.hasTag("archived")
```

- **`and` / `or`**: short-circuit boolean reduction over child results.
- **`not`**: logical-NOT applied to the conjunction of its children (effectively `!(child[0] && child[1] && ...)`).
- The global `filters:` key applies to all views; each `view.filters` filter applies *on top of* the global one (they concatenate with AND).

A bare filter-string must evaluate to something truthy/falsy; `isTruthy()` is called on the result.

---

## 11. Identifier & `.base` property naming conventions

- **Bare identifiers in formulas** → frontmatter property lookup (`note.<name>` implicit).
- **Prefixed identifiers**: `note.`, `file.`, `formula.`, `this.` — resolved via `BasesEntry.getByIdentifier` (§5).
- **Property names exposed to the `properties:` config section** use the **prefix syntax** as the key:
  - `note.status:` → frontmatter key `status`
  - `file.ext:` → built-in file field `ext`
  - `formula.ppu:` → user-defined formula `ppu`
  - `status:` (unprefixed) → also works; same as `note.status`.
- Property IDs from YAML go through `parsePropertyId(str)` (`renamed/parsePropertyId.js`) which splits on the first `.` into `{type, name}` (type ∈ `"note" | "file" | "formula" | unprefixed-defaults-to-note`).

---

## 12. Error surfaces & parse-failure modes

- **Tokenizer failure or unexpected character**: the Lezer parser emits a `⚠` node; `OK(root)` detects it and `PK` returns `{type: "invalid", parseError, rawValue}`.
- **Parse succeeds but trailing/leading whitespace-noise**: `PK` checks `text.substring(0, root.from).trim() === ""` (and symmetric for the end) and returns `invalid` otherwise.
- **`FK` unknown node**: throws `"Unknown node type \"X\" ..."` — caught by `PK` and converted to `invalid`.
- **`NK.fromParsedResult` unknown AST type**: throws `"Invalid type X"` — caught by `NK.parse`, converted to `RK` (invalid sentinel, stores error message).
- **Runtime errors during evaluation** (type mismatch in arithmetic, unknown function, wrong arg count, wrong arg type, infinite formula-reference loop): thrown from the evaluator; caught by `DK.getValue` and wrapped into a `yW` error-Value that renders as an error badge in table cells.
- **User-facing messages** are localised via `TK.msgErrorNotEnoughArguments`, `TK.msgErrorTooManyArguments`, `TK.msgErrorTypeError`, `TK.msgErrorInvalidFunction`, `TK.msgErrorInvalidInstanceFunction`, `TK.msgErrorInvalidArrayAccess`, `TK.msgErrorInvalidObjectAccess`, `TK.msgErrorParseFormula`, `TK.msgErrorMustBeAType`, `TK.msgErrorFormulaValues`, `TK.msgErrorInfiniteLoop` (from `TK = gm.formulas`).

---

## 13. Plugin surface

From `renamed/_internal.js:733870-733883` (and duplicates from identical code chunks at 734252, 734633, 735014, 735395 — they're the same plugin API shape re-emitted per source chunk):

```js
// In Plugin base class:
this.registerGlobalFunc(fn);
// — fn = {name, params: [{name, type:[ValueClass], optional?, variadic?}], apply(ctx, ...args), docString?}
// — wraps a teardown in this.register(() => QW.removeGlobal(fn.name))

this.registerInstanceFunc(ValueClass, fn);
// — adds fn for a specific Value subclass
// — wraps a teardown in this.register(() => QW.removeForType(ValueClass, fn.name))
```

Plugins **cannot**:

- Add new `Value` subclasses (no `registerValueType` equivalent; closed hierarchy). A plugin could construct a subclass and return instances from its registered functions, but `isType("MyType")` would only work if `static type = "MyType"` is set on the subclass — and no other built-in would dispatch on it.
- Add new operators.
- Override the hard-cased functions (`if`, `list.map`, `list.filter`, `list.reduce`, `object.map`, `object.filter`).
- Hook the parser / AST (the Lezer grammar is shipped pre-serialised).

---

## 14. Observed divergence between help docs and implementation

| Surface             | Help doc says                                       | Implementation says                                                             |
|---------------------|-----------------------------------------------------|---------------------------------------------------------------------------------|
| `duration` shorthand | Units listed: y/M/w/d/h/m/s (no `ms` / `millisecond`) | Also accepts `ms`, `millisecond`, `milliseconds`                                |
| `number(bool)`      | "Booleans will return a 1 or 0"                     | Confirmed; uses `bool ? 1 : 0`                                                  |
| `list.sort()`       | "smallest to largest"                               | Implementation uses `.sort()` without a comparator; `ListValue.sort` details not re-confirmed in this pass. **Unit-test at Cluster K impl time.** |
| `date.year` etc.    | Documented fields only                              | Also `date.timestamp` (ms since epoch) is exposed via `objectAccess`            |
| `!NullValue`        | Not addressed                                       | Returns `NullValue` (propagates), not `true` as JS `!null` would                |
| Arithmetic `D + D`  | Not documented (only `D - D` is documented)         | **Throws.** Intentional — only `D + Du` / `D - Du` / `D - D` are valid.        |
| `N * Du`            | Docs warn "duration must be on the left"            | Confirmed; throws if duration is on the right                                   |
| `file.backlinks`    | Warns "performance heavy"                           | Confirmed — eagerly scans `metadataCache.resolvedLinks`, cached per FileValue  |
| `file.properties`   | Documented                                          | Works via `FileValue.objectAccess("properties")` but not in the `FILE_PROPERTIES` constant used for auto-complete (`renamed/BasesEntry.js:103-118`) |

None of these contradict the help docs; they are *extensions* beyond the documented surface. Plan accordingly at Cluster K test-writing time (spec both the documented contract and the observed behaviour).

---

## 15. Implementation options for Cluster K (non-prescriptive)

The scouting doc punts implementation choice to Cluster K planning; recording observations here for the planning phase.

### 15.1 Parser choice

1. **Port the Lezer grammar to tree-sitter.** Reconstruct the `.grammar` file from the node-names + precedence table in §3. Tree-sitter is already a dep via `libs/markoff-parser`. Produces the same CST node names; the AST-walker port from `FK` is mechanical. *Risk:* tree-sitter GLR doesn't exactly match Lezer's LR, so edge-case error recovery may differ — harmless for valid inputs.
2. **Hand-rolled Pratt parser in C++.** ~250 lines. Zero new runtime deps. Easier to debug in C++ toolchain. *Risk:* subtle precedence/associativity bugs; mitigate with a golden-file test corpus generated from Obsidian's parser.
3. **Transliterate the Lezer serialised state machine.** The 1.4KB `stateData` blob at `renamed/_internal.js:387808` could be ported verbatim if we implement a Lezer-style LR driver in C++. Highest fidelity, highest engineering cost, ongoing drift risk.

### 15.2 Evaluator choice

`FK`'s AST + `NK` dispatcher is 600–800 lines of transparent JS. A C++ port is a straight transliteration. `std::variant<NullValue, BooleanValue, NumberValue, StringValue, DateValue, DurationValue, ListValue, ObjectValue, FileValue, LinkValue, UrlValue, IconValue, ImageValue, HTMLValue, TagValue, RegExpValue>` vs `std::unique_ptr<Value>` is still a real trade-off (noted in scouting §"Key architectural questions" #1). Recommendation: **heap-allocated polymorphic hierarchy with `QSharedPointer<Value>`** — matches Obsidian's JS object identity for `is instanceof` checks, avoids `std::visit` verbosity on a 16-type variant, and fits Qt conventions.

### 15.3 Out of scope for Cluster K MVP (deferrable)

- The formula **editor**: tree-sitter-based syntax highlighting + autocomplete is deferrable until after the evaluator ships. Plain-text editing is adequate for MVP.
- The summary-formula **UI** (the dropdown of default summaries in `XK`): pre-bake the 15 defaults; custom summaries come from `summaries:` config.
- `registerGlobalFunc` / `registerInstanceFunc` plugin surface: pure Cluster N work; not an MVP concern.

---

## Why noticed now

Cluster N (plugin-ready surfaces) closed 2026-04-17. With N done, user asked "what's left?" The answer surfaced Cluster K as the largest remaining parity item, gated on this DSL extraction. Decision: do it now rather than leave a 90-minute spike blocking a multi-week cluster.

## Action taken

- This addendum written.
- Will append pointer to `docs/obsidian-audit/00-taxonomy.md` under `## Addenda` (next commit).
- Cluster K scouting doc (`superpowers/plans/2026-04-14-cluster-k-bases-SCOUTING.md`) unblocks: "Expand to full plan when the Bases DSL is extracted" condition is now satisfied. Actual plan expansion remains a separate work item.
- No cluster plan citations to update yet (no in-flight cluster consumes this).
- No implementation behaviour chosen here — §15 options are for Cluster K planning, not decisions.
