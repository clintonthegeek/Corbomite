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

**Second-sweep remediation, batch 3 complete.** Batches 1–2 landed 2026-10-01
(see Recent decisions): `ThemeService` minimum-viable revival, release-hygiene
A.2/A.3/A.4 + E.1, the `MarkoffAdapters` / `MarkoffRenderEngine` /
`PluginInstance` deletions, the dead Kate session-restore cluster in
`CorbomiteMDI`, and the test-command fix. **Suite 328/328.** ~500 lines of dead
code removed, 4 punch-list items closed, 2 new tests added (both
falsifiability-checked).

**Next, in order:**

1. **Cluster O Phase O5** (`BasesViewActions` / `GraphViewActions`) — the
   declared next cluster work, and the first thing in a while that is
   user-facing rather than hygiene.
2. **Wire, don't delete, the remaining 9 test-only islands.** The important
   distinction the batch established: `Hotkey`/`HotkeyFile` *are* the
   hotkeys.json subsystem and `VaultScanner`+`IgnoreFilter` *are* the
   user-ignore-filters chain. Both features are still wanted and still
   un-wired, so those islands are the *implementations waiting to be connected*,
   not deletable dead weight. Deleting them would remove the only code that
   could satisfy those punch-list items. Only `EmbedDepthGuard` and
   `RegexRenderEngine` look like genuine delete candidates.
3. **`eState.scroll` interop fix** (P1) — Corbomite writes a 0.0–1.0 fraction
   into an Obsidian-shared field that Obsidian reads as a line number. Convert
   on write, or denylist the field.
4. **Steer Markoff** for (a) a local, D2-undo-integrated frontmatter edit API
   and (b) a `LABELS "benchmark"` on the two canvas perf tests. Both cross-repo.
5. **Split Cluster F** into F-a (File Recovery + Note Composer) / F-b
   (Workspaces + sidedock substrate); file the Tags pane as a real cluster;
   retire Cluster E.
6. `resolveSubpath` is the tested Obsidian-parity implementation with zero
   production callers while an unfaithful regex scrubber ships — needs the
   render path given a `Markoff::Document`, then retire the scrubber.

Not started, deliberately: release-hygiene B (mmdr reproducibility), C
(remaining dead code), D (renames), E (packaging remainder).

**Two corrections this work made to its own audit** — a "fix" whose test passed
against unfixed code, and a fix its own test caught, both reverted rather than
shipped; and two entries in the appraisal's dead-code list (`SpatialGrid`,
`ParseResult`) that were **live** and would have broken things. The lesson is in
the audit's §10 and the punch list's 2026-10-01 section: a gate never seen red
proves nothing, and a symbol whose only references live in a header you did not
read is not dead.

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

Build/test: `cmake --build --preset dev -j 10`. Tests: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 10`. Offscreen is mandatory. **The `-E "benchmark|perf"` exclusion is load-bearing** (changed 2026-10-01): it drops all four wall-clock timing tests. `tst_benchmark_layout` carries a native `benchmark` label, but the two `tst_canvas_perf_*` tests come from the markoff-family submodule with no label, and they flake red under `-j10` purely from machine load while passing standalone every time. Without the exclusion, "the suite is green" is not a reliable signal. The proper fix is a `LABELS "benchmark"` in markoff-canvas, filed. Work on `master` (no feature branches). **Never `git add -A`** — `testvaults/` is deliberately dirty; stage by path. Pre-pull.
