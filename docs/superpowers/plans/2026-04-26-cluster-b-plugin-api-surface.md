# Cluster B — Plugin API surface completion

> **Created 2026-04-26 from audit reset.** Stub plan; needs brainstorm + full plan expansion before dispatch. The plugin host (Cluster Q/N legacy) shipped with 6 of the ~13 Obsidian registration verbs exposed to plugins. Six host-side registries have no plugin proxy; six more verbs are entirely missing. This cluster closes that gap.

## Goal

Every Obsidian registration verb that has a sensible KDE/Qt analogue can be reached from a third-party plugin via the Corbomite plugin facade. Permission tokens move into a public header. The `data.json` watcher exists.

## Audit references

- [audit-2026-04-26/plugin.md](../../audit-2026-04-26/plugin.md) §"Registration verb coverage matrix" — full enumeration
- [audit-2026-04-26/ui-bundle.md](../../audit-2026-04-26/ui-bundle.md) §"HoverPopover lifecycle"
- [audit-2026-04-26/editor.md](../../audit-2026-04-26/editor.md) §"EditorSuggest"
- [audit-2026-04-26/editor-markdown.md](../../audit-2026-04-26/editor-markdown.md) §"MarkdownRenderer.render"
- [audit-2026-04-26/rendering.md](../../audit-2026-04-26/rendering.md) §"RenderContext-equivalent"

## Scope (in scope)

**Missing plugin proxies for existing host registries (6):**
1. `registerHoverLinkSource` (proxy over `HoverLinkSourceRegistry`)
2. `registerEditorSuggest` (proxy over `EditorSuggestManager`)
3. `registerMarkdownPostProcessor` (proxy over `PostProcessorRegistry`)
4. `addRibbonIcon` (proxy over `RibbonToolBar`)
5. Embed registration (proxy over `EmbedRegistry`)
6. `registerMarkdownCodeBlockProcessor` (proxy over `CodeBlockProcessorRegistry`)

**Verbs entirely missing (6):**
7. `addStatusBarItem`
8. `registerObsidianProtocolHandler` (`obsidian://...` URLs)
9. `registerEditorExtension` (requires defining a `Markoff::EditorExtension` shim type — depends on Cluster E)
10. `MarkdownRenderer.render(app, md, el, sourcePath, component)` static API
11. Lucide icon registry (`addIcon` / `removeIcon`)
12. `data.json` external-edit watcher (`onExternalSettingsChange` lifecycle hook)

**Permission system polish:**
13. Move permission tokens from .cpp to a public header
14. Document the permission model in `docs/plugin-development/`

## Out of scope

- Markoff Editor wrapper API parity → **Cluster E** (`registerEditorExtension` is blocked on it)
- Internal plugin gap fill → **Cluster F**

## Phases

TBD — brainstorm before dispatch. Likely 5 phases grouped by surface area (rendering, editor, UI chrome, lifecycle, permissions).

## Status

**Plan-needed** (stub).
