# Cluster I (post-reset) — Editor & Workspace UI Surfacing (retro)

> **Letter collision warning.** Two different clusters have carried the letter I.
> This file is the **post-reset** Cluster I (*Editor & Workspace UI Surfacing*,
> re-lettered from legacy Cluster V at the 2026-04-26 reset). The **pre-reset**
> Cluster I (*MetadataCache parity*, closed 2026-04-15) has its own retro at
> [`cluster-i.md`](cluster-i.md). They are unrelated.

**Closed:** 2026-08-20 — by absorption, not by execution.
**Plan:** [`superpowers/plans/archive/2026-04-26-cluster-i-editor-workspace-ui-surfacing.md`](../superpowers/plans/archive/2026-04-26-cluster-i-editor-workspace-ui-surfacing.md)
**Spec:** [`superpowers/specs/2026-04-20-cluster-v-editor-workspace-ui-surfacing-design.md`](../superpowers/specs/2026-04-20-cluster-v-editor-workspace-ui-surfacing-design.md)
**Execution retro for the part that actually shipped:** [`cluster-v.md`](cluster-v.md)

## Disposition

This cluster never ran under its own letter. Its six phases resolved as:

| Phase | Disposition |
|---|---|
| 1 — App-shell action wiring | **Shipped 2026-04-20 as legacy Cluster V** (`c0e63f44`..`037d536e`). See `cluster-v.md`. |
| 2+3 — Markoff editor menu tree | **Shipped 2026-04-20 as legacy Cluster V** (`a5224b20`..`127530f4`). |
| 4 — ReadingView interactions | **Absorbed into Markoff Phase C5** (2026-04-20). Substrate (`libs/readingview`) has since been deleted by the 2026-05-25 foundation port. |
| 5 — Workspace power-features | **Split.** Split-right/down, popout, reopen-closed shipped in **Cluster L Phase L4** (2026-08-17/18) as `split_right`, `split_down`, `tab_move_to_new_window`, `tab_undo_close`. The tail — **link-with-active-pane** (checkable, over `WorkspaceLeaf::setGroup`) and **move-tab-left/right** (Ctrl+Shift+PgUp/PgDn) — **folded into Cluster O as task O5.T5** (2026-08-20). |
| 6 — Search UI toggles + toasts | **Punch-listed** 2026-08-20 as two P3 items (`[search][P3][ex-cluster-i]` regex/match-case toggles; `[ui-bundle][P3][ex-cluster-i]` `Notice::post` at 5 failure sites). Not action-context work, so not Cluster O's. |
| 7 — Closeout | This file. |

## Why it stayed open for four months

The plan file carried a **false header** — "phases not yet executed" — while Phases
1–4 had in fact shipped under the cluster's *previous* letter (V) five days after the
plan was re-lettered. The 2026-06-10 docs audit caught it and stapled a `⚠` banner to
the top, but the cluster was never formally closed, so it kept appearing in
`PROJECT-STATE.md`'s active table as "Needs rewrite" for another ten weeks.

**Lesson (procedural, worth generalising):** when a cluster is re-lettered, the retro
and the closeout must be re-pointed in the same commit as the rename. A re-lettered
plan whose work shipped under the old letter is invisible to every status ritual —
the plan looks unexecuted, the retro looks orphaned, and only a full-tree audit
reconnects them. The 2026-04-26 reset re-lettered several clusters; this is the one
that cost the most.

**Second lesson:** the letter collision itself (two Cluster I retros) is a direct
cost of resetting the lettering scheme without namespacing. Post-reset artifacts
should have carried a distinguishing suffix from the start.

## What Cluster O inherits

Cluster O ([plan](../superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md))
supersedes this cluster's *framing*, not just its remnant. Cluster I's thesis was
"surface built-but-unreachable features by adding actions to a static menu tree."
That thesis is what produced the current problem: ~40 markdown-shaped actions in one
flat surface, 17 of them permanently disabled, with no notion of which document type
they apply to. Cluster O's thesis is the correction — actions belong to a document
type, and the chrome is a function of what is focused.

Two concrete artifacts of Cluster I that Cluster O re-lights rather than replaces:

- **`View::zoomIn/zoomOut/zoomReset`** — Phase 1 added these base virtuals precisely
  so app-shell actions could dispatch to whichever view is active. The port left
  `MarkdownView`'s overrides as empty `TODO`s and `MainWindow` bypassing them
  entirely. Cluster O task **O1.T3** restores the dispatch.
- **`tst_mainwindow_action_wiring`** — Phase 2's introspection test over
  `actionCollection()`. Cluster O extends the same technique into
  `tst_action_context_no_silent_noop`, which is O1's acceptance gate.

## Follow-ups leaving this cluster

- `[search][P3][ex-cluster-i]` — regex + match-case toggles in the search panel.
- `[ui-bundle][P3][ex-cluster-i]` — `Notice::post` at the 5 swallowed-error sites.
  **Paths recorded by Cluster I predate several refactors — re-confirm each before
  editing.**
- Cluster O **O5.T5** — link-with-active-pane, move-tab-left/right.
