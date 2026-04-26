# Project state

> Slim session-start orientation. **Reset 2026-04-26** after a full audit (`docs/audit-2026-04-26/`) regrouped everything into a fresh A-onwards cluster scheme + a flat punch list. Pre-reset state archived at `docs/archive-2026-04-26/PROJECT-STATE-pre-reset.md`.

## Two tracks

Work flows through **two parallel tracks**. Both must be checked at session start.

1. **Punch list** — flat severity-ranked list of small fixes. File: [`docs/punch-list.md`](punch-list.md). Pick from top (P0 first). 58 items at reset.
2. **Strategic clusters** — multi-phase coordinated initiatives. Index: [`docs/superpowers/plans/INDEX.md`](superpowers/plans/INDEX.md). 10 active at reset (A–J).

Most P0/P1 punch-list items are **silent vault-format corruption risks**. Drain them before strategic-cluster work unless explicitly redirected.

## Current focus

**P1 #1–#3 (workspace serializer consolidation) done** (2026-04-26). Single hybrid writer (KDDW LayoutSaver JSON for split topology + Workspace for leaf payload) at `libs/core/src/WorkspaceSerializer.cpp`; `Workspace::serialize`/`deserialize` are thin forwarders. 18 fixtures cover the contract. P1 #4 (`m_unknownRoot` left/right write-through) and the `m_tabGroupOf` lag-after-drag follow-up remain in P1; next pick from top of P1 unless redirected.

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

- **2026-04-26 — Workspace serializer consolidation done.** P1 #1, #2, #3 closed. `Workspace::serialize`/`deserialize` delegate to `WorkspaceSerializer::toJson`/`fromJson`. KDDW `LayoutSaver::serializeLayout()` JSON drives split topology; `Workspace::findLeafById` drives per-leaf state; the two join on `DockWidget::uniqueName`. Per-group `currentTab` round-trips via `Core::Group::currentTabIndex()`. Audit addendum [`obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md`](obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md) corrects the stale "no public Group enumeration API" claim. Spec: [`specs/2026-04-26-workspace-serializer-consolidation-design.md`](superpowers/specs/2026-04-26-workspace-serializer-consolidation-design.md). Plan: [`plans/2026-04-26-workspace-serializer-consolidation.md`](superpowers/plans/2026-04-26-workspace-serializer-consolidation.md).
- **2026-04-26 — P0 sweep complete.** All 15 P0 silent-corruption items closed. Last two: `FileManager::renameFile` rewritten to drive surgical edits from MetadataCache positions (covers markdown-style + full-path + frontmatter link forms via a single `rewriteLinkLiteral` helper); `PluginManager` ⇄ `.obsidian/{core,community}-plugins.json` cross-app sync landed per [`specs/2026-04-26-plugin-enable-state-cross-app-compromise.md`](superpowers/specs/2026-04-26-plugin-enable-state-cross-app-compromise.md) — JSON wins on vault-open, KConfig authoritative thereafter, dual-write gated by `X-Obsidian-Id` (manifest field + 7-entry internal alias dict).
- **2026-04-26 — Tracking system reset.** Audit produced 58 punch-list items + 6 audit-derived clusters (A–F). 4 in-flight plans re-lettered (G–J). 8 SCOUTING/one-shot plans archived. `backlog.md` retired. PROJECT-STATE slimmed. Old state at `docs/archive-2026-04-26/`. See [`docs/audit-2026-04-26/README.md`](audit-2026-04-26/README.md) for the audit synthesis.

## Open questions

- Which track to start? (Recommendation: Cluster A — vault-format sweep — covers the silent-corruption P0s under one coordinated test fixture.)
- Should Cluster A be a real cluster or just "drain the P0 punch-list items"? (Defer until first session post-reset — depends on whether shared test fixtures are worth the cluster overhead.)

## Last touched

2026-04-26 — workspace serializer consolidation (P1 #1-#3). See decisions-archive for the full closeout.
