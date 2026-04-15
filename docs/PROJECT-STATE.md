# PROJECT-STATE

> **Living document.** Single source of truth for "where we are right now" on the Obsidian-compatibility roadmap. Keep under 200 lines. Update at the end of every meaningful work session per the Ritual 2 / Ritual 3 checklists in `docs/CONTRIBUTING-OPS.md`. Older "Recent decisions" entries archive to `docs/decisions-archive.md` quarterly.

**Last updated:** 2026-04-14 — Cluster A Phase 2 landed; LinkResolver (6-step shortest-path) + links-schema-v1 (subpath column in PK) + SQLiteIndex rewire. 20 LinkResolver tests + 4 graph tests green.

---

## Current focus

**Cluster A — link/frontmatter correctness, Phase 1 (in progress).** markoff-parser ported from yaml-cpp → RapidYAML (ryml); new `YamlValue` + frontmatter API in `libs/markoff-parser/include/markoff-parser/{Document,YamlValue}.h`. `libs/core/LinkUtils` landed with `stripHeading` (AT regex), `stripHeadingForLink` (PT regex), and `resolveSubpath` dispatching on `^block` / `[^footnote]` / heading; 19 tests green. The original-plan `Corbomite::FrontMatter` wrapper was dropped as redundant — `Markoff::YamlValue` is the single YAML surface, used directly by downstream consumers. **Next:** Cluster A Phase 2 — `libs/storage/LinkResolver` (6-step shortest-path-wins) + `links` table migration to add `subpath` column.

---

## Roadmap (16 clusters — 14 parity + 1 post-parity + 1 internal refactor)

Status legend: `Not started` · `Plan-needed` (no cluster plan yet) · `Stub plan` (sketch exists, expand before dispatch) · `In progress (phase N)` · `Blocked — waiting on X` · `Done` · `Deferred`.

| Cluster | Title | Plan | Status | Blocks / Notes |
|---|---|---|---|---|
| A | Link / frontmatter correctness | [full](superpowers/plans/2026-04-14-cluster-a-link-frontmatter-correctness.md) | In progress (phase 1) | Keystone — blocks D, F, I, J, K, L |
| B | Vault I/O | [full](superpowers/plans/2026-04-14-cluster-b-vault-io.md) | Not started | Blocks C, E, G, H, N |
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

### Cluster A — Link / frontmatter correctness
- **Phase:** 2 of 3 (complete)
- **Last completed step:** `libs/storage/LinkResolver` + `links` table migration to schema v1 (adds `subpath` column + in-PK) + SQLiteIndex rewire + unresolved-target `.md` normalisation — 20 LinkResolver tests + 4 graph tests green, zero regressions (2026-04-14)
- **Next expected step:** Phase 3 — `libs/core/FrontMatterWriter` with `QSaveFile` atomic write + mutator API; wire existing ad-hoc frontmatter edit call-sites through it; end-to-end regression test.
- **Owner:** clinton (pair w/ Claude)
- **Date last touched:** 2026-04-14
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

---

## Recent decisions

Append-only. Most recent on top. Archive entries older than ~3 months to `docs/decisions-archive.md` (quarterly).

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
