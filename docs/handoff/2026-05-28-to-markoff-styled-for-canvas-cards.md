# Handoff → Markoff devs: `markoff-styled` as Corbomite's canvas-card renderer

**From:** Corbomite (downstream consumer)
**Date:** 2026-05-28
**Corbomite branch:** `master`
**Markoff pin:** current (post `v0.7.0-freeze`)
**About:** `libs/markoff-styled/` v0 (the QWidget-based light renderer you dogfooded 2026-05-27)

In response to: nothing pending — this is a *proactive* steer from us. No reply needed unless you have concerns; we're naming what we'd build on top of v0 so your v0.1/v0.2 priorities can factor it in (or explicitly defer it back to us).

---

## TL;DR

We completed a full reverse-engineering audit of Obsidian's Canvas feature
([`docs/obsidian-audit/domains/canvas.md`](../obsidian-audit/domains/canvas.md))
and the single biggest finding is that Obsidian canvas cards are *real
`MarkdownPreviewView` instances* — every card gets transclusions, math,
mermaid, callouts, interactive checkboxes, and plugin post-processors **for
free** because it's the DOM. A Qt-native canvas pays for each of those
explicitly.

The cheapest answer that doesn't drag in `QWebEngineView` is to use
`markoff-styled` as the per-card renderer. Your v0 surface already gives us
five things we previously had no good answer for: dual edit/preview, source-
path context, scroll save/restore, click+hover via `LinkService`, and a small-
enough widget to embed via `QGraphicsProxyWidget`. The hash-gated `StyleApplier`
is exactly the right shape for the per-frame churn a canvas with many cards
will impose.

There are three additions to `markoff-styled` that would *unblock* the
canvas-card use, and a handful more that would shrink the fidelity gap further.
We can do them downstream as Corbomite-side glue if you'd rather not bake them
into `markoff-styled`, but they feel general enough that we think they belong
upstream. Below is the proposal, with concrete API sketches and a Tier-1/2/3
priority so you can pick freely.

---

## Context: why `markoff-styled`, not `markoff-live`

For canvas cards we want widget-based, no QML, no maximalist render. The card
needs to be small, paint-cheap (you may have hundreds on screen), embeddable in
a QGraphicsView scene without imposing a QML stack, and able to be torn down
and re-mounted as the user pans and zooms. `markoff-styled`'s charter — "byte-
exact source with overlay styling, light enough to dogfood for normal note
editing" — matches the canvas-card profile better than `markoff-live` does. We
do *not* want to fight the maximalist live-render delegates inside a 250×60
node.

We've already noted (and respect) the v0 non-goals: no images, no math, no
mermaid, no transclusions, no tables, no callouts, no plugin post-processors.
The ask below is not "do all of these now." It's "open the right contract
shapes so Corbomite can fill them in without modifying `markoff-styled`."

---

## Tier 1 — three additions that unblock the canvas-card use

These are the ones we think actually need to live in `markoff-styled`, because
they touch internals. If any of them are no-gos from your side, tell us and
we'll redesign downstream.

### T1-1. Expose the renderer pair (document + applier) for `QGraphicsItem`-native paint

**The problem.** `QGraphicsProxyWidget`-per-card scales badly. Hundreds of
proxies hurt; non-integer scale blurs (canvas zooms continuously); proxy input
handling has known quirks. The natural model for canvas:

- *Focused card* (being edited): real `Editor` widget via `QGraphicsProxyWidget`.
- *Unfocused cards*: same underlying `QTextDocument`, painted directly into a
  `QGraphicsItem::paint` via `QAbstractTextDocumentLayout::draw()`.

For that to work, Corbomite needs to drive `StyleApplier` against a
`QTextDocument` *without* an `Editor` host. v0 currently couples them — the
applier reaches into `QTextEdit` for scroll capture (`StyleApplier::setTextEdit`,
`src/StyleApplier.h:?`).

**What we'd love.** Expose the renderer as a usable pair:

```cpp
// public header
namespace Markoff::Styled {
class DocumentRenderer {  // formerly StyleApplier, extracted
public:
    void setTheme(const Theme *);
    void setFontScale(qreal);
    void setMarkoffDocument(MarkoffDocument *);
    void setTextDocument(QTextDocument *);  // any QTextDocument, not just an Editor's
    void rerender();
    quint64 hashSkips() const;
};
}
```

The existing `Editor` keeps composing `DocumentRenderer` internally (zero
behavior change for current consumers); the scroll-capture machinery moves to
an `EditorScrollPreserver` that lives on `Editor` only.

Corbomite then builds a `MarkoffTextCardItem : QGraphicsObject` that owns its
own `QTextDocument` + `DocumentRenderer` and paints via
`document()->documentLayout()->draw(painter, ctx)`. On focus, swap to a real
`Editor` proxy widget at the same coords; on blur, swap back. The doc is the
same instance — no reparse, no relayout.

**If this is too invasive,** an acceptable alternative is making
`StyleApplier` public (move it to `include/markoff/styled/`) with the
`setTextEdit` call optional + no-op when null. We can then drive it ourselves.

### T1-2. `idealHeight(qreal width)` for auto-fit-to-content cards

**The problem.** Obsidian's double-click-bottom/right-resizer auto-fit reads
`scrollHeight` after layout. We need the same. `QTextDocument` exposes this if
you've called `setTextWidth(w)`; we just need a clean entry point.

**What we'd love.** On `Editor`, and on the extracted `DocumentRenderer`
(T1-1):

```cpp
qreal Editor::idealHeight(qreal width) const;
qreal DocumentRenderer::idealHeight(qreal width) const;  // wraps QTextDocument::size() after setTextWidth
```

This is a tiny addition but it's the difference between cards that grow with
content vs. cards that scrollbar-clip.

### T1-3. Embed/object-replacement extension hook

**The problem.** The single biggest content gap is `![[…]]` embeds — file
transclusions, images, eventually PDF/media. Obsidian routes these through
`embedRegistry` (`app.pretty.js:153937`). The Qt-native fit is
`QTextDocument::QTextObjectInterface` + `QChar::ObjectReplacementCharacter`.

**What we'd love.** A registration surface:

```cpp
namespace Markoff::Styled {

struct EmbedRequest {
    QString kind;           // "image" | "note" | "pdf" | "audio" | "video" | "math-block" | ...
    QString target;         // link text (image path, note name, etc.)
    QString subpath;        // optional "#heading" / "#^block"
    qreal   maxWidth;       // available width hint
    QString fromContext;    // the contextPath from Editor::setFromContext
};

class EmbedRenderer {
public:
    virtual ~EmbedRenderer();
    virtual QSizeF intrinsicSize(const EmbedRequest &) = 0;
    virtual void   paint(QPainter *, const QRectF &, const EmbedRequest &) = 0;
    virtual int    flags() const { return 0; }  // future: live | static | needs-input
};

class Editor /* and DocumentRenderer */ {
    void registerEmbedRenderer(const QString &kind, EmbedRenderer *);  // "image","note","math-block",…
};
}
```

`StyleApplier` detects embed spans (which the parser already gives you via the
inline-span / image-node shapes you've stabilized), writes a
`ObjectReplacementCharacter` at that source position with a custom
`QTextFormat::ObjectType`, and dispatches via the registered renderer when
laid out.

This *one* hook unlocks everything downstream wants: Corbomite registers an
image renderer (raster via `QImageReader` + cache), a note renderer
(instantiates *another* `markoff-styled` `Editor`/`DocumentRenderer` for
transclusion with a depth-guard ≤5 — matches Obsidian's behavior at
`app.pretty.js:153940`), a math renderer (we own JKQTMathText), a mermaid
renderer (we own `libs/mmdr`), and a missing-target placeholder. None of
those payloads need to live in `markoff-styled`.

**Block-level embeds** (math `$$…$$`, mermaid fences, callout bodies, tables)
are a natural extension: an embed `kind` whose flag says "owns the whole
block." `StyleApplier` skips its normal block formatting for those.

---

## Tier 2 — independently useful additions, lower urgency

We can live without these initially; landing them shrinks the visible fidelity
gap card-by-card.

### T2-4. Inline image rendering (`![](path.png)`, `![[image.png]]`)

The narrow case of T1-3. The cheapest big win because dragged-into-canvas
files are most commonly images. If T1-3 lands you can implement T2-4 as a
built-in default `EmbedRenderer` for `kind="image"` shipped with
`markoff-styled` (using `QTextDocument::addResource(ImageResource, ...)`
internally is fine).

### T2-5. Lightweight / suspended mode

Obsidian's strategy at low zoom is *content unmount* below a `zoomBreakpoint`
(`app.pretty.js:192577`) — the card outline + label survive but the renderer
detaches. Corbomite's canvas-side equivalent is: at low zoom, don't run
`StyleApplier`; paint a cached `QPicture` or the first N characters in a tiny
font; rebind on zoom-in.

For that we'd want:

```cpp
void StyleApplier::setSuspended(bool);  // skip restyle while true; resume re-renders on toggle-off
QPicture DocumentRenderer::snapshotPicture(qreal width) const;  // optional convenience for placeholder painting
```

`setSuspended` is the minimal addition; `snapshotPicture` is nice-to-have.

### T2-6. Callout visual treatment (`> [!type] Title\n> body`)

Worth doing in `markoff-styled` because every markdown view wants callouts and
they're the single most visible "this is not Obsidian" cue. Implementation is
`QTextFrameFormat` + a custom block painter inside `StyleApplier`. We're happy
to contribute a PR here if you want.

### T2-7. Stable per-instance fold/scroll-state key

Obsidian persists folds per `canvasFile#^nodeId`. Canvas cards have stable
node ids. The contract we'd want:

```cpp
void Editor::setPersistenceKey(const QString &key);
QByteArray Editor::saveState() const;
void       Editor::restoreState(const QByteArray &);
```

Canvas serializes the `QByteArray` into per-vault local view state (NOT the
`.canvas` file — that's ephemeral by Obsidian convention, see
`domains/canvas.md` §2).

---

## Tier 3 — fine to defer / Corbomite-side

These don't need anything from you:

- **Tables.** Render as styled monospace text for now. We'll write the real
  `QTextTable` renderer downstream when we get to it (or as a `kind="table"`
  embed renderer once T1-3 is in).
- **Interactive checkboxes.** Once T1-1 (paint-mode) is in, the canvas scene
  can hit-test the checkbox region on unfocused cards and route the toggle
  back into the source. Focused-proxy cards get the real CheckBox interaction
  via the proxy.
- **Math/mermaid renderers themselves.** Corbomite ships JKQTMathText and
  `libs/mmdr`; they're our problem to drive once T1-3 is in.
- **Plugin markdown post-processors** (Dataview-style). Not part of
  `markoff-styled`'s charter; if/when we want this it's a Corbomite registry
  that runs after `markoff-styled` renders, not an upstream feature.

---

## What's already great in v0 (so this doesn't read like a wishlist of
complaints)

- The `Editor` / `MarkdownView` contract gives canvas dual edit/preview *for
  free* via `setReadOnly`. We don't have to invent a separate reading widget.
  This is the single biggest win.
- `setFromContext(QString)` is exactly what canvas cards need to resolve
  `![[…]]` relative to the host `.canvas` file.
- The `LinkService` indirection is clean — Corbomite passes a service that
  resolves to its vault, no global hooks needed.
- Scroll preservation on in-place edits (`StyleApplier::captureScrollBefore­Edit`
  + `QTimer::singleShot(0)` restore) is already the right behavior for
  cards-that-blur-and-refocus.
- The hash-gated restyle (`computeBlockHash` bit-pack) means we won't pay
  full-doc reformat cost on per-keystroke edits across hundreds of off-screen
  cards. Perfect.
- The "delimiters stay visible, formatting overlays" decision means edit ↔
  preview swap can be a `setReadOnly` flip with no remount. That's *better*
  than Obsidian — they pay a real CodeMirror↔preview remount on every focus.
- The deferred `Cmd::changeKind` via `QTimer::singleShot(0)` to avoid re-entry
  is exactly the right shape. We'll need the same pattern for embed inserts.

---

## What we're decidedly not asking for

- We're not asking `markoff-styled` to grow QML, plugin sandboxes, or a math
  engine. The contracts above keep all that downstream.
- We're not asking for "Obsidian parity inside `markoff-styled`." Cards that
  show 90% of common card content (text, headings, lists, code, links,
  images, transclusions of other notes, math, mermaid) at canvas-realistic
  perf are the bar. Tables / interactive task lists / community-plugin
  post-processors can be missing from a v1 canvas without anyone noticing.

---

## Concrete asks, prioritized

If you can do anything, the highest-leverage starting point is:

1. **T1-1** (extract `DocumentRenderer` from `Editor`'s lifecycle) —
   architectural; gates the rest.
2. **T1-3** (embed-renderer registration hook) — unlocks ~5 features in one
   contract.
3. **T1-2** (`idealHeight(width)`) — trivial; lets us auto-fit cards.

The rest can land at your pace, or we can carry them downstream and upstream
later.

If T1-1 specifically is too invasive right now, an interim move that unblocks
us is **making `StyleApplier` a public header** with the `setTextEdit` call
optional — we'll drive it manually against our own `QTextDocument` until the
clean extraction is feasible.

We'll wait to start the canvas-side integration until you've had a chance to
read this and react. If you'd rather we maintain a downstream patch set
against `markoff-styled` for the canvas-specific bits and *not* land any of
this upstream, that's a fine answer too — say so and we'll plan accordingly.

---

## Where this fits the canvas plan

The canvas domain audit (`docs/obsidian-audit/domains/canvas.md` §11.3) named
"per-card render strategy" as the project-defining decision that dwarfs the
plumbing-vs-Graffodil question. Tier-1 items 1, 2, and 3 above are what turn
that decision from "embed `QWebEngineView` per card (heavy, big dep)" into
"reuse `markoff-styled` (light, native, already shipping)." We'd like to take
that deal. Tell us how you'd like to play it.

— Corbomite agent, 2026-05-28
