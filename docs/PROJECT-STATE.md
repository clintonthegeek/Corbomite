# PROJECT-STATE

> **Living document.** Single source of truth for "where we are right now" on the Obsidian-compatibility roadmap. Keep under 200 lines by offloading closeout prose to `decisions-archive.md`. Follow Ritual 2/3 in `CONTRIBUTING-OPS.md` after every meaningful work session.

**Last updated:** 2026-04-20 — Cluster V Phase 1 complete; Phase 2+3 in flight (Markoff-side tasks 2.1 + 2.2 landed in submodule, bumped into outer repo at `a5224b20`).

---

## Current focus

**Cluster V (Editor & Workspace UI surfacing) — in flight, Phase 2+3.** Phase 1 landed (Find/Zoom/About wiring, `editor_toggle_mode` Ctrl+E, `View::zoomIn/Out/Reset` virtuals, `KColorSchemeManager` theme applier). Phase 2+3 now underway in the plan at `superpowers/plans/2026-04-20-cluster-v-editor-workspace-ui-surfacing.md` — Markoff-side additions (`SetHeading1..6` + Ctrl+1..6, `Editor::cursorInTable()`) shipped in the submodule and bumped into the outer tree; next up are the CalloutPicker / InsertTable dialogs and the MainWindow action-registration block.

---

## Roadmap (20 clusters — 15 parity + 3 UI-chrome + 1 post-parity + 1 internal refactor)

Status legend: `Not started` · `Plan-needed` (no cluster plan yet) · `Stub plan` (sketch exists, expand before dispatch) · `In progress (phase N)` · `Blocked — waiting on X` · `Done` · `Deferred`.

| Cluster | Title | Plan | Status | Blocks / Notes |
|---|---|---|---|---|
| A | Link / frontmatter correctness | [full](superpowers/plans/archive/2026-04-14-cluster-a-link-frontmatter-correctness.md) | Done | Unblocks D, F, I, J, K, L |
| B | Vault I/O | [full](superpowers/plans/archive/2026-04-14-cluster-b-vault-io.md) | Done | Unblocks C, E, G, H, N |
| C | Lifecycle / plugin primitives | [full](superpowers/plans/archive/2026-04-14-cluster-c-lifecycle-plugin-primitives.md) | Done (primitives + hookup) | Phases 1–3 + 4a landed 2026-04-15. Unblocks G, H, N. Phase 4b-d (SessionDestroyer, hotkeys.json I/O, Modal Scope push/pop) deferred — build when consumers demand |
| D | Search / suggester parity | [full](superpowers/plans/archive/2026-04-14-cluster-d-search-suggester-parity.md) | Done | Landed 2026-04-15 across 5 commits (70f7d64, 39ac6e1, f939ba9, fd612e5, f3367f7). Unblocks H, I |
| E | Three-mode pivot (Source/LivePreview/Reading) | [full](superpowers/plans/archive/2026-04-14-cluster-e-markoff-three-mode-pivot.md) | Done | Landed 2026-04-15 across 18 commits (7 phases + fork Phases 1+2 prereq + docs). MVP contract satisfied: three-mode encoding round-trips to `workspace.json` via `PaneLeaf.unknown["eState"]`; `Corbomite::SourceEditor` (qutepart-corbomite-backed) + `Markoff::Editor` (existing) + `Corbomite::ReadingView::ReadingView` (greenfield) live as `QStackedWidget` children of `NoteEditorWidget`; visual-line float scroll + fold persist per-leaf; ReadingView has section recycling + 10240-byte async parse + 5ms/10-section frame budget + virtualization + heading fold. See `cluster-retros/cluster-e.md`. Unblocks J. |
| F | Templates / Daily Notes / Moment | [full](superpowers/plans/archive/2026-04-15-cluster-f-templates-daily-notes-moment.md) | Done | Landed 2026-04-15 (5 phases + doc closeout). Enables vault-portable template + daily-note config |
| G | Views hierarchy + TextFileView contract | [Part 1](superpowers/plans/archive/2026-04-15-cluster-g-views-hierarchy.md), [Part 2](superpowers/plans/archive/2026-04-15-cluster-g-part2-workspace.md) | Done | Full cluster closed 2026-04-16. Part 1 (15 commits) + Part 2 infra (11 commits) + Tasks 9-10 (3 commits on top of pre-existing `e3143f1` delete). See `cluster-retros/cluster-g.md`. Unblocks M, N. |
| H | Menus / hover / suggester UI | [full](superpowers/plans/archive/2026-04-14-cluster-h-menus-hover-suggester-ui.md) | Done | Landed 2026-04-15 across 5 commits. Unblocks N |
| I | MetadataCache parity | [full](superpowers/plans/archive/2026-04-15-cluster-i-metadatacache-parity.md) | Done | Landed 2026-04-15 (10 commits across 8 phases). Unblocks F, J, K (partial), L |
| J | Embed / rendering primitives | [full](superpowers/plans/archive/2026-04-15-cluster-j-embed-rendering.md) | Done | Landed 2026-04-15 across 18 commits (6 phases). HoverPopover renders math/mermaid/wiki-links/images/nested embeds via `EmbedRenderer` + ReadingView; internal registries (`EmbedRegistry`, `PostProcessorRegistry`, `CodeBlockProcessorRegistry`) + lifecycle types (`MarkdownRenderChild`, `EmbedDepthGuard`) live in `libs/core/`; `Markoff::LinkRenderer` + `ReadingView::LinkRenderer`/`EmbedRenderer` consolidate inline link emission. See `cluster-retros/cluster-j.md`. Unblocks K, L-extensions, N. |
| K | Bases | [full](superpowers/plans/archive/2026-04-17-cluster-k-bases.md) + [DSL addendum](obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md) | Done | Closed 2026-04-17 across 9 phases (~37 commits). Hand-rolled Pratt-parser formula engine + `std::shared_ptr<Value>` 18-type hierarchy + `.base` YAML round-trip + QTableView-backed BasesView registered as a built-in view for extension `.base`. MVP parity: 5/5 "must work" audit §9 features (open/render/filter/sort/inline-edit). 12 deferred follow-ups (cards/list layouts, KPluginFactory wrapping, rich widgets, view-rename wikilink rewrite, embed-in-markdown, clipboard export, formula editor, NewItemMenu, per-view undo, column-reorder persist, multi-key sort UI, group-header render). Retro at [`cluster-retros/cluster-k.md`](cluster-retros/cluster-k.md). |
| L | Properties panel | — | Done | Landed 2026-04-15 as single-phase normal task (commit 89b1df4); 6 editor widget types + 500ms debounced writeback via FrontMatterWriter |
| M | Internal-plugin feature audits (Graph, Canvas) | — | Deferred | Treat as two normal tasks; no cluster plan needed |
| N | Plugin-ready surfaces | [full](superpowers/plans/archive/2026-04-17-cluster-n-plugin-ready-surfaces.md) | Done | Closed 2026-04-17 across 19 commits (`2e4e3a4` → `ec32fc0`) in 5 phases. VaultProxy QObject + SearchProxy + PluginContext::search + stop-gap deletion + QtKeychain SecretStorage + plugin data.json + corbomite_add_plugin CMake helper + CorbomiteConfig.cmake + MinVersion/ApiLevel enforcement + note-stats reference plugin + docs/plugin-development/ (1737 lines). Retro at `cluster-retros/cluster-n.md`. |
| O | Advanced query layer (graph + enriched FTS) | [scouting](superpowers/plans/2026-04-14-cluster-o-query-layer-SCOUTING.md) | Scouting doc | Post-parity. Expand only after A/B/I/K land and demand signals materialise |
| P | Graffodil adoption (libs/forcegraph + libs/canvas) | [scouting](superpowers/plans/2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md) | Scouting doc | Internal refactor. Parallelisable with parity roadmap. Expand after Graffodil API stabilises (2–3 wk observation) + Cluster A lands |
| Q | Internal-plugin wrapping + permissions | [full](superpowers/plans/archive/2026-04-16-cluster-q-internal-plugin-wrapping.md) | Done | Closed 2026-04-17. 8 InternalPlugins shipped: Backlinks, Outlinks, Outline, Properties, Search, FileExplorer, LocalGraph (full), GraphView (shell — main-area view-type registration deferred). PluginsPage in SettingsDialog. Permission-gated proxies + lifecycle. Retro at `cluster-retros/cluster-q.md`. |
| R | View-header menus (hamburger "…" on every leaf) | [full](superpowers/plans/archive/2026-04-19-cluster-r-view-header-menus.md) | Done | Closed 2026-04-19 across 4 phases (~20 commits `09296582` → `c5448b38`). P1 substrate (MenuSectionHelper canonical order + submenu + `View::onMoreOptionsMenu` hook) + P2 EditableFileView universal items + P3 per-view specialisations (MarkdownView / CanvasFileView / GraphView) + P4 inline backlinks-in-document PostProcessor. Disabled-placeholder slots for G#6 / S / T / Qutepart-fork P3. Absorbs Cluster H follow-up #2 (menu-construction-site migrations) partially; residue at EditorViewSpace tab bar + TextControl + CorbomiteMDI Sidebar tracked in Cluster U scope. Retro at [`cluster-retros/cluster-r.md`](cluster-retros/cluster-r.md); 5 follow-ups captured (top two: Cluster U File Explorer, plugin mid-construction menu injection). Spec at `superpowers/specs/2026-04-19-cluster-r-view-header-menus-design.md`. 10 audit addenda at `obsidian-audit/addenda/2026-04-19-*.md` + one new addendum `2026-04-19-file-explorer-context-menu.md`. |
| U | File Explorer enhancements (right-click + keyboard) | [scouting](superpowers/plans/2026-04-19-cluster-u-file-explorer-enhancements-SCOUTING.md) | Scouting doc | Added 2026-04-19 as Cluster R closeout follow-up #1. Reuses Cluster R primitives (FileManager prompt modals + Platform + PathUtils + MenuSectionHelper). 6 phases sketched: right-click substrate, file-row items, folder-row items, empty-area menu, keyboard (F2/Delete/Enter/Ctrl-click), Cluster H follow-up #2 residue. 3–5 days once expanded. Blocked on: Cluster R follow-up #2 (plugin menu-injection ordering) landing first or same-phase, and `TFolder*` / `TAbstractFile*` modal signature decision. Audit addendum at `obsidian-audit/addenda/2026-04-19-file-explorer-context-menu.md`. |
| S | Bookmarks core plugin | [spec](superpowers/specs/2026-04-19-cluster-s-bookmarks-design.md) | Plan-needed (spec written 2026-04-19) | Single-phase normal task; ~5 days. `src/plugins/bookmarks/` InternalPlugin with `.obsidian/bookmarks.json` Obsidian-compat round-trip (unknown-key preservation), right-dock `BookmarksView` with drag-reorder, 7 `bookmarks:*` commands, "Bookmark…" modal. Unblocks Cluster R's "Bookmark…" menu slot. Addendum at `obsidian-audit/addenda/2026-04-19-bookmarks-core-plugin.md`. |
| T | File Recovery plugin (Version History) | — | Deferred (post-parity) | Ship only if a user asks. Covers periodic snapshots + modal UI + restore. Cluster R ships a disabled "Open version history" menu slot that activates when T lands. Addendum at `obsidian-audit/addenda/2026-04-19-file-recovery-plugin.md`. |
| V | Editor & Workspace UI surfacing | [full](superpowers/plans/2026-04-20-cluster-v-editor-workspace-ui-surfacing.md) + [spec](superpowers/specs/2026-04-20-cluster-v-editor-workspace-ui-surfacing-design.md) | In progress (phase 2+3) | Phase 1 complete (`c0e63f44` → `037d536e`). Phase 2+3 Markoff prereqs landed 2026-04-20 (submodule `f62be59`, `9c917a3`; outer `a5224b20`): `SetHeading1..6` + `cursorInTable()`. Next: dialogs + MainWindow registration. Debt split to Cluster V.2. |
| V.2 | Editor/Workspace debt cleanup | [scouting](superpowers/plans/2026-04-20-cluster-v2-debt-cleanup-SCOUTING.md) | Scouting doc | Companion to V. Holds the fold-gutter coordinator wiring, 6× VaultConfig writer routing, persisted metadata cache loader, AutosaveReactor delay wiring, optional LRU upgrade, and post-V dead-code audit pass. ~4-5 days. Blocked on V landing. |
| W | Canvas & Graph affordances | [scouting](superpowers/plans/2026-04-20-cluster-w-canvas-graph-affordances-SCOUTING.md) | Scouting doc | Added 2026-04-20 from same audit. Split from V because it needs UX design: canvas Link/File node creation, tool palette, resize handles + snap, group ops, force-graph physics + pin UI, GraphViewTab signal wiring. 8 phases, ~8-12 days. Coordinates with Cluster M (graph/canvas audit) and Cluster P (Graffodil). |

---

## In-flight work items

### Cluster V — Editor & Workspace UI surfacing
- **Phase:** 2+3 of 6 (Markoff menu surfacing + fold actions)
- **Last completed step:** Tasks 2.1 + 2.2 — Markoff `SetHeading1..6` actions with Ctrl+1..6 shortcuts and `Editor::cursorInTable()` accessor; submodule bumped (2026-04-20)
- **Next expected step:** Task 2.3 — `CalloutPickerDialog` (26 Obsidian callout types)
- **Owner:** clinton
- **Date last touched:** 2026-04-20
- **Open sub-questions:** none

When work begins, each in-flight cluster gets a row here:

```
### Cluster X — <title>
- **Phase:** N of M
- **Last completed step:** <one line, with date>
- **Next expected step:** <one line>
- **Owner:** <human / agent name>
- **Date last touched:** YYYY-MM-DD
- **Open sub-questions:** <list, or "none">
```

Move the row to "Recent decisions" or a cluster retro on completion.

## Parallel long-term internal refactors

*Not on the Obsidian-parity roadmap. Shaping work that runs alongside cluster work.*

| Project | Plan | Status | Notes |
|---|---|---|---|
| Qutepart-Corbomite fork | [spec](superpowers/specs/2026-04-15-qutepart-corbomite-fork-design.md) + [plan](superpowers/plans/2026-04-15-qutepart-corbomite-fork.md) | Phases 1 + 2 done | Phases 1 (vendor + CMake + smoke test) and 2 (`Corbomite::SourceEditor` shim + visual-line float scroll + cursor/fold API + NoteEditorWidget mount) both landed 2026-04-15. Phase 2 extended `Qutepart` public API with `scrollPositionVisualLine` accessors (logged in library's `PROVENANCE.md`). Next: Phase 3 (public find/replace API) — parallelisable with Cluster E Phase 1. Phases 4–8 run asynchronously. |
| Graffodil adoption | [scouting](superpowers/plans/2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md) | Scouting | Listed as Cluster P in the roadmap above — internal refactor porting libs/forcegraph + libs/canvas onto Graffodil. |

---

## Recent decisions

Append-only. Most recent on top. Archive entries older than ~3 months to `docs/decisions-archive.md` (quarterly).

- **2026-04-20 — Cluster V scoped via brainstorm; spec written; debt split to V.2.** Three-agent recon across editor/reading/workspace subsystems validated the scouting doc but caught several errors: fork Phase 6 does **not** gate the fold gutter (fork Phase 6 is about themes, unrelated), `NoteEditorWidget::cycleViewMode()` does not exist (API is `setViewMode(ViewMode)`), `Editor::cursorInTable()` does not exist (must be added), ReadingView emits `wikiLinkHovered` not `linkHovered` (adapter/rename needed), `DisplayProperty` on math **is already honored** (drop from scope), stacked Notices + tab context-menu residue already shipped in 2026-04-19 odd-jobs (shrinks Phase 5+6). KColorSchemeManager available system-wide (one-line `find_package(KF6ColorScheme)` add). User chose *surface-first* framing (option "a") with the constraint that nothing slips — so V covers the 6 user-visible phases (~5-7 days) and new Cluster V.2 scouting doc captures everything deferred (fold-gutter coordinator, 6× VaultConfig writer routing with merge-unknown helper, persistent metadata cache hookup, autosave delay spinbox, LRU-reopen upgrade, post-V dead-code audit). Architecture decisions baked into spec: new `View::zoomIn/Out/Reset` virtuals with default no-op (graph-view behavior); KColorSchemeManager single-dispatcher via `MainWindow::onSettingsApplied` connected to `CorbomiteSettings::configChanged`; `editor_toggle_mode` defined with `Ctrl+E` + View > Editor Mode submenu radio; new `Markoff::ActionId::SetHeading1..6` + `Ctrl+1..6` for direct-select H1-H6 menu; menu layout adds Edit/View/Format/Heading/Insert/Table with `activeView isa MarkdownView` gates and `cursorInTable()` gate on Table submenu; Phase 6 nominates 7 toast call sites, plan commits to 5 of 7. Next: `superpowers:writing-plans` for V, then execution.

- **2026-04-20 — Dead-code audit + two new scouting clusters (V, W).** Five parallel Explore agents audited editor stack, app shell, vault/storage, plugin system, and canvas/graph for built-but-unreachable features. Pattern confirmed (recent `Corbomite::EmptyView` was representative): roughly 50 features have no UI surface. Findings split: Tier 1+2 (dead app-shell actions + unsurfaced editor actions + Workspace power-features + persistent cache loader + Notice toasts + search UI completion) → **Cluster V** scouting doc. Tier 4 (canvas node creation + tool palette + resize/snap + group ops + force-graph physics + signal wiring + mermaid double-dispatch) → **Cluster W** scouting doc. Tier 3 / Tier 5 (Bases discoverability, plugin-API dead methods like `VaultProxy::modifyBinary` / `SearchProxy` link/tag queries / `secrets()/process()/network()`) deferred to whenever real third-party plugin development materialises. Both new scouting docs at `superpowers/plans/2026-04-20-cluster-{v,w}-*-SCOUTING.md`. INDEX updated. Backlog updated.

- **2026-04-19 — Odd-jobs sweep: 5 small follow-ups shipped.** `Corbomite::EmptyView` in `libs/core/` registered as viewType `"empty"` (Create/Go-to/Close buttons) + `WorkspaceLeaf::setViewState` falls back to it on missing factory — closes Cluster G follow-ups #1 (empty-state) and #2 (unknown-viewType fallback). `View::onTabMenu` default impl + `WorkspaceTabs::requestClose{Tab,Others,ToRight,All}` helpers + `QTabBar::customContextMenu` wiring — closes Cluster G #7. `SearchDSL::CompiledPlan` gained `regexPatterns` + `caseSensitiveTerms`; `SQLiteIndex::searchCompiled` overload post-filters via `QRegularExpression` / `Qt::CaseSensitive` over `notes_fts.content` — closes Cluster D #3 (regex) and #4 (match-case). `ui.views` permission scope clarified in `API-REFERENCE.md` §Permissions (narrow: `ViewRegistrar` only; sidebar `createView()` doesn't need it) — closes the Cluster N open spec question.

- **2026-04-19 — Backlog sweep: six small items struck as already closed by recent commits.** `56d0db85` (stacked Notices), `1d4b3a9a` (CorbomiteConfigVersion.cmake + lastOpenFiles sibling key), `5f858059` (ViewRegistry error-path hardening), `96c59821` (search-result rich snippet rendering), `261fb3cd` (Bases column-reorder persist). `backlog.md` updated inline. No code changes.

- **2026-04-19 — Cluster R + S specs written; 10 audit addenda fill view-header-menu gaps.** Brainstorming session scoped the per-view hamburger menu (the "…" overflow in the ItemView header) after the user noted the markdown hamburger only shows a single unfunctional "Rename…" entry. Audit survey of the 20 Obsidian menu items found 15 had some form of prior documentation (in `views.md`/`workspace.md`/`ui-bundle.md` or in Cluster G/H/Qutepart plans) but were never collected into a menu-wiring cluster, and 10 had zero coverage. Four audit addenda newly document previously-missing core-plugin / feature surfaces (Bookmarks, file-recovery UI, Canvas Export-as-image, Graph Copy-screenshot); six addenda document the modals and platform primitives (`promptForFileRename` / `promptForMove` / `promptForDeletion` / `openWithDefaultApp` / `showInFolder` / merge-file-modal) and the add-file-property menu entry. Design decision: split the work into **Cluster R** (view-header menus, 4-phase ~6-7 days: P1 menu substrate alignment including `View::onMoreOptionsMenu` hook + `MenuSectionHelper` canonical-order rename `title→close`, `action-primary→pane`, add `find`+`view.linked`, new `addSubmenu`; P2 `EditableFileView` universal file-menu items including real `promptForFileRename` modal replacing the stub + 9 other entries; P3 per-view specialisations on `MarkdownView`/`CanvasFileView`/`GraphView`; P4 inline backlinks-in-document via Cluster J's PostProcessorRegistry) and **Cluster S** (Bookmarks core plugin, single-phase ~5 days: `.obsidian/bookmarks.json` round-trip with unknown-key preservation, right-dock `BookmarksView` on `BookmarksModel`, 7 `bookmarks:*` commands, "Bookmark…" modal with group picker). Four items ship in R as **disabled placeholders** whose tooltips cite the blocking cluster: "Open in new window" (G#6), "Find…"/"Replace…" (Qutepart fork P3), "Open version history" (deferred Cluster T — file-recovery plugin), "Bookmark…" (Cluster S — goes live when S ships). "Open linked view" submenu in R P3 uses **interim dispatch** to each plugin's `:open` command (focuses dock panel) until Cluster G follow-up #3 (`Workspace::openLinkText`) lands; then submenu upgrades to real leaf-opening without menu changes. "Merge entire file with…" explicitly dropped as deferred (low-value). `MenuSectionHelper` has zero production callers today (Cluster H shipped it but FileExplorerPanel's reference got absorbed into the plugin migration), so the section-key renames are cost-free. 11 doc edits: 10 new addenda + 2 new specs + `00-taxonomy.md` addenda index + `PROJECT-STATE.md` roadmap table (added R/S/T rows; heading updated 16→19 clusters) + Cluster G follow-ups #3/#6 annotated R-blocking-partial + Cluster H follow-up #2 annotated "partially absorbed by R" + this Recent-decisions entry + `docs/superpowers/plans/INDEX.md` (to be updated). **Next:** `superpowers:writing-plans` for R, then execution via `superpowers:executing-plans` or parallel dispatch. S follows after R P1 lands. How to apply: any future view-header-menu additions go through `onMoreOptionsMenu(MenuSectionHelper &helper)`; the full section-assignment rulebook is in §4.2 of the R spec. Audit reference discipline: if implementation surfaces a new Obsidian behaviour, add an addendum at `obsidian-audit/addenda/YYYY-MM-DD-<topic>.md` per `addenda/README.md` — never edit the original domain docs.

- *(Entries for 2026-04-15 through 2026-04-17 rolled off to `docs/decisions-archive.md` on 2026-04-19. See that file for Q.0, Q, K, N, G-Part1, J, and E phase closeouts.)*

---

## Open questions blocking progress

Questions that need human input before the linked work can proceed.

- **Should Cluster Q be expanded to a full plan via brainstorm now?** User directed next focus (2026-04-16) to internal-plugin wrapping: an `InternalPlugin` `Component` wrapper around built-in features (FileExplorer / Search / Backlinks / Outlinks / Outline / Properties / LocalGraph), `core-plugins.json` persistence, Settings "Core plugins" toggle page, and an explicit plugin-permissions system ("this plugin wants a, b, c — allow?") that gates a broader-than-community-Plugin surface. Design input captured at `memory/project_cluster_q_permissions.md`. Next session step: dispatch `superpowers:brainstorming` against that input + Cluster G done state. New cluster slot allocated as Q (existing M/N/O/P retained as-is to avoid semantic collision with the pre-existing "feature audits" / "plugin-ready surfaces" / "query layer" / "Graffodil adoption" scopes).

*(No open questions. The Vault/VaultModel tension was resolved by Cluster Q.0 — canonical `Corbomite::Vault` + `FileManager` in `libs/vault/` replace VaultModel entirely; `VaultProxy` + `FileManagerProxy` facade them with permission gating. See `docs/cluster-retros/cluster-q0.md`.)*

When questions arise, format:
```
### <one-line question>
- **Blocks:** <cluster ID + phase, or "general planning">
- **Context:** <one paragraph — why we need this answer>
- **Asked:** YYYY-MM-DD by <session/agent>
- **Resolved:** YYYY-MM-DD with answer "<one-line answer>" (then move to Recent decisions)
```

---

## Pointers

- **Audit reference (canonical):** `docs/obsidian-audit/` — taxonomy + 15 domain docs + 5 synthesis docs + 2 running lists. Treat as read-only except for *additions* via `addenda/`.
- **Cluster plans:** `docs/superpowers/plans/` — one file per cluster, named `2026-MM-DD-cluster-<letter>-<title>.md`. Index at `docs/superpowers/plans/INDEX.md`.
- **Rituals (how to update this file and others):** `docs/CONTRIBUTING-OPS.md`.
- **Cluster retrospectives:** `docs/cluster-retros/cluster-<letter>.md` — written when a cluster lands fully.
- **Unified backlog (map):** `docs/backlog.md` — every deferred follow-up, not-started cluster, and known-flaky test, grouped by theme. Read before picking up new work.
- **Decisions archive (journal):** `docs/decisions-archive.md` — append-only closeout summaries + rolled-off decisions. Consult for *why* a prior call was made.
