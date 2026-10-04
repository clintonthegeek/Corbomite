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

**Audit-the-audit corrections are landing.** Suite **345/345** (from 339). Eight of the
nine audit P0s are closed with mutation-verified tests; 15 new items were filed from the
implementation pass. Seven agent claims were **refuted rather than coded** — including one
of our own prior addenda that blamed a frozen doc for a P0 that was really our code's bug,
and four numbers that were simply wrong. That refutation rate is the argument for
re-verifying agent claims before implementing them.

**Closed this pass:** the 16-key `app.json`/`appearance.json` partition plus `.obsidian/
<corePluginId>.json` and the settings value encodings; unknown canvas node/edge
preservation at byte level; `.base` property-id canonicalisation; `hotkeys.json`
`"cmd": []` unbinding; the watcher ENOENT grace; hidden-path and `checkPath` parity; the
quick-switcher fuzzy penalty rule and several search-DSL operators; Bases' `not:`/NOR and
a batch of DSL builtin corrections.

**Next:** the largest measured gap is **~182 of Obsidian's 190 core command ids are
unimplemented** — a mapping-table job, not a code fix. Then M5.4/M5.5.

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
