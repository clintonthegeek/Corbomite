# Cluster D — Bases UI completion

> **⚠ 2026-06-10:** 90% executed via D.1–D.4c + formula editor + filter builder (see [INDEX](INDEX.md)). Only D.5 (Bases plugin API) remains — and it is absent from the scope list below. Treat this file as historical; D.5 needs a fresh brainstorm+spec.
>
> **⚠ 2026-08-20 — D.6 discovered, shipped, and live-verified same day: there was no UX to create a new `.base` file.** Every phase above (D.1–D.4c, formula editor, filter builder) assumed a `.base` file already exists and built out the *editing* surface. Nobody built the *creation* surface — verified at discovery time: `.base` was registered only for **opening**; no `file_new_base` KAction, no `createNewBase()` method, no File Explorer "New Base" context-menu entry (compare the shipped canvas equivalents). Implemented same-day exactly as scoped below (§D.6) — `file_new_base` KAction (icon `x-office-spreadsheet`, File-menu-only per the `file_new_canvas` precedent), `MainWindow::createNewBase()`, `FileExplorerView::onNewBaseIn()` + "New Base"/"New Base Here" context-menu entries, `corbomiteui.rc.in` version bump 12→13. User live-tested against a real vault and confirmed it works. Both D.6 open questions resolved by implementation: icon = `x-office-spreadsheet` (Breeze has it, mimetype-appropriate); creation flow = silent create-then-rename (no upfront dialog), matching canvas.
>
> **⚠ 2026-08-20 — Filter builder's leaf editor upgraded from raw formula text to a point-and-click picker, shipped and live-verified same day.** Scope item 2 below ("Filter UI (structured builder, not raw YAML editing)") was only half-delivered by the 2026-05-28 filter-builder ship: it built the AND/OR/NONE **group** nesting but every individual **leaf** rule was still a bare formula-expression text box — a user had to already know the formula DSL (e.g. type `!note.Practical.isEmpty()` by hand) to filter on anything. Discovered when a user asked "how do I filter to notes that have this property set" and had no path to an answer without being handed a raw expression. New `Corbomite::Bases::FilterRuleRow` (`libs/bases/{include/corbomite/bases/FilterRuleRow.h,src/FilterRuleRow.cpp}`) replaces each leaf with Property / Operator / Value dropdowns — matching what Obsidian's own Filter menu actually offers (verified against `testvaults/obsidian-help/Bases/Views.md` on the mounted drive: property + type-aware operator + value, with a `</>` advanced-editor escape hatch for anything more complex). Operator lists are type-aware; since Corbomite has no central property-type registry (unlike Obsidian's `metadataTypeManager`), the type is inferred by sampling up to 500 rows of actual data per property (`FilterPropertyInfo.{h,cpp}`, `buildFilterPropertyInfos`). A toggle switches to the old raw-text box for anything the picker can't express; loading an existing filter reverse-parses it against the picker's own template set, falling back losslessly to advanced mode on no match. 11 new tests (`tst_filter_rule_row`), full offscreen suite green (328/328 excl. the by-design benchmark timeout), user live-verified.

> **Created 2026-04-26 from audit reset.** Stub plan; needs brainstorm + full plan expansion before dispatch. Cluster K (legacy) shipped the Bases runtime + data model + Pratt parser as MVP. The UI is skeletal: no formula editor, no group rendering, no properties drawer, no export, no drag, no hover, no undo, no multi-key sort UI. This cluster builds out those surfaces.

## Goal

Bases reaches feature-parity-comparable UX for vault users editing `.base` files. A user opens a `.base` file, sees a fully-functional table view with grouping, can filter via a structured UI (not just YAML), can edit cells round-tripping back to source `.md`, can drag rows, can sort multi-column, can export.

## Audit references

- [audit-2026-04-26/bases.md](../../audit-2026-04-26/bases.md) — entire doc; especially §"Missing (prioritized: structural vs cosmetic)"
- [audit-2026-04-26/bases.md](../../audit-2026-04-26/bases.md) §"On-disk `.base` format compatibility" — overlap with Cluster A (key order)
- [obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md](../../obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md) — formula DSL reference

## Scope (in scope)

1. Formula editor (with parser-backed validation + autocomplete)
2. Filter UI (structured builder, not raw YAML editing) — **group structure shipped 2026-05-28; leaf-level point-and-click property/operator/value picker shipped 2026-08-20 (see banner above). Fully done.**
3. Group rendering (currently group-by parses but groups don't render)
4. Properties drawer (per-row frontmatter editor)
5. Export (CSV / JSON / clipboard)
6. Drag (row reorder, drag-out-as-link)
7. Hover (rich preview popover on row hover)
8. Undo stack integration (cell edits go through QUndoStack)
9. Multi-key sort UI
10. Vault-bound function plumbing — `file()`, `LinkValue::asFile`, `looseEquals`, `linksTo` (currently stubs)
11. `TagListValue` for `file.tags.contains("#parent")` matching subtag semantics
12. `ListValue::sort()` null-comparator bug fix (small, could go to punch list — keep here for cluster coherence)

## Out of scope

- `.base` YAML key-order preservation → **Cluster A** (shares root cause with frontmatter writer)
- Other view kinds (cards, list, calendar) → defer to a future cluster

## Phases

TBD — brainstorm. Likely 4–5 phases: (1) formula editor + autocomplete, (2) filter UI + structured builder, (3) group rendering + properties drawer, (4) interactivity (drag, hover, undo, multi-key sort), (5) Vault-bound function plumbing + cleanup.

## Status

**Only D.5 (Bases plugin API) remains.** D.1–D.4c, formula editor, filter builder (group structure), D.6 (base creation UX), and the filter builder's leaf-level picker are all shipped — see banners above and `docs/decisions-archive.md`. D.5 is plan-needed (stub, needs a fresh brainstorm+spec).

---

## D.6 — Base creation UX (discovered 2026-08-20)

### Problem

Corbomite can open, render, edit, and save `.base` files (D.1–D.4c, formula editor, filter builder — all shipped). It cannot **create** one. There is no menu item, toolbar button, context-menu entry, or command-palette action anywhere in the app that produces a new `.base` file. The only way to get a `.base` file into a Corbomite-managed vault today is to author it outside the app (another Obsidian-compatible tool, or by hand) and let Corbomite discover it on next scan.

This is invisible in the tracking history because Cluster D's own scope list (§"Scope (in scope)" above) enumerates *editing* surfaces only — formula editor, filter UI, group rendering, properties drawer, export, drag, hover, undo, sort UI, vault-bound functions. "How does a `.base` file come to exist" was never asked. Cluster K (legacy) shipped the Bases *runtime* as "viewer + parser + persistence" (see punch-list `[bases][ui-bundle][P2]`, resolved 2026-06-10 pointer to Cluster D) — creation was implicitly out of scope there too and nothing since has picked it up. Cluster O's Bases-related work (Phase O5, `BasesViewActions`) is scoped to promoting the **7 in-view toolbar buttons** (Properties/Sort+group/Views/Filters/Drawer/+New/Results) of an *already-open* `BasesView` to KActions — that `+New` button creates a **note** matching the base's filter (Obsidian's `NewItemMenu`, §1 of the audit doc), not a new `.base` file. Neither cluster owns this gap.

### What's already implemented (reusable, no new plumbing needed)

- **`FileManager::createNewFile(TFolder *parent, const QString &name, const QString &extension, const QByteArray &content)`** (`libs/vault/include/corbomite/vault/FileManager.h:71`) is fully generic — this is exactly what `MainWindow::createNewCanvas()` already calls with `extension="canvas"`. The same call with `extension="base"` works today with zero library changes.
- **An empty `.base` file is a fully valid file.** `BasesQuery::fromString` (`libs/bases/src/BasesQuery.cpp:185-196`) explicitly special-cases an empty/whitespace-only string and returns a default one-view `{type: "table", name: "All"}` query — this replicates the audit's §8 `[CRIT]` invariant "empty `.base` is valid." So unlike canvas (which needs seed JSON `{"nodes":[],"edges":[]}`, `MainWindow.cpp:1938-1939`), a new base needs **no seed content at all** — `QByteArray()` is sufficient and `BasesView` will render it as an empty, immediately-editable Table view.
- **`.base` extension → `BasesView` factory** is already registered for *opening* (`MainWindow.cpp:1582`, `ViewRegistry::registerViewWithExtensions`), so `openFileInWorkspace(tf->path)` on a freshly-created `.base` file will correctly open it in `Corbomite::Bases::BasesView` with no new wiring.
- **`FileManager::promptForFileRename`** (used by `createNewCanvas()` for the create-then-rename flow) works on any `TFile`, `.base` included — no `.base`-specific rename path needed.
- **`NotesTreeModel::isTreeFile`** already accepts `base` (`libs/models/src/NotesTreeModel.cpp:17`) and tags it via `FileTypeRole` (`:101`), so a newly-created `.base` file shows up in the File Explorer tree immediately (this part of Cluster K already shipped, per punch-list `[bases][ui-bundle]` "FileExplorer hid `.base` files").

In short: **this is a wiring-only gap.** The model/vault/view layers already support base creation end to end; only the *action* layer (KActions + menu/context-menu entries + one `MainWindow` method) is missing. This makes D.6 much smaller than a typical Cluster D phase — closer in size to the canvas M2.6 task than to the formula-engine-scale phases D.1–D.5.

### Obsidian parity reference

`docs/obsidian-audit/addenda/2026-04-19-file-explorer-context-menu.md` (File Explorer right-click menu enumeration) documents Obsidian's actual affordance set:

- **Folder-row context menu, action section:** New note → New canvas → **New base** → New folder (in that order).
- **Empty-area / vault-root context menu:** New note → New canvas → **New base** → New folder → Collapse all folders → Show attachments.
- That addendum's own "Implementation notes for Corbomite" section (written 2026-04-19, before D.1–D.5 shipped and before this gap was noticed) already anticipated the command name: `file-explorer:new-base-in-folder`, alongside the note/canvas/folder siblings it also anticipated — those note/canvas/folder ones have since shipped; the base one has not.

No hamburger/File-menu equivalent is documented in that addendum (it covers File Explorer right-click only), but Corbomite's own `MainWindow` already exposes `file_new_note` and `file_new_canvas` as top-level File-menu/toolbar KActions (`corbomiteui.rc.in:10-11`) beyond what Obsidian's hamburger offers — Corbomite's existing convention (one global KAction per creatable file type, independent of File Explorer) should extend to bases for consistency, command-palette reachability, and hotkey-assignability.

### KActions and wiring to create and register

Mirroring `file_new_canvas` / `createNewCanvas()` / `onNewCanvasIn()` exactly:

1. **`file_new_base`** — new top-level `KAction`, registered in `MainWindow`'s action setup alongside `file_new_note`/`file_new_canvas` (`MainWindow.cpp:1244-1253`). Text: `i18n("New Base")`. Icon: needs a decision (see Open questions) — candidates `"view-form-table"`, `"x-office-spreadsheet"`, or a KDE breeze-icon-theme name matching Bases' conceptual "table/database view" identity, analogous to canvas's `"draw-rectangle"`. Connects to a new `MainWindow::createNewBase(const QString &folder = QString())`.
2. **`MainWindow::createNewBase(const QString &folder)`** — new method, structurally identical to `createNewCanvas()` (`MainWindow.cpp:1905-1947`): open-vault-if-needed guard, resolve/create the target `TFolder*`, call `m_fileManager->createNewFile(parent, QString(), QStringLiteral("base"), QByteArray())` (no seed content needed per above), `openFileInWorkspace(tf->path)`, then `m_fileManager->promptForFileRename(tf, this)`. Default filename follows the existing `collisionFreeName` "Untitled" convention already used by canvas/note creation — no new naming logic.
3. **`corbomiteui.rc.in`** — add `<Action name="file_new_base"/>` immediately after `file_new_canvas` in both locations it currently appears (`:11` File menu, `:73` toolbar) so the action reaches the File menu, the main toolbar, the command palette, and the Hotkeys settings page (Corbomite's one true `KActionCollection` surface, per the Cluster O audit) automatically — no separate command-palette or hotkey registration needed, KXMLGUI provides all three from one `addAction`.
4. **`FileExplorerView::onNewBaseIn(const QString &folder)`** — new method, structurally identical to `onNewCanvasIn()` (`FileExplorerView.cpp:158-179`): resolves the folder-context `TFolder*` and calls `m_fmProxy->createNewFile(parent, QString(), QStringLiteral("base"), QByteArray())` through the plugin proxy (same `vault.write`-gated path canvas uses).
5. **`FileExplorerView`'s folder-row context menu** (`FileExplorerView.cpp:214-221` today: New Note Here, New Canvas Here) — add "New Base Here" between them per the Obsidian ordering above, wired to `onNewBaseIn(contextPath)`.
6. **`FileExplorerView`'s empty-area/root context menu** (`FileExplorerView.cpp:246-253` today: New Note, New Canvas) — add "New Base" between them, wired to `onNewBaseIn(QString())`.

That's the complete action-layer surface: **one new KAction, one new `MainWindow` method, one new `FileExplorerView` method, two new context-menu entries, one `.rc` edit.** No new dialogs (reuses `promptForFileRename`), no new vault/model code (§"What's already implemented" above), no new tests beyond the standard create-file regression pattern already covering canvas (`tst_file_manager_newfile`-style).

### Open questions — resolved 2026-08-20 at implementation time

- **Icon.** Resolved: `x-office-spreadsheet` (present in the Breeze theme at all standard sizes; a table/spreadsheet-shaped mimetype icon reads correctly for Bases' "database view" identity). Tree-row per-type icon consumption (`NotesTreeModel::FileTypeRole` has no `Qt::DecorationRole` consumer) was scoped out — remains open, but doesn't block file creation.
- **Should "New Base" prompt for anything before creating?** Resolved: no upfront dialog — silent create-then-rename, exactly matching `createNewCanvas()`. Confirmed correct by the empty-file-is-valid invariant (a blank `Untitled.base` opens immediately into a usable, editable Table view) and by live testing.
- **Folder-scoped `+ New` button parity.** Still unaddressed, still out of scope for D.6 — `BasesQuery.newItemFolder`/`newItemTemplate` remain a separate concept (a base's own in-view `+ New` button, for creating matching *notes*) from this file-creation gap.

### Audit references

- [`docs/obsidian-audit/domains/bases.md`](../../obsidian-audit/domains/bases.md) §9 "Observable user features" — "Create a `.base` file anywhere; opens as a Bases view" is the first bullet.
- [`docs/obsidian-audit/addenda/2026-04-19-file-explorer-context-menu.md`](../../obsidian-audit/addenda/2026-04-19-file-explorer-context-menu.md) — canonical Obsidian context-menu ordering and the (unfulfilled) `file-explorer:new-base-in-folder` command-name anticipation.
- [`docs/audit-2026-08-20-context-sensitive-ui.md`](../../audit-2026-08-20-context-sensitive-ui.md) §4.6, §"O5" — the adjacent, non-overlapping gap (Bases' *in-view* toolbar has no KActions); useful to dispatch near D.6 since both touch Bases' action surface, but scoped separately (D.6 is file creation, O5 is in-view-toolbar promotion).

### Status

**Shipped and live-verified 2026-08-20.** Landed exactly as scoped — one KAction, one `MainWindow` method, one `FileExplorerView` method, two context-menu entries, one `.rc` edit. No follow-up needed beyond the still-open (non-blocking) tree-icon-consumption item noted above.
