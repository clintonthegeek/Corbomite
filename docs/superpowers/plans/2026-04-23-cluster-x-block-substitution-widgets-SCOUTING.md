# Cluster X — Block-substitution widget promotion (SCOUTING)

**Date:** 2026-04-23
**Status:** Scouting — expansion trigger documented below
**Depends on:** Markoff Phase C work-unit C8 ("Inline-ORC canonical coherence") landing + regression tests green
**Enabled by:** `ImageBlockItem` pattern already in place in Markoff Live

## Goal

Promote block-level substitutions (currently rendered as `ObjectReplacementCharacter` glyphs inside a `MarkdownTextItem`'s `QTextDocument`) out of the text document into peer `QGraphicsItem`s, modelled on `ImageBlockItem`. This:

1. Fulfils the design intent — these blocks are meant to be interactive widgets (pan, zoom, edit-in-place, error panes, export-as-image, etc.), which is impossible while they are character-level glyphs inside the text engine.
2. Reduces the inline-ORC translation surface that C8 establishes to only *inline* substitutions (inline math `$…$`, inline checkbox `[ ]`), which can never become widgets because they participate in line flow and baseline alignment.

## In-scope substitution types

| Type | Current representation | Target |
|---|---|---|
| Block display math `$$…$$` | ORC in text block | `DisplayMathBlockItem : BlockItem` — renders via `JKQTMathText`, supports click-to-edit (popover with full LaTeX editor), copy rendered PNG, right-click menu |
| Mermaid fence ` ```mermaid ` | ORC in text block | `MermaidBlockItem : BlockItem` — renders via `mmdr`, supports pan/zoom, error pane when parse fails, copy rendered SVG, right-click menu |

## Out of scope

- **Inline math / inline checkbox**: stay ORC, covered permanently by C8's translator. Line-flow participation means they cannot easily be peer `QGraphicsItem`s without breaking Qt's text layout.
- **Tables**: currently `QTextTable` frames inside text items, not ORC substitutions. Tables have their own local/canonical coherence concerns (row/cell edits, splits) — captured separately in backlog. A future cluster may promote tables to their own widget, but not this one.
- **Other inline renderers** (rendered tag chips, wikilink badges, embed previews) — none are currently ORC-substituted; if/when they are added, they follow the inline-ORC invariant from C8.

## Prior art

- `libs/markoff-family/libs/markoff-live/src/ImageBlockItem.h/.cpp` — the canonical block-item pattern. Peer to `MarkdownTextItem` in the scene, has its own `BlockItem::toMarkdown()` for round-trip, participates in `SceneCoordinator::m_items` and `m_itemMap`.
- `MarkdownSplitter::split()` already segments the source into `{Text, Image}` segments — Cluster X adds `DisplayMath` and `Mermaid` segment kinds.
- `BlockItem` / `StubBlockItem` base class already supports the widget lifecycle.

## Architectural questions (answered during expansion, not now)

- Does `DisplayMathBlockItem` render inline or open a popover on click? Obsidian-parity points to inline-rendered with click-to-edit-LaTeX-in-popup; needs brainstorming.
- Mermaid error state: render an error pane with the LaTeX source visible, or collapse to a placeholder block?
- Does block math gain a "math number" label (figure-style), or stay unnumbered?
- What's the interaction model when block math/mermaid is inside a blockquote or callout? (They might still need to nest inside a text item's structure.)
- Paste behaviour: when a user pastes a markdown string with `$$…$$` or mermaid fences, does `MarkdownSplitter::split` run on the paste and split it into new block items, or do they land as raw text inside a text item until the next full-reparse?

## Deliverables (expected on expansion)

- Design doc at `docs/superpowers/specs/YYYY-MM-DD-cluster-x-block-substitution-widgets-design.md`.
- Implementation plan at `docs/superpowers/plans/YYYY-MM-DD-cluster-x-block-substitution-widgets.md`.
- New `MarkdownSegment` kinds + splitter support.
- Two new block-item classes + their tests.
- Migration of the existing ORC-based rendering pipelines out of `MarkdownTextItem` for these types (the inline-ORC translator loses these as transitional-scope cases).
- Regression tests covering: round-trip (markdown → split → block item → `toMarkdown` → identical source), interactive gestures, paste behaviour.
- Cluster retro at `docs/cluster-retros/cluster-x.md`.

## Expansion trigger

Cluster X is brainstormed and expanded to a full plan when **all** of the following hold:

1. Markoff Phase C work-unit C8 has landed on `master`.
2. C8 regression tests are green (inline-ORC canonical coherence verified).
3. Phase C is on a path to close (no other cross-repo breaking changes pending that would thrash the Live scene model).

Expansion involves:

- Re-read this scouting doc + C8 design doc.
- Brainstorm the architectural questions above one at a time.
- Produce design doc and implementation plan.
- Move this scouting doc to `archive/` with a closeout pointer.

## Cross-links

- Bug report + hypothesis confirmation: dogfood session trace 2026-04-23 — decisions-archive entry of same date.
- C8 design doc: `../../../libs/markoff-family/docs/specs/2026-04-23-inline-orc-canonical-coherence.md`.
- Phase C3 spec (sets the canonical-bridge foundation Cluster X builds on): `../../../libs/markoff-family/docs/specs/2026-04-20-phase-c3-markoff-document-content-authoritative.md`.
- `ImageBlockItem` pattern reference: `../../../libs/markoff-family/libs/markoff-live/src/ImageBlockItem.{h,cpp}`.
