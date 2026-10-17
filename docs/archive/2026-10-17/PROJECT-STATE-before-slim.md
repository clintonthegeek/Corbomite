# Frozen PROJECT-STATE — 2026-10-17, before the slim

Journal, open questions, and the cluster table as they stood. Not the live status.
Live status is `docs/PROJECT-STATE.md`.

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

**Markoff vNext is in the working tree and not finished.** The local pin `dca64686`, the ephemeral `EditingSessionSource`, note save, and canvas paint identity are landed but uncommitted; the remaining steps are [`superpowers/plans/2026-10-17-markoff-vnext-finish.md`](superpowers/plans/2026-10-17-markoff-vnext-finish.md) and the punch list section `Added 2026-10-17`. The metadata-cache P0 and the paused L5 soak stay queued under that, and Cluster M6.2/M6.3 are unchanged.

## Two tracks

Work flows through **two parallel tracks**; both are checked at session start.

1. **Punch list** — flat, severity-ranked, small fixes: [`punch-list.md`](punch-list.md).
   **Start at its `Added 2026-10-17` section** (Markoff vNext finish), then `Added 2026-10-04` for the other ranked queue.
2. **Strategic clusters** — multi-phase coordinated work:
   [`superpowers/plans/INDEX.md`](superpowers/plans/INDEX.md).

Most P0/P1 punch-list items are **silent vault-format-corruption risks**. Drain them
before strategic-cluster work unless explicitly redirected.

**Tracker (2026-10-17): 91 open / 151 closed / 2 partial**, plus 2 do-not-regress
guardrails. Verify with `grep -c` on `punch-list.md`.

**Test baseline (2026-10-17): 267/267** offscreen with `-E "benchmark|perf"`, on Markoff `dca64686`. Markoff's own tests are excluded because both Markoff test options are forced OFF. `tst_metadatacache_worker_integration` passed in 0.22 s in that run and stays the filed P0 in `Added 2026-10-04`. The load-sensitive tests named in `CLAUDE.md` are still not regressions by themselves.

## Active strategic clusters

Seven active; INDEX carries the plan-file links and full status.

| Cluster | Status | Next |
|---|---|---|
| **L** Workspace/KDDW | L0–L4 closed. **L5 soak PAUSED** | **Run the soak** — six `workspace.json` changes landed 2026-10-04 and this is the gate |
| **M** Canvas authoring | M0–M5 closed; **M6.1 canvas references indexed 2026-10-09** | **M6.2** rename rewrite; then M6.3 missing-file placeholder |
| **O** Context-sensitive UI | O0–O7 closed | O6.T1/T2/T4 inspector slots, O7.T6 sidebar raise-on-type — both deferred |
| **D** Bases UI | Only D.5 — spec written, awaiting review | review, then build |
| **F-a** File Recovery + Composer | Plan written 2026-10-01, dispatchable, ~5–8d | dispatch |
| **F-b** Workspaces + sidedock | Plan written 2026-10-01, spec-first, ~6–9d | dispatch |
| **Q** Tags pane | Plan written 2026-10-01, dispatchable, ~3–5d | dispatch |

Closed (post-reset): A, B, C, **E** (retired 2026-10-01, never executed), **I** (by
absorption into O), **K**, **N**, **P**, **D**/**F** (stubs; F split into F-a/F-b).
**Obsoleted by the foundation rewrite (2026-05-25):** G, H, J. **Q is post-reset**
(Tags pane), not legacy Cluster Q (closed 2026-04-17) — qualify when ambiguous.

## Open questions

- **Eyeball-verification backlog** — nine UI work-packages shipped offscreen-only and have
  never been seen by a human: D.2 grouped rendering, D.3 panels/drawer, D.4a cell clicks,
  D.4b export/+New, D.4c undo + drift banner, formula-editor popups, filter-builder dialog,
  properties panel, and canvas M4.4's chrome overlay (handle/dot sizing, hover-vs-selection
  visibility). One consolidated session in front of the running app clears all nine.
  Tracked in [`USER-TESTING.md`](USER-TESTING.md); deliberately deferred by user decision.
- **Is `tst_metadatacache_worker_integration` a real defect?** Filed as the top P0 in the
  punch list's `Added 2026-10-04` section. Three 10 s waits all time out under `-j6`, which
  reads as a lost signal rather than slow execution. On 2026-10-09 it passed 48 concurrent
  process runs plus a six-worker CTest batch including `tst_realistic`; the original report
  remains un-reproduced and the all-suite run did not complete in this environment.
- **M5.2's on-canvas control cluster** — its own plan says it exists in Obsidian only because
  Obsidian has no menu bar, and Corbomite has both a canvas menu and a canvas toolbar. Decide
  slim-to-affordances-only or drop before building it.
- **`repeatable` hotkeys** unimplemented, and a code-only binding has no user-visible form.
  Both are Obsidian-parity residuals of the dispatcher, not blockers.

## Recent decisions

- **2026-10-04 — the scope stack stays flat; the command table gets a gated root slot.**
  Obsidian's "modal first, hotkey table last" ordering comes from a per-window scope stack plus
  `Scope.parent`, and buys a bug: `Modal` allocates a parentless `Scope`, so the root — the only
  place `HotkeyManager` registers its catch-all — is unreachable and **no** `hotkeys.json` binding
  fires while a modal is up. Corbomite keeps the flat walk (ordering without the bug) and states
  the table's position explicitly: `ScopeManager`'s root slot, always walked last, gated on
  `QApplication::activeModalWidget()`. A `code` binding is matched as a *name*, because Obsidian's
  `bake()` folds it into the `key` slot and compares that string; bindings Qt cannot name
  (`NumpadAdd`) are refused and reported, never coerced. Addendum:
  `2026-10-04-hotkeys-json-dispatcher.md`; closeout: `decisions-archive.md`.
- **2026-10-03 — Corbomite does not reproduce Obsidian's sanitiser URI-scheme bypass.**
  Obsidian runs DOMPurify with `ALLOW_UNKNOWN_PROTOCOLS: true`, which short-circuits the
  allowed-scheme regexp so `javascript:` hrefs survive, on the main render path. Our
  doctrine is fidelity on data and format; where fidelity would mean shipping a known XSS
  vector in an app that opens vaults it did not author, security wins and the divergence is
  recorded. `rendering.md` §11's absolute "must match exactly" is narrowed to the other four
  keys. Addendum: `2026-10-03-rendering-sanitiser-deliberate-divergence.md`.
- **2026-10-03 — the RE corpus stays untracked; the tooling is tracked.** Obsidian is
  `UNLICENSED` (Dynalist Inc.) and Corbomite is GPLv3, so no part of the extracted source,
  the readable tree, or the rename map may be committed. `RE/*` is ignored with
  `RE/tools/` re-included, because the tooling is what makes the corpus reproducible — and
  the previous corpus was lost precisely because it was untracked with no map. Do not move
  or delete `RE/tools/`.
- **2026-10-03 — an addendum refuting a frozen doc requires source proof.** The
  2026-10-01 split-direction addendum declared it refuted `workspace.md:79`, then restated
  line 79 verbatim in its own table; six independent source paths confirm the doc was right
  and the inversion was in our serializer. The 2026-10-01 file stays as historical record;
  the mis-attribution is documented. Its *code* finding still stands.
- **2026-10-02 — six per-library handoff documents.** `docs/UPSTREAM-ASKS.md` is the index;
  the sendable documents are in `docs/handoff/`. All three P0s in it were re-verified by hand
  before writing, because a data-loss claim should not be taken on trust.

- **2026-10-02 - Cluster O closed; canvas dark mode; frontmatter P1.**
  `BasesViewActions` + `GraphViewActions` (the latter proving a *plugin* can
  ship a provider via `ViewRegistrar`, no new API surface); tab reorder +
  linked panes; the shared `GraphControlsPanel` singleton fixed by removing the
  sharing rather than insulating it. O7: `insert_wiki_link` (never blocked on
  Markoff at all), `heading_increase`/`decrease` (needed the cached
  `EditorContext`), all 14 stubs carry a greppable blocker, the two
  exec-and-discard placebo dialogs are disabled, the palette drops disabled
  entries and gained Tab/Format groups. **Canvas dark mode**: 14-role
  `CanvasPalette`; the export brush follows, so a dark canvas exports dark -
  and the falsifiable claim is "cards stay visible on the background", not "is
  dark dark". **P1**: `MarkdownView`'s closing-fence scan matched `----` and
  `---not a fence`, splicing a bogus row into prose.
- **2026-10-01 - Tier 2 quick wins.** `eState.scroll` interop (was P1) and
  `.obsidian/app.json` (5 of 6 keys) fixed; the tested subpath resolver now
  serves the render path; Markoff handoff written.
- **2026-10-01 - Dispatched remediation.** 7 disabled tests re-enabled; ~500
  lines of dead code removed; cluster bookkeeping; a suspected data-loss bug
  **refuted** - the D2 UndoLog does survive an external reload, but stale
  targets make undo inert, so it is a UX wart rather than content loss, and the
  guard I had already written for it was reverted.
- **2026-10-01 - Second-sweep appraisal.** Code sound, trackers not: ~12
  test-only islands, `ThemeService` dead, release-hygiene orphaned at ~7%,
  obsidian-audit addenda silent for 2 months (a frozen doc still taught a
  split-direction convention that had already cost a P0). 17 PARITY-MATRIX
  rows corrected; 2 of 3 contested subagent claims rejected on verification.
- **2026-08-21 - Cluster P closed** (build tree 11 GB -> 1.8 GB) and **v0.2.0
  tagged**.

Older closeouts (2026-04-26 reset -> 2026-08-21) and all full closeout
paragraphs: [`decisions-archive.md`](decisions-archive.md).
