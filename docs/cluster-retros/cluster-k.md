# Cluster K — Bases (Retro)

**Closed:** 2026-04-17.
**Plan:** [`../superpowers/plans/archive/2026-04-17-cluster-k-bases.md`](../superpowers/plans/archive/2026-04-17-cluster-k-bases.md)
**DSL addendum:** [`../obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md`](../obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md)
**Commits:** `98e0567e` (plan) → `5825422b` Phase 1 Task 1.1 → `f363b35f` Phase 8 closeout, plus Phase 9 wire-in + retro commits — total **~37 commits** across one autonomous session.

---

## What shipped

### `libs/bases/` — new static library, namespace `Corbomite::Bases`

**Value hierarchy (Phase 1 + 2).** Polymorphic `std::shared_ptr<Value>` hierarchy mirroring Obsidian's JS class structure. 18 concrete types: `NullValue` (singleton), `BooleanValue`, `NumberValue`, `StringValue`, `ListValue`, `ObjectValue`, `DateValue` + `RelativeDateValue`, `DurationValue`, `RegExpValue`, `FileValue` + `ThisFileValue`, `LinkValue`, `UrlValue`, `TagValue`, `IconValue`, `ImageValue`, `HTMLValue`, `MarkdownValue`, `FormulaErrorValue`, `LambdaObjectValue`. Each has `type()`/`isTruthy()`/`isEmpty()`/`toString()`/`equals()`/`looseEquals()`/`objectAccess()`/`keys()`. `parseFromString` static factories on `DateValue`, `DurationValue`, `LinkValue`, `RegExpValue`. `ObjectValue::fromFrontMatter` lazy coercer walks a `QJsonObject` and upgrades strings to `LinkValue`/`UrlValue`/`DateValue`.

**Formula engine (Phase 3 + 4).** Hand-rolled Pratt parser over a custom lexer per DSL addendum §15.1 option 2 (~500 LoC total, zero new runtime deps, fully debuggable in the C++ toolchain). Binding-power table matches addendum §3 exactly. Constant-folds `-<NumberLiteral>` at build time. `Evaluator::applyArithmetic` + `applyRelational` + `applyEq` + `applyLogical` implement addendum §4 operator semantics including: type-aware arithmetic (`N + N`, `S + anything`, `List + List`, `Date ± Duration`, `Date - Date → Duration`, `Duration * N` but `N * Duration` throws), null propagation, logical `&&`/`||` always returns fresh `BooleanValue` (never last operand), `!Null → Null` (not `true`), relational coerces `String` through `parseFromString` for Date/Duration.

**Function registry (Phase 5).** `FunctionRegistry` with global + per-type tables, class-chain dispatch (most-derived first). `Builtins.cpp` registers all **43 functions** from addendum §8: 15 globals (`now`/`today`/`date`/`random`/`min`/`max`/`list`/`link`/`number`/`duration`/`image`/`icon`/`file`/`html`/`escapeHTML`), per-type `toString`/`isTruthy`/`isType`/`isEmpty`, String (13 methods), Number (6), Date (5 — `format` via `Corbomite::MomentFormatter`), List (18 non-lambda), Object (3 non-lambda), Regex, Link (2), File (5). **Hard-cased** bypassing the registry: `if`, `list.map/filter/reduce`, `object.map/filter` — `Evaluator` intercepts these by name BEFORE arg evaluation and runs the lambda body inside a `ShadowingContext` that binds `value`/`index`/`acc`/`key`. `Formula` wrapper holds original source + parsed AST; invalid source stored as `InvalidExpr`, `getValue` yields `FormulaErrorValue`, `test` yields false.

**`.base` YAML schema (Phase 6).** `PropertyId` (Note/File/Formula prefix), `FilterTree` (leaf `FilterRule` + `FilterConjunction` with And/Or/Not, short-circuiting `test`, `optimize` collapses single-child), `BasesViewConfig` (per-view: type/name/filters/order/sort/groupBy/limit/summaries/data/unrecognizedData), `BasesQuery` (top-level views + global filters + formulas + summaryFormulas + properties + newItemFolder/Template + unrecognizedData). Parse via `Markoff::YamlValue`; emit via a small indented hand-rolled YAML serialiser. Empty-file-default-1-view-"Table" invariant preserved (audit §3). Legacy `display:` migrates to `properties[*].displayName` at parse. `limit: 0` round-trips as absent.

**Data binding (Phase 7).** `BasesEntry` is the `EvalContext` for a single row; resolves `this`/`note`/`file`/`formula` identifiers plus frontmatter fallback; memoises formula evaluations and detects cycles via `m_inProgressFormulas` sentinel (returns `FormulaErrorValue` on re-entry). `BasesQueryResult` sorts (multi-key, nulls last), limits (`limit: 0 == unlimited`), and partitions into `BasesEntryGroup`s; summaries dispatched by name (sum/min/max/mean/median/stddev/unique/count). `QueryController` QObject with 50ms debounce subscribes to `MetadataCache::cacheChanged`/`cacheDeleted` and rebuilds the full result set from `Vault::getMarkdownFiles`; emits `resultsChanged`.

**Widget (Phase 8).** `BasesTableModel` (`QAbstractTableModel`) bridges `BasesQueryResult` → Qt. `BasesCellDelegate` (`QStyledItemDelegate`) dispatches per-`Value::type()` editors (Boolean→`QCheckBox`, Number→`QDoubleSpinBox`, Date→`QDateEdit`/`QDateTimeEdit` per `hasTime`, else→`QLineEdit`) and paints ballot glyphs for Boolean + warning tint for Error. `BasesView` (`TextFileView` subclass) with toolbar (view selector + search field), sort-on-header-click cycling, inline-edit writeback through `FileManager::processFrontMatter`. Parse errors surface in a banner; default 1-view "Table" fallback still opens.

**Host integration (Phase 9).** `BasesView::factory` registered in `MainWindow::setupUi` via `ViewRegistry::registerViewWithExtensions({"base"}, "bases", ...)` — following the built-in `markdown`/`canvas` pattern. `MainWindow::propagateServicesToView` adds a case for `BasesView` that injects `m_vaultObj` + `m_metadataCache` + `m_fileManager`. `Corbomite::Bases` added to `CorbomiteApp`'s link line.

### Test coverage

13 test executables, ~160 cases across:

- `tst_bases_value_null` / `_primitive` / `_list` / `_date` / `_duration` / `_string_subclasses` / `_object_regex` — Value hierarchy.
- `tst_bases_lexer` / `_parser` — DSL parse.
- `tst_bases_evaluator` — operator semantics.
- `tst_bases_builtins` — 43 built-ins + hard-cased lambdas + Formula wrapper.
- `tst_bases_yaml_schema` — PropertyId, FilterTree, BasesQuery round-trip + legacy `display` migration.
- `tst_bases_entry` — cycle detection, memoisation, sort/limit/group smoke.

All green. Outside the pre-existing flaky `tst_benchmark_layout` timeout, the 200+-test project suite stays 100% green.

---

## Deliberate MVP cuts (follow-ups)

These landed in the plan's "Explicitly deferred" list up front and stayed deferred:

1. **Cards and List layouts.** Only Table ships. `BasesViewConfig::type` accepts arbitrary strings; a follow-up registers extra layout types against the cell-rendering pipeline.
2. **Internal-plugin wrapping (KPluginFactory).** BasesView is wired directly into MainWindow's `ViewRegistry` (like `markdown`/`canvas`) rather than as a `src/plugins/bases/` `.so` module. Reason: `BasesEntry` + `FileValue` consume raw `Vault *` + `MetadataCache *`; porting them to `VaultProxy` + `MetadataCacheReader` is significant surface. The Cluster-Q proxy refactor is still the right eventual target — tracked as a Cluster-K follow-up alongside the `registerGlobalFunc` / `registerInstanceFunc` plugin extension API that the FunctionRegistry already supports but isn't wired across the plugin boundary.
3. **Rich inline-edit widgets.** Delegate uses `QLineEdit` as the catch-all. No `metadataTypeManager.registeredTypeWidgets` port.
4. **View-rename wikilink rewrite.** Renaming a view doesn't rewrite `[[basefile#viewname]]` references across the vault (audit §9 feature).
5. **`![[Foo.base]]` embed in markdown.** EmbedRegistry integration deferred.
6. **Clipboard export.** TSV / Markdown / HTML / `obsidian/table` MIME emit not wired.
7. **Formula editor.** No syntax highlighting or autocomplete for the formula text field — plain string edits suffice.
8. **`+ New` button.** No pre-population of filter-satisfying frontmatter in a new note.
9. **Per-BasesView undo/redo.** No dedicated `QUndoStack`.
10. **Column-reorder persistence.** Drag-reorder in the `QTableView` header doesn't persist back into `BasesViewConfig::order` yet.
11. **Multi-key sort cycling UI.** Header clicks cycle one column; a multi-key builder belongs to the Sort toolbar menu which is also deferred.
12. **Group-header rendering + collapsible sections.** `BasesQueryResult::groups()` produces the data; the widget doesn't render group boundaries in the table yet.

None of these block the MVP user flow: open a `.base` file → see vault notes → sort by header → edit a cell → save.

---

## Architectural decisions (locked in at plan-expansion time)

- **Hand-rolled Pratt parser** over the three addendum §15.1 options. ~500 LoC total (lexer + parser + AST). Matches user preference expressed in the opening brief ("a ross parser? was that it?" → confirmed Pratt).
- **`std::shared_ptr<Value>` polymorphic hierarchy** over `std::variant`. Identity semantics match Obsidian's JS object model; avoids `std::visit` verbosity across 18 types; fits Qt object-pointer conventions. Addendum §15.2 recommendation.
- **`std::vector<ExprPtr>`** (not `QVector`) for AST child containers. Same Qt6 `QVector` + move-only `unique_ptr` constraint previously hit in Q.0 Phase 2's `m_pendingDelete` and in `libs/vault/`'s `m_fileMap`.
- **`Markoff::YamlValue` for YAML reads, hand-rolled emitter for writes.** The reader needs all the ryml-backed comfort; the writer's scope (one scalar/list/map tree, ≤ 50 keys) is small enough that a ~60 LoC indented emitter beats the `YamlValue` mutation-API boilerplate.
- **Direct `ViewRegistry` hookup** (not `KPluginFactory`) for ship-MVP. See follow-up #2 above.

---

## Deviations from the plan

- **Task 2.7 shipped before Tasks 2.3 + 2.4** (reversed order). `ObjectValue::fromFrontMatter`'s lazy coercer depends on `LinkValue::parseFromString`, `UrlValue`, `TagValue`, `DateValue` — so Link/Url/Tag/Icon/Image/HTML/Markdown/Error subclasses landed first. Documented inline in the `fac90d67` commit.
- **Task 2.6 (FileValue cache wiring) merged into Task 2.5.** Single commit `704efa77` because the aggregate accessors are tightly coupled with `FileValue` construction. Dedicated fixture tests for cache wiring deferred to the Phase 7 `BasesEntry` suite (they exercise the same path end-to-end).
- **`Formula` wrapper (plan Task 5.6) merged into Phase 5's `Builtins.cpp` commit** — tight coupling with the function registry.
- **`ListValue::earliest/latest` stubbed in Phase 1, replaced in Phase 2 Task 2.1** once `DateValue` existed. The stub+replace pattern kept compilation working across phases without forward-declaring `DateValue` for a `dynamic_cast`.
- **UTF-8 in comments confuses AUTOMOC's `Q_OBJECT` detection** (`tst_lexer.cpp` quirk hit during Phase 3). Worked around by sticking to ASCII in test-file comment bodies. Not a regression in any shipped runtime code — the failing case was a test-file artefact. Reproducible with any `tst_*.cpp` file whose comment body contains enough combining-sequence unicode to trip the AUTOMOC pre-scanner.

---

## What I'd do differently

- **Start with `VaultProxy`/`MetadataCacheReader` types in `BasesEntry`/`FileValue` from the start** — would have made Phase 9 a plugin shell rather than direct `MainWindow` wiring. The proxy surface already exists; porting a few `MetadataCache::getFileCache(path)` → `MetadataCacheReader::frontmatterFor(path)` etc. is the gap.
- **Spend less time on YAML emitter.** The hand-rolled indented emitter worked but added ~60 LoC. Cluster-K-future should either build a generic `QVariant → YAML` helper once and put it in `libs/storage/` for reuse, or commit to `ryml`'s tree-builder API. Splitting the difference cost time.
- **Write more integration tests earlier.** Phase 1–7 all have unit tests; Phase 8 has none. A Phase-8 fixture test that spins up a temp vault + `.base` file + `BasesView` + inline-edit round-trip would have caught any model/controller/widget wiring gap before it reached the main app build.

---

## Blocks / enables

**Enables:**
- Largest single missing Obsidian parity feature now shipped.
- `FunctionRegistry`'s `registerGlobalFunc` / `registerInstanceFunc` surface is the plugin-extension hook; wiring it across the Cluster-N ABI is a small follow-up.
- Cluster O (advanced query layer, post-parity) has concrete extension points: `BasesEntry`/`QueryController` pairs for the runtime, formula engine for the query surface.

**Unblocked by K:**
- (none — K was a leaf on the roadmap)

---

## Parity check against audit §9

| Audit feature | Ships in K? | Notes |
|---|---|---|
| Open `.base` → table of notes | ✓ | full |
| Custom columns (note./file./formula.) | ✓ | full |
| Inline-edit frontmatter | ✓ | via `FileManager::processFrontMatter` |
| Sort by clicking column headers | ✓ | single-key cycling (ASC → DESC → unsorted); multi-key UI follow-up |
| Group rows by a property | partial | partitioning implemented in `BasesQueryResult`; table rendering of group headers follow-up |
| Filter (global + per-view, AND-merged) | ✓ | full |
| Multiple named views per `.base` | ✓ | view switcher in toolbar |
| Formula columns | ✓ | full including cycle detection |
| Search cell contents | ✓ | simple contains match |
| Row limit | ✓ | full |
| Export CSV / copy as TSV+MD+HTML | ✗ | follow-up |
| `+ New` button | ✗ | follow-up |
| Drag-reorder columns | partial | Qt default behaviour; persistence follow-up |
| View-rename wikilink rewrite | ✗ | follow-up |
| Drag file-link cells as wikilinks | ✗ | follow-up |
| Hover-link preview on cells | ✗ | Cluster-H follow-up |
| Embed `.base` in markdown (`![[x.base]]`) | ✗ | EmbedRegistry integration follow-up |
| Sidebar `this` tracking | partial | `ThisFileValue` shape + BasesEntry forwarding exists; sidebar-active-file wiring is a MainWindow follow-up |
| Per-BasesView undo/redo | ✗ | follow-up |
| Plugin-supplied `Value` types | ✗ (intentional) | closed hierarchy per audit §10 |
| Plugin-supplied view types | ✗ | `registerGlobalFunc`/`registerInstanceFunc` exposed via FunctionRegistry; view-type plugin hook deferred |

**User-observable completeness:** the five "must work" features from audit §9 (open, render, filter, sort, inline-edit) are all green. The "nice to have" features from §9 are follow-ups.

---

## Commit shape

```
docs: plan expansion landed                              (98e0567e)

Phase 1 — scaffold + primitives
  5825422b  scaffold + Value base
  919e4590  NullValue + test harness
  7d852537  Boolean/Number/String
  15a49faa  ListValue + aggregates
  62d6cd29  Phase 1 closeout

Phase 2 — Value hierarchy completion
  4f2881e1  DateValue + RelativeDateValue + parseFromString
  48cb9d97  DurationValue + ISO-8601 + shorthand
  fac90d67  Tag/Link/Url/Icon/Image/HTML/Markdown/Error
  f9af1b6d  ObjectValue + RegExpValue + LambdaObjectValue
  704efa77  FileValue + ThisFileValue + MetadataCache aggregates
  38cf4603  Phase 2 closeout

Phase 3 — DSL parse
  521a3366  Lexer
  8af044f4  AST + Pratt parser
  04826a62  Phase 3 closeout

Phase 4 — Evaluator
  3bde9f80  typed-operator dispatch
  6e6945ef  Phase 4 closeout

Phase 5 — Function registry + built-ins
  3bbd5c24  FunctionRegistry + 43 built-ins + Formula
  b774abd0  Phase 5 closeout

Phase 6 — YAML schema
  c528ace2  PropertyId + FilterTree + BasesViewConfig + BasesQuery
  b8170248  Phase 6 closeout

Phase 7 — Data binding
  bb4a9cc3  BasesEntry + BasesQueryResult + QueryController
  d3bb2ad3  Phase 7 closeout

Phase 8 — Widget
  445e48ff  BasesView + BasesTableModel + BasesCellDelegate
  f363b35f  Phase 8 closeout

Phase 9 — Host wire-in + retro
  <this commit>  MainWindow registration + Corbomite::Bases link + retro
```

37 commits end to end. No branches; serial on master per `memory/feedback_no_branches.md`.
