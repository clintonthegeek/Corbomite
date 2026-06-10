# Cluster retrospectives

> **Append-only directory.** One file per landed cluster, written per Ritual 3 in `docs/CONTRIBUTING-OPS.md`. Provides historical record of what changed during implementation vs the original cluster plan.

## File naming

`cluster-<letter>.md` — e.g. `cluster-a.md`, `cluster-b.md`. Lowercase letter, no date prefix. **Caution: cluster letters are NOT globally unique.** Lettering restarted at A on 2026-04-26 (post-reset scheme), so a pre-reset (legacy) letter can collide with a post-reset one — existing retros here are mostly legacy-letter clusters. Qualify per the INDEX convention ("legacy Cluster Y" vs "Cluster Y (post-reset)"); if a post-reset cluster collides with an existing retro filename, disambiguate the new file's name and say so in its header.

## File format

```markdown
# Cluster <X> retrospective

**Cluster:** <X> — <title>
**Plan file:** `superpowers/plans/<file>.md`
**Started:** YYYY-MM-DD
**Completed:** YYYY-MM-DD
**Phases delivered:** N of M (note any skipped/deferred)

## What changed vs the plan

<one paragraph: how implementation diverged from the plan, why, and whether the divergence is documented in the plan as a revision or stayed implicit>

## What surprised

<one paragraph: any audit-doc fact that turned out wrong, any KDE/Qt prior art that was unexpectedly useful or useless, any sub-task that took dramatically more or less time than estimated, any bug discovered along the way>

## Downstream effects

<one paragraph: which Roadmap statuses changed (clusters newly unblocked, STUB plans worth expanding now, deferred items now appropriate to start). Cite cluster IDs.>

## Lessons for the next cluster

<optional one paragraph: process improvements, documentation gaps closed, prior-art search refinements, agent-prompt template changes>
```

## Scope

Cluster retros are about *the cluster's process and outcomes*, not about Obsidian. Facts about Obsidian go in `docs/obsidian-audit/addenda/`.

Length: 200–500 words per retro.
