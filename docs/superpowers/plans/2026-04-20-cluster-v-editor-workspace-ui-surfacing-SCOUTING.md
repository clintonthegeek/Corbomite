# Cluster V — Editor & Workspace UI surfacing (SCOUTING)

**Type:** Scouting doc (not yet dispatchable). Expand to a full plan once the user confirms scope and any blockers below are resolved.

**Motivation.** A 2026-04-20 dead-code audit (5 parallel Explore agents over the editor stack, app shell, vault/storage, plugin system, and canvas/graph) confirmed a recurring pattern: features have been built per `docs/obsidian-audit/` spec but never reached the user — either no menu entry / shortcut / dialog exposes them, or a menu entry exists but is wired to a no-op. The most recent example was `Corbomite::EmptyView`, which lived in `libs/core/` for an entire cluster cycle without an instantiation path until commit `ef7fb86b`.

This cluster closes that gap for the **editor + app-shell + workspace** layer. (Canvas / graph affordance gaps are factored into [Cluster W](2026-04-20-cluster-w-canvas-graph-affordances-SCOUTING.md). Plugin-API surface dead methods are deferred to whenever real plugin development consumes them.)

---

## Audit references

- 2026-04-20 audit transcripts (in conversation; not persisted) — five Explore agents with non-overlapping scopes returned a unified punch list. Findings cited below by `file:line`.
- [`../../obsidian-audit/01-markoff-gaps.md`](../../obsidian-audit/01-markoff-gaps.md) — original spec for editor formatting actions, fold gutter, callouts, search bar, table ops.
- [`../../obsidian-audit/domains/leaf-utilities.md`](../../obsidian-audit/domains/leaf-utilities.md) — canonical menu section order (reused).
- [`../../cluster-retros/cluster-e.md`](../../cluster-retros/cluster-e.md) — three-mode pivot retro (mode-toggle action lives here).
- [`../../cluster-retros/cluster-g.md`](../../cluster-retros/cluster-g.md) — Workspace retro (popout / split / linked-pane methods live here).

---

## Scope (rough phasing — revisit on plan expansion)

### Phase 1 — Kill or wire dead app-shell actions

Tier-1 findings: menu entries / shortcuts the user can click *today* that do nothing. Each must be either hooked up or removed (don't ship dead UI).

- `src/app/MainWindow.cpp:784-786` — `KStandardAction::find` is connected to an empty lambda. Wire to active `View::find()` (Markdown/Source/Reading already implement find slots; route through `Workspace::activeView`).
- `src/app/MainWindow.cpp:798-808` — `view_zoom_in` / `view_zoom_out` / `view_zoom_reset` (Ctrl+= / Ctrl+- / Ctrl+0) have no slots. Wire to `View::zoomIn()` / `zoomOut()` / `zoomReset()` via `WorkspaceController`. Markoff `Editor`, `SourceEditor`, and `ReadingView` all already have zoom plumbing internally.
- `src/app/MainWindow.cpp:788-789` — `KStandardAction::aboutApp` / `aboutKDE` connected to `[]() {}`. Replace with `KAboutApplicationDialog` / `KAboutKdeDialog::exec()`.
- `src/app/corbomiteui.rc.in:40,63` — `editor_toggle_mode` referenced by menu + toolbar but never defined in `MainWindow::setupActions()` → KXMLGUI raises "unknown action" at startup. Define the action and wire to `NoteEditorWidget::cycleViewMode()` (Source ↔ LivePreview ↔ Reading).
- `src/app/corbomite.kcfg:48-52` — `Appearance/Theme` (light/dark/system) is persisted by `SettingsDialog` but never applied. Wire on settings save + on app startup via `KColorSchemeManager` (KF6) or fall back to manual `qApp->setStyle()` for now.
- `src/app/corbomite.kcfg:37-45` — `Files/TrashOption` and `PromptDelete` saved by `SettingsDialog` but `FileManager::promptForDeletion` ignores both. Read at delete time; respect `TrashOption=DontUseTrash` to bypass `VaultTrash` and use `QFile::remove`; respect `PromptDelete=false` to skip the modal.

### Phase 2 — Surface Markoff editor actions

Tier-2 findings: `Markoff::Editor` registers ~25 `QAction`s internally (Bold, Italic, Strikethrough, InlineCode, InsertLink, InsertWikiLink, InsertImage, InsertCodeBlock, InsertBlockQuote, InsertHorizontalRule, IncreaseHeading, DecreaseHeading, ToggleCheckbox, InsertTable, FoldAll, UnfoldAll, ToggleFold, Find, FindNext, FindPrevious, Replace, ShowSearchBar, ShowReplaceBar, plus 6 table row/column ops). The host (`MainWindow`) never calls `Editor::action(ActionId)` to retrieve any of them, so they have no menu / toolbar / shortcut path.

- Extend `corbomiteui.rc.in` with **Format** menu (Bold / Italic / Strikethrough / Inline code / Link / Image / Code block / Block quote / Horizontal rule), **Heading** submenu (H1–H6 + Increase / Decrease), and **Insert** menu (WikiLink / Table / Callout / Checkbox).
- Add **Edit > Find / Find Next / Find Previous / Replace** entries; wire `KStandardAction::find` (Phase 1) and `KStandardAction::findNext` to `Editor::action(ActionId::Find)` etc. SearchBar widget (`libs/markoff-family/libs/markoff/include/markoff/SearchBar.h`) is already complete; just needs the show signal wired.
- Add **View > Fold All / Unfold All / Toggle Fold at Cursor** with default shortcuts (Ctrl+Shift+- / Ctrl+Shift+= / Ctrl+. — match Obsidian).
- For each action: pull from `Editor::action()` when the active leaf's `View` is a `MarkdownView`; disable when the active view doesn't expose them. Use `Workspace::activeLeafChanged` to refresh enable state.
- **Insert Callout** needs a small picker dialog (12 callout types from `obsidian-audit/01-markoff-gaps.md §Callouts`); wrap `Editor::insertCallout(type)`.
- **Insert Table** needs a rows/columns dialog before calling existing table-insert internals.
- Table row/column ops are currently only reachable via right-click context menu (`Editor.cpp:625-633`); leave the context menu in place but also expose under **Table** submenu when cursor is in a table. Use `Editor::cursorInTable()` (add accessor if missing) for enable state.

### Phase 3 — Fold gutter completion

`libs/markoff-family/libs/markoff/src/FoldGutter.cpp:70` has `paint()` returning early with TODO; arrow glyphs render but interaction is incomplete. `src/editor/SourceEditor.cpp:80-95` stores `m_pendingFoldedHeadings` for round-trip but never applies them to Qutepart (TODO marked phase-4/phase-7 of the qutepart-corbomite fork).

- Decide: complete `FoldGutter::paint()` independently in Markoff, **or** defer until Qutepart-fork Phase 6 (gutter fold arrow with themed QIcon) lands and align both editors on one implementation.
- Recommended: defer — Qutepart fork Phase 6 is the agreed substrate; finishing Markoff's fold gutter independently risks divergent UX.
- Within this cluster, ship `Markoff::Editor` heading-fold *actions* (Phase 2 above) without the gutter; click-to-fold via gutter waits on the fork.

### Phase 4 — ReadingView heading collapse + hover popover

- `libs/readingview/include/corbomite/readingview/ReadingView.h:79` `toggleFold(sectionIdx)` exists with full state machinery (`foldedHeadings` / `setFoldedHeadings` round-trip via `NoteEditorWidget.cpp:225/257`) but no click handler on heading widgets calls it. Add a click area / disclosure triangle to each `HeadingItem` in `ReadingView::SectionLayout`.
- `libs/readingview/include/corbomite/readingview/ReadingView.h:92` `codeBlockProcessorRegistry()` is registered with built-in mermaid/math/syntax-highlighters but `SectionLayout` calls renderers directly (`SectionLayout.cpp:903` for `BlockKind::DisplayMermaid`). Route block dispatch through the registry so plugin-registered processors (deferred to plugin-API work) become reachable. Two-line change.
- `src/editor/HoverPopover.cpp` is wired only for Markoff editor mode (`NoteEditorWidget.cpp:52-63`). Add the same `linkHovered → HoverPopover::scheduleShow` connection for `ReadingView` (it has the same signal). Hooks up Cluster J's embed-preview popover for the reading mode that needed it most.
- `libs/readingview/src/ReadingMathObject.h:36` `DisplayProperty` flag is set when math is `$$…$$` block but `drawObject()` never reads it; block math renders inline. Honour `DisplayProperty` in `drawObject` (centre, own line, larger size).

### Phase 5 — Workspace power-features

`libs/core/include/corbomite/core/Workspace.h` declares `splitLeaf()`, `popoutLeaf()`, `reparentToMain()`, `windows()` (line 57, 65–67) and `propagatePinToGroup()` / `findOrCreateUnpinnedLeaf()` (line 76–78) but no UI invokes any of them. Cluster G shipped the implementations.

- **Split pane:** add `Ctrl+\` (vertical split) and `Ctrl+Shift+\` (horizontal split) bound to `Workspace::splitLeaf(activeLeaf, Direction)`. Already in Cluster R hamburger as "Split right / Split down" — hoist the same actions to global shortcuts and the **View** menu.
- **Popout to window:** add **View > Move to new window** + entry in Cluster R hamburger (currently a disabled placeholder for `Cluster G#6`). Wire to `Workspace::popoutLeaf(activeLeaf)`. `reparentToMain()` and `windows()` come along free.
- **Linked-pane group / pin propagation:** add **View > Link with active pane** toggle. Use `Workspace::propagatePinToGroup` when toggled on. UX: pinned leaves with the same group ID stay locked together when one navigates.
- **LRU reopen + tab move + close-others:** `libs/models/include/corbomite/models/TabModel.h:41,44,58,61` — `moveTab`, `closeOtherTabs`, `lruSortedPaths`, `reopenLastClosed` all exist. Add **File > Reopen closed tab** (Ctrl+Shift+T), tab context-menu **Close others / Close to the right** (already partly wired by Cluster G follow-up odd-jobs sweep — check residue), and drag-reorder for tabs (already present via Qt; just expose via shortcut Ctrl+Shift+PgUp/PgDn for keyboard tab-move).

### Phase 6 — Search UI completion + toast notifications

- `SearchView.cpp:113-122,157` literal banner says *"regex, line:, block:, section: coming soon"* — but the 2026-04-19 odd-jobs sweep already shipped regex + case-sensitive backend (`SearchDSL::CompiledPlan::regexPatterns` + `caseSensitiveTerms` post-filter through `SQLiteIndex::searchCompiled` overload). UI just needs:
  - Two toggle buttons in the search bar (regex on/off, match-case on/off) bound to `SearchDSL::ParseOptions`.
  - Remove the "coming soon" banner.
  - Defer `line:` / `block:` / `section:` operators (no parser support yet — separate work).
- `src/dialogs/Notice.h` — toast notification widget shipped in Cluster H Phase 5, never instantiated. Wire to:
  - File save errors (currently swallowed — see `Vault::saveDocument` callers).
  - Vault watcher external-change notifications.
  - Plugin load failures (currently logged only).
  - Any `qWarning()` site that should reach the user.

### Phase 7 — Settings reachability + persisted-cache loader

- `Storage::CachedMetadataStore::loadInto()` + `MetadataCache::installPersistedState()` — implemented but never invoked at startup. Wire `MainWindow::openVault` to call the loader before `MetadataWorker::scanVault`, so a previously-indexed vault opens fast. Persist on shutdown via the symmetric `CachedMetadataStore::saveFrom`.
- `VaultConfig` writers (`writeAppJson`, `writeAppearanceJson`, `writeCommunityPlugins`, `writeHotkeys`, `writeDailyNotesJson`, `writeTemplatesJson` — `VaultConfig.h:49-77`) — never called. Audit each: route the relevant `SettingsDialog` page's apply-handler through the matching writer, so vault-portable settings actually persist into `.obsidian/`.
- `AutosaveReactor::setDelayMs()` — never called; delay hardcoded to 2000 ms. Expose under **Settings > Editor > Autosave delay**.

### Phase 8 — Audit pass + remove ceremony

After Phases 1–7 land:

- Re-run a focused dead-code grep over `src/app/`, `libs/markoff-family/`, `libs/readingview/`, `libs/core/`, `libs/storage/`, `libs/models/`. Anything still dead either gets wired or deleted. No leniency.
- Update `corbomite.kcfg` to remove keys that no Settings page surfaces *and* no code reads.
- Delete fully-dead public methods identified by audit (cited in the Tier-2 / Tier-3 punch list) where Phases 1–7 didn't claim them.

---

## Primitive inventory

**Reused:**

- `Markoff::Editor::action(ActionId)` — full action registry already exists.
- `SearchBar` widget — feature-complete.
- `Workspace::splitLeaf` / `popoutLeaf` / `propagatePinToGroup` — Cluster G.
- `TabModel::moveTab` / `closeOtherTabs` / `reopenLastClosed` — Cluster G + odd-jobs sweep.
- `HoverPopover` + `EmbedRenderer` — Cluster J.
- `Notice` widget — Cluster H Phase 5.
- `Storage::CachedMetadataStore` — Cluster I.
- `KStandardAction`, `KColorSchemeManager`, `KAboutApplicationDialog` — KF6.

**New primitives:**

- Callout-type picker dialog (~12 entries; trivial `QComboBox`).
- Insert-Table dialog (rows × columns spinner + first-row-as-header checkbox).
- `Editor::cursorInTable()` accessor (probably already exists; confirm).
- `MainWindow::cycleViewMode()` slot bridging the new `editor_toggle_mode` action to `NoteEditorWidget`.
- `View::zoomIn/zoomOut/zoomReset` virtual methods on the `View` base + concrete impls (Markdown / Source / Reading).
- A small "settings → live application" plumbing for theme + autosave-delay (signal from `KConfigSkeleton::configChanged`).

---

## Blockers / prerequisites

1. **Qutepart-Corbomite fork Phase 6 (themed gutter fold arrow)** — Phase 3 of this cluster (fold gutter) should defer to it. Coordinate sequencing: either land fork Phase 6 first, or accept that Cluster V Phase 3 is a stub and gutter click-to-fold ships in the fork plan.
2. **`KColorSchemeManager` availability** — confirm KF6 version pinned by Corbomite ships it; otherwise fall back to manual style switching for theme application (Phase 1 finding).
3. **Permission-gated Phase 7 writers** — `VaultConfig` writers may overwrite user-edited `.obsidian/*.json` outside Corbomite's awareness. Define a "merge unknown keys, write known keys" pattern (Cluster S already did this for `bookmarks.json`; reuse).

---

## Out of scope (deferred)

- **Plugin-API dead methods** (Tier-5 from audit): `VaultProxy::modifyBinary/append/process/create/createFolder/trash/remove/on/off`, `FileManagerProxy` advanced ops, `SearchProxy` link/tag queries, `secrets()/process()/network()` proxies. Per user direction (2026-04-20): defer to whenever real third-party plugin development materialises — they are API debt only if no plugin ever calls them, but writing fake consumers now is wasted work.
- **Bases discoverability** (`libs/bases/` has no menu / sidebar entry to create or open `.base` files). Defer to Cluster M (Internal-plugin feature audits) or a focused "Bases discoverability" follow-up.
- **Canvas / graph affordances** — split into [Cluster W](2026-04-20-cluster-w-canvas-graph-affordances-SCOUTING.md).

---

## Estimate (rough)

7–10 days once expanded. Phase 1 + Phase 2 are the heaviest (action wiring across `corbomiteui.rc.in` + `MainWindow::setupActions` + Markoff retrieval + dialogs). Phases 3–7 are mostly small wiring tasks. Phase 8 audit pass is half a day if Phases 1–7 are tight.

---

## Expansion triggers

Expand to a full plan when:

1. The user confirms Cluster V is the next major UI cluster (vs Cluster S Bookmarks plan, Cluster U File Explorer, Cluster M audits).
2. Qutepart-fork Phase 6 sequencing decision is made (block Cluster V Phase 3 on it, or stub the gutter).
3. `KColorSchemeManager` availability confirmed.

Until then this scouting doc captures the audit's punch list so the context doesn't evaporate.
