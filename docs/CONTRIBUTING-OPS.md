# Contributing — Operational Rituals

> **Living document.** The rules a returning agent or human contributor follows so multi-session work stays coherent. Three rituals: **Session start**, **Cluster phase done**, **Cluster done**. Each is a checklist, not advice — execute the steps in order.

This file is the *operating system* for the long-term-state machinery. The **state** itself lives in `docs/PROJECT-STATE.md`, the **plans** live in `docs/superpowers/plans/`, the **canonical audit** lives in `docs/obsidian-audit/`. This file tells you when to read or write each.

If you're a fresh agent / human starting a session, read **Ritual 1** first and follow it before touching any code.

---

## Ritual 1 — Session start

Goal: in <5 minutes, know what's going on, what to do next, and what to verify. Two tracks (punch list + strategic clusters) — both must be checked.

1. **Read `CLAUDE.md`** (project root). Confirms build/test commands and points at the long-term-state files.
2. **Read `docs/PROJECT-STATE.md` top-to-bottom** (~30 lines). Names current focus across both tracks.
3. **Skim `docs/punch-list.md` P0 section.** Anything new at the top since you last looked? P0s are silent vault-format-corruption risks — by default, drain them before resuming cluster work unless PROJECT-STATE says otherwise.
4. **If picking up strategic-cluster work, read the cluster plan(s) for the current focus.** Linked from PROJECT-STATE.
5. **If picking up a punch-list item, read the audit sub-report it cites.** Each punch-list line ends with `see audit-2026-04-26/<domain>.md §"section"` — that section has the full analysis + suggested fix sketch.
6. **Read audit-doc sections cited in the plan or punch-list item.** Don't skim — these are the specs you're implementing against. Look at `docs/obsidian-audit/addenda/` for any addenda dated after the audit/plan you're using.
7. **Glance at recent git commits.** `git log --oneline -10`. Catches work PROJECT-STATE may not yet reflect (Ritual 2 may have been skipped). If you see a discrepancy, prefer git reality and update PROJECT-STATE before doing anything else.
8. **State the situation back to the human in one sentence.** "Per PROJECT-STATE, current focus is `<track>: <cluster-or-punch-item>`, last touched `<date>`; next step is `<step>`. Confirm or redirect?" — wait for confirmation before starting work. If PROJECT-STATE says "Idle", ask which track to start on (recommend punch-list P0s by default).

**Do NOT** start work without a confirmed focus. **Do NOT** silently start a different cluster or punch-list item than PROJECT-STATE indicates without updating PROJECT-STATE first.

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
6. **Run the relevant tests** (if you wrote/touched code). Check `cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j 10 -R <pattern>`. The `QT_QPA_PLATFORM=offscreen` env var is **required** — without it ~24 GUI tests abort. Tests only exist if the build was configured with `CORBOMITE_PORT_BUILD_TESTS=ON` (default OFF; the checked-in `build-dev` cache has it ON).
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
9. **Run the full test suite.** `cd build-dev && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j 10` (the offscreen env var is **required** — without it ~24 GUI tests abort; requires a configure with `CORBOMITE_PORT_BUILD_TESTS=ON`). Confirm no regressions in unrelated tests.
10. **Commit** with the same message convention; subject like "feat(<area>): cluster <X> complete".
11. **Tell the human:** "Cluster `<X>` complete; retrospective written; downstream `<list>` unblocked; suggest next focus is `<Y>`. Confirm or redirect?"
12. **Archive the plan + spec files.** `git mv docs/superpowers/plans/<cluster-plan>.md docs/superpowers/plans/archive/` and the matching spec to `docs/superpowers/specs/archive/`. Update `docs/superpowers/plans/INDEX.md` so the cluster row's "Plan file" column points at the new `archive/...` path. **The retro file stays where it is** in `docs/cluster-retros/`.

---

## Ritual 5 — Working Markoff + Corbomite as one system (cross-repo)

Goal: when work touches Markoff, keep both repos coherent and both sets of conventions honoured.

Applies whenever you touch `/home/clinton/dev/Markoff/` — directly, or via the submodule at `libs/markoff-family/`. Adds obligations on top of Rituals 1/2/3, it does not replace them.

> **Rewritten 2026-06-10.** The original version of this ritual described the Phase C ownership-handoff workflow (per-work-unit `v0.X.0` tags, `MARKOFF_READING_USE_REAL_COREDEPS` stub retirement, Phase B bridge code, the `phase-c-status.md` board). That era closed with Markoff's 2026-05-25 `v0.7.0-freeze` merge and the Corbomite re-pin. The retired text is preserved below under "Historical — Phase C workflow"; do not execute it.

### Current workflow (post-`v0.7.0-freeze`)

Markoff is a submodule pinned at `libs/markoff-family/`. Cross-repo work follows four rules:

1. **Coordinate via dated handoff briefs** in both repos' `docs/handoff/` directories (Corbomite side: `docs/handoff/`; Markoff side: `libs/markoff-family/docs/handoff/`). Every cross-repo steer, freeze, or merge confirmation is a dated brief, not a chat-only agreement.
2. **Advance the pin explicitly.** Pin bumps land as their own commits with subject `chore(submodule): advance markoff-family to <sha>`. **Pre-flight audit:** always run `git -C libs/markoff-family rev-list <new>..<current>` before committing the bump — see the `feedback_submodule_pin_audit` memory.
3. **Never re-pin into a window a handoff brief warns against.** If a Markoff-side brief flags a commit range as unsafe for consumers, the pin skips past it or waits.
4. **After every re-pin, run the full Corbomite suite** (`cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j 10` — the offscreen env var is required; configure needs `CORBOMITE_PORT_BUILD_TESTS=ON`) **and re-check the punch-list items gated on the pin** (items waiting on a Markoff-side fix).

Smoke end-to-end (`./build-dev/bin/Corbomite` + ctest) before declaring any cross-repo work-unit shipped.

### Commit-convention notes

Markoff-side commits use Markoff's convention (no `Cluster X phase N` footer — Markoff isn't cluster-aware). Corbomite-side pin bumps use the `chore(submodule): advance markoff-family to <sha>` subject above. Both sides get the same `Co-Authored-By` trailer.

### Historical — Phase C workflow (retired 2026-05-25; do not execute)

> **Dated banner, 2026-06-10:** everything below describes the Phase C ownership era and is kept only as a record. The handoff briefs live under `libs/markoff-family/docs/handoff/`.

#### Ownership scope (historical)

This agent held commit authority on Markoff's `master` per the ownership-handoff brief in `libs/markoff-family/docs/handoff/`. Permitted:
- Create and land specs, plans, implementation, and tags in the Markoff repo.
- Retire Phase B bridge code (`MARKOFF_READING_USE_REAL_COREDEPS`, stubs) as Phase C work-units required.
- Move types between repos when the interface design called for it.

Not permitted (without user check-in):
- Change Markoff's public API surface (class names under `Markoff::`, public header paths under `include/markoff/`) in a way not already in a landed spec.
- Break the current Markoff tag that CorbomiteApp builds against without landing both sides in the same pin bump.
- Vendor the `mmdr` Rust crate into Markoff (Phase B decision; reopenable only via user ok).

#### Markoff-side invariants (historical)

1. **Standalone Markoff build green.** `cd /home/clinton/dev/Markoff && rm -rf build-dev && cmake -S . -B build-dev && cmake --build build-dev -j && cd build-dev && ctest` on a fresh checkout must pass with zero external projects present.
2. **No `Corbomite`-named types in Markoff public interfaces.** The Phase B stubs under `libs/markoff-reading/stubs/corbomite/` were the one exception; they retired in Phase C work-unit C1.
3. **Tests that need Corbomite concretes gate on the CMake option** (Phase B) or its Phase C successor (the DI seam's host-injection mechanism).
4. **Every Phase C work-unit tags a new Markoff minor version.** `v0.3.0`, `v0.4.0`, …. Tags are append-only; never force-move.
5. **Markoff `master` is append-only.** No force-push. Revert commits are the only way to undo.
6. **Commit identity is unified.** Same author + co-author trailer used on Corbomite commits.

#### Session flow for a Phase C work-unit (historical)

Authoritative status board was **`libs/markoff-family/docs/phase-c-status.md`** in the Markoff submodule: spec in `docs/specs/`, plan in `docs/plans/`, implement on Markoff `master`, tag `v0.X.0`, bump the submodule pin, adapt the Corbomite side, smoke end-to-end, retire bridge code (tag `v0.X.1`), close the work-unit on the status board, update Corbomite's PROJECT-STATE. Phase C completed when all seven work-units closed, the `MARKOFF_READING_USE_REAL_COREDEPS` option and its stubs were gone, and CorbomiteApp ran on the final Phase C Markoff tag with no bridge code lingering.

---

## Ritual 4 — Test enrichment cycle (recurring)

Goal: after a cluster lands or after a multi-session push of code, hunt for the bugs that per-class unit tests miss — cross-component, cross-session, and UI-observable behaviour.

**When to run:** after Ritual 3 (cluster done), or any time a multi-day implementation push has merged ≥10 commits without a coverage check, or on a fixed cadence (e.g. "every other cluster"). The human triggers the cycle; the agent executes it.

Execute every step in order.

1. **Open the matrix.** `docs/testing/test-coverage-matrix.md`. Skim the seams and lifecycle columns. Add any new seam introduced by recent work as a new row (blank cells); add any new lifecycle as a new column.
2. **Refresh the test inventory.** Walk the existing tests in `tests/`, `libs/*/tests/` — for each test, identify which seam × lifecycle cell(s) it covers. Update the matrix cells (`✓ tst_<name>`).
3. **Pick the cycle's targets.** Choose N highest-risk blank cells (default `N=6`). "Highest-risk" = recently-touched code, persistence-heavy, or known to interact with multiple subsystems. Document the picks in a new "Cycle M" section in `docs/testing/test-coverage-bug-hunt.md` under "Cycle log".
4. **For each target cell, write a failing scenario test.** Use Tier B (cross-session, in `tests/integration/`) by default. Use Tier A (`tests/e2e/`) only when the bug is UI-display-only. Each test:
   - Sets up state representing the lifecycle (e.g. for L4 "schema bump": pre-populate persisted DBs at version N, simulate version-N+1 open).
   - Asserts the expected behaviour.
   - If it passes: the cell wasn't a gap after all — update the matrix to `✓` and move on.
   - If it fails: this is a bug. Mint a `BUG-YYYYMMDD-NNN` ID, wrap the asserts with `QEXPECT_FAIL("", "BUG-xxx: <short reason>", Continue)`, append a row to the inventory table, update the matrix cell to the BUG-ID.
5. **Do NOT fix bugs in the same cycle.** The cycle is for hunting. Filing the bug = the deliverable. Fixes are scheduled separately by the human.
6. **Cycle close-out.** In `docs/testing/test-coverage-bug-hunt.md`, update the cycle log entry with: cells targeted, bugs filed (by ID), tests landed (by name).
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

## Punch-list hygiene (`docs/punch-list.md`)

> Replaces the legacy `backlog.md` (retired 2026-04-26 in the tracking reset; archived at `docs/archive-2026-04-26/backlog.md`).

- When a punch-list item is closed by a commit, **mark the checkbox `[x]`** with a `(YYYY-MM-DD #commit-shortsha)` suffix. Do **not** delete — the closed entry preserves the audit→fix trail.
- New punch-list items go under their severity bucket (P0 most urgent, P6 least). One-line entries only. Format: `- [ ] Pn [domain] short title — file.cpp:line — see audit-YYYY-MM-DD/<domain>.md §"section"`. Match existing entries.
- When discovering a new gap mid-cluster: if it's a single-fix item, add it to the punch list under the appropriate severity bucket and continue. If it's a multi-phase initiative, create a new cluster stub plan under `docs/superpowers/plans/` and add a row to `INDEX.md`.
- When the punch list exceeds ~150 active items, propose splitting into per-domain files (`docs/punch-list/<domain>.md`) — flag it in PROJECT-STATE Open questions, don't act unilaterally.
- After running a fresh audit cycle that supersedes the prior one (e.g. another `docs/audit-YYYY-MM-DD/` is created): roll the closed (`[x]`) entries to `docs/decisions-archive.md` under a `## Punch-list roll-up YYYY-MM-DD` header, regenerate the active-items list from the new audit, update the file's "Last refreshed" date.
