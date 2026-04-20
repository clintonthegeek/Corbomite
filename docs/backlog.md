# Backlog — Unified Map

> **Living document.** Every deferred follow-up, every not-yet-started cluster, every scouting doc, every known-flaky test. Grouped by theme, not by originating cluster. Strike items (`~~text~~`) when closed with a one-line closure + date; quarterly roll strike-throughs into `decisions-archive.md`.

## Reading order

Clusters to-do → Plugin API / extension surfaces → Editor / Views / Workspace → Search / metadata → Plugin primitives + lifecycle → Out-of-tree extractions → Cluster-specific follow-up buckets → Stability.

---

## 1. Not-started, plan-needed, and scouting clusters

### Cluster A — Link / frontmatter correctness
- **Source:** [Cluster A plan](superpowers/plans/2026-04-14-cluster-a-link-frontmatter-correctness.md)
- **Blocks:** many downstream clusters (keystone)
- **Scope:** large
- **Details:** Full-plan cluster, status "Not started". Correctness work for link resolution and frontmatter parsing — described in the INDEX as a keystone that other clusters depend on weakly or strongly. No consumer has forced the start yet; should be the first cluster picked up when the roadmap resumes parity work.

### Cluster B — Vault I/O
- **Source:** [Cluster B plan](superpowers/plans/2026-04-14-cluster-b-vault-io.md)
- **Blocks:** depends weakly on A; wires `WorkspaceState` / `SessionManager` / `MainWindow` to `VaultConfig` + `WorkspaceState` (Phase 3b)
- **Scope:** large
- **Details:** Full-plan cluster, status "Not started". Primitives landed in 2026-04-14 (Phases 1–6: `DataAdapter`, `FileSystemAdapter`, `VaultConfig`, `WorkspaceState`, `CaseSensitivityProbe`, `VaultTrash`, `IgnoreFilter`, `VaultProcess`). The outstanding work is Phase 3b: app-layer wiring of `SessionManager` / `EditorViewManager` / `MainWindow` onto `WorkspaceState` rather than the current `session.json` path (~500–1000 LOC). The full Phase 3b design is in the B plan; unblocked now that Cluster G/Q are done.

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

### Cluster S — Bookmarks core plugin
- **Source:** [INDEX.md](superpowers/plans/INDEX.md) row S; [spec](superpowers/specs/2026-04-19-cluster-s-bookmarks-design.md); [addendum](obsidian-audit/addenda/2026-04-19-bookmarks-core-plugin.md)
- **Blocks:** Cluster R's "Bookmark…" menu slot (currently a disabled placeholder)
- **Scope:** medium
- **Details:** Plan-needed (spec written 2026-04-19). Single-phase, ~5 days. Ships `.obsidian/bookmarks.json` Obsidian-compatible round-trip, a right-dock panel, 7 `bookmarks:*` commands, and a "Bookmark…" modal. Activating the placeholder injected by Cluster R into all view hamburger menus.

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

### Multi-Notice stacking coordinator
- **Source:** Cluster H follow-up #5; [docs/PROJECT-STATE.md §Cluster H follow-ups](PROJECT-STATE.md)
- **Blocks:** any code path that fires multiple notices in quick succession
- **Scope:** small
- **Details:** Today only one `Notice` can be on screen at a time. A singleton coordinator that vertically stacks open notices is needed before any code path fires multiple notices concurrently. A natural prerequisite for plugin-generated notices.

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

### `ui.views` permission semantics for `createView()`-only plugins
- **Source:** Cluster N retro §Discovered during execution; [cluster-retros/cluster-n.md](cluster-retros/cluster-n.md)
- **Blocks:** plugin API spec completeness
- **Scope:** small
- **Details:** The `ui.views` permission token is checked at `registerView()` call sites, but a plugin that only calls `createView()` (no `registerView()`) never touches those sites and therefore never needs to declare the token. A spec decision is needed: should `ui.views` be required for `createView()` too? Flagged in `API-REFERENCE`; needs resolution before ApiLevel bump.

### `CorbomiteConfigVersion.cmake`
- **Source:** Cluster N retro §Discovered during execution; [cluster-retros/cluster-n.md](cluster-retros/cluster-n.md)
- **Blocks:** `find_package(Corbomite 1.0 EXACT)` for out-of-tree plugins
- **Scope:** small
- **Details:** `CorbomiteConfig.cmake` is emitted and installed but `CorbomiteConfigVersion.cmake` is not, so version-constrained `find_package` calls from third-party plugin CMake projects will fail. Trivial fix (add a `write_basic_package_version_file` call); deferred out of scope for the Cluster N closeout commit.

### Distro packaging path validation
- **Source:** Cluster N retro §Discovered during execution; [cluster-retros/cluster-n.md](cluster-retros/cluster-n.md)
- **Blocks:** first deb/rpm/flatpak/AppImage packaging effort
- **Scope:** small
- **Details:** The `docs/plugin-development/DISTRIBUTION.md` doc states `.so` install paths as the packaging convention, but no distro package exercises them yet. When the first packaging effort (deb / rpm / flatpak / AppImage) lands, verify the install path conventions are honoured and correct any mismatches.

---

## 3. Editor, Views, Workspace

### Empty-state "New Tab" view
- **Source:** Cluster G follow-up #1; [docs/PROJECT-STATE.md §Cluster G follow-ups](PROJECT-STATE.md)
- **Blocks:** nothing hard; improves discoverability
- **Scope:** small
- **Details:** `WorkspaceLeaf` has no analogue of Obsidian's empty-pane placeholder (the `tD` view) that shows "Create new note" / "Go to file" / "Close" actions when a leaf has no document loaded. A newly-created leaf with no view is currently just blank. Audit reference: `docs/obsidian-audit/domains/views.md §1` (the `eD`/`tD`/`nD` trio). Not blocking round-trip compat; improves new-user discoverability.

### Unknown-viewType fallback view
- **Source:** Cluster G follow-up #2; [docs/PROJECT-STATE.md §Cluster G follow-ups](PROJECT-STATE.md)
- **Blocks:** third-party plugin view types; vault portability across plugin sets
- **Scope:** small
- **Details:** `WorkspaceLeaf::setViewState(type, ...)` called with an unregistered type has undefined UX. Obsidian shows a canned "doesn't look like anything to me" fallback (`nD`). This matters once third-party plugins register custom view types and vaults move between installs with mismatched plugin sets. Audit reference: `docs/obsidian-audit/domains/views.md §1`. Build alongside or after `ViewRegistry` error-path hardening (follow-up #8).

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

### `lastOpenFiles` sibling key restored on load
- **Source:** Cluster G follow-up #5; [docs/PROJECT-STATE.md §Cluster G follow-ups](PROJECT-STATE.md)
- **Blocks:** nothing; trivial one-line fix
- **Scope:** small
- **Details:** `Workspace::serialize()` emits the `lastOpenFiles` sibling key alongside `main`; `SessionManager` preserves it via unknown-key passthrough on save; but `Workspace::deserialize` is never fed that sibling key on load. Pre-existing gap carried from the Cluster G Task 9 retro (`cluster-retros/cluster-g.md §What surprised`). Not blocking any cluster; a trivial fix once a consumer cares (e.g. the "recently opened" list).

### `WorkspaceWindow` popout integration
- **Source:** Cluster G follow-up #6; [docs/PROJECT-STATE.md §Cluster G follow-ups](PROJECT-STATE.md)
- **Blocks:** Cluster R "Open in new window" menu slot (currently a disabled placeholder)
- **Scope:** large
- **Details:** `WorkspaceWindow` exists in `libs/core/` (Task 7 was deferred during Cluster G Part 2) but the full popout-window lifecycle — separate window state persistence, cross-window leaf moves — is unbuilt. Cluster R already ships the "Open in new window" hamburger menu slot as a disabled placeholder; when this follow-up lands, the slot goes live without menu-schema changes.

### `View.onTabMenu` default implementation
- **Source:** Cluster G follow-up #7; [docs/PROJECT-STATE.md §Cluster G follow-ups](PROJECT-STATE.md)
- **Blocks:** third-party plugin views wanting to customise the tab context menu
- **Scope:** small
- **Details:** The tab context menu actions (Close / Close Others / Close After / Close All / Move to New Window) are currently `MainWindow`-wired rather than implemented as a `View` base-class virtual with overridable default. This is cosmetic until a third-party plugin view wants to customise the tab context menu.

### `ViewRegistry` error-path hardening
- **Source:** Cluster G follow-up #8; [docs/PROJECT-STATE.md §Cluster G follow-ups](PROJECT-STATE.md)
- **Blocks:** clean third-party plugin view type registration
- **Scope:** small
- **Details:** Three error paths in `ViewRegistry` are currently undefined: registering a duplicate `viewType`, requesting a factory for an unregistered type, and a factory-throw during construction. Audit and harden once the unknown-viewType fallback view (follow-up #2 above) lands, as that follow-up is the first consumer of the unregistered-type path.

---

## 4. Search and metadata

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

### Regex post-filter in search
- **Source:** Cluster D follow-up #3; [docs/PROJECT-STATE.md §Cluster D follow-ups](PROJECT-STATE.md)
- **Blocks:** nothing; defer until user-facing demand
- **Scope:** small
- **Details:** The parser builds `Regex` nodes; `compile()` currently flags them as unsupported. Implementation is straightforward: fetch all candidate paths for the surrounding clause, then `QRegularExpression::match` over each note body. The pattern is the same shape as the `match-case` follow-up below. Defer until a single user-facing demand surfaces.

### True `match-case` semantics
- **Source:** Cluster D follow-up #4; [docs/PROJECT-STATE.md §Cluster D follow-ups](PROJECT-STATE.md)
- **Blocks:** accurate case-sensitive search
- **Scope:** small
- **Details:** FTS5's default `porter unicode61` tokeniser is case-folding, so `match-case` queries silently degrade to case-insensitive (surfaced in the `unsupported` set). Real implementation: fetch candidates via FTS5, then apply `QString::contains(..., Qt::CaseSensitive)` re-check per candidate. Same shape as the regex post-filter follow-up above.

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

### Snippet-text rich rendering in `SearchResultsModel`
- **Source:** Cluster D follow-up #7; [docs/PROJECT-STATE.md §Cluster D follow-ups](PROJECT-STATE.md)
- **Blocks:** nothing; `SearchMatch.matches` is populated but display is plain text
- **Scope:** small
- **Details:** `SearchMatch.matches` is now populated, but the `QTreeView` default delegate prints plain text. Two options: (a) add a `SearchResultsDelegate` that calls `ResultHighlighter::drawHighlighted` — consistent with `QuickSwitcher`/`CompletionPopup`; or (b) flip `Qt::DisplayRole` to rich-text with `<b>` tags and let Qt render it — one line of code. Option (a) is the architecturally consistent path.

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

### Column-reorder persistence
- **Source:** Cluster K retro §Deliberate MVP cuts #10; [cluster-retros/cluster-k.md](cluster-retros/cluster-k.md)
- **Blocks:** nothing; Qt default drag-reorder works but doesn't persist
- **Scope:** small
- **Details:** `QTableView` header drag-reorder is enabled by Qt's default behaviour, but the reordered column sequence is not persisted back into `BasesViewConfig::order`. A `sectionMoved` signal handler on `QHeaderView` should write the new order into the config and trigger a save.

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
