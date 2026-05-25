# Cluster D.3 — Bases Toolbar Menus, Properties Drawer & Inline-Edit Polish — Design

**Date:** 2026-05-25
**Status:** Approved (design phase)
**Cluster:** D (Bases UI completion) — sub-project D.3
**Predecessors:** D.1 (backend correctness), D.2 (read-side rendering) — both shipped 2026-05-25
**Library:** `libs/bases` (`Corbomite::Bases`)

## Goal

Give `.base` editors the interactive query-management surface that D.2's read-only
rendering left out: three toolbar popover menus (Properties, Sort+group, Views), a
per-row properties drawer for frontmatter editing, and verified end-to-end inline
cell editing. After D.3 a user can manage which columns show and in what order,
build a multi-key sort and a group-by from a structured UI (not just header clicks),
rename/duplicate/delete/reorder named views, and edit a selected note's frontmatter
either inline in a cell or via a side drawer — all persisting to the `.base`/`.md`
files through the existing save paths.

## Audit references

- [`audit-2026-04-26/bases.md`](../../audit-2026-04-26/bases.md) §"Missing (prioritized)" items 10 (toolbar menus), and the "Inline-edit frontmatter cells" / "Sort+group menu" / "Views menu" rows of the MVP table.
- [`obsidian-audit/domains/bases.md`](../../obsidian-audit/domains/bases.md) §"Toolbar family (`BasesView.js:659–2470`)" — the canonical menu decomposition:
  - **Properties menu** (`GX`/`KX`/`YX`/`QX`) — visible-properties list + per-property edit + drag reorder + "Add property"/"Add formula"/"Hide all".
  - **Sort+group menu** (`e$`/`t$`/`n$`/`i$`) — sort-row stack + group-by row (property combo + direction).
  - **Views menu** (`s$`/`a$`/`l$`) — named-view picker + per-view form (name, layout-type, "Set default"/"Duplicate"/"Delete").
- [`obsidian-audit/domains/bases.md`](../../obsidian-audit/domains/bases.md) §`BasesViewConfig` API roles (`setOrder`/`setSortProperty`/`setGroupBy`/`clone`) — the mutation vocabulary D.3's pure helpers mirror.

## Scope

**In scope (D.3):**

1. Three toolbar popover menus: Properties, Sort+group, Views.
2. Per-row properties drawer (frontmatter editor for the selected entry), in a right-hand `QSplitter` pane.
3. Inline cell-edit verification + cleanup of the `BasesTableModel::*Role` references in `BasesCellDelegate`.

**Out of scope (→ D.4 or later):** formula editor, structured filter builder, undo/redo (`QUndoStack`), CSV/TSV/Markdown export, drag-out-as-wikilink, hover preview popover, right-click file context menu, plugin-registered view types. The Properties menu's **"Add formula"** action is omitted in D.3 because it requires the (D.4) formula editor.

## Architecture

D.3 follows D.2's `SortCycle` discipline: **every config mutation is a pure free
function** in a widget-free header (`ViewConfigOps.h`), unit-tested in isolation; the
popup panels and the drawer are thin GUI shells that call those helpers, then trigger
`QueryController::recomputeNow()` and `BasesView::requestSave()`. Frontmatter edits
(cell + drawer) route through the existing `FileManager::processFrontMatter` path,
identical to `BasesTreeModel::setData`.

### Data flow

```
Panel / drawer interaction
   → pure helper mutates BasesViewConfig / BasesQuery       (config edits)
       OR FileManager::processFrontMatter(file, mutator)     (value edits)
   → QueryController::recomputeNow()
   → QueryController::resultsChanged
   → BasesTreeModel reset (already wired in D.2)
   → BasesView::requestSave()  (config edits; value edits persist via the .md write)
```

`requestSave()` is the inherited `TextFileView` debounced disk write that header-sort,
column-reorder, and view-switch already use — D.3 adds no new persistence path for
config. Frontmatter value edits persist through the file write inside
`processFrontMatter` and surface back as a `cacheChanged` → recompute.

### Components

#### 1. `ViewConfigOps` — pure mutation helpers (new)

`libs/bases/include/corbomite/bases/ViewConfigOps.h` + `src/ViewConfigOps.cpp`. No Qt
widgets; depends only on `BasesViewConfig`, `BasesQuery`, `PropertyId`. All functions
are free functions in `Corbomite::Bases`. Mutate in place (matching `cycleHeaderSort`).

Column ops (operate on `BasesViewConfig::order`; visibility == membership in `order`):
- `void setColumnVisible(QVector<PropertyId> &order, const PropertyId &pid, bool visible, const QVector<PropertyId> &allProps);`
  — when showing, insert at the position implied by `allProps` ordering (stable); when hiding, remove.
- `void moveColumn(QVector<PropertyId> &order, int from, int to);`
- `void hideAllColumns(QVector<PropertyId> &order);` — clears `order`.

Sort ops (operate on `BasesViewConfig::sort`; complements D.2's `cycleHeaderSort`):
- `void setSortDirection(QVector<SortKey> &sort, const PropertyId &pid, const QString &dir);` — set/insert a key's direction ("ASC"/"DESC").
- `void removeSortKey(QVector<SortKey> &sort, const PropertyId &pid);`
- `void addSortKey(QVector<SortKey> &sort, const PropertyId &pid, const QString &dir);` — append if absent.

Group ops:
- `void setGroupBy(BasesViewConfig &cfg, const std::optional<PropertyId> &pid, const QString &dir);` — `nullopt` clears `cfg.groupBy`.

View CRUD (operate on `BasesQuery::views`, a `std::vector<std::unique_ptr<BasesViewConfig>>`):
- `bool duplicateView(BasesQuery &q, const QString &name, const QString &newName);` — deep-copy the named view (copy fields), append; returns false if `name` missing or `newName` already taken.
- `bool deleteView(BasesQuery &q, const QString &name);` — refuses to delete the last remaining view (returns false; a `.base` must keep ≥1 view per the empty-file invariant).
- `bool renameView(BasesQuery &q, const QString &oldName, const QString &newName);` — false on collision/missing. (View-rename does **not** rewrite `[[basefile#viewname]]` backlinks — that's a separate audit item, out of D.3.)
- `bool setDefaultView(BasesQuery &q, const QString &name);` — moves the named view to index 0 (Obsidian's "default" == `views[0]`).

#### 2. Toolbar popup panels (new)

Three `QToolButton`s appended to the existing toolbar `QHBoxLayout` in `BasesView`
(after the search field). Each button's `setMenu`-equivalent is a custom `QFrame`
panel with `Qt::Popup` window flag, shown anchored under the button. Each panel takes
a `BasesView*` (or narrower callbacks) to read current state and to request
`recomputeNow()` + `requestSave()` after a mutation.

- **`PropertiesMenuPanel`** (`include/corbomite/bases/PropertiesMenuPanel.h` + cpp)
  - A vertical list, one row per property in `controller->result()->properties()`:
    `[checkbox visible] [display name] [⠿ drag handle]`.
  - Checkbox → `setColumnVisible(activeView->order, pid, checked, allProps)`.
  - Drag handle reorders → `moveColumn`. (Use a `QListWidget` with `InternalMove` drag, or row up/down buttons if drag proves fiddly — drag preferred, buttons acceptable fallback.)
  - Footer: **Add property** (a combo/menu of discovered-but-hidden note.* keys → `setColumnVisible(..., true, ...)`), **Hide all** (`hideAllColumns`). **No "Add formula" in D.3.**

- **`SortGroupMenuPanel`** (`.h` + cpp)
  - Top section: one row per `SortKey` — `[property combo] [ASC/DESC toggle] [✕ remove]`; an **Add sort** button appends a key (`addSortKey` with the first unused property, ASC).
  - Bottom section: a single group-by row — `[property combo incl. "(none)"] [direction toggle]` → `setGroupBy`.
  - Property combos are populated from `result()->properties()`.

- **`ViewsMenuPanel`** (`.h` + cpp)
  - List of `query->views` by name with the active one marked. Selecting one calls `BasesView::setActiveView`.
  - Per-view actions (buttons or a small context menu): **Rename** (inline `QLineEdit` → `renameView`), **Duplicate** (`duplicateView`, auto-suffix " copy"), **Delete** (`deleteView`, disabled when only one view), **Set default** (`setDefaultView`).
  - Layout-type combo shows only `"table"` today — rendered read-only/display-only; per-view-type options are deferred.

#### 3. `PropertiesDrawer` (new)

`include/corbomite/bases/PropertiesDrawer.h` + cpp. A `QWidget` placed in the right
pane of a `QSplitter` that now wraps `BasesView`'s table (table left, drawer right).
Collapsed by default; toggled by a toolbar `QToolButton` (checkable). Repopulates on
the table's selection change.

- Header: the selected entry's file name (or "(no selection)").
- Body: a scrollable form, one row per frontmatter key of the selected `BasesEntry`
  — `[key label] [type-appropriate editor]`. Editors reuse the same per-type widgets
  the cell delegate builds (`QLineEdit`/`QDoubleSpinBox`/`QCheckBox`/`QDate(Time)Edit`),
  chosen from the value's `type()`.
- Footer: **+ add field** — prompts for a key name, adds an empty string field.
- Edits commit via `m_fm->processFrontMatter(entry->file(), mutator)` on editingFinished;
  the resulting `cacheChanged` recomputes and the drawer repopulates from the new selection state.
- Enumerate keys via `BasesEntry::getPropertyKeys()`; read values via `getValue(PropertyId{Note, key})`.

#### 4. Inline-edit polish (modify)

- Verify the existing `BasesCellDelegate::createEditor`/`setEditorData`/`setModelData`
  + `BasesTreeModel::setData` path works end-to-end on the `QTreeView` (it was written
  against the now-superseded `BasesTableModel` but the role integers coincide).
- **Cleanup:** change the `index.data(BasesTableModel::ValueTypeRole/ValuePtrRole)`
  reads in `BasesCellDelegate.cpp` to `BasesTreeModel::` constants (same values; clarity
  + removes the stale `#include "BasesTableModel.h"` dependency from the delegate where
  possible). This is a non-behavioural rename.

## Testing

- **`tst_view_config_ops`** (new, `QTEST_APPLESS_MAIN`) — exhaustive unit tests for every
  `ViewConfigOps` helper: column show/hide/insert-position/reorder/hide-all; sort
  set-direction/add/remove; group set/clear; view duplicate (incl. name-collision
  refusal), delete (incl. last-view refusal), rename (incl. collision), set-default
  (index-0 move). No widgets — mirrors `tst_sortcycle`.
- **Panels + drawer + inline edit:** GUI surfaces, verified by build + launch against a
  multi-view grouped `.base` in a dev vault (manual: toggle columns, build a 2-key sort,
  set/clear group-by, duplicate+rename+delete a view, edit a cell, edit via the drawer,
  confirm `.base`/`.md` on disk update). The model-side `setData` is already covered by
  `tst_bases_tree_model`.
- **Regression:** full `libs/bases` suite (`ctest -R tst_bases`) stays green; the
  pre-existing foundation-port failures outside `libs/bases` are unrelated.

## Definition of done

- `ViewConfigOps` helpers exist and `tst_view_config_ops` passes.
- Properties / Sort+group / Views popup panels open from toolbar buttons, mutate the
  active view config, recompute the table, and persist to the `.base` via `requestSave()`.
- Properties drawer shows/edits the selected entry's frontmatter via `processFrontMatter`;
  toggled by a toolbar button; tracks selection.
- Inline cell editing verified end-to-end on the `QTreeView`; delegate references the
  `BasesTreeModel` role constants.
- Full `libs/bases` suite green; clean build.
- No formula editor / filter builder / undo / export / drag / hover / context menu
  (all D.4+); "Add formula" absent from the Properties menu.

## Risks / notes

- **Available-property enumeration.** The Properties menu and Sort/group combos source
  candidates from `controller->result()->properties()`, which D.2 already unions visible
  `order` with discovered `note.*` keys. A property hidden from `order` stays discoverable
  (still in `properties()` if any row carries the key), so it can be re-shown. Fully-empty
  columns that were never in `order` won't appear — acceptable for D.3.
- **`Qt::Popup` focus/dismiss.** Popup panels must close on outside-click and not steal the
  toolbar button toggle. Standard `Qt::Popup` handles outside-click; verify the toggle
  button doesn't re-open on the dismiss click (guard with a timestamp or `QToolButton`
  popup-mode if needed).
- **Drawer ↔ recompute loop.** A drawer edit triggers recompute → model reset → selection
  may clear. Repopulate the drawer from the *persisted selection* (the entry's file path),
  re-resolving the row after reset, rather than holding a `QModelIndex`.
- **View CRUD on `unique_ptr` vector.** `duplicateView` must deep-copy fields into a fresh
  `BasesViewConfig` (the vector owns `unique_ptr`s); reuse `BasesQuery::clone()`'s
  field-copy approach or copy members explicitly.
- **`deleteView` last-view guard** preserves the empty-file `views[0]` invariant the parser
  relies on.
