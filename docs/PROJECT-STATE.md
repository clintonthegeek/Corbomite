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

Remediation sweep. **Suite 339/339.** Tracker: 60 open / 137 closed.

**Done since the second-sweep appraisal** (2026-10-01 → 02): `ThemeService`
revival; ~500 lines of dead code; 7 disabled tests re-enabled; cluster
bookkeeping (E retired, F split into F-a/F-b, Tags pane → Cluster Q, D.5
specced, L5 checkboxed); `eState.scroll` interop (was P1); `.obsidian/app.json`;
`resolveSubpath` wired in. **Cluster O closed** — bases + graph providers, tab
reorder, linked panes, the plugin ViewActions path, O6.T3's shared-panel fix, and
O7 (3 new verbs, blockers on all 14 stubs, placebo dialogs disabled, palette
regrouped). **Canvas dark mode** shipped. Canvas file picker lists the Vault.
**P1s**: the frontmatter closing-fence scan, and `indexFinished` firing on a
partly-built index.

**A build bug worth naming:** the test gate had three conventions, two of which
looked like ours but were not. `BUILD_TESTING` is declared by **ECM**, so 12
subdirectories were reading a third-party default; and no preset ever set
`CORBOMITE_PORT_BUILD_TESTS`, whose default is OFF. A clean `cmake --preset dev`
therefore produced **109 tests instead of 345** — all from the markoff-family
submodule, none of them Corbomite's — and the canonical test command reported
success against a tree containing none of our tests. Verified by reproduction,
fixed, and the residual submodule leak documented rather than patched.

Also deleted a **test-only island**: `MarkdownRenderer` + `RegexRenderEngine`
had zero production references (the runtime has always used `StyledRenderEngine`),
and existed only to satisfy two QSKIPped tests that asserted nothing. The two
canvas tests that referenced them were repointed at the production engine first.

**Upstream:** [`docs/UPSTREAM-ASKS.md`](docs/UPSTREAM-ASKS.md) — 32 prioritised
asks for Markoff and Graffodil (both are our own repos, so we can file them).
Three **P0s** verified by hand: collabtext's `assert()`-only guards are a heap
OOB *write* in release and a version vector that records a delete that never
happened; Markoff keys collab op payloads without the `BlockId`, so a multi-block
paste loses two-thirds of its content (reproduced); and the parser's
`parsedFrontmatter()` sets its "already parsed" flag five lines before writing
the value, so a concurrent reader gets empty frontmatter.

**Next:**
1. **LIVE EYEBALL** — the single run-through is
   [`docs/USER-TESTING.md`](docs/USER-TESTING.md), one maintained file for every
   pass. First pass done: canvas dark mode confirmed good; two of my own bugs
   found (§7 shortcuts had **no Ctrl** despite a commit message claiming Obsidian
   parity; there was **no Tab menu** despite a comment claiming one). Both fixed.
   **§10 (the settings dialog) is the open one** — diagnosed from source, not
   reproduced.
2. **Cluster M** — M5.5 `jump-to-group` (needs `zoomToRect`, which does not
   exist) + `convert-to-file` (needs 1 new undo command); M5.4 read-only lock
   (needs a per-file `SessionManager` tier-3 bucket + ~10 mutation gates);
   M5.1 zoom clamp (blocked on a read-only submodule; consumer-side fix).
   **M5.3 is blocked on a corpus contradiction** — the plan's stated precedent
   does not exist, and three audit sources disagree on whether leaf `eState`
   persists at all. Needs the user's call. M5.2 is dropped: Cluster O shipped it.
3. **Release hygiene B** — the unreproducible mmdr blob. **DEFERRED by user
   decision 2026-10-02.** **A decision, not more
   work**: the Rust source is not in this repo and not reachable from it, so a
   rebuild is *impossible*, not merely undocumented. Documented honestly in
   `libs/mmdr/README.md`. It blocks the Mermaid dark-theme item. Three options in
   the plan; B.3's history-rewrite decision is unrecorded anywhere.
4. **Dispatch** F-a (File Recovery + Note Composer, ~5–8d) and Q (Tags pane,
   ~3–5d). Both specced, neither started — these are multi-day and need a
   dedicated session.
5. **Smaller punch items** — 11 remaining `if(FALSE)` test gates, 3 classes
   named `VaultResourceProvider`, the embed single-render gap, `examples/`
   compile-unverified, Cluster O6.T1/T2/T4.

**Standing lesson** — thirteen of my own claims this session were wrong and were
caught by checking rather than shipped. The recurring shape is **asserting a
property in prose that was never checked against the code**: a parity claim about
a keyboard shortcut that had no `Ctrl`; a comment about a "Tab menu" that did not
exist; two "the file is inside an `#if 0`" claims that were only *mostly* true,
each hiding a live symbol outside the block. Also: a falsifiability check that
came back green against a deliberately reintroduced bug, an over-strong
invariant that failed on correct behaviour, and a batch of bulk-regex edits that
matched more than intended. See the audit's §10 — a gate never seen red proves
nothing.

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
