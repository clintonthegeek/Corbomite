# Design — markoff-styled headless rendering → Corbomite card-view + reading mode

**Date:** 2026-05-29
**Status:** Design (awaiting user review → writing-plans)
**Track:** Cross-repo convergence (Corbomite consumer ↔ Markoff)
**Companion handoff:** [`docs/handoff/2026-05-29-to-markoff-styled-document-renderer.md`](../../handoff/2026-05-29-to-markoff-styled-document-renderer.md)

## Scope discipline

**Rendering only.** This design gives Corbomite real read-only markdown rendering for three
surfaces (canvas cards, HoverPopover, reading mode) by consuming a headless renderer
extracted from Markoff's QWidget `markoff-styled` leaf. It does **not**:

- touch Markoff's "QML-live is the maximalist superset" constitution
  (`Markoff:docs/specs/2026-05-08-e-arc-framing.md`);
- retire the QML live view or alter Corbomite's live-editing path;
- add delimiter-hiding, find, or any editing capability to styled;
- ask Markoff to render tables/math/images/callouts/mermaid/embeds yet.

**Parked, not dropped (recoverable future):** the owner's longer-term intent to make styled
the primary view and retire QML is deferred. Markoff has pre-authored the hook for it
(`Markoff:docs/specs/2026-05-26-markoff-styled-leaf-design.md:18` — styled "becomes the basis
for a QWidget live-render view if the project decides QtQuick deps are no longer desirable").
Revisit once styled proves editing + delimiter-hide + rich-block parity. This paragraph is the
breadcrumb.

## Background (verified against the live Markoff repo, not the stale pin)

- `Markoff::Styled::Editor` (live repo `~/dev/Markoff/libs/markoff-styled`) is a QWidget
  (`QTextEdit` + `QTextDocument`, **no QML, no KF6**) that **subclasses `Markoff::MarkdownView`**
  — the same polymorphic base as live and source. It is a drop-in leaf for Corbomite's
  `NoteEditorWidget::activeLeaf()` swap dispatch.
- Renders today: headings, paragraphs, code blocks, blockquotes, lists (continuous ordered
  numbering + task checkboxes), HR, and all 9 inline span kinds. Honors `setReadOnly`.
- Rendering engine is `StyleApplier` (a `QTextCursor` formatting pass over the document driven
  by `MarkoffDocument::d2DocumentChanged`), plus an inert `DocHighlighter` slot reserved for
  cursor-derived formatting (not used here).
- **Gap for Corbomite's use:** `StyleApplier` is welded to the `QTextEdit`. There is no headless
  path to (a) populate a caller-owned `QTextDocument` or (b) `paint()` into a `QGraphicsItem`.
- Corbomite's render slot is empty: `MarkoffRenderEngine::render` is a deprecated stub that
  `setPlainText`s raw markdown (`libs/core/src/MarkoffRenderEngine.cpp`); nothing in the app
  calls `setRenderEngine` (audit-2026-05-29 finding). `HoverPopover` and the canvas
  `FileCardItem` both want a renderer.

## The Markoff ask (full detail in the handoff)

A headless **`Markoff::Styled::DocumentRenderer`**, factored out of `StyleApplier`, read-only,
no cursor/editing:

- **T1 — QTextDocument population:** render a `MarkoffDocument` (or markdown bytes) into a
  caller-owned `QTextDocument`. Drives reading mode + HoverPopover + a detached doc.
- **T2 — paint surface:** `paint(QPainter*, const QRectF&, …)` + `idealHeight(qreal width)` so a
  `QGraphicsItem` (canvas card) can draw and auto-size without hosting a widget.

Unsupported block kinds (tables/math/images/embeds) degrade gracefully (rendered as their source
text or a placeholder), not error.

## Corbomite components

### 0. Prerequisite — advance the submodule pin (gating)

Corbomite's `libs/markoff-family` is pinned ~79 commits behind `~/dev/Markoff` master and
predates `markoff-styled` entirely. Everything below requires re-pinning to a Markoff commit that
contains the new `DocumentRenderer`. This is real work: an API-drift / port-compat build pass
across the freeze→master delta. It also discharges the re-pin + port-branch-merge that Markoff is
already waiting on Corbomite for. **Sequenced first.**

### 1. Reading mode — read-only styled leaf

Replace the no-op Reading stub in `NoteEditorWidget` (`readingView()` returns `nullptr`; Reading
falls back to a read-only Live that looks identical — audit "Reading-mode dead-end" fork) with a
**read-only `Markoff::Styled::Editor` leaf**:

- Construct a styled `Editor` as a third stack leaf; `setReadOnly(true)`.
- `activeLeaf()` returns it for `ViewMode::Reading`; existing `setDocument`/`MarkdownView` swap
  dispatch already supports it.
- Remove the dead `Markoff::Reading::ReadingView` forward-decl + stub.
- **Needs ~zero new Markoff work** (styled already honors read-only as a top-level widget).
- **Retires the open `Capabilities::Editable=false` read-only-Live steer** to Markoff — reading
  no longer depends on QML.

### 2. Canvas cards — wire the render slot

- Add a `StyledRenderEngine : MarkdownRenderEngine` in `libs/core` (or repurpose
  `MarkoffRenderEngine`) that wraps `Markoff::Styled::DocumentRenderer` and returns a
  `RenderedDocument` backed by the populated `QTextDocument` (T1), with `FileCardItem` painting via
  the paint/idealHeight surface (T2) where a `QGraphicsItem` is involved.
- Make the missing `setRenderEngine` call (`MainWindow` → `CanvasViewTab`/`CanvasFileView` →
  `CanvasScene`) and ensure `CanvasScene::setRenderEngine` re-renders already-built cards
  (audit finding: it currently only stores the pointer).

### 3. HoverPopover — rebuild on the renderer

Repoint `HoverPopover`'s render path from the retired `Markoff::Reading::ReadingView`/`EmbedRenderer`
to the headless renderer (T1 QTextDocument). Unblocks the frozen hover-preview P2.

## Data flow

```
MarkoffDocument ──► Markoff::Styled::DocumentRenderer ──┬─T1─► QTextDocument ─► RenderedDocument ─► reading leaf / HoverPopover
                    (factored from StyleApplier)        └─T2─► paint(QPainter)/idealHeight ─────► FileCardItem (QGraphicsItem)
```

## Error handling / graceful degradation

- **Unsupported blocks** (table/math/image/embed): renderer emits source text or a typed
  placeholder; never throws, never blanks the whole document.
- **Null renderer:** consumers keep today's safe fallback (text cards' regex fallback; popover
  stays empty) — no crash if the engine isn't wired.
- **Re-pin build breakage:** treated as a contained port-compat task in its own plan phase, gated
  before consumer wiring.

## Testing

- Reading leaf: read-only round-trip + document swap into the styled leaf
  (mirror `markoff-source` leaf-contract tests on the Corbomite side).
- `StyledRenderEngine`: a `MarkoffDocument` with the 7 supported block kinds → non-empty
  `RenderedDocument`; an unsupported block → graceful placeholder (falsifiable).
- Canvas: `CanvasScene::setRenderEngine` re-renders existing cards (the audit gap); a file card
  with a real engine renders non-empty content.
- HoverPopover: renders non-empty for a resolvable link target.

## Sequencing

1. Re-pin submodule + port-compat build pass (prerequisite).
2. Reading-mode read-only styled leaf (cheapest; ~zero Markoff dependency).
3. Markoff ships `DocumentRenderer` (T1 then T2) per the handoff.
4. Canvas cards + HoverPopover on the renderer.

## Deferred (tracked, not in this design)

QML retirement; styled-as-primary; delimiter-hide + find in styled; tables/math/images/embeds in
the renderer; promotion of live's machinery into markoff-core; repo merger; the markoff-core freeze
spec (Markoff is waiting on Corbomite to drive it from real port pressure — a candidate next steer
once this lands).

## Open cross-repo dependencies

- **On Markoff:** the `DocumentRenderer` (T1+T2). Handoff issued 2026-05-29.
- **From Corbomite (now unblocked to act):** submodule re-pin + port-branch merge; and the
  read-only-styled reading decision **retires** the prior read-only-Live (`Capabilities::Editable`)
  ask — communicated in the handoff.
