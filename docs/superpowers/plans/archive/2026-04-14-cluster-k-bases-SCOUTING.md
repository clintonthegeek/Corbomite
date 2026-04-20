# Cluster K — Bases (SCOUTING)

> **Living-status note:** This is a *scouting document*, not a plan. It captures prior-art breadcrumbs + open architectural questions so a full plan can be written efficiently when Cluster K is unblocked. Live status is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md). When expanded to a full plan, rename to drop `-SCOUTING` suffix and update `INDEX.md`.

**Scouting written:** 2026-04-14.
**Blocker status:** **Resolved 2026-04-17.** DSL extraction landed as [`docs/obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md`](../../obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md) (15 sections: parser architecture, EBNF, precedence, type-aware operator semantics, evaluation context, Value hierarchy, string-escape rules, full function catalog, summary formulas, filter structure, identifier resolution, error surfaces, plugin surface, observed impl-vs-doc divergence, implementation options for this cluster). All gating conditions (Cluster A/I/J in flight or done) are also satisfied — A, I, J are all closed. **Ready for plan expansion.**
**Expand to full plan when:** *(was: the Bases formula/filter DSL is extracted — now satisfied.)* Expand now; the phasing sketched in §"Rough phasing" below is a starting point, and §15 of the DSL addendum provides the parser/evaluator implementation-choice analysis that this scouting doc's §"Key architectural questions" #1 asked for.

**Covers (deferred to full plan):** P2.13 (Bases), P3.12 (OperatorFuncConfigs plugin registry).

## Why deferred

Bases' implementation pivots around a DSL Obsidian uses for filters and formulas — the grammar for `tag:#foo AND priority > 3` style expressions, the formula syntax for computed columns (`note.modified > date("2024-01-01")`). The Pass 2 audit (`domains/bases.md`) established that the parser (`DK`) lives *outside* the audited `bases/` directory. Writing K's plan without that DSL means the Formula Engine phase is a black box; every design decision is speculative.

The audit estimated 8–10 weeks for a Bases MVP. That's too much scope to plan poorly. **Wait for the DSL extraction.**

## Audit input roster (when expanding)

- `domains/bases.md` §1 (entire section) — 18-class Value hierarchy (Value, BooleanValue, DateValue, DurationValue, FileValue, HTMLValue, IconValue, ImageValue, LinkValue, ListValue, NotNullValue, NullValue, NumberValue, ObjectValue, PrimitiveValue, RegExpValue, RelativeDateValue, StringValue, TagValue, UrlValue). Closed hierarchy; plugins cannot extend.
- `domains/bases.md` §3 — `.base` YAML schema. Root object with optional `views`/`filters`/`properties`/`formulas`/`summaries`/`newItemFolder`/`newItemTemplate`. No version field. Forward-compat via `unrecognizedData` round-trip. Empty file → default 1-view "Table".
- `domains/bases.md` §1 — `BasesEntry` (one vault note projected as `{local, note, file, properties}`), `BasesQueryResult` (materialised query result), `BasesView` (TextFileView subclass), `BasesViewConfig` (in-memory mirror of the `.base` file).
- `domains/bases.md` §11 — everything in Corbomite is Missing. New `libs/bases/` library + Qt table widget + inline-edit delegate.
- `VAULT-FORMAT.md` §6 — canonical `.base` file format consolidation.
- `GAP-ANALYSIS.md` §Cluster K.

## Controller-side follow-up (blocking) — ~~BLOCKING~~ **RESOLVED 2026-04-17**

~~Must happen before the full plan is written:~~ **Done.**

~~**Extract `DK`/`RK`/`JK`/`PX` Bases formula/filter DSL parser.**~~ Landed as [`docs/obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md`](../../obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md). Key findings that reshape planning:

1. The parser is **Lezer** (CodeMirror 6's parser generator), not hand-rolled — the Bases DSL is a small JS-style expression language with LR grammar + node-walker AST + typed interpreter (`NK` subclass hierarchy).
2. Filter and formula grammars are **the same language**; filter-objects (`and`/`or`/`not`) compose outside the DSL in `BasesViewConfig`.
3. Closed 16-class `Value` hierarchy with `static type` strings; plugins cannot add `Value` subclasses or operators — only functions via `registerGlobalFunc` / `registerInstanceFunc`.
4. ~50 built-in functions catalogued (14 global + per-type methods on String/Number/Date/List/Object/Regexp/Link/File + 15 default summary formulas expressed *as* formulas over an implicit `values` list).
5. Several special-cases hard-coded in the call-evaluator (`if`, `list.map/filter/reduce`, `object.map/filter`) that bypass the function registry.

Three implementation options for the C++ port are sketched in §15 of the addendum (tree-sitter grammar port / hand-rolled Pratt parser / full Lezer state-machine transliteration). **Plan-expansion task should pick one during phase 3 design.**

## Prior-art breadcrumbs (local paths — do NOT clone)

The prior-art story for Bases is thinner than for A–H because the Value hierarchy is bespoke and the DSL is proprietary. Only the *rendering* side has KDE analogues.

| Target | Local path | Note |
|---|---|---|
| **Qt table widget with per-cell custom rendering** | Qt6 native: `QTableView` + `QStyledItemDelegate` | No KDE-specific widget needed. The Value subclasses' `renderTo(el, ctx)` maps onto delegate `paint()` |
| **Inline-edit delegate for typed cells** | `~/src/kde/src/kdevelop/kdevplatform/` — grep for `QStyledItemDelegate` subclasses; `~/src/kde/src/kate/` for editable table examples | Per-type editor widgets (text, date, bool, list) are well-known Qt patterns |
| **YAML parsing** | Already solved by `libs/markoff-parser` + yaml-cpp (per recent commit `f318b7a`) | Cluster A's `FrontMatter` handles this. Reuse directly |
| **Date / duration value formatting** | Qt6 native: `QDate`, `QDateTime`, `QDateEdit` | Plus Moment-compat layer from Cluster F for format tokens |
| **Typed variant store** | C++20 native: `std::variant<StringValue, NumberValue, ...>` with virtual dispatch via `std::visit` | Direct port of Obsidian's hierarchy to a variant-of-subclasses |
| **Incremental-refresh against MetadataCache** | Cluster I's 5-event MetadataCache ordering | Subscribe to `linksResolved`, recompute affected rows |
| **Search/filter grammar parser** | `~/src/kde/src/baloo/src/lib/term.cpp` — Baloo's term parser for its file metadata search | Closest KDE analogue for a structured query DSL; may inform parser design once the Bases DSL is extracted |
| **Spreadsheet-like column management** | External — consider `QtSpreadsheet` (GPL) or KDE Calligra Sheets (`~/src/kde/src/` if cloned — verify) | Lower-priority |

## Key architectural questions to resolve during planning

1. **`std::variant` vs `std::unique_ptr<Value>` for the Value hierarchy?** Variant is cache-friendly and avoids heap; unique_ptr mirrors Obsidian's JS class hierarchy and supports virtual dispatch cleanly. Lean variant for performance, but the 18-type discriminant + `std::visit` for every operation may get verbose. Benchmark once DSL arrives.
2. **Does `QueryController` (incremental-refresh) warrant its own class, or roll into `BasesView`?** Obsidian separates them. Likely mirror for clarity.
3. **Inline edit through the table: does it write back to the source note's frontmatter?** Yes per audit. Requires Cluster A's `FrontMatterWriter::process` to be available. Plan Phase 1 should include that explicit dependency.
4. **Rendering `FileValue` cells — embed a `Markoff::LinkRenderer` (Cluster J) or build a mini-renderer?** Pass 3 synthesis flagged `RenderContext` as the shared `Markoff::LinkRenderer` extraction target. Reuse when J's extraction lands; in the interim, temporary ad-hoc `QLabel` rendering.
5. **Filter predicates that don't translate to SQLite FTS5 (regex, formula expressions) — post-filter over the full result set, or pre-compute a row-ID set?** Cluster D Phase 4 investigates this for search; Bases reuses the same architecture.
6. **Closed Value hierarchy: does Corbomite preserve the closure for compat, or open it for extensibility?** Leans toward preservation (don't surprise plugin authors who ported from Obsidian), with a deliberate "Corbomite extension" escape hatch later.

## Rough phasing (for planning, not prescriptive)

- Phase 1: `Value` hierarchy (18 classes, maybe as `std::variant`). Unit tests per type.
- Phase 2: `.base` YAML schema parse/write. `BasesViewConfig` in-memory mirror.
- Phase 3: Formula/filter DSL parser (**blocked on DSL extraction**). Lexer + recursive-descent parser.
- Phase 4: `BasesEntry` + `BasesQueryResult` + incremental refresh via MetadataCache.
- Phase 5: `BasesView` table widget (`QTableView` + delegate). Non-editing render.
- Phase 6: Inline edit — delegate-driven per-cell editors writing back via `FrontMatterWriter`.
- Phase 7: Grouping, sorting, filter-UI (interactive filter builder).
- Phase 8: Multiple view types (table, cards, list) within one `.base` file.
- Phase 9: `OperatorFuncConfigs` plugin registry.

## When to expand

Trigger: **a Bases DSL extraction addendum lands at `docs/obsidian-audit/addenda/`** (referenced from `00-taxonomy.md`). Ritual 3 Step 5 flags the stub for expansion.

Expansion effort: ~3–4 hours once the DSL is in hand. The phase structure is mostly already here; the formula-engine phases need concrete grammar citations and target-class signatures.
