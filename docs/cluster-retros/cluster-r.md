# Cluster R — View-header menus (Retro)

**Closed:** 2026-04-19.
**Spec:** [`../superpowers/specs/archive/2026-04-19-cluster-r-view-header-menus-design.md`](../superpowers/specs/archive/2026-04-19-cluster-r-view-header-menus-design.md)
**Plan:** [`../superpowers/plans/archive/2026-04-19-cluster-r-view-header-menus.md`](../superpowers/plans/archive/2026-04-19-cluster-r-view-header-menus.md)
**Commits (range):** `09296582` (spec) · `c3e8a8b7` (plan) · `b82d19f0` (MenuSectionHelper canonical order + submenu) · `134761c7` (View::onMoreOptionsMenu hook + onPaneMenu(source)) · `583b721f` (ItemView::showMoreOptionsMenu rewrite) · `ce17b0c2` (FileNameValidator) · `d308c10d` / `94d00be6` / `f56ab027` (rename / move / delete modals on FileManager) · `ba789aef` (Platform::openWithDefaultApp + showInFolder) · `943920e8` (PathUtils::obsidianUrlFor + corbomiteUrlFor) · `dcd745ed` (register `:open` commands on 5 sidebar plugins) · `515b29fd` (file-explorer:reveal-file + FileExplorerView::revealPath) · `a40e1c91` (markdown:add-metadata-property + helper) · `d5f8ffbc` (ExportToPdf helper) · `b144a157` (MarkdownView::onMoreOptionsMenu wiring) · `baf41d19` (CanvasScene renderToImage / renderToSvg export pipeline) · `c28bae8e` (CanvasFileView export-as-image modal + hamburger) · `814188af` (EditableFileView hamburger wiring, P2.8) · `5a354333` (GraphView hamburger + corbomite-graph-view:copy-screenshot) · `c5448b38` (fix: unload plugins before CommandRegistry destroy).

---

## What shipped

### P1 — Menu substrate alignment

- `MenuSectionHelper` canonical section order matches Obsidian verbatim (12 sections: `close → pane → open → action → find → info → info.copy → view → view.linked → system → "" → danger`); `addSubmenu(sectionId, title, icon)` returns a nested helper; `finalize()` walks the canonical order, inserts section separators, applies icons.
- `View::onMoreOptionsMenu(MenuSectionHelper&)` is the canonical subclass hook; `View::onPaneMenu(QMenu*, QString source)` kept as back-compat / tab-bar entry. `ItemView::showMoreOptionsMenu()` constructs the menu, invokes `onMoreOptionsMenu`, then `onPaneMenu("more-options")`, then `MenuEventEmitter::leafMenu` for plugin injection, then `finalize()`, then `exec()`.

### P2 — Universal file-menu items on `EditableFileView`

All items wired in `EditableFileView::onMoreOptionsMenu`:

- **action section:** Rename… (`FileManager::promptForFileRename`), Move to… (`FileManager::promptForMove`), Bookmark… (disabled placeholder → Cluster S), Add file property (MarkdownView-specific, `markdown:add-metadata-property`), Export to PDF (MarkdownView-specific, `ExportToPdf` helper).
- **info.copy submenu:** Copy Obsidian URL (`PathUtils::obsidianUrlFor`), Copy vault path, Copy absolute path.
- **view.linked submenu:** Open backlinks / outlinks / file properties / outline / local graph — each dispatches the corresponding sidebar-plugin's `:open` command (wired via commit `dcd745ed`). "Open version history" disabled placeholder for Cluster T.
- **system section:** Open in default app (`Platform::openWithDefaultApp`), Show in system explorer (`Platform::showInFolder`), Reveal in navigation (`file-explorer:reveal-file` command → `FileExplorerView::revealPath`).
- **pane section:** Split right, Split down (dispatch `workspace:split-vertical`/`workspace:split-horizontal`), Open in new window (disabled placeholder → Cluster G follow-up #6).
- **find section:** Find…, Replace… (disabled placeholders → Qutepart fork Phase 3).
- **danger section:** Delete (`FileManager::promptForDeletion`).

Supporting infrastructure: `FileNameValidator` (shared rename/create validation), modal dialogs on `FileManager` (`promptForFileRename`, `promptForMove`, `promptForDeletion`).

### P3 — Per-view specialisations

- **MarkdownView:** view section gets Reading-view / Source-mode / Split toggles + Backlinks-in-document toggle (Reading-view-gated, persisted to `viewState["backlinksInDocument"]`).
- **CanvasFileView:** action section gets Export as image… (modal + `CanvasScene::renderToImage` / `renderToSvg` pipeline).
- **GraphView:** action section gets Copy screenshot (`corbomite-graph-view:copy-screenshot` command).

### P4 — Inline backlinks-in-document PostProcessor

Wired as a Markdown PostProcessor keyed on `viewState["backlinksInDocument"]` — when toggled, renders the backlinks list inline at the bottom of the Reading view.

### Tests

Menu substrate + EditableFileView universal items + per-view specialisations all have unit coverage. Pre-existing flakies (`tst_benchmark_layout` timeout, `tst_e2e_gui::tabBar-count`) unchanged.

---

## Follow-ups

Captured 2026-04-19 from the Cluster R vs Obsidian exposure audit. Listed in rough priority order.

### 1. File Explorer right-click context menu **(HIGH — new cluster)**

**Gap.** Obsidian exposes Rename / Move / Delete / Copy path / Show in folder / Open in default app / Reveal in navigation / Bookmark… from **both** the pane hamburger *and* the File Explorer right-click. Corbomite has the hamburger side; File Explorer (`src/plugins/file-explorer/FileExplorerView.*`) currently has no right-click menu on file/folder rows.

**Impact.** A high-discoverability entry point Obsidian users rely on. Muscle-memory gap.

**Resolution.** New cluster — scouting plan at [`../superpowers/plans/archive/2026-04-19-cluster-u-file-explorer-enhancements-SCOUTING.md`](../superpowers/plans/archive/2026-04-19-cluster-u-file-explorer-enhancements-SCOUTING.md). Reuses `FileManager::promptForFileRename/promptForMove/promptForDeletion` + `Platform::*` + `PathUtils::*` primitives already shipped in Cluster R. Audit addendum at [`../obsidian-audit/addenda/2026-04-19-file-explorer-context-menu.md`](../obsidian-audit/addenda/2026-04-19-file-explorer-context-menu.md).

### 2. Plugin mid-construction menu injection **(BLOCKING for plugin API 1.0 — Cluster H follow-up)**

**Gap.** Obsidian fires `workspace.trigger("file-menu" | "leaf-menu" | "editor-menu", menu, …)` *before* `menu.show()`, so plugins can call `item.setSection(...)` and land in the correct canonical bucket. Corbomite's `MenuEventEmitter::leafMenu` currently emits *after* `MenuSectionHelper::finalize()` — plugin items fall into the unset `""` bucket and can't participate in section ordering.

**Impact.** Does not break Cluster R's own UX (all built-in items sit in correct sections). Breaks plugin-API parity: third-party plugins have no way to inject items into `view.linked` / `system` / `danger` / etc.

**Resolution.** Cluster H plugin-API follow-up. Two options, pick one:

- **Option A (minimal):** Move `MenuEventEmitter::leafMenu` emission to *before* `helper.finalize()` in `ItemView::showMoreOptionsMenu()`. Plugins receive `QMenu*` and a `QString` sectionId-via-property convention.
- **Option B (ergonomic):** Expose `MenuSectionHelper*` directly to plugin listeners (requires ABI stability of `MenuSectionHelper`; promote to `libs/core/` public header + document as shape-stable).

Option B is the better long-term shape; Option A is a one-line fix if we want parity before plugin API 1.0 lands.

### 3. Hotkey hints on menu items **(P4 polish)**

**Gap.** Obsidian's `Menu` renders `[Ctrl+S]` hints next to command-backed items. Corbomite's `MenuSectionHelper` doesn't populate them; `QAction::setShortcut()` is not called when the command is only dispatched (not bound as an action's shortcut).

**Impact.** Users can't discover keyboard shortcuts from the menu.

**Resolution.** Post-finalize pass in `MenuSectionHelper::finalize()` — for each action whose data-role holds a `commandId`, look up the command's hotkey via `CommandRegistry::hotkeyFor(commandId)` and append `[<keystring>]` to the action's text (or, better, set it as the action's shortcut so Qt renders it natively right-aligned). ~0.5 day.

### 4. Danger-section styling **(theme polish)**

**Gap.** Obsidian's `menu.addItem(…).setWarning()` applies `.is-warning` CSS to render the item in red. Qt's `QAction` has no warning/danger semantic; Corbomite's Delete item sits in the danger section but has no visual differentiation.

**Impact.** Minor — the section separator visually separates Delete from benign items, but there's no colour cue.

**Resolution.** Two approaches, either works:

- **Theme-driven:** Set `action->setProperty("corbomite-danger", true)` in `MenuSectionHelper::addToSection(QAction*, "danger")`; theme QSS targets `QMenu::item[corbomite-danger="true"] { color: <danger-red>; }`.
- **Widget-driven:** Custom `QWidgetAction` with a `QLabel` painted with a danger palette.

Theme approach is simpler. ~0.5 day.

### 5. CanvasFileView inheritance from EditableFileView **(deferred — view hierarchy refinement)**

**Gap.** Obsidian's class hierarchy pairs `FileView` + `EditableFileView` always — every file view inherits Rename/Move/Delete reactively. Corbomite's `CanvasFileView` inherits `FileView` but *not* `EditableFileView`, so Rename/Move/Delete are not on the Canvas hamburger (by design — spec §4.2 task 2 flagged this). In Obsidian, Canvas *does* get Rename/Move/Delete (they're file operations, not text operations).

**Impact.** Canvas users opening the hamburger don't see Rename/Move/Delete there. They can still rename via File Explorer (once #1 lands) or via command palette.

**Resolution.** Promote `CanvasFileView` to inherit from `EditableFileView` in a future view-hierarchy cluster. The "editable" in the class name refers to file-level editability (rename, delete), not text-editability — this matches Obsidian's usage. Blocked on: confirming no CanvasFileView code path assumes text-editor shape from `EditableFileView` (likely clean since the class only wires file-ops). Design task only until then.

---

## Notes on convention absorption

- **Cluster H follow-up #2** (migrate 5 menu construction sites to `MenuSectionHelper`): partially absorbed by R's P1 substrate alignment + P3 CanvasScene + MarkdownView rewires. Residue: EditorViewSpace tab bar + TextControl + CorbomiteMDI Sidebar — these still construct `QMenu` directly without `MenuSectionHelper`. Not blocking for Cluster R's UX goals; fold into #1 (File Explorer cluster) if they share code paths, otherwise carry as standalone H residue.
- **Cluster G follow-up #3** (`openLinkText` dispatcher): R ships the "Open linked view" submenu dispatching `<plugin>:open` commands; when G#3 lands, these can be upgraded to open side-by-side via the dispatcher.
- **Cluster G follow-up #6** (WorkspaceWindow popout): "Open in new window" hamburger slot is wired as disabled with a tooltip referencing G#6. Activates automatically when G#6 lands (no Cluster R re-work needed; just remove the `setEnabled(false)` guard).
- **Cluster S** (Bookmarks): "Bookmark…" slot is wired as disabled with tooltip. Activates automatically when S lands.
- **Cluster T** (File Recovery): "Open version history" slot wired as disabled placeholder.
- **Qutepart fork P3** (find/replace API): "Find…" / "Replace…" slots wired as disabled placeholders.
