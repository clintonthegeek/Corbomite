# Corbomite — agent orientation

Read this, then [`docs/PROJECT-STATE.md`](docs/PROJECT-STATE.md). That file is the only status page. When any other doc disagrees with it, PROJECT-STATE wins.

## What to do

Follow the trajectory in order. Do not start a later step while an earlier one is open. A box that says it is blocked on an upstream handoff does not hold the next step. The canvas card editor is that box.

The current action is trajectory step 3: Cluster M6.3 (missing-file placeholder). Then the backlog.

Markoff vNext's unblocked work is already in the working tree, uncommitted. Do not redo it. Do not build a collaboration journal. Markdown files stay the on-disk contract.

State the trajectory step and the next action in one line, then do that action.

## Before editing

1. Read PROJECT-STATE top to bottom.
2. Read the punch list through `## Next`, including `## Now` while it has an open box. Do not pick from Backlog before that.
3. Read the plan that step names.
4. Before implementing from a frozen Obsidian audit doc, read the corrections gate at the top of [`docs/obsidian-audit/addenda/README.md`](docs/obsidian-audit/addenda/README.md).
5. `git log --oneline -10`. If git and PROJECT-STATE disagree about what landed, trust git, fix PROJECT-STATE, and stop. Do not grow that file.

Build and test commands are in [`CLAUDE.md`](CLAUDE.md). The checklists for closing a step are in [`docs/CONTRIBUTING-OPS.md`](docs/CONTRIBUTING-OPS.md).

## Commit, and update the status docs

Commit when a change is finished and its tests are green. Do not leave a finished fix uncommitted for the next session. One concern per commit. Subject: `<type>(<area>): <subject>`.

In that same commit, update the docs that track the work:

- Check the punch-list box. A box is `[x]` when the work is in the tree. The line says if a commit is still owed.
- If a trajectory step closed, replace that step's text in PROJECT-STATE with at most three sentences. Write the full closeout as a new dated H2 in [`docs/decisions-archive.md`](docs/decisions-archive.md). Do not add a `Previously` cascade, a Recent-decisions list, or phase counters.
- A new fact about Obsidian is an addendum under `docs/obsidian-audit/addenda/`. Do not edit the frozen audit docs. A Corbomite choice goes in the plan or in [`docs/cluster-retros/`](docs/cluster-retros/).

Do not commit the RE corpus, secrets, the dirty `DocumentRenderer` history-pending hunk, or the untracked `libs/markoff-family/libs/collabtext/` tree.

## Checking behaviour against Obsidian

The readable Obsidian 1.13.7 tree is [`RE/README.md`](RE/README.md). Regenerate it with `RE/extract-obsidian.sh`. Query it with `RE/tools/q.js`. The corpus is gitignored. Never commit it, and do not delete `RE/tools/`.

Durable facts live in [`docs/obsidian-audit/addenda/`](docs/obsidian-audit/addenda/). The domain docs in `docs/obsidian-audit/domains/` are frozen and wrong in places. Read the corrections gate first. Corbomite's feature parity is [`docs/PARITY-MATRIX.md`](docs/PARITY-MATRIX.md), not the frozen Pass 3 synthesis files.

## Do not

- Grow PROJECT-STATE. It stays about a page.
- Pick work from `docs/audit-2026-04-26/`, from `docs/superpowers/plans/2026-06-10-road-to-dogfood.md`, or from anything under `docs/archive/` or `docs/superpowers/{plans,specs}/archive/`.
- Repair relative links inside those archives.
- `add_subdirectory` the untracked collabtext tree. CollabText is the installed package. `MARKOFF_BUILD_LIVE` stays OFF.
