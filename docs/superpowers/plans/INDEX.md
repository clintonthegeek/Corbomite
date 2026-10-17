# Cluster plans — index

Table of contents. [`../../PROJECT-STATE.md`](../../PROJECT-STATE.md) is the status page. When this file disagrees with it, PROJECT-STATE wins.

**Last updated:** 2026-10-17. Punch-list §Next closed and Cluster M6 (M6.2 canvas rename rewriting, M6.3 missing-file placeholder) landed in code. M6's live-eyeball check is queued in the punch-list; the current action is trajectory step 4, the punch-list backlog. [`2026-10-17-markoff-vnext-finish.md`](2026-10-17-markoff-vnext-finish.md) is the record of landed Markoff work. Do not re-execute its "Already done" section. Do not dispatch [`2026-06-10-road-to-dogfood.md`](2026-06-10-road-to-dogfood.md); its Phases 2–6 were already stale on 2026-10-01.

The long status essays, the last-updated cascade, and the closed-cluster writeups are frozen at [`../../archive/2026-10-17/cluster-index-before-slim.md`](../../archive/2026-10-17/cluster-index-before-slim.md). Do not execute that file.

## Step 3 — after punch-list §Next

| Cluster | Plan | Status |
|---|---|---|
| M | [Canvas authoring](2026-08-19-cluster-m-canvas-authoring-parity.md) | M0–M5 and M6.1 are done. Next, when step 3 opens: **M6.2** rename rewriting, then **M6.3** missing-file placeholder. Do not start while step 1 or step 2 is open. |

Cluster L is closed. The 2026-10-17 soak findings are punch-list `[cluster-l]` items. Do not re-run the soak.

## Later — not current

| Cluster | Plan | Status |
|---|---|---|
| F-a | [File Recovery and Note Composer](2026-10-01-cluster-f-a-file-recovery-and-composer.md) | Plan written 2026-10-01. Not started. Do not dispatch until PROJECT-STATE reaches step 4. |
| Q | [Tags pane](2026-10-01-cluster-q-tags-pane.md) | Plan written 2026-10-01. Not started. Post-reset Q, not legacy Cluster Q. |
| D.5 | Spec section inside the [archived Cluster D plan](archive/2026-04-26-cluster-d-bases-ui-completion.md) | Bases plugin API. Not a live plan. Do not dispatch from the archive. |
| F-b | [Named workspaces and sidedock](2026-10-01-cluster-f-b-workspaces-and-sidedock.md) | Not dispatchable. Named layouts are `.obsidian/plugins/workspaces/data.json`. There is no `.obsidian/workspaces.json`. The plan body still uses the old name; its banner is the correction. |

Cluster O is closed (O0–O7). Leftover inspector slots are punch-list backlog items, not a next phase.

## Closed

One line each. Essays are in the archive snapshot and in [`../../decisions-archive.md`](../../decisions-archive.md).

| Cluster | Closed | Where |
|---|---|---|
| A | 2026-04-27 | [plan](archive/2026-04-26-cluster-a-vault-format-compat.md) |
| B | 2026-04-28 | [plan](archive/2026-04-28-cluster-b-plugin-api-surface.md) |
| C | 2026-04-27 | [plan](archive/2026-04-26-cluster-c-workspace-serializer.md) |
| D | 2026-04-26 stub; D.1–D.4 shipped later | [plan](archive/2026-04-26-cluster-d-bases-ui-completion.md). D.5 is the Later row above. |
| E | 2026-10-01, retired, not executed | [plan](archive/2026-04-26-cluster-e-markoff-editor-api-parity.md) |
| F | 2026-10-01, split into F-a and F-b | [plan](archive/2026-04-26-cluster-f-internal-plugin-gap-fill.md) |
| G, H, J | 2026-05-25, obsolete after the foundation rewrite | [G](archive/2026-04-26-cluster-g-markoff-phase-c8.md), [H](archive/2026-04-26-cluster-h-block-substitution-widgets.md), [J](archive/2026-04-26-cluster-j-qutepart-fork.md). Do not dispatch. |
| I | 2026-08-20, absorbed by O | [plan](archive/2026-04-26-cluster-i-editor-workspace-ui-surfacing.md) |
| K | 2026-08-18 | [plan](archive/2026-08-15-cluster-k-markoff-canvas-adoption.md) |
| L | 2026-10-17 | [plan](archive/2026-08-17-cluster-l-workspace-stabilization.md). L0–L5 closed. Soak findings stay on the punch list. |
| N | 2026-08-20 | [plan](archive/2026-08-20-cluster-n-rich-clipboard.md) |
| O | 2026-10-02, O0–O7 closed; leftovers are backlog | [plan](2026-08-20-cluster-o-context-sensitive-ui.md) |
| P | 2026-08-21 | [plan](archive/2026-08-20-cluster-p-shared-libraries.md) |

## Reading order

PROJECT-STATE, then the punch list through `## Next`, then the plan that step names. Do not pick a row from this index because it looks active.

Post-reset letters start at A (2026-04-26). A letter that also appears in `archive/` from before that date is "legacy Cluster X".
