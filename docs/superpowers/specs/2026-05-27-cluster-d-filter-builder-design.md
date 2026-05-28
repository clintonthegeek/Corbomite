# Cluster D — Filter Builder (design)

**Date:** 2026-05-27
**Cluster:** D (Bases UI completion). Follows the formula editor (shipped 2026-05-27). After this, only D.5 (plugin API) remains in D.
**Status:** Approved 2026-05-27.

## Goal

A UI to build and edit Bases filters — the And/Or/Not predicate trees — instead of
hand-editing `.base` YAML, covering both filter scopes:

- **Global** — `BasesQuery::filters` (`FilterPtr`), applied to every view.
- **Per-view** — `BasesViewConfig::filters` (`FilterPtr`), AND-merged on top of the global
  filter (addendum §10).

The `FilterTree` backend is **already complete**: `FilterNode` (abstract), `FilterRule`
(leaf wrapping a `Formula` predicate), `FilterConjunction` (`Conj::And/Or/Not` over a
`QVector<FilterPtr>`), `FilterNode::serialize()` → YAML shape, `FilterConjunction::optimize()`
(collapses a single-child And/Or into its child; leaves Not + multi-child as-is), and
`parseFilter(YamlValue)`. This sub-project is the **UI layer**, reusing `FormulaInput`
(the validated + autocompleting expression widget from the formula editor) for leaf
predicates.

Filter DSL reference:
[`docs/obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md`](../../obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md)
§10 (filter structure: a bare predicate string, or a one-key `and`/`or`/`not` map whose
value is a list of child filters; `not` = `!(c0 && c1 && …)`; global and per-view
concatenate with AND).

## Decisions (from brainstorming)

1. **Leaf editor:** raw DSL predicate per leaf, each edited with a `FormulaInput`
   (validated + autocompleting). Nested via And/Or/Not group rows. (Structured
   property/op/value rows deferred; the leaf widget stays factored so they could be added
   later.)
2. **Scope:** edit **both** global and per-view filters, via a scope toggle in one dialog.
3. **Surface:** a **modal `QDialog`** launched from a new "Filters" `QToolButton` in the
   `BasesView` toolbar (consistent with the formula-editor dialogs; handles a tall nested
   tree + the scope toggle better than a `Qt::Popup`).

## Architecture — mutable spec + pure converters

`FilterNode`/`FilterRule`/`FilterConjunction` are **immutable** (const accessors, no
setters). Rather than add mutators, introduce a small **mutable value tree** the UI edits,
plus pure converters bridging it to the immutable backend tree. All tree logic lives in
this pure unit; the widgets only render/mutate a `FilterSpec`.

### `FilterSpec` (new pure type, `libs/bases`)

A copyable value node — either a leaf or a group:

```cpp
struct FilterSpec {
    enum class Kind { Leaf, Group };
    Kind kind = Kind::Group;
    QString expression;            // Leaf: the predicate source
    Conj conj = Conj::And;         // Group: And/Or/Not (reuses FilterTree's Conj enum)
    QVector<FilterSpec> children;  // Group: ordered children
};
```

### Pure converters

```cpp
FilterSpec fromFilter(const FilterPtr &node);   // backend tree -> editable spec
FilterPtr  toFilter(const FilterSpec &spec);    // editable spec -> backend tree
```

- **`fromFilter(nullptr)`** → an empty And-group (`{Group, And, {}}`) — the dialog always
  shows a top-level group.
- **`fromFilter(FilterRule)`** → an And-group wrapping one leaf (`{Group, And, [leaf(src)]}`),
  so a bare top-level predicate is still presented as "All of: [one rule]".
- **`fromFilter(FilterConjunction)`** → `{Group, conj, [fromFilter(child)…]}`.
- **`toFilter`**:
  - **Leaf** → `FilterRule(Formula(expression))`.
  - **Group** → build a `FilterConjunction(conj, [toFilter(child)…])` after **dropping
    blank-text leaf children** (a just-added, unfilled rule must not error or serialize),
    then return `FilterConjunction::optimize()` (collapses a single-child And/Or to the
    bare child → `status == "done"` round-trips as a bare string, not `and: [string]`;
    Not and multi-child preserved).
  - **A group with no (surviving) children** → `nullptr` (no filter at that scope; avoids
    emitting an empty `and: []`).

Round-trip is faithful modulo the bare-rule ↔ single-child-And normalization, which is
semantically identical and already how `optimize()` behaves.

## Widgets

### `FilterBuilderWidget` (new, `libs/bases`)

Recursive editor over one `FilterSpec` **group**.

- **Header row:** a **Conj `QComboBox`** (`All`→And, `Any`→Or, `None`→Not) + an "[+ rule]"
  button and an "[+ group]" button.
- **Child rows**, in `children` order:
  - **leaf** → a `FormulaInput` (candidates injected) + a delete (✕) `QToolButton`.
  - **group** → a nested `FilterBuilderWidget` (visually indented) + a delete button.
- On any edit (conj change, leaf text change, add rule/group, delete), it rebuilds its
  `FilterSpec` from the live widget state and emits `changed()`.
- API: `FilterSpec spec() const`; `void setSpec(const FilterSpec &group, const QStringList &candidates)`
  (rebuilds the row widgets); `bool isValid() const` + `validityChanged(bool)` — aggregates
  the leaves' `FormulaInput::isExpressionValid()` (empty leaves are neutral/valid and get
  dropped by `toFilter`; only a **non-empty unparseable** expression is invalid). A nested
  group is valid iff all its descendants are.
- Children owned via normal Qt parenting; fully torn down + rebuilt on `setSpec`.

### `FilterBuilderDialog` (new, `libs/bases`)

A resizable `QDialog`.

- A **scope control** (a `QComboBox` or segmented buttons: "This view" / "All views
  (global)") over a `QStackedWidget` holding **two `FilterBuilderWidget`s** — one seeded
  from the per-view spec, one from the global spec — so toggling preserves each scope's
  in-progress edits.
- A `QDialogButtonBox(Ok|Cancel)`. **OK disabled** while *either* builder reports invalid.
- API: `void setScopes(const FilterSpec &globalSpec, const FilterSpec &perViewSpec, const QStringList &candidates)`;
  `FilterSpec globalSpec() const`; `FilterSpec perViewSpec() const`.

## `BasesView` wiring

- A new `m_filtersBtn` `QToolButton` in the toolbar (alongside Properties/Sort/Views).
- Clicked → construct a `FilterBuilderDialog`, `setScopes(fromFilter(m_query->filters),
  fromFilter(m_activeView->filters), formulaCandidateList())`, `exec()`.
- On `Accepted`: `m_query->filters = toFilter(dlg.globalSpec())`;
  `m_activeView->filters = toFilter(dlg.perViewSpec())`; then `onConfigMutated()` (recompute
  + `requestSave()`) — the same chokepoint the formula dialogs use.
- Reuses the existing `formulaCandidateList()` helper (filters resolve the same identifiers
  as formulas; `FormulaCandidates::Mode::NamedFormula`).

## Testing

- **Pure / TDD (`tst_filter_spec`):** `fromFilter`/`toFilter` round-trips — bare rule,
  nested and/or/not, `nullptr`→empty-group, empty-group→`nullptr`, blank-leaf drop,
  single-child-And collapse, Not-with-one-child preserved, Conj↔label mapping.
- **Offscreen widget (`tst_filter_builder_widget`):** add-rule/add-group/delete rebuilds the
  spec; validity aggregation (one invalid leaf → group invalid); nested group spec round-trip.
- **Offscreen widget (`tst_filter_builder_dialog`):** OK gating on either-scope invalidity;
  scope toggle preserves both specs; `globalSpec()`/`perViewSpec()` accessors return edits.
- **Integration (`tst_bases_view_wiring`):** opening the dialog and applying both scopes
  writes `m_query->filters` + `m_activeView->filters` and round-trips through
  `getViewData()`/`setViewData()`.
- **Pending user eyeball** (offscreen Qt can't render the nested tree / drive a modal): the
  builder tree layout and dialog rendering — joins the D.2–D.4c + formula-editor backlog.

## Definition of done

- Global and per-view filters can be built/edited from the toolbar Filters button as nested
  And/Or/Not trees of raw predicates; the `.base` round-trips them (existing serializer).
- Leaf predicates validate live and autocomplete; invalid expressions block OK.
- All bases tests green; clean build; offscreen launch clean.
- Closeout written to `decisions-archive.md`; PROJECT-STATE + INDEX updated; interactive
  verification noted as pending user eyeball.

## Out of scope (deferred)

- Structured property/operator/value leaf rows (leaf widget stays factored for a later add).
- Drag-reorder of filter rows/groups.
- The full-text search bar (already a separate `BasesView` feature).
- D.5 (plugin API) — the remaining D sub-project.
