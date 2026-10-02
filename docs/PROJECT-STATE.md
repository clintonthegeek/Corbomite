# Project state

> Slim session-start orientation. **Collapsed twice on 2026-10-01** (145 → 101 →
> 125) to restore the contract: most of the original file was append-only
> journals that `decisions-archive.md` already holds in full, newest-first.
> Everything removed here is preserved there. Current health report:
> [`audit-2026-10-01-second-sweep-appraisal.md`](audit-2026-10-01-second-sweep-appraisal.md).
>
> **Rule, and note it has now been broken twice:** this file states *where we
> are*, not *what happened*, and it regrew +16 (mine) then +11 (a dispatched
> agent's) within one session before being compressed again. A phase closing
> goes here as ONE line and into `decisions-archive.md` as a paragraph. Do not
> add a journal here, and if you add cluster rows, cut something.
>
> Pre-reset state archived at `docs/archive-2026-04-26/PROJECT-STATE-pre-reset.md`.

## Current focus

Remediation sweep. **Suite 340/340.** Tracker: 58 open / 135 closed.

**Done since the second-sweep appraisal** (2026-10-01 → 02): `ThemeService`
revival; release-hygiene A.2/A.3/A.4 + E.1; ~500 lines of dead code; 7
disabled tests re-enabled; cluster bookkeeping (E retired, F split into
F-a/F-b, Tags pane → Cluster Q, D.5 specced, L5 checkboxed); `eState.scroll`
interop (was P1); `.obsidian/app.json` (5 of 6 keys); `resolveSubpath` wired
into the render path. **Cluster O5 complete** — bases' seven toolbar controls
and the graph's five verbs are real KActions, tab reorder + linked panes
landed, and `ViewRegistrar` gained a ViewActions path so a *plugin* can ship
a provider with no new API surface. **O6.T3** fixed the shared
`GraphControlsPanel` singleton.

**Next, in order:**
1. **O7** — Cluster O closeout: rebuild the live-eyeball checklist, retire
   the remaining 18 plan checkboxes explicitly rather than leaving them to
   rot, decide the O6.T1/T2/T4 inspector slot (currently a punch item).
2. **Cluster M5/M6** + leftovers: canvas file-picker via `Vault`; canvas dark
   mode (now actionable since `ThemeService` emits).
3. **Resume Cluster L5 soak** (6 scenarios) and clear the human-eyeball
   backlog. Both need a human.
4. **Dispatch the new plans:** F-a (File Recovery + Note Composer, ~5–8d),
   Q (Tags pane, ~3–5d). F-b and D.5 are spec-first.
5. **Release hygiene B** (mmdr reproducibility), C, D, E — B blocks the
   mermaid item.

**Cross-repo** — written, awaiting dispatch:
[`handoff/2026-10-01-to-markoff-frontmatter-undo-and-test-labels.md`](handoff/2026-10-01-to-markoff-frontmatter-undo-and-test-labels.md)
(local D2-undo-integrated frontmatter edit, stale `Origin.h`, `benchmark`
labels on two canvas perf tests, the never-accepted frontmatter fence steer).

**Investigated and deliberately left open**, reasoning recorded rather than
half-solved: `split dimension` (KDDW exposes **no** public separator-size
setter — three options weighed, recommend accepting it); `hotkeys.json`
(re-scoped to a cluster, not a wiring fix — nothing reads it today and the
substance is an Obsidian-command-id mapping table).

**Standing lesson** — eight of my own claims this session were wrong and were
caught by checking rather than shipped: a "fix" whose test passed against
unfixed code; an undo fix its own test caught; a suspected data-loss bug whose
mechanism was real but consequence inert; an `reinterpret_cast` from
`nullptr`; two dead-code-list entries that were live; a group-id rule taken
from the audit that orphans members; and a first pass at the bases toolbar
test that passed against reverted code. See the audit's §10 — a gate never
seen red proves nothing.

## Two tracks

Work flows through **two parallel tracks**. Both must be checked at session start.

1. **Punch list** — flat severity-ranked list of small fixes. File:
   [`docs/punch-list.md`](punch-list.md). Pick from top (P0 first). Read the
   2026-10-01 section first — it holds the current P1s.
2. **Strategic clusters** — multi-phase coordinated work. Index:
   [`docs/superpowers/plans/INDEX.md`](superpowers/plans/INDEX.md).

Most P0/P1 punch-list items are **silent vault-format-corruption risks**. Drain them
before strategic-cluster work unless explicitly redirected.

**Test baseline (2026-10-01):** 329/330 offscreen. The one red,
`tst_canvas_perf_formatted`, passes standalone — a `-j10` perf-under-load flake, not
a defect.

## Active strategic clusters (snapshot)

| Cluster | Status (detail in INDEX) | Source |
|---|---|---|
| **L** Workspace/KDDW | **L0–L4 closed. L5 PAUSED mid-flight** — 6 soak scenarios now checkboxed | 2026-08-17 |
| **M** Canvas authoring | **M0–M4 done.** Next: M5, M6 | 2026-08-19 |
| **O** Context-sensitive UI | **O0–O4 CLOSED.** Next: O5 (`BasesViewActions`/`GraphViewActions`) | 2026-08-20 |
| **D** Bases UI | Only D.5 — **spec written 2026-10-01, awaiting review** | Audit |
| **F-a** File Recovery + Composer | Plan written 2026-10-01, dispatchable, ~5–8d | 2026-10-01 |
| **F-b** Workspaces + sidedock | Plan written 2026-10-01, **spec-first**, ~6–9d | 2026-10-01 |
| **Q** Tags pane | Plan written 2026-10-01, dispatchable, ~3–5d | 2026-10-01 |

Closed (post-reset): A, B, C, E (retired 2026-10-01, never executed), I (by
absorption), K, N, O, P. **Obsoleted by the foundation rewrite (2026-05-25):**
G, H, J — J's obsolescence is now confirmed (Cluster K Phase 5 retired the QML
Live leaf J would have forked around), which closes the open question below.
**Q is post-reset** (Tags pane), not legacy Cluster Q (closed 2026-04-17) —
qualify when ambiguous. F no longer exists as a dispatchable stub.

Full table + plan-file links: [`docs/superpowers/plans/INDEX.md`](superpowers/plans/INDEX.md).

## Open questions

- **Eyeball-verification backlog.** Eight UI work-packages shipped offscreen-only
  and have never been seen by a human: D.2 grouped rendering, D.3 panels/drawer,
  D.4a cell clicks, D.4b export/+New, D.4c undo + drift banner, formula editor
  popups, filter builder dialog, properties panel. One consolidated session in
  front of the running app would clear all eight. (Completion popup eyeball DONE
  2026-06-11.)
- **Cluster M M4.4's chrome overlay** (handle/dot sizing, colors, hover-vs-selection
  dot visibility) is best-guess pending a live look — the same standing gate.

## Recent decisions

- **2026-10-02 — Cluster O5 complete, O6.T3 landed.** Bases' seven toolbar
  controls and the graph's five verbs are now real KActions; `ViewRegistrar`
  gained a ViewActions path so a **plugin** can ship a provider with no new
  API surface (same `ui.views` permission). O6.T3 fixed the shared
  `GraphControlsPanel` singleton — falsifiability check *segfaulted* against
  the old design, which pinned the defect as shared ownership rather than
  missing disconnects. Three bugs the tests caught: `propertiesPaneVisible()`
  read `isVisible()`; Qt 6 silently clears a button's menu when
  `setDefaultAction()` installs an action carrying one; and the audit's
  "reuse the target's group id" orphans the source's members.
- **2026-10-01 — Tier 2 quick wins.** `eState.scroll` interop (was P1) and
  `.obsidian/app.json` (5 of 6 keys) fixed; the tested subpath resolver now
  serves the render path; Markoff handoff written (3 asks). Two items
  investigated and deliberately left open with the reasoning recorded:
  `split dimension` (KDDW exposes no public separator-size setter) and
  `GraphControlsPanel` (now fixed — see above).
- **2026-10-01 — Dispatched remediation.** 7 disabled tests re-enabled; ~500
  lines of dead code removed; cluster bookkeeping (E retired, F split into
  F-a/F-b, Tags pane → Cluster Q, D.5 specced, L5 checkboxed); a suspected
  data-loss bug **refuted** — the D2 UndoLog does survive an external reload,
  but stale targets make undo inert, so it is a UX wart rather than content
  loss, and the guard I had already written for it was reverted.
- **2026-10-01 — Second-sweep appraisal.** Code sound, trackers not: ~12
  test-only islands, `ThemeService` dead, release-hygiene orphaned at ~7%,
  obsidian-audit addenda silent for 2 months (a frozen doc still taught a
  split-direction convention that had already cost a P0). 17 PARITY-MATRIX
  rows corrected; 2 of 3 contested subagent claims rejected on verification.
- **2026-08-21 — Cluster P closed** (build tree 11 GB → 1.8 GB) and **v0.2.0
  tagged**; Cluster O closed end to end.

Older closeouts (2026-04-26 reset → 2026-08-21) and all full closeout
paragraphs: [`decisions-archive.md`](decisions-archive.md).
