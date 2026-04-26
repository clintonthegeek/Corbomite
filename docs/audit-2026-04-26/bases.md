# Bases domain audit

**Audit date:** 2026-04-26
**Spec:** `/home/clinton/dev/Corbomite/docs/obsidian-audit/domains/bases.md`
**DSL addendum:** `/home/clinton/dev/Corbomite/docs/obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md`
**Implementation under review:** `/home/clinton/dev/Corbomite/libs/bases/`
**Cluster K context:** Per project memory, Cluster K (Bases MVP) closed 2026-04-17 — "hand-rolled Pratt parser + shared_ptr Value hierarchy + QTableView widget wired as built-in view (not plugin)." This audit measures parity against the canonical Pass 2 spec + the formula-DSL addendum.

The Bases gap was flagged in Pass 1 as **the largest single feature gap** in Corbomite. The headline finding: that gap has been **substantially closed at the runtime layer**. Corbomite ships a complete typed-Value hierarchy (all 19 spec types except the `MarkdownValue` rendering path), a working Pratt parser + tree evaluator covering the entire formula grammar including hard-cased lambdas (`if`/`map`/`filter`/`reduce`), a YAML `.base` round-trip, a `QTableView`-backed widget registered as a first-class view-type for `.base` extensions, and an incremental-refresh `QueryController` driven by `MetadataCache::cacheChanged`. What is **not** closed is everything plugin-extensible (no `registerView`/`registerGlobalFunc` plugin surface), most rich cell rendering (no inline image/HTML/Markdown render — the cell delegate paints text or checkmarks only), and the polished toolbar UI surfaces (no properties drawer, no formula editor, no group-rendering, no export).

## Architecture fit

The library mirrors the Obsidian Pass 2 decomposition almost one-to-one:

| Obsidian | Corbomite |
|---|---|
| `Value` + 18 subclasses | `Value` (`Value.h:21`) + ~21 subclasses in `Values.h:27-460` |
| `BasesEntry`, `QueryContext` | `BasesEntry` (`BasesEntry.h:28`) — collapses `QueryContext`; no `local`/`localUsed` flag |
| `Filter`/`FilterRule`/`FilterConjunction` | `FilterNode`/`FilterRule`/`FilterConjunction` (`FilterTree.h`) |
| `BasesViewConfig` / `BasesQuery` | Same names (`BasesViewConfig.h:41`, `BasesQuery.h:18`) |
| `BasesEntryGroup`, `BasesQueryResult` | Same names (`BasesQueryResult.h:19, 32`) |
| `BasesView` (`HX`, `TextFileView` subclass) | `Corbomite::Bases::BasesView` (`BasesView.h:32`) — `TextFileView` subclass |
| `QueryController` (out-of-domain in JS source) | `QueryController` (`QueryController.h:29`) — Corbomite owns it |
| Lezer LR parser | Hand-rolled Pratt parser (`Parser.cpp`) + tree-walking evaluator (`Evaluator.cpp`) |
| Cell renderers `d$`/`f$`/`m$` | Single `BasesCellDelegate` dispatching by `ValueTypeRole` (`BasesCellDelegate.h:19`) |
| Built-in catalog (`addGlobal`/`addForType`) | `FunctionRegistry` + `Builtins.cpp` |

Translation choices that diverge from the audit's "Recommended" path:

- **Pratt parser, not Lezer-port.** Addendum §15.1 option 2; `Parser.cpp:32-55` is a 25-line binding-power table that diffs cleanly against addendum §3 precedence.
- **`std::shared_ptr<Value>` hierarchy, not `std::variant`.** Addendum §15.2 explicitly recommended this; everything uses `dynamic_cast`. RTTI-heavy; fine for a few thousand notes, watch at 100k+.
- **`QueryController` is in-domain.** Pass 2 noted Obsidian's source for this is missing (Open Q2/Q3); Corbomite owns it in `src/QueryController.cpp`.
- **`BasesView` is a `TextFileView` subclass** — matches audit's `HX`. Registration via `m_viewRegistry->registerViewWithExtensions({"base"}, "bases", ...)` (`MainWindow.cpp:1533-1535`).

`libs/bases/` is a static library, not a plugin .so — matches project memory ("wired as built-in view (not plugin)") and the project's plugin-API-stability principle.

## MVP coverage assessment

Starting from audit §9 "Observable user features" — the de-facto MVP definition:

| Feature | Status | Where |
|---|---|---|
| Open `.base`; renders via ViewRegistry binding | P | `MainWindow.cpp:1533-1535`, `BasesView.cpp:70` |
| Table of vault notes matching filter | P | `QueryController::recomputeNow` (`QueryController.cpp:74-116`) |
| Columns for `note.*`, `file.*`, `formula.*` | P | `BasesEntry::getValue` (`BasesEntry.cpp:84-95`); columns from `BasesQueryResult::properties` |
| Inline-edit frontmatter cells | pa | `BasesTableModel::setData` → `m_fm->processFrontMatter`, `PropertyKind::Note` only; `QLineEdit`/`QCheckBox`/`QDoubleSpinBox`/`QDateEdit` editors only |
| Sort by header click, multi-key cycle | pa | `BasesView::onHeaderClicked` (`BasesView.cpp:147-164`); single-key only — replaces sort, no shift-click multi-key |
| Group rows by property | pa | `BasesQueryResult::groups()` computes partitions; `BasesTableModel` is flat — groups computed but invisible |
| Filter (global + per-view, AND-merged) | P | `QueryController.cpp:96-97` |
| Multiple named views with switcher | P | `QComboBox`; `BasesQuery::getViewConfig(name)` falls back to `views.front()` per invariant |
| Define formulas; reference as columns | P | `BasesEntry::formulaValue` (`BasesEntry.cpp:97-114`) |
| Search across rendered cells (debounced) | pa | `QueryController.cpp:100-110`, 50ms debounce. Only scans `m_cfg->order` columns |
| Row limit cap | P | `BasesQueryResult::applyLimit`; preserves `limit: 0 = unlimited` |
| Export CSV/TSV/Markdown/`obsidian/table` | M | No exporter, no `QMimeData::setData` |
| `+ New` button | M | No UI; `newItemFolder`/`newItemTemplate` parse but unused |
| Drag-reorder columns | P | `setSectionsMovable` + `onSectionMoved` writes back to `order` |
| View-rename auto-rewrites `[[basefile#viewname]]` | M | No `MetadataCache::updateInternalLinks` callout |
| Drag file-link cells as wikilinks | M | No delegate `mimeData` |
| Right-click cells for file context menu | M | No `contextMenuEvent` |
| Hover file-link cells (page-preview) | M | No `hover-link` emission |
| Embed `.base` in markdown as `![[my.base]]` | M | Out of `bases/` scope; not in EmbedRegistry either |
| Sidebar-mode tracks active md file as `this` | pa | `QueryController::setCurrentFile` exists; `BasesView` never subscribes to workspace `file-open` |
| Per-`BasesView` undo/redo | M | No `QUndoStack` |
| Properties toolbar (`GX`/`KX`/`YX`/`QX`) | M | Whole `BasesView.js:659-2470` family absent |
| Formula editor | M | No `FormulaEditor`; hand-edit `.base` and reload |
| Sort+group menu | M | Header click only |
| Views menu (rename/duplicate/delete) | M | Switcher only |
| Results menu (limit input, copy, export) | M | |
| Save with debounce | pa | `requestSave()` inherited from `TextFileView`; 2s contract not Bases-enforced |

**Net runtime assessment.** Open a `.base`, see the rows, edit a cell, sort by a column, switch a view: works. Polished toolbar UI for managing the query: missing. Most "rich" interactions (drag, hover preview, undo, export) are not yet wired. The `.base` round-trip preserves global structure but is lossy for some shapes (see §"On-disk `.base` format compatibility" below).

## Value hierarchy coverage matrix (19 types)

`P` = present; `pa` = partial; `M` = missing.

| Obsidian type | Corbomite class | Status | Notes |
|---|---|---|---|
| `Value` (abstract) | `Value` (`Value.h:21`) | P | `equals`/`looseEquals`/`objectAccess`/`keys`/`isTruthy`/`type`/`toString` virtuals match. `staticEquals`/`staticLooseEquals` mirror Obsidian's null-safe statics. Symmetric loose-equals invariant honoured at `Value.cpp:32` |
| `NullValue` | `NullValue` (`Values.h:27`) | P | Singleton via `NullValue::instance()` (`NullValue.cpp:10-14`). Private ctor enforces; no friend-leak |
| `NotNullValue` | — | M | Marker base class is missing; the hierarchy collapses `NotNullValue` and `PrimitiveValue` into Bool/Number/String directly extending `Value`. Functionally OK, but the `instanceof NotNullValue` check pattern is unrealizable. Low impact: this is only used by Obsidian internally as a discriminator, never publicly. |
| `PrimitiveValue` | — | M | Same as above — `BooleanValue`/`NumberValue`/`StringValue` extend `Value` directly (`Values.h:42, 63, 81`). Affects nothing user-visible |
| `BooleanValue` | `BooleanValue` (`Values.h:42`) | P | `equals`/`looseEquals` correct; loose-equals coerces to Number per audit |
| `NumberValue` | `NumberValue` (`Values.h:63`) | P | `isTruthy` checks `!= 0 && !NaN` per spec; `toString` returns `∞`/`NaN`/`-∞`(?) — note: only `∞` is checked, **negative infinity prints as `∞` too** (`NumberValue.cpp:18` doesn't distinguish sign). Minor cosmetic bug |
| `StringValue` | `StringValue` (`Values.h:81`) | P | `length` accessor present at `StringValue.cpp:22-24`. `equals` is type-strict — Tag and String with same text are NOT equal (`StringValue.cpp:13-17`); matches audit. |
| `ListValue` | `ListValue` (`Values.h:104`) | P | All aggregates `min`/`max`/`sum`/`mean`/`median`/`stddev`/`earliest`/`latest` (`ListValue.cpp:155-221` + `DateValue.cpp:126-148`). `sort()`, `flatten()` (one-level), `unique()` via `staticLooseEquals`. **Bug:** `sort()` null-comparator (`ListValue.cpp:108-109`) puts nulls **first** not last (returns `false` for null `a`, so `a < anything` is false but `b < null-a` is also false). Inconsistent with `BasesQueryResult::compareValues` which correctly nulls-last. |
| `ObjectValue` | `ObjectValue` (`Values.h:276`) | P | `fromFrontMatter` implements lazy coercion Link → URL → Date → fall-through (`ObjectValue.cpp:18-32`); `tags` → `ListValue<TagValue>`. `getInsensitive` after exact-match. Order preserved via parallel `m_order`. |
| `RegExpValue` | `RegExpValue` (`Values.h:381`) | P | `parseFromString` recognises `/body/flags`. Flags `i`/`m`/`s` mapped; `g`/`u`/`y` parsed-but-ignored (Qt differences). |
| `DateValue` | `DateValue` (`Values.h:148`) | P | `parseFromString` accepts both addendum §6.1 regex shapes. `objectAccess` exposes all 8 fields incl. `timestamp`. `equals` is `m_hasTime`-aware. |
| `RelativeDateValue` | `RelativeDateValue` (`Values.h:174`) | pa | `toString()` hand-rolled humaniser (`DateValue.cpp:98-122`) — 30-day months, 365-day years, `secsTo`-based, no leap or DST. Acceptable per addendum §15.3 deferred polish. |
| `DurationValue` | `DurationValue` (`Values.h:421`) | P | 7-component struct. `parseFromString` covers ISO-8601 + shorthand incl. `ms`/`millisecond`/`milliseconds`; case-sensitive `M`/`m` split preserved. `addToDate` calendar-aware. `totalMilliseconds` explicitly approximate (`365*86400000 + 6h` year, `30*86400000` month). |
| `IconValue` | `IconValue` (`Values.h:233`) | pa | Class works; no icon rendering — delegate paints text. |
| `FileValue` | `FileValue` (`Values.h:322`) | P | All 14 `file.*` accessors. Per-instance aggregate caches. Backlinks does O(vault) reverse scan with per-source short-circuit (`FileValue.cpp:135-157`). `enable_shared_from_this` for the `file.file` self-reference. |
| `UrlValue` | `UrlValue` (`Values.h:220`) | P | `display` preserved. |
| `LinkValue` | `LinkValue` (`Values.h:195`) | pa | `parseFromString` handles `[[x]]`/`[[x\|display]]`. **Missing:** TFile resolution. `looseEquals` only compares string forms; `linksTo` builtin same. No `Vault` plumbed into Value layer. |
| `ImageValue` | `ImageValue` (`Values.h:240`) | pa | Class + `image()` global; no `<img>` rendering, no `vault.getResourcePath` integration; cache-busting `?<mtime>` invariant (audit §12) unrealised. |
| `HTMLValue` | `HTMLValue` (`Values.h:247`) | pa | Class + `html()` global; **no sanitization**. Currently safe-by-omission since cell delegate doesn't render HTML — would become **stored XSS** path through frontmatter if rendering added without sanitisation. |
| `MarkdownValue` | `MarkdownValue` (`Values.h:254`) | pa | Class with `type = "Markdown"`; **no constructor builtin** — `markdown(...)` global is missing from `Builtins.cpp`. Effectively unreachable. |
| `FormulaErrorValue` | `FormulaErrorValue` (`Values.h:261`) | P | `type = "Error"`, `isTruthy = false`. Cell delegate paints with warning tint (`BasesCellDelegate.cpp:111-124`). |
| `TagValue` | `TagValue` (`Values.h:183`) | P | `tagMatches` is bidirectional (`StringSubclasses.cpp:11-19`) — Audit §1 specifies one direction only; bidirectional is friendly but undocumented divergence. |
| `DW` (`TagListValue`) | — | M (defensible) | Obsidian uses a `ListValue` subclass for `file.tags` whose `.includes()` uses `tagMatches`. Corbomite uses plain `ListValue<TagValue>`; `.includes` calls `staticLooseEquals` → `TagValue::equals` (string-strict). **Regression:** `file.tags.contains("#parent")` won't match `#parent/child`. Workaround: `file.hasTag("#parent")` (`FileValue.cpp:234-244`). |
| `ThisFileValue` (`sY`) | `ThisFileValue` (`Values.h:367`) | P | Forwarder closure to `BasesEntry::getByIdentifier` with `this` recursion guard. |
| `LambdaObjectValue` | `LambdaObjectValue` (`Values.h:308`) | P | Used for the `formula` identifier wrapper. Corbomite-side adapter. |

**Summary:** all 19 spec-named types are present as classes; one (`MarkdownValue`) is stranded with no constructor builtin; three (`HTMLValue`, `ImageValue`, `IconValue`) lack the rendering paths their `.renderTo` would normally provide; one (`DW`/`TagListValue`) has been replaced with a plain ListValue creating a real `file.tags.contains` regression. The `NotNullValue`/`PrimitiveValue` markers are absent — defensible since they're internal to Obsidian.

## Formula DSL coverage matrix

### Operators

| Op | Status | Where |
|---|---|---|
| `\|\|` (logical OR, short-circuit, returns Bool not last operand) | P | `Evaluator.cpp:151-155` — short-circuits, returns `BooleanValue` per addendum §3 note |
| `&&` (logical AND, short-circuit) | P | `Evaluator.cpp:157-159` |
| `==` / `!=` (loose equality via `staticLooseEquals`) | P | `Evaluator.cpp:128-129, 164-168` |
| `<` / `>` / `<=` / `>=` (relational) | P | `Evaluator.cpp:172-260` — DateString and DurationString coercion paths implemented; null propagation present at `:174-175` |
| `+` (arithmetic / string-concat / list-concat / Date+Duration / Duration+Duration) | P | `Evaluator.cpp:264-336` covers all rows of addendum §4.3 dispatch table |
| `-` (arithmetic / Date-Date→Duration / Date-Duration / Duration-Duration) | P | Same dispatch |
| `*` (number / `Du * N`, **but symmetric `N * Du` not enforced as throw**) | pa | `Evaluator.cpp:311-315` only matches `Du * N`; `N * Du` falls through to "invalid operator" error which **matches** the addendum §4.3 throw behaviour. OK |
| `/` (number / `Du / N`) | P | Same |
| `%` (modulo) | P | `Evaluator.cpp:286` uses `std::fmod` |
| Unary `!` (with null-propagation) | P | `Evaluator.cpp:103-107` — `!Null` returns `Null` per addendum §4.4 |
| Unary `-` (negate, with constant-fold) | P | `Evaluator.cpp:109-112` + `Parser.cpp:57-66` constant-folds `-<Number-literal>` per addendum §4.4 |
| `()` postfix call | P | `Parser.cpp:160-174` |
| `[]` postfix index | P | `Parser.cpp:175-182`, dispatch in `Evaluator.cpp:81-97` |
| `.` postfix member | P | `Parser.cpp:183-190`, dispatch in `Evaluator.cpp:73-79` |
| `[...]` array literal | P | `Parser.cpp:123-139` + `Evaluator.cpp:65-71` |
| `()` grouping | P | `Parser.cpp:100-108` |

**Precedence table verification:** `Parser::infixBp` (`Parser.cpp:32-50`) and `prefixBp` (`Parser.cpp:52-55`) match the addendum §3 precedence ladder exactly: `OrOr {1,2}`, `AndAnd {3,4}`, `EqEq/BangEq {5,6}`, relational `{7,8}`, additive `{9,10}`, multiplicative `{11,12}`, unary prefix `13`. Higher = tighter; left-associative because right-bp = left-bp + 1. Clean.

### Functions

#### Globals (addendum §8.2)

| Function | Status | Notes |
|---|---|---|
| `now()` | P | `Builtins.cpp:43-47` |
| `today()` | P | `Builtins.cpp:49-54` |
| `date(str)` | P | `Builtins.cpp:56-63` returns FormulaError on parse failure (matches "error Value on parse failure" semantics) |
| `if(cond, then, else?)` | P | **Hard-cased** in `Evaluator.cpp:371-380` per addendum §5.2 — bypasses registry, so the lambda body sees the surrounding scope. Two- or three-arg validated |
| `random()` | P | `Builtins.cpp:65-69` |
| `min(...)` / `max(...)` | P | Variadic; numeric-only; non-number arg returns FormulaError |
| `list(elem)` | P | List-returns-self / wrap-in-1-elem semantics (`Builtins.cpp:99-107`) |
| `link(path, display?)` | P | `Builtins.cpp:109-117` |
| `number(x)` | P | Date→ms, Bool→0/1, String→parseFloat, Number→identity (`Builtins.cpp:119-139`) |
| `duration(str)` | P | `Builtins.cpp:141-149` |
| `image(path)` | P (constructor only) | Returns a stranded `ImageValue` since no renderer |
| `icon(name)` | P (constructor only) | Same |
| `file(path)` | M | `Builtins.cpp:166-174` returns `NullValue::instance()` unconditionally with a comment "Without a Vault binding at the evaluator layer". A working `file()` requires plumbing a `Vault*` reference into the `EvalContext` interface or the FunctionRegistry — neither exists. **Real gap.** |
| `html(str)` | P | `Builtins.cpp:176-181` |
| `escapeHTML(str)` | P | `Builtins.cpp:183-194` |

#### Per-type Value methods (addendum §8.3 — registered for `Value` itself)

| Method | Status |
|---|---|
| `x.toString()` | P |
| `x.isTruthy()` | P |
| `x.isType(name)` | P |
| `x.isEmpty()` | P (delegates to per-type override) |

#### String methods (addendum §8.4)

`startsWith`, `endsWith`, `trim`, `title`, `isEmpty`, `replace`, `reverse`, `lower`, `split`, `contains`, `containsAny`, `containsAll`, `slice`, `repeat`: **all present** (`Builtins.cpp:230-342`). `replace` accepts both String and `RegExpValue` patterns (`Builtins.cpp:263-268`). `split` similarly polymorphic. **Verified consistent with the `s.length` field-not-method invariant** — accessing `length` is via `objectAccess`, not as a method. **Confirmed no `s.upper()`** — matches addendum §8.4 footnote.

#### Number methods (addendum §8.5)

`round` (with optional digits arg), `ceil`, `floor`, `abs`, `toFixed`, `isEmpty`: present (`Builtins.cpp:346-383`). `toFixed` returns `StringValue` per spec.

#### Date methods (addendum §8.6)

`format`, `date`, `time`, `relative`, `isEmpty`: present (`Builtins.cpp:387-425`). `format` plumbs through `Corbomite::MomentFormatter::format` — assumes that helper exists in `corbomite/core/`. Audit at MomentFormatter to confirm full Moment.js token coverage out of scope here.

#### List methods (addendum §8.7)

`earliest`, `latest`, `median`, `mean`, `max`, `min`, `sum`, `stddev`, `join`, `reverse`, `flat`, `unique`, `contains`, `containsAny`, `containsAll`, `slice`, `sort`, `isEmpty`: present (`Builtins.cpp:429-514`). **Lambdas (`map`/`filter`/`reduce`):** hard-cased in `Evaluator.cpp:381-433` per addendum §5.2 — `index`/`value`/`acc` shadowing-context bindings are correctly built and pushed into a `ShadowingContext` adapter (`Evaluator.cpp:347-364`) before evaluating the lambda body.

#### Object methods (addendum §8.8)

`isEmpty`, `keys`, `values`: present (`Builtins.cpp:518-542`). `map`/`filter` hard-cased in `Evaluator.cpp:434-462`; `map` returns a `ListValue`, `filter` returns an `ObjectValue` per addendum §8.8.

#### RegExp methods (addendum §8.9)

`matches`: present (`Builtins.cpp:546-554`). Matches Pass 2 spec.

#### Link methods (addendum §8.10)

| Method | Status |
|---|---|
| `l.asFile()` | M | Returns `NullValue::instance()` unconditionally (`Builtins.cpp:564-570`) — same blocker as `file()` global. Requires Vault binding |
| `l.linksTo(other)` | pa | String-equality only (`Builtins.cpp:571-578`); doesn't resolve wikilinks to TFiles |
| `l.matches(pattern)` | M | Not registered; addendum §8.10 marked it as unconfirmed-surface |

#### File methods (addendum §8.11)

`asLink`, `hasLink`, `inFolder`, `hasTag` (variadic, hierarchical via `tagMatches`), `hasProperty`: all present (`Builtins.cpp:583-622`).

### Summary formulas (addendum §9)

The 15 default summary formulas are **not** all expressed as DSL strings. Instead, `BasesQueryResult::summaryValue` (`BasesQueryResult.cpp:155-180`) hard-codes a subset by name: `sum`/`min`/`max`/`mean`/`average`/`median`/`stddev`/`unique`/`count`. **Missing:** `Range` (number), `Range` (date), `Earliest`, `Latest`, `Checked` (boolean), `Unchecked`, `Empty`, `Filled`. All trivially expressible by extending the dispatch table. No UI yet selects between them anyway, so this is a deferred follow-up. **Custom summaries via `summaries:` map** — parsed by `BasesQuery::fromString` (`BasesQuery.cpp:185-188`) but never evaluated; `summaryValue`'s `summaryFn` arg is treated as a name, not a formula reference.

### What's missing entirely from the DSL implementation

- **Plugin-side `registerGlobalFunc`/`registerInstanceFunc`** (addendum §13) — the `FunctionRegistry::addGlobal`/`addForType` methods are public but no plugin manifest path uses them; Bases is a static lib not loaded as a plugin. Decision tracks the project's plugin-API-stability principle.
- **`l.matches(...)`** on Link, **`number(NaN check)`**, and various polish items.
- **Source-position-bearing AST.** Errors come back as a `QString message`; there's no `{line, col, source-span}` info. The formula editor (which doesn't exist) will need this.

Overall: **DSL coverage is genuinely strong.** Operators are 100%, hard-cased lambdas + `if` are correct, and the function catalog is ~85% of the addendum's surface. The two real gaps are the Vault-bound functions (`file()`, `LinkValue::asFile`/`looseEquals` resolution) and the missing markdown-construction builtin.

## On-disk `.base` format compatibility

Corbomite's parser/serializer round-trips through `Markoff::YamlValue` for parsing (`BasesQuery.cpp:157`) and a hand-rolled emitter for writing (`BasesQuery.cpp:78-141`). This is the single biggest compatibility concern in this audit.

### What round-trips correctly

- **Empty `.base` → default 1-view "Table"** (`BasesQuery.cpp:147-154`) per audit invariant `[CRIT]`.
- **Top-level keys:** `views`, `filters`, `formulas`, `summaries`, `properties`, `display` (legacy migration), `newItemFolder`, `newItemTemplate` (`BasesQuery.cpp:172-219`).
- **Legacy `display:` migration into `properties[].displayName`** (`BasesQuery.cpp:222-232`) per audit §3 invariant.
- **`unrecognizedData`** at `BasesQuery` level (`BasesQuery.cpp:208-218`, written back at `BasesQuery.cpp:283-285`) — preserved for forward-compat.
- **`unrecognizedData`** at `BasesViewConfig` level (`BasesViewConfig.cpp:88, 134-136`) — preserved.
- **PropertyId casing.** `parsePropertyId` (`PropertyId.cpp:11-26`) splits on first `.` and tolerates unprefixed → Note. Round-trips with `buildPropertyId` (`PropertyId.cpp:28-36`).
- **View-config required defaults** — type defaults to `"table"`, name defaults to `"All"` if absent (`BasesViewConfig.cpp:93-94`).

### What is **lossy** or **divergent**

- **`unrecognizedData` is scalar-only.** `BasesQuery::fromString` lines 209-218 only stores Null/Bool/Int/Double/String into `unrecognizedData`; **maps and sequences are silently dropped** (the `default: break;` comment "complex shapes skipped for MVP unrecognizedData"). This **violates audit invariant `[CRIT] Unknown keys round-trip verbatim`** for any forward-compat YAML that nests data — and that is precisely the use case `unrecognizedData` exists to serve. A future Obsidian release adding a `theme:` block would lose the block on first save.
- **`PropertyConfig.unrecognizedData` is declared but unused.** `PropertyConfig` (`BasesViewConfig.h:34-38`) has the field, but the parser at `BasesQuery.cpp:189-196` reads only `displayName` — every other key inside a `properties.<id>: { … }` map is dropped on load.
- **Key ordering is alphabetised.** The hand-rolled `emitMap` at `BasesQuery.cpp:108-127` iterates `QVariantMap::constBegin()` which is **alphabetised by key**, not insertion-ordered. The audit calls out "YAML key-ordering should be preserved by the serialiser." Result: an Obsidian-authored `.base` opened, edited (or even just saved unchanged), and written back will have all keys reshuffled. This is highly visible diff churn for anyone version-controlling their vault — the most likely user-perceivable round-trip regression.
- **Limit `0` is dropped on write.** `BasesViewConfig::toMap` line 126 only emits `limit` when `limit > 0`. The audit invariant says `limit: 0 == unlimited` — that's true for **read** semantics, but a user who explicitly types `limit: 0` for clarity will lose the key on save.
- **String-quoting heuristic.** `yamlQuoteIfNeeded` (`BasesQuery.cpp:14-50`) avoids quoting alphanumeric strings but quotes anything containing `:`/`#`/`-`/`[`/`{`/`"` etc. Strings starting with `-` are quoted (good — would otherwise look like a list item). Strings looking like booleans/null are quoted. Strings looking like numbers are quoted ("allDigitish" check). Reasonable heuristic.
- **YAML emitter is hand-rolled**, not a real YAML library. There's no test for round-tripping known Obsidian-authored `.base` files (`tst_yaml_schema.cpp` exercises Corbomite-authored shapes only — see "Notable concerns" below).
- **The `properties:` block round-trip drops** `unrecognizedData` per-property entirely (`BasesQuery.cpp:255-264` only emits `displayName`).

### Filter grammar

- **Atomic rules** are stored as Formula source strings (`FilterTree.h:34-46`); `FilterRule::test` evaluates the formula and returns isTruthy. Matches audit §3 filter-grammar.
- **`and`/`or`/`not`** parsed via `parseFilter` (`FilterTree.cpp:57-80`), matching only one of the three keys per node (Obsidian forbids multiple-conjunction-in-one-map similarly).
- **`not` semantics** — `FilterConjunction::test` for `Not` returns `!any-child-passes`, i.e. "no child passes". `For/of` loop returns `false` if any child passes (`FilterTree.cpp:28-31`). Matches the audit §1 `mY` definition.

### Validation

- Parse errors are stored on a `QString *parseError` out-param (`BasesQuery.cpp:143`), surfaced via `BasesView`'s `m_errorBanner` (`BasesView.cpp:94-96`). The audit's `BasesView.setViewData catches and stores the error; the view shows an error banner` invariant is honoured.

## Implemented (parity-equivalent)

- Pratt parser covers the whole DSL grammar; lexer disambiguates regex from division (`Lexer.cpp:19-32`); single-quoted-string normalisation per addendum §7 (`Lexer.cpp:35-56`).
- Evaluator: null-propagation, Date/Duration coercion, hard-cased `if`/`map`/`filter`/`reduce`/`object.map`/`object.filter` with iteration-bound shadowing (`Evaluator.cpp:347-462`).
- `FunctionRegistry` class-chain dispatch (`FunctionRegistry.cpp:43-83`); exhaustive across all 19 types.
- All 15 global builtins except `file()`; all per-type method tables (Value/String/Number/Date/List/Object/RegExp/Link/File).
- `ObjectValue::fromFrontMatter` lazy coercion: Wikilink → URL → Date → fallback (`ObjectValue.cpp:16-32`); `tags` special-cased (`ObjectValue.cpp:60-68`).
- BasesEntry identifier-dispatch with case-insensitive frontmatter fallback (`BasesEntry.cpp:55-74`).
- Per-entry formula memoisation + cycle detection (`BasesEntry.cpp:97-114`) — port of `aY`/`rY` pattern.
- `BasesQueryResult` sort+limit, group-by with nulls-last, properties union (`BasesQueryResult.cpp:64-153`).
- `BasesView`: view selector, search, error banner, sortable/movable headers (`BasesView.cpp:21-66`).
- `BasesTableModel` with `ValueTypeRole`/`ValuePtrRole` for delegate dispatch (`BasesTableModel.h:28-30`).
- `QueryController` 50ms debounced recompute on `cacheChanged`/`cacheDeleted` (`QueryController.cpp:23-65`) — closes Pass 2 Open Q3.
- `BasesView` registered for `.base` extension (`MainWindow.cpp:1533-1535`); services injected at view-creation (`MainWindow.cpp:1087-1090`).
- 13 test executables, ~2000 lines, ~411 assertions.

## Partial / divergent

- **`PropertyConfig.unrecognizedData` declared but ignored** (see §"On-disk" above).
- **Top-level `unrecognizedData` truncates non-scalar shapes** (see §"On-disk").
- **YAML emitter alphabetises keys** (see §"On-disk").
- **`limit: 0` is dropped on save** (see §"On-disk").
- **Sort cycling is single-key.** `BasesView::onHeaderClicked` (`BasesView.cpp:147-164`) replaces the entire sort key list rather than appending. Multi-key sort is supported by the *data model* (`BasesQueryResult::applySort` walks the full `sort` vector, `BasesQueryResult.cpp:79-86`) — the UI just doesn't expose it.
- **Group-by has no UI rendering** even though the data model computes groups. Rows render flat under a single header.
- **Search scans only `m_cfg->order`** (`QueryController.cpp:102`). If a column isn't in the visible-order list (e.g. seen-but-not-ordered note.* keys exposed by `BasesQueryResult::properties()` union), search misses it.
- **Sidebar coupling is half-done.** `QueryController::setCurrentFile` exists (`QueryController.cpp:51-55`) but `BasesView` doesn't wire workspace `file-open` events to call it. The `this` identifier therefore always resolves to the `.base` file itself (the row's own `localFile` defaults to `f` in `QueryController::recomputeNow` line 89).
- **`ListValue::sort()` mis-positions nulls** (see Value matrix — comparator returns `false` for null `a`, putting nulls first not last).
- **`RelativeDateValue::toString` calendar approximations** (30-day months, 365-day years).
- **`TagValue::tagMatches` is bidirectional**; Obsidian's audit §1 documents only one direction. May or may not be a divergence — needs a vault test.
- **`DW`/`TagListValue` collapsed to `ListValue<TagValue>`** — `file.tags.contains("#parent")` won't match `#parent/child`. Workaround: use `file.hasTag("#parent")`.
- **`HTMLValue.renderTo` is missing** — currently safe-by-omission since the cell delegate doesn't render HTML. If/when it's added, sanitization must come along too.
- **`MarkdownValue` is unreachable** — no constructor builtin.
- **`LinkValue.looseEquals` and `linksTo` use string equality** instead of TFile resolution.
- **`file()` global returns `null`** unconditionally — addendum §8.2 contract not met.
- **`asFile()` returns `null`** unconditionally — same blocker.
- **`Formula` copy-constructor re-parses** on copy (`Formula.cpp:21-32, 34-48`). Functional but wasteful when formulas are stored in maps and copied — `m_ast` is `std::shared_ptr<Expr>` so it could safely be aliased. The constructor comment "Re-parse to get a fresh AST owned by this copy. Cheap." is true for short formulas but unnecessary; the copy path violates the documented purpose of `m_ast` being a shared_ptr.

## Missing (prioritized)

### Structural / behavioural — affects correctness

1. **`unrecognizedData` truncates non-scalar shapes** — invariant violation, will lose user data on first save round-trip with any future Obsidian feature that nests YAML.
2. **YAML emitter alphabetises keys** — every save reshuffles the file; massive diff churn for VCS users.
3. **`PropertyConfig.unrecognizedData` is unused** — same forward-compat violation, scoped to per-property settings.
4. **`ListValue::sort()` puts nulls first** — silent comparator bug.
5. **`file()` and `LinkValue::asFile()`** — both stubs. Without them, formulas like `file("Notes/X.md").mtime` always return `null`.
6. **`file.tags.contains("...")` does string equality**, breaking the hierarchical-tag invariant. Either reintroduce `TagListValue` or special-case `ListValue::includes` for TagValue elements.
7. **No `MetadataCache::cacheChanged`-triggered re-evaluation of single rows** — every cache change re-runs the full query (`QueryController::recomputeNow` rescans `Vault::getMarkdownFiles()`). For 5k+ note vaults this is slow. Audit §11 flagged this risk; mitigation is "debounce 50ms" which only helps with bursts.
8. **Sidebar `this` binding never updates** — `QueryController::setCurrentFile` is wired but never called by `BasesView`'s workspace-event subscription (it has none).

### Cosmetic / UX

9. **No formula editor** — user must hand-edit `.base` files outside the app and reload.
10. **No properties / sort+group / views / results menus** — the entire Bases toolbar suite from `BasesView.js:659-2470` is absent.
11. **No group-rendering** in the table (groups are computed but invisible).
12. **No multi-key sort cycle** in the header click handler.
13. **No export** (CSV/TSV/Markdown/`obsidian/table` MIME).
14. **No "+ New" button** (`newItemFolder` and `newItemTemplate` round-trip but unused).
15. **No drag, no context menu, no hover-link, no per-view undo/redo** on cells.
16. **No view-rename auto-rewrite** of `[[basefile#viewname]]` references.
17. **No rich rendering** for `Image` / `HTML` / `Markdown` / `Icon` cells.
18. **Plugin-API surfaces** (`registerGlobalFunc`, `registerInstanceFunc`, `BasesPluginInstance.registerView`) — entire absence per Cluster K plan-of-record.

### Tests not present

- No round-trip test against an Obsidian-authored `.base` file. `tst_yaml_schema.cpp` builds queries programmatically and exercises only Corbomite's emitter shape.
- No fuzz / property test on the parser.
- No test exercising `QueryController` against a live `MetadataCache`.

## Notable translation successes

- **Pratt parser at ~200 lines** diffs cleanly against the addendum precedence table; replaces a 1.4KB serialised Lezer state machine with reviewable code.
- **Hard-cased dispatch (`if`, `map`/`filter`/`reduce`) in the evaluator** (`Evaluator.cpp:367-462`), mirroring `jK.getValue` (addendum §5.2). Lambda-shadowing semantics correct.
- **Symmetric `staticLooseEquals(a, b)`** invariant honoured at `Value.cpp:32` (`a->loose(b) || b->loose(a)`).
- **`FunctionRegistry::classChain`** (`FunctionRegistry.cpp:43-68`) — single-source most-derived-to-base table for instance-method resolution. Adding a new Value subclass = one edit.
- **`enable_shared_from_this` on `FileValue`** for the `file.file` self-reference accessor (`FileValue.cpp:69-71`).
- **Single `BasesCellDelegate`** with role-based dispatch vs Obsidian's three-class `d$/f$/m$` factory; idiomatic Qt.
- **`Lexer::isRegexAllowedAfter`** correctly distinguishes `/regex/` from division based on previous-token kind (`Lexer.cpp:19-32`).
- **All audit `[CRIT]` read-side invariants honoured** (empty file, top-level YAML object, `views[0]` default, PropertyId grammar, `limit: 0 == unlimited`, NullValue singleton, frontmatter live alias).
- **`MetadataCache::cacheChanged` integration** (`QueryController.cpp:27-32`) closes Pass 2 Open Q3 without needing the missing Obsidian `QueryController` source.

## Notable concerns / suspected bugs

- **YAML key-order alphabetisation on write** (`BasesQuery.cpp:111-127`, `QVariantMap::constBegin()`). Most user-visible round-trip regression; every save reshuffles the file. Fix: order-preserving container or pre-defined emit order list.
- **`unrecognizedData` truncates non-scalars** at `BasesQuery.cpp:209-218` (top-level). `BasesViewConfig.cpp:88` correctly uses `yamlToVariant`; the inconsistency is itself a smell.
- **`ListValue::sort()` null comparator** (`ListValue.cpp:108-109`) appears to put nulls first not last; row-sort in `BasesQueryResult::compareValues` (`BasesQueryResult.cpp:18-20`) correctly handles null-last. Inconsistent and likely a bug.
- **`Formula` copy constructor re-parses** unnecessarily (`Formula.cpp:21-32`); `m_ast` is `shared_ptr` precisely to avoid this.
- **`TagValue::tagMatches` is bidirectional** vs audit §1's one-direction spec — undocumented divergence.
- **`ListValue<TagValue>::contains` uses string-strict equality** (no `DW`/`TagListValue` analog). `file.tags.contains("#parent")` won't match `#parent/child` — real semantic regression for the documented use case.
- **`addToDate` order of operations** for negative durations (`DurationValue.cpp:98-112`): year then month then day then ms-delta. Edge cases at month boundaries (e.g. March 31 - "1 month 1 day") can drift; acceptable, consistent with QDate.
- **`BasesView` lacks workspace `file-open` / `layout-change` subscriptions** (audit §5) — `QueryController::setCurrentFile` is plumbed but never called. `this` formula-identifier always resolves to the `.base` itself, never the active sidebar file. **Functional behaviour gap.**
- **`Formula::test` per-cell parse errors surface as `FormulaErrorValue` with warning tint** but no actionable repair UI (no formula editor exists).
- **Empty `views:` array** triggers a second default-view inject at `BasesQuery.cpp:234-239`; result name hard-coded `"All"` rather than localised. UI-only.
- **`QJsonDocument`-based string-escape decoding** (`Lexer.cpp:121-131`) — JSON escapes are a strict subset of JS escapes (`\v` valid in JS, not JSON). Per addendum §7 `JSON.parse` is exactly what Obsidian uses, so behaviour matches.

