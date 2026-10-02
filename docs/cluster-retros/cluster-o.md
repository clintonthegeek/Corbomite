# Cluster O retro — context-sensitive menu/toolbar/sidebar

**Closed 2026-10-02**, after the 2026-08-20 plan and a second-sweep audit that
re-derived its remaining work. Plan:
[`../superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md`](../superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md).

## What the cluster was

One idea, stated in the audit: of ~79 `KAction`s in Corbomite, **zero** belonged
to canvas, bases or graph; 17 were permanently disabled stubs; 7 silently
no-opped off-markdown. Every control existed — as a `QToolButton` with its
behaviour trapped in a `clicked` lambda — but was unreachable from the menubar,
the command palette, the Hotkeys page, or the keyboard.

The mechanism was a tiered `ViewActions` provider per view type, with three
tiers of state: **A** presence (is this type installed at all), **B**
capability (is it enabled *right now*), **C** checked state (what is it set
to).

## What actually shipped

| Phase | Delivered |
|---|---|
| O0–O4 | Tier-A/B/C plumbing, `MarkdownViewActions`, `CanvasViewActions` |
| O5.T1/T2 | `BasesViewActions` — 7 toolbar controls promoted |
| O5.T3/T4 | `GraphViewActions` — 5 verbs **and** the plugin-registration path |
| O5.T5 | Cluster I tail: `tab_move_left`/`right`, `leaf_link_with_active_pane` |
| O6.T3 | `GraphControlsPanel` per-instance fix |
| O7.T1/T2/T4/T5 | 3 new real verbs, blockers on all 14 stubs, placebo dialogs disabled, palette regrouped |
| O7.T3 | folded into the existing Markoff handoff |
| O6.T1/T2/T4, O7.T6 | **not done** — see below |

## The three findings worth keeping

**1. The plugin-registration path was the real content of O5.T4, and it is a
lifetime problem, not an API problem.** A provider's vtable lives in the plugin
`.so`. Registering one is easy; *un*registering it before the library unloads
is the whole task. `KXMLGUIFactory` would otherwise hold a client whose code no
longer exists. Three safeguards: `onUnload()` hands it back explicitly (the one
hook that provably runs first), the registrar destructor is only the
abnormal-teardown fallback, and `QPointer` runs end to end so an abrupt
teardown nulls rather than dangles.

**2. O6.T3's falsifiability check settled the shape of the fix.** The punch list
said "don't `disconnect()` over it; the structural fix is O6's relocation" —
correct instinct, and the check proved why. Reinstating the shared singleton
made the test binary **segfault** rather than fail an assertion, because the
defect was shared *ownership* (a widget parented to one tab, referenced by
another), not missing disconnects. A `disconnect()` patch would have left the
crash in place.

**3. Two upstream sources were wrong in ways that mattered.** The audit's
`workspace.md:123` says `setGroupMember` reuses the *target's* group id; taken
literally that **orphans the source's existing members** — a test caught it
doing exactly that. And `EditorContext` turned out to be the only route to the
current heading level, which is why `heading_increase`/`decrease` were
"cheap-looking" but needed a cached context first.

## What I got wrong, and what it cost

The cluster ran six phases and the recurring failure was **claiming
verification I had not performed**. Recorded because the pattern generalises:

- A fix whose test passed against unfixed code (O5) — found by deliberately
  reverting the fix.
- A falsifiability check that came back **green against a deliberately
  reintroduced bug**, because with no cursor context "disabled" was correct
  either way. The test was worthless for the property it claimed to cover;
  that gap is now written into the test file rather than papered over.
- An over-strong invariant ("every disabled action carries a blocker") that
  went red on *correct* behaviour — 27 live actions are legitimately
  Tier-B-disabled with no editor bound. Removed.
- Three separate times I edited a file with a bulk regex that matched more than
  intended and had to revert. The lesson: on a file with 18 near-identical call
  sites, verify the *count* of edits, not just that it compiled.

## Not done, and why

- **O6.T1/T2/T4** (the `View`-level inspector slot) — recorded as a punch item.
  Nothing there closes a reported bug, and it is speculative UI surface that
  wants a live eyeball before it is trusted.
- **O7.T6** (sidebar raise-on-type) — deferred by the plan's own instruction;
  blocked on `[workspace][P2]` sidebar-layout persistence.
- **O7.T7's soak** — the live eyeball is handed over separately. The retro is
  written now and will be amended with what was actually observed.

## The lesson to carry forward

The value of this cluster was not the menus. It was that **promoting a control
to an action forces you to answer three questions you had been avoiding**: does
it know whether it can act (Tier B), does it know what state it is in (Tier C),
and — if it lives in a plugin — who unregisters it. Every real bug found in
O5/O6/O7 came from answering those questions, not from adding menus.