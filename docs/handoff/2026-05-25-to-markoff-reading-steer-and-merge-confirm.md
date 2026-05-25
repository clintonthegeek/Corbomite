# Handoff → Markoff devs: Corbomite merged; Reading-mode steer + gating-item confirms

**From:** Corbomite (downstream consumer)
**Date:** 2026-05-25
**Corbomite branch:** `master` (foundation port landed here)
**Markoff pin:** `v0.7.0-freeze` (`1e0f332`)

In response to: `libs/markoff-family/docs/handoff/2026-05-25-to-corbomite-merge-complete.md`

---

## TL;DR

Step 5 is done on our side. We re-pinned to `v0.7.0-freeze`, merged
`port/foundation-exploration` → Corbomite `master`, and retired the port
branch (it's reachable via the merge commit). Build + launch verified clean,
including your re-vendored `libs/jkqtmathtext` (thank you — no more
machine-local symlink). The cross-repo ordering held: you merged first, so our
master's submodule pin resolves everywhere.

Below: the Reading-mode steer you asked for, plus confirmation on the two
gating items you're tracking from us.

## Reading-mode direction: **read-only Live (`Capabilities::Editable=false`)**

We've decided **against** restoring a separate Reading leaf. Drive the existing
QML/D2 Live editor with editing disabled instead. Rationale:

- **One rendering pipeline.** A second purpose-built Reading renderer would
  drift against Live. The "live-render maximalist" philosophy of the new
  foundation argues for Reading = Live minus editing affordances.
- **The capability model already exists** in the E-arc — this leans on it
  rather than adding surface.
- **HoverPopover rewires onto Live delegates regardless.** Its old
  `Reading::ReadingView` renderer dependency is gone; pointing it at a
  read-only Live config is the rewire we'd do in either world, so this avoids
  doing it twice.

What we'd need from a read-only Live mode, for planning on your side:

1. A clean capability/flag that disables text mutation, caret-driven editing
   affordances, and IME — while keeping selection, link Ctrl+click, checkbox
   *click-to-toggle* (a read-mode interaction, not a text edit), and
   scroll/`setCursorLine`-style navigation.
2. A decision on **delimiter visibility** in read-only mode. Cursor-aware
   hide/show (E2) is an editing affordance; in read-only we'd expect markers
   fully hidden (rendered appearance), not cursor-dependent. Flag or mode
   constant is fine.
3. A renderer entry point HoverPopover can instantiate to render a snippet of
   markdown read-only into a popup widget (the old `ReadingView`-as-renderer
   role).

No rush — this is the steer, not a spec request. If it's easier to spec
collaboratively, we can co-author against your capability model.

## Gating-item confirmations

- **Word count (#10):** your proposed `wordCount` + `wordCountChanged` on
  `MarkoffDocument` is exactly the shape we want. We'll consume it from our
  status-bar word-count label. Cheap; whenever it fits your queue.
- **Undo/redo (#11):** confirmed **our side**. We'll wire to
  `MarkoffDocument::d2UndoLog`. Flag only if that's not the intended public
  undo entry point — we're assuming it is.

## Still tracking (no action requested)

Embeds (#7) + callouts → your **E3**; Mermaid (#6) + math parity → your **E5**.
We'll restore the Corbomite-side consumers as those phases land. E4 Phase H
(dogfood + `v0.7.0-e4` tag) is yours to close at your pace; doesn't gate us.

When you're ready to draft the `markoff-core` freeze spec, ping us — we'll
surface the consumer API surfaces our port actually leans on.

---

*Corbomite master is the foundation port now. Onward.*
