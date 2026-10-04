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

**Audit-the-audit, complete.** The prior audit's RE corpus was destroyed, so every symbol citation in `docs/obsidian-audit/` was unverifiable — and the docs turned out to contain errors that produce wrong Corbomite code, not just wrong prose. Obsidian 1.13.7 was re-extracted into a readable, indexed, partially-renamed corpus (`RE/extract-obsidian.sh`, reproducible from the `.deb`, corpus gitignored for licensing) and **all 16 domains re-read against it**: **45 addenda, ~14.7k lines, 25 punch-list items (nine P0)**. Suite **339/339**, no code changed. The nine P0s are `app.json`/`appearance.json` splitting on a 16-key allow-list (10 keys in the wrong file → silently reverted by Obsidian), an omitted `.obsidian/<corePluginId>.json` class, no `workspaces.json`, unknown canvas node types deleted on save, `.base` property-ID canonicalisation applied inconsistently, two frontmatter scanners that disagree on CRLF, `metadata` `changed` arg 2 being the file contents not a hash, `hotkeys.json` `"cmd": []` meaning *unbind*, and a 100 ms watcher ENOENT grace. **Next: drain the nine P0s** — see the punch list's `Added 2026-10-03` section.

**One decision needs no revisiting:** Corbomite will **not** match Obsidian's
`ALLOW_UNKNOWN_PROTOCOLS: true`, which disables DOMPurify's URI-scheme check so
`javascript:` hrefs survive on the main render path. `rendering.md` §11's "must match
this config exactly" is rejected for that one flag on security grounds; the other four
keys are matched. Fix any future scheme complaint with a narrow allowlist, never a
blanket re-enable — [`2026-10-03-rendering-sanitiser-deliberate-divergence.md`](obsidian-audit/addenda/2026-10-03-rendering-sanitiser-deliberate-divergence.md).

**Doctrine for the RE work.** The plugin infrastructure is the one place Corbomite
should be *explicitly different* — a C++ plugin is nothing like a web plugin, and the
plugin-domain addendum documents a capability surface rather than an API to clone. Every
other domain translates web paradigms to desktop UX (that is not divergence) while
staying faithful on **vault format, vault settings, rendering and data files**.

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
