# Docs Reorganisation — Design

**Date:** 2026-04-19
**Author:** Claude (brainstorming session) + user
**Status:** Spec — pending plan via `superpowers:writing-plans`
**Supersedes:** n/a (one-shot reorganisation)
**Touches:** `docs/**`, `CLAUDE.md` (root-level file, not `docs/CLAUDE.md`)

---

## 1. Problem

`docs/` has grown unreadable. Concretely:

1. **`docs/PROJECT-STATE.md` is 155 KB in 226 lines.** The "Last updated" entry (line 5) and "Current focus" entry (line 11) are each 20–30 KB single paragraphs of `**Previously:** … **Previously:** … **Previously:** …` — every cluster closeout since the project started has been appended to the front and never pruned.
2. **Scattered follow-up lists buried inside PROJECT-STATE:** Cluster G / H / D / C follow-ups, plus Controller-side extractions, plus Known-flaky tests, plus Open questions. No single surface answers "what's left to do."
3. **Plans + specs for completed clusters sit next to active ones.** `docs/superpowers/plans/` has 47 files; most are for closed clusters (D, E, F, G, H, I, J, K, L, N, Q.0, Q) or pre-cluster sprints from 2026-03-30 → 2026-04-13.
4. **Root-level orphan docs** — superseded specifications (`CORBOMITE_SPECIFICATION.md`, `OBSIDIAN_SPECIFICATION.md`), one-off retrospectives, reference manuals (`HandbookGraphDrawing.txt`, `kde-power-software-design-guide/`) that post-date their usefulness.
5. **State drift.** `PROJECT-STATE.md` line 21-22 lists Clusters A and B as `Done`; `docs/superpowers/plans/INDEX.md` line 11-12 lists them as `Not started`. Resolved: **A and B are Done.**
6. **An archival mechanism was specified but never built.** CONTRIBUTING-OPS Ritual 2 says "Older Recent-decisions entries archive to `docs/decisions-archive.md` quarterly" — but the file does not exist and the rule was never followed. Nothing pointed Ritual 2 at a real destination.

## 2. Goals

- **Route + map.** A thin "where are we right now" cursor (PROJECT-STATE) sitting on top of a complete "everything left to do" backlog.
- **Self-maintaining.** Rituals that produce volume (Ritual 2, Ritual 3) must have named offload destinations so the state doc stays thin by construction, not by vigilance.
- **No broken inbound links.** Retros, plans, audit docs, and memory files all reference current paths. Use `git mv` and update INDEX + PROJECT-STATE, but avoid rewriting every retro.
- **Don't overreach.** The obsidian-audit, retros, plugin-development docs, research/, and build instructions are working fine; this reorg does not touch them.

## 3. Non-goals

- Rewriting cluster retros, audit docs, or plugin-development docs.
- Consolidating multiple plans into one or splitting large plans.
- Any change to `CLAUDE.md` files in memory (`~/.claude/projects/.../memory/`).
- Any schema change to PROJECT-STATE's section set — just pruning the contents of existing sections and moving overflow elsewhere.

## 4. Architecture

### 4.1 File inventory after reorg

```
docs/
├── CLAUDE.md                          [no — this is the project root CLAUDE.md; see §4.2]
├── PROJECT-STATE.md                   [REWRITTEN — thin route cursor, ≤200 lines HARD cap]
├── CONTRIBUTING-OPS.md                [UPDATED — Ritual 2/3 offload rules]
├── backlog.md                         [NEW — unified map, see §4.3]
├── decisions-archive.md               [NEW — append-only journal, see §4.4]
├── cluster-retros/                    [UNCHANGED — stays live forever]
├── obsidian-audit/                    [UNCHANGED]
├── plugin-development/                [UNCHANGED]
├── testing/                           [NEW folder]
│   ├── test-coverage-matrix.md        [MOVED from docs/]
│   └── test-coverage-bug-hunt.md      [MOVED from docs/]
├── superpowers/
│   ├── plans/
│   │   ├── INDEX.md                   [UPDATED — active-only rows, pointer to archive]
│   │   ├── <active plan files>       [A, B done — their plans archive. U, S, O, P, M remain; R done this week → archive]
│   │   └── archive/
│   │       └── README.md              [NEW — one-line “closed plans, do not modify”]
│   ├── specs/
│   │   ├── <active spec files>
│   │   └── archive/
│   │       └── README.md
│   └── research/                      [UNCHANGED]
└── archive/
    ├── README.md                      [NEW — one-line description]
    ├── CORBOMITE_SPECIFICATION.md     [superseded by obsidian-audit]
    ├── OBSIDIAN_SPECIFICATION.md      [superseded by obsidian-audit]
    ├── HandbookGraphDrawing.txt       [graph work done]
    ├── Obsidian_Sync_Overview.md      [informational, not referenced]
    ├── graph-performance-log.md       [graph cluster closed]
    ├── graph-view-blockers.md         [graph cluster closed]
    ├── 2026-04-15-program-state.md    [one-off retrospective]
    ├── 2026-04-15-session-0-intent-recheck.md [one-off retrospective]
    ├── search-dsl-spec.md             [superseded by D implementation + audit]
    └── kde-power-software-design-guide/ [project-inception reference]
```

### 4.2 Target shape of `PROJECT-STATE.md`

A ≤200-line hard cap. Sections (order preserved from current file so inbound "§Current focus" etc. references still resolve):

1. **Header** — 3 lines. Name, one-line purpose, `**Last updated:** YYYY-MM-DD — one-sentence summary.` The multi-kilobyte `**Previously:** …` cascade is **banned** (see §4.6 discipline rule).
2. **Current focus** — ≤20 lines. Today's active cluster/phase + next expected step. If idle, say so. No history.
3. **Roadmap** — one-row-per-cluster table. Status column is the authoritative status (INDEX mirrors this, not vice versa).
4. **In-flight work items** — ≤1 row per active cluster using the existing `Cluster X — <title>` block format already defined in CONTRIBUTING-OPS.
5. **Parallel long-term internal refactors** — short table (currently 2 rows).
6. **Recent decisions** — **last 20 entries only**, most-recent on top. Older entries move to `decisions-archive.md` when the 21st is added (enforced by Ritual 2 offload step).
7. **Known-flaky tests** — short list, unchanged.
8. **Open questions blocking progress** — unchanged.
9. **Pointers** — cross-links to the other docs (add `backlog.md`, `decisions-archive.md`).

Explicitly removed from PROJECT-STATE (offloaded to `backlog.md`):

- "Cluster G follow-ups" list (8 items).
- "Cluster H follow-ups" list (6 items).
- "Cluster D follow-ups" list (7 items).
- "Cluster C follow-ups" list (3 items).
- "Controller-side follow-ups (out-of-tree extractions)" list (6 items; one already done, keep as struck-through in archive).

### 4.3 `backlog.md` shape

The unified map. ≤500 lines, grouped **by theme, not by originating cluster**. Each entry:

```markdown
### <short title>
- **Source:** <cluster letter + retro/plan link>
- **Blocks:** <cluster plan, feature, or "nothing">
- **Scope:** <small / medium / large>
- **Details:** <one paragraph>
```

Sections (draft):

1. **Not-started and plan-needed clusters** — A/B are Done; the live ones are M (deferred), O (scouting, post-parity), P (scouting, parallel), S (plan-needed), T (deferred), U (scouting), plus the qutepart-corbomite-fork parallel refactor phases 3-8.
2. **Plugin API and extension surfaces** — absorbs Cluster H/N follow-ups that gate plugin 1.0 (menu injection ordering, HoverPopover rect anchoring, plugin-facing wrappers, multi-Notice stacking, SuggestPopup widget).
3. **Editor, Views, Workspace** — absorbs Cluster G/Qutepart follow-ups (empty-state view, unknown-viewType fallback, Workspace.openLinkText dispatcher, FileView.receiveSyncState, lastOpenFiles restore, WorkspaceWindow popout, View.onTabMenu default, ViewRegistry error paths).
4. **Search and metadata** — absorbs Cluster D follow-ups (`line:`/`block:`/`section:`/`task*:` operators, property-call, regex post-filter, match-case, KCommandBar FuzzyMatcher swap, Quick-Switcher prefix modes, SearchResults rich snippets).
5. **Plugin primitives + lifecycle** — absorbs Cluster C follow-ups (SessionDestroyer, hotkeys.json I/O, Modal Scope push/pop).
6. **Out-of-tree extractions** — the 5 remaining controller-side items (DOMPurify `SL`, Turndown `hP`, 25 plugin IDs, `AC`+`PC`, search-panel DSL). `DK/RK/JK/PX` already done.
7. **Cluster-K follow-ups** — 12 items from the K retro (cards/list layouts, plugin wrapping, rich widgets, rename-rewrite, embed-in-markdown, etc.).
8. **Cluster-N follow-ups** — distribution UX, sandbox decision, JS plugin shim, `CorbomiteConfigVersion.cmake`, etc.
9. **Cluster-Q follow-ups** — remaining: `tst_propertiespanel` mock-proxy rewrite.
10. **Stability** — the 4 known-flaky tests.

Items already done (e.g. `DK/RK/JK/PX` formula DSL addendum) appear **struck through** with a date + pointer — this preserves the "why did X take so long" trail.

### 4.4 `decisions-archive.md` shape

Append-only chronological journal. Two content sources:

1. **Cluster closeout summaries** — the long `**Previously:** …` paragraphs currently choking PROJECT-STATE, broken out one per closeout with a dated H2 header:
   ```markdown
   ## 2026-04-19 — Cluster R closed
   4 phases landed (~20 commits, …). <full paragraph from PROJECT-STATE>
   ```
2. **Recent decisions rolloff** — entries aged out of PROJECT-STATE's top-20 window.

Reverse-chronological (newest on top). No table of contents — `grep` is the navigation tool for a journal.

### 4.5 Archival mechanics

- **Move**, don't copy: `git mv` preserves file history.
- **Plans archived:** all 2026-03-30 → 2026-04-13 pre-cluster plans (~18 files) + plans for Done clusters (A, B, D, E, F, G×3, H, I, J, K, N, Q, Q.0, R). Kept active: U, S, O, P, M, T, qutepart-corbomite-fork.
- **Specs archived:** match the plan set.
- **INDEX.md update:** Done-cluster rows get their plan-file link rewritten to `archive/...`; status stays `Done`. Active-cluster rows unchanged.
- **`docs/archive/` top-level** holds everything in §4.1's `archive/` block.
- **Each archive directory gets a `README.md`** with a single sentence: "Closed plans. Do not modify. If a plan here is still referenced by a live plan, promote it back or excerpt into a new live doc." (variants per folder).

### 4.6 New discipline rule — "Do not regrow PROJECT-STATE"

Added to `CLAUDE.md` and enforced by `CONTRIBUTING-OPS.md` Ritual 2:

> When a cluster or phase closes, write **at most 3 sentences** in PROJECT-STATE's "Current focus" section (replacing the previous top entry), and the **full** closeout paragraph into `decisions-archive.md` under a new dated H2 header. The `**Previously:** …` cascade pattern is banned. If you find yourself writing a `Previously:` block in PROJECT-STATE, you are writing in the wrong file.

This is the single change that prevents the mess from reoccurring.

## 5. `CLAUDE.md` changes

Target file: `/home/clinton/dev/Corbomite/CLAUDE.md` (project-root — the one shown at session start).

### 5.1 Additions

- New pointers in "Long-term project state" block:
  - `docs/backlog.md` — unified map (deferred follow-ups + not-started clusters + open questions). Read before picking up *new* work.
  - `docs/decisions-archive.md` — append-only journal. Don't read at session start; consult when tracing *why* of prior decisions.
  - `archive/` subdirectories are frozen — don't follow links into them for live work.
- New discipline rule box (see §4.6).

### 5.2 Updates

- Session-start TL;DR becomes 6 steps (one net-add, one net-drop):
  1. ~~Read this CLAUDE.md.~~ *(drop — already loaded)*
  2. Read `docs/PROJECT-STATE.md` top-to-bottom. *(now feasible — ≤200 lines)*
  3. If picking up new work (not continuing a live cluster), skim `docs/backlog.md` for candidates. *(new)*
  4. Read the cluster plan(s) for the current focus.
  5. Read audit-doc sections cited in the plan.
  6. Glance at `git log --oneline -10`.
  7. State the situation back and wait for confirmation.
- Drop drift-prone counts from the INDEX sentence ("5 full plans + 3 stubs + 3 scouting doc"). Replace with unqualified pointer.
- No change to Building / Testing / Library structure / Code conventions sections.

## 6. `CONTRIBUTING-OPS.md` changes

### Ritual 2 (Cluster phase done)

Replace the existing "Recent decisions append" bullet with:

> **If a non-trivial decision was made** (technology choice, design call, deviation from the cluster plan), append one bullet to PROJECT-STATE's `Recent decisions`. **If that brings the list above 20 entries, immediately roll the oldest entry into `decisions-archive.md` under its existing date header (create the header if absent).** Never leave `Recent decisions` above 20 entries.

Add a new step:

> **If a cluster phase closed or the cluster itself closed**, write at most 3 sentences into PROJECT-STATE §Current focus (replacing the previous top entry) AND the full closeout paragraph into `decisions-archive.md` under a new dated H2 header. `**Previously:** …` cascades in PROJECT-STATE are banned.

### Ritual 3 (Cluster done)

Add a new step:

> **Archive the plan + spec files.** `git mv docs/superpowers/plans/<cluster-plan>.md docs/superpowers/plans/archive/` and the matching spec. Update `docs/superpowers/plans/INDEX.md` to point the cluster row's "Plan file" column at the new `archive/...` path. The retro file stays where it is.

### Backlog hygiene

Add short section at the end:

> **`docs/backlog.md`:** when closing a backlog item, **strike it through** (`~~text~~`) with a one-line closure note + date. Quarterly (or whenever the file exceeds 500 lines), move all struck-through items to `decisions-archive.md` under a `## Backlog roll-up YYYY-MM-DD` header.

## 7. Execution phasing (preview — full plan comes from `superpowers:writing-plans`)

Rough shape, not the final plan:

1. **Create skeleton destinations** — make `docs/archive/`, `docs/superpowers/plans/archive/`, `docs/superpowers/specs/archive/`, `docs/testing/`, and empty `backlog.md` + `decisions-archive.md` files with headers. Commit.
2. **Move the obvious archives** — root-level orphans + pre-cluster plans + closed-cluster plans. `git mv` only; no content edits. One commit per cohort.
3. **Populate `decisions-archive.md`** — extract the `**Previously:** …` cascades from PROJECT-STATE into dated entries. Verify every original paragraph is preserved verbatim before deleting from PROJECT-STATE.
4. **Populate `backlog.md`** — walk each of the 5 scattered follow-up lists + the "Known-flaky tests" + any buried A/B follow-ups, regroup by §4.3 theme.
5. **Rewrite PROJECT-STATE** — prune to §4.2 shape. Hard-cap review.
6. **Update INDEX.md** — repoint Done-cluster rows at archive paths.
7. **Update CLAUDE.md + CONTRIBUTING-OPS.md** per §5 and §6.
8. **Final link-sweep** — grep for references to every moved file; fix any stale links in cluster-retros + memory files. (Retro content itself stays untouched; only paths updated if necessary.)
9. **Final-read test** — start from the new CLAUDE.md, follow the new session-start ritual, confirm you land at a meaningful "current focus" in <5 minutes without opening any archived file.

## 8. Open risks

- **Inbound links from outside `docs/`.** The user's memory files at `~/.claude/projects/.../memory/` reference `docs/PROJECT-STATE.md`, `docs/CONTRIBUTING-OPS.md`, `docs/obsidian-audit/`, `docs/superpowers/plans/`. The *paths* of live files don't change; only closed-cluster plan links break. Cluster retros cross-link to plan files by path — those will need targeted updating if they link to a now-archived plan. Mitigation: phase 8 grep sweep.
- **PROJECT-STATE hard cap enforcement.** ≤200-line cap is aspirational until the first Ritual-2 execution under the new rules. If the first cluster closure after reorg re-grows PROJECT-STATE, the discipline rule is failing. Mitigation: CLAUDE.md rule + Ritual 2 offload step is bold-faced; reviewable by `wc -l docs/PROJECT-STATE.md` as a smoke test.
- **Buried A/B follow-ups.** A and B are Done but may have generated follow-ups that got lost in the wall-of-text. Execution step: `grep -i "cluster [ab]" docs/PROJECT-STATE.md` (before rewrite) to surface, lift into `backlog.md`.
- **`test-coverage-matrix.md` future.** Kept live per user direction. If no Cycle 2 is planned within a quarter, reconsider archive.

## 9. Definition of done

- `docs/PROJECT-STATE.md` is ≤200 lines, contains no `**Previously:** …` paragraph.
- `docs/backlog.md` exists and contains every follow-up previously scattered in PROJECT-STATE + every not-started/scouting/deferred cluster.
- `docs/decisions-archive.md` exists and contains the extracted closeout paragraphs (text-equivalent to the old PROJECT-STATE cascades).
- `docs/superpowers/plans/INDEX.md` has no 404s; Done-cluster rows point at `archive/` paths.
- `docs/archive/` + `plans/archive/` + `specs/archive/` each exist with a `README.md`.
- `docs/testing/` contains the two test-coverage files.
- `CLAUDE.md` references the new files and carries the "Do not regrow PROJECT-STATE" rule.
- `CONTRIBUTING-OPS.md` Rituals 2 and 3 codify the offload-on-close behaviour.
- Following the new session-start ritual from a fresh context lands a reader at a meaningful "current focus" in <5 minutes with no archived-file reads.
- The state drift (A/B Done vs Not started) is resolved in favour of Done, with any buried A/B follow-ups surfaced into `backlog.md`.
