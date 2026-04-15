# Cluster J — Embed / rendering primitives (design spec)

**Date:** 2026-04-15
**Status:** Brainstormed, approved. Next: `writing-plans` skill produces the implementation plan.
**Supersedes:** [`2026-04-14-cluster-j-embed-rendering-primitives-STUB.md`](../plans/2026-04-14-cluster-j-embed-rendering-primitives-STUB.md) (to be renamed to full plan once `writing-plans` emits the plan).

---

## Purpose

Land Obsidian-parity rendering for embedded notes, heading-scoped embeds, and block-scoped embeds; consolidate inline-link emission in both `libs/markoff/` and `libs/readingview/` onto first-class `LinkRenderer` classes with honest per-caller `hover-link` source strings; introduce two internal registries (post-processor, code-block-processor) that existing built-ins (Mermaid, math, syntax-highlighting) route through without behavioural change; and deliver a user-visible win by swapping HoverPopover's preview renderer to produce section-granularity, rich-content previews.

This cluster is *not* a plugin-API cluster. All registries are internal Corbomite surfaces. Cluster N will later bridge a stable plugin ABI onto these same registries once the plugin-runtime decision (QJSEngine vs dlopen vs QuickJS) has been made in its own spike.

## Scope boundaries

**In scope:**
- Shared interfaces in `libs/core/`: `VaultResourceProvider`, `MarkdownRenderChild`, `EmbedRegistry`, `EmbedDepthGuard`.
- Internal registries in `libs/core/`: `PostProcessorRegistry`, `CodeBlockProcessorRegistry`. Sync-only API.
- `Markoff::LinkRenderer` consolidates current ad-hoc link emission across `libs/markoff/`.
- `ReadingView::LinkRenderer` + `ReadingView::EmbedRenderer` for the reading-mode pipeline.
- Built-in registrations: Mermaid + math + syntax-highlighting onto `CodeBlockProcessorRegistry`; markdown + image (via wikilink-shim) + stub-placeholder `.pdf`/audio/video onto `EmbedRegistry`.
- HoverPopover's internal renderer swaps to `EmbedRenderer`; cursor-position anchoring unchanged.

**Out of scope (ship-or-flag residuals or deferred to other clusters):**
- Stable plugin ABI for the registries → Cluster N.
- Real PDF renderer (Poppler-Qt6 vs Okular KPart spike) → future feature-cleanup task; pairs with Cluster G's KPart spike.
- Real audio/video renderers (QMediaPlayer) → post-parity.
- Markoff API extensions: `hoveredLinkRect()`, `Editor::setCursor(line, col)`, fractional-scroll precision → Markoff polish follow-up list.
- Search-side consumers of AST ranges (`line:`/`block:`/`section:`/`task*:`) → Cluster D follow-up #1.
- Compat-alias from `"bases"` hover-link source → Cluster N shim concern.

## Architecture

**Organising principle:** Split by concern. Interfaces, registries, and lifecycle types are output-target-agnostic and live in `libs/core/`. The two render-specific consumers (Markoff live-preview / Reading mode) each keep their own thin `LinkRenderer` shim implementing the shared interfaces, because Markoff's inline-edit decoration pipeline and ReadingView's section-mount pipeline are structurally different and don't benefit from a single forced-through abstraction.

Future Markoff-family consolidation is permitted but explicitly out of J's scope.

### Library layout changes

| Target | Library | Purpose |
|---|---|---|
| `VaultResourceProvider` (abstract) | `libs/core/` | Shared interface for vault file lookup, image byte-loading, wikilink resolution. Promoted from `libs/readingview/` (Cluster E flagged this promotion in its retro §7). |
| `MarkdownRenderChild` | `libs/core/` | Lifecycle-tied widget / scene-node subtree. Extends `Corbomite::Component` from Cluster C. Auto-unloads when its owning section is recycled by ReadingView's `SectionRecyclePool`. |
| `EmbedRegistry` | `libs/core/` | `register(extensionOrType, factoryFn) → EmbedHandle`. Dispatch table for `![[target]]` by file extension. |
| `EmbedDepthGuard` | `libs/core/` | Context-passed integer; rejects render past the Obsidian-documented `JZ` cap (value confirmed by Explore Prompt 2); produces a standard placeholder. |
| `PostProcessorRegistry` | `libs/core/` | `register(priority, fn) → Handle`. Stable-sorted by priority (insertion-order ties). Sync API. |
| `CodeBlockProcessorRegistry` | `libs/core/` | `register(language, fn) → Handle`. Per-language dispatch on fenced code blocks. Sync API. |
| `Markoff::LinkRenderer` | `libs/markoff/` | Sole inline-link emitter in Markoff. Replaces scattered ad-hoc paths. Emits `hover-link` with honest per-caller source strings. |
| `ReadingView::LinkRenderer` | `libs/readingview/` | Sibling implementation for the section-mount pipeline. Same interface contract as Markoff's. |
| `ReadingView::EmbedRenderer` | `libs/readingview/` | Per-embed mini-renderer; resolves subpath via `MetadataCache.headings`/`MetadataCache.blocks`; mounts as sub-scene-graph child of the owning `ReadingSection`. Respects `EmbedDepthGuard`. |

HoverPopover (`src/ui/HoverPopover.*`) is rewired internally; its public surface is unchanged.

### Data flow (ReadingView case)

```
ReadingPipeline::rebuild(rawMarkdown)
  → section-splitter emits ReadingSection[]
    → each section: SpanRenderer processes inline spans
      → inline wikilink `[[...]]` hits ReadingView::LinkRenderer
      → inline embed `![[...]]` hits ReadingView::EmbedRenderer
        → EmbedRenderer consults EmbedRegistry by extension
        → markdown embeds: recurse with depth+1 (EmbedDepthGuard)
          → at depth == JZ_CAP: placeholder
        → image embeds: wikilink-shim delegates to SpanRenderer image path
        → .pdf / .mp3 / .mp4: placeholder card via stub factory
      → fenced code block hits CodeBlockProcessorRegistry
        → per-language lookup: mermaid / math / default syntax-highlighting
      → post-layout: PostProcessorRegistry iterates in priority order
```

### Data flow (Markoff live-preview case)

```
Markoff::Editor render cycle
  → inline decoration layer
    → wikilink `[[...]]` hits Markoff::LinkRenderer
    → embed `![[...]]` currently deferred (Markoff has no section-mount pipeline;
      LivePreview embeds render as link-form per Cluster E's scope cut)
  → RenderContext-equivalent passes honest source string to hover-link emission
```

### HoverPopover rewire

```
Before: HoverPopover::showPreview(targetPath, subpath)
  → QTextBrowser::setMarkdown(rawMarkdown)  [crude fallback, no math/mermaid]

After:  HoverPopover::showPreview(targetPath, subpath)
  → ReadingView::EmbedRenderer::renderInto(popoverContentWidget, targetPath, subpath)
    → single-section mount via standard embed path
    → math, mermaid, wiki-links, images all Just Work in previews
  [anchor is still QCursor::pos(); rect-based anchoring is deferred follow-up]
```

## Decisions (from brainstorming)

Each locked via explicit user approval:

1. **Scope discipline — hybrid.** Internal registries land now; stable plugin ABI deferred to N. Built-ins (Mermaid, math, syntax-highlighting) register themselves via the internal API as a proof-out, but the API is not declared stable.

2. **Library placement — split by concern.** Interfaces + registries + lifecycle in `libs/core/`; each of Markoff and ReadingView implements its own thin renderer shim. Two implementations of `LinkRenderer` is explicitly acceptable given the pipelines are structurally different; future Markoff-family consolidation is allowed but out of J's scope.

3. **Embed depth — match Obsidian exactly.** Render a "(embed depth exceeded)"-shaped placeholder at the Obsidian-documented `JZ` cap. Explore Prompt 2 confirms the integer. User-configurable cap and degrade-to-link behaviour are both deferred.

4. **PDF embed — stub placeholder, defer renderer.** `EmbedRegistry` registers `.pdf` to a first-class factory that produces a labelled "PDF preview not yet available" card. Validates the registry dispatch without biting off Poppler/Okular. Real renderer lives outside J.

5. **Image/audio/video — asymmetric.** Images reuse ReadingView's existing `![](path)` SpanRenderer path via a wikilink-to-markdown-shim in the image-factory. Audio + video register stub placeholder cards like PDF. Symmetric with the PDF decision; avoids blocking on G's ViewRegistry.

6. **`hover-link` source string — honest per-caller.** `LinkRenderer` takes a `source` parameter from its caller. Obsidian's hardcoded `"bases"` quirk is *not* preserved; documented in an audit addendum so a future Cluster N `obsidian` shim can bridge it if a ported plugin hardcodes the string.

7. **HoverPopover absorption — API swap now, anchor UX deferred.** J replaces HoverPopover's internal `QTextBrowser::setMarkdown()` with `EmbedRenderer`. The popover still anchors on `QCursor::pos()`; `hoveredLinkRect()` rect-anchoring is named as a Markoff-API follow-up.

8. **Registry API — fully sync.** Post-processor and code-block-processor signatures return `void`. Async work follows ReadingView's established pattern (processor mutates scenegraph with placeholder, kicks off async work, updates node via signal when ready). Design choice will have a prominent `// WHY:` block at the registry class header documenting the choice and marking it for re-evaluation after real-world use.

## Phase breakdown

Six phases. Phases 3 and 4 are parallel-dispatchable (disjoint libraries — same pattern as Cluster E Phase 0).

**Phase 1 — Interfaces in `libs/core/`**
- `VaultResourceProvider` (abstract; promoted from `libs/readingview/`).
- `MarkdownRenderChild` (inherits `Component`).
- `EmbedRegistry` (registrar + dispatch; no factories yet).
- `EmbedDepthGuard` (context-passed int + placeholder struct).
- Unit tests per class.
- Existing `libs/readingview/` `VaultResourceProvider` becomes a re-export / typedef of the `libs/core/` one; ReadingView's adapter unchanged.

**Phase 2 — Internal registries in `libs/core/`**
- `PostProcessorRegistry` with stable priority sort (std::stable_sort over registered handles).
- `CodeBlockProcessorRegistry` with per-language std::unordered_map + fallback.
- `// WHY:` header comment on each class documenting sync-placeholder-plus-async-update pattern.
- Registry-reentrancy + unregister-during-iterate tests.
- No built-ins registered yet.

**Phase 3 (parallel with Phase 4) — `Markoff::LinkRenderer`**
- Audit and consolidate all current ad-hoc link-emission paths in `libs/markoff/src/`.
- Single `LinkRenderer` class emits `hover-link` with per-caller honest source strings.
- Delete or port-to-LinkRenderer every other inline-link emission site.
- Regression-test all Markoff link-click + hover paths.

**Phase 4 (parallel with Phase 3) — `ReadingView::LinkRenderer` + `EmbedRenderer`**
- `LinkRenderer` in `libs/readingview/src/` implementing the same contract as Markoff's.
- `EmbedRenderer` with `renderInto(parent, targetPath, subpath, depth)` API.
- Subpath resolution: delegates to `MetadataCache.headings` + `MetadataCache.blocks` (Cluster I).
- Self-embed test hits `JZ_CAP` and mounts the placeholder; no infinite loop.
- Heading-embed + block-embed tests.

**Phase 5 — Built-in registrations + media stubs**
- Refactor ReadingView's Mermaid / math / syntax-highlighting call-sites to register onto `CodeBlockProcessorRegistry`; dispatcher replaces direct dispatch.
- `EmbedRegistry` built-ins: `.md` → markdown-embed, `.png`/`.jpg`/`.gif`/`.svg`/`.webp` → image via wikilink-to-markdown-shim, `.pdf`/`.mp3`/`.wav`/`.mp4`/`.webm` → placeholder-card factory.
- Before/after behaviour tests confirm zero regression in the rendering of math/mermaid/images/syntax-highlighted code.
- Placeholder-card visual test for `.pdf`/audio/video.

**Phase 6 — HoverPopover renderer swap**
- Replace `QTextBrowser::setMarkdown()` with `EmbedRenderer::renderInto()` in `src/ui/HoverPopover.cpp`.
- Preserve `QCursor::pos()` anchoring.
- Test: hover a wikilink whose target has math / mermaid / images / nested wikilinks; all render in the popover.
- Test: existing non-rich hover previews still render (regression gate).

## Definition of done

### Must-pass hard gates

1. `![[Note]]` renders the full note body in ReadingView with correct class-chain equivalent structure.
2. `![[Note#heading]]` renders only the heading's section body (via `MetadataCache.headings`).
3. `![[Note#^block]]` renders only the block span (via `MetadataCache.blocks`).
4. Self-embed terminates at Obsidian's `JZ` cap with a named placeholder; no infinite loop under any embed topology.
5. Mermaid, math, syntax-highlighting all reach ReadingView via `CodeBlockProcessorRegistry`; visible behaviour unchanged.
6. `hover-link` events emit per-caller honest source strings everywhere; no `"bases"` hardcode anywhere in the landed code.
7. HoverPopover previews render via `EmbedRenderer`; math / mermaid / wikilinks / images all work in hover previews.
8. `VaultResourceProvider` is a single `libs/core/` interface; ReadingView and Markoff both consume it; app-layer adapter forwards as before.
9. `Markoff::LinkRenderer` is the sole inline-link emitter in `libs/markoff/`; all ad-hoc paths removed or routed through it.
10. No regressions in Cluster E's 15 test executables or Cluster I's 6 test executables.

### Ship-or-flag residuals (visible, non-blocking)

- `.pdf` / `.mp3` / `.mp4` embeds render a first-class "[file-type] preview not yet available" card with the filename; not a crash, not a silent skip.
- Hover previews anchor on `QCursor::pos()`; rect-based anchoring is a named follow-up.
- Search operators (`line:` / `block:` / `section:` / `task*:`) remain unbuilt; J's section/block/heading resolution in `MetadataCache` is what they'd consume when a future cluster lands them.
- Registries are internal; plugin ABI is explicitly not committed.

### Named follow-ups (to be collected in the plan's §Residuals)

- `Markoff::Editor::hoveredLinkRect()` — enables rect-anchored hover previews.
- `Markoff::Editor::setCursor(line, col)` — pre-existing Cluster E / F follow-up; gives column-granular cursor sets.
- Fractional-scroll precision in Source mode — fork Phase 4 territory.
- Real PDF renderer (Okular KPart vs Poppler-Qt6 spike). Pairs with Cluster G's KPart spike.
- Real audio/video embed renderers (QMediaPlayer).
- Search-side AST consumers (Cluster D follow-up #1).
- Stable plugin ABI for `PostProcessorRegistry` + `CodeBlockProcessorRegistry` (Cluster N).
- Compat-alias from `"bases"` hover-link source for plugin-compat (Cluster N shim).

## Audit references (for the implementation plan)

- `domains/rendering.md §1, §11` — RenderContext primitives; `renderFileLink` hardcoded `"bases"` source quirk.
- `domains/editor-markdown.md §1, §10` — file-embed class chain (`markdown-embed` / `markdown-embed-title` / `markdown-embed-content`); infinite-recursion guard `JZ`; MarkdownRenderChild lifecycle.
- `domains/leaf-utilities.md §15`, `SHARED-SYMBOLS.md` — `aJ` (EmbedRegistry), `sJ` (embed-render-child factory), `JZ` (embed-depth guard).
- `02-extension-surfaces.md` — `registerMarkdownPostProcessor(fn, sortOrder)`, `registerMarkdownCodeBlockProcessor(lang, fn)` signatures.
- `addenda/2026-04-15-markoff-footnote-def-offset-shift.md` — flag: offset arithmetic in AST consumers (relevant to subpath resolution).
- `cluster-retros/cluster-e.md §downstream-effects` — "hover-link preview at section granularity becomes practical now that ReadingView can render an arbitrary section without the full note" — J closes this.
- `cluster-retros/cluster-e.md §residual-follow-ups item 7` — "Shared `Corbomite::Core::VaultResourceProvider`" — J lands this promotion.

## KDE prior-art references

- `~/src/kde/src/kdevelop/kdevplatform/language/codecompletion/` — async-render + widget-lifecycle patterns for popup-hosted rich content.
- `~/src/kde/src/kdevelop/kdevplatform/language/duchain/navigation/` — documentation-tooltip multi-stage rendering and recursion-guard precedents.
- `~/src/kde/src/kwidgetsaddons/src/kmessagewidget.*` — pattern for dismissible content cards (useful for placeholder embed cards).

## Explore dispatches (required when plan is written)

**Prompt 1 — KDevelop async-render patterns** (as stubbed originally; carried forward verbatim to the plan). Target: `Markoff::EmbedRenderer` + `EmbedDepthGuard` translation plan.

**Prompt 2 — Embed depth confirmation** (as stubbed). Target: exact integer of `JZ` cap + boundary behaviour from Obsidian source.

**Prompt 3 (new) — Current Markoff link-emission inventory.** Before Phase 3 dispatches: walk `libs/markoff/src/` and enumerate every current path that emits an inline link, a wikilink, a tag link, an external link, or a `hover-link` signal. Report: list of call-sites; current API shapes; which are used where; which look ad-hoc vs deliberate. Output feeds Phase 3's consolidation.

## Estimated effort

2 weeks / single multi-phase session (reference: Cluster E's 18-commit single-session landing). Phases 3 and 4 in parallel; Phase 5 is mechanical; Phase 6 is a focused swap with good test anchors. Phase 1 + 2 are small pure-library phases.

## Downstream unblocks

- **Cluster H follow-ups** — rest of HoverPopover UX (rect-anchoring) becomes a Markoff-API task once J ships the renderer swap.
- **Cluster K (Bases)** — Bases uses a RenderContext-equivalent for cell rendering; J's `LinkRenderer` + `EmbedRenderer` are the substrate.
- **Cluster L extensions** — rendering embedded property values (when a property stores a wikilink or image) falls out of J.
- **Cluster N** — bridges the internal registries to a plugin ABI; J's API shapes inform N's shim design.
- **Cluster D follow-up #1** — search-side operators get section/block/heading resolution via `MetadataCache`; the render side is J's concern and now ships.
