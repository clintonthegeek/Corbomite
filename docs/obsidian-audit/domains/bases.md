# `obsidian/bases` — typed-value database view ("Bases" feature)

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/bases/`
**File count:** 25
**Files:** `BasesEntry.js`, `BasesEntryGroup.js`, `BasesQueryResult.js`, `BasesView.js`, `BasesViewConfig.js`, `BooleanValue.js`, `DateValue.js`, `DurationValue.js`, `FileValue.js`, `HTMLValue.js`, `IconValue.js`, `ImageValue.js`, `LinkValue.js`, `ListValue.js`, `NotNullValue.js`, `NullValue.js`, `NumberValue.js`, `ObjectValue.js`, `PrimitiveValue.js`, `RegExpValue.js`, `RelativeDateValue.js`, `StringValue.js`, `TagValue.js`, `UrlValue.js`, `Value.js`.

**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> Bases is Obsidian's database/spreadsheet view over the vault's frontmatter. A user creates a `.base` file describing filters, formulas, groupings and a view config (table/cards/list/etc.); Bases rolls every matching note's frontmatter into a `BasesEntry`, evaluates expressions/formulas through a typed `Value` hierarchy, and renders interactive table-like views. This is the post-1.9 release feature parity-blockers complain about most; **Corbomite has no equivalent**. Key exports: `BasesView` (`TextFileView` subclass), `BasesViewConfig`, `BasesEntry`, `BasesEntryGroup`, `BasesQueryResult`, `Value` and ~18 typed subclasses. Reads + writes `.base` files (YAML); reads frontmatter from every matched note via `MetadataCache`. **MISSING from Corbomite — largest single feature gap.** Pass 2 focus: (a) the `.base` file schema; (b) the `Value` type hierarchy and formula/comparison semantics; (c) `BasesQueryResult` incremental refresh.

**De-minifier artifact note:** Of 25 files only **5 unique extraction windows** exist; 20 are byte-identical exports differing only in their `// public API symbol:` comment. The clusters: **`app.js 123910–124529`** (8 files — canonical `Value.js` contains `Value`, `yW`(Error), `NullValue`, `NotNullValue`, `PrimitiveValue`, `StringValue`, `NumberValue`, `BooleanValue`, `ListValue`); **`app.js 124533–125332`** (12 files — canonical `TagValue.js` contains `TagValue`, `DW`(TagListValue), `ObjectValue`, `RegExpValue`, `DateValue`, `RelativeDateValue`, `DurationValue`, `IconValue`, `FileValue`, `UrlValue`, `LinkValue`, `ImageValue`, `HTMLValue`); **`app.js 133498–133942`** (canonical `BasesEntry.js` — `BasesEntry` + `QueryContext`(iY) + filter-tree classes + `PropertyConfig`); **`app.js 133955–134411`** (canonical `BasesViewConfig.js` — `BasesViewConfig` + `BasesQuery`(EY)); **`app.js 141808–144969`** (3 files — canonical `BasesView.js`, 3167 lines). In `BasesView.js`, lines 5–169 are the **trailing `TextFileView` class** from `views/` domain (extractor bleed-over — already audited in `views.md`); actual Bases content begins at line 170 with `VX = "bases"`. Also noted: the `QueryController`, `parsePropertyId`, `nY`/`xY`/`SY`/`OY`/`LY`/`kY`/`PY` utility functions, `MY`/`TY`/`FY` (filter-parser, view-config-builder, default-folder-resolver), and `RK`/`DK`/`JK`/`PX`/`IY` (formula error/container/language-support/extension) are **referenced but not declared** inside the `bases/` extraction — they live in adjacent app.js regions. The formula expression grammar (operators, function vocabulary) is therefore **not visible inside this domain's source**. After de-duplication the audit corpus is 5 canonical files, ~5500 lines of source reading.

---

## 1. Public API surface

19 architecturally distinct classes. The `Value` hierarchy (18 subclasses) is bulk, with shared patterns: `toString()`, `isTruthy()`, `equals(other)`, `looseEquals(other)`, `renderTo(el, ctx)`, `keys()`, `objectAccess(name)`, static `type` string, and instance `icon` (lucide key). Only the architecturally-important classes are detailed below; the 18 `Value` subclasses are summarised in a table. Shell classes (`BasesView`, `BasesEntry`, `BasesQueryResult`, `BasesViewConfig`, `BasesQuery`) get full subsections.

### `Value` (`bases/Value.js:154`) — abstract base

- **Kind:** class. **Exported as:** `Value`. Static `type = "Any"`.
- **Signature:** Zero-arg constructor; instance `icon: string`. Static `Value.equals(a, b)` / `Value.looseEquals(a, b)` are null-safe. Instance API: `equals`/`looseEquals` (override), `renderTo(el, ctx)` (override), `keys() → string[]`, `objectAccess(name) → Value | null`, `isTruthy()`, getter `type` (returns `this.constructor`).
- **Semantics:** Every cell in a Bases table is a `Value` subclass. Equality is double-tiered: `equals` is same-type structural; `looseEquals` is cross-type coerced (e.g. `StringValue("2024-01-01")` looseEquals `DateValue(2024-01-01)`). `renderTo` writes DOM into `el` using `RenderContext` for shared primitives (file-link, external-link, tag chips).
- **Lifecycle:** Pure data. Instantiated lazily by `ObjectValue.fromFrontMatter`'s lazy evaluator, by `BasesEntry.objectAccess`, or by formula evaluation. Never destroyed — held by transient `BasesEntry` / `BasesQueryResult` references.
- **Mixes in:** neither.

### `Value` subclass table

| Class | Static `type` | Native wrap | Key override behaviour |
|---|---|---|---|
| `NullValue` | `"Null"` | singleton sentinel | `isTruthy() → false`; `equals()` → `true`; render no-op. Constructor throws after singleton init — use `NullValue.value`. |
| `NotNullValue` | inherited | abstract | Marker base for all non-null values. |
| `PrimitiveValue` | inherited | `T` | `equals: ===`, `looseEquals: ==` (JS-coerced). |
| `StringValue` | `"String"` | `string` | `objectAccess("length")` → `NumberValue`. |
| `NumberValue` | `"Number"` | `number` | Renders `"∞"` for non-finite. |
| `BooleanValue` | `"Boolean"` | `boolean` | Renders disabled `<input type="checkbox">`. |
| `ListValue` | `"List"` | `Array<Value\|raw>` | Lazy element wrap. Aggregate methods `sum`/`mean`/`median`/`min`/`max`/`stddev` (numeric only), `earliest`/`latest` (date only), `unique`/`sort`/`flatten`/`concat`/`join`/`includes` (uses `looseEquals`). |
| `TagValue` (→`StringValue`) | inherited | `string` | Stores `lowerTag`; `tagMatches(other)` is hierarchical-prefix (`#parent` matches `#parent/child` via `/` boundary). Render: `RenderContext.renderTag` → click→global-search. |
| `DW` (internal `TagListValue`) | inherited | `Array<TagValue>` | `includes` uses `tagMatches` (hierarchical), not `looseEquals`. Not in public-API symbol list. |
| `ObjectValue` | `"Object"` | `Record<string, Value\|raw>` | Case-insensitive `getInsensitive`. Static **`fromFrontMatter(app, file, raw)`** — wraps raw frontmatter with a lazy evaluator that auto-coerces strings to `LinkValue`/`UrlValue`/`DateValue`, arrays and nested objects recursively; `"tags"` key becomes `DW` (TagListValue). |
| `RegExpValue` | `"RegExp"` | `RegExp` | `isTruthy() → true`. Formula-only (not produced from frontmatter). |
| `DateValue` | `"Date"` | `{date: Date, time: boolean}` | `time` flag distinguishes date-only from datetime. `objectAccess`: `"year"`/`"month"`(1-based)/`"day"`/`"hour"`/`"minute"`/`"second"`/`"millisecond"`/`"timestamp"` → `NumberValue`. `parseFromString` accepts `YYYY-MM-DD` (date-only) or `YYYY-MM-DD[ T]HH:MM[:SS[.ms]][TZ]` (datetime). Render: disabled `<input type="datetime-local"\|"date">`. |
| `RelativeDateValue` (→`DateValue`) | inherited | same | `toString()` → `moment(date).fromNow()`; render uses relative text. Formula-only. |
| `DurationValue` | `"Duration"` | 7-field `{years, months, days, hours, minutes, seconds, milliseconds}` | `addToDate(dateVal, subtract?)` calendar-aware. `getMilliseconds()` via `Date` math. `objectAccess("years"/"months"/"weeks"/"days"/"hours"/"minutes"/"seconds"/"milliseconds")` via `moment.duration`. `parseFromString`: ISO-8601 `PnYnMnWnDTnHnMnS` or shorthand `"5 days"`/`"3w"`/`"-2h"` (singular/plural; uppercase `M`=months, lowercase `m`=minutes). Render: `moment.duration(...).humanize()`. |
| `IconValue` (→`StringValue`) | inherits `"String"` | `string` (lucide key) | Render: `getIcon(data)` or `"question-mark-glyph"` fallback. |
| `FileValue` | `"File"` | `{app, file: TFile}` | Cached aggregate accessors `getLinks`/`getBacklinks`/`getEmbeds`/`getTags`/`getProps` (per-instance cache). `objectAccess` returns one of the 14 `file.*` members (see §2). Render: `RenderContext.renderFileLink`. |
| `UrlValue` (→`StringValue`) | `"URL"` | `{data, display?}` | Render: `RenderContext.renderExternalLink` → `<a target="_blank" rel="noopener">`. |
| `LinkValue` (→`StringValue`) | `"Link"` | `{app, data: linkpath, sourcePath, display?}` | `toString()` → `"[[linkpath\|display]]"`. `looseEquals` resolves wikilinks to `TFile`s. `resolve()` via `metadataCache.getFirstLinkpathDest`. Static `parseFromString(app, text, sourcePath)` for `[[…]]` syntax; `fromReference(app, sourcePath, ref)` from MetadataCache reference. |
| `ImageValue` (→`StringValue`) | `"Image"` | `string` (path or URL) | Render: if vault-path (`YC(data)`), resolves via `metadataCache`+`vault.getResourcePath`; else `<img src=data>` with desktop `file:///` → `Platform.resourcePathPrefix` rewrite. |
| `HTMLValue` (→`StringValue`) | `"HTML"` | `string` | Render: `sanitizeHTMLToDom(data)` + `app.fixFileLinks`. Produced by formulas. |
| unnamed `MarkdownValue` (→`StringValue`) | `"Markdown"` | `string` | Bundled with `HTMLValue` in source (`bases/TagValue.js:776`). Render: `Zx(Kx(data))` markdown→DOM pipeline, then sanitise. Formula-only. |
| `yW` (internal `FormulaError`→`Value`) | `"Error"` | `{message: string}` | Represents a formula evaluation error inline. Render: `.bases-formula-error` div with warning icon + message + tooltip. |

### `BasesEntry` (`bases/BasesEntry.js:34`)

- **Kind:** class. **Exported as:** `BasesEntry`.
- **Signature:** Constructor `(ctx: QueryContext, file: TFile)`. Fields: `ctx`, `app`, `file`, `frontmatter` (raw `Record<string, unknown>` — direct alias into `MetadataCache.getFileCache(file).frontmatter ?? {}`), `note: ObjectValue` (wrapped frontmatter), `implicit: FileValue`, `formulaResults: aY`, `formula: lY`.
- **API:** `getValue("note.x"|"file.x"|"formula.x") → Value|null` (null for unrecognised), `getByIdentifier(name) → Value` (handles `"this"`/`"note"`/`"file"`/`"formula"` plus frontmatter fallback), `getPropertyKeys() → string[]` (raw frontmatter keys), `getRawProperty(key) → unknown` (case-insensitive), `keys()` (frontmatter keys + `"this"`+`"note"`+`"file"`+`"formula"`). Static `FILE_PROPERTIES` — array of 14 `file.*` property IDs.
- **Purpose:** One vault note as Bases sees it. Reads frontmatter at construction; stale if `MetadataCache` mutates after. Not stable across re-evaluation — never store across event boundaries.
- **Lifecycle:** Created per-matching-note during `BasesQueryResult` construction. `aY` (formula-results cache) implements per-entry memoisation with cycle detection via `rY = Symbol()` sentinel (throws on re-entry).

### `QueryContext` (`iY` at `bases/BasesEntry.js:5`)

Holds `{app, filter, formulas, _local: BasesEntry|null, localUsed: boolean}`. The `local` getter sets `localUsed = true` so the controller knows whether the query depends on the active sidebar file. `regenerateLocal()` rebuilds for a new current-file.

### Filter-tree classes (`bases/BasesEntry.js:198–373`)

Abstract base `uY` (`Filter`) with statics `Filter.and(...)`, `.or(...)`, `.not(...)` — all call `.optimize()`. Subclasses:
- **`hY` (`FilterRule`)** — leaf; wraps `rule` (parsed formula expression). `test(entry) → rule.test(entry)`. `serialize() → rule.toString()` (single-string predicate).
- **`pY` (`FilterConjunction`)** — abstract holder; `conjunction: "and"|"or"|"not"`, `filters: uY[]`. `serialize()` produces `{and|or|not: [...]}`.
- **`dY` (`AndFilter`)**, **`fY` (`OrFilter`)**, **`mY` (`NotFilter`)** — concrete. Each has `optimize()` (collapses single-child wrappers, flattens same-conjunction nesting) and `test(entry)` (short-circuit AND / OR; NOT is "no child passes").

### `PropertyConfig` (`gY` at `bases/BasesEntry.js:390`)

Per-property metadata at `BasesQuery.properties[propId]`. Fields: `propertyId`, `displayName?`, `unrecognizedData`. `getDisplayName()` falls back to `vY[propertyId]()` for built-in `file.*` names, else the humanised key. Persisted only when serialise-non-empty.

### `BasesViewConfig` (`bases/BasesViewConfig.js:5`)

- **Kind:** class. **Exported as:** `BasesViewConfig`.
- **Signature:** Constructor `(query, type, name)`. Fields: `type: string`, `name: string`, `filters?`, `groupBy? {property, direction}`, `order?: PropertyId[]`, `sort?: Array<{property, direction}>`, `limit?: number`, `summaries?: Record<PropertyId, string>`, `data?: Record<string, unknown>` (free-form per-view-type options).
- **API roles:** *CRUD* — `clone(newName)`, `serialize`, `get`/`set`/`getAll`/`getAsPropertyId`; *Order* — `getOrder`/`setOrder`; *Sort* — `getSort`/`setSortProperty(propId, "ASC"|"DESC"|"TOGGLE"|"NONE")`; *Group* — `setGroupBy({property, direction}|null)`; *Limit* — `getLimit`/`setLimit(n)` (0=unlimited); *Summaries* — `getSummaryKey`/`setSummaryKey`; *Display* — `getDisplayName`/`getPropertyConfig`; *Formula-eval* — `getEvaluatedFormula(controller, key)` parses `data[key]` as formula, evaluates against controller's current-file `BasesEntry`, returns `NullValue.value` on error. Every mutator calls `this.query.save()` which piping through to `BasesView.saveQuery` → debounced 2s disk write.

### `BasesQuery` (`EY` at `bases/BasesViewConfig.js:158`)

- **Kind:** class. **Exported as:** `BasesQuery` (Obsidian docs name).
- **Signature:** Constructor `()`. Fields: `views: BasesViewConfig[]`, `filters?: FilterTree`, `formulas: Record<name, DK>`, `summaryFormulas?: Record<name, DK>`, `properties: Record<PropertyId, PropertyConfig>`, `unrecognizedData`, `newItemFolder?`, `newItemTemplate?`, `file?: TFile` (attached post-load), `saveFn?: (q)=>void` (attached).
- **API:** Static `fromString(text)` — empty yields default 1-view "Table" query; else `parse(parseYaml(text))`. Static `parse(obj)` — strict; throws localised errors for schema violations (see §3). `getSerializable()` produces plain object for `stringifyYaml`, round-tripping `unrecognizedData`. `toString()` = `stringifyYaml(getSerializable())` or `""`. `clone()` deep-copies via serialise+parse. Mutators: `setGlobalFilters`, `setViewFilters`, `setFormulas`, `setSummaryFormula`, `deleteSummaryFormula`, `renameSummaryFormula`, `getSummaryFormula`, `getPropertyConfig`, `removeFormula`, `getViewConfig(name?|undefined→views[0])`. All mutators call `save()` → `saveFn(this)`.

### `BasesEntryGroup` (`bases/BasesView.js:2472`)

- **Kind:** class. **Exported as:** `BasesEntryGroup`.
- `new BasesEntryGroup(entries: BasesEntry[], key?: Value)`. `hasKey()` → `!!key && !Value.equals(key, NullValue.value)`. Produced by `BasesQueryResult.groupedData` partitioning.

### `BasesQueryResult` (`bases/BasesView.js:2483`)

- **Kind:** class. **Exported as:** `BasesQueryResult`.
- Constructor `(app, config, allProperties, data)` — calls `applySort(data)` and `applyLimit(data)` in-place.
- **Lazy props:** `groupedData: BasesEntryGroup[]` (cached) — partitions by `config.groupBy.property`; null-keyed group sorted to end. `properties: PropertyId[]` (cached) — intersection of `config.getOrder()` with known properties plus any `note.*` from `MetadataTypeManager`.
- **Methods:** `applySort` — sorts via per-pair type dispatch (nulls last; bool by isTruthy; number numeric; date by `.date.getTime()`; duration by `.getMilliseconds()`; else locale string-compare via `ub()`; multi-key tie-break recurses to next sort key). `applyLimit` — splices past `config.getLimit()`. `getSummaryValue(view, group, propId, fnName) → Value` — per-group summary-formula evaluation cached in `WeakMap<group, {propId: {fnName: Value}}>`.

### `BasesView` shell (`HX` at `bases/BasesView.js:171`) — `TextFileView` subclass

- **Kind:** class. **Exported as:** `BasesView`.
- **Signature:** Constructor `(leaf, plugin)`. Sets `navigation = true`, `isPlaintext = false`, `lastData = ""`. Builds a `QueryController` attached to `contentEl`.
- **View metadata:** `getViewType() → "bases"` (constant `VX`); `getIcon()` from registered view-type's icon or `"lucide-layout-list"`; `canAcceptExtension(ext) → ext === "base"`.
- **State:** `getState/setState` extends `TextFileView` with `viewName: string` (active named view within the `.base`).
- **Persistence:** `getViewData()` → `query?.toString() ?? lastData`. `setViewData(text, clear)` → `BasesQuery.fromString(text)`, attaches `query.file` and `query.saveFn`, pushes into `controller.setQuery`.
- **Sidebar coupling:** listens to `workspace.on('file-open')` and `workspace.on('layout-change')`; in sidebar the controller's current-file tracks the workspace's active file. Plus `receiveSyncState` (Obsidian Sync) and `onGroupChange` (linked-tab-group).

### `BasesView` layout-abstract (`bases/BasesView.js:2679`) — distinct from `HX`

- The **layout-renderer base class** that per-view-type (table/cards/list) layouts extend. Owned by `QueryController.view`. Extends `Component`.
- **Methods:** `getViewActions()` (results-menu extra actions), `createFileForView(targetEl, fmMutator?)` (+ button), `createTransaction(mutator)` (frontmatter edit with undo/redo), `updateProperty(file, name, value)` (inline-edit write via `FileManager.processFrontMatter`), `undoTransaction`/`redoTransaction` (per-`BasesView` undo stack), `exportTable()` → 4-format exporter, `copyToClipboard()` (TSV+Markdown+HTML+`obsidian/table`), `createGroupHeadingEl(group)`, `createRenderer(propId, el)` dispatching between three cell renderers.
- **Lifecycle:** Constructed by plugin-supplied factory `(controller) => new XLayout(controller)`. Destroyed on view-type swap or parent shell unload.

### Cell renderers (`bases/BasesView.js:2991–3166`)

- **`p$`** — abstract `(view, prop, el)`.
- **`d$` (`MetadataPropertyRenderer`)** — for `note.*`. Reads raw frontmatter via `entry.getRawProperty`, looks up inferred type via `metadataTypeManager.getTypeInfo`, instantiates matching property editor; `onChange(newValue)` → `view.updateProperty(file, name, newValue)`. **This is the inline-edit path.**
- **`f$` (`FileNameRenderer`)** — for `file.name`. Renders `RenderContext.renderFileLink`.
- **`m$` (`ValueRenderer`)** — everything else. Reads `entry.getValue(propId)`, catches errors, sets `data-property-type` attr, calls `value.renderTo(el, app.renderContext)`. On error, shows `.bases-formula-error` with dblclick→open formula editor.

### `NewItemMenu` (`jX` at `bases/BasesView.js:351`)

"+" button. Creates new note in `query.newItemFolder` (defaulting via `FY(...)` to template's parent or standard new-file parent), seeded with frontmatter from `query.newItemTemplate` or computed filter-satisfying defaults. Opens inline `HoverEditor` popover for title rename.

### `FormulaEditor` (`UX` at `bases/BasesView.js:565`)

Small CodeMirror editor. Validates by re-parsing into `DK`; shows valid/invalid icon + error tooltip. Static `appendHelpIcon(el)` opens <https://help.obsidian.md/bases/functions>.

### Toolbar family (`bases/BasesView.js:659–2470`)

- **Properties menu** (`GX`/`KX`/`YX`/`QX`) — visible-properties list + per-property edit (display name, type, formula) + reorder via drag + "Add property"/"Add formula"/"Hide all".
- **Sort+group menu** (`e$`/`t$`/`n$`/`i$`) — sort-row stack + group-by row with property combobox + direction selector.
- **Views menu** (`s$`/`a$`/`l$`) — named-view picker + per-view config form (name, layout-type dropdown, type-specific options, "Set default"/"Duplicate"/"Delete").
- **Search bar** (`ZX`) — full-text filter over rendered cells; updates via `queryController.updateSearchQuery`.
- **Results menu** (`$X`) — limit input, "Show all", "Copy table" (4 MIME types), "Export CSV", per-layout actions.

---

## 2. Data structures

### `BasesQuery` (in-memory mirror of `.base`)

```typescript
{
  views: BasesViewConfig[];        // never empty after parse — default 1 "Table" view
  filters?: FilterTree;            // global; AND-merged with per-view filters
  formulas: Record<string, DK>;    // name → parsed formula expression
  summaryFormulas?: Record<string, DK>;
  properties: Record<PropertyId, PropertyConfig>;
  unrecognizedData: Record<string, unknown>;       // forward-compat round-trip
  newItemFolder?: string;
  newItemTemplate?: string;
  file?: TFile;                    // attached after setViewData
  saveFn?: (q) => void;            // debounced disk write
}
```

### `BasesViewConfig` (one named view)

```typescript
{
  query: BasesQuery;               // back-pointer
  type: string;                    // "table" | "cards" | "list" | plugin-registered
  name: string;                    // unique within query.views
  filters?: FilterTree;            // AND-merged with query.filters
  groupBy?: {property: PropertyId, direction: "ASC"|"DESC"};
  order?: PropertyId[];            // visible-column order
  sort?: Array<{property: PropertyId, direction: "ASC"|"DESC"}>;  // multi-key
  limit?: number;                  // 0 = unlimited
  summaries?: Record<PropertyId, string>;
  data?: Record<string, unknown>;  // free-form per-view-type options
}
```

### `PropertyId`

String with one of three prefixes:
- `"note.<key>"` — frontmatter property (case-insensitive lookup).
- `"file.<member>"` — implicit file metadata. 14 members: `file.file`, `file.name`, `file.basename`, `file.fullname`, `file.path`, `file.folder`, `file.ext`, `file.ctime`, `file.mtime`, `file.size`, `file.links`, `file.backlinks`, `file.embeds`, `file.tags`.
- `"formula.<name>"` — named formula from `BasesQuery.formulas[name]`.

Parser `parsePropertyId(s) → {type, name}` / builder `nY(type, name) → string` live outside this domain.

### `FilterTree` (rooted at `uY`)

```typescript
type FilterTree = FilterRule | FilterConjunction;
type FilterRule = {rule: ParsedRule};                        // hY
type FilterConjunction = {
  conjunction: "and"|"or"|"not",
  filters: FilterTree[]
};
type ParsedRule = {
  formula: ExpressionAST,                                    // DK or RK on parse error
  toString(): string,                                        // serialises as single-line predicate string
  test(entry: BasesEntry): boolean,
};
```

The leaf rule's serialised form is a **formula-expression string** (e.g. `"note.status == \"open\""`); the expression DSL is owned by `DK`/`RK`/`JK`/`PX` outside this domain.

### `BasesEntry`, `BasesQueryResult`, `QueryContext`

See §1 — shapes already documented there.

---

## 3. On-disk contracts

**This section is Corbomite's hard compatibility target.**

### `.base` files

- **Path:** any vault location with `.base` extension. Treated as a `TFile` with `extension === "base"`.
- **Format:** **YAML** (parsed via `parseYaml`, written via `stringifyYaml` — `BasesViewConfig.js:175,358`). Empty file is valid (yields default 1-view "Table" query). Non-object root throws `msgErrorInvalidQueryFormat`.
- **Writer:** `BasesView.requestSave` (debounced 2s) → `TextFileView.save` → `vault.modify(file, query.toString())`. Synchronous `saveImmediately` on tab close.
- **Reader:** `BasesView.setViewData(text)` → `BasesQuery.fromString(text)`. Re-read on `vault.on('modify')` for the same file.

#### Schema

```yaml
# All top-level keys optional. Unknown keys preserved in unrecognizedData, round-trip on save.

filters:                          # optional global FilterTree
  ...

views:                            # array of named views; defaults to [{type: "table", name: <localised>}]
  - type: table                   # required; "table" or any plugin-registered view-type
    name: "All notes"             # required; unique within views; regex-validated (no "#:|^[[ ]] %%")
    filters: ...                  # optional view-scoped FilterTree (AND-merged with global)
    order:                        # optional visible-column order
      - file.name
      - note.status
      - formula.priority
    sort:                         # optional multi-key sort; first = primary
      - {property: note.due, direction: ASC}
      - {property: file.name, direction: ASC}
    groupBy:                      # optional grouping (single-keyed)
      property: note.status
      direction: ASC
    limit: 100                    # optional row cap; 0 / absent = unlimited
    summaries:                    # optional per-property summary function name
      note.amount: sum
    # Free-form view-type-specific options alongside recognised keys:
    image: note.cover             # e.g. for "cards" view
    rowHeight: 120

properties:                       # optional Record<PropertyId, PropertyConfig>
  note.status:
    displayName: "Status"
  formula.priority:
    displayName: "Priority"
    # plus any unrecognized keys for forward-compat

display:                          # LEGACY — migrated at parse into properties[].displayName
  note.status: Status

formulas:                         # optional Record<name, expressionString>
  priority: "if(note.urgent, 1, 2)"
  weighted: "note.score * 0.7 + formula.priority * 0.3"

summaries:                        # optional Record<name, expressionString>
  totalAmount: "sum(note.amount)"

newItemFolder: "Inbox"            # optional; + New button target
newItemTemplate: "Templates/Task.md"  # optional
```

#### Filter grammar (`FilterTree` serialisation)

At any level, one of:
- **Conjunction** — `{and: [<filter>, …]}` / `{or: [<filter>, …]}` / `{not: [<filter>, …]}` (`BasesEntry.js:285-298`). Empty lists collapse via `optimize()`.
- **Atomic rule** — a **string** formula expression, parsed at load time. Example: `"note.status == \"open\""`. **The expression DSL is not defined inside `bases/`** — it lives in the `DK` formula parser at adjacent app.js (see Open Q1).

Example:

```yaml
filters:
  and:
    - "file.ext == \"md\""
    - or:
        - "note.status == \"open\""
        - "note.status == \"in_progress\""
    - not:
        - "file.tags.contains(\"archive\")"
```

#### Migration / versioning

- **No `version` field.** Forward-compat via `unrecognizedData` preservation at every nesting level (`BasesQuery`, `BasesViewConfig`, `PropertyConfig`).
- **Legacy `display:` migration** — the legacy top-level `display: {propId: displayName}` is migrated into `properties[propId].displayName` at parse (`BasesViewConfig.js:214-232`). New writes use only `properties`.
- **No file-level schema header.** Corbomite's writer must preserve unknown keys identically; YAML key-ordering should be preserved by the serialiser.

#### Validation errors

`BasesQuery.parse` throws localised errors for: non-object root; `views` not array; `properties` / `display` not object; `display` values not strings; `formulas` / `summaries` not object or non-string values; `newItemFolder` / `newItemTemplate` not strings. `BasesView.setViewData` catches and stores the error; the view shows an error banner.

### Frontmatter (read-only)

Bases reads (never writes) frontmatter via `app.metadataCache.getFileCache(file).frontmatter`. **`BasesEntry.frontmatter` is a live alias.** Mutations go through `FileManager.processFrontMatter` (the inline-edit path in `BasesView.updateProperty` / `createTransaction`).

### No `.obsidian/` files

Bases writes nothing under `.obsidian/`. Internal-plugin settings (if any) would be at `.obsidian/plugins/bases/data.json` per convention, but no such read/write is visible inside `bases/`.

### `file-recovery` backup on save failure

On `vault.modify` throw, `TextFileView.save:106` calls `fileManager.storeTextFileBackup(file.path, content)` — writes into `file-recovery` internal plugin's data store.

---

## 4. Events emitted

Bases owns no public-facing `Events` emitter. `QueryController` (out-of-domain) has an internal `events` emitter to which `BasesView` subscribes for one signal:

| Event name | Payload | Triggered when | Typical consumers |
|---|---|---|---|
| `view-changed` | `()` | Active named view changes | `BasesView.onViewChanged` (`BasesView.js:188,342`) — calls `workspace.requestSaveLayout()` + `leaf.updateHeader()`. |

`Value`/`BasesEntry`/`BasesQueryResult` never emit events.

---

## 5. Events consumed

| Listener | Subscribes to | Why |
|---|---|---|
| `BasesView.js:27` | `vault.on('modify', onModify)` (via `TextFileView`) | External modify to `.base` → reload via `loadFileInternal` |
| `BasesView.js:200-205` | `workspace.on('file-open', updateCurrentFile)` | Sidebar mode tracks active note as `this` |
| `BasesView.js:207-212` | `workspace.on('layout-change', onLayoutChange)` | Toggle `navigation` based on sidebar-mode |
| `Value.js:90-99` | *emitter* `workspace.trigger('hover-link', {source: "bases"})` | File-link cells on mouseover |
| `BasesView.js:188` | `controller.events.on('view-changed')` | Save layout + tab-header refresh on view-name change |

**`MetadataCache` events** — Bases needs `changed`/`resolved` for incremental refresh, but the listener is in `QueryController` (out-of-domain). See Open Q3.

---

## 6. Commands registered

`No commands registered here.` `BasesView` as a `TextFileView` registers no `addCommand` calls inside `bases/`. The `BasesPlugin` internal plugin (outside this domain) is the likely source of any `bases:`-prefixed commands. Toolbar-bound actions (Search, Sort, Copy, Export CSV) are direct `addEventListener` handlers.

---

## 7. Registries owned

### `BasesPluginInstance.viewTypeRegistrations`

- **Stores:** `Record<typeKey, {name(), icon, options(viewConfig) → OptionDescriptor[], create(controller) → BasesView}>`. Shape inferred from `plugin.getRegistration(type)` and `plugin.getRegistrations()` call sites (`BasesView.js:228,1930,2231,2254`).
- **Populated by:** `BasesPlugin` internal-plugin startup; built-ins at minimum `"table"`. Plugin-extendability unconfirmed (Open Q5).
- **Read by:** `BasesView.getIcon`, Views-toolbar icon picker, View-config form layout-type dropdown, per-view-type options rendering.
- **Persistence:** in-memory; rebuilt on plugin load.

The `BasesPluginInstance` class itself is not in this domain — lives at `obsidian/plugin/internal-plugins/bases/` (not extracted).

### `Workspace.operatorFuncConfigs` (owned by `workspace/`, registered by `BasesPlugin`)

Per `workspace.md:369-371`. Registry of comparison-operator descriptors keyed by id. Populated by `BasesPlugin.onload`; read by `DK` formula parser. Not directly touched inside `bases/`.

---

## 8. Invariants

`[CRIT]` = required for `.base` file-format compatibility.

- `[CRIT]` **Top-level YAML object.** Root must be an object (or empty) — `BasesViewConfig.js:178`.
- `[CRIT]` **Empty `.base` is valid.** Yields default 1-view "Table" query. Corbomite must replicate or empty files error open.
- `[CRIT]` **Unknown keys round-trip verbatim.** `unrecognizedData` at every level (`BasesQuery`, `BasesViewConfig`, `PropertyConfig`). Read→serialise must not lose keys.
- `[CRIT]` **`views` order matters.** `views[0]` is default when `getViewConfig(undefined)`.
- `[CRIT]` **View name uniqueness.** Validator enforces on editor; tolerate duplicates on read.
- `[CRIT]` **`PropertyId` grammar.** `note.*` / `file.*` / `formula.*` with exactly the 14 `file.*` members (`BasesEntry.FILE_PROPERTIES`). Frontmatter key lookup case-insensitive; persisted casing should preserve user input.
- **Sort / group direction strings are uppercase** (`"ASC"`/`"DESC"` only).
- **`limit: 0` means "unlimited"** — not `null`, not absent, not negative.
- **`NullValue` is a singleton.** `new NullValue()` after init throws. Always use `NullValue.value`.
- **Sort puts nulls last** — both row sort and group sort (`BasesView.js:2616-2617,2542-2546`).
- **Loose-equals is symmetric.** `Value.looseEquals(a,b)` tries `a.looseEquals(b)` then `b.looseEquals(a)`.
- **`BasesEntry.frontmatter` is a live alias** into `MetadataCache`. Mutations must go through `FileManager.processFrontMatter`.
- **`FileValue` aggregate caches are per-instance.** `_cachedLinks`/`_cachedBacklinks`/`_cachedEmbeds`/`_cachedTags`/`_cachedProps`. Query refresh creates new `FileValue`s so cache is naturally bounded.
- **Formula infinite-loop detection is per-entry.** `aY.getFormulaValue` uses `rY = Symbol()` sentinel; throws on re-entry.
- **`BasesView` sets `isPlaintext = false`** — skips 3-way merge on external modify; accepts disk verbatim.
- **Save is debounced 2s** (via `TextFileView`); `saveImmediately` on tab close.
- **`BasesEntry.FILE_PROPERTIES` is closed** — plugins cannot extend.
- **`getValue` returns `null` (not `NullValue.value`) for unrecognised property-ID types** — `BasesEntry.js:67-68`. Renderers special-case `null`/`undefined`/`NullValue.value` together.

---

## 9. Observable user features

- Create a `.base` file anywhere; opens as a Bases view (via `ViewRegistry` binding `base` → `"bases"`).
- See a **table of every vault note matching the filter**, one row per note, one column per visible property.
- Add columns for any frontmatter key (`note.*`), file metadata (`file.*`), or formula (`formula.*`).
- **Edit frontmatter inline** by clicking a cell — uses `metadataTypeManager`'s widget for the inferred type. Writes back to source note. **This is the spreadsheet-like differentiator.**
- **Sort by clicking column headers** (multi-key, cycle ASC→DESC→unsorted).
- **Group rows by a property** with collapsible group headings + optional summary cells (sum/count/mean).
- **Filter** with global + per-view filters (AND-merged).
- **Multiple named views** in one `.base` file, each with own filter/columns/sort/group/view-type. Switch via toolbar.
- **Define formulas** in the `.base`; reference as columns. Access `note.*`, `file.*`, `formula.*`, `this.*` (active sidebar file).
- **Search across rendered cells** (debounced input).
- **Row limit** cap.
- **Export to CSV** / copy as TSV / Markdown / HTML / `obsidian/table` (Obsidian-native paste).
- **+ New button** creates note with filter-satisfying frontmatter pre-populated (optionally from template).
- **Drag-reorder columns** in properties toolbar.
- **Rename a view** — auto-rewrites `[[basefile#viewname]]` wikilinks across the vault via `MetadataCache.updateInternalLinks` (`BasesView.js:2178-2224`).
- **Drag file-link cells** as wikilinks into the editor.
- **Right-click file-link cells** for standard file context menu.
- **Hover file-link cells** (with modifier) for page-preview popover via `hover-link` event (`source: "bases"`).
- **Embed `.base` files in markdown** as `![[my.base]]` — likely renders the first view inline (handler in `embeds/`, see Open Q6).
- **Bases in sidebar** — tracks active markdown file as `this` context for formulas.
- **Per-`BasesView` undo/redo** of inline edits (separate from editor undo).
- **No source-view fallback** — `.base` files are only editable through the Bases UI.

---

## 10. Extension surfaces exposed

| Surface | Registration verb | Consumer call site | What plugins supply |
|---|---|---|---|
| Operator-function configs | `Workspace.registerOperatorFuncConfigs(id, config)` | `BasesPlugin` (out-of-domain); used by `DK` formula parser | Per-`Value`-type comparison/transform operator descriptor |
| Bases view-type | `BasesPluginInstance.registerView(typeKey, factory)` (verb inferred — not visible inside `bases/`) | `BasesView.js:228,1930,2231,2254` via `plugin.getRegistration`/`getRegistrations` | `{name(), icon, options(viewConfig), create(controller)}` |

Plugin-supplied **`Value` subclasses** have no visible registration API inside `bases/`. The hierarchy appears closed at source-level (no `registerValueType`); adding new types would require patching `ObjectValue.fromFrontMatter`'s lazy evaluator and `BasesQueryResult.applySort` dispatch. **Bases is not plugin-extensible for typed values as of Obsidian 1.12.7.**

Plugin-supplied **`RenderContext` overrides** are not exposed.

---

## 11. Corbomite mapping

Every concept is **Missing** — no `libs/bases/`, no `BasesView` widget, no typed-value layer, no formula engine, no `.base` reader/writer.

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `BasesQuery` (`.base` root) | — | Missing | New `libs/bases/Query`. YAML via `yaml-cpp` or `qyaml-cpp`. **Must preserve key order + unknown keys.** |
| `BasesViewConfig` / `BasesEntry` / `BasesEntryGroup` / `BasesQueryResult` | — | Missing | Standard plumbing. |
| `Value` hierarchy (18 classes) | — | Missing | **Maps cleanly onto `std::variant<NullV, StringV, NumberV, BoolV, DateV, DurationV, FileV, LinkV, TagV, ListV, ObjectV, UrlV, ImageV, HtmlV, IconV, RegexV, MarkdownV, FormulaErrorV>`** plus an `IValue` interface with `renderTo(QWidget*, RenderContext&)`, `equals`, `looseEquals`, `objectAccess`, `keys`, `isTruthy`, `toString`. `std::visit`-dispatch matches JS pattern; alternative is a QObject hierarchy with virtual methods. Variant-of-classes approach is closer to the source. |
| `NullValue.value` singleton | `IValue::null()` | Missing | Singleton distinguished from default-constructed value. |
| `DateValue` (date + time flag) | — | Missing | Wrap `QDateTime` + `bool hasTime`. Parse via `QDateTime::fromString` with two formats. |
| `DurationValue` (7-field calendar duration) | — | Missing | No native Qt equivalent. Custom POD. ISO-8601 + shorthand parser. `addToDate` via `QDate::addYears`/`addMonths`/`addDays`. `humanize()` hand-rolled or via KCalendarSystem. |
| `FileValue` → `NoteMeta*` wrap | — | Missing | 14 `file.*` properties map directly; `file.links/embeds/backlinks/tags` need Corbomite metadata-cache equivalent. |
| `LinkValue` / wikilink resolution | — | Missing | Depends on Corbomite getting Obsidian-style link resolver — currently missing `getFirstLinkpathDest`. |
| `TagValue` hierarchical matching | — | Missing | `#parent` matches `#parent/child`. Custom predicate. |
| `ListValue` aggregates | — | Missing | `sum`/`mean`/`median`/`min`/`max`/`stddev`/`earliest`/`latest`/`unique`/`sort`/`flatten` via `<algorithm>`. |
| `ObjectValue.fromFrontMatter` lazy coercer | — | Missing | YAML-walk auto-coercing strings to `Date`/`Url`/`Link` based on shape. |
| `FilterTree` | — | Missing | `std::variant<AndF, OrF, NotF, RuleF>` with `.test(BasesEntry&)`. Atomic rule depends on formula engine. |
| Formula engine (`DK`/`RK`/`JK`/`PX`) | — | Missing | **Largest sub-task.** Grammar not visible inside `bases/` — lives in adjacent app.js. Recommend separate `libs/formula/` library, reverse-engineer grammar from <https://help.obsidian.md/bases/functions> + `DK` parser audit. Tree-sitter or hand-rolled Pratt parser both reasonable. |
| `BasesView` `TextFileView` subclass | — | Missing | New `Corbomite::Bases::BasesView` widget. **Interacts with Markoff/QGraphicsView pivot** — define `IFileView` abstraction first. |
| Layout-renderer abstract + Table/Cards/List layouts | — | Missing | `IBasesLayout` interface with `QWidget* createWidget(QueryController*)`. |
| Cell renderers (`d$`/`f$`/`m$`) | — | Missing | `IBasesCellRenderer` per cell. Inline-edit (`d$`) needs `metadataTypeManager` widget factory. |
| `metadataTypeManager` widget factory | — | Missing | Per-property-type widget registry. Corbomite's `libs/models/VaultModel` has no equivalent. |
| `RenderContext` (internal-link/external-link/tag click) | — | Missing | Click→openLink, contextmenu→fileMenu, drag→linkText. Same as markdown-preview link rendering — should be a shared primitive. |
| `QueryController` | — | Missing | Active layout, current-file tracking, search, results refresh. **Source not extracted — incremental refresh strategy opaque** (Open Q3). |
| `BasesQueryResult.applySort` per-type comparator | — | Missing | Numeric/date/duration/boolean/lexicographic dispatch with multi-key tie-break. |
| `exportTable` (TSV/Markdown/`obsidian/table` clipboard) | — | Missing | Multi-MIME via `QMimeData::setData`. |
| `+ New` button with filter-satisfying frontmatter | — | Missing | Couples Bases to FileManager's create-with-frontmatter path. |
| `[[basefile#viewname]]` rewrite on view rename | — | Missing | Needs `iterateAllRefs`-equivalent in MetadataCache. |
| Per-view undo/redo | — | Missing | Separate `QUndoStack` per `BasesView`. |
| `registerOperatorFuncConfigs` workspace registry | — | Missing | Workspace-level extension surface for filter operators. |

**Translation:** `std::variant`-based `Value` matches JS's structural polymorphism; parallel `IValue*`-virtual if `std::visit` ergonomics suffer. Formula engine is the single biggest cost — plan ~3-4 weeks before Bases runnable. Total Bases implementation: ~8-10 weeks minimum for a functional MVP.

---

## 12. Markoff gap confirmations / discoveries

Bases is not itself an editor/rendering extension point, but touches the rendering pipeline at:

- **`HTMLValue`/`MarkdownValue` cell renderers** call `Zx(Kx(data))` (markdown-to-DOM) and `sanitizeHTMLToDom(data)` plus `app.fixFileLinks` — same primitives as Markoff preview-mode. **Confirmation:** Bases benefits from any Markoff sanitiser/link-resolver improvement. Corbomite should ensure these reusable from non-MarkdownView contexts.
- **`ImageValue.renderTo`** uses `app.vault.getResourcePath(file)` (`app://local/...?<mtime>` URL). Same convention as `MarkdownRenderer` image embeds. **Cache-busting `?<mtime>` query string is an invariant.**
- **`RenderContext.renderFileLink`** mirrors `MarkdownPreviewRenderer`'s wikilink behaviour — `<span class="internal-link">` with `data-href`, `is-unresolved` toggle, click/contextmenu/drag/hover-link bindings. **Discovery:** this rendering is duplicated from the markdown renderer. Corbomite should expose a shared "render internal link in non-editor context" primitive — useful for any non-markdown view (Bases, Backlinks, Outline).
- **Inferred:** `![[my.base]]` embed in markdown dispatches through the embed registry (`embeds/EmbedRegistry`, out of this audit). Not a new Markoff gap but worth noting.

Append to `01-markoff-gaps.md` under `## Pass 2 additions — bases`: `RenderContext.renderFileLink/renderExternalLink/renderTag` plus `app.fixFileLinks` should be Markoff-library primitives, not Bases-private.

---

## 13. Open questions

1. **Filter / formula expression grammar.** The atomic rule is a string parsed by `DK` (outside `bases/`). Cannot enumerate operators (`==`, `contains`, `matches`, `in`, …), functions (`if`, `sum`, `count`, `link`, `date`, …), or precedence from inside this domain. Pass 3 must extract `DK`/`JK`/`PX`/`RK` from adjacent app.js. Help page <https://help.obsidian.md/bases/functions> is the user-facing reference.
2. **`QueryController` source.** Referenced by `new QueryController(app, plugin, contentEl, currentFile)` at `BasesView.js:181` but not declared inside audited files. Incremental refresh keystone. Pass 3 should grep `app.js` for `QueryController = (function` and produce a sibling `bases-controller.md` if large.
3. **Incremental refresh strategy.** When frontmatter changes (`MetadataCache.changed`), is the query re-run via full rescan, diff update, or per-entry invalidation? Mechanism is in `QueryController` (Q2). Likely debounced full re-eval, unconfirmed.
4. **Live-reload on `.base` external modify.** `BasesView.onModify` (`:121-123`) calls `loadFileInternal` only if `!saving`. `isPlaintext = false` short-circuits 3-way merge (`TextFileView.loadFileInternal:140-156`). Does Bases silently discard unsaved local edits? Re-confirm.
5. **`BasesPlugin` API.** Internal plugin lives at `obsidian/plugin/internal-plugins/bases/` (not extracted). Does it expose `data.json` settings? Are there `bases:`-prefixed commands? Plugin-extensible view-types?
6. **`.base` embed handler.** How does `![[Foo.base]]` render inside markdown? Likely first-view read-only inline. Handler in `embeds/EmbedRegistry` (Wave 2 `rendering` audit).
7. **Plugin-defined `Value` types.** Confirmed-not-supported by this audit; worth a Pass 3 check of community plugins for `Value.prototype` monkey-patching.
8. **`RegExpValue` source.** No `ObjectValue.fromFrontMatter` path produces one. Presumably formula-only via a `regex(...)` builtin. Confirm with formula audit.
9. **`Markdown` value type** (unnamed second subclass at `TagValue.js:776-789`). Produced by which formula function? Likely `markdown(...)` / `richtext(...)`.
10. **Layout-registration verb.** `plugin.getRegistration(type)` is read; `plugin.registerView(type, factory)` is inferred. Where does `BasesPlugin` register the built-in `"table"`? Almost certainly `internal-plugins/bases/main.js`.

---

## 14. Recommended Pass 3 synthesis input

1. **Promote the `.base` schema (Section 3) into `VAULT-FORMAT.md` as a top-level subsection** alongside `.canvas` and `.obsidian/workspace.json`. Corbomite hard-compatibility target. Include the filter-grammar example, the PropertyId grammar (`note.x` / `file.x` / `formula.x`), the `unrecognizedData` round-trip rule, and the empty-file → default-table rule.
2. **Promote the `Value` hierarchy (§1, §11) into `GAP-ANALYSIS.md` as the largest single missing-feature item**, with `std::variant`-based C++ translation and a separate formula-engine sub-project. Estimate: ~3-4 weeks formula engine, ~2 weeks Value hierarchy + Query/Entry/Config plumbing, ~3 weeks Qt table widget + inline-edit + `metadataTypeManager` integration = ~8-10 weeks for functional MVP.
3. **Flag the `QueryController` source-extraction gap** — incremental refresh contract between `BasesQuery` and `MetadataCache` events is undocumented inside this domain. Pass 3 should grep broader `app.js` for `QueryController = (function` and produce `bases-controller.md` before Corbomite implementation begins; otherwise risk naive full-rescan on every `changed` event.

---

## 15. Cross-domain references

| Other domain | Reference type | Brief description |
|---|---|---|
| `views` | parent | `BasesView` (`HX`) extends `TextFileView`. First 165 lines of `BasesView.js` are bleed-over from `views/` and audited there. |
| `views` | registration | `ViewRegistry.registerViewWithExtensions(["base"], "bases", factory)` — registered by `BasesPlugin`, not `bases/` (see `views.md:256`). |
| `vault` | consumer | `vault.modify` (save), `vault.cachedRead`/`read` (load), `vault.getResourcePath` (images), `vault.getFileByPath`/`getAvailablePath`/`create` (+ New button). `FileManager.processFrontMatter` (inline-edit write), `FileManager.getNewFileParent`, `FileManager.renameProperty`, `FileManager.storeTextFileBackup` (on save error). |
| `metadata` | consumer | `MetadataCache.getFileCache(file).frontmatter` (every `BasesEntry.frontmatter`). `iterateRefsForFile`, `resolvedLinks`, `iterateAllRefs`, `getFirstLinkpathDest`, `updateInternalLinks` (view-rename rewrite). `MetadataTypeManager.getTypeInfo`/`getAllProperties`/`getPropertyInfo`/`setType`/`registeredTypeWidgets` (inline-edit cell renderer). **Incremental refresh** via `MetadataCache.changed`/`resolved` consumed by `QueryController` (out-of-domain). |
| `parsing` | consumer | `parseYaml`/`stringifyYaml` (`.base` round-trip). `parseFrontMatterTags`, `getLinkpath`, `parseLinktext`, `stripHeading`, `stripHeadingForLink` (view-rename). |
| `workspace` | consumer + emitter | Listens: `on('file-open')`, `on('layout-change')`. Emits: `trigger('hover-link', {source: "bases"})` (`Value.js:90-99`). Calls: `openLinkText`, `handleLinkContextMenu`, `handleExternalLinkContextMenu`, `requestSaveLayout`, `isInSidebar`, `getActiveFile`, `getActiveViewOfType`. **Consumer** of `Workspace.operatorFuncConfigs` registry (`workspace.md:369-371`). |
| `core` | consumer | `App` pervasive (`app.workspace`, `app.metadataCache`, `app.vault`, `app.fileManager`, `app.dragManager`, `app.keymap`, `app.metadataTypeManager`, `app.internalPlugins`, `app.renderContext`). `Scope` for keyboard. `Keymap.isModEvent`. |
| `editor` | consumer | `EditorView` (CodeMirror) for toolbar formula editor (`UX`). `Compartment`, `placeholder`, `tooltips` extensions. |
| `editor/markdown` / `rendering` | sibling | `.base` embed handler in markdown is out-of-domain (Open Q6). `sanitizeHTMLToDom`, `Zx`/`Kx` (markdown→DOM), `app.fixFileLinks`, `getIcon`/`setIcon`/`setTooltip`/`displayTooltip`. |
| `ui/menu` | consumer | `Menu.forEvent(e).addSections(...).addItem(...)` in cell contextmenu handlers. |
| `ui/components` | consumer | `SearchComponent`, `TextComponent`, `DropdownComponent`, `SliderComponent`, `ToggleComponent`, `MI` (combobox), `XI` (multitext), `$k` (number stepper), `ab` (chooser). |
| `ui/popups` | consumer | `Notice`, `S$.create` (hover-popover for new-note editing). |
| `utils` | consumer | Many — `debounce`, `lc` (array-move), `qV` (case-insensitive key), `ub` (locale compare), `pb` (numeric compare), `Qc`/`Yc` (URL validators), `YC` (wikilink predicate), `HV`/`zV` (date formatters), `xW` (tag normaliser), `FX` (3-way merge, bypassed by Bases), many more. |
| `platform` | consumer | `Platform.isPhone` (mobile branching), `Platform.isDesktopApp` + `Platform.resourcePathPrefix` (image URL). |
| `i18n` | consumer | `gm.plugins.bases`, `gm.interface`, `gm.nouns`, `gm.dialogue`. |
| `plugin/internal-plugins/bases` | parent (out-of-domain) | `BasesPlugin` registers `base`-extension binding, view-type registry, operator-func configs. **Not in this audit dump.** |

### Short symbols from other domains referenced here

| Short symbol | Defined in | Used for |
|---|---|---|
| `parsePropertyId` / `nY` / `xY` / `SY` | utils-adjacent | PropertyId parse/build/case-fold/serialise |
| `OY` / `LY` | utils-adjacent | Property-type label / property-id → widget-key |
| `kY` | utils-adjacent | "Is this property groupable?" predicate |
| `PY` | utils-adjacent | Unique-name suffixer (e.g. "(2)", "(3)") |
| `MY` / `TY` / `FY` | utils-adjacent | Filter-parser / view-config-builder / new-item-defaults computer |
| `DK` / `RK` / `JK` / `PX` / `IY` | `editor/`-adjacent | Formula container / error / language-support / extension wrapper / default extensions |
| `ZK` | `editor/`-adjacent | Aggregate-formula context (`new ZK(app, values)`) |
| `eY` / `gm.plugins.bases` | i18n | Bases i18n string bundle |
| `qV` / `vc` / `ac` | utils | Case-insensitive key / array shape predicate / dedupe |
| `Qc` / `Yc` / `YC` | utils | URL validator / URL safety / wikilink predicate |
| `nE` / `hu` / `ou` | utils | Linktext→display / path→shortname / filename-safe-fy |
| `HV` / `zV` | utils (date) | `YYYY-MM-DD` / `HH:MM:SS` formatters |
| `zW` | utils (`bases/`-adjacent) | Generic JS-value → `Value`-subclass coercer |
| `xW` | utils | Tag normaliser |
| `FX` | utils | 3-way text merge (bypassed by `isPlaintext = false`) |
| `Mb` | utils | Adapter-promise barrier |
| `sanitizeHTMLToDom`, `Zx`, `Kx` | rendering | HTML sanitiser / markdown-to-DOM pipeline |
