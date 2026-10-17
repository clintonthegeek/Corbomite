# Project state

Where we are. Not a journal. A closeout is a paragraph in [`decisions-archive.md`](decisions-archive.md). This file stays short. The 2026-10-17 journal copy is frozen at [`archive/2026-10-17/PROJECT-STATE-before-slim.md`](archive/2026-10-17/PROJECT-STATE-before-slim.md). Do not execute it.

## Trajectory

Do these in order. Do not start a later step while an earlier one is open. A box blocked on an upstream handoff does not hold the next step.

**Current action:** the metadata `changed` payload in punch-list §Next.

1. **Markoff vNext** is in the working tree, uncommitted. The pin, the session seam, canvas paint identity, and the host comment cleanup are landed. Offscreen suite **267/267** on this pin. The card-editor swap stays blocked on the Markoff handoff and does not hold the current action. Re-apply the `DocumentRenderer` history-pending check on every re-pin. Do not build a collaboration journal. Do not redo this step.
2. **Punch-list §Next, in progress.** Two `[x]` items are in the tree, not committed: the metadata-cache fix (`indexFinished` waits until worker parses are done), and the frontmatter decision — one permissive reader, and `FileManager` splices only the YAML payload so a CRLF note's delimiters survive a property edit. The L5 soak is closed. Its findings are open `[cluster-l]` backlog items, after the metadata `changed` payload.
3. **Then — Cluster M**: M6.2 rename rewriting, then M6.3 missing-file placeholder.
4. **Later** — the punch-list backlog, then clusters F-a, Q, D.5, and F-b, plus Cluster O's leftover inspector slots. Index: [`superpowers/plans/INDEX.md`](superpowers/plans/INDEX.md). `2026-06-10-road-to-dogfood.md` is not a dispatch.

Markdown files stay the on-disk contract. Standing constraints are the guardrails at the top of [`punch-list.md`](punch-list.md).

## Facts a session needs

- Tests (2026-10-17): **267/267** with `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 10`, on Markoff `dca64686`. Markoff's own tests are off. The previous pin's count was 354.
- Punch-list counts live in that file's header. Closed history: [`archive/2026-10-17/punch-list-2026-10-17-full.md`](archive/2026-10-17/punch-list-2026-10-17-full.md).
- Dev config is `~/.config/corbomite-devrc`. Do not commit the RE corpus. Do not follow links into `docs/archive/` for live tasks.

## Session start

Read [`../AGENTS.md`](../AGENTS.md), then this file, then punch-list §Next. Commit a finished change once its tests are green, and update the punch list and this file in that commit. Do not commit the RE corpus.
