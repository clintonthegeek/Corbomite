# Cluster D — Bases UI completion

> **⚠ 2026-06-10:** 90% executed via D.1–D.4c + formula editor + filter builder (see [INDEX](../INDEX.md)). Only D.5 (Bases plugin API) remains — and it is absent from the scope list below. Treat this file as historical; D.5 needs a fresh brainstorm+spec.
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

---

## D.5 — Bases plugin API (SPEC — not a dispatch)

> **⚠ 2026-10-01 — written as a brainstorm/spec for user review. NOT
> dispatchable.** All three status documents (`PROJECT-STATE.md`, `INDEX.md`,
> and this file's own banners) have said "only D.5 remains" since
> 2026-05-29 with nothing behind the item — a checkbox, a scope, or a spec. This
> section is that spec, at brainstorm depth. Nothing here is scheduled. Phase
> numbers below are a *proposed* breakdown for whoever picks it up after review;
> the §4 open questions are the actual gating decisions, and several of them
> (public API surface, permission tokens, the vault-access seam) should be
> reviewed **together with any other Bases plugin-API work** rather than
> piecemeal.
>
> Sizing: the library work is small (a facade over existing headless classes);
> the API-design and review work is the cost. Do not let a small diff become a
> public-ABI decision by accident.

### D.5.1 — What "a Bases plugin API" could mean

Five candidate surfaces, in descending order of how much a plugin author
actually wants them. They are **not** of equal value and this spec deliberately
does not promise all five.

**(a) Custom formula functions.** A plugin registers `myScore(note)` (or a
per-type member function) into the formula evaluator.

- **Status: nearly free, and the closest thing to already existing.**
  `Corbomite::Bases::FunctionRegistry` already has a **process-global
  instance** (`libs/bases/include/corbomite/bases/FunctionRegistry.h:65` —
  `static FunctionRegistry &global();`), `addGlobal()` / `addForType()`,
  `findGlobal`/`findInstance`, and `allNames()` for autocomplete. Builtins are
  registered into it exactly once (`libs/bases/src/Builtins.cpp:635-637`,
  `registerBuiltins(r)`).
- `BasesView` takes an optional injected registry and otherwise falls back to
  the global one (`libs/bases/src/BasesView.cpp:270,276`), so a plugin that
  registers into `FunctionRegistry::global()` is picked up by every live view
  without any host wiring.
- **Blocker:** plugins cannot reach `corbomite-bases` at all today —
  `PluginContext` has no `bases()` accessor
  (`libs/vault/include/corbomite/vault/PluginContext.h:116-137`), and although
  `corbomite-bases` *is* installed and exported for `find_package(Corbomite)`
  consumers (`libs/bases/CMakeLists.txt:128-133`), its `target_link_libraries`
  puts `Corbomite::Core/Storage/Vault` and `Markoff::Parser` behind
  `$<BUILD_INTERFACE:>` (`libs/bases/CMakeLists.txt:118-125`), so a
  third-party plugin linking `Corbomite::Bases` would compile against a
  partially-unresolvable dependency graph. An in-tree plugin could link it
  today; a distributed one cannot.

**(b) Headless query execution.** A plugin runs a `.base` query and gets rows
back — the building block for a custom view, an export, a dashboard, a
"largest N notes by X" panel, or a script.

- **Status: the pieces exist and are deliberately widget-free.**
  `BasesQuery::fromString` (with the empty-base default,
  `libs/bases/src/BasesQuery.cpp:186-195`), `Evaluator`, `BasesVaultResolver` behind the narrow `VaultResolver`
  interface (`libs/bases/include/corbomite/bases/VaultResolver.h` — two
  methods, `fileAt` / `resolveLinkTarget`), `EvalContext::vault()`
  (`EvalContext.h:33`), `BasesQueryResult::rows()` / `properties()`
  (`BasesQueryResult.h:41,47`), and `TableExporter` (CSV/TSV/Markdown/HTML/
  Obsidian-table, explicitly "stack-allocated temporary").
- **Blocker:** two. (i) **No way to get an `EvalContext`/`VaultResolver`.**
  The host builds these inside `BasesView::setServices(...)`; a plugin gets
  *proxies* from `PluginContext` (`VaultProxy`, `MetadataCacheReader`), not the
  concretes `BasesVaultResolver` needs. (ii) **Permission model undefined.**
  Running a query reads the whole vault. Under the existing tokens that is
  `vault.read` + `metadata.read` — but a formula function registered under
  `ui.editor`-ish semantics would execute *inside* someone else's query, which
  is a different trust question.
- This is the surface with the highest user value **and** the highest design
  cost. It is the one worth doing.

**(c) Cell rendering / value hooks.** A plugin renders one Bases value type
itself (an Image cell, a Map, a rating).

- **Status: the delegate is a single hard-dispatched
  `QStyledItemDelegate`** (`libs/bases/include/corbomite/bases/BasesCellDelegate.h`
  — `createEditor`/`setEditorData`/`setModelData`/`paint` dispatching on
  `BasesTableModel::ValueTypeRole`), and its own header says rich rendering for
  Image/HTML/Markdown "defers to a placeholder label; a Cluster-J
  `EmbedRenderer` wire-in is a follow-up."
- **Assessment: do not expose the delegate.** It would mean making value
  dispatch a registry and giving plugins a paint callback on every repaint.
  That is the plugin API's most invasive possible surface, for the least
  demanded feature. If rich cells matter, ship them in-tree (the
  `EmbedRenderer` follow-up is already named) and revisit later.

**(d) New view kinds.** A plugin registers `cards` / `calendar` for `.base`
files.

- **Status: `ViewConfigOps` and `ViewsMenuPanel` exist**
  (`libs/bases/include/corbomite/bases/ViewConfigOps.h` — duplicate/rename view
  ops; `ViewsMenuPanel.h`), and Cluster D explicitly deferred "other view kinds
  (cards, list, calendar)" to "a future cluster".
- **Blocker:** a view kind is not a `ViewRegistry` view. `ViewRegistrar::
  registerView` (`libs/core/include/corbomite/core/proxies/ViewRegistrar.h:27`)
  registers a *file view* by extension — a plugin **can** ship its own
  file-backed view for its own extension today. What it cannot do is contribute
  a *view kind* to someone else's `.base` file, because view-kind dispatch lives
  inside `BasesView`. That is a deeper refactor, not a registrar.

**(e) Export.** A plugin exports a query result.

- **Status: free once (b) exists** — `TableExporter` is pure and widget-free.
  Do not design a separate export API; expose (b) and (e) falls out.

### D.5.2 — What the generic registries already give a Bases plugin today

So this section is not read as "Bases has no plugin story":

| Generic surface | Source | Usable by a Bases plugin today? |
|---|---|---|
| Register a file view for an extension | `PluginContext::views()` → `ViewRegistrar::registerView` / `registerExtensions` (`PluginContext.h:122`) | **Yes** — a plugin can ship a competing view for its own extension, and register an extension mapped to the existing `bases` type only if it can construct a `BasesView` factory itself (in-tree it can; distributed it would need the library) |
| Add commands | `PluginContext::commands()` → `CommandRegistrar` (`PluginContext.h:121`) | **Yes** — e.g. `bases:runQuery`, `bases:export` |
| Add ribbon icons / status-bar items / menus | `RibbonRegistrar`, `StatusBarRegistrar`, `MenuInjector` (`PluginContext.h:121-135`) | **Yes** |
| Register post-processors, code blocks, embeds, decorators, suggesters | the `ui.rendering` / `ui.editor` registrars (`PluginContext.h:128-136`) | **Yes** — including markdown-in-a-note features that a Bases view emits |
| Per-plugin KConfig page + `data.json` | `Plugin::configPages()` / `PluginContext::config` / `loadData`/`saveData` | **Yes** |
| Vault read/write through proxies | `VaultProxy`, `FileManagerProxy`, `MetadataCacheReader` (`PluginContext.h:116-119`) | **Yes** — and note `MetadataCacheReader` already exposes `tagsIn()` / `allTags()`, the same proxy pattern D.5 would follow for a Bases facade |

### D.5.3 — Proposed shape (one option, for review)

A **`BasesContext` facade** handed to plugins that hold a permission, mirroring
the existing proxy style rather than exposing `corbomite-bases` types:

```
PluginContext::bases()  ->  BasesContext          // gated on a new token
    QList<BasesQueryResultRow> runQuery(basePathOrViewName)   // (b), row-shaped POD
    bool registerFormulaFunction(localId, BasesFunctionSpec)  // (a)
    QString exportAs(runQueryResultRef, format)               // (e), reuses TableExporter
```

with `BasesQueryResultRow` a plain value type (property id, display name, cell
text, cell type tag) so the plugin ABI does **not** drag `ValuePtr` /
`EvalContext` / `std::type_index` across the boundary. `corbomite-bases` stays
an implementation detail the host calls into; plugins never link it.

Why this shape rather than exposing the library directly:

1. **ABI containment.** The existing plugin ABI is deliberately narrow (a
   `CORBOMITE_PLUGIN_API_LEVEL` integer, `PluginApi.h:15`) with everything
   proxied. Handing out library types makes every Bases refactor a plugin ABI
   break.
2. **Permission honesty.** A query is a whole-vault read. It deserves its own
   token (`bases.read`) rather than riding on `vault.read` + `metadata.read`
   implicitly.
3. **The `EvalContext::vault()` question has to be answered once, centrally.**
   Whatever the answer is, it belongs behind a facade, not in every plugin.

### D.5.4 — Proposed phase breakdown (post-review)

- **D5.0 — Review this section.** Answer §D.5.5 below. No code.
- **D5.1 — (a) formula functions, in-tree only.** Expose `registerFormulaFunction`
  through the facade for in-tree plugins; add tests proving a registered
  function is visible to a live `BasesView` *and* appears in autocomplete
  (`FunctionRegistry::allNames()` feeds the candidates). Deliberately not
  exposed to distributed plugins until the CMake interface gap
  (`$<BUILD_INTERFACE:>`) is fixed or the facade fully replaces the types.
- **D5.2 — (b) headless execution.** `VaultResolver` construction from the
  host's concrete services, row-shaped results, `metadata`-equivalent
  invalidation (re-run on `indexFinished`). This is the bulk of the work.
- **D5.3 — (e) export.** Small; rides on D5.2.
- **D5.4 — permissions + docs.** New token in `PluginPermissions.h`, manifest
  documentation, `docs/plugin-development/{API-REFERENCE,permissions}.md`
  updates.
- **D5.5 — (c)/(d) explicitly deferred**, recorded as decisions with reasons so
  they are not re-litigated.

### D.5.5 — Open questions for the user (blocking D5.1)

1. **Is a formula-function hook the minimum viable plugin API, or is headless
   execution the thing that matters?** The order above assumes functions first
   because they are cheap; if query execution is the actual goal, D5.1's value
   drops and the cluster is really one phase.
2. **Who may register a formula function?** A function runs inside every
   user's query. Suggest `bases.formulas` as a distinct token, not folded into
   `ui.editor`.
3. **Can a plugin's function see the vault?** `EvalContext::vault()` currently
   exposes `fileAt()` + `resolveLinkTarget()` to *builtins*. Granting that to a
   third-party function is a security decision, not an API decision.
4. **Does a plugin get row values or only formatted cell text?** Row values
   (typed) are far more useful and much more ABI to carry. Recommend: values in
   a versioned row POD, with the `CORBOMITE_PLUGIN_API_LEVEL` bumped to 2 if
   it is exposed.
5. **Should in-tree plugins be allowed to link `corbomite-bases` directly** as a
   shortcut, or must everything go through the facade even in-tree? Two answers
   here means two code paths to keep working forever.
6. **Interaction with Cluster F-b Q5** (a `PluginContext` accessor for named
   workspaces): both add a `PluginContext` method and a permission token. Batch
   the public-surface review rather than shipping two uncoordinated additions.

### D.5.6 — Audit references

- [`docs/obsidian-audit/domains/bases.md`](../../obsidian-audit/domains/bases.md) §2 (the `FileValue` aggregate accessors — `getTags`/`getLinks`/`getProps` on the cached per-file object, `bases.md:45`), §9 ("Observable user features"), and the corpus's own Open Questions — the source of what a Bases plugin API is *for*.
- [`docs/obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md`](../../obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md) §8.1 — the `FnParam` contract that `BasesFunction` implements.
- [`docs/audit-2026-04-26/plugin.md`](../../audit-2026-04-26/plugin.md) §"Internal-plugin inventory" — Bases listed as `NOT-A-PLUGIN-YET`; still accurate.
- [`docs/plugin-development/API-REFERENCE.md`](../../plugin-development/API-REFERENCE.md) + [`permissions.md`](../../plugin-development/permissions.md) — the docs any new surface must update.
- [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Cluster D row and [`superpowers/plans/INDEX.md`](INDEX.md) Cluster D row — both say "only D.5 remains", which is why this section exists.
