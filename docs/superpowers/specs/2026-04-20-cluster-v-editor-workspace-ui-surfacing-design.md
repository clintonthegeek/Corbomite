# Cluster V — Editor & Workspace UI Surfacing (Design)

**Spec date:** 2026-04-20
**Scouting doc:** [`../plans/2026-04-20-cluster-v-editor-workspace-ui-surfacing-SCOUTING.md`](../plans/2026-04-20-cluster-v-editor-workspace-ui-surfacing-SCOUTING.md)
**Companion overflow cluster:** V.2 (debt cleanup) — scouting doc at [`../plans/2026-04-20-cluster-v2-debt-cleanup-SCOUTING.md`](../plans/2026-04-20-cluster-v2-debt-cleanup-SCOUTING.md)

## 1. Motivation

A 2026-04-20 dead-code audit (5 parallel Explore agents) confirmed that ~50 features are built-per-spec but never reach the user. This spec closes the **user-visible** gaps for the editor + app-shell + workspace layer. Non-visible plumbing (VaultConfig writer routing, persisted metadata cache loader, autosave delay setting, dead-code audit pass) is deferred to Cluster V.2 so it does not slow the user-facing wins.

**Framing.** Cluster V is *surface-first*: if a menu item exists but does nothing, fix it. If a built-in method has no caller but also no user-visible absence, queue it for V.2. Nothing is dropped; everything lives in V.2's scouting doc + `backlog.md`.

## 2. Scope

### In scope (ship in V)

- **Phase 1** — Dead app-shell actions: `edit_find`, `view_zoom_{in,out,reset}`, `aboutApp`, `aboutKDE`, define missing `editor_toggle_mode` action, apply `Appearance/Theme` via `KColorSchemeManager`, apply `Files/TrashOption` + `Files/PromptDelete` in `FileManager::promptForDeletion`.
- **Phase 2** — Surface all 29 `Markoff::Editor` `ActionId`s in `corbomiteui.rc.in` via new Format / Heading / Insert / Table menus plus Edit > Find/Replace and View > Fold entries. Adds `Editor::cursorInTable()` accessor. Introduces `View::zoomIn/zoomOut/zoomReset` base virtuals; implements them in `MarkdownView`, `SourceEditorView`, `ReadingView`. Callout picker dialog (26 types) + Insert-Table dialog.
- **Phase 3** — Fold **actions only** (Fold All / Unfold All / Toggle Fold at Cursor) pulled through Phase 2's menu path. Gutter click-to-fold → V.2.
- **Phase 4** — ReadingView interactions: HeadingItem click → `toggleFold`, `wikiLinkHovered` → `HoverPopover` wiring, route `BlockKind::DisplayMermaid` dispatch through `codeBlockProcessorRegistry`.
- **Phase 5** — Workspace power-features: Split (`Ctrl+\`, `Ctrl+Shift+\`), Popout (View > Move to new window), Linked-pane toggle (View > Link with active pane), Ctrl+Shift+T reopen-closed, Ctrl+Shift+PgUp/PgDn tab move.
- **Phase 6** — Search UI completion: regex + match-case toggles, remove "coming soon" tooltip. Notice-toast surfacing for 5 swallowed-failure sites.

### Out of scope (queued to V.2)

- Fold gutter click-to-fold completion (Markoff-internal coordinator wiring).
- VaultConfig writer routing (6 writers × settings pages).
- Persisted metadata cache loader (`CachedMetadataStore::loadInto` / `saveFrom`).
- Autosave delay setting (`AutosaveReactor::setDelayMs` wiring).
- Phase 8 audit pass (post-landing dead-code sweep).
- Any LRU-reopen upgrade beyond single-LIFO.

### Permanently out of scope

- Plugin-API dead methods (`VaultProxy::modifyBinary/append/process/create/...`, advanced `FileManagerProxy`, `SearchProxy` link/tag queries, `secrets()/process()/network()` proxies). Per user direction 2026-04-20: defer to real third-party plugin development.
- Bases discoverability (no menu entry to create/open `.base` files). Deferred to Cluster M or a focused follow-up.
- Canvas / graph affordances (split into Cluster W).

## 3. Architectural decisions

### 3.1 `View` base class zoom contract

Add three virtual methods to `libs/core/include/corbomite/core/View.h` with no-op defaults:

```cpp
virtual void zoomIn() {}
virtual void zoomOut() {}
virtual void zoomReset() {}
```

Override in:
- `MarkdownView` → forwards to `Markoff::Editor::action(ActionId::ZoomIn/Out)->trigger()` + a new `Editor::resetZoom()` method (or `setFontSize` to default).
- `SourceEditorView` → calls corresponding `Corbomite::SourceEditor` methods (add if missing; Qutepart has font-zoom internals to expose).
- `ReadingView` → scales the root `QGraphicsView` zoom factor; emits `zoomChanged`.

`WorkspaceController` routes `view_zoom_*` shell actions to `workspace->activeLeaf()->view()->zoomIn()` etc. Views that don't zoom silently no-op (matches Obsidian graph view).

### 3.2 Theme wiring

Add `find_package(KF6ColorScheme REQUIRED)` to `CMakeLists.txt` and link `KF6::ColorScheme` where the applier lives (`src/app/MainWindow.cpp`).

At startup (after reading kcfg) and on `SettingsDialog` apply:

```cpp
auto *mgr = KColorSchemeManager::instance();
const QString theme = CorbomiteSettings::self()->theme();
if (theme == QLatin1String("system")) {
    mgr->activateScheme(QModelIndex()); // unset → track OS
} else {
    const QString schemeId = (theme == QLatin1String("dark"))
        ? QStringLiteral("BreezeDark")
        : QStringLiteral("BreezeLight");
    mgr->activateScheme(mgr->model()->indexForSchemeId(schemeId));
}
```

The `SettingsDialog` apply-handler emits `CorbomiteSettings::configChanged` (already exists via `KConfigSkeleton`); `MainWindow::onSettingsApplied()` (new slot) dispatches to the theme applier. Single choke point.

### 3.3 `editor_toggle_mode`

Defined in `MainWindow::setupActions()`:

```cpp
auto *act = actionCollection()->addAction(QStringLiteral("editor_toggle_mode"));
act->setText(i18n("Toggle Editor Mode"));
act->setIcon(QIcon::fromTheme(QStringLiteral("view-preview")));
actionCollection()->setDefaultShortcut(act, QKeySequence(QStringLiteral("Ctrl+E")));
connect(act, &QAction::triggered, this, &MainWindow::cycleEditorMode);
```

`MainWindow::cycleEditorMode()` bridges to the active leaf's `NoteEditorWidget`: cycles `Source → LivePreview → Reading → Source` by reading current mode and calling `setViewMode()`.

Additionally, a **View > Editor Mode** submenu with three radio-exclusive actions allows direct select. Enabled only when active view is `MarkdownView`.

### 3.4 Menu structure (corbomiteui.rc.in)

```
Edit:   (existing)  / — / Find / Find Next / Find Previous / Replace
View:   (existing) / — / Editor Mode ▸ / Fold All / Unfold All / Toggle Fold /
        — / Zoom In / Zoom Out / Zoom Reset /
        — / Split right / Split down / Move to new window / Link with active pane
Format: Bold / Italic / Strikethrough / Inline code /
        — / Insert Link / Insert Wiki Link / Insert Image /
        — / Code block / Block quote / Horizontal rule
Heading: H1 / H2 / H3 / H4 / H5 / H6 / — / Increase level / Decrease level
Insert: Table… / Callout… / Checkbox
Table:  (cursor-gated) Insert row above / below / Insert column left / right / Delete row / Delete column
```

Enable rules:
- Whole Format / Heading / Insert / Table menus gated on `activeView isa MarkdownView`.
- Table submenu additionally gated on `Editor::cursorInTable()`.
- Enable-state refreshed on `Workspace::activeLeafChanged`.

H1–H6 implemented as a `QActionGroup` with `setExclusive(true)`; each binds to a new `Markoff::ActionId::SetHeading{1..6}` (see §5 new primitives). Default shortcuts `Ctrl+1`…`Ctrl+6` (Obsidian-match). Checkmark updates on cursor-moved signal by reading current heading level.

### 3.5 Settings → live application plumbing

Single `MainWindow::onSettingsApplied()` slot connected once at construction to `CorbomiteSettings::configChanged` (KConfigSkeleton signal). It dispatches to:

- `applyTheme()` (this cluster, Phase 1)
- `applyAutosaveDelay()` (V.2)
- Future appliers added as a single call in the same slot.

This gives one grep-able choke point.

### 3.6 Phase 6 toast sites

The spec nominates 7 candidate call sites; the plan commits to wiring **5 of 7** (author's choice):

1. `Vault::saveDocument` failure (currently swallowed).
2. Plugin load failure in `PluginManager` (currently `qWarning`-logged only).
3. `VaultScanner` / watcher external-change notification (informational toast).
4. `CachedMetadataStore::open` failure (graceful degrade notification).
5. Unknown view-type fallback (EmptyView spawn) — informational.
6. VaultConfig write failure (if any writers are called in V; otherwise defer).
7. Export/print failures (if surfaced).

Each toast uses `new Notice(msg, 4000)` or `setAction(label, callback)` for actionable ones (e.g., "Save failed: Retry?"). Stacking is already implemented (commit `56d0db85`).

## 4. Phase ordering

```
Phase 1 (app-shell)
  └─► Phase 2 + Phase 3 (menus + fold actions)
        ├─► Phase 4 (ReadingView)      ╮
        ├─► Phase 5 (Workspace)         ├─► parallelisable
        └─► Phase 6 (search + toasts)  ╯
                                       └─► V closeout (handoff doc + V.2 scouting + PROJECT-STATE + backlog)
```

Commit count target: 5 feature commits + 1 closeout/docs commit. `superpowers:subagent-driven-development` can be used to parallelise P4/P5/P6 at execution time.

## 5. Primitive inventory

### Reused (no changes)

- `Markoff::Editor::action(ActionId)` — 29 ActionIds, all fully implemented with shortcuts.
- `Markoff::SearchBar` — feature-complete.
- `Workspace::splitLeaf / popoutLeaf / reparentToMain / propagatePinToGroup / findOrCreateUnpinnedLeaf / windows` — all implemented and tested.
- `TabModel::moveTab / closeOtherTabs / lruSortedPaths / reopenLastClosed`.
- `WorkspaceTabs::requestClose{Tab,Others,ToRight,All}` + `View::onTabMenu` hook (context-menu residue shipped in 2026-04-19 odd-jobs).
- `HoverPopover` + `EmbedRenderer` (Cluster J).
- `Notice` widget with stacking (Cluster H + 2026-04-19 stacking commit).
- `ReadingView::toggleFold / foldedHeadings / codeBlockProcessorRegistry`.
- `ReadingMathObject::drawObject` — DisplayProperty already honored; drop from scope.
- `KStandardAction`, `KAboutApplicationDialog`, `KAboutKdeDialog` (KF6).
- `KColorSchemeManager` (system-available).
- `KConfigSkeleton::configChanged` signal.

### New primitives (this cluster)

- `View::zoomIn/Out/Reset` base virtuals + three concrete overrides.
- `Editor::cursorInTable()` accessor on `Markoff::Editor`.
- `Editor::resetZoom()` (or equivalent: `setFontSize(defaultSize)` path).
- `Markoff::ActionId::SetHeading1…SetHeading6` — six new ActionIds + implementations (call existing heading-level setter with target level). Needed for direct-select H1–H6 menu items (Ctrl+1…Ctrl+6 per Obsidian defaults). ~30 LOC addition in `Editor::createActions()`.
- `Corbomite::SourceEditor` zoom methods (`zoomIn/Out/resetZoom`) — add if missing; Qutepart exposes font-zoom internals.
- `MainWindow::cycleEditorMode()` slot + matching View > Editor Mode submenu.
- `MainWindow::onSettingsApplied()` slot (single dispatcher).
- `MainWindow::applyTheme()` helper.
- `CalloutPickerDialog` — 26 callout types listed in `obsidian-audit/01-markoff-gaps.md §Callouts`; trivial `QComboBox` + preview label.
- `InsertTableDialog` — rows + columns spinboxes + "first row as header" checkbox.
- `ReadingView::linkHovered(QString)` signal — rename / alias of existing `wikiLinkHovered`; preserves existing emit sites.
- `WorkspaceController::splitActive(Qt::Orientation)` / `popoutActive()` / `toggleLinkActive()` / `reopenClosed()` / `moveActiveTab(int direction)` helper slots (thin wrappers so shortcut actions have single connect targets).

## 6. Data flow

### Phase 1 zoom flow
```
user presses Ctrl+=
  → QAction view_zoom_in triggered
  → MainWindow::onZoomIn()
  → WorkspaceController::zoomActive()
  → workspace->activeLeaf()->view()->zoomIn()
  → (MarkdownView) m_editor->action(ActionId::ZoomIn)->trigger()
  → Markoff::Editor setFontSize delta
```

### Phase 1 theme flow
```
user toggles Dark in Settings → apply
  → CorbomiteSettings::self()->save()
  → configChanged signal
  → MainWindow::onSettingsApplied()
  → applyTheme() reads kcfg, calls KColorSchemeManager::activateScheme
  → QApplication::paletteChanged propagates to all widgets
```

### Phase 2 menu enable state
```
Workspace::activeLeafChanged
  → MainWindow::refreshEditorActions()
  → iterate Format/Heading/Insert/Table menus
    → setEnabled(activeView isa MarkdownView)
  → Table submenu additionally setEnabled(editor->cursorInTable())
  → heading group exclusive: set checked on matching level
```

### Phase 4 ReadingView click-to-fold
```
QGraphicsView mousePressEvent
  → ReadingView::mousePressEvent
  → hit-test fold-arrow QGraphicsPolygonItem
  → item->data(kFoldArrowSectionIdxProperty).toInt()
  → toggleFold(sectionIdx)
  → SectionLayout::relayout + emit foldedHeadingsChanged
  → NoteEditorWidget::persistEphemeralState (already wired)
```

### Phase 6 search toggles
```
user clicks regex ▣ toggle in SearchBar
  → SearchBar::regexToggled(bool)
  → SearchView::setParseOptions({.regex = true})
  → next executeSearch() passes opts to SearchDSL::compile
  → CompiledPlan populates regexPatterns
  → SQLiteIndex::searchCompiled post-filters
```

## 7. Error handling / edge cases

- **Theme "system" + headless session:** `KColorSchemeManager::activateScheme(QModelIndex())` works in all environments; no palette change attempted if OS can't be queried.
- **Zoom on non-zoomable view:** no-op per base virtual default. No user feedback (consistent with Obsidian).
- **Find / Replace on `ReadingView`:** gated — only emit find-bar show on MarkdownView/SourceEditorView; Reading view returns a disabled state. Do not crash.
- **Split on a leaf with unsaved state:** no special handling; `splitLeaf` clones ephemeral state per Cluster G contract.
- **Popout then close the popout window:** existing `Workspace::reparentToMain` or app-shutdown flow handles; not a new concern.
- **Ctrl+Shift+T with empty close-history:** no-op; silently succeeds.
- **Callout / Insert Table dialog dismissed:** no-op; no text inserted.
- **`Editor::cursorInTable()` at document start / between tables:** returns `false`; Table submenu disables.
- **Toast shown from non-GUI thread:** route through `QMetaObject::invokeMethod` with `Qt::QueuedConnection` so `Notice` always instantiates on GUI thread.
- **KXMLGUI action-collection race on startup:** `editor_toggle_mode` must be `addAction`'d before `setupGUI(Default, corbomiteui.rc)` is called (existing order in MainWindow ctor; no change needed).

## 8. Testing strategy

**Unit + integration tests (CTest):**
- `tst_view_zoom` — stub `View` subclass asserts virtual dispatch; MarkdownView unit ensures zoom forwards to Editor.
- `tst_editor_cursor_in_table` — 4 fixtures: outside-table / first-cell / last-cell / across-tables boundary.
- `tst_mainwindow_action_wiring` — introspect `actionCollection()` for every action added in Phases 1/2/3/5; assert not-null + shortcut bound.
- `tst_reading_view_click_to_fold` — offscreen `ReadingView`, synth mouse press on arrow hit-test, assert `foldedHeadings` contains the index.
- `tst_search_options_regex` — set regex toggle on, issue query, assert `CompiledPlan::regexPatterns` populated.
- `tst_theme_applier` — activate dark / light / system, assert `QApplication::palette()` diverges.

**Manual smoke tests (documented in plan, run before commit):**
- Launch app, no `KXMLGUI` warnings in stderr re `editor_toggle_mode`.
- All new menu entries reachable with mouse + keyboard; disabled/enabled transitions correct.
- Toggle theme in Settings, observe instant redraw.
- Ctrl+E cycles three modes.
- Insert Callout / Insert Table dialogs render correctly and produce valid markdown.
- Split + Popout + Close cycle preserves open files.

**No tests added for:**
- Callout / Insert-Table dialog internals (UI-only).
- Notice toast sites (integration tested via manual smoke; unit tests on swallowed-error paths are impractical).

## 9. Risks and mitigations

| Risk | Mitigation |
|---|---|
| `KColorScheme` dependency not in current Corbomite CMake → build break | One-line `find_package` add; verified package installed on dev machine. Falls back gracefully if somehow missing (theme stays "system"). |
| Menu enable-state bugs on rapid leaf switching | `refreshEditorActions` is O(~40 action-enables); cheap even if over-invoked. No debounce needed. |
| `editor_toggle_mode` cycle gets out of sync with radio submenu | Single source of truth: `NoteEditorWidget::viewMode()` read-before-cycle; submenu updates on `viewModeChanged` signal. |
| `Workspace::popoutLeaf` edge cases (Cluster G shipped it but no UI exercised it before) | Cluster G's `tst_workspace_window` covers it; manual smoke-test with multi-window session during Phase 5. |
| Toast routing from non-GUI threads crashes | Route through `QMetaObject::invokeMethod` pattern. Unit-untestable; manual smoke with plugin-load failure. |
| Overlap with in-flight Qutepart-fork Phase 3 (public find API) | MarkdownView uses `Markoff::Editor::Find`; SourceEditorView needs find action too — if fork Phase 3 hasn't landed by the time Cluster V Phase 2 executes, the SourceEditorView Find action is a no-op stub with a tooltip "Find in Source mode coming soon (blocked on Qutepart fork Phase 3)". Non-blocking. |

## 10. Success criteria

Cluster V is "done" when:

1. Every menu entry added in Phases 1–6 has a working effect or is correctly disabled (no click → nothing).
2. `ctest --output-on-failure -j 10` passes, with the new tests listed in §8 green.
3. `grep KXMLGUI` in startup log shows no "unknown action" for `editor_toggle_mode` or any Phase 2 action.
4. Manual smoke checklist (§8) passes in a vault with ≥20 notes + 2 canvases.
5. Handoff doc committed at cluster retros path with links to V.2 scouting, backlog entries, and a punch list of skipped items.
6. PROJECT-STATE updated per ritual: ≤3 sentences in §Current focus; full closeout paragraph in `decisions-archive.md`.
7. V.2 scouting doc referenced from PROJECT-STATE roadmap table; every deferred item has a backlog entry.

## 11. Handoff to V.2

Everything omitted from V that was in the original scouting doc is captured in `../plans/2026-04-20-cluster-v2-debt-cleanup-SCOUTING.md` and cross-linked from `backlog.md`. At V closeout, the retro writes a one-line pointer per deferred item confirming it's tracked.

---

## Appendix A — Callout types (for picker dialog)

From `obsidian-audit/01-markoff-gaps.md §Callouts` (26 types): `note`, `abstract`, `summary`, `tldr`, `info`, `todo`, `tip`, `hint`, `important`, `success`, `check`, `done`, `question`, `help`, `faq`, `warning`, `caution`, `attention`, `failure`, `fail`, `missing`, `danger`, `error`, `bug`, `example`, `snippet`.

Dialog UX: `QComboBox` with type name + a preview label that updates live showing `> [!type] title\n> body`.

## Appendix B — File-level change inventory (rough)

| File | Phase | Change |
|---|---|---|
| `CMakeLists.txt` | 1 | Add `find_package(KF6ColorScheme REQUIRED)` |
| `src/app/MainWindow.{h,cpp}` | 1,2,3,5 | setupActions additions; new slots (cycleEditorMode, onSettingsApplied, applyTheme, refreshEditorActions); menu-enable-state glue |
| `src/app/WorkspaceController.{h,cpp}` | 2,5 | zoomActive, splitActive, popoutActive, toggleLinkActive, reopenClosed helpers |
| `src/app/corbomiteui.rc.in` | 1,2,3,5 | Full menu layout per §3.4 |
| `src/app/corbomite.kcfg{,c}` | 1 | No key changes; ensure readers populate widgets in SettingsDialog apply path (review only) |
| `libs/core/include/corbomite/core/View.h` | 2 | Add zoomIn/Out/Reset virtuals |
| `libs/markoff-family/libs/markoff/include/markoff/ActionId.h` | 2 | Add `SetHeading1`…`SetHeading6` enum values |
| `libs/markoff-family/libs/markoff/include/markoff/Editor.h` + `.cpp` | 2 | Add `cursorInTable()`, `resetZoom()`, wire new SetHeading1..6 actions |
| `libs/editor/SourceEditor.{h,cpp}` (qutepart-corbomite) | 2 | Add `zoomIn/zoomOut/resetZoom` methods if missing |
| `libs/readingview/...ReadingView.h` + `.cpp` | 4 | Add `linkHovered` signal (alias/rename); mousePressEvent hit-test arrow |
| `libs/readingview/src/SectionLayout.cpp` | 4 | DisplayMermaid → codeBlockProcessorRegistry dispatch |
| `src/editor/NoteEditorWidget.{h,cpp}` | 4 | Wire ReadingView linkHovered → HoverPopover |
| `src/plugins/search/SearchView.cpp` + SearchBar UI files | 6 | Regex/case-sensitive toggle buttons; remove "coming soon" tooltip |
| `src/vault/FileManager.cpp` | 1 | Read TrashOption + PromptDelete kcfg keys in promptForDeletion |
| `src/editor/NoteEditorWidget.cpp` | 1 | Expose mode-cycle helper or read viewMode() for MainWindow |
| `src/dialogs/CalloutPickerDialog.{h,cpp}` | 2 | NEW |
| `src/dialogs/InsertTableDialog.{h,cpp}` | 2 | NEW |
| `tests/core/tst_view_zoom.cpp` | 2 | NEW |
| `tests/markoff/tst_editor_cursor_in_table.cpp` | 2 | NEW |
| `tests/app/tst_mainwindow_action_wiring.cpp` | 1,2,5 | NEW |
| `tests/readingview/tst_click_to_fold.cpp` | 4 | NEW |
| `tests/search/tst_search_options_regex.cpp` | 6 | NEW |
| `tests/app/tst_theme_applier.cpp` | 1 | NEW |
| 5–7 toast-site source files | 6 | Add `Notice` instantiation on failure paths |

## Appendix C — Open spec items at time of writing

None. All blockers listed in the scouting doc have been resolved:

- **KColorSchemeManager availability:** CONFIRMED (installed system-wide; one-line CMake add).
- **Qutepart fork Phase 6 sequencing:** SCOUTING DOC WAS WRONG — fork Phase 6 is about themes, not gutter. Real question (fold-gutter coordinator wiring) is a Markoff-internal issue deferred to V.2.
- **Overwrite-unknown-keys pattern for VaultConfig writers:** not in V scope (all moved to V.2); V.2 spec will formalise the pattern before wiring the writers.
