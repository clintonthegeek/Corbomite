# Project state

Where we are. Not a journal. A closeout is a paragraph in [`decisions-archive.md`](decisions-archive.md). This file stays short. The 2026-10-17 journal copy is frozen at [`archive/2026-10-17/PROJECT-STATE-before-slim.md`](archive/2026-10-17/PROJECT-STATE-before-slim.md). Do not execute it.

## Trajectory

Do these in order. Do not start a later step while an earlier one is open. A box blocked on an upstream handoff does not hold the next step.

**Current action:** trajectory step 4 — the punch-list backlog (open P1s first), then clusters Q, D.5, F-b and Cluster O's inspector slots. Cluster F-a is landed in code (all phases; commits `186f67f3`/`4dc0f607`/`f9b4b7e7`/`cfa9f0cb`), leaving only its two live-eyeball gates, queued in the punch list.

1. **Markoff vNext** landed (`150446a2`): the pin (`dca64686`), the session seam, canvas paint identity, and the host comment cleanup. The card-editor swap and the re-pin check stay open in **Now**, blocked on the Markoff handoff, and do not hold step 3. Re-apply the `DocumentRenderer` history-pending check on every re-pin; do not build a collaboration journal.
2. **Punch-list §Next closed 2026-10-17.** The metadata-cache `indexFinished` fix (`30b9f264`), the frontmatter single-reader decision plus payload splice (`f5e5bf13`), the canvas-reference index M6.1 (`faba8c25`), and the metadata `changed` payload (`3f1d05f1`) are all in the tree. Full closeout in [`decisions-archive.md`](decisions-archive.md).
3. **Cluster M6 landed in code** (M6.2 `05961e27`): `FileManager` routes non-Markdown sources through a per-extension `LinkRewriter`, and `Canvas::CanvasLinkRewriter` rewrites file-node, group, and text-card links through `CanvasDocument`; M6.3 adds the missing-file placeholder with Create / Locate… / Remove and vault-event auto-resolve. M6's live-eyeball exit check is queued as punch-list `[canvas][P3][cluster-m]`.
4. **Later** — the punch-list backlog, then clusters Q, D.5, and F-b, plus Cluster O's leftover inspector slots. Cluster F-a (File Recovery & Note Composer) shipped 2026-10-17 except its two live-eyeball gates (P1.8 modal/restore, P3.7 merge), which are queued in the punch list. Index: [`superpowers/plans/INDEX.md`](superpowers/plans/INDEX.md). `2026-06-10-road-to-dogfood.md` is not a dispatch.

Markdown files stay the on-disk contract. Standing constraints are the guardrails at the top of [`punch-list.md`](punch-list.md).

## Facts a session needs

- Tests (2026-10-17): **274/274** with `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 3`, on Markoff `dca64686`. Markoff's own tests are off. (Was 267 at session start; +2 canvas tests, −1 retired `tst_vaultscanner`, +5 Cluster F-a: `tst_recovery_snapshot`, `tst_recovery_pruning`, `tst_version_history`, `tst_merge_file_dialog`, `tst_merge_file_pipeline`.) Build with `-j 3` on this machine (4 cores, shared) — the old `-j 10` was for a different dev box.
- Punch-list counts live in that file's header. Closed history: [`archive/2026-10-17/punch-list-2026-10-17-full.md`](archive/2026-10-17/punch-list-2026-10-17-full.md).
- Dev config is `~/.config/corbomite-devrc`. Do not commit the RE corpus. Do not follow links into `docs/archive/` for live tasks.

## Session start

Read [`../AGENTS.md`](../AGENTS.md), then this file, then punch-list §Next. Commit a finished change once its tests are green, and update the punch list and this file in that commit. Do not commit the RE corpus.
