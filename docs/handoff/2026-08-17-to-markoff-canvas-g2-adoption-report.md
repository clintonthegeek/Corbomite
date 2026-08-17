# Handoff → Markoff devs: G2 (Corbomite adoption) dogfood report — canvas leaf verified, 10/14 findings fixed, 4 low-severity gaps remain

**From:** Corbomite (downstream consumer of `Markoff::Canvas::EditorWidget`)
**Date:** 2026-08-17
**Corbomite branch:** `feature/find-replace`
**Markoff pin:** `03f7ceb1` (was `94f661c3` at Cluster K's Phase 0 re-pin, `2026-08-15`)
**Plan:** [`docs/superpowers/plans/2026-08-15-cluster-k-markoff-canvas-adoption.md`](../superpowers/plans/2026-08-15-cluster-k-markoff-canvas-adoption.md) — Phase 3 (gap-fix) now done.

## TL;DR

G2's open question — "is canvas ready to adopt as Corbomite's LivePreview
engine?" — has an answer: **yes, with 4 low-severity gaps left, none of
which block continuing.** Two dogfood sessions (2026-08-16/17) behind the
`CanvasLivePreview` settings toggle found 14 issues; 10 are fixed and
**live-user-confirmed** against the running app (not just offscreen
tests). The remaining 4 are cosmetic/feature-gap, not correctness bugs,
and one is frozen pending a separate Markoff item (E3, callouts).

Per prior direction, all fixes were patched **directly in this
submodule** rather than routed through a handoff-and-wait cycle — joint
local ownership of both repos makes that faster for bugs this size. This
doc is a status report closing the G2 loop, not an ask; nothing here
requires Markoff-side action unless you want to fold these fixes back
into your own working tree ahead of Corbomite's next re-pin cadence (they
already are your working tree's history — see commits below — this is
just the write-up).

## What shipped (10 of 14 findings)

All in `libs/markoff-canvas` unless noted. Full detail + root causes in
Corbomite's `docs/decisions-archive.md` (2026-08-17 entries) and
`docs/punch-list.md` `[cluster-k]` section.

| # | Finding | Commit(s) | User-confirmed |
|---|---|---|---|
| 1 | New Note unusable — zero-block empty doc, every keystroke bailed | `9389fb21` | yes |
| 2 | `$$` math block Enter/Backspace/Delete lockup | `9389fb21`, `794e28ec` | yes |
| 3 | Undo after paste leaves phantom "unresolved link" styling | `9389fb21` | yes |
| 4 | List-item promotion doubles the typed `-` marker | `a0dd2a08` | yes |
| 5 | Tab leaks focus to app chrome instead of acting on text | `a0dd2a08`, `794e28ec` | yes |
| 6 | Wikilink click/middle-click don't match Obsidian nav semantics | `7b386915` (+ Corbomite `44d9eb66`, see below) | yes |
| 7 | F3 find-next doesn't scroll the viewport to the match | `8e091a8f` | yes — "works great" |
| 8 | Double-click word-select / triple-click paragraph-select missing | `b2bd4d60` | yes — "works great" |
| 9 | Ctrl+Scroll scrolled instead of zooming; Ctrl+=/Ctrl+-/Ctrl+0 did nothing | `4f94e61b`, `03f7ceb1` | yes |
| 10 | Bare-URL / no-alt-text image embeds render blank until clicked into | `1e45ae8e` | yes — "properly show placeholders" |

**Two findings needed a second pass after live re-testing caught an
incomplete fix** — logged here since it's a pattern, not a one-off:
- Math Enter (#2): first pass always inserted a literal newline with no
  way to exit the block; second pass added an end-of-formula split.
- Tab (#5): first pass added literal-tab-insertion inside `keyPressEvent`,
  which never ran because `QWidget::event()` steals Tab via
  `focusNextPrevChild()` before `keyPressEvent()` — offscreen tests
  couldn't catch this since there's no toolbar to focus into in that
  harness. Fixed by overriding `focusNextPrevChild()` to always return
  `false`.
- Wikilink (#6) needed a second pass too, but the second half of the bug
  turned out to live **outside** this submodule entirely — see below.

## One finding, #6, exposed a bug in Corbomite's own navigation plumbing

Fixing the click-activation gesture (canvas-side, correct, `7b386915`)
surfaced that Corbomite had **no in-place-navigate path at all** for link
clicks — every activation, plain or middle-click alike, funneled through
`onNoteActivated`/`openFileInWorkspace`, which only ever creates a new
leaf or switches to one already showing the file. `WorkspaceLeaf`
already had a working `navigate()` + `LeafHistory`, wired to the
tab-frame's back/forward buttons, that nothing had ever called from the
link-click path. This is a Corbomite host-wiring gap unrelated to canvas
specifically (it would have affected Live/Styled identically); fixed in
Corbomite `44d9eb66`, not in this submodule. Flagging in case it's useful
context if you ever build/verify link-navigation against a host harness
of your own — the shape (`openInNewTab` bool needs to survive the
leaf→host signal boundary) may be a useful sanity check for other
consumers of `Markoff::LinkActivation`.

## What's still open (4, all low severity)

- **Callouts render as empty lines** — pre-existing gap, frozen pending
  Markoff's own E3 item; not new information, just confirmed it persists
  on canvas too.
- **Document-title header not surfaced in Corbomite** — canvas already
  ships the title band; this is Corbomite-side UI wiring, not a canvas
  gap.
- **No readable-line-width (fixed-width column) setting** — Corbomite
  settings-page feature; unclear yet whether canvas needs a new render
  parameter to support it or whether it's purely a Corbomite-side layout
  constraint. Not scoped in detail yet.
- **Format-verb reveal radius wider than expected** — `my |**bold**
  word` / `my **bold** |word` reveal delimiters when the caret is merely
  *adjacent* to the token, not inside it. Cosmetic.

## Suite state

markoff-family full suite: **316/316** (`scripts/run-tests.sh`, offscreen).
Corbomite full suite: **154/154** offscreen (excl.
realistic/benchmark/perf; one `tst_canvas_perf_500` failure reproduced
under `-j10` parallel contention, confirmed flaky/unrelated — passes
standalone).

## Recommendation

Not pausing for a fresh Markoff-side development push — nothing found in
two dogfood sessions points at canvas needing more engine work before
Corbomite continues. Corbomite's plan (Phase 4: flip the *dev-build*
default to canvas, still toggleable) is the natural next step whenever
the user wants to proceed; the 4 remaining gaps are deferrable polish,
not blockers. G2 can be considered answered.
