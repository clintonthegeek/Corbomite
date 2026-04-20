# Contributing — Operational Rituals

> **Living document.** The rules a returning agent or human contributor follows so multi-session work stays coherent. Three rituals: **Session start**, **Cluster phase done**, **Cluster done**. Each is a checklist, not advice — execute the steps in order.

This file is the *operating system* for the long-term-state machinery. The **state** itself lives in `docs/PROJECT-STATE.md`, the **plans** live in `docs/superpowers/plans/`, the **canonical audit** lives in `docs/obsidian-audit/`. This file tells you when to read or write each.

If you're a fresh agent / human starting a session, read **Ritual 1** first and follow it before touching any code.

---

## Ritual 1 — Session start

Goal: in <5 minutes, know what's going on, what to do next, and what to verify.

1. **Read `CLAUDE.md`** (project root). Confirms build/test commands and points at the long-term-state files.
2. **Read `docs/PROJECT-STATE.md` top-to-bottom.** This is the single source of truth for current focus, in-flight work, and open questions.
3. **Read the cluster plan(s) for the current focus.** Linked from PROJECT-STATE roadmap. Read all 15 sections (they're short individually) of any plan you'll touch.
4. **Read audit-doc sections cited in the plan you'll touch.** The plan's "Audit references" section names them. Don't skim — these are the specs you're implementing against.
5. **Read any addenda touching the same domain.** `docs/obsidian-audit/addenda/` — look for files dated after the plan you're working on, or filenames matching the cluster's domain.
6. **Glance at recent git commits.** `git log --oneline -10`. Catches any work the project-state file may not yet reflect (Ritual 2 may have been skipped). If you see a discrepancy, prefer git reality and update PROJECT-STATE before doing anything else.
7. **State the situation back to the human in one sentence.** "Per PROJECT-STATE, current focus is `<cluster>` at phase `<N>`, last touched `<date>`; next expected step is `<step>`. Confirm or redirect?" — wait for confirmation before starting work. If PROJECT-STATE says "Idle", ask which cluster to start on.

**Do NOT** start work without a confirmed focus. **Do NOT** silently start a different cluster than PROJECT-STATE indicates without updating PROJECT-STATE first.

---

## Ritual 2 — Cluster phase done

Goal: when a cluster phase or sub-task lands, leave PROJECT-STATE and the audit-state machinery accurate so the next session picks up cleanly.

Execute every applicable step. Skip none silently.

1. **Update the cluster's in-flight row in `PROJECT-STATE.md`:**
   - Bump `Phase: N of M` if a phase fully completed.
   - Update `Last completed step` with the step text + today's date.
   - Update `Next expected step` with the next step from the cluster plan.
   - Update `Date last touched`.
   - If new sub-questions arose during work, add them to `Open sub-questions`. If sub-questions were resolved, strike them through or remove.
2. **If a phase fully completed, update the Roadmap row's Status column** to `In progress (phase N+1)` or `Done` as appropriate.
3. **If a non-trivial decision was made** (technology choice, design call, deviation from the cluster plan), append one bullet to `PROJECT-STATE.md` `Recent decisions`. Format: `- **YYYY-MM-DD — <decision>.** Reason: <one sentence>.` Most-recent on top. **If the list now exceeds 20 entries, immediately move the oldest bullet to `docs/decisions-archive.md`** under its date header (create the header if absent). Never leave `Recent decisions` above 20 entries.
3b. **If a cluster phase closed, or the cluster itself closed**, write at most 3 sentences into `PROJECT-STATE.md` §Current focus (replacing the previous top entry) AND the **full** closeout paragraph into `docs/decisions-archive.md` under a new dated H2 header. The `**Previously:** …` cascade pattern in `PROJECT-STATE.md` is banned — if you find yourself writing it, you are writing in the wrong file.
4. **If work revealed a new fact about Obsidian** that the audit didn't capture — or contradicts what the audit said — write an addendum:
   - File: `docs/obsidian-audit/addenda/YYYY-MM-DD-<short-topic>.md`.
   - Format: header (date, discovered-during cluster, supersedes/extends which audit doc), then a paragraph or table of the fact, then a "Why noticed now" paragraph.
   - Append a one-line link in `docs/obsidian-audit/00-taxonomy.md` under the `## Addenda` section.
   - If the addendum **contradicts** an audit claim, also update the relevant cluster plan's "Audit references" to cite both. **Never edit the original audit doc** — it's the snapshot of what we believed at audit time.
5. **If work revealed a new gap or correctness bug**, add a bullet to the appropriate gap-list:
   - Markoff-related: `docs/obsidian-audit/01-markoff-gaps.md` under a `## Implementation additions — <YYYY-MM>` heading (create if missing).
   - Plugin-extension surface: `docs/obsidian-audit/02-extension-surfaces.md` similarly.
   - Otherwise (general Corbomite-vs-Obsidian gap): a Recent-decisions bullet in PROJECT-STATE plus a `GAP-ANALYSIS` addendum if priority warrants it.
6. **Run the relevant tests** (if you wrote/touched code). Check `cmake --build build && cd build && ctest --output-on-failure -R <pattern>`.
7. **Commit** (only if user authorised), with message format:
   ```
   <type>(<area>): <subject>

   Cluster <X> phase <N>: <one-line context>
   ```
   E.g. `feat(storage): add DataAdapter mtime-hint write\n\nCluster B phase 1: enables plugin data.json echo-suppression.`
8. **Tell the human one sentence:** "Phase `<N>` of cluster `<X>` complete; PROJECT-STATE updated; next step is `<step>`. Continue, or pause?"

**Do NOT** mark a phase complete in PROJECT-STATE if tests are failing or implementation is partial. Use "In progress (phase N — paused: <reason>)" instead.

---

## Ritual 3 — Cluster done

Goal: when a *full* cluster lands, archive its state cleanly and propagate downstream effects.

Execute every applicable step.

1. **Flip the cluster's Roadmap row Status to `Done`** in PROJECT-STATE.
2. **Move the cluster's in-flight row out of "In-flight work items"** in PROJECT-STATE (delete the row; the Roadmap row remains).
3. **Write a cluster retrospective** at `docs/cluster-retros/cluster-<letter>.md`:
   - One paragraph: what changed vs the original plan.
   - One paragraph: what surprised (good or bad).
   - One paragraph: what blocks/enables status changed for downstream clusters.
   - Optional: lessons for the next cluster.
   - Length: 200–500 words.
4. **Propagate unblocking effects.** Find every other cluster's Roadmap row that says `Blocked — waiting on <this cluster>`. Update them to `Not started` (or `In progress` if work has begun on them concurrently).
5. **Re-evaluate STUB and SCOUTING plans.** Any STUB or SCOUTING plan whose documented expansion-trigger just fired: flag in PROJECT-STATE's `Open questions` whether to expand it now. Wait for human direction. (STUBs expand in ~hour-scale effort; SCOUTING docs usually in ~2–4 hour effort.)
6. **Update `docs/superpowers/plans/INDEX.md`** Status column to `Done` for the completed cluster.
7. **Add a Recent-decisions bullet** in PROJECT-STATE: `- **YYYY-MM-DD — Cluster <X> landed.** See cluster-retros/cluster-<letter>.md.`
8. **Consider memory write.** If the cluster resolved a long-standing bug or established a load-bearing pattern, write a one-line memory entry per the auto-memory rules (`~/.claude/projects/.../memory/MEMORY.md`). Examples worth memorising: "vault-switch crash resolved by Kate-session destroy/rebuild pattern", "FrontMatter library is yaml-cpp configured per Obsidian options".
9. **Run the full test suite.** `cd build && ctest --output-on-failure`. Confirm no regressions in unrelated tests.
10. **Commit** with the same message convention; subject like "feat(<area>): cluster <X> complete".
11. **Tell the human:** "Cluster `<X>` complete; retrospective written; downstream `<list>` unblocked; suggest next focus is `<Y>`. Confirm or redirect?"
12. **Archive the plan + spec files.** `git mv docs/superpowers/plans/<cluster-plan>.md docs/superpowers/plans/archive/` and the matching spec to `docs/superpowers/specs/archive/`. Update `docs/superpowers/plans/INDEX.md` so the cluster row's "Plan file" column points at the new `archive/...` path. **The retro file stays where it is** in `docs/cluster-retros/`.

---

## Ritual 4 — Test enrichment cycle (recurring)

Goal: after a cluster lands or after a multi-session push of code, hunt for the bugs that per-class unit tests miss — cross-component, cross-session, and UI-observable behaviour.

**When to run:** after Ritual 3 (cluster done), or any time a multi-day implementation push has merged ≥10 commits without a coverage check, or on a fixed cadence (e.g. "every other cluster"). The human triggers the cycle; the agent executes it.

Execute every step in order.

1. **Open the matrix.** `docs/test-coverage-matrix.md`. Skim the seams and lifecycle columns. Add any new seam introduced by recent work as a new row (blank cells); add any new lifecycle as a new column.
2. **Refresh the test inventory.** Walk the existing tests in `tests/`, `libs/*/tests/` — for each test, identify which seam × lifecycle cell(s) it covers. Update the matrix cells (`✓ tst_<name>`).
3. **Pick the cycle's targets.** Choose N highest-risk blank cells (default `N=6`). "Highest-risk" = recently-touched code, persistence-heavy, or known to interact with multiple subsystems. Document the picks in a new "Cycle M" section in `docs/test-coverage-bug-hunt.md` under "Cycle log".
4. **For each target cell, write a failing scenario test.** Use Tier B (cross-session, in `tests/integration/`) by default. Use Tier A (`tests/e2e/`) only when the bug is UI-display-only. Each test:
   - Sets up state representing the lifecycle (e.g. for L4 "schema bump": pre-populate persisted DBs at version N, simulate version-N+1 open).
   - Asserts the expected behaviour.
   - If it passes: the cell wasn't a gap after all — update the matrix to `✓` and move on.
   - If it fails: this is a bug. Mint a `BUG-YYYYMMDD-NNN` ID, wrap the asserts with `QEXPECT_FAIL("", "BUG-xxx: <short reason>", Continue)`, append a row to the inventory table, update the matrix cell to the BUG-ID.
5. **Do NOT fix bugs in the same cycle.** The cycle is for hunting. Filing the bug = the deliverable. Fixes are scheduled separately by the human.
6. **Cycle close-out.** In `docs/test-coverage-bug-hunt.md`, update the cycle log entry with: cells targeted, bugs filed (by ID), tests landed (by name).
7. **Update PROJECT-STATE.** Add a "Recent decisions" bullet noting the cycle ran, link to the cycle's log entry.

**Do NOT** mark a cell `✓` based on any test that doesn't actually exercise the lifecycle dimension. "Tested in isolation" ≠ "tested in this lifecycle." When in doubt, leave the cell blank and add a partial-coverage `~` only if a test definitively covers part of the cell.

---

## Conventions

### Files that are **canonical** (treat as read-only):
- `docs/obsidian-audit/00-taxonomy.md` (except for `## Addenda` link list at tail)
- `docs/obsidian-audit/01-markoff-gaps.md` (except for `## Pass 2 additions` and `## Implementation additions` appendices)
- `docs/obsidian-audit/02-extension-surfaces.md` (same — appendix-only edits)
- `docs/obsidian-audit/domains/*.md` (15 domain docs — *never edit*; correct via addenda)
- `docs/obsidian-audit/{FEATURE-MATRIX,VAULT-FORMAT,GAP-ANALYSIS,PLUGIN-API-SKETCH,SHARED-SYMBOLS}.md` (Pass 3 synthesis — re-run synthesis to update; don't hand-edit)

### Files that are **living** (update per the rituals):
- `docs/PROJECT-STATE.md`
- `docs/CONTRIBUTING-OPS.md` (this file — update only when rituals themselves change)
- `docs/superpowers/plans/INDEX.md`
- `docs/superpowers/plans/2026-*-cluster-*.md` (cluster plans — update "Audit references" when addenda change facts; otherwise update via Ritual 2 sub-step 4)

### Files that are **append-only** (add new entries; don't rewrite existing ones):
- `docs/obsidian-audit/addenda/` (one new file per addendum)
- `docs/cluster-retros/` (one new file per landed cluster)
- `docs/decisions-archive.md` (quarterly archive of older PROJECT-STATE Recent decisions)

### Naming conventions
- Cluster plans: `YYYY-MM-DD-cluster-<letter>-<short-title>.md` (full plans), `…-STUB.md` suffix (stub plans — sketches ready to expand with ~hour-scale effort), or `…-SCOUTING.md` suffix (pre-plan notes capturing breadcrumbs when the cluster is externally blocked — not dispatchable, expand when trigger documented in the file fires).
- Addenda: `YYYY-MM-DD-<short-topic>.md`.
- Cluster retros: `cluster-<letter>.md`.
- Dates are absolute (YYYY-MM-DD), never relative ("yesterday", "next week").

### Commit message format
- `<type>(<area>): <subject>` first line (Conventional Commits style).
- Blank line.
- Body line: `Cluster <X> phase <N>: <one-line context>` when applicable.
- This makes `git log --grep="Cluster A"` immediately useful.

---

## When the rituals don't fit

Edge cases:
- **Quick bug fix outside any cluster.** Skip Rituals 2/3; use a normal commit. Add a one-line note to PROJECT-STATE Recent decisions if it's load-bearing.
- **Pure documentation update.** Skip Rituals 2/3; commit normally. If you're updating PROJECT-STATE itself, that *is* the ritual — no further bookkeeping.
- **Exploratory spike (no commit).** Skip Rituals 2/3. If the spike produced findings, write an addendum and add an Open Questions bullet so the next session can pick up.
- **Cluster plan needs revision mid-work.** Edit the cluster plan in place; add a one-line note in its header (`> **Revised YYYY-MM-DD:** <what changed>`); add a Recent-decisions bullet.

---

## Rationale (why these specific rituals)

- **Ritual 1 puts PROJECT-STATE first.** Avoids the failure mode of starting a session and immediately diverging from prior work because the agent never read where prior work left off.
- **Ritual 2 is checklist-shaped.** No "use your judgement" steps — judgement-free updates happen reliably even at session-end fatigue.
- **Ritual 3 has propagation steps.** A landed cluster unblocks others mechanically; if no one updates downstream rows, future agents see incorrect Blocked statuses.
- **Audit docs are read-only.** They cost ~25 hours of agent compute. Hand-editing them produces drift; addenda preserve the integrity of the audit-as-snapshot while letting facts evolve.
- **Living files are explicit.** No agent should ever edit a "canonical" doc and wonder if they were supposed to. Lists at the top of this file resolve it.

---

## Backlog hygiene (`docs/backlog.md`)

- When a backlog item is closed, **strike it through** (`~~text~~`) with a one-line closure note + date in the Details block. Do not delete — the crossed-out entry preserves the "why did we deprioritise X?" trail for the current quarter.
- Quarterly (or whenever `backlog.md` exceeds 500 lines), move all struck-through items to `docs/decisions-archive.md` under a `## Backlog roll-up YYYY-MM-DD` header.
- New backlog entries follow the schema defined at the top of `backlog.md`.
