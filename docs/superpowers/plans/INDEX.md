# Cluster Plans — Index

> **Living document.** Table of contents over `docs/superpowers/plans/cluster-*.md`. One row per cluster plan. Status mirrors `docs/PROJECT-STATE.md` Roadmap — when those diverge, **PROJECT-STATE is authoritative.** Update the Status column here per Ritual 3 (cluster done) or Ritual 2 (mid-cluster status change).

**Last updated:** 2026-04-14 — initial creation.

## Plans

| Cluster | Title | Plan file | Type | Status | Key dependencies |
|---|---|---|---|---|---|
| A | Link / frontmatter correctness | [2026-04-14-cluster-a-link-frontmatter-correctness.md](2026-04-14-cluster-a-link-frontmatter-correctness.md) | Full | Not started | none — keystone |
| B | Vault I/O | [2026-04-14-cluster-b-vault-io.md](2026-04-14-cluster-b-vault-io.md) | Full | Not started | depends weakly on A |
| C | Lifecycle / plugin primitives | [2026-04-14-cluster-c-lifecycle-plugin-primitives.md](2026-04-14-cluster-c-lifecycle-plugin-primitives.md) | Full | Blocked on B Phase 1–2 | resolves vault-switch crash |
| D | Search / suggester parity | [2026-04-14-cluster-d-search-suggester-parity.md](2026-04-14-cluster-d-search-suggester-parity.md) | Full | Not started | weakly blocks on A |
| E | Markoff three-mode pivot | [2026-04-14-cluster-e-markoff-three-mode-pivot.md](2026-04-14-cluster-e-markoff-three-mode-pivot.md) | Full | Blocked on B Phase 3 + A Phase 1 | |
| F | Templates / Daily Notes / Moment | [2026-04-14-cluster-f-templates-daily-notes-moment-STUB.md](2026-04-14-cluster-f-templates-daily-notes-moment-STUB.md) | Stub | Stub plan | expand after A + I land |
| G | Views hierarchy + TextFileView contract | — | — | Plan-needed | write after A–E start landing |
| H | Menus / hover / suggester UI | — | — | Plan-needed | write after A–E start landing |
| I | MetadataCache parity | [2026-04-14-cluster-i-metadatacache-parity-STUB.md](2026-04-14-cluster-i-metadatacache-parity-STUB.md) | Stub | Stub plan | expand after A + C in flight |
| J | Embed / rendering primitives | [2026-04-14-cluster-j-embed-rendering-primitives-STUB.md](2026-04-14-cluster-j-embed-rendering-primitives-STUB.md) | Stub | Stub plan | expand after E + I in flight |
| K | Bases | — | — | Plan-needed (blocked) | controller-side DSL extraction first (PROJECT-STATE follow-up #3) |
| L | Properties panel | — | — | Deferred | normal task after A/B/I/C |
| M | Internal-plugin feature audits (Graph, Canvas) | — | — | Deferred | two normal tasks |
| N | Plugin-ready surfaces | — | — | Deferred | builds incrementally on B + C |

## Conventions

- **Full plans** follow the structure: goal, audit references, target classes, KDE/Qt prior art (with `~/src/kde/src/<repo>` paths), work breakdown phases, Explore-agent dispatch prompts, definition of done, blocks/enables, preserved-compat-quirks.
- **Stub plans** are sketches — same structure, smaller. Mark `STUB` in filename until expanded.
- **Status values** must match PROJECT-STATE roadmap statuses verbatim.
- **Plan-needed** = no plan file exists yet and writing one is the next planning task.
- **Deferred** = the audit/synthesis explicitly recommended treating as a normal implementation task without a cluster plan.

## Reading order

If you have to start somewhere blind: read PROJECT-STATE first, then the cluster plan(s) marked "Not started" or "In progress" in PROJECT-STATE, then the audit references those plans cite.
