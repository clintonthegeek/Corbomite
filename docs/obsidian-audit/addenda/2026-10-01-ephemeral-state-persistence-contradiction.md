# `EphemeralState` persistence and `eState.scroll` semantics are contradictory in the corpus

**Date:** 2026-10-01
**Discovered during:** second-sweep remediation, fixing the punch-list item
"`eState.scroll` stores a 0.0–1.0 scroll fraction, not a visual line"
([`docs/punch-list.md`](../punch-list.md), re-ranked P4→P1).
**Supersedes / extends:** `domains/views.md` §`EphemeralState` (line 151, 159)
vs `domains/workspace.md` §2 leaf/history record shapes (lines 214, 220). **These
two contradict each other and neither is authoritative.**
**Relevant cluster plans:** `superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md`
(the Cluster L compat doctrine), `docs/punch-list.md` §Added 2026-10-01.

## Finding

Three statements in the corpus cannot all be true at once.

**1. `views.md:151` heads the type with "not persisted."**

> ### `EphemeralState` (not persisted; passed via `WorkspaceLeaf.setViewState(_, eState)`)

**2. `views.md:159` documents `scroll` as a line number, not a position fraction.**

> `scroll?: number;   // line for scroll-to-match`

**3. `workspace.md:214` and `:220` nevertheless embed `eState` in Obsidian's
serialised workspace records** — once per leaf record and once per navigation
history entry:

> `{ leafId; state: ViewState; eState; parentId?; rootId?; leafHistory: {...} }`
> `{ title; icon; state: ViewState; eState }`

If `EphemeralState` is genuinely never persisted, it cannot appear in a
serialised shape. If it *is* persisted inside those records, then its `scroll`
field is something Obsidian reads back — and statement 2 says that something is a
line number.

A fourth data point makes the contradiction concrete: `tests/storage/tst_ephemeral_state.cpp`
carried the comment *"Shape from `docs/obsidian-audit/domains/workspace.md` §3: a
leaf's `eState` for a markdown view carries scroll (float)"* — citing a section
**that does not exist**. `workspace.md` never documents `eState.scroll`'s type at
all. So the code and its tests had been built against a citation that was never
checked, and the test itself seeded `scroll = 128.42` — a value that is only
meaningful as a **line number**, not as a 0.0–1.0 fraction.

## Why noticed now

Chasing the P1 item "Corbomite writes a 0.0–1.0 fraction into an Obsidian-shared
`eState.scroll` that Obsidian reads as a line number" required settling what
Obsidian actually does with the field, and it turns out the corpus cannot say.

The item's own framing also needed correcting: it assumed `eState` is
Obsidian-shared and read back. That rests on statement 3, which statement 1
contradicts.

## Action taken

**Code: the conservative resolution.** `EphemeralState::toJson()` no longer writes
Corbomite's viewport fraction under Obsidian's `scroll` key. The fraction moved to
`corbomite.scrollFraction` inside the same `eState` object, and
`fromJson()` reads only that key. An Obsidian-authored `scroll` is left in
`extraKeys` and round-trips verbatim, and is deliberately **not** interpreted as a
fraction on load.

This is safe under **either** reading:

- If Obsidian does persist and read `eState.scroll` as a line number, we have
  stopped writing a value that would restore the note to line 0.
- If Obsidian never persists it at all, we have lost nothing — our own scroll
  restore still works, under our own key.

The alternative (converting fraction ↔ line) was rejected as unfounded: it needs
the viewport height, which is not available at serialize time and is not stable
across window sizes.

**Tests.** New `tests/storage/tst_ephemeral_scroll_interop.cpp` (8 cases) asserts
both directions, including that an Obsidian line-number value is not read as a
fraction. Falsifiability-checked: 4 of its cases go red against the old
behaviour. Existing assertions in `tst_ephemeral_state.cpp` and
`tests/e2e/tst_workspace_roundtrip_mode.cpp` were updated to the new contract,
and the bogus `workspace.md §3` citation is corrected in place.

## Note for future implementers

**Do not treat the Obsidian-side answer to this question as settled.** If someone
gets real evidence — an Obsidian-authored `workspace.json` containing a populated
`eState.scroll`, or decompiled source reading it back — that settles statement 1
vs 3, and the conservative resolution should then be revisited (it may become a
straight conversion, or may be confirmed exactly right). Until then, the
non-destructive choice is the correct one: never write a value into a field whose
meaning the reference does not establish, and round-trip whatever the other
application wrote.

This is also the second time this session that a **test comment citing an
audit-doc section turned out to be the only support for a design decision**, and
the citation did not check out. `views.md:159` is at least a real line — but its
own heading contradicts its body. Treat audit line-references as leads, not
authority.