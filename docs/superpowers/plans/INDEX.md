# Cluster Plans — Index

> **Living document.** Table of contents over `docs/superpowers/plans/cluster-*.md`. One row per cluster plan. Status mirrors `docs/PROJECT-STATE.md` Roadmap — when those diverge, **PROJECT-STATE is authoritative.** Update the Status column here per Ritual 3 (cluster done) or Ritual 2 (mid-cluster status change).

**Last updated:** 2026-04-17 — Cluster Q closed (Tasks 9-21 in one autonomous overnight session). 8 InternalPlugins ship as `.so` modules under `src/plugins/`. Retro at `../../cluster-retros/cluster-q.md`. **Previously:** Cluster Q.0 closed. All 11 phases landed across 2026-04-16 → 2026-04-17 (~50 commits). Canonical `Corbomite::Vault` + `FileManager` in `libs/vault/` replace the old three-way Vault split; `VaultProxy` + `FileManagerProxy` plugin facades; plugin-system types (Plugin / PluginContext / PluginManager / PluginPermissionGrantDialog) co-located in `libs/vault/`. Cluster Q plan retargeted (commit `02f8898`). Retro at `../../cluster-retros/cluster-q0.md`. **Next:** Cluster Q execution — dispatch Tasks 7–20 via a separate session. VaultService deleted, NoteService absorbed into VaultModel, RecentVaults helper extracted, CorbomiteApp now owns the vault lifecycle. 4 commits: `e52c89c` · `c1cc26f` · `cfe975a` · `22e4637`. Phase 9 (plugin proxy layer rewrite) is next. Prior: Phase 7 (consumer migration wave 2), Phase 6 (sidebar panels), Phases 4+5 (Vault configDir + FileManager), Phase 3 (Vault mutation API), Phase 2 (VaultScanner + Watcher inside libs/vault), Phase 1 (libs/vault scaffold).

## Plans

| Cluster | Title | Plan file | Type | Status | Key dependencies |
|---|---|---|---|---|---|
| A | Link / frontmatter correctness | [2026-04-14-cluster-a-link-frontmatter-correctness.md](2026-04-14-cluster-a-link-frontmatter-correctness.md) | Full | Not started | none — keystone |
| B | Vault I/O | [2026-04-14-cluster-b-vault-io.md](2026-04-14-cluster-b-vault-io.md) | Full | Not started | depends weakly on A |
| C | Lifecycle / plugin primitives | [2026-04-14-cluster-c-lifecycle-plugin-primitives.md](2026-04-14-cluster-c-lifecycle-plugin-primitives.md) | Full | Done (primitives + hookup) | Phase 4b-d held for consumers (see PROJECT-STATE §Follow-ups) |
| D | Search / suggester parity | [2026-04-14-cluster-d-search-suggester-parity.md](2026-04-14-cluster-d-search-suggester-parity.md) | Full | Done | landed 2026-04-15 (5 commits) |
| E | Three-mode pivot (Source/LivePreview/Reading) | [2026-04-14-cluster-e-markoff-three-mode-pivot.md](2026-04-14-cluster-e-markoff-three-mode-pivot.md) | Full | Done | landed 2026-04-15 (18 commits); see `cluster-retros/cluster-e.md` |
| F | Templates / Daily Notes / Moment | [2026-04-15-cluster-f-templates-daily-notes-moment.md](2026-04-15-cluster-f-templates-daily-notes-moment.md) | Full | Done | landed 2026-04-15 (5 phases + doc closeout) |
| G | Views hierarchy + TextFileView contract | [Part 1](2026-04-15-cluster-g-views-hierarchy.md), [Part 2](2026-04-15-cluster-g-part2-workspace.md) | Full | Done | Full cluster closed 2026-04-16. Part 1 (15 commits) + Part 2 infra (11 commits) 2026-04-15; Tasks 9-10 (3 commits on top of pre-existing `e3143f1` delete) 2026-04-16. See `cluster-retros/cluster-g.md`. Unblocks M, N. |
| H | Menus / hover / suggester UI | [2026-04-14-cluster-h-menus-hover-suggester-ui.md](2026-04-14-cluster-h-menus-hover-suggester-ui.md) | Full | Done | landed 2026-04-15 (5 commits) |
| I | MetadataCache parity | [2026-04-15-cluster-i-metadatacache-parity.md](2026-04-15-cluster-i-metadatacache-parity.md) | Full | Done | landed 2026-04-15 (10 commits, 8 phases) |
| J | Embed / rendering primitives | [2026-04-15-cluster-j-embed-rendering.md](2026-04-15-cluster-j-embed-rendering.md) | Full | Done | landed 2026-04-15 (18 commits, 6 phases); see `cluster-retros/cluster-j.md`; spec at `../specs/2026-04-15-cluster-j-embed-rendering-design.md` |
| K | Bases | [2026-04-14-cluster-k-bases-SCOUTING.md](2026-04-14-cluster-k-bases-SCOUTING.md) | Scouting | Scouting doc (blocked) | expand when Bases DSL extraction addendum lands |
| L | Properties panel | — | — | Done | landed 2026-04-15 (commit 89b1df4) as single-phase normal task |
| M | Internal-plugin feature audits (Graph, Canvas) | — | — | Deferred | two normal tasks |
| N | Plugin-ready surfaces | — | — | Deferred | builds incrementally on B + C |
| O | Advanced query layer (post-parity) | [2026-04-14-cluster-o-query-layer-SCOUTING.md](2026-04-14-cluster-o-query-layer-SCOUTING.md) | Scouting | Scouting doc | additive graph+FTS over markdown vault; **post-parity** |
| P | Graffodil adoption (internal refactor) | [2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md](2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md) | Scouting | Scouting doc | port libs/forcegraph + libs/canvas onto Graffodil; **parallelisable with parity** |
| Q | Internal-plugin wrapping + permissions | [full](2026-04-16-cluster-q-internal-plugin-wrapping.md) | Full | Done | Closed 2026-04-17 in one overnight session. Tasks 1-6 + 8 had landed earlier. Tasks 9-20 + retro shipped in 13 commits (`8999f88` → `de72cd4`). 8 InternalPlugins under `src/plugins/`; libvault flipped to SHARED for cross-boundary qobject_cast. Retro at `../../cluster-retros/cluster-q.md`. |
| Q.0 | Vault architecture refactor (Cluster Q prerequisite) | [full](2026-04-16-cluster-q0-vault-architecture.md) | Full | Done | Closed 2026-04-17 after 11 phases. Canonical `Corbomite::Vault` + `FileManager` in `libs/vault/` replace the three-way `Corbomite::Vault` stub / `VaultModel` / `VaultService` split; `VaultProxy` + `FileManagerProxy` plugin facades; plugin-system types co-located in `libs/vault/`. Retro at `../../cluster-retros/cluster-q0.md`. Spec at `../specs/2026-04-16-vault-architecture-design.md`. |

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
