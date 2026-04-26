# Project state

> Slim session-start orientation. **Reset 2026-04-26** after a full audit (`docs/audit-2026-04-26/`) regrouped everything into a fresh A-onwards cluster scheme + a flat punch list. Pre-reset state archived at `docs/archive-2026-04-26/PROJECT-STATE-pre-reset.md`.

## Two tracks

Work flows through **two parallel tracks**. Both must be checked at session start.

1. **Punch list** — flat severity-ranked list of small fixes. File: [`docs/punch-list.md`](punch-list.md). Pick from top (P0 first). 58 items at reset.
2. **Strategic clusters** — multi-phase coordinated initiatives. Index: [`docs/superpowers/plans/INDEX.md`](superpowers/plans/INDEX.md). 10 active at reset (A–J).

Most P0/P1 punch-list items are **silent vault-format corruption risks**. Drain them before strategic-cluster work unless explicitly redirected.

## Current focus

**Idle** at reset (2026-04-26). Awaiting human direction on which track to start: punch-list P0 sweep, or one of the active strategic clusters.

## Active strategic clusters (snapshot)

| Cluster | Title | Status | Source |
|---|---|---|---|
| A | Vault-format compatibility sweep | Plan-needed (stub) | Audit |
| B | Plugin API surface completion | Plan-needed (stub) | Audit |
| C | Workspace serializer fidelity rebuild | Plan-needed (stub) | Audit |
| D | Bases UI completion | Plan-needed (stub) | Audit |
| E | Markoff Editor API parity | Plan-needed (stub) | Audit |
| F | Internal-plugin gap fill | Plan-needed (stub) | Audit |
| G | Markoff Phase C8 (inline-ORC coherence) | In-flight | Carried (was Phase C8 plan) |
| H | Block-substitution widgets | Scouting (blocked on G) | Carried (was Cluster X) |
| I | Editor & Workspace UI surfacing | In-flight | Carried (was Cluster V) |
| J | Qutepart-Corbomite Fork | In-flight (Phases 1+2 done) | Carried (was parallel refactor) |

Full table + plan-file links: [`docs/superpowers/plans/INDEX.md`](superpowers/plans/INDEX.md).

## Recent decisions

- **2026-04-26 — Tracking system reset.** Audit produced 58 punch-list items + 6 audit-derived clusters (A–F). 4 in-flight plans re-lettered (G–J). 8 SCOUTING/one-shot plans archived. `backlog.md` retired. PROJECT-STATE slimmed. Old state at `docs/archive-2026-04-26/`. See [`docs/audit-2026-04-26/README.md`](audit-2026-04-26/README.md) for the audit synthesis.

## Open questions

- Which track to start? (Recommendation: Cluster A — vault-format sweep — covers the silent-corruption P0s under one coordinated test fixture.)
- Should Cluster A be a real cluster or just "drain the P0 punch-list items"? (Defer until first session post-reset — depends on whether shared test fixtures are worth the cluster overhead.)

## Last touched

2026-04-26 — tracking reset. No code changes since `72d53c4d` (cluster-v2 fixup post-review).
