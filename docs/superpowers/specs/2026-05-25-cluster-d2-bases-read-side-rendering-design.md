# Cluster D.2 — Bases read-side rendering: groups, sort, rich cells

**Date:** 2026-05-25
**Cluster:** D (Bases UI completion) — sub-project **D.2**, read-side rendering.
**Status:** Design approved; plan to follow.
**Substrate:** Independent of the Markoff foundation rewrite (Bases is `QTableView`/`QTreeView`-based), except Markdown cells (deferred — frozen on the read-only-Live renderer).
**Builds on:** D.1 (backend correctness), shipped 2026-05-25.

## Context

Cluster K shipped the Bases runtime + a flat read-side view: `BasesView` hosts a `QTableView` over `BasesTableModel` (`QAbstractTableModel`, `rowCount = result()->rows()`) with `BasesCellDelegate`. The query layer already computes more than the view shows: `BasesQueryResult::groups()` returns partitioned, null-last `BasesEntryGroup`s with per-group `summaryValue()`, and `applySort()` already does multi-key, type-dispatched sorting. The view ignores all of it — groups are invisible, sort isn't interactively buildable, and rich cell types fall back to plain text. D.2 surfaces the configured/computed result in the view.

**Obsidian baseline (verified against source** at `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/bases/` **and `docs/obsidian-audit/domains/bases.md`):**
- Grouping: "collapsible group headings + optional summary cells" (`createGroupHeadingEl`, `collapse-icon is-collapsed`, `getSummaryValue`) — domain doc §9 #394.
- Sort: "Sort by clicking column headers (multi-key, cycle ASC→DESC→unsorted)" — §9 #393. (The richer toolbar **Sort+group menu** — `sortDirectionCombobox` etc. — is a separate, larger config surface → **D.3**.)
- Cells: per-`Value` `renderTo(el, renderContext)` (e.g. `HTMLValue.renderTo` → `ctx.renderTag`). Corbomite's single `BasesCellDelegate` is the idiomatic-Qt equivalent. Inline editing (`MetadataPropertyRenderer`/`d$`, writes frontmatter) is a distinct path → **D.3**.

## Goal

Open a grouped `.base` and see collapsible group headings with row counts and summary cells; click column headers to build a multi-key sort; and see Icon/Image/HTML cells rendered richly. All read-side: D.2 makes **no `.base` writes** and no inline edits.

## Scope

**In scope:**
1. View/model restructuring: flat table → 2-level tree (`QTreeView` + `QAbstractItemModel`).
2. Collapsible group rendering: heading rows (label + count + chevron), null-key group last, summary-cell display.
3. Rich read-only cells: Icon, Image, HTML.
4. Header-click multi-key sort: cycle ASC→DESC→unsorted, Shift-click for secondary keys, header indicators.

**Out of scope (→ D.3 or frozen):**
- Toolbar Sort+group / Properties / Views / Results menus; inline cell editing; summary-formula *configuration*; the "+ New" button.
- Markdown cells (frozen on the read-only-Live renderer — plain-text fallback for now).
- **Persistence:** collapse state and header-sort are session-local in D.2 (see Design §2, §4); persisting them (leaf ephemeral state / `.base` write) is deferred.

## Design

### 1. View & model restructuring

Replace `BasesView`'s `QTableView` with a `QTreeView`, and `BasesTableModel` (`QAbstractTableModel`) with a 2-level tree model (`QAbstractItemModel` — name it `BasesTreeModel`; `BasesTableModel` is used only by `BasesView`, so this is a contained swap).

Tree shape, driven by `BasesQueryResult::groups()`:
- **Grouped** (`config.groupBy` set): root → one **group node** per `BasesEntryGroup` → **entry leaves**. The null-key group (`!group.hasKey()`) renders **last** (the result already orders it last).
- **Ungrouped** (no `groupBy` → a single keyless group): entries hang **directly off the root** as a flat list — no heading node. (Implementation: when the only group is keyless, collapse the group level so `index(row, …, root)` maps straight to entries.)

Model interface (standard 2-level tree):
- `columnCount` = visible-property count (same at all levels; `propertyAt(column)` retained).
- `rowCount(parent)`: root → group count (grouped) or entry count (ungrouped); group node → its entry count; entry leaf → 0.
- `index`/`parent`: encode node kind via `internalId` (e.g. group nodes carry their group index; entry leaves carry an encoded group+row, with the parent being that group). Entry parent = its group; group parent = invalid.
- `data(index, role)`:
  - **Entry** rows — today's `BasesTableModel::data` logic verbatim (per-cell `getValue(propertyAt(col))`, error tint, etc.).
  - **Group** rows — col 0: group label (`key->toString()` or a `(no value)` constant for the null-key group) + entry count; other cols: `result()->summaryValue(groupIndex, propertyAt(col), fnName)` **iff** that property has a summary formula configured in the active view, else empty. A custom role (e.g. `IsGroupRowRole`) lets the delegate distinguish group rows.

Existing header wiring (`sectionClicked` sort, `sectionMoved` reorder) and edit-triggers transfer unchanged — `QTreeView::header()` is a `QHeaderView`.

### 2. Group heading rendering

QTreeView draws expand/collapse via the branch indicator on col 0; no manual collapse machinery. A group-heading delegate path (in `BasesCellDelegate`, gated on `IsGroupRowRole`) styles the parent row: bold label, `(N)` count, summary values aligned under their columns. **Collapse state is in-memory for D.2** — groups start expanded each open; persisting collapsed group keys (leaf ephemeral state) is a deferred follow-up.

### 3. Rich read-only cells

Extend `BasesCellDelegate`'s existing `type()` dispatch (Boolean/Number/Date/Error) with:
- **Icon** (`type() == "Icon"`): resolve via the Lucide icon registry / `QIcon::fromTheme`, paint centered.
- **Image** (`type() == "Image"`): resolve the referenced vault file to a `QPixmap`, paint scaled-to-fit (with a sensible row-height cap); fall back to the path text if unresolved.
- **HTML** (`type() == "HTML"`): render via a `QTextDocument::setHtml` painted into the cell rect (no Markoff dependency).
- **Markdown** (`type() == "Markdown"`): **plain-text fallback** (`toString()`) — full rendering deferred to the read-only-Live renderer.

Exact Icon/Image `renderTo` semantics resolved against the canonical source chunk during implementation (the per-file de-min duplicates obscure them).

### 4. Header-click multi-key sort

On `header()->sectionClicked(logicalIndex)`:
- Resolve `propertyAt(logicalIndex)`. If it's not currently in `m_cfg.sort`, append it ASC. If it's the most-recent/only key, cycle ASC→DESC→remove. Shift-click **appends** a new key (secondary, tertiary…) rather than replacing, enabling multi-key build-up.
- Re-run the query result's sort (`applySort` consumes `m_cfg.sort`) and refresh the model. **Session-local** — D.2 does not write `m_cfg.sort` back to the `.base`. Persistent sort config is D.3's toolbar menu.
- Indicators: a header section delegate / `QHeaderView` paint hook draws the ASC/DESC arrow plus a small priority index (1, 2, …) on each sorted column. (Native `QHeaderView` shows only one indicator; multi-key needs custom header painting.)

Exact cycle semantics (does a third click remove the key or keep DESC; how Shift interacts) pinned against the Obsidian source during implementation.

### 5. Testing

The tree model is the testable core:
- `rowCount`/`index`/`parent` for **grouped**, **ungrouped**, and **null-key-group-present** cases (incl. round-trip `parent(index(...)) == expected`).
- `data`: entry-row cell values match the old flat model; group-row col 0 = label + count; group-row summary cells present only when configured; `IsGroupRowRole` set on group rows only.
- Sort handler: clicking a header mutates `m_cfg.sort` per the cycle (append ASC → DESC → remove; Shift appends a key); verified at the handler/controller level.
- Delegate: unit-test the value→display dispatch helper (`displayText()`-style) for the new types; painting itself is GUI and not pixel-tested.

Use the existing `libs/bases/tests` harness; new `tst_bases_tree_model` (or extend the model test) + sort-handler cases. All existing bases tests stay green.

## Definition of done

- A grouped `.base` renders collapsible group headings with counts + summary cells; ungrouped renders flat (no headings); null-key group renders last.
- Icon/Image/HTML cells render richly; Markdown falls back to text.
- Header clicks build/cycle a multi-key sort with visible indicators; the view re-sorts; no `.base` write occurs.
- New model/sort unit tests pass; full `libs/bases` suite green.
- No inline edits, no toolbar menus, no `.base` writes (those are D.3).

## References

- [obsidian-audit/domains/bases.md](../../obsidian-audit/domains/bases.md) — §1 (`BasesEntryGroup`, `BasesQueryResult`, Cell renderers `BasesView.js:2991–3166`, Toolbar family), §9 (#393 sort, #394 grouping), §11 (Corbomite mapping).
- Source (verified): `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/bases/` — `BasesView.js` (`createGroupHeadingEl`, `is-collapsed`, `getSummaryValue`, sort menu), per-`Value` `renderTo`.
- Cluster D stub: [plans/2026-04-26-cluster-d-bases-ui-completion.md](../plans/2026-04-26-cluster-d-bases-ui-completion.md). D.1 spec: [specs/2026-05-25-cluster-d1-bases-backend-correctness-design.md](2026-05-25-cluster-d1-bases-backend-correctness-design.md).

## Current-state anchors (verified 2026-05-25)

- `BasesView` (QTableView host, header wiring): `libs/bases/src/BasesView.cpp` (`m_table` :46, `sectionClicked` :58, `sectionMoved` :60, model build :147).
- `BasesTableModel` (flat): `libs/bases/src/BasesTableModel.cpp` (`rowCount` → `result()->rows()` :39).
- `BasesQueryResult::groups()` / `summaryValue()` / `applySort()`: `libs/bases/src/BasesQueryResult.cpp` (:96, :73). `BasesEntryGroup` in `include/corbomite/bases/BasesQueryResult.h:19`.
- `BasesCellDelegate` type dispatch: `libs/bases/src/BasesCellDelegate.cpp` (display helper :29, `paint` :111).
