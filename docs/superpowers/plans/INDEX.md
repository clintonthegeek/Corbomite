# Cluster Plans — Index

> **Living document.** Table of contents over `docs/superpowers/plans/cluster-*.md`. One row per cluster plan. Status mirrors `docs/PROJECT-STATE.md` Roadmap — when those diverge, **PROJECT-STATE is authoritative.** Update the Status column here per Ritual 3 (cluster done) or Ritual 2 (mid-cluster status change).

**Last updated:** 2026-04-14 — added Cluster H full plan, Clusters G/K scouting docs, Cluster O (post-parity query layer), and Cluster P (Graffodil adoption — internal refactor, parallelisable with parity). Both O and P sit outside the Obsidian-parity roadmap: O is a native-C++-advantage *exceeding* Obsidian after parity; P is an *internal restructuring* that shares Corbomite's graph/canvas code with PlanStan via the Graffodil library.

## Plans

| Cluster | Title | Plan file | Type | Status | Key dependencies |
|---|---|---|---|---|---|
| A | Link / frontmatter correctness | [2026-04-14-cluster-a-link-frontmatter-correctness.md](2026-04-14-cluster-a-link-frontmatter-correctness.md) | Full | Not started | none — keystone |
| B | Vault I/O | [2026-04-14-cluster-b-vault-io.md](2026-04-14-cluster-b-vault-io.md) | Full | Not started | depends weakly on A |
| C | Lifecycle / plugin primitives | [2026-04-14-cluster-c-lifecycle-plugin-primitives.md](2026-04-14-cluster-c-lifecycle-plugin-primitives.md) | Full | Done (primitives + hookup) | Phase 4b-d held for consumers (see PROJECT-STATE §Follow-ups) |
| D | Search / suggester parity | [2026-04-14-cluster-d-search-suggester-parity.md](2026-04-14-cluster-d-search-suggester-parity.md) | Full | Done | landed 2026-04-15 (5 commits) |
| E | Markoff three-mode pivot | [2026-04-14-cluster-e-markoff-three-mode-pivot.md](2026-04-14-cluster-e-markoff-three-mode-pivot.md) | Full | Blocked on B Phase 3 + A Phase 1 | |
| F | Templates / Daily Notes / Moment | [2026-04-14-cluster-f-templates-daily-notes-moment-STUB.md](2026-04-14-cluster-f-templates-daily-notes-moment-STUB.md) | Stub | Stub plan | expand after A + I land |
| G | Views hierarchy + TextFileView contract | [2026-04-14-cluster-g-views-hierarchy-SCOUTING.md](2026-04-14-cluster-g-views-hierarchy-SCOUTING.md) | Scouting | Scouting doc | expand when C Phase 1 lands |
| H | Menus / hover / suggester UI | [2026-04-14-cluster-h-menus-hover-suggester-ui.md](2026-04-14-cluster-h-menus-hover-suggester-ui.md) | Full | Done | landed 2026-04-15 (5 commits) |
| I | MetadataCache parity | [2026-04-14-cluster-i-metadatacache-parity-STUB.md](2026-04-14-cluster-i-metadatacache-parity-STUB.md) | Stub | Stub plan | expand after A + C in flight |
| J | Embed / rendering primitives | [2026-04-14-cluster-j-embed-rendering-primitives-STUB.md](2026-04-14-cluster-j-embed-rendering-primitives-STUB.md) | Stub | Stub plan | expand after E + I in flight |
| K | Bases | [2026-04-14-cluster-k-bases-SCOUTING.md](2026-04-14-cluster-k-bases-SCOUTING.md) | Scouting | Scouting doc (blocked) | expand when Bases DSL extraction addendum lands |
| L | Properties panel | — | — | Deferred | normal task after A/B/I/C |
| M | Internal-plugin feature audits (Graph, Canvas) | — | — | Deferred | two normal tasks |
| N | Plugin-ready surfaces | — | — | Deferred | builds incrementally on B + C |
| O | Advanced query layer (post-parity) | [2026-04-14-cluster-o-query-layer-SCOUTING.md](2026-04-14-cluster-o-query-layer-SCOUTING.md) | Scouting | Scouting doc | additive graph+FTS over markdown vault; **post-parity** |
| P | Graffodil adoption (internal refactor) | [2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md](2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md) | Scouting | Scouting doc | port libs/forcegraph + libs/canvas onto Graffodil; **parallelisable with parity** |

## Conventions

- **Full plans** follow the structure: goal, audit references, target classes, KDE/Qt prior art (with `~/src/kde/src/<repo>` paths), work breakdown phases, Explore-agent dispatch prompts, definition of done, blocks/enables, preserved-compat-quirks.
- **Stub plans** are sketches — same structure, smaller. Enough content that expansion to a full plan is an edit, not a rewrite. Mark `STUB` in filename.
- **Scouting docs** are pre-plan notes: prior-art breadcrumbs, architectural questions, rough phasing. *Not dispatchable.* Used when the cluster is blocked on an external dependency (DSL extraction, another cluster's API shape) and we don't want the context to evaporate. Mark `SCOUTING` in filename. Expansion trigger documented in the doc itself.
- **Status values** must match PROJECT-STATE roadmap statuses verbatim.
- **Plan-needed** = no plan file exists yet and writing one is the next planning task.
- **Deferred** = the audit/synthesis explicitly recommended treating as a normal implementation task without a cluster plan.

## Reading order

If you have to start somewhere blind: read PROJECT-STATE first, then the cluster plan(s) marked "Not started" or "In progress" in PROJECT-STATE, then the audit references those plans cite.
