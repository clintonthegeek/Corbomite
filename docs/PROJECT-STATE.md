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

**Second-sweep remediation.** Done: the appraisal + tracker reconciliation,
`ThemeService` revival, release-hygiene A.2/A.3/A.4 + E.1, ~500 lines of dead
code, 7 disabled tests re-enabled, cluster bookkeeping (E retired, F split,
Tags pane → Cluster Q, D.5 specced, L5 checkboxed). Suite **337/337**.

**Next, in order:**

1. **Cluster O5** (`BasesViewActions`/`GraphViewActions` + Cluster I tail) — the
   declared next work and the first user-facing item in a while. Then O6, O7.
2. **Tier 2 quick wins** — `eState.scroll` interop (P1, Corbomite writes a
   fraction where Obsidian reads a line number); `app.json` + `hotkeys.json`
   consumption (2× P2, and `Hotkey`/`HotkeyFile` already exist, they are just
   unwired); `resolveSubpath` (the tested Obsidian-parity resolver has zero
   production callers while a regex scrubber ships); `GraphControlsPanel`
   per-instance; split `dimension` apply-to-KDDW.
3. **Cross-repo asks** (write and dispatch, then wait): a Markoff API for a
   local, D2-undo-integrated frontmatter edit; `LABELS "benchmark"` on the two
   canvas perf tests; re-steer the frontmatter fence (the re-pin landed without
   it, so it was not accepted, not pending).
4. **Cluster M5/M6**, plus M's leftovers: canvas file-picker via `Vault`, and
   canvas dark mode (now actionable since `ThemeService` emits again).
5. **Resume Cluster L5 soak** (6 scenarios) and clear the human-eyeball backlog.
6. **Dispatch the new plans:** F-a (File Recovery + Note Composer, ~5–8d), Q
   (Tags pane, ~3–5d). F-b and D.5 are spec-first.

Deliberately not started: release-hygiene B (mmdr reproducibility), C, D, E.

**Standing lesson from this work** — three of my own claims were wrong and were
caught by checking rather than shipped: a "fix" whose test passed against
unfixed code, an undo fix its own test caught, and a suspected data-loss bug
whose mechanism was real but whose consequence was inert. Two entries in the
appraisal's dead-code list were live and would have broken things. See the
audit's §10: a gate never seen red proves nothing, and a symbol whose only
references live in a header you did not read is not dead.

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

- **2026-10-01 — Dispatched remediation.** 7 disabled tests re-enabled
  (328→336); a suspected data-loss bug **refuted** (the D2 UndoLog survives an
  external reload, but stale targets make undo inert — a UX wart, not content
  loss; characterization test enabled in its place, and a host-side guard I had
  already written was reverted); `tst_quadtree` seeded. 337/337.
- **2026-10-01 — Cluster bookkeeping.** E retired; F → F-a/F-b; Tags pane →
  Cluster Q; D.5 specced; L5 checkboxed. Three new dispatchable items.
- **2026-10-01 — Second-sweep appraisal.** Code sound, trackers not: ~12
  test-only islands, `ThemeService` dead, release-hygiene orphaned at ~7%,
  obsidian-audit addenda silent for 2 months (a frozen doc still taught a
  split-direction convention that had already cost a P0). 17 PARITY-MATRIX rows
  corrected; 2 of 3 contested subagent claims rejected on verification.
- **2026-08-21 — Cluster P closed** (build tree 11 GB → 1.8 GB) and **v0.2.0
  tagged**; Cluster O closed end to end.

Older closeouts (2026-04-26 reset → 2026-08-21) and all full closeout paragraphs:
[`decisions-archive.md`](decisions-archive.md).

---

Build/test: `cmake --build --preset dev -j 10`. Tests: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 10`. Offscreen is mandatory. **The `-E "benchmark|perf"` exclusion is load-bearing** (changed 2026-10-01): it drops all four wall-clock timing tests. `tst_benchmark_layout` carries a native `benchmark` label, but the two `tst_canvas_perf_*` tests come from the markoff-family submodule with no label, and they flake red under `-j10` purely from machine load while passing standalone every time. Without the exclusion, "the suite is green" is not a reliable signal. The proper fix is a `LABELS "benchmark"` in markoff-canvas, filed. Work on `master` (no feature branches). **Never `git add -A`** — `testvaults/` is deliberately dirty; stage by path. Pre-pull.
