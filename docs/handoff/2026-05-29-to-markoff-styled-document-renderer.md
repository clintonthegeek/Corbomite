# Handoff → Markoff devs: a headless `Markoff::Styled::DocumentRenderer`

**From:** Corbomite (downstream consumer)
**Date:** 2026-05-29
**Corbomite branch:** `master`
**Markoff pin (current):** `dc86ca7` (`v0.7.0-freeze-14`) — **stale; we will re-pin to a master commit
that contains the renderer below** (see "Re-pin coordination").
**Refreshes:** [`2026-05-28-to-markoff-styled-for-canvas-cards.md`](2026-05-28-to-markoff-styled-for-canvas-cards.md)
— same idea, now that `markoff-styled` is much further along than when we first asked.

## TL;DR

We want to render read-only markdown in three Corbomite surfaces — **canvas cards**,
**hover-link popovers**, and a new **reading-mode leaf** — by reusing `markoff-styled`'s rendering,
*without* hosting a `QTextEdit`. Please factor a headless **`Markoff::Styled::DocumentRenderer`** out
of `StyleApplier` that can (T1) populate a caller-owned `QTextDocument` from a `MarkoffDocument`, and
(T2) `paint()` into a `QPainter`/report `idealHeight(width)` for a `QGraphicsItem`. Read-only, no
cursor, no editing. This is a **rendering ask only** — explicitly *not* a request to change the E-arc
constitution or retire QML (see "What we're NOT asking for").

## What's already great

`markoff-styled` is exactly the QWidget leaf we hoped for: it subclasses `Markoff::MarkdownView`,
carries no QML/KF6, honors `setReadOnly`, and already renders the 7 block kinds + all 9 inline span
types we need for card/reading content. The `StyleApplier`/`DocHighlighter` content-vs-cursor split is
clean. The reason we can't consume it yet is purely packaging: the formatting pass is welded to the
`QTextEdit`, and our two non-widget surfaces (a `QGraphicsItem` canvas card and a detached preview doc)
can't host one.

## The ask — `Markoff::Styled::DocumentRenderer`

Factor the content-formatting pass (`StyleApplier`'s `iterateBlocks()` → `blockText`/`blockKind`/
`inlineSpansFor`/`blockAttrs` → `QTextCursor` block+char formats, roughly
`~/dev/Markoff/libs/markoff-styled/src/StyleApplier.cpp:553-595` and `charFormatForSpan` `:183-216`)
into a class that targets a `QTextDocument` it does **not** own a widget for. Sketch (names/shape your
call):

```cpp
namespace Markoff::Styled {

class DocumentRenderer {
public:
    explicit DocumentRenderer(const Markoff::Theme &theme);
    void setTheme(const Markoff::Theme &);

    // T1 — populate a caller-owned document (reading mode, hover popover, detached doc).
    void renderInto(QTextDocument *target, const MarkoffDocument *doc) const;
    // convenience for non-MarkoffDocument callers (hover snippet from raw bytes):
    void renderInto(QTextDocument *target, const QByteArray &markdownUtf8) const;

    // T2 — paint surface for QGraphicsItem cards (no widget, no scroll area).
    qreal idealHeight(const MarkoffDocument *doc, qreal width) const;
    void  paint(QPainter *p, const QRectF &rect, const MarkoffDocument *doc) const;
};

} // namespace Markoff::Styled
```

- **Read-only, stateless-per-call** (or cheaply reusable): no cursor authority, no `d2DocumentChanged`
  subscription, no edit routing. This is the subtractive "replace the live highlighter with a one-pass
  static render" path your own framing doc already names
  (`Markoff:docs/specs/2026-05-08-e-arc-framing.md:318`).
- T2 can be implemented over T1 (`QTextDocument::documentLayout()->draw()` +
  `documentLayout()->documentSize()`); if that's how you do it, great — we just need the two entry
  points so we don't reach into a widget.

### Tiering / priority

- **T1 first** unblocks reading mode + hover popover immediately on our side.
- **T2** unblocks canvas cards. Both in one go is ideal but T1-then-T2 is fine.

### Graceful degradation (no new rendering work requested)

Tables, math, images, callouts, mermaid, embeds are **out of scope here** — render them as their
source text or a typed placeholder, exactly as styled does today. We are **not** asking you to advance
styled's block coverage for this; cards/reading will simply show those as text until styled grows them
on its own schedule.

## Falsifiable acceptance

1. `renderInto(doc, markoffDoc)` on a document containing all 7 supported block kinds yields a
   non-empty `QTextDocument` whose block/char formats match what the styled `Editor` produces for the
   same input (a golden compare against the widget path).
2. `idealHeight(doc, w)` is monotonic-ish and `paint()` into a `QRectF` of that height clips nothing
   for the supported kinds.
3. An unsupported block (e.g. a table) renders as source text / placeholder — never empty, never a
   crash.
4. No QML, no `QQuickWidget`, no top-level widget is constructed anywhere in the path.

## What we're NOT asking for

- **Not** asking you to retire QML, demote the live view, or amend the E-arc framing. The
  QML-superset constitution stands; this is a consumer rendering convenience, fully inside your
  "subset/static-render" model.
- **Not** asking for editing, cursor, selection, delimiter-hide, or find in this renderer.
- **Not** asking for new block-kind coverage (tables/math/images/embeds).
- **Not** asking for a QGraphicsScene integration on your side — just the `paint`/`idealHeight` entry
  points; the `QGraphicsItem` lives in Corbomite.

If any of this is better carried downstream in Corbomite instead (e.g. you'd rather expose `StyleApplier`
as a public header with an optional detached-doc mode and let us drive it), say so and we'll plan
accordingly — same offer as last time.

## Reading-mode decision change (retires a prior ask)

We are adopting a **read-only `Markoff::Styled::Editor` leaf** as Corbomite's reading mode. This
**retires our earlier steer** (`2026-05-25-to-markoff-reading-steer-and-merge-confirm.md`) that asked
for a `Capabilities::Editable=false` read-only mode on the **Live** view. You can **drop that pending
ask** — reading mode no longer depends on QML, and styled already honors `setReadOnly` as a top-level
widget. (If you'd already started `Capabilities::Editable`, it's still useful for a future
read-only-Live, just no longer on Corbomite's critical path.)

## Re-pin coordination

Our submodule is 79 commits behind your master and predates `markoff-styled`. We'll advance the pin to
a master commit that includes `DocumentRenderer` (this also completes the re-pin + port-branch merge you
flagged as waiting on us). If the renderer lands behind a tag, point us at it. Happy to pair on any
freeze-→-master API drift we hit during the port-compat pass — and that drift is exactly the "real port
pressure" you wanted to drive the `markoff-core` freeze spec, so expect a follow-up surfacing the API
surface we actually lean on.
