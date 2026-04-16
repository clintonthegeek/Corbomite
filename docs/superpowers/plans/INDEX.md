# Cluster Plans — Index

> **Living document.** Table of contents over `docs/superpowers/plans/cluster-*.md`. One row per cluster plan. Status mirrors `docs/PROJECT-STATE.md` Roadmap — when those diverge, **PROJECT-STATE is authoritative.** Update the Status column here per Ritual 3 (cluster done) or Ritual 2 (mid-cluster status change).

**Last updated:** 2026-04-15 — Cluster G Part 1 landed (15 commits): View hierarchy + ViewRegistry + TextFileView + concrete subclasses + EditorViewSpace ViewRegistry wiring. Prior same-session: Clusters I + F + L + E + J landed back-to-back.

## Plans

| Cluster | Title | Plan file | Type | Status | Key dependencies |
|---|---|---|---|---|---|
| A | Link / frontmatter correctness | [2026-04-14-cluster-a-link-frontmatter-correctness.md](2026-04-14-cluster-a-link-frontmatter-correctness.md) | Full | Not started | none — keystone |
| B | Vault I/O | [2026-04-14-cluster-b-vault-io.md](2026-04-14-cluster-b-vault-io.md) | Full | Not started | depends weakly on A |
| C | Lifecycle / plugin primitives | [2026-04-14-cluster-c-lifecycle-plugin-primitives.md](2026-04-14-cluster-c-lifecycle-plugin-primitives.md) | Full | Done (primitives + hookup) | Phase 4b-d held for consumers (see PROJECT-STATE §Follow-ups) |
| D | Search / suggester parity | [2026-04-14-cluster-d-search-suggester-parity.md](2026-04-14-cluster-d-search-suggester-parity.md) | Full | Done | landed 2026-04-15 (5 commits) |
| E | Three-mode pivot (Source/LivePreview/Reading) | [2026-04-14-cluster-e-markoff-three-mode-pivot.md](2026-04-14-cluster-e-markoff-three-mode-pivot.md) | Full | Done | landed 2026-04-15 (18 commits); see `cluster-retros/cluster-e.md` |
| F | Templates / Daily Notes / Moment | [2026-04-15-cluster-f-templates-daily-notes-moment.md](2026-04-15-cluster-f-templates-daily-notes-moment.md) | Full | Done | landed 2026-04-15 (5 phases + doc closeout) |
| G | Views hierarchy + TextFileView contract | [2026-04-15-cluster-g-views-hierarchy.md](2026-04-15-cluster-g-views-hierarchy.md) | Full | Part 1 done | Part 1 landed 2026-04-15 (15 commits); spec at `../specs/2026-04-15-cluster-g-views-hierarchy-design.md`; Part 2 (workspace containers, popout, stacked tabs, pin, history) needs separate spec |
| H | Menus / hover / suggester UI | [2026-04-14-cluster-h-menus-hover-suggester-ui.md](2026-04-14-cluster-h-menus-hover-suggester-ui.md) | Full | Done | landed 2026-04-15 (5 commits) |
| I | MetadataCache parity | [2026-04-15-cluster-i-metadatacache-parity.md](2026-04-15-cluster-i-metadatacache-parity.md) | Full | Done | landed 2026-04-15 (10 commits, 8 phases) |
| J | Embed / rendering primitives | [2026-04-15-cluster-j-embed-rendering.md](2026-04-15-cluster-j-embed-rendering.md) | Full | Done | landed 2026-04-15 (18 commits, 6 phases); see `cluster-retros/cluster-j.md`; spec at `../specs/2026-04-15-cluster-j-embed-rendering-design.md` |
| K | Bases | [2026-04-14-cluster-k-bases-SCOUTING.md](2026-04-14-cluster-k-bases-SCOUTING.md) | Scouting | Scouting doc (blocked) | expand when Bases DSL extraction addendum lands |
| L | Properties panel | — | — | Done | landed 2026-04-15 (commit 89b1df4) as single-phase normal task |
| M | Internal-plugin feature audits (Graph, Canvas) | — | — | Deferred | two normal tasks |
| N | Plugin-ready surfaces | — | — | Deferred | builds incrementally on B + C |
| O | Advanced query layer (post-parity) | [2026-04-14-cluster-o-query-layer-SCOUTING.md](2026-04-14-cluster-o-query-layer-SCOUTING.md) | Scouting | Scouting doc | additive graph+FTS over markdown vault; **post-parity** |
| P | Graffodil adoption (internal refactor) | [2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md](2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md) | Scouting | Scouting doc | port libs/forcegraph + libs/canvas onto Graffodil; **parallelisable with parity** |

## Parallel long-term internal refactors (not cluster-numbered)

| Project | Plan file | Spec | Status | Key notes |
|---|---|---|---|---|
| Qutepart-Corbomite fork | [2026-04-15-qutepart-corbomite-fork.md](2026-04-15-qutepart-corbomite-fork.md) | [2026-04-15-qutepart-corbomite-fork-design.md](../specs/2026-04-15-qutepart-corbomite-fork-design.md) | Phases 1+2 done (2026-04-15) | 8-phase shaping of vendored `qutepart-cpp`. Phase 1 (vendor) + Phase 2 (`Corbomite::SourceEditor` shim) both landed. Next: Phase 3 (public find/replace API). |

## Conventions

- **Full plans** follow the structure: goal, audit references, target classes, KDE/Qt prior art (with `~/src/kde/src/<repo>` paths), work breakdown phases, Explore-agent dispatch prompts, definition of done, blocks/enables, preserved-compat-quirks.
- **Stub plans** are sketches — same structure, smaller. Enough content that expansion to a full plan is an edit, not a rewrite. Mark `STUB` in filename.
- **Scouting docs** are pre-plan notes: prior-art breadcrumbs, architectural questions, rough phasing. *Not dispatchable.* Used when the cluster is blocked on an external dependency (DSL extraction, another cluster's API shape) and we don't want the context to evaporate. Mark `SCOUTING` in filename. Expansion trigger documented in the doc itself.
- **Status values** must match PROJECT-STATE roadmap statuses verbatim.
- **Plan-needed** = no plan file exists yet and writing one is the next planning task.
- **Deferred** = the audit/synthesis explicitly recommended treating as a normal implementation task without a cluster plan.

## Reading order

If you have to start somewhere blind: read PROJECT-STATE first, then the cluster plan(s) marked "Not started" or "In progress" in PROJECT-STATE, then the audit references those plans cite.
