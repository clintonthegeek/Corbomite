# Contributing — Operational Rituals

Checklists for a returning agent. The status page is [`PROJECT-STATE.md`](PROJECT-STATE.md). Plans live in [`superpowers/plans/`](superpowers/plans/). The audit lives in [`obsidian-audit/`](obsidian-audit/). The retired Phase C ritual is in [`archive/2026-10-17/CONTRIBUTING-OPS-before-slim.md`](archive/2026-10-17/CONTRIBUTING-OPS-before-slim.md). Do not execute that file.

Run Ritual 1 before touching code.

## Ritual 1 — Session start

1. Read [`../AGENTS.md`](../AGENTS.md), then [`PROJECT-STATE.md`](PROJECT-STATE.md). The trajectory is the order of work. PROJECT-STATE is the only status page.
2. Read [`../CLAUDE.md`](../CLAUDE.md) for build and test commands.
3. Read [`punch-list.md`](punch-list.md) through `## Next`, including `## Now` while that section has an open box. Do not pick from Backlog while an earlier trajectory step is open. The card-editor box may stay open; it does not hold Next up.
4. Read the plan the current step names. The Markoff finish plan is the record of landed work. Do not re-execute its "Already done" section.
5. Read the addendum or audit section that item cites. Read the corrections gate at the top of [`obsidian-audit/addenda/README.md`](obsidian-audit/addenda/README.md) before implementing from a frozen domain doc. To check a claim against Obsidian's source, use [`../RE/README.md`](../RE/README.md).
6. `git log --oneline -10`. If git and PROJECT-STATE disagree about what landed, trust git, fix PROJECT-STATE, and stop there. Do not grow the file.
7. State the trajectory step and the next action in one line, then do that action. Do not switch steps unless the user redirects.

## Ritual 2 — A step or a phase landed

1. Replace the trajectory text in [`PROJECT-STATE.md`](PROJECT-STATE.md) with at most three sentences. That file has no `Phase: N of M`, no last-completed step, no roadmap table, no Recent decisions, and no open-questions list. Do not invent them. Do not write a `Previously` cascade.
2. Write the full closeout as a new dated H2 in [`decisions-archive.md`](decisions-archive.md).
3. A new fact about Obsidian is an addendum: `docs/obsidian-audit/addenda/YYYY-MM-DD-<topic>.md`, plus a one-line link under `## Addenda` in [`obsidian-audit/00-taxonomy.md`](obsidian-audit/00-taxonomy.md). Never edit a frozen audit doc. If the addendum contradicts a claim, cite both from the cluster plan.
4. A new single-fix gap goes on the punch list as one line under Backlog, at its severity. Put it in Next only when the user says it outranks the current Next items. A multi-phase gap is a cluster stub plus an INDEX row, and it stays in trajectory step 4 until PROJECT-STATE says otherwise.
5. If you touched code: `cmake --build --preset dev -j 10`, then `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 10 -R <pattern>`. Offscreen is required. The exclusion is required. Tests exist only when `CORBOMITE_PORT_BUILD_TESTS=ON` (the `dev` preset sets it).
6. Commit the finished change. Subject: `<type>(<area>): <subject>`. Body may name the cluster and phase. The punch-list mark and any PROJECT-STATE or decisions-archive update go in that commit. Do not commit the RE corpus, the dirty `DocumentRenderer` hunk, or the untracked `libs/collabtext` tree.
7. One sentence: what landed, and what the next trajectory action is. Continue that action.

Do not mark a step done when tests are failing or the work is partial.

## Ritual 3 — A cluster is done

1. Move its row to Closed in [`superpowers/plans/INDEX.md`](superpowers/plans/INDEX.md). Do not add a roadmap row to PROJECT-STATE.
2. Replace the trajectory text (at most three sentences) and write the closeout in `decisions-archive.md`, as in Ritual 2. Do not add a Recent-decisions bullet anywhere in PROJECT-STATE.
3. Write `docs/cluster-retros/cluster-<letter>.md`: what changed, what surprised, what it unblocks. A few short paragraphs.
4. If another plan says it was blocked on this cluster, update that plan's status. Leave it in trajectory step 4 unless PROJECT-STATE promotes it.
5. Run the full suite: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 10`.
6. `git mv` the plan to `docs/superpowers/plans/archive/` and its spec to `docs/superpowers/specs/archive/`. Point the INDEX row at the new path. The retro stays in `docs/cluster-retros/`.
7. Commit the closeout. Tell them the cluster is done and which step is next, then start that step.

## Ritual 4 — Test enrichment

The human triggers this. The agent executes it. Hunt only. Do not fix bugs in the same cycle.

1. Open [`testing/test-coverage-matrix.md`](testing/test-coverage-matrix.md). Add a row for a new seam and a column for a new lifecycle.
2. Refresh which test covers which cell.
3. Pick the highest-risk blank cells (default six). Log them as a cycle in [`testing/test-coverage-bug-hunt.md`](testing/test-coverage-bug-hunt.md).
4. For each cell, write a scenario test, Tier B (`tests/integration/`) unless the bug is display-only. A pass fills the cell. A fail gets a `BUG-YYYYMMDD-NNN` id, `QEXPECT_FAIL`, and a matrix cell set to that id.
5. Close the cycle log with cells, bug ids, and test names.
6. If the cycle's result is load-bearing, one paragraph in `decisions-archive.md`. Do not add it to PROJECT-STATE.

A cell is `✓` only when a test exercises that lifecycle. Isolation is not that lifecycle.

## Ritual 5 — Markoff and Corbomite together

Applies when work touches `/home/clinton/dev/Markoff/` or the submodule `libs/markoff-family/`. Adds to Rituals 1–3. Does not replace them.

1. Cross-repo decisions are dated briefs in both repos' `docs/handoff/` directories.
2. A pin bump is its own commit: `chore(submodule): advance markoff-family to <sha>`. Before committing, run `git -C libs/markoff-family rev-list <new>..<current>`.
3. Do not re-pin into a range a handoff brief marks unsafe.
4. After every re-pin, build with `cmake --build --preset dev -j 10` and run `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 10`. Re-apply the `DocumentRenderer` history-pending check (finish plan §2) and re-check punch items that name the pin.
5. On this pin, do not `add_subdirectory` the untracked `libs/markoff-family/libs/collabtext/` tree. CollabText is the installed package.

Markoff commits follow Markoff's convention. Corbomite pin bumps use the subject above.

## Conventions

Canonical, do not hand-edit except the named appendix:

- `docs/obsidian-audit/00-taxonomy.md` except the `## Addenda` link list
- `docs/obsidian-audit/01-markoff-gaps.md` and `02-extension-surfaces.md`, except their appendices
- `docs/obsidian-audit/domains/*.md`
- the Pass 3 synthesis files in `docs/obsidian-audit/` (`FEATURE-MATRIX`, `VAULT-FORMAT`, `GAP-ANALYSIS`, `PLUGIN-API-SKETCH`, `SHARED-SYMBOLS`)

Living:

- `docs/PROJECT-STATE.md`, this file, `docs/superpowers/plans/INDEX.md`, `docs/punch-list.md`
- a cluster plan's audit-reference list, when an addendum changes a fact

Append-only:

- `docs/obsidian-audit/addenda/`
- `docs/cluster-retros/`
- `docs/decisions-archive.md`

Names: cluster plans `YYYY-MM-DD-cluster-<letter>-<title>.md`; addenda `YYYY-MM-DD-<topic>.md`; retros `cluster-<letter>.md`. Dates are absolute. Post-reset letters start at A (2026-04-26). A letter that also exists in `plans/archive/` or in a pre-reset closeout is "legacy Cluster X" when you mean the old one.

## When the ritual does not fit

- A small fix outside a cluster: commit it when its tests are green. If the call is load-bearing, one paragraph in `decisions-archive.md` in that commit.
- A docs-only edit: no Ritual 2 or 3. Editing PROJECT-STATE is the bookkeeping.
- A spike with no commit: an addendum or one Backlog line if the finding should survive. Do not add an open-questions section to PROJECT-STATE.
- A plan that needs a revision: edit it in place and add a dated header note. A load-bearing call also gets a `decisions-archive.md` paragraph.

## Punch-list hygiene

- The live file is open work plus the two working-tree `[x]` lines that say they are not committed. Closed history stays in [`archive/2026-10-17/punch-list-2026-10-17-full.md`](archive/2026-10-17/punch-list-2026-10-17-full.md). Do not copy closed items back.
- Mark `[x]` when the work is in the tree. Say in the line if it is not committed yet.
- A new item is one line. Default it to Backlog under its severity. Do not recreate dated "Added" sections. The order of work is PROJECT-STATE's trajectory, then Now, then Next, then Backlog.
- Guardrails at the top of the punch list are constraints. Do not turn one into a task.
- Do not split the punch list into per-domain files unless the user asks.
