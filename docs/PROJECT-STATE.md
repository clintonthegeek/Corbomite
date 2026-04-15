# PROJECT-STATE

> **Living document.** Single source of truth for "where we are right now" on the Obsidian-compatibility roadmap. Keep under 200 lines. Update at the end of every meaningful work session per the Ritual 2 / Ritual 3 checklists in `docs/CONTRIBUTING-OPS.md`. Older "Recent decisions" entries archive to `docs/decisions-archive.md` quarterly.

**Last updated:** 2026-04-15 — **Clusters A + B (fully inclusive of 3b) DONE.** Phase 3b harvested KDevelop's Sublime::AreaIndex as `libs/core/PaneLayout`, built `PaneLayoutBridge` (QSplitter↔PaneLayout), rewired SessionManager to write `.obsidian/workspace.json` with `_corbomite` unknown-key namespace for Qt-specific state. 47/47 tests green.

---

## Current focus

**Idle — Clusters A + B (incl. 3b) complete.** App now writes `.obsidian/workspace.json` on session save (no more `.corbomite/session.json`). `.corbomite/` remains for derived caches only (SQLite index). Ready to start the next cluster. Per dependency graph: **C (Lifecycle / plugin primitives)** unblocked; **D (Search / suggester)** unblocked; **H (menus/hover/suggester UI)** parallelisable. Recommended next: C (unblocks the bulk of the remaining roadmap) or H (self-contained, parallelisable).

---

## Roadmap (16 clusters — 14 parity + 1 post-parity + 1 internal refactor)

Status legend: `Not started` · `Plan-needed` (no cluster plan yet) · `Stub plan` (sketch exists, expand before dispatch) · `In progress (phase N)` · `Blocked — waiting on X` · `Done` · `Deferred`.

| Cluster | Title | Plan | Status | Blocks / Notes |
|---|---|---|---|---|
| A | Link / frontmatter correctness | [full](superpowers/plans/2026-04-14-cluster-a-link-frontmatter-correctness.md) | Done | Unblocks D, F, I, J, K, L |
| B | Vault I/O | [full](superpowers/plans/2026-04-14-cluster-b-vault-io.md) | Done | Unblocks C, E, G, H, N |
| C | Lifecycle / plugin primitives | [full](superpowers/plans/2026-04-14-cluster-c-lifecycle-plugin-primitives.md) | Not started | Blocked — waiting on B Phase 1–2 (DataAdapter + VaultConfig). Also resolves vault-switch crash |
| D | Search / suggester parity | [full](superpowers/plans/2026-04-14-cluster-d-search-suggester-parity.md) | Not started | Weakly blocked on A (LinkUtils for heading-match search) |
| E | Markoff three-mode pivot | [full](superpowers/plans/2026-04-14-cluster-e-markoff-three-mode-pivot.md) | Not started | Blocked — waiting on B Phase 3 (WorkspaceState) and A Phase 1 (frontmatter parsing) |
| F | Templates / Daily Notes / Moment | [stub](superpowers/plans/2026-04-14-cluster-f-templates-daily-notes-moment-STUB.md) | Stub plan | Expand after A + I land |
| G | Views hierarchy + TextFileView contract | [scouting](superpowers/plans/2026-04-14-cluster-g-views-hierarchy-SCOUTING.md) | Scouting doc | Expand to full plan when Cluster C Phase 1 lands |
| H | Menus / hover / suggester UI | [full](superpowers/plans/2026-04-14-cluster-h-menus-hover-suggester-ui.md) | Not started | Parallelisable with A/B/D; blocked softly on C Phase 1 (Component base) |
| I | MetadataCache parity | [stub](superpowers/plans/2026-04-14-cluster-i-metadatacache-parity-STUB.md) | Stub plan | Expand after A lands and C is in flight |
| J | Embed / rendering primitives | [stub](superpowers/plans/2026-04-14-cluster-j-embed-rendering-primitives-STUB.md) | Stub plan | Expand after E lands and I is in flight |
| K | Bases | [scouting](superpowers/plans/2026-04-14-cluster-k-bases-SCOUTING.md) | Scouting doc | Blocked — expand when Bases DSL extraction addendum lands (follow-up #3 below) |
| L | Properties panel | — | Deferred | Treat as normal implementation task when A/B/I/C are done |
| M | Internal-plugin feature audits (Graph, Canvas) | — | Deferred | Treat as two normal tasks; no cluster plan needed |
| N | Plugin-ready surfaces | — | Deferred | Builds incrementally on top of B + C; no upfront plan |
| O | Advanced query layer (graph + enriched FTS) | [scouting](superpowers/plans/2026-04-14-cluster-o-query-layer-SCOUTING.md) | Scouting doc | Post-parity. Expand only after A/B/I/K land and demand signals materialise |
| P | Graffodil adoption (libs/forcegraph + libs/canvas) | [scouting](superpowers/plans/2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md) | Scouting doc | Internal refactor. Parallelisable with parity roadmap. Expand after Graffodil API stabilises (2–3 wk observation) + Cluster A lands |

---

## In-flight work items

*(none — Cluster A complete 2026-04-14)*

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

---

## Recent decisions

Append-only. Most recent on top. Archive entries older than ~3 months to `docs/decisions-archive.md` (quarterly).

- **2026-04-15 — Cluster B Phase 3b landed (Sublime pattern harvest).** Harvested KDevelop's `Sublime::AreaIndex` (`~/src/kde/src/kdevelop/kdevplatform/sublime/areaindex.{h,cpp}`, LGPL-2.0+) as the in-memory model for Corbomite's pane layout. Three commits: (1) `libs/core/PaneLayout` — B-tree index with stacked-views-per-leaf (= Obsidian tabs node) + bidirectional JSON round-trip to Obsidian's SplitNode shape; (2) `libs/core/PaneLayoutBridge` — pure `QSplitter` ↔ `PaneLayout` serialization with QWidget-opaque pane handles (testable without the full editor stack); (3) SessionManager rewrite to emit `.obsidian/workspace.json` with `_corbomite` namespace for Qt-specific state (window geometry, sidebar widths, expanded folders). MainWindow + EditorViewManager rewired. Dead code removed: `SessionManager::{buildSplitLayoutJson, encodeSplitterNode}`, `EditorViewManager::{buildSessionState, restoreFromSession, rebuildSplitLayout, restoreTabState}`. Net +2400 LOC library code, −350 LOC app code. How to apply: session state lives at `<vault>/.obsidian/workspace.json` now; `.corbomite/` holds only derived caches. Future tab-group features become small extensions of `PaneLayoutIndex` instead of schema rewrites.
- **2026-04-14 — Cluster B landed (Phases 1-6).** Primitives shipped: `libs/storage/DataAdapter` (abstract FS surface with `WriteHints{mtimeMs}` for echo-suppression), `FileSystemAdapter` implements it with `QSaveFile` atomic writes; `VaultConfig` round-trips `.obsidian/*.json` with unknown-key preservation (core-plugins legacy array→object migration included); `WorkspaceState` reads/writes `.obsidian/workspace.json` 5-variant SplitNode tree (split/tabs/leaf/floating/window/mobile-drawer) with node-level unknown-key preservation; `CaseSensitivityProbe` one-shot bool; `VaultTrash` writes `.trash/<base><suffix>.<ext>` per Obsidian's desktop convention; `IgnoreFilter` glob/regex matcher wired into `VaultScanner`; `VaultProcess::process` for atomic whole-body RMW with per-path mutex serialisation. Full end-to-end test simulates an Obsidian-written vault (app/appearance/core-plugins/community-plugins/hotkeys/workspace/plugin-data) + no-op-save + reload + deep-unknown-key preservation assertions. **Decision: no backward compat for old `.corbomite/session.json`** — only cache data (SQLite index) stays in `.corbomite/`; all config + session state moves to `.obsidian/`. **Deferred as 3b:** wiring `SessionManager` / `EditorViewManager` / `MainWindow` to use `WorkspaceState` instead of their current session.json path. Primitives ready; app-layer integration ~500-1000 LOC. Commits: 0be552c (P1), b03b567 (P2), d681c03 (P3), 7a24ef8 (P4), e633ce2 (P5), + P6. How to apply: future clusters should write config through `VaultConfig`, write note bodies through `VaultProcess::process`, read layout via `WorkspaceState`.
- **2026-04-14 — Cluster A landed (all 3 phases).** Phase 1: `libs/core/LinkUtils` with `stripHeading` (AT regex), `stripHeadingForLink` (PT regex), `resolveSubpath` (block/footnote/heading dispatch). Phase 2: `libs/storage/LinkResolver` (6-step shortest-path-wins + same-folder preference) + `links` schema v1 via `PRAGMA user_version` (adds `subpath` column, includes it in PK so heading-distinct wikilinks are separate rows); old `resolveWikilink`/`m_nameToPath` deleted. Phase 3: `libs/core/FrontMatterWriter` with `QSaveFile` atomic write (`processFrontMatter` contract — read/parse/mutate/stringify/rename-atop, comments dropped per Obsidian compat). P0.1/P0.2/P0.3/P0.5 all addressed; P1.6/P1.7/P1.8 all built. Commits: 6765a5d (Phase 1), f97bbdd (Phase 2), + Phase 3. Tests: 19 LinkUtils + 20 LinkResolver + 16 FrontMatterWriter + all downstream consumers green. One subtle bug found during integration: `QString()` binds as SQLite NULL and violates `NOT NULL DEFAULT ''` columns silently under `INSERT OR IGNORE` — guarded with explicit `.isNull() ? "" : v`. Reason: keystone cluster; blocks D, F, I, J, K, L. How to apply: downstream clusters can now assume wikilink subpaths are distinct in the `links` table and `backlinksFor(target)` returns LinkInfo with `.subpath` populated; frontmatter mutations go through `Corbomite::FrontMatterWriter`, not ad-hoc file I/O.
- **2026-04-14 — Cluster A Phase 1 scope revised: drop `libs/core/FrontMatter`, keep YAML in markoff-parser.** Original plan called for a `Corbomite::FrontMatter` wrapper around yaml-cpp in `libs/core`. During setup, markoff-parser was ported to RapidYAML (ryml, 10–70× faster than yaml-cpp) and grew a full frontmatter API: `Document::{frontmatterRaw, frontmatterSpan, frontmatterHasEofClose, parsedFrontmatter, withFrontmatter}` returning `Markoff::YamlValue` with Obsidian-compat options (null→"", lineWidth 0, YAML 1.2 strict, order-preserving). A second YAML wrapper in `libs/core` would just forward — dropped. Consumers (libs/storage, FrontMatterWriter) use `Markoff::YamlValue` directly. API contract for the port is captured at `libs/markoff-parser/docs/2026-04-14-yaml-api-contract-for-cluster-a.md`. Reason: avoid duplicate surface, keep YAML knowledge in one place, benefit from ryml perf for vault-scanning. How to apply: when future clusters need structured YAML, import `<markoff-parser/YamlValue.h>`, do not reach for yaml-cpp.
- **2026-04-14 — Cluster P scouting doc added: port `libs/forcegraph/` + `libs/canvas/` onto Graffodil.** Graffodil (`~/dev/Graffodil/` v0.1.0, 40+ source files, 12/12 tests passing) is a QGraphicsView-based graph scene framework explicitly designed to subsume both of Corbomite's graph-rendering libraries (see `~/dev/Graffodil/docs/graffodil-design.md` §Boundary Map — our files named as planned consumers). It already contains transplanted versions of our `ForceLayout`/`MultilevelLayout`/`QuadTree`/batch code, plus new capabilities (Sugiyama layout, pluggable `EdgePathStrategy`/`TerminusStyle`, richer tool framework). Net benefit: ~3,500 LOC consolidated, cross-pollination with PlanStan at zero additional cost, material feature gains (Sugiyama, AnchorHighlight, circular layout). Prescribed migration order per the design doc: **Canvas first** (lighter, de-risks framework) → **ForceGraph second** (larger, validates Batch at 10k nodes). Risks: API is evolving (recent `Side`→`Anchor` refactor 2026-04-13) so expansion must wait for 2–3 weeks of API stability. Keeps `CanvasDocument`/`CanvasCommands`/`GraphDataBuilder`/card rendering in Corbomite (Corbomite-specific concerns). Explicit recommendations for expanding Graffodil documented in §Recommendations to expand Graffodil (cached-content hook, group/container nodes, obstacle-aware edge routing, undo-signal contract).
- **2026-04-14 — Cluster O scouting doc added: advanced query layer as a post-parity goal.** Triggered by a Nov-2025 Substack polemic arguing "notes apps make bad AI-memory substrates, use databases." The polemic is right about AI-memory and wrong to generalise to all knowledge systems. Reason: plain-text markdown has won across 50 years for durable portability/longevity reasons. Corbomite's value is being an Obsidian-compatible notes app; swapping markdown for a DB vault would destroy it. BUT — Corbomite's native-C++ substrate genuinely exceeds Obsidian's JS-in-browser substrate for graph/DB workloads, and Obsidian plugins (Dataview, Datacore, Breadcrumbs, JuggL) reveal real unmet power-user demand. Scouting doc captures the "Option 2" path: an **additive, opt-in, never-authoritative** query layer (graph DB + PageRank-weighted FTS + optional vault-mutation transaction log for multi-agent safety) over the unchanged markdown vault. Markdown stays the source of truth; indexes live in `.obsidian/corbomite-indexes/`; a vault opened in Obsidian still works. Explicitly reject "Option 3" (vault-as-DB) as compat-destroying. An "AI-companion SQLite export" sub-feature can ship earlier (~2 wks after A+I) to gauge demand cheaply. Expansion trigger: A/B/I/K landed + user demand visible. KDE prior art: Baloo (`~/src/kde/src/baloo/`) is directly architecturally parallel.
- **2026-04-14 — Cluster H full plan written; Clusters G and K get scouting docs.** H is parallelisable with A/B/D and its prior-art targets (KDevelop hover-tooltips, KDevelop completion popup, KMessageWidget) are ripe for exploration now. G and K defer to full plans because G depends on C Phase 1 signatures and K depends on the Bases DSL extraction. Scouting docs capture prior-art breadcrumbs + architectural questions + rough phasing so full-plan expansion is ~90–240 min instead of green-field. A new convention lands: `*-SCOUTING.md` filenames for "pre-plan notes not ready to dispatch."
- **2026-04-14 — Long-term-state machine adopted.** Standing up `PROJECT-STATE.md` + `CONTRIBUTING-OPS.md` + `plans/INDEX.md` + `obsidian-audit/addenda/` as the four-file persistence system. Reason: audit produced ~94k words of reference; we need a stable cursor to navigate it across sessions. CLAUDE.md is the single entry point. See `docs/CONTRIBUTING-OPS.md` for rituals.
- **2026-04-14 — Cluster F/I/J kept as stub plans.** Won't expand until their dependencies (A–E) are at least in flight. Reason: full plans written now would be re-edited once A–E reveal Corbomite-side surface details.
- **2026-04-14 — Local KDE source convention adopted.** All cluster plans require agents to grep `~/src/kde/src/<repo>` and forbid cloning from `invent.kde.org`. Reason: every KDE repo we cited is present locally; cloning wastes time and risks version drift.
- **2026-04-14 — Cluster C and plugin moved to Wave 3 of the audit.** Originally Wave 1; pushed last so they could cite completed sibling docs. Reason: aggregator domains produce sharper docs against finished consumers. Worked.
- **2026-04-14 — Pass 1 corrections applied silently in Pass 3 synthesis.** Four corrections: `QueryController` is Bases (not search); `App.prototype.on` is no-op (events fire on `app.workspace`); `HoverPopover` hover delay is 300ms (poll interval is 500ms); `PopoverState.js` was mis-extracted (enum reconstructed from use-sites). Synthesis docs reflect corrected facts; Pass 1 file is left as-is for historical record.

---

## Open questions blocking progress

Questions that need human input before the linked work can proceed.

- **None right now.** Roadmap items A and B are unblocked and ready to dispatch.

When questions arise, format:
```
### <one-line question>
- **Blocks:** <cluster ID + phase, or "general planning">
- **Context:** <one paragraph — why we need this answer>
- **Asked:** YYYY-MM-DD by <session/agent>
- **Resolved:** YYYY-MM-DD with answer "<one-line answer>" (then move to Recent decisions)
```

---

## Controller-side follow-ups (out-of-tree extractions)

Outstanding items the audit flagged as needing `_internal.js` grep work outside the audited `obsidian/` tree. None block A–E; F/G/H/K/L progress varies with these.

1. **DOMPurify `SL` allowlist.** Security-critical for plugin HTML sanitisation. Grep `_internal.js` for `ALLOWED_TAGS` / `ALLOWED_ATTR`. *Blocks: full plan for H.*
2. **Turndown `hP` rule set.** Web-clipper paste-from-browser compat. Grep `_internal.js` for `new TurndownService` / `.addRule(`. *Blocks: nothing critical; nice-to-have.*
3. **Bases `DK`/`RK`/`JK`/`PX` formula/filter DSL parser.** Grammar lives outside `bases/`. Authoritative user-facing grammar at `help.obsidian.md/bases/functions`. *Blocks: full plan for K.*
4. **25 unnamed internal-plugin manifest IDs.** Extract from `core/App.js:611-641`. *Blocks: completing FEATURE-MATRIX.md feature catalog.*
5. **`AC` (appearance-keys allow-list) + `PC` (vault-config defaults).** *Blocks: `.obsidian/app.json` + `appearance.json` round-trip in Cluster B.*
6. **Search-panel DSL grammar.** Source is in the out-of-tree `global-search` internal plugin. Reverse-engineer from Obsidian user docs + grep `openGlobalSearch("..."` call sites. *Blocks: Cluster D Phase 4.*

---

## Pointers

- **Audit reference (canonical):** `docs/obsidian-audit/` — taxonomy + 15 domain docs + 5 synthesis docs + 2 running lists. Treat as read-only except for *additions* via `addenda/`.
- **Cluster plans:** `docs/superpowers/plans/` — one file per cluster, named `2026-MM-DD-cluster-<letter>-<title>.md`. Index at `docs/superpowers/plans/INDEX.md`.
- **Rituals (how to update this file and others):** `docs/CONTRIBUTING-OPS.md`.
- **Cluster retrospectives:** `docs/cluster-retros/cluster-<letter>.md` — written when a cluster lands fully.
