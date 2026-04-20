# Backlog — Unified Map

> **Living document.** Every deferred follow-up, every not-yet-started cluster, every scouting doc, every known-flaky test. Grouped by theme, not by originating cluster. Strike items (`~~text~~`) when closed with a one-line closure + date; quarterly roll strike-throughs into `decisions-archive.md`.

## Reading order

Clusters to-do → Plugin API / extension surfaces → Editor / Views / Workspace → Search / metadata → Plugin primitives + lifecycle → Out-of-tree extractions → Cluster-specific follow-up buckets → Stability.

---

## 1. Not-started, plan-needed, and scouting clusters

### Cluster B Phase 3b — WorkspaceState app-layer wiring (Cluster B residue)
- **Source:** [Cluster B plan (archived)](superpowers/plans/archive/2026-04-14-cluster-b-vault-io.md)
- **Blocks:** nothing currently; cleans up legacy `session.json` persistence path
- **Scope:** medium (~500–1000 LOC)
- **Details:** Cluster B is **Done** (primitives landed 2026-04-14 across Phases 1–6: `DataAdapter`, `FileSystemAdapter`, `VaultConfig`, `WorkspaceState`, `CaseSensitivityProbe`, `VaultTrash`, `IgnoreFilter`, `VaultProcess`). The residual Phase 3b is app-layer wiring of `SessionManager` / `EditorViewManager` / `MainWindow` onto `WorkspaceState` rather than the current `session.json` path. Full design in the archived B plan; unblocked now that Cluster G/Q are closed. No consumer is currently forcing the switch — pick up when a feature needs per-vault workspace state that the legacy path can't provide.

### Cluster M — Internal-plugin feature audits (Graph, Canvas)
- **Source:** [INDEX.md](superpowers/plans/INDEX.md) row M
- **Blocks:** nothing currently
- **Scope:** medium
- **Details:** Two normal tasks — a feature audit of the Graph internal plugin and of the Canvas internal plugin — to check them against the Obsidian audit and log gaps. Status "Deferred". GraphView is now a KPluginFactory `.so` module (`src/plugins/graph-view/`); Canvas is still in `CorbomiteApp`. The deferred reason is that both audits are additive and non-blocking; pick them up when roadmap quiets between parity clusters.

### Cluster O — Advanced query layer (post-parity)
- **Source:** [Cluster O scouting](superpowers/plans/2026-04-14-cluster-o-query-layer-SCOUTING.md)
- **Blocks:** nothing; post-parity
- **Scope:** large
- **Details:** Scouting doc (not dispatchable). Additive graph + FTS layer over the markdown vault, sitting above the existing `SQLiteIndex`. Concrete extension points exist in `BasesEntry` / `QueryController` (Cluster K) and the Pratt formula engine; Cluster O would build the query-surface API on top of those. Intentionally deferred until parity clusters A–S land.

### Cluster P — Graffodil adoption (internal refactor)
- **Source:** [Cluster P scouting](superpowers/plans/2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md)
- **Blocks:** nothing blocking; parallelisable
- **Scope:** large
- **Details:** Scouting doc. Port `libs/forcegraph` and `libs/canvas` onto the Graffodil graph-rendering library, replacing the hand-rolled `QGraphicsScene` substrate. Parallelisable with parity work. Blocked on expanding the scouting doc into a full plan once the canvas + graph surface has stabilised post-Cluster-M audit.

### Cluster S — Bookmarks core plugin (closed 2026-04-20)
- **Source:** [retro](cluster-retros/cluster-s.md); [plan](superpowers/plans/2026-04-20-cluster-s-bookmarks.md); [spec](superpowers/specs/2026-04-19-cluster-s-bookmarks-design.md); [addendum](obsidian-audit/addenda/2026-04-19-bookmarks-core-plugin.md)
- **Status:** Done. Full plugin shipped: `src/plugins/bookmarks/` with `.obsidian/bookmarks.json` round-trip, right-dock panel with drag-reorder and Rename/Move-to-group/Delete context menu, 7 `bookmarks:*` commands (2 functional, 5 gated), `BookmarkModal`, VaultProxy rename/delete sync, Cluster R "Bookmark…" hamburger slot live.
- **Follow-up — WorkspaceController accessors for 5 stubbed commands:** `bookmarks:bookmark-all-tabs`, `bookmark-current-heading`, `bookmark-current-block`, `bookmark-current-search`, and `bookmark-current-graph` are registered with `checkCallback→false` because the plugin-facing `WorkspaceController` does not yet expose `openTabPaths()` / `activeHeading()` / `activeBlockId()` / `activeSearchQuery()` / `activeGraphOptions()`. The mutation helpers (`BookmarksPlugin::bookmarkAllTabs` / `bookmarkHeading` / `bookmarkBlock` / `bookmarkSearch` / `bookmarkGraph`) are implemented and tested — only the workspace-side wiring is missing. Once those accessors land (natural home: a Cluster V follow-up on the WorkspaceController proxy surface), swap the `stubRaw(...)` entries in `BookmarksPlugin::registerCommands` for real callbacks that call into the helpers. Tests already cover the helper logic in `tst_bookmarks_commands`.
- **Follow-up — Settings tab (deferred Task 3.3):** the plan's optional Task 3.3 (single "Back up bookmarks.json before first write each session" checkbox) is not shipped because no plugin in the tree today wires a settings tab via `createSettingsTab` / `SettingsTab`. When that plumbing lands (plugin infrastructure cluster), add a `BookmarksSettingsTab` consuming `ctx->saveData` / `loadData`.
- **Follow-up — `MarkdownView::ensureBlockIdAtCursor()` auto-insert:** spec default is "don't auto-insert; show a Notice when `bookmark-current-block` fires with no block id under cursor." When a second feature wants this helper (e.g. a link-to-block action), add the editor method and switch `bookmarks:bookmark-current-block`'s behavior.
- **Follow-up — "Remove all broken bookmarks" bulk action:** the `unknownKeys["_orphaned"]` marker is already plumbed through JSON round-trip, `VaultProxy::deletedFile` flags it. A panel-header action that walks the tree and removes flagged items would round out the stale-bookmark UX.
- **Follow-up — `WorkspaceController::openLinkText(path, subpath)` use:** `BookmarksView::onActivated` currently falls back to `openFile(path)` so heading-scoped bookmarks land on the file root. Swap for `openLinkText` when Cluster G follow-up #3 lands.

### Cluster T — File Recovery plugin (Version History)
- **Source:** [INDEX.md](superpowers/plans/INDEX.md) row T; [addendum](obsidian-audit/addenda/2026-04-19-file-recovery-plugin.md)
- **Blocks:** nothing currently; activates the disabled "Open version history" menu slot from Cluster R when built
- **Scope:** large
- **Details:** Deferred (post-parity). No plan file. Full version-history plugin analogous to Obsidian's file-recovery core plugin; records per-save snapshots accessible from the per-view hamburger's "Open version history" slot. Cluster R already injected the disabled menu entry; this cluster makes it live.

### Cluster U — File Explorer enhancements (right-click context menu + keyboard)
- **Source:** [Cluster U scouting](superpowers/plans/2026-04-19-cluster-u-file-explorer-enhancements-SCOUTING.md); [audit addendum](obsidian-audit/addenda/2026-04-19-file-explorer-context-menu.md)
- **Blocks:** nothing hard; fills the top UX gap identified during Cluster R closeout
- **Scope:** medium
- **Details:** Scouting doc (2026-04-19). Cluster R's closeout audit identified the File Explorer right-click context menu as the top remaining UX gap vs Obsidian. The scouting doc proposes reusing Cluster R primitives (`FileManager` prompt modals, `Platform`, `PathUtils`, `MenuSectionHelper`) and may absorb the Cluster H follow-up residue for `EditorViewSpace` tab bar / `TextControl` / `CorbomiteMDI Sidebar`. Estimated 3–5 days once the scouting doc is expanded to a full plan.

### Cluster V — Editor & Workspace UI surfacing (surface-first — plan-needed, spec written)
- **Source:** [Cluster V scouting](superpowers/plans/2026-04-20-cluster-v-editor-workspace-ui-surfacing-SCOUTING.md) + [spec](superpowers/specs/2026-04-20-cluster-v-editor-workspace-ui-surfacing-design.md)
- **Blocks:** Cluster V.2 (debt-cleanup follow-on)
- **Scope:** medium (~5-7 days)
- **Details:** Scope narrowed via brainstorm 2026-04-20 from the 8-phase scouting doc down to 6 surface-first phases: dead app-shell actions (Find / zoom / About / theme / trash / `editor_toggle_mode` with `Ctrl+E`), Markoff menu surfacing (Format / Heading with `Ctrl+1..6` / Insert / Table / Edit Find-Replace / View Fold) with new `View::zoomIn/Out/Reset` virtuals + `Editor::cursorInTable` + `ActionId::SetHeading1..6`, fold actions (gutter click-to-fold deferred to V.2), ReadingView interactions (click-to-fold + `linkHovered → HoverPopover` + `codeBlockProcessorRegistry` dispatch), Workspace power-features (Ctrl+\ / Ctrl+Shift+\ / popout / link / Ctrl+Shift+T), Search UI (regex + match-case toggles) + toast-surfacing for 5 swallowed-error sites. Next: writing-plans → executing-plans.

### Cluster V.2 — Editor/Workspace debt cleanup (scouting doc)
- **Source:** [Cluster V.2 scouting](superpowers/plans/2026-04-20-cluster-v2-debt-cleanup-SCOUTING.md)
- **Blocks:** nothing hard; activates only after Cluster V lands
- **Scope:** medium (~4-5 days)
- **Details:** Companion to Cluster V holding everything V deferred under the "surface-first" framing agreed 2026-04-20. Six phases: (1) fold-gutter click-to-fold — complete `FoldGutter::paint()` + wire Markoff-internal coordinator (scouting doc's "defer to Qutepart fork Phase 6" was a misread; fork Phase 6 is about themes); (2) 6× `VaultConfig` writer routing (`writeAppJson`, `writeAppearanceJson`, `writeCommunityPlugins`, `writeHotkeys`, `writeDailyNotesJson`, `writeTemplatesJson`) paired with their SettingsDialog apply-handlers via a new merge-unknown-keys helper (reuse Cluster S bookmarks precedent); (3) `CachedMetadataStore::loadInto`/`saveFrom` hookup at `MainWindow::openVault`/`closeVault` for fast cold-start; (4) Autosave delay spinbox wired into `AutosaveReactor::setDelayMs` via the `MainWindow::onSettingsApplied` dispatcher Cluster V introduces; (5) optional LRU-reopen upgrade from single-LIFO to multi-entry list; (6) post-V dead-code audit pass. Expand to full plan after V lands.

### Cluster W — Canvas & Graph affordances
- **Source:** [Cluster W scouting](superpowers/plans/2026-04-20-cluster-w-canvas-graph-affordances-SCOUTING.md)
- **Blocks:** nothing hard; coordinates with Cluster M (audit) and Cluster P (Graffodil)
- **Scope:** large (~8-12 days)
- **Details:** Scouting doc (2026-04-20). Same audit as Cluster V; split out because canvas + graph need UX design, not just menu wiring. 8 phases: canvas Link/File node creation completeness, tool palette + switching, resize handles + snap-to-grid + multi-select highlight, group operations (Ctrl+G / colour / align), force-graph physics sliders + pin/unpin UI + filter UI, `GraphViewTab` 3 unconnected signals (open-in-new-tab / reveal / delete), embed registry consolidation (mermaid double-dispatch) + media-stub backlog, audit pass. Defer real PDF / audio / video renderers.

### Qutepart-Corbomite fork — Phases 3–8 (Source editor)
- **Source:** [Fork plan](superpowers/plans/2026-04-15-qutepart-corbomite-fork.md); [spec](superpowers/specs/2026-04-15-qutepart-corbomite-fork-design.md)
- **Blocks:** Cluster E Source-mode polish; Phase 3 unblocks find/replace API consumers
- **Scope:** large
- **Details:** Phases 1 (vendor) and 2 (`Corbomite::SourceEditor` shim + visual-line float scroll) landed 2026-04-15. Six phases remain: Phase 3 (public find/replace API), Phase 4 (fractional scroll ±0.5 line precision), Phase 5 (syntax-highlight grammar registration), Phase 6 (gutter fold arrow with themed `QIcon`), Phase 7 (cursor-column preservation across Source↔LivePreview transitions — needs `Markoff::Editor::setCursor(line,col)` extension), Phase 8 (shared `Corbomite::Core::VaultResourceProvider` promotion). Not cluster-numbered; runs in parallel with parity clusters.

---

## 2. Plugin API and extension surfaces

### Migrate remaining menu sites onto MenuSectionHelper
- **Source:** Cluster H follow-up #2; [cluster-retros/cluster-r.md](cluster-retros/cluster-r.md)
- **Blocks:** plugin mid-construction menu injection (Cluster H residue), plugin API 1.0
- **Scope:** medium
- **Details:** After Cluster R, three menu-construction sites remain un-migrated onto `MenuSectionHelper` + `MenuEventEmitter`: the `EditorViewSpace` tab bar, `TextControl`, and `CorbomiteMDI Sidebar`. `FileExplorerPanel` (now `src/plugins/file-explorer/`) is the canonical example. `CanvasScene` and the Markdown Editor view-header menu were migrated during Cluster R (P1/P3). The three remaining sites are mechanical one-by-one refactors; blocking for clean plugin mid-construction menu injection.

### SuggestPopup as a dedicated widget
- **Source:** Cluster H follow-up #4; [docs/PROJECT-STATE.md §Cluster H follow-ups](PROJECT-STATE.md)
- **Blocks:** any plugin (or built-in) that wants rich-content suggestions
- **Scope:** medium
- **Details:** `SuggestPopup` should be a separate widget from `CompletionPopup`, hosting a per-row `EditorSuggest::renderSuggestion()` delegate. Today `CompletionPopup` still renders all suggestions with no per-suggester customisation. Build when the first plugin or built-in wants rich (non-text) suggestion rows.

### ~~Multi-Notice stacking coordinator~~ Done 2026-04-19 — commit `56d0db85` stacks open `Notice` toasts vertically in `src/dialogs/Notice.cpp`.

### Plugin-facing wrappers for hover/suggest surfaces
- **Source:** Cluster H follow-up #6; [docs/PROJECT-STATE.md §Cluster H follow-ups](PROJECT-STATE.md)
- **Blocks:** plugin API 1.0 surface completeness
- **Scope:** medium
- **Details:** `HoverLinkSourceRegistry`, `EditorSuggestManager`, `RibbonSlot`, and `MenuEventEmitter` still have no proxy equivalents in `PluginContext`. These are the remaining Cluster-H-originated surfaces that need plugin-facing wrappers before the plugin API can be considered complete. Build when the first plugin consumer demands them (Cluster N direction).

### In-app plugin browse / install UI
- **Source:** Cluster N retro §Open follow-ups; [cluster-retros/cluster-n.md](cluster-retros/cluster-n.md)
- **Blocks:** third-party plugin distribution UX
- **Scope:** medium
- **Details:** `SettingsDialog`'s Plugins page lists only locally-discovered plugins. No "browse the store" or "install from URL" flow exists; third-party install is a manual `cp -r` into `~/.local/share/corbomite-dev/plugins/`. A future cluster should add a plugin registry browser and install-from-URL or install-from-archive flow, matching Obsidian's Community Plugins page.

### Sandbox / process-isolation decision
- **Source:** Cluster N retro §Open follow-ups; [cluster-retros/cluster-n.md](cluster-retros/cluster-n.md)
- **Blocks:** security story for third-party plugins
- **Scope:** large
- **Details:** Plugins currently run in-process as KPluginFactory `.so` modules. No sandbox exists. The decision — seccomp, Linux namespaces, or WebEngine for a JS-shim path — is deferred to a future cluster. The native-C++ substrate shipped in Cluster N is the foundation; process isolation builds on top of it.

### JS plugin shim layer
- **Source:** Cluster N retro §Open follow-ups; [cluster-retros/cluster-n.md](cluster-retros/cluster-n.md)
- **Blocks:** Obsidian JS plugin portability (if ever targeted)
- **Scope:** large
- **Details:** The native-C++ plugin substrate from Cluster N was chosen over a V8/WebEngine embed; however, a JS shim on top remains possible as future work. Noted in the Cluster N design decisions. Depends on the sandbox/process-isolation decision above.

### ~~`ui.views` permission semantics for `createView()`-only plugins~~ Resolved 2026-04-19 — `ui.views` gates `ViewRegistrar` only (main-area view-type registration); sidebar `createView()` via `X-Corbomite-DockArea` does **not** require it. `docs/plugin-development/API-REFERENCE.md §Permissions` updated to state this explicitly.

### ~~`CorbomiteConfigVersion.cmake`~~ Done 2026-04-19 — commit `1d4b3a9a` adds `write_basic_package_version_file` to the top-level `CMakeLists.txt` so version-constrained `find_package(Corbomite)` calls work.

### Distro packaging path validation
- **Source:** Cluster N retro §Discovered during execution; [cluster-retros/cluster-n.md](cluster-retros/cluster-n.md)
- **Blocks:** first deb/rpm/flatpak/AppImage packaging effort
- **Scope:** small
- **Details:** The `docs/plugin-development/DISTRIBUTION.md` doc states `.so` install paths as the packaging convention, but no distro package exercises them yet. When the first packaging effort (deb / rpm / flatpak / AppImage) lands, verify the install path conventions are honoured and correct any mismatches.

---

## 3. Editor, Views, Workspace

### ~~Empty-state "New Tab" view~~ Done 2026-04-19 — `Corbomite::EmptyView` (`libs/core/src/EmptyView.cpp`) registered from `MainWindow` as viewType `"empty"` with Create-new-file / Go-to-file / Close buttons. `WorkspaceLeaf::setViewState` also falls back to `"empty"` when a factory is missing (absorbs Cluster G follow-up #2 "unknown-viewType fallback").

### ~~Unknown-viewType fallback view~~ Done 2026-04-19 — `WorkspaceLeaf::setViewState` now falls back to the registered `"empty"` view when a factory lookup misses (covered alongside Empty-state "New Tab" view above). Distinct `nD` vs `tD` visual treatment deferred until a consumer needs different copy.

### Centralised `Workspace::openLinkText` dispatcher
- **Source:** Cluster G follow-up #3; [docs/PROJECT-STATE.md §Cluster G follow-ups](PROJECT-STATE.md)
- **Blocks:** faithful plugin API; Cluster R "Open linked view" submenu upgrade
- **Scope:** medium
- **Details:** Obsidian routes all link opens through a single `Workspace::openLinkText(linktext, source, mode, opts)` dispatcher that consults `activeLeaf` and `getLeaf(mode)` and handles sidebar-pinned-leaf focus semantics. Corbomite currently resolves links ad-hoc inside each view (e.g. `NoteEditorWidget` handles wikilink clicks directly). The missing dispatcher blocks a faithful plugin API. Note: Cluster R's "Open linked view" submenu uses an interim dispatch to each plugin's `:open` command; when this dispatcher lands the submenu upgrades to real leaf-opening without menu-schema changes. Audit reference: `docs/obsidian-audit/domains/workspace.md §§1, 6`.

### `FileView::receiveSyncState` linked-pane hook
- **Source:** Cluster G follow-up #4; [docs/PROJECT-STATE.md §Cluster G follow-ups](PROJECT-STATE.md)
- **Blocks:** group-linked pane file sync
- **Scope:** medium
- **Details:** Obsidian's group-linked panes sync the active file via a `FileView::receiveSyncState(peer)` virtual. Corbomite wires `setGroup(id)` + `groupChanged` at the `WorkspaceLeaf` level but has no base-class hook for cross-group file sync. View subclasses would currently have to hand-roll sync, which means they don't. Audit reference: `docs/obsidian-audit/domains/workspace.md §1`, `domains/views.md §1`.

### ~~`lastOpenFiles` sibling key restored on load~~ Done 2026-04-19 — commit `1d4b3a9a` feeds the `lastOpenFiles` sibling key into `Workspace::deserialize` via `SessionManager`.

### Word-processor-style Format/Heading checkstate + contextual menu gating
- **Source:** Cluster V Phase 2+3 closeout review, 2026-04-20
- **Blocks:** nothing ships broken without this, but the Format/Heading/Insert menus currently advertise actions whose check-state never reflects the cursor context (Bold isn't shown as active when typing inside `**foo**`, Heading radio only updates on active-leaf change, etc.)
- **Scope:** medium (host side) / medium (Markoff side — blocked on Markoff rewrite landing)
- **Details:** Corbomite's `MainWindow::refreshEditorActions` only exercises the subset of checks Markoff exposes today (`cursorInTable()`, `currentHeadingLevel()`). To achieve real word-processor UX — Bold highlighted inside a bold span, Table submenu hidden/disabled outside a table, Toggle Checkbox checked on a task line, Insert > Callout disabled inside an existing callout — we need a per-cursor context snapshot from Markoff covering block kind, inline span membership, table coordinates, and link/tag/footnote spans. Requirements drafted for the Markoff rewrite team at `libs/markoff-family/libs/markoff/docs/specs/2026-04-20-consumer-editor-state-surface.md` — proposes a single `EditorContext` struct + `contextChanged` signal + pull accessor. Markoff rewrite is on a separate branch; don't block on it. When it lands, retire the "static enable, never checked" Format-toolbar state in `refreshEditorActions`.

### Markoff editor right-click menu enrichment (`editor-menu` parity)
- **Source:** Cluster V scope review, 2026-04-20
- **Blocks:** Obsidian UX parity (Format/Heading/Insert/Table are reachable via menubar + command palette but not via right-click, which Obsidian users reach for first)
- **Scope:** medium (host side — blocked on Markoff rewrite landing the `aboutToShowContextMenu` signal)
- **Details:** `libs/markoff-family/libs/markoff/src/Editor.cpp::contextMenuEvent` currently emits only Cut/Copy/Paste/Select-All (plus table ops when inside a GFM table). Obsidian's `editor-menu` surface also includes Format (Bold/Italic/Strikethrough/InlineCode), Insert (Link/WikiLink/Image/Callout/Table/Checkbox), Heading (H1-H6 + Increase/Decrease), and fold toggles — and third-party plugins inject into this same menu via `workspace.trigger("editor-menu", menu, editor, view)`. Markoff requirements drafted alongside the `EditorContext` snapshot spec at `libs/markoff-family/libs/markoff/docs/specs/2026-04-20-consumer-editor-state-surface.md §9` — proposes `Q_SIGNAL void aboutToShowContextMenu(QMenu*, EditorContext, QPoint)` emitted mid-`contextMenuEvent` after Markoff's built-ins and before `menu.exec()`. Host side: `MainWindow` connects the signal, wraps the `QMenu *` in `Corbomite::MenuSectionHelper` (Cluster R), appends Format/Heading/Insert/Table entries with section tags (`action` / `action-primary`), and calls `helper.flush()`. Same signal is the natural dispatch point for the future plugin `editor-menu` contribution hook (tracked as "Menu mid-construction plugin hook" in `FEATURE-MATRIX.md §9`) — plugin proxy slots connect to the same signal.

### Obsidian-compat command-id mirror in `CommandRegistry`
- **Source:** Cluster V scope review, 2026-04-20
- **Blocks:** `.obsidian/hotkeys.json` round-trip (tracked at §5 "`.obsidian/hotkeys.json` load/save"); future plugin-API `Plugin.addCommand({id: "editor:toggle-bold", ...})` calls binding to existing built-in actions
- **Scope:** small
- **Details:** Cluster V Phase 2+3 registers ~25 menubar actions in `KActionCollection` under Corbomite-native object names (`format_bold`, `heading_1`, `insert_table`, …) — these are idiomatic for `corbomiteui.rc.in` and KXMLGUI but don't match Obsidian's canonical command ids (`editor:toggle-bold`, `editor:set-heading-1`, `editor:insert-table`). KActionCollection object names containing `:` also break the XMLGUI XML parser, so renaming isn't an option. Compat-aligned pattern: after `setupActions()` runs, call a new `MainWindow::mirrorBuiltInsIntoCommandRegistry()` that loops through a static `QHash<QString, QString>` (Corbomite id → Obsidian id) and calls `m_commandRegistry->registerBuiltInCommand(obsidianId, qAction)` for each pair. `KCommandBar` palette already consumes `CommandRegistry` under the "Commands" group, so the mirror shows up there alongside plugin commands. `.obsidian/hotkeys.json` load/save (§5 backlog item) then only has to walk `CommandRegistry` once.

### `WorkspaceWindow` popout integration
- **Source:** Cluster G follow-up #6; [docs/PROJECT-STATE.md §Cluster G follow-ups](PROJECT-STATE.md)
- **Blocks:** Cluster R "Open in new window" menu slot (currently a disabled placeholder)
- **Scope:** large
- **Details:** `WorkspaceWindow` exists in `libs/core/` (Task 7 was deferred during Cluster G Part 2) but the full popout-window lifecycle — separate window state persistence, cross-window leaf moves — is unbuilt. Cluster R already ships the "Open in new window" hamburger menu slot as a disabled placeholder; when this follow-up lands, the slot goes live without menu-schema changes.

### ~~`View.onTabMenu` default implementation~~ Done 2026-04-19 — `View::onTabMenu(QMenu *)` now emits Close / Close Others / Close All to the Right / Close All via new `WorkspaceTabs::requestCloseTab/Others/ToRight/All` helpers. `WorkspaceTabs` wires `QTabBar::customContextMenuRequested` to invoke it, so right-clicking a tab shows the canonical context menu; subclasses override to extend. "Move to New Window" deferred with Cluster G follow-up #6.

### ~~`ViewRegistry` error-path hardening~~ Done 2026-04-19 — commit `5f858059` hardens the three error paths (duplicate-registration, unregistered-type lookup, factory-throw) in `libs/core/src/ViewRegistry.cpp`.

### Ribbon-style toolbar micro-UX experiments

- **Origin:** 2026-04-20 ribbon-to-toolbar refactor (`docs/superpowers/plans/2026-04-20-ribbon-to-toolbar.md`, `docs/superpowers/specs/2026-04-20-ribbon-to-toolbar-design.md`).
- **Status:** Deferred; not tied to a cluster.
- **Summary:** In the switch from `RibbonSlot` to the top-docked `RibbonToolBar`, two Obsidian-ribbon UX affordances were dropped: (1) in-place drag-reorder of individual icons, (2) right-click → hide *this* icon. Users reorder and hide via the standard KDE *Settings → Configure Toolbars* dialog; visibility persists per-vault via `workspace.json['left-ribbon'].hiddenItems` when toggled through `RibbonStateController`.
- **Action if revived:** Subclass `KToolBar` (in `RibbonToolBar`) with an event filter for right-click → add a per-item context menu exposing "Hide this icon", and implement drag-drop reorder via `mousePressEvent` / `mouseMoveEvent` handlers operating on the underlying `QAction` list. Note: `QJsonObject` does not preserve key insertion order, so a drag-reorder feature also needs a separate ordering array (e.g. `['left-ribbon'].order: string[]`) — deviating from Obsidian's on-disk schema.
- **Rationale for deferral:** Not worth the subclass complexity until a user complaint lands; *Configure Toolbars* covers the 99% case.

---

## 4. Search and metadata

### ~~Regex post-filter in search~~ Done 2026-04-19 — `SearchDSL::CompiledPlan` gained `regexPatterns` + `caseSensitiveTerms`; `SQLiteIndex::searchCompiled` overload post-filters FTS5 candidates via `QRegularExpression` / `QString::contains(Qt::CaseSensitive)` over the `notes_fts.content` column. `SearchView` routes to the new overload when either list is populated.

### ~~True `match-case` semantics~~ Done 2026-04-19 — shipped together with the regex post-filter above; match-case now emits the inner subtree for FTS5 candidate narrowing and re-checks literal terms with `Qt::CaseSensitive` before returning.

### `line:` / `block:` / `section:` / `task*:` search operators
- **Source:** Cluster D follow-up #1; [docs/PROJECT-STATE.md §Cluster D follow-ups](PROJECT-STATE.md)
- **Blocks:** nothing currently; build when first user complains
- **Scope:** medium
- **Details:** The search query parser recognises these operators but `compile()` rejects them as `unsupported`. Implementation requires a markdown-AST post-filter using the existing `markoff-parser` tree-sitter output. The consumer-side substrate is now ready: Cluster J (landed 2026-04-15) shipped heading/section/block resolution via `MetadataCache.headings` / `.blocks`. Wiring the filter is straightforward once a user complaint justifies the work.

### `[key]` / `[key:val]` property-call operator
- **Source:** Cluster D follow-up #2; [docs/PROJECT-STATE.md §Cluster D follow-ups](PROJECT-STATE.md)
- **Blocks:** property-filter search queries; coordinate with Cluster I MetadataCache
- **Scope:** medium
- **Details:** The tokeniser skips the brackets and the AST has no node for property-call expressions. Implementation needs a new `note_properties(note_path, key, value_text, value_num)` side-table. Should coordinate with Cluster I (MetadataCache parity), which built the parallel cache. Land together to avoid double migration.

### KCommandBar palette wired through `CommandRegistry`
- **Source:** Cluster D follow-up #5; [docs/PROJECT-STATE.md §Cluster D follow-ups](PROJECT-STATE.md)
- **Blocks:** nothing; cosmetic divergence only
- **Scope:** small
- **Details:** `KCommandBar` currently uses KDE's built-in fuzzy matcher from `KF6::WidgetsAddons`. Replacing it with `Corbomite::FuzzyMatcher` would require hooking into `KCommandBar` internals or replacing the widget. Cosmetic divergence only; defer until the divergence becomes user-visible or a plugin needs the custom matcher.

### Quick-Switcher Obsidian-style mode switching (`#` / `^` / `[[`)
- **Source:** Cluster D follow-up #6; [docs/PROJECT-STATE.md §Cluster D follow-ups](PROJECT-STATE.md)
- **Blocks:** nothing; Cluster H suggester-UI surface
- **Scope:** small
- **Details:** Belongs to the Cluster H suggester-UI surface. The matcher infrastructure is ready; the UI prefix-routing (typing `#` switches to heading mode, `^` to block mode, `[[` to wikilink mode) is the remaining work. Build when a user notices the missing mode switching.

### ~~Snippet-text rich rendering in `SearchResultsModel`~~ Done 2026-04-19 — commit `96c59821` adds `SearchResultsDelegate` using `ResultHighlighter::drawHighlighted`, wired into `SearchView`.

---

## 5. Plugin primitives and lifecycle

### SessionDestroyer — per-vault plugin lifecycle
- **Source:** Cluster C follow-up #1; [docs/PROJECT-STATE.md §Cluster C follow-ups](PROJECT-STATE.md)
- **Blocks:** per-vault `Component` / `PluginInstance` lifecycle scoping
- **Scope:** medium
- **Details:** → see §2 (Sandbox/process-isolation). Value: per-vault plugin lifecycle teardown, per-vault `KConfig` isolation. Blocker for building now: no `Component` or `PluginInstance` is currently per-vault-scoped; vault switch works fine via the existing ad-hoc `closeAllDocuments` path (`MainWindow.cpp:942`). Build when the first per-vault plugin surface lands. Related to the Cluster N retro note that current teardown is synchronous with no plugin-finalise hook.

### `.obsidian/hotkeys.json` load/save
- **Source:** Cluster C follow-up #2; [docs/PROJECT-STATE.md §Cluster C follow-ups](PROJECT-STATE.md)
- **Blocks:** user-configurable plugin hotkeys
- **Scope:** small
- **Details:** The `Hotkey` primitive at `libs/core/Hotkey.h` is ready. Build when the first user-configurable hotkey-backed command registers. The `HotkeyFile` round-trip class exists in skeleton form; it needs wiring into `PluginContext` so plugins can declare and load their hotkeys from the standard Obsidian path.

### Modal/Menu `Scope` push/pop
- **Source:** Cluster C follow-up #3; [docs/PROJECT-STATE.md §Cluster C follow-ups](PROJECT-STATE.md)
- **Blocks:** first plugin-provided Modal
- **Scope:** small
- **Details:** → see §2 (Plugin-facing wrappers). Existing `QuickSwitcher` / `KCommandBar` paths handle their own `Esc` and do not use `Scope`. Rewriting them through `Scope` is churn. Build when the first plugin-provided `Modal` lands (Cluster N direction). The `Scope` push/pop mechanism is the prerequisite for plugins to receive keyboard events inside their modals without leaking to the host.

### Vault-switch sidebar-hosting regression test
- **Source:** 2026-04-19 — sidebar-invisible fix follow-up (see `decisions-archive.md`)
- **Blocks:** nothing; guards against regression
- **Scope:** small
- **Details:** The bug where `releasePluginView`'s queued `deleteLater` collided with the next `hostPluginView`'s `createToolView(identifier)` call (leaving sidebars empty after a vault swap) shipped despite `tst_vault_switch` existing — the test didn't assert tool-view hosting afterwards. An attempted regression guard in `tst_vault_switch` turned out to be a false pass: QTest's event-loop pumping during `settle()` drains the `DeferredDelete` queue before the next `openVault`, so the offscreen path doesn't hit the race that the real app hit. A true regression test probably needs either (a) a direct API-level check on `CorbomiteMDI::createToolView` — create, `deleteLater`, verify duplicate-identifier create fails, verify it succeeds after event-loop drain — or (b) a GUI test that drives the UI via the "Open Vault" action and asserts non-zero sidebar widths. Punted for now; the fix is synchronous-delete in `MainWindow::releasePluginView` plus `qWarning` on the `createToolView` / `hostPluginView` refuse paths, so the next occurrence is at least findable from the terminal.

---

## 6. Out-of-tree extractions (controller-side)

### DOMPurify `SL` allowlist
- **Source:** Controller-side follow-up #1; [docs/PROJECT-STATE.md §Controller-side follow-ups](PROJECT-STATE.md)
- **Blocks:** full plan for Cluster H (plugin HTML sanitisation)
- **Scope:** small
- **Details:** Security-critical for plugin HTML sanitisation. Grep `_internal.js` for `ALLOWED_TAGS` / `ALLOWED_ATTR` to extract the DOMPurify allowlist that Obsidian uses. Until this is extracted, Corbomite's HTML sanitisation for plugin-rendered content is unspecced.

### Turndown `hP` rule set
- **Source:** Controller-side follow-up #2; [docs/PROJECT-STATE.md §Controller-side follow-ups](PROJECT-STATE.md)
- **Blocks:** nothing critical; web-clipper paste-from-browser compat
- **Scope:** small
- **Details:** Web-clipper paste-from-browser compatibility. Grep `_internal.js` for `new TurndownService` / `.addRule(` to extract the Turndown HTML-to-Markdown rule set that Obsidian applies on web-content paste. Nice-to-have; not blocking any current cluster.

### ~~**DK/RK/JK/PX** formula/filter DSL parser.~~ Done 2026-04-17 — see [addendum](obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md).

### 25 unnamed internal-plugin manifest IDs
- **Source:** Controller-side follow-up #4; [docs/PROJECT-STATE.md §Controller-side follow-ups](PROJECT-STATE.md)
- **Blocks:** completing `FEATURE-MATRIX.md` feature catalog
- **Scope:** small
- **Details:** Extract from `core/App.js:611–641`. The 25 unnamed internal-plugin manifest IDs are needed to complete the feature catalog in `docs/obsidian-audit/FEATURE-MATRIX.md`. Without them the catalog has gaps in the built-in plugin surface.

### `AC` (appearance-keys allow-list) + `PC` (vault-config defaults)
- **Source:** Controller-side follow-up #5; [docs/PROJECT-STATE.md §Controller-side follow-ups](PROJECT-STATE.md)
- **Blocks:** `.obsidian/app.json` + `appearance.json` round-trip in Cluster B
- **Scope:** small
- **Details:** Grep `_internal.js` for the `AC` appearance-keys allow-list and the `PC` vault-config-defaults object. Both are needed to implement faithful `.obsidian/app.json` and `appearance.json` round-trip in Cluster B Phase 3b. Without them the config round-trip will silently drop or corrupt Obsidian-written fields.

### Search-panel DSL grammar
- **Source:** Controller-side follow-up #6; [docs/PROJECT-STATE.md §Controller-side follow-ups](PROJECT-STATE.md)
- **Blocks:** Cluster D Phase 4
- **Scope:** medium
- **Details:** The search-panel DSL grammar lives in the out-of-tree `global-search` internal plugin and has not been reverse-engineered. Approach: combine Obsidian user docs with `grep openGlobalSearch("..."` call sites in `_internal.js`. Blocks Cluster D Phase 4 (full DSL compile coverage).

---

## 7. Cluster-K follow-ups

### Cards and List layouts for BasesView
- **Source:** Cluster K retro §Deliberate MVP cuts #1; [cluster-retros/cluster-k.md](cluster-retros/cluster-k.md)
- **Blocks:** nothing; Table is the only shipped layout
- **Scope:** medium
- **Details:** Only the Table layout shipped in Cluster K. `BasesViewConfig::type` already accepts arbitrary strings; a follow-up registers extra layout types (Cards, List) against the cell-rendering pipeline. The data binding and `BasesQueryResult` partitioning infrastructure exists; the work is purely UI-layer layout variants.

### Internal-plugin wrapping for BasesView (KPluginFactory)
- **Source:** Cluster K retro §Deliberate MVP cuts #2; [cluster-retros/cluster-k.md](cluster-retros/cluster-k.md)
- **Blocks:** plugin extension of Bases view types and formula functions
- **Scope:** medium
- **Details:** `BasesView` is currently wired directly into `MainWindow`'s `ViewRegistry` (like `markdown`/`canvas`) rather than as a `src/plugins/bases/` `.so` module. Porting requires migrating `BasesEntry` + `FileValue` from raw `Vault *` + `MetadataCache *` to `VaultProxy` + `MetadataCacheReader`. The `FunctionRegistry::registerGlobalFunc` / `registerInstanceFunc` plugin extension API already exists; wiring it across the plugin boundary is the remaining gap.

### Rich inline-edit widgets in BasesCellDelegate
- **Source:** Cluster K retro §Deliberate MVP cuts #3; [cluster-retros/cluster-k.md](cluster-retros/cluster-k.md)
- **Blocks:** nothing; `QLineEdit` catch-all works for MVP
- **Scope:** medium
- **Details:** `BasesCellDelegate` uses `QLineEdit` as the catch-all editor. A proper implementation ports `metadataTypeManager.registeredTypeWidgets` — custom editors per value type (e.g. a date picker for `DateValue`, a multi-select for `ListValue`). Build when users report friction with the plain-text editor for typed properties.

### View-rename wikilink rewrite
- **Source:** Cluster K retro §Deliberate MVP cuts #4; [cluster-retros/cluster-k.md](cluster-retros/cluster-k.md)
- **Blocks:** audit §9 view-rename feature
- **Scope:** medium
- **Details:** Renaming a Bases view does not rewrite `[[basefile#viewname]]` references across the vault. Audit reference: `docs/obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md` §9. This requires a vault-wide wikilink rewrite pass keyed on the `basefile#oldname` anchor, similar in shape to the general link-rename work in Cluster A.

### `![[Foo.base]]` embed in markdown
- **Source:** Cluster K retro §Deliberate MVP cuts #5; [cluster-retros/cluster-k.md](cluster-retros/cluster-k.md)
- **Blocks:** `EmbedRegistry` integration
- **Scope:** medium
- **Details:** `EmbedRegistry` (from Cluster J) has no handler registered for `.base` files. Registering one would allow `![[Foo.base]]` embeds to render an inline Bases table inside a markdown note. The `EmbedRenderer` plumbing exists; the work is a `BasesView`-backed embed handler.

### Clipboard export (TSV / Markdown / HTML)
- **Source:** Cluster K retro §Deliberate MVP cuts #6; [cluster-retros/cluster-k.md](cluster-retros/cluster-k.md)
- **Blocks:** parity audit §9 export feature
- **Scope:** small
- **Details:** No clipboard export is wired. The parity audit requires TSV, Markdown, and HTML emit, plus an `obsidian/table` custom MIME type. The `BasesQueryResult` data model is already fully populated; the work is serialising it into each format and placing on the clipboard.

### Formula editor (syntax highlighting + autocomplete)
- **Source:** Cluster K retro §Deliberate MVP cuts #7; [cluster-retros/cluster-k.md](cluster-retros/cluster-k.md)
- **Blocks:** formula authoring UX
- **Scope:** medium
- **Details:** The formula text field is a plain `QLineEdit`. No syntax highlighting or autocomplete for the Pratt DSL. A proper formula editor would integrate with `markoff-parser` or a dedicated Bases grammar to highlight keywords and offer function-name completions. Build when user complaints about formula authoring UX surface.

### `+ New` button (NewItemMenu) with filter-satisfying frontmatter
- **Source:** Cluster K retro §Deliberate MVP cuts #8; [cluster-retros/cluster-k.md](cluster-retros/cluster-k.md)
- **Blocks:** new-note creation from inside a Bases view
- **Scope:** small
- **Details:** No "New" button or `NewItemMenu` exists in `BasesView`. The intended behaviour is pre-populating the frontmatter of a new note so it satisfies the active view's filters (e.g. pre-filling `status: active` if the filter is `status = "active"`). `BasesQuery::newItemFolder` + `newItemTemplate` fields are already parsed from the `.base` YAML; the work is the modal and creation flow.

### Per-BasesView undo/redo
- **Source:** Cluster K retro §Deliberate MVP cuts #9; [cluster-retros/cluster-k.md](cluster-retros/cluster-k.md)
- **Blocks:** nothing; inline edits are not undoable
- **Scope:** medium
- **Details:** No dedicated `QUndoStack` exists for `BasesView`. Inline-edit writebacks go directly through `FileManager::processFrontMatter` with no undo record. A per-view undo stack should sit between the delegate's `setModelData` and the `FileManager` write.

### ~~Column-reorder persistence~~ Done 2026-04-19 — commit `261fb3cd` wires `QHeaderView::sectionMoved` to rewrite `BasesViewConfig::order` in `libs/bases/src/BasesView.cpp`.

### Multi-key sort cycling UI
- **Source:** Cluster K retro §Deliberate MVP cuts #11; [cluster-retros/cluster-k.md](cluster-retros/cluster-k.md)
- **Blocks:** nothing; single-key sort works
- **Scope:** medium
- **Details:** Header clicks cycle a single column through ASC → DESC → unsorted. A multi-key sort builder — a Sort toolbar menu or a shift-click-to-add-key interaction — belongs to the Sort toolbar menu that is also deferred. `BasesQueryResult` already supports multi-key sort at the data layer; the UI is the remaining work.

### Group-header rendering and collapsible sections
- **Source:** Cluster K retro §Deliberate MVP cuts #12; [cluster-retros/cluster-k.md](cluster-retros/cluster-k.md)
- **Blocks:** grouped Bases views
- **Scope:** medium
- **Details:** `BasesQueryResult::groups()` produces the correct grouping data but the `QTableView` widget does not render group boundaries or collapsible section headers. A `QStyledItemDelegate` or a custom proxy model is needed to insert group-header rows and handle expand/collapse state.

---

## 8. Cluster-N follow-ups

*(All Cluster-N follow-ups surfaced in §2.)*

---

## 9. Cluster-Q follow-ups

### `tst_propertiespanel` mock-proxy rewrite
- **Source:** Cluster Q retro §Cluster Q follow-ups #5; [cluster-retros/cluster-q.md](cluster-retros/cluster-q.md); confirmed still open in Cluster N retro
- **Blocks:** nothing; test coverage for `PropertiesView` writeback
- **Scope:** small
- **Details:** The legacy `tst_propertiespanel` test was too tightly coupled to direct injection to retrofit to the proxy-based architecture introduced in Cluster Q. A new test at `src/plugins/properties/tests/tst_properties_plugin.cpp` partially covers `createView`; the writeback semantics from the old test (editing a frontmatter value via the Properties panel and verifying the file is updated) still need a mock-proxy rewrite. The mock `VaultProxy` + `FileManagerProxy` infrastructure from other plugin tests is the template.

---

## 10. Stability (known-flaky tests)

### `tst_markoff_inline_math` — inline math flakiness
- **Source:** [docs/PROJECT-STATE.md §Known-flaky tests](PROJECT-STATE.md)
- **Blocks:** nothing
- **Scope:** small
- **Details:** Inline math rendering test; flaky under offscreen Qt QPA. Pre-existing, not introduced by recent clusters. Schedule a dedicated stability pass when the roadmap quiets.

### `tst_renderengine` — legacy renderer flakiness
- **Source:** [docs/PROJECT-STATE.md §Known-flaky tests](PROJECT-STATE.md)
- **Blocks:** nothing
- **Scope:** small
- **Details:** Legacy `RegexRenderEngine` / `MarkdownRenderer` tests; flaky on slow CI. Pre-existing. Fixing may require either relaxing time-bound assertions or replacing the timing-sensitive test logic with deterministic alternatives.

### `tst_completion_popup` — popup timing flakiness
- **Source:** [docs/PROJECT-STATE.md §Known-flaky tests](PROJECT-STATE.md)
- **Blocks:** nothing
- **Scope:** small
- **Details:** Popup show/hide timing-sensitive under offscreen QPA. Pre-existing. The most likely fix is converting the timed wait to a `QSignalSpy`-based wait that blocks until the popup is actually shown/hidden rather than sleeping for a fixed interval.

### `tst_benchmark_layout` — layout timeout flakiness
- **Source:** [docs/PROJECT-STATE.md §Known-flaky tests](PROJECT-STATE.md)
- **Blocks:** nothing; excluded from "green suite" counts
- **Scope:** small
- **Details:** Timeout under offscreen QPA + cold cache; the 1500 ms gate is already permissive but still flakes. Pre-existing. The most likely fix is raising the timeout threshold or measuring the benchmark relative to a calibration run rather than against an absolute constant.

---

*(A and B follow-up sweep 2026-04-19: grep found no deferred/follow-up items specific to Clusters A or B beyond what is already captured above — A is fully not-started and appears only in "Recent decisions" context; B's deferred Phase 3b wiring is captured in §1 Cluster B above.)*
