# Cluster J — Embed / rendering primitives (STUB)

> **Living-status note:** This file is the *plan* (stub). Live status (Stub plan / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file. Edit this file only when the plan itself changes (or when expanded to a full plan — at which point rename to drop `-STUB` suffix and update INDEX).

**Plan written:** 2026-04-14 (stub). Expand to full plan after Cluster E (Markoff three-mode pivot) lands the section-recycle + async-parse infrastructure this cluster builds on, and Cluster I (MetadataCache) exposes the heading/block/section ranges needed for `![[Note#heading]]` and `![[Note#^block]]` embeds.

**Covers:** P2.12 (file-embed `![[Note]]` rendering), P2.22 (`RenderContext` inline-primitive renderer — synthesis-flagged as the shared `Markoff::LinkRenderer` extraction target), P2.23 (`aJ` EmbedRegistry), P3.9 (post-processor + code-block-processor registries — both live in `editor/markdown/MarkdownPreviewRenderer` per `rendering.md` correction).

## Goal

Build the embed-rendering pipeline + the post-processor / code-block-processor registries that make Markoff render arbitrary embedded content (notes, headings, blocks, images, PDFs, custom plugin types). Includes the `RenderContext` extraction the Pass 3 synthesis identified — `RenderContext` is currently used by both editor and non-editor surfaces (Bases, hover-popover) and should be a shared `Markoff::LinkRenderer`. Also lands the registry surface plugins extend to add post-processors and custom code-block languages.

## Audit references

- **File-embed DOM/class chain:** `domains/editor-markdown.md §1` — `markdown-embed`/`inline-embed`/`markdown-embed-title`/`markdown-embed-content` class chain. Per-embed `MarkdownPreviewView` mini-renderer. Subpath via `resolveSubpath + NT(...)`.
- **Infinite-recursion guard:** `domains/editor-markdown.md §1` — counts `.internal-embed` ancestors via `JZ(containerEl)`, passed as `sJ.load({depth})`. Concrete depth-counter algorithm.
- **`RenderContext`:** `domains/rendering.md §1` — provides ambient renderer primitives (`renderFileLink`, `renderTagLink`, `renderExternalLink`, bidi isolation, etc.). Used by both editor and non-editor (Bases) surfaces.
- **`renderFileLink` hardcoded source:** `domains/rendering.md §11` — emits `hover-link` with `source: "bases"` regardless of caller; Page-Preview must register that source. Brittle but load-bearing.
- **Post-processor registry:** `domains/rendering.md §1` (correction noted: lives in `editor/markdown/MarkdownPreviewRenderer`, not `rendering/`) + `02-extension-surfaces.md` — `registerMarkdownPostProcessor(fn, sortOrder)`. Plugins walk + mutate the rendered DOM. Priority-sorted, stable.
- **Code-block-processor registry:** same — `registerMarkdownCodeBlockProcessor(lang, fn)`. Plugins claim a fenced-code language. Called with `(source, el, ctx)`.
- **`EmbedRegistry` (`aJ`):** `domains/leaf-utilities.md §15` resolved this short symbol — defined in views stratum (likely `views/ViewRegistry` neighbourhood). Plugins register custom embed types (e.g. `.drawio` → diagram embed).
- **Embed → preview-pipeline integration:** rides on Cluster E's `PreviewPipeline` (depth-counter respects async-parse + recycle + virtual-scroll boundaries).

## Target classes

| Class | File | Notes |
|---|---|---|
| `Markoff::LinkRenderer` | `libs/markoff/src/LinkRenderer.{h,cpp}` | Extracted shared renderer for inline link/tag/external-link primitives. Replaces ad-hoc `RenderContext` calls scattered across Markoff |
| `Markoff::EmbedRenderer` | `libs/markoff/src/EmbedRenderer.{h,cpp}` | Per-embed mini-renderer; takes target path + subpath, produces a sub-`PreviewPipeline` mounted as a child item |
| `Markoff::EmbedDepthGuard` | `libs/markoff/src/EmbedDepthGuard.{h,cpp}` | Counts `.internal-embed` ancestor depth; refuses render past N (Obsidian default appears to be 4 — confirm against source) |
| `Corbomite::EmbedRegistry` | `libs/storage/src/EmbedRegistry.{h,cpp}` (or `libs/core/`) | `register(extensionOrType, factoryFn) → EmbedHandle`. Backs `![[file.ext]]` dispatch. Plugin-extensible |
| `Markoff::PostProcessorRegistry` | `libs/markoff/src/PostProcessorRegistry.{h,cpp}` | `register(fn, sortOrder) → Handle`. Iterated in sort order over rendered DOM (or rendered scene-graph in Markoff terms) |
| `Markoff::CodeBlockProcessorRegistry` | `libs/markoff/src/CodeBlockProcessorRegistry.{h,cpp}` | `register(language, fn) → Handle`. Per-language dispatch on fenced code blocks; fallback to default Prism-like pass-through |
| `Markoff::MarkdownRenderChild` | `libs/markoff/include/markoff/MarkdownRenderChild.h` | Lifecycle-tied widget subtree (cite `domains/editor-markdown.md §10`); auto-unloads when its containing section is recycled |

## Sub-tasks (when expanded)

1. **Extract `LinkRenderer`** — audit current Markoff link-emission paths, consolidate into `Markoff::LinkRenderer`. Verify the `hover-link` source-string convention (`"bases"` vs the hardcoded value Obsidian ships) — Corbomite chooses a more honest convention or matches Obsidian for plugin-compat.
2. **`EmbedRegistry`** — typed dispatch on file extension and on `![[...|alias]]` shape. Built-in registrations: `.md` → markdown-embed, image extensions → image-embed (Cluster G ViewRegistry covers them), `.pdf` → PDF-embed (P4.4 deps).
3. **`EmbedRenderer` integration with PreviewPipeline** — embedding a note creates a sub-pipeline. Inherits the parent's section-budget but consumes from a depth-decremented allowance. Test: a note that embeds itself stops at depth 4 (or whatever Obsidian's actual cap turns out to be) with a graceful "(embed depth exceeded)" placeholder.
4. **`EmbedDepthGuard`** — extract the depth-counter algorithm from `editor-markdown.md §1`. Implement as a context-passed integer (not DOM ancestor walk — Markoff has no DOM).
5. **`PostProcessorRegistry`** — sortOrder-priority registry. Stable sort (insertion-order ties). Each post-processor takes `(rendered scene-graph node, RenderContext)` and may mutate. Async post-processors return a future; pipeline awaits before declaring section "rendered."
6. **`CodeBlockProcessorRegistry`** — per-language dispatch. Default fallback through `KSyntaxHighlighting`. Built-in claim: `mermaid` (already via `libs/mmdr/`), `math` (via `libs/jkqtmathtext/`).
7. **`MarkdownRenderChild`** — lifecycle base (extends `Corbomite::Component` from Cluster C). Returned from post-processors / code-block processors; recycle pool calls `unload` when the section is recycled.
8. **Plugin extension surface** — `Plugin::registerMarkdownPostProcessor`, `Plugin::registerMarkdownCodeBlockProcessor`, `Plugin::registerEmbedType` route to these registries (cite `PLUGIN-API-SKETCH.md §5`).

## Explore prompts

> *(One required — KDevelop async-render patterns.)*

**Prompt 1 — KDevelop async-render patterns:**
> Read KDevelop's documentation tooltip / code-completion async-render code at `~/src/kde/src/kdevelop/kdevplatform/language/codecompletion/` and `~/src/kde/src/kdevelop/kdevplatform/language/duchain/navigation/`. Do NOT clone from upstream — local source is current. Identify: (a) how KDevelop stages a multi-stage render where each stage may produce a partial result, (b) how widget lifecycle is tied to the surrounding view's state, (c) precedent for an "embed depth" or recursion-guard pattern in any nested-render path. Report a translation plan for `Markoff::EmbedRenderer` + `EmbedDepthGuard`. Under 600 words.

**Prompt 2 — Embed depth confirmation:**
> The audit at `docs/obsidian-audit/domains/editor-markdown.md §1` documents an "infinite-recursion guard via JZ(containerEl) counting .internal-embed ancestors passed as sJ.load({depth})". Grep `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/` for the actual depth limit constant (look for `JZ`, `depth`, `internal-embed`). Report the exact integer used and the behaviour at boundary. Under 300 words.

## Definition of done

- `Markoff::LinkRenderer` is the sole inline-link emitter in Markoff; ad-hoc paths consolidated.
- `Markoff::EmbedRenderer` + `EmbedDepthGuard` render `![[Note]]`, `![[Note#heading]]`, `![[Note#^block]]` with the documented DOM/class structure.
- Self-embed test stops at the Obsidian-documented depth with a clear placeholder.
- `PostProcessorRegistry` and `CodeBlockProcessorRegistry` operate, sort-stable, async-aware.
- Built-in code-block processors registered: mermaid (existing `libs/mmdr/`), math (existing `libs/jkqtmathtext/`), default (KSyntaxHighlighting).
- `MarkdownRenderChild` lifecycle ties cleanup to section recycle.
- `EmbedRegistry` is extension-pointed for the future plugin system.

## Blocks / enables

- **Depends on:** Cluster A (`LinkUtils` for subpath parsing), Cluster C (`Component` for MarkdownRenderChild lifecycle), Cluster E (`PreviewPipeline` + section recycling for embed integration), Cluster I (`MetadataCache.headings`/`.blocks` for subpath resolution).
- **Blocks:** Cluster H (hover-link preview is essentially an embed-of-one-section in a popover), Cluster K (Bases uses RenderContext for cell rendering), Cluster L (Properties panel renders embedded property values).
- **Enables:** rich preview / reading mode parity with Obsidian; plugin-system rendering surface.
- **Estimated effort:** 3–4 weeks. Phase 5 (PostProcessorRegistry + sort-stable async ordering) is the most subtle sub-project; the rest are mechanical given the audit specs.

## Notes on expansion

When expanding to full plan:
- Read `libs/markoff/src/ReadingView.cpp` and `libs/markoff/src/Editor.cpp` to inventory current link-emission paths before designing the `LinkRenderer` extraction.
- Confirm whether Markoff's existing rendering already has any embed support (Pass 2 said it was "unconfirmed") — if there's a partial implementation, this cluster extends it rather than building from scratch.
- The `hover-link source: "bases"` quirk in `RenderContext.renderFileLink` deserves explicit handling: Corbomite either matches the Obsidian-shipped string (compat) or uses a clean per-caller source (correctness). Document the trade-off in the full plan.
