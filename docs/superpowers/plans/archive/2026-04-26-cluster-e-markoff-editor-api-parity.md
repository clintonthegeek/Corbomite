# Cluster E — Markoff Editor API parity (plugin shim)

> **🗄 RETIRED 2026-10-01 — do not dispatch. Zero user-facing work survives.**
>
> Every scope item was verified against the tree on 2026-10-01 and each one is
> either completed, obsoleted, or moot. The body below is kept verbatim as a
> historical record (see `docs/decisions-archive.md`, 2026-10-01).
>
> **Item-by-item disposition:**
>
> | # | Scope item | Disposition | Evidence |
> |---|---|---|---|
> | 1 | Document the Obsidian `Editor` API surface | **Done** — by the audit itself, not by this cluster | `docs/obsidian-audit/domains/editor.md`, `…/editor-markdown.md`, `…/PLUGIN-API-SKETCH.md` |
> | 2 | Wrapper methods in `Markoff::Editor` / `Markoff::Source::SourceEditor` | **Obsolete** — `Markoff::Editor` no longer exists; the source leaf is `Markoff::Source::Editor` | `Markoff::Editor` survives only as comment residue (`src/editor/MarkdownView.cpp:12`, `src/app/MainWindow.h:10`); the current leaf header is `libs/markoff-family/libs/markoff-source/include/markoff/source/Editor.h` |
> | 3 | Unify cursor coordinate base (Live + Source agree on 0/1-indexing) | **Completed, then obsoleted by a different design** — contract-v2 Phase 1 Task 8 shipped Ln/Col through the leaf-agnostic `Markoff::MarkdownView` base, so the leaves no longer need to agree with each other, they only need to agree with the base | `docs/superpowers/plans/2026-06-10-phase1-markoff-repin-contract-v2.md:792`; `src/editor/NoteEditorWidget.h:156` (`cursorInfoChanged(line, column, wordCount)`) → `src/app/MainWindow.cpp:2698-2702` |
> | 4 | Multi-cursor in Live via `Qutepart::extraCursors` | **Moot** — qutepart was deleted `da9a0a2c`; Corbomite's `SourceEditor` is a `QPlainTextEdit` shim whose own header says so, and multi-cursor is now Markoff-internal | `src/editor/SourceEditor.h:11-18`; no `extraCursors` symbol tree-wide; multi-cursor lives in `markoff-canvas` / `markoff-live` / `collabtext` |
> | 5 | `EditorExtension` type stub | **Shipped in a different shape** — Cluster B (closed 2026-04-28) landed `Corbomite::DecorationProvider` + `Plugin::registerEditorExtension`; the "full EditorExtension ABI" this item wanted is the thing that was *never written* | `libs/core/include/corbomite/core/Decoration.h:32-41`; `libs/vault/include/corbomite/vault/Plugin.h:145-158` |
> | 6 | Paste-from-HTML pipeline (Turndown analogue) | **Shipped** — HTML→markdown conversion is `Markoff::ClipboardCodec` in markoff-core; Corbomite's side landed as Cluster N (2026-08-20), which ported 8 clipboard actions onto `MarkdownViewActions` | `libs/markoff-family/libs/markoff-core/include/markoff/core/ClipboardCodec.h`; `docs/superpowers/plans/2026-08-20-cluster-n-rich-clipboard.md`. No `insertFromMimeData` remains in Corbomite's own code |
> | 7 | Suspected bugs: `absoluteCursorPos()` O(N), CJK desync at `TextControl.cpp:1683`, parallel completion-trigger paths | **Moot** — the cited file does not exist anywhere in the tree | `find . -name 'TextControl*'` (excluding `packaging/`, `build-*`) returns zero hits |
> | 8 | Coordinate with Clusters G and H | **Dead sequencing** — G and H were obsoleted by the 2026-05-25 foundation rewrite and archived 2026-06-10 | `docs/superpowers/plans/INDEX.md` §"Obsoleted by the foundation rewrite" |
>
> **Residue worth keeping (one line):** if/when plugin-author pressure
> arrives, re-derive the `Editor` surface against D2 + the `MarkdownView`
> contract-v2 — **not** against this body. Contract-v2's design principle is
> explicit that plugin-facing editor surfaces stay deferred
> (`2026-06-10-phase1-markoff-repin-contract-v2.md:24`: *"Plugin APIs that
> reach into the text widget stay blocked/deferred. This is accepted; do not
> design plugin-facing editor surfaces in this phase."*), so any future
> Cluster E is a **new** brainstorm against the current contract, not an
> expansion of this stub. Two known honesty bugs a future pass would have to
> route around: `Plugin::registerEditorExtension` is registered but its
> registry is never consulted by the render path (`Plugin.h:150-155`), and
> `Plugin::registerEmbed` advertises an embed system with **zero** registered
> `EmbedFactory`s (punch-list §C, `[plugin][P2]`).

> **⚠ 2026-06-10:** Body below targets the RETIRED pre-port editor (qutepart, extraCursors, line/column API). Re-scope against the D2 block model + MarkdownView contract v2 before any dispatch. Sequencing note re Cluster B is dead (B closed 2026-04-28).

> **Created 2026-04-26 from audit reset.** Stub plan; needs brainstorm + full plan expansion before dispatch. Markoff exposes a structurally-cleaner-than-CodeMirror API but is missing many of the Obsidian `Editor` methods that plugins assume. This cluster builds the shim.

## Goal

A Corbomite plugin written against the Obsidian `Editor` interface — `getCursor`, `setCursor`, `getLine`, `setLine`, `getRange`, `replaceRange`, `getSelection`, `setSelections`, `replaceSelection`, `posAtCoords`, `posAtMouse`, `coordsAtPos`, `lineCount`, `firstLine`, `lastLine`, `listSelections` — works against the live Markoff editor in both Source and Live modes. Multi-cursor works in both modes. The `EditorExtension` type exists for `Cluster B` to expose via `registerEditorExtension`.

## Audit references

- [audit-2026-04-26/editor.md](../../../audit-2026-04-26/editor.md) §"Markoff API gaps" — full enumeration
- [audit-2026-04-26/editor.md](../../../audit-2026-04-26/editor.md) §"Notable concerns / suspected bugs" — Live `cursorColumn()` 1-based vs Source 0-based; CJK autocorrect cursor desync; absoluteCursorPos O(N)

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
