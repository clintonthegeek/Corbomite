# Docs Reorganisation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganise `docs/` so `PROJECT-STATE.md` is a thin ≤200-line route, a new `backlog.md` is the unified map, a new `decisions-archive.md` is the journal, closed plans/specs archive-in-place, and root-level orphans move to `docs/archive/`. Update `CLAUDE.md` + `CONTRIBUTING-OPS.md` with offload rules so the mess can't regrow.

**Architecture:** Extraction-not-rewriting. All historical prose is moved verbatim to `decisions-archive.md`; scattered follow-up lists are regrouped-by-theme into `backlog.md`; PROJECT-STATE and INDEX are pruned. `git mv` preserves history for every archived file. Rituals 2 and 3 are updated to name the offload destinations that the current rituals lack.

**Tech Stack:** Plain markdown. `git mv` for relocations. `grep`, `wc -l`, `ls` for verification. No code changes, no build required.

**Spec:** `docs/superpowers/specs/2026-04-19-docs-reorg-design.md` — authoritative source for §4 shapes, §5–6 file edits, and §9 definition of done.

---

## Phase 1 — Skeleton destinations

### Task 1: Create archive + testing directories with READMEs

**Files:**
- Create: `docs/archive/README.md`
- Create: `docs/superpowers/plans/archive/README.md`
- Create: `docs/superpowers/specs/archive/README.md`
- Create: `docs/testing/README.md`

- [ ] **Step 1: Create `docs/archive/README.md`**

```markdown
# docs/archive

Frozen historical documents. Do not modify. Sources:

- Superseded project specifications (before the `obsidian-audit/` pass).
- One-off retrospectives.
- Reference manuals from closed work.

If a file here is still referenced by a live plan, excerpt the relevant part into a new live doc rather than editing this one.
```

- [ ] **Step 2: Create `docs/superpowers/plans/archive/README.md`**

```markdown
# plans/archive

Closed cluster plans and pre-cluster sprint plans. Do not modify.

Live plans remain at `docs/superpowers/plans/` and are indexed in `INDEX.md`.
```

- [ ] **Step 3: Create `docs/superpowers/specs/archive/README.md`**

```markdown
# specs/archive

Specs for closed work. Do not modify.

Live specs remain at `docs/superpowers/specs/`.
```

- [ ] **Step 4: Create `docs/testing/README.md`**

```markdown
# docs/testing

Live test-coverage tracking. The matrix enumerates component seams × lifecycle scenarios; the bug-hunt log records bugs surfaced during enrichment cycles.
```

- [ ] **Step 5: Verify the four directories exist and commit**

Run: `ls -d docs/archive docs/superpowers/plans/archive docs/superpowers/specs/archive docs/testing`
Expected: all four paths print without error.

```bash
git add docs/archive/README.md docs/superpowers/plans/archive/README.md docs/superpowers/specs/archive/README.md docs/testing/README.md
git commit -m "docs: create archive + testing skeleton dirs"
```

### Task 2: Seed empty `backlog.md` and `decisions-archive.md`

**Files:**
- Create: `docs/backlog.md`
- Create: `docs/decisions-archive.md`

- [ ] **Step 1: Write `docs/backlog.md` skeleton**

```markdown
# Backlog — Unified Map

> **Living document.** Every deferred follow-up, every not-yet-started cluster, every scouting doc, every known-flaky test. Grouped by theme, not by originating cluster. Strike items (`~~text~~`) when closed with a one-line closure + date; quarterly roll strike-throughs into `decisions-archive.md`.

## Reading order

Clusters to-do → Plugin API / extension surfaces → Editor / Views / Workspace → Search / metadata → Plugin primitives + lifecycle → Out-of-tree extractions → Cluster-specific follow-up buckets → Stability.

---

## 1. Not-started, plan-needed, and scouting clusters

*(populated in Task 8)*

## 2. Plugin API and extension surfaces

*(populated in Task 8)*

## 3. Editor, Views, Workspace

*(populated in Task 8)*

## 4. Search and metadata

*(populated in Task 8)*

## 5. Plugin primitives and lifecycle

*(populated in Task 8)*

## 6. Out-of-tree extractions (controller-side)

*(populated in Task 8)*

## 7. Cluster-K follow-ups

*(populated in Task 8)*

## 8. Cluster-N follow-ups

*(populated in Task 8)*

## 9. Cluster-Q follow-ups

*(populated in Task 8)*

## 10. Stability (known-flaky tests)

*(populated in Task 8)*
```

- [ ] **Step 2: Write `docs/decisions-archive.md` skeleton**

```markdown
# Decisions Archive

> **Append-only journal.** Closeout summaries for landed clusters + decisions rolled off `PROJECT-STATE.md`'s top-20 `Recent decisions` window. Reverse-chronological. Not read at session start; consulted when tracing *why* a prior decision was made.

Conventions:
- One H2 per event, dated `## YYYY-MM-DD — <one-line subject>`.
- Body is the original prose verbatim — do not rewrite.
- Newest entry on top.
- Struck-through backlog items roll up under a `## Backlog roll-up YYYY-MM-DD` header.

---

*(populated in Task 7)*
```

- [ ] **Step 3: Verify files exist and commit**

Run: `ls docs/backlog.md docs/decisions-archive.md`
Expected: both paths print.

```bash
git add docs/backlog.md docs/decisions-archive.md
git commit -m "docs: seed backlog.md and decisions-archive.md skeletons"
```

---

## Phase 2 — Move obvious archives (git mv only)

### Task 3: Archive root-level orphan docs

**Files:**
- Move: 9 files + 1 folder from `docs/` to `docs/archive/`
- Move: 2 files from `docs/` to `docs/testing/`

- [ ] **Step 1: Move root-level one-offs to `docs/archive/`**

```bash
cd /home/clinton/dev/Corbomite
git mv docs/CORBOMITE_SPECIFICATION.md docs/archive/
git mv docs/OBSIDIAN_SPECIFICATION.md docs/archive/
git mv docs/HandbookGraphDrawing.txt docs/archive/
git mv docs/Obsidian_Sync_Overview.md docs/archive/
git mv docs/graph-performance-log.md docs/archive/
git mv docs/graph-view-blockers.md docs/archive/
git mv docs/2026-04-15-program-state.md docs/archive/
git mv docs/2026-04-15-session-0-intent-recheck.md docs/archive/
git mv docs/search-dsl-spec.md docs/archive/
git mv docs/kde-power-software-design-guide docs/archive/
```

- [ ] **Step 2: Move test-coverage docs to `docs/testing/`**

```bash
git mv docs/test-coverage-matrix.md docs/testing/
git mv docs/test-coverage-bug-hunt.md docs/testing/
```

- [ ] **Step 3: Verify `docs/` top-level is reduced to the live set**

Run: `ls docs/ | grep -v '^\(archive\|backlog.md\|cluster-retros\|CONTRIBUTING-OPS.md\|decisions-archive.md\|obsidian-audit\|plugin-development\|PROJECT-STATE.md\|superpowers\|testing\)$'`
Expected: empty output (every remaining entry is in the allow-list).

- [ ] **Step 4: Commit**

```bash
git commit -m "docs: archive root-level orphans + relocate test-coverage to testing/"
```

### Task 4: Archive pre-cluster plans (2026-03-30 → 2026-04-13) and their specs

**Files:**
- Move: 25 plan files from `docs/superpowers/plans/` to `docs/superpowers/plans/archive/`
- Move: matching specs from `docs/superpowers/specs/` to `docs/superpowers/specs/archive/`

- [ ] **Step 1: Move pre-cluster plan files to archive**

```bash
cd /home/clinton/dev/Corbomite
for f in docs/superpowers/plans/2026-03-*.md \
         docs/superpowers/plans/2026-04-01-*.md \
         docs/superpowers/plans/2026-04-02-*.md \
         docs/superpowers/plans/2026-04-03-*.md \
         docs/superpowers/plans/2026-04-13-*.md; do
  [ -f "$f" ] && git mv "$f" docs/superpowers/plans/archive/
done
```

- [ ] **Step 2: Move pre-cluster spec files (same date cohort) to archive**

```bash
for f in docs/superpowers/specs/2026-03-*.md \
         docs/superpowers/specs/2026-04-01-*.md \
         docs/superpowers/specs/2026-04-02-*.md \
         docs/superpowers/specs/2026-04-03-*.md \
         docs/superpowers/specs/2026-04-13-*.md; do
  [ -f "$f" ] && git mv "$f" docs/superpowers/specs/archive/
done
```

- [ ] **Step 3: Verify plan archive cohort landed**

Run: `ls docs/superpowers/plans/archive/ | wc -l`
Expected: ≥18 (18 pre-cluster plans + README).

Run: `ls docs/superpowers/plans/ | grep '^2026-03'`
Expected: empty output.

- [ ] **Step 4: Commit**

```bash
git commit -m "docs: archive pre-cluster plans + specs (2026-03-30 → 2026-04-13)"
```

### Task 5: Archive Done-cluster plans + specs

Done clusters per spec §4.5: **A, B, D, E, F, G (×3 parts), H, I, J, K, N, Q, Q.0, R.** Kept active: **M, O, P, S, T, U, qutepart-corbomite-fork.**

**Files:**
- Move: ~18 plan files from `docs/superpowers/plans/` to `docs/superpowers/plans/archive/`
- Move: matching specs from `docs/superpowers/specs/` to `docs/superpowers/specs/archive/`

- [ ] **Step 1: Move Done-cluster plan files**

```bash
cd /home/clinton/dev/Corbomite
git mv docs/superpowers/plans/2026-04-14-cluster-a-link-frontmatter-correctness.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-14-cluster-b-vault-io.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-14-cluster-c-lifecycle-plugin-primitives.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-14-cluster-d-search-suggester-parity.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-14-cluster-e-markoff-three-mode-pivot.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-14-cluster-g-views-hierarchy-SCOUTING.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-14-cluster-h-menus-hover-suggester-ui.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-15-cluster-f-templates-daily-notes-moment.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-15-cluster-g-views-hierarchy.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-15-cluster-g-part2-workspace.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-15-cluster-i-metadatacache-parity.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-15-cluster-j-embed-rendering.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-16-cluster-q0-vault-architecture.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-16-cluster-q-internal-plugin-wrapping.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-17-cluster-k-bases.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-17-cluster-n-plugin-ready-surfaces.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-18-cluster-g-part3-split-semantics.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-19-cluster-r-view-header-menus.md docs/superpowers/plans/archive/
```

Also the cluster-C "spec-driven test audit" + test-enrichment plan:
```bash
git mv docs/superpowers/plans/2026-04-16-spec-driven-test-audit.md docs/superpowers/plans/archive/
git mv docs/superpowers/plans/2026-04-15-test-enrichment-cycle.md docs/superpowers/plans/archive/
```

- [ ] **Step 2: Move matching Done-cluster specs**

Cluster S's Bookmarks spec (`2026-04-19-cluster-s-bookmarks-design.md`) stays live because S is Plan-needed. Everything else in the Done set archives:

```bash
git mv docs/superpowers/specs/2026-04-14-rapidyaml-port-design.md docs/superpowers/specs/archive/
git mv docs/superpowers/specs/2026-04-14-markoff-editor-polish-design.md docs/superpowers/specs/archive/
git mv docs/superpowers/specs/2026-04-15-cluster-g-part2-workspace-design.md docs/superpowers/specs/archive/
git mv docs/superpowers/specs/2026-04-15-cluster-g-views-hierarchy-design.md docs/superpowers/specs/archive/
git mv docs/superpowers/specs/2026-04-15-cluster-j-embed-rendering-design.md docs/superpowers/specs/archive/
git mv docs/superpowers/specs/2026-04-15-completion-popup-rewrite.md docs/superpowers/specs/archive/
git mv docs/superpowers/specs/2026-04-16-cluster-q-internal-plugin-wrapping-design.md docs/superpowers/specs/archive/
git mv docs/superpowers/specs/2026-04-16-vault-architecture-design.md docs/superpowers/specs/archive/
git mv docs/superpowers/specs/2026-04-17-cluster-n-plugin-ready-surfaces-design.md docs/superpowers/specs/archive/
git mv docs/superpowers/specs/2026-04-19-cluster-r-view-header-menus-design.md docs/superpowers/specs/archive/
```

- [ ] **Step 3: Verify the live plan set is the expected 7 items**

Run: `ls docs/superpowers/plans/ | grep -v '^archive\|^INDEX.md$'`
Expected: exactly these 7 files:
```
2026-04-14-cluster-k-bases-SCOUTING.md
2026-04-14-cluster-o-query-layer-SCOUTING.md
2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md
2026-04-15-qutepart-corbomite-fork.md
2026-04-19-cluster-u-file-explorer-enhancements-SCOUTING.md
```

Wait — Cluster K is **Done** (the SCOUTING predates the full plan which is already archived in Step 1). The scouting doc is historical; move it too:

```bash
git mv docs/superpowers/plans/2026-04-14-cluster-k-bases-SCOUTING.md docs/superpowers/plans/archive/
```

Now re-run the verify grep. Expected live set is exactly:
```
2026-04-14-cluster-o-query-layer-SCOUTING.md
2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md
2026-04-15-qutepart-corbomite-fork.md
2026-04-19-cluster-u-file-explorer-enhancements-SCOUTING.md
```

That's 4 active plan files. M, S, T have no plan files (they are "Plan-needed" or "Deferred").

- [ ] **Step 4: Commit**

```bash
git commit -m "docs: archive Done-cluster plans + specs (A,B,C,D,E,F,G,H,I,J,K,N,Q,Q.0,R)"
```

---

## Phase 3 — Populate `decisions-archive.md`

### Task 6: Extract `**Previously:** …` cascade from PROJECT-STATE into journal entries

The current `PROJECT-STATE.md` line 5 (`**Last updated:** …`) and line 11 (`## Current focus` body) are each 20-30 KB single paragraphs made of `**Previously:** <event-paragraph>` blocks in reverse chronological order. Each `Previously:` block corresponds to one closeout event that deserves its own dated entry in `decisions-archive.md`.

**Files:**
- Read: `docs/PROJECT-STATE.md` (lines 5 and 11)
- Modify: `docs/decisions-archive.md`

- [ ] **Step 1: Identify every `**Previously:**` boundary in the two walls**

Run: `grep -oc '\*\*Previously:\*\*' docs/PROJECT-STATE.md`
Expected: a count ≥ 20 (approximate — each cascade has many).

- [ ] **Step 2: Extract each `Previously:` block as a dated H2 entry**

Method (executor does this manually or with a one-shot script):
1. For **line 5**, split on `**Previously:**` — the first chunk is the actual "Last updated" for 2026-04-19 (stays in PROJECT-STATE after rewrite). Every subsequent chunk is one past "Last updated" paragraph.
2. For **line 11**, same split — first chunk is current focus for 2026-04-19; subsequent chunks are past "Current focus" entries.
3. For each extracted chunk, infer the date from the first recognisable date/commit reference in that chunk (the text itself cites dates like "landed 2026-04-17", "closed 2026-04-15", etc.).
4. Append to `docs/decisions-archive.md` **in reverse chronological order** (newest on top under the header) with structure:

   ```markdown
   ## YYYY-MM-DD — <one-line subject extracted from first sentence>

   <the full paragraph verbatim — do NOT rewrite — just strip leading `**Previously:** `>
   ```

5. Tag each extracted entry with a source suffix at the bottom of the block: `*(from PROJECT-STATE §Last updated)*` or `*(from PROJECT-STATE §Current focus)*` so future readers can tell why they were promoted.

Dates expected (sampled, verify against actual content; ≥20 entries total):
`2026-04-19`, `2026-04-18`, `2026-04-17`, `2026-04-16`, `2026-04-15`, `2026-04-14`, `2026-04-13`, `2026-04-03`, `2026-04-02`, `2026-04-01`, `2026-03-31`, `2026-03-30`. Some dates have multiple events — disambiguate subjects (e.g. `2026-04-15 — Cluster I closed` and `2026-04-15 — Cluster E Phase 0 landed`).

- [ ] **Step 3: Verify byte-preservation**

After population, measure: the sum of the extracted-block byte counts in `decisions-archive.md` should be within 5% of the total `**Previously:** …` cascade bytes in `PROJECT-STATE.md` lines 5 + 11 (allowing for header lines and the dropped `**Previously:**` prefix).

Run: `wc -c docs/decisions-archive.md`
Expected: ≥ 40000 (back-of-envelope — most of the 155 KB of PROJECT-STATE is these cascades).

- [ ] **Step 4: Commit**

```bash
git add docs/decisions-archive.md
git commit -m "docs: extract PROJECT-STATE cascade into decisions-archive.md"
```

**Do NOT delete the cascades from PROJECT-STATE in this task.** That happens in Task 9 (PROJECT-STATE rewrite) after backlog population confirms nothing else needs extracting.

### Task 7: Roll older `Recent decisions` into archive

PROJECT-STATE §Recent decisions (currently lines ~116-173, ~30+ bullets) needs pruning to last 20. Older bullets move to `decisions-archive.md`.

**Files:**
- Read: `docs/PROJECT-STATE.md` §Recent decisions
- Modify: `docs/decisions-archive.md`

- [ ] **Step 1: Read all bullets under `## Recent decisions`**

Run: `sed -n '/^## Recent decisions/,/^## /p' docs/PROJECT-STATE.md | head -n -1 | grep -c '^- '`
Expected: the current bullet count (likely 30+).

- [ ] **Step 2: Count and split**

- Keep the newest 20 bullets in PROJECT-STATE (done in Task 9).
- For every bullet beyond the newest 20, append to `decisions-archive.md` under a new header:
  ```markdown
  ## Recent-decisions roll-off (pre-<cutoff-date>)

  <bullets in reverse-chronological order>
  ```
  where `<cutoff-date>` is the date on the 21st-newest bullet.

- [ ] **Step 3: Commit**

```bash
git add docs/decisions-archive.md
git commit -m "docs: roll older Recent-decisions bullets into archive"
```

---

## Phase 4 — Populate `backlog.md`

### Task 8: Regroup scattered follow-ups into thematic sections

Source sections in `PROJECT-STATE.md`:
- `## Cluster G follow-ups` (8 items)
- `## Cluster H follow-ups` (6 items)
- `## Cluster D follow-ups` (7 items)
- `## Cluster C follow-ups` (3 items)
- `## Controller-side follow-ups (out-of-tree extractions)` (6 items; #3 already done — preserve as struck-through)
- `## Known-flaky tests` (4 items)

Additional sources:
- `docs/cluster-retros/cluster-k.md` — 12 deferred follow-ups.
- `docs/cluster-retros/cluster-n.md` — "Explicitly deferred (follow-ups)" block.
- `docs/cluster-retros/cluster-q.md` — remaining open follow-up(s).
- `docs/superpowers/plans/INDEX.md` — rows for M, O, P, S, T, U + qutepart-corbomite-fork.

**Files:**
- Read: the above sources
- Modify: `docs/backlog.md` (replace `*(populated in Task 8)*` stubs under each section)

- [ ] **Step 1: Populate §1 Not-started, plan-needed, and scouting clusters**

For each active/deferred cluster (M, O, P, S, T, U + qutepart-corbomite-fork phases 3-8), write an entry using the spec §4.3 schema:

```markdown
### Cluster M — Internal-plugin feature audits (Graph, Canvas)
- **Source:** INDEX.md row M
- **Blocks:** nothing
- **Scope:** medium (two normal tasks)
- **Details:** Deferred. Targeted feature audits of the Graph and Canvas internal plugins to catch drift between implementation and Obsidian behaviour. No plan file exists; expand to a full plan when scheduled.
```

Repeat for O, P, S, T, U. For qutepart-corbomite-fork, one entry describing Phases 3–8 (Phase 3 = public find/replace API; the rest are asynchronous shaping work — reference the plan file).

- [ ] **Step 2: Populate §2 Plugin API and extension surfaces**

Move Cluster H follow-ups #2-6 (mechanical menu migrations, HoverPopover rect anchor, SuggestPopup widget, multi-Notice stacking, plugin-facing wrappers). H follow-up #1 landed 2026-04-15 — skip or include struck-through. Add Cluster-N Explicitly-deferred items: in-app plugin browse/install UI, sandbox/process isolation decision, JS plugin shim, Cluster C Phase 4b-d (SessionDestroyer, hotkeys.json I/O, Modal Scope push/pop), SessionDestroyer hook, partial H #6 hover/suggest wrappers, `ui.views` semantics for createView-only plugins, `CorbomiteConfigVersion.cmake`, distro packaging validation, `tst_propertiespanel` mock-proxy rewrite.

For each item, follow the §4.3 schema verbatim (copy title + Source link + Blocks/Scope/Details paragraph).

- [ ] **Step 3: Populate §3 Editor, Views, Workspace**

Move Cluster G follow-ups (all 8 items) plus Cluster E Phases 4-8 carryovers from the qutepart fork where they concern editor UX (only if not duplicating §1's qutepart entry — Phase 3 is find/replace; keep that under §1 not here).

- [ ] **Step 4: Populate §4 Search and metadata**

Move Cluster D follow-ups (all 7 items).

- [ ] **Step 5: Populate §5 Plugin primitives and lifecycle**

Move Cluster C follow-ups (all 3 items). If any overlap with §2 (plugin API), mark one canonical location and cross-link from the other with `→ see §2`.

- [ ] **Step 6: Populate §6 Out-of-tree extractions**

Move Controller-side follow-ups (6 items). Item #3 (`DK/RK/JK/PX`) is **done 2026-04-17** — preserve as `~~strikethrough~~` with closure note pointing at the addendum.

- [ ] **Step 7: Populate §7 Cluster-K follow-ups**

Read `docs/cluster-retros/cluster-k.md` "deferred follow-ups" section (12 items — cards/list layouts, plugin wrapping, rich widgets, view-rename wikilink rewrite, embed-in-markdown, clipboard export, formula editor, NewItemMenu, per-view undo, column-reorder persist, multi-key sort UI, group-header render). One entry each.

- [ ] **Step 8: Populate §8 Cluster-N follow-ups**

Read `docs/cluster-retros/cluster-n.md` — mostly consumed by §2 in Step 2. Anything left (e.g. in-app plugin browser, specific build-system items unique to N) lives here.

- [ ] **Step 9: Populate §9 Cluster-Q follow-ups**

Only `tst_propertiespanel` mock-proxy rewrite remains (per memory `project_cluster_q_followups`). One entry.

- [ ] **Step 10: Populate §10 Stability**

The 4 known-flaky tests from PROJECT-STATE §Known-flaky tests:
- `tst_markoff_inline_math`
- `tst_renderengine`
- `tst_completion_popup`
- `tst_benchmark_layout`

Each gets one entry with the existing one-line description as `Details`.

- [ ] **Step 11: Surface any buried A/B follow-ups**

Run: `grep -in -A1 "cluster [ab]" docs/PROJECT-STATE.md | head -n 100`
Scan results for any follow-up language ("deferred", "not yet", "TODO", "follow-up") pointing at Cluster A or B work. If found, add an entry under §1 or §4 as appropriate. If nothing surfaces, add a one-line note at the bottom of `backlog.md`:

```markdown
---

*(A and B follow-up sweep 2026-04-19: grep found none beyond what is already captured above.)*
```

- [ ] **Step 12: Verify line budget and commit**

Run: `wc -l docs/backlog.md`
Expected: 200–500 lines.

```bash
git add docs/backlog.md
git commit -m "docs: populate backlog.md from scattered follow-up lists"
```

---

## Phase 5 — Rewrite `PROJECT-STATE.md`

### Task 9: Prune PROJECT-STATE to the §4.2 shape

This is the high-value task. After this, the mess is gone.

**Files:**
- Modify: `docs/PROJECT-STATE.md` (full rewrite of lines 5 + 11 + deletion of follow-up sections)

- [ ] **Step 1: Rewrite the `**Last updated:**` line (line 5)**

Replace the entire wall-of-text paragraph with a single line of the form:

```markdown
**Last updated:** 2026-04-19 — Docs reorganisation landed: scattered follow-ups consolidated into `backlog.md`, historical closeout prose moved to `decisions-archive.md`, closed plans + specs archived in-place. See `decisions-archive.md` for the full session log.
```

**No `**Previously:** …` cascade.** If you are writing one, stop.

- [ ] **Step 2: Rewrite `## Current focus` (line 9 onward, one blank line after the H2 opener)**

Replace the entire wall-of-text paragraph with a ≤20-line block describing only today's active work. Suggested shape:

```markdown
## Current focus

**Docs reorganisation in progress (2026-04-19).** Spec at `superpowers/specs/2026-04-19-docs-reorg-design.md`; plan at `superpowers/plans/2026-04-19-docs-reorg.md`. Thin `PROJECT-STATE` + `backlog.md` + `decisions-archive.md` substrate going live; `archive/` dirs created; closed plans + specs archived; root orphans moved to `docs/archive/`; test-coverage docs relocated to `docs/testing/`.

**Next focus after reorg lands:** user-selected from `backlog.md` §1 candidates — Cluster S Bookmarks plan writing; Cluster U File Explorer plan expansion; `tst_propertiespanel` mock-proxy rewrite; or one of the Cluster-K/N follow-ups.
```

- [ ] **Step 3: Delete the follow-up sections now in backlog.md**

Delete these H2 sections and their bodies entirely:
- `## Cluster G follow-ups (deferred — ...)`
- `## Cluster H follow-ups (deferred — consumer- or scope-gated)`
- `## Cluster D follow-ups (deferred — consumer- or schema-gated)`
- `## Cluster C follow-ups (deferred until a consumer needs them)`
- `## Controller-side follow-ups (out-of-tree extractions)`
- `## Known-flaky tests (pre-existing, not introduced by recent clusters)`

- [ ] **Step 4: Prune `## Recent decisions` to last 20 bullets**

(Entries beyond 20 were moved to `decisions-archive.md` in Task 7.) Keep only the top 20 under this header.

- [ ] **Step 5: Update `## Pointers` section to include new docs**

Add two bullets:

```markdown
- **Unified backlog (map):** `docs/backlog.md` — every deferred follow-up, not-started cluster, and known-flaky test. Read before picking up new work.
- **Decisions archive (journal):** `docs/decisions-archive.md` — append-only closeout summaries + rolled-off decisions. Consult for *why* a prior call was made.
```

- [ ] **Step 6: Verify ≤200-line hard cap**

Run: `wc -l docs/PROJECT-STATE.md`
Expected: ≤200.

If the line count exceeds 200, something was not moved. Re-read spec §4.2 to identify the overflow.

- [ ] **Step 7: Verify no `**Previously:**` pattern remains**

Run: `grep -c '\*\*Previously:\*\*' docs/PROJECT-STATE.md`
Expected: `0`.

- [ ] **Step 8: Commit**

```bash
git add docs/PROJECT-STATE.md
git commit -m "docs: rewrite PROJECT-STATE as thin route (≤200 lines, no Previously cascade)"
```

---

## Phase 6 — Update `INDEX.md`

### Task 10: Repoint Done-cluster rows at `archive/` paths

**Files:**
- Modify: `docs/superpowers/plans/INDEX.md`

- [ ] **Step 1: Rewrite the `**Last updated:**` paragraph (line 3)**

Replace the massive wall-of-text with:

```markdown
**Last updated:** 2026-04-19 — Docs reorganisation: Done-cluster plan links repointed at `archive/` paths; closed clusters A, B, C, D, E, F, G, H, I, J, K, N, Q, Q.0, R archived. Active: M (deferred), O + P (scouting), S (plan-needed), T (deferred), U (scouting). Parallel refactor: qutepart-corbomite-fork (phases 3–8 remaining).
```

- [ ] **Step 2: Update the Plans table — for every Done cluster, prefix plan-file paths with `archive/`**

For each Done-cluster row (A, B, C, D, E, F, G×3, H, I, J, K, N, Q, Q.0, R) in the table, change its `Plan file` column link target from `2026-MM-DD-cluster-X-....md` to `archive/2026-MM-DD-cluster-X-....md`. L already has `—` and stays `—`. M stays `—`. S stays `— (plan pending)`. T stays `—`.

For K, additionally repoint the `DSL addendum` link that currently points at `../../obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md` — that file was **not** archived, so leave it unchanged.

- [ ] **Step 3: Verify no 404s in INDEX**

Run:
```bash
cd /home/clinton/dev/Corbomite
grep -oE '\]\([^)]+\.md\)' docs/superpowers/plans/INDEX.md | \
  sed 's/](//; s/)$//' | \
  while read p; do
    abs="docs/superpowers/plans/$p"
    [ -f "$abs" ] || echo "MISSING: $p"
  done
```
Expected: no "MISSING:" lines.

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/plans/INDEX.md
git commit -m "docs: INDEX.md points Done-cluster rows at archive/ paths"
```

---

## Phase 7 — Update `CLAUDE.md` + `CONTRIBUTING-OPS.md`

### Task 11: Add new pointers + discipline rule to `CLAUDE.md`

**Files:**
- Modify: `/home/clinton/dev/Corbomite/CLAUDE.md`

- [ ] **Step 1: Add pointers to `backlog.md` + `decisions-archive.md` in the "Long-term project state" block**

After the existing `docs/superpowers/plans/INDEX.md` pointer line, insert:

```markdown
**Unified backlog (map):** [`docs/backlog.md`](docs/backlog.md). Every deferred follow-up, not-started cluster, and known-flaky test, grouped by theme. Read before picking up new work.

**Decisions archive (journal):** [`docs/decisions-archive.md`](docs/decisions-archive.md). Append-only closeout summaries + rolled-off decisions. Consult for *why* a prior call was made — not at session start.

**Archive directories are frozen.** `docs/archive/`, `docs/superpowers/plans/archive/`, and `docs/superpowers/specs/archive/` contain closed work. Don't follow links into them for live tasks.
```

- [ ] **Step 2: Add the discipline rule**

At the end of the "Long-term project state" block (before the `---` divider that precedes "Building"), insert:

```markdown
**Do not regrow `PROJECT-STATE.md`.** When a cluster or phase closes, write **at most 3 sentences** in `PROJECT-STATE.md` §Current focus (replacing the previous top entry) and the **full** closeout paragraph into `docs/decisions-archive.md` under a new dated H2 header. The `**Previously:** …` cascade pattern is banned. If you find yourself writing a `Previously:` paragraph in `PROJECT-STATE.md`, you are writing in the wrong file.
```

- [ ] **Step 3: Rewrite the session-start TL;DR**

Replace the current 6-step TL;DR with:

```markdown
**Session-start ritual (TL;DR — full version in `CONTRIBUTING-OPS.md`):**
1. Read `docs/PROJECT-STATE.md` top-to-bottom.
2. If picking up new work (not continuing a live cluster), skim `docs/backlog.md` for candidates.
3. Read the cluster plan(s) for the current focus.
4. Read audit-doc sections cited in the plan.
5. Glance at `git log --oneline -10`.
6. State the situation back: "Per PROJECT-STATE, current focus is X, last touched Y; next step is Z. Confirm or redirect?" — wait for confirmation before working.
```

(Drops the old step 1 "Read this CLAUDE.md" — already loaded; drops the drift-prone "5 full plans + 3 stubs + 3 scouting docs" count by just pointing at INDEX without quantifying.)

- [ ] **Step 4: Drop the drift-prone count from the INDEX sentence**

In the paragraph that currently reads "... at `docs/superpowers/plans/INDEX.md`. 5 full plans + 3 stubs + 3 scouting docs + the qutepart-corbomite-fork plan as of 2026-04-15." — replace with:

```markdown
**Cluster plans (one per work cluster) + parallel long-term internal refactors:** [`docs/superpowers/plans/INDEX.md`](docs/superpowers/plans/INDEX.md). INDEX is the table of contents over active plans; closed-cluster plans live under `plans/archive/` and are linked from INDEX.
```

- [ ] **Step 5: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: update CLAUDE.md with backlog/archive pointers and PROJECT-STATE discipline rule"
```

### Task 12: Update `CONTRIBUTING-OPS.md` Rituals 2 and 3

**Files:**
- Modify: `docs/CONTRIBUTING-OPS.md`

- [ ] **Step 1: Update Ritual 2 step 3 (Recent decisions append)**

Find the existing bullet `If a non-trivial decision was made ... append one bullet to Recent decisions. Format: …` and replace with:

```markdown
3. **If a non-trivial decision was made** (technology choice, design call, deviation from the cluster plan), append one bullet to `PROJECT-STATE.md` `Recent decisions`. Format: `- **YYYY-MM-DD — <decision>.** Reason: <one sentence>.` Most-recent on top. **If the list now exceeds 20 entries, immediately move the oldest bullet to `docs/decisions-archive.md`** under its date header (create the header if absent). Never leave `Recent decisions` above 20 entries.
```

- [ ] **Step 2: Add a new Ritual 2 step — cluster/phase closeout offload**

Insert after the current step 3 as step 3b:

```markdown
3b. **If a cluster phase closed, or the cluster itself closed**, write at most 3 sentences into `PROJECT-STATE.md` §Current focus (replacing the previous top entry) AND the **full** closeout paragraph into `docs/decisions-archive.md` under a new dated H2 header. The `**Previously:** …` cascade pattern in `PROJECT-STATE.md` is banned — if you find yourself writing it, you are writing in the wrong file.
```

- [ ] **Step 3: Update Ritual 3 — add plan-archive step**

Ritual 3 is "Cluster done." Add a new step at the end of its checklist:

```markdown
N. **Archive the plan + spec files.** `git mv docs/superpowers/plans/<cluster-plan>.md docs/superpowers/plans/archive/` and the matching spec to `docs/superpowers/specs/archive/`. Update `docs/superpowers/plans/INDEX.md` so the cluster row's "Plan file" column points at the new `archive/...` path. **The retro file stays where it is** in `docs/cluster-retros/`.
```

- [ ] **Step 4: Add a "Backlog hygiene" section**

At the end of the file, before any existing trailing content (or as a new final section), add:

```markdown
---

## Backlog hygiene (`docs/backlog.md`)

- When a backlog item is closed, **strike it through** (`~~text~~`) with a one-line closure note + date in the Details block. Do not delete — the crossed-out entry preserves the "why did we deprioritise X?" trail for the current quarter.
- Quarterly (or whenever `backlog.md` exceeds 500 lines), move all struck-through items to `docs/decisions-archive.md` under a `## Backlog roll-up YYYY-MM-DD` header.
- New backlog entries follow the schema in `backlog.md`'s reading-order preamble.
```

- [ ] **Step 5: Commit**

```bash
git add docs/CONTRIBUTING-OPS.md
git commit -m "docs: CONTRIBUTING-OPS Rituals 2+3 offload rules; backlog hygiene section"
```

---

## Phase 8 — Link sweep

### Task 13: Fix any stale references to moved files

**Files:**
- Modify: various (identified by grep)

- [ ] **Step 1: Grep for references to any archived file**

Run this broad sweep:
```bash
cd /home/clinton/dev/Corbomite
for moved in CORBOMITE_SPECIFICATION.md OBSIDIAN_SPECIFICATION.md HandbookGraphDrawing.txt \
             Obsidian_Sync_Overview.md graph-performance-log.md graph-view-blockers.md \
             2026-04-15-program-state.md 2026-04-15-session-0-intent-recheck.md \
             search-dsl-spec.md test-coverage-matrix.md test-coverage-bug-hunt.md \
             kde-power-software-design-guide; do
  matches=$(grep -rln --include='*.md' "$moved" docs/ CLAUDE.md 2>/dev/null | \
            grep -v "docs/archive\|docs/testing" || true)
  [ -n "$matches" ] && echo "=== $moved ===" && echo "$matches"
done
```

Expected: either empty output, or a small list of retros / docs that need link updates.

- [ ] **Step 2: For each surviving reference, fix the link**

For each file the Step 1 grep surfaced (likely zero or a handful), open it and update the link:
- Files moved to `docs/archive/` → add `archive/` to the path.
- Test-coverage files moved to `docs/testing/` → add `testing/` to the path.

Rule: **only update link paths.** Do not rewrite surrounding prose.

- [ ] **Step 3: Grep for references to archived cluster plans**

```bash
for moved in 2026-04-14-cluster-a-link-frontmatter-correctness.md \
             2026-04-14-cluster-b-vault-io.md \
             2026-04-14-cluster-c-lifecycle-plugin-primitives.md \
             2026-04-14-cluster-d-search-suggester-parity.md \
             2026-04-14-cluster-e-markoff-three-mode-pivot.md \
             2026-04-14-cluster-g-views-hierarchy-SCOUTING.md \
             2026-04-14-cluster-h-menus-hover-suggester-ui.md \
             2026-04-14-cluster-k-bases-SCOUTING.md \
             2026-04-15-cluster-f-templates-daily-notes-moment.md \
             2026-04-15-cluster-g-views-hierarchy.md \
             2026-04-15-cluster-g-part2-workspace.md \
             2026-04-15-cluster-i-metadatacache-parity.md \
             2026-04-15-cluster-j-embed-rendering.md \
             2026-04-15-test-enrichment-cycle.md \
             2026-04-16-cluster-q0-vault-architecture.md \
             2026-04-16-cluster-q-internal-plugin-wrapping.md \
             2026-04-16-spec-driven-test-audit.md \
             2026-04-17-cluster-k-bases.md \
             2026-04-17-cluster-n-plugin-ready-surfaces.md \
             2026-04-18-cluster-g-part3-split-semantics.md \
             2026-04-19-cluster-r-view-header-menus.md; do
  matches=$(grep -rln --include='*.md' "$moved" docs/ 2>/dev/null | \
            grep -v "docs/superpowers/plans/archive\|docs/superpowers/specs/archive" || true)
  [ -n "$matches" ] && echo "=== $moved ===" && echo "$matches"
done
```

For each hit, update the link to include `archive/` in the path.

- [ ] **Step 4: Verify nothing is broken**

Run the end-to-end markdown-link check from Task 10 Step 3 but across all live docs:

```bash
find docs CLAUDE.md -name '*.md' -not -path '*/archive/*' -not -path '*/obsidian-audit/*' | \
  while read f; do
    dir=$(dirname "$f")
    grep -oE '\]\(([^):]+\.md)\)' "$f" 2>/dev/null | sed 's/](//; s/)$//' | \
    while read rel; do
      target="$dir/$rel"
      [ -f "$target" ] || echo "BROKEN in $f: $rel"
    done
  done
```

Expected: no "BROKEN" lines.

- [ ] **Step 5: Commit**

```bash
git commit -am "docs: fix stale references after reorg"
```

(If Step 1-3 found nothing to update, skip the commit.)

---

## Phase 9 — Final read-through test

### Task 14: Dry-run the new session-start ritual

- [ ] **Step 1: Read `CLAUDE.md` top-to-bottom**

Confirm: you see pointers to `PROJECT-STATE.md`, `backlog.md`, `decisions-archive.md`, `CONTRIBUTING-OPS.md`, `plans/INDEX.md`, `obsidian-audit/`. You see the "Do not regrow PROJECT-STATE" rule. Session-start TL;DR has 6 steps.

- [ ] **Step 2: Read `PROJECT-STATE.md` top-to-bottom**

Measure time from open to understanding "current focus":
Run: `wc -l docs/PROJECT-STATE.md && wc -c docs/PROJECT-STATE.md`
Expected: ≤200 lines and ≤30 KB. Confirm there is **no** `**Previously:**` paragraph.

- [ ] **Step 3: Skim `backlog.md`**

Confirm: every section (§1-§10) has real content, not `*(populated in Task 8)*` stubs. Confirm the known-flaky tests appear under §10.

- [ ] **Step 4: Verify `INDEX.md` has no 404s**

Re-run the grep from Task 10 Step 3:
```bash
cd /home/clinton/dev/Corbomite
grep -oE '\]\([^)]+\.md\)' docs/superpowers/plans/INDEX.md | \
  sed 's/](//; s/)$//' | \
  while read p; do
    abs="docs/superpowers/plans/$p"
    [ -f "$abs" ] || echo "MISSING: $p"
  done
```
Expected: empty.

- [ ] **Step 5: Spot-check `decisions-archive.md`**

Confirm: it has at least 20 dated H2 entries in reverse-chronological order; opening the newest one shows the same closeout paragraph that used to be at the front of `PROJECT-STATE.md`'s `**Last updated:**` wall.

- [ ] **Step 6: Final commit if any Step 1-5 touch-ups were needed**

```bash
git status
# if anything is modified, commit it:
git commit -am "docs: final reorg touch-ups after read-through"
```

- [ ] **Step 7: Declare done**

Verify against spec §9 Definition of done — every bullet should be satisfied:
- `PROJECT-STATE.md` ≤200 lines, no `**Previously:**` paragraph. ✓
- `backlog.md` exists + populated. ✓
- `decisions-archive.md` exists + populated. ✓
- `INDEX.md` has no 404s; Done rows at archive/ paths. ✓
- `docs/archive/`, `plans/archive/`, `specs/archive/` exist with READMEs. ✓
- `docs/testing/` contains the two test-coverage files. ✓
- `CLAUDE.md` references new files + discipline rule. ✓
- `CONTRIBUTING-OPS.md` Rituals 2+3 updated + Backlog hygiene section. ✓
- State drift (A/B) resolved (both Done). ✓
