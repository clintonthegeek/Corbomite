# Project state

> Slim session-start orientation. **Collapsed 2026-10-01** (145 → 95 lines) to
> restore the contract: 84% of the old file was two append-only journals
> ("Recent decisions" + an undeclared second one, "Last touched") that
> `decisions-archive.md` already holds in full and in newest-first order. Every
> entry removed here is preserved there. The remaining length is the cluster
> table + current focus; both are orientation, not history. Current health
> report: [`audit-2026-10-01-second-sweep-appraisal.md`](audit-2026-10-01-second-sweep-appraisal.md).
>
> **Rule reaffirmed:** this file states *where we are*, not *what happened*. A
> phase closing goes here as one line and into `decisions-archive.md` as a
> paragraph. Do not add a third journal here.
>
> Pre-reset state archived at `docs/archive-2026-04-26/PROJECT-STATE-pre-reset.md`.

## Current focus

**Drain the punch list's newly-filed 2026-10-01 section before new cluster work.**
The second sweep found that the trackers had drifted for ~6 weeks: `ThemeService`
is a `#if 0` stub with five live consequences, ~12 subsystems have zero production
callers while reporting as tested, and the ~30-task release-hygiene plan was
orphaned at ~7% complete. Start with the P1 items in the punch list's
2026-10-01 section — `ThemeService` revival, then release-hygiene Phase A (legal)
+ E.1 (AppStream metainfo), then the `addFrontmatterRow` undo defect.

**Next cluster work after that:** Cluster O Phase O5 (`BasesViewActions` /
`GraphViewActions`), then Cluster M M5/M6, then resume the paused Cluster L5 soak.

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
| **L** Workspace/KDDW | **L0–L4 closed. L5 PAUSED mid-flight** — 6 soak scenarios left, 0 checkboxes | 2026-08-17 |
| **M** Canvas authoring | **M0–M4 done.** Next: M5, M6 | 2026-08-19 |
| **O** Context-sensitive UI | **O0–O4 CLOSED.** Next: O5 (`BasesViewActions`/`GraphViewActions`) | 2026-08-20 |
| **D** Bases UI | Only D.5 (plugin API) remains — needs spec | Audit |
| **F** Internal plugins | Stub, **largest unstarted user-facing surface.** Split F-a/F-b recommended | Audit |
| **E** Editor API parity | **Obsolete — retire** (~15–30 min to archive) | Audit |

Closed (post-reset): A, B, C, I (by absorption), K, N, O, P.
**Obsoleted by the foundation rewrite (2026-05-25):** G, H, J — J's obsolescence is
now confirmed (Cluster K Phase 5 retired the QML Live leaf J would have forked
around), which closes the long-standing open question below.

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

- **2026-10-01 — Second-sweep appraisal: the code is sound, the trackers were not.**
  4 exhaustive agent passes over code + docs after a ~6-week gap since v0.2.0.
  Findings: the test suite's greenness partly certifies nothing (~12 zero-production-
  caller subsystems carrying 250+ test refs); `ThemeService` is a `#if 0` stub whose
  dead `themeChanged` signal breaks 5 downstream things including two false `[x]`
  punch-list closures; release-hygiene was orphaned at ~7%; the obsidian-audit
  addenda went silent for 2 months, so a frozen doc still teaches a split-direction
  convention that already cost a P0. Also recorded: two of three contested subagent
  claims were **rejected** on verification (`go_back`/`go_forward` work fine; the
  vault save path *is* covered). 17 PARITY-MATRIX rows corrected, 5 punch-list items
  closed, 1 re-ranked P4→P1, 15 filed. Full report + remediation order: the audit doc.
- **2026-08-21 — Cluster P CLOSED (P0–P6).** `build-dev/` 11 GB → 1.8 GB (−84%).
  Caught a real cross-repo regression: re-pinning Markoff to `master` HEAD instead of
  the branch Corbomite's pin sat on silently dropped Cluster N's clipboard API —
  missed twice by incremental rebuilds, caught immediately by the Arch PKGBUILD's
  fresh clone.
- **2026-08-21 — v0.2.0 tagged and released.** Cluster O closed end to end.

Older closeouts (2026-04-26 reset → 2026-08-21) and all full closeout paragraphs:
[`decisions-archive.md`](decisions-archive.md).

---

Build/test: `cmake --build --preset dev -j 10`. Tests: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10` (offscreen is mandatory; `-E benchmark` excludes the by-design timing test). Work on `master` (no feature branches). **Never `git add -A`** — `testvaults/` is deliberately dirty; stage by path. Pre-pull.
