# Handoff → Markoff devs: green light to merge `exploration/new-foundation` → `master`

**From:** Corbomite (downstream consumer; submodule `libs/markoff-family`)
**Date:** 2026-05-25
**Corbomite branch:** `port/foundation-exploration`
**Markoff pin as of this writing:** `03f088a` (exploration/new-foundation tip)

---

## TL;DR

**You are clear to abandon the old four-leaf editor and merge `exploration/new-foundation`
into Markoff `master` whenever it suits you.** The one thing that was holding you
back — "Corbomite still compiles against the old code" — is no longer true.
Corbomite's `port/foundation-exploration` branch compiles, links, and runs against
the new foundation. Our `master` is now a frozen v0.6.x relic we are happy to leave
behind. We are not asking you to preserve the old leaves, the QGraphicsView scene,
`QTextDocument` substitution, or `ObjectReplacementCharacter` plumbing for our sake.

Please observe **merge order**: Markoff merges to its `master` first, *then* we merge
Corbomite. Reversed, Corbomite `master` would carry a submodule pin nobody can resolve.

---

## Where Corbomite stands

- We bumped our submodule pin to your current tip `03f088a` on 2026-05-25 — a clean
  79-commit fast-forward from the prior pin (`cb0f147`), no divergence.
- Corbomite builds clean against it (full dev preset, app + test targets link).
- The app launches against our starter vault and renders documents in Live mode with
  **zero QML errors / Qt warnings** in the log.
- The four port-blocking items from our first port session (2026-05-20) are all closed:
  Find UI wiring, the doc-sharing doubling bug, source-mode-empty, and the
  `MarkoffDocument::resetContent`/D2 workaround. Your E-arc status board reflects this
  from your side too.
- Recent Corbomite-side port commits: Find UI port complete, `heading_1..6` actions
  wired via `LiveActionController`, per-leaf source+live format dispatch.

Full Corbomite-side port status: [`docs/port-foundation-exploration.md`](../port-foundation-exploration.md).

## What we think of the new widget

Candidly: it's the right call, and it's further along than we expected.

- The **block-delegate architecture is correct.** Moving every block to a real QML
  delegate (TableDelegate, CodeBlock, MathDelegate, …) over a D2/CollabText model
  eliminates entire bug classes we had been fighting in the old substrate — see the
  "clusters we are retiring" section below. We are not mourning the QTextDocument scene.
- **E1–E4 are dogfood-usable for everyday editing** from where we sit: inline
  formatting, cursor-aware delimiter hide/show, cross-block keyboard nav, editing
  affordances, theme + zoom + dark toggle, wikilink/standard-link Ctrl+click
  navigation, and now graphical table editing with cell wrap + smart column widths.
  The tables in particular came up "quite usable" in our integration too.
- The **D2 document model + serialize-for-save path** is a clear improvement over the
  old canonical-buffer juggling. (We note you caught the 0-byte-save data-loss bug in
  `toMarkdownUtf8` during E4 — thank you; that fix came in with this pin bump.)

In short: from the consumer side, the new foundation is a better product and a better
codebase. Retiring the old one is not a loss for us.

## Corbomite roadmap items the rewrite obsoletes (for your awareness)

This is context, not a request — it explains *why* we're comfortable dropping the old
code. Three of our strategic clusters were built entirely on the old substrate and are
now moot because of how you rebuilt things:

- **Inline-ORC canonical coherence** (our Cluster G) — guarded against `U+FFFC`
  corruption inside `QTextDocument`-substituted glyphs. The new model has no such
  glyphs; the corruption class can't occur. Your E1 InlineHighlighter is the
  replacement. **Obsolete.**
- **Block-substitution widget promotion** (our Cluster H) — its entire goal was to
  promote math/mermaid *out of* `QTextDocument` *into* peer graphics items. That *is*
  your new baseline. **Obsolete — superseded by your E5.**
- **Qutepart-Corbomite Source-mode fork** (our Cluster J) — we were going to vendor and
  own a Source-mode widget. You now ship `Markoff::Source::Editor` (block-aware d2
  edits, format ops). We expect to retire this and consume yours. **Likely obsolete.**

We mention these only so you understand we have positively *re-planned around* the new
foundation — we are not merely tolerating it.

## What we still need from you (gating, not blocking)

These are the Markoff E-phases that gate currently-degraded Corbomite features. None of
them block your merge to master — they're the roadmap we're tracking against on our side:

| Corbomite degradation (port doc) | Gated on Markoff phase |
|---|---|
| Embeds non-functional (#7) | **E3** (wikilinks/embeds/tags/callouts) |
| Callouts not rendered | **E3** |
| Mermaid is a no-op (#6) | **E5** (math/Mermaid Live parity) |
| Math block parity | **E5** |
| Reading-mode features — HoverPopover (#8), checkbox-toggle, `setCursorLine` | Reading leaf retired; we need either a Reading restoration **or** a read-only Live (`Capabilities::Editable == false`) we can drive. A steer on which direction you intend would let us plan. |
| Word count not updated (#10) | Small add — `wordCount` on `MarkoffDocument` + a `wordCountChanged` signal equivalent. |
| Undo/redo (#11) | We'll wire to `MarkoffDocument::d2UndoLog` ourselves; flag if that's not the intended public path. |

The single most useful steer for our planning: **your intent for read-only/Reading
rendering** (restore a Reading view vs. a non-editable Live configuration). Several of
our frozen features hang off that one decision.

## Housekeeping on our side (no action needed from you)

- Three stale Corbomite experiment branches built on the old editor
  (`markoff-fold-v2`, `markoff-reading-split`, `markoff-source-split`) are being
  retired. Nothing in them is salvageable into the QML world.
- When you're ready to draft the `markoff-core` freeze spec, we'd like it to be driven
  by real port pressure rather than the speculative draft. We can contribute the
  consumer-side pressure points (the API surfaces our port actually leans on) whenever
  that's useful.

## Merge sequence (shared plan, restated)

1. You complete the E-arc to the point you're comfortable freezing the core API.
2. Freeze spec drafted from real port pressure (we'll feed in our consumer surface).
3. Tag Markoff (we've been assuming something like `v0.7.0-freeze`).
4. **Merge Markoff `exploration/new-foundation` → Markoff `master`.** ← you, first.
5. **Merge Corbomite `port/foundation-exploration` → Corbomite `master`**, repinning to
   the new Markoff master tip. ← us, second.

Steps 1–3 are not prerequisites for step 4 if you'd rather merge sooner — the only hard
ordering constraint is **4 before 5**. If you want to merge the foundation to master
ahead of the freeze, go ahead; we'll track your master tip on our port branch as we
already do.

---

*Questions or corrections welcome — this is our read of the situation as the downstream
consumer, written to unblock your merge decision.*
