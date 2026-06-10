# Cluster E — Markoff Editor API parity (plugin shim)

> **⚠ 2026-06-10:** Body below targets the RETIRED pre-port editor (qutepart, extraCursors, line/column API). Re-scope against the D2 block model + MarkdownView contract v2 before any dispatch. Sequencing note re Cluster B is dead (B closed 2026-04-28).

> **Created 2026-04-26 from audit reset.** Stub plan; needs brainstorm + full plan expansion before dispatch. Markoff exposes a structurally-cleaner-than-CodeMirror API but is missing many of the Obsidian `Editor` methods that plugins assume. This cluster builds the shim.

## Goal

A Corbomite plugin written against the Obsidian `Editor` interface — `getCursor`, `setCursor`, `getLine`, `setLine`, `getRange`, `replaceRange`, `getSelection`, `setSelections`, `replaceSelection`, `posAtCoords`, `posAtMouse`, `coordsAtPos`, `lineCount`, `firstLine`, `lastLine`, `listSelections` — works against the live Markoff editor in both Source and Live modes. Multi-cursor works in both modes. The `EditorExtension` type exists for `Cluster B` to expose via `registerEditorExtension`.

## Audit references

- [audit-2026-04-26/editor.md](../../audit-2026-04-26/editor.md) §"Markoff API gaps" — full enumeration
- [audit-2026-04-26/editor.md](../../audit-2026-04-26/editor.md) §"Notable concerns / suspected bugs" — Live `cursorColumn()` 1-based vs Source 0-based; CJK autocorrect cursor desync; absoluteCursorPos O(N)

## Scope (in scope)

1. Document the Obsidian `Editor` API surface
2. Implement missing wrapper methods in `Markoff::Editor` / `Markoff::Source::SourceEditor`
3. Unify cursor coordinate base (Live + Source must agree on 0/1-indexing)
4. Multi-cursor in Live mode (currently Source-only via Qutepart's `extraCursors`)
5. `EditorExtension` type stub (interface; concrete extensions are plugin-supplied)
6. Paste-from-HTML pipeline (Turndown analogue) — currently `insertFromMimeData` only honours `hasText()`
7. Address suspected bugs: `absoluteCursorPos()` O(N), CJK autocorrect cursor desync at `TextControl.cpp:1683`, two parallel completion-trigger paths
8. Coordinate with **Cluster G** (Markoff Phase C8) and **Cluster H** (block-substitution widgets) — those clusters touch the same surface; sequence carefully

## Out of scope

- Vim mode → defer
- Spell check → defer
- Triple-click line-extend → punch list
- Plugin-API `registerEditorExtension` exposure → **Cluster B** (depends on the shim landing here first)

## Phases

TBD — brainstorm. Likely 4 phases: (1) API audit + method coverage, (2) coordinate-base unification + multi-cursor parity, (3) paste pipeline, (4) `EditorExtension` shim type for Cluster B handoff.

## Status

**Plan-needed** (stub). Sequencing: should land before Cluster B's `registerEditorExtension` work.
