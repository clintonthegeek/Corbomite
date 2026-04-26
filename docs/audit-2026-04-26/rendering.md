# Rendering domain audit

Audit target: Obsidian's `obsidian/rendering` directory (11 files,
`RenderContext.js` + lazy-loader shims for MathJax / Mermaid / PDF.js / Prism +
Turndown / DOMPurify / Tooltip / search-result helpers).

Mapping target: Corbomite's `libs/markoff-family/libs/markoff-reading/`
(`ReadingView` class), `libs/markoff-family/libs/markoff-core/` (`MathRenderer`,
`MermaidRenderer` interface, `CodeBlockProcessorRegistry`, `EmbedRegistry`,
`LinkResolver`), `libs/mmdr/` (Rust mermaid FFI), `libs/jkqtmathtext/` (LaTeX
typesetter), and the host-app surfaces in `src/editor/HoverPopover.cpp` +
`src/plugins/search/`.

## Architecture fit (Qt paint + native libs vs HTML/CSS)

Corbomite has chosen the natural Qt translation: instead of a DOM tree under a
WebKit/Chromium engine, `ReadingView`
(`libs/markoff-family/libs/markoff-reading/include/markoff/reading/ReadingView.h:47`)
is a `Markoff::MarkdownView` that composes a `QGraphicsView` and uses
`SectionLayout` to materialise per-section `QGraphicsItemGroup` trees. Each
"section" plays the role of an Obsidian `MarkdownPreviewSection`: parsed from a
tree-sitter AST, virtualised through `VirtualScrollController`, recycled through
`SectionRecyclePool`, and frame-budget mounted via
`mountInitialWindowWithBudget` (see `ReadingView.cpp:543-624` for the budget
loop — `kFrameBudgetMs` / `kFrameBudgetSections`). This is functionally a
faithful translation of Obsidian's progressive section rendering — arguably
*better*, since it gets virtualisation and recycling for free in the Qt
graphics scene.

The translation principle "lazy JS engines → bundled libs / subprocess bridges"
is honoured but not uniformly: math is *statically linked* via `JKQTMathText`
(`libs/jkqtmathtext/`); mermaid is *statically linked* via the pre-built Rust
archive `libs/mmdr/libmermaid_rs_renderer.a`
(`libs/mmdr/CMakeLists.txt:7-12`) with a thin `Corbomite::Core::MermaidRenderer`
adapter (`libs/core/src/MermaidRenderer.cpp:12-28`) wrapping `mmdr_render_svg` /
`mmdr_free`. Both bypass Obsidian's `loadMathJax()` / `loadMermaid()` deferred
contracts: there is no `loadMathJax`-equivalent shim that returns a
ready-future, so when the plugin API ports plugins that `await loadMathJax()`,
the call site has to be rewritten or shimmed (flagged below in §Concerns).

The `RenderContext` class (Obsidian's ambient inline-primitive renderer for
links, tags, external links) has *no central equivalent*. The closest analogue
is `Markoff::Reading::LinkRenderer`
(`libs/markoff-family/libs/markoff-reading/src/LinkRenderer.cpp`) — but it is a
QObject that emits *signals*, not a renderer that materialises link DOM. Link
spans are emitted directly by `SpanRenderer` into per-section `QTextCharFormat`
runs (with `WikiLinkTargetProperty` carrying the target — see
`ReadingView.cpp:822`), and hover/click are dispatched via `eventFilter`
(`ReadingView.cpp:335-376`). This is the right Qt-native translation of "every
internal link must be clickable / hoverable / draggable" *for ReadingView's own
content*, but it is *not* the cross-surface ambient renderer that Bases /
suggesters / search-result rows / plugin-supplied HTML can also call. That's a
real architectural gap (see §Missing).

## Implemented (parity-equivalent)

- **Inline math `$...$` and display math `$$...$$`.** `SpanRenderer` detects
  both forms (`SpanRenderer.cpp:83-118`) and emits `MathObjectMarker` text
  formats with `InlineMathSourceProperty` carrying the LaTeX.
  `SectionLayout::layoutInline` then re-stamps the formats with
  `ReadingMathObject::TypeId` (`SectionLayout.cpp:415-443`) and the registered
  `ReadingMathObject` text-object handler paints the math image at draw time.
  `Markoff::MathRenderer::render(latex, displayMode, fontSize, dpr)`
  (`libs/markoff-family/libs/markoff-core/src/MathRenderer.cpp:79-100`) is the
  canonical entry point; results are cached process-wide in a
  `(latex, displayMode, fontSize, dpr)` `QHash` behind a `QMutex` (lines 35-46),
  so re-renders of the same equation are O(1).
- **Display-math fenced code blocks.** `ReadingView` registers `math` and
  `latex` languages on its built-in `CodeBlockProcessorRegistry`
  (`ReadingView.cpp:241-254`), routing fenced ` ```math ... ``` ` blocks
  through `MathRenderer::render(source, /*displayMode=*/true)`.
- **Mermaid fenced blocks.** `ReadingView::registerBuiltinCodeBlockProcessors()`
  (`ReadingView.cpp:226-235`) registers the `mermaid` language to a callback
  that resolves either an injected `Markoff::MermaidRenderer` or a lazy
  `Markoff::DefaultMermaidRenderer` no-op fallback. `Corbomite::Core::Markdown`
  /-equivalent SVG goes into a `QGraphicsSvgItem` per `SectionLayout.cpp` (the
  SVG layout path is in the same file, search for `QGraphicsSvgItem`). The
  Rust FFI gives us a 100% offline, no-Node mermaid implementation —
  effectively equivalent to Obsidian's bundled mermaid singleton, with `mmdr`
  freeing the SVG buffer through `mmdr_free`.
- **Code-block syntax highlighting.** `CodeBlockHighlighter` wraps
  `KSyntaxHighlighting::AbstractHighlighter` (`CodeBlockHighlighter.h:35-54`),
  reading the language tag from `QTextFormat::BlockCodeLanguage` and applying
  KSH formats per token. The KSH "Breeze Light"/"Breeze Dark" theme is picked
  from `Markoff::Theme::isDark` (header docstring lines 30-34). Coverage:
  ~300 KSH definitions, comparable in scope to Prism.
- **Wikilinks (clickable + hoverable).** `SpanRenderer` stamps
  `WikiLinkTargetProperty` onto link spans; `ReadingView::wikiLinkTargetAt`
  hit-tests against the `QGraphicsTextItem` document layout
  (`ReadingView.cpp:804-827`); LMB emits `wikiLinkActivated`; mouse-move with
  300ms debounce emits `linkHovered(href, globalPos)` (`ReadingView.cpp:123-133`).
  Host wires this to `HoverPopover` (see `src/editor/HoverPopover.cpp`).
- **Hover popover.** `HoverPopover` (`src/editor/HoverPopover.cpp:41-153`) is
  a `Qt::ToolTip`-flagged `QFrame` containing a *real `ReadingView`* — math,
  mermaid, syntax-highlighting, wiki-links and images all render through the
  same pipeline as full reading mode. This is genuinely better than Obsidian's
  popover (which uses a sub-MarkdownPreviewView): parity *plus* it shares
  recycling pools.
- **Embedded markdown `![[Note]]`, `![[Note#heading]]`, `![[Note#^block]]`.**
  `EmbedRenderer::render` / `renderMarkdown`
  (`libs/markoff-family/libs/markoff-reading/src/EmbedRenderer.cpp`) does
  recursive expansion with subpath slicing through `MetadataCache`. Depth cap
  enforced via `Markoff::EmbedDepthGuard` (≥5 levels → clickable placeholder).
- **Embedded images.** `registerBuiltinEmbedFactories` (lines 366-411)
  registers `png/jpg/jpeg/gif/svg/webp` extensions through `imageWikilinkShim`
  which rewrites `![[foo.png]]` to `![](foo.png)` so the SpanRenderer image
  path consumes it.
- **Search-result highlighting.** `Corbomite::ResultHighlighter::drawHighlighted`
  (`libs/search/include/corbomite/search/ResultHighlighter.h`) is the canonical
  helper — accepts `QVector<QPair<int,int>> matches` and bolds those character
  ranges in a different colour. Used by `SearchResultsDelegate`
  (`src/plugins/search/SearchResultsDelegate.cpp:62`). This is exactly the
  `renderResults` parity Obsidian relies on.
- **Footnote definitions extraction + numbering.** `Document::fromMarkdown`
  (`libs/markoff-family/libs/markoff-parser/src/Document.cpp:78-145`) extracts
  `[^label]: content` definitions, numbers references in order of first
  appearance, and rewrites `[^label]` to `<sup>N</sup>` before the tree-sitter
  parse.
- **Plugin-extensible code-block processors.** The
  `Markoff::CodeBlockProcessorRegistry` (header lines 23-49) lets plugins
  register their own fenced-language handlers — broader than Obsidian, where
  the equivalent registry lives in `MarkdownPreviewRenderer`.
- **Plugin-extensible embeds.** `Markoff::EmbedRegistry` (parallel registry)
  lets plugins register per-extension embed factories.

## Partial / divergent (per-engine)

### Math (JKQTMathText vs MathJax)

**Coverage gap.** `JKQTMathText` is a respected Qt-native LaTeX typesetter with
its own parser at `libs/jkqtmathtext/parsers/jkqtmathtextlatexparser.cpp`, but
its capability ceiling is well below MathJax v3:

- AMS extensions (`\begin{align}`, `\begin{cases}`, `\binom`, `\substack`,
  `\boxed`, `\xrightarrow`) are partially implemented at best — JKQTMathText
  has a `mathEnvironment` parser path (lines 41-50) but the supported
  environment list is much narrower than MathJax's `extension/[ams,physics,mhchem]`.
- `\newcommand` / `\renewcommand` user-macro support is not present in
  JKQTMathText's parser — searching `parsers/jkqtmathtextlatexparser.cpp` for
  "newcommand" returns no hits.
- Color / `\color{red}` may or may not work depending on the parser node
  table; AMS-specific symbols (`\mathbb{R}`, `\mathfrak{g}`) are likely partial.
- No font stack switching — Obsidian users routinely toggle between MathJax's
  `STIX-Web`, `TeX`, `Asana` sets; here we hardcode `mt.useXITS()`
  (`MathRenderer.cpp:51`).

**Behaviour divergence.** `MathRenderer::render` always wraps the input in
`$...$`, even for display mode, and adjusts only font size (lines 56-58 +
docstring). MathJax's `tex2chtml(latex, {display:true})` actually switches the
parser into display-mode (different spacing rules around `=`, `\quad`,
operator alignment). Heavy-equation notes will render visually different.

**No batching equivalent.** Obsidian's `finishRenderMath()` debounces 100ms
across an entire markdown render
(`/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/rendering/finishRenderMath.js`).
JKQTMathText is per-call synchronous so we get results immediately, but
without batched font/layout amortisation. For equation-dense notes this
matters less because of the LRU cache; first-paint of a new note can still
stutter.

**No lazy-load shim.** No `loadMathJax()` no-op resolved future for plugin
API parity. A ported Obsidian plugin that does `await loadMathJax();
MathJax.tex2chtml(...)` will fail to even compile against our API surface.

### Mermaid (mmdr Rust FFI vs mermaid.js)

**Coverage.** The `mmdr` Rust library is a clone of mermaid-rs-renderer
(`libs/mmdr/README.md:3`) — capability ceiling unknown but it's a pure-Rust
mermaid implementation. Diagram types supported are whatever upstream
mermaid-rs-renderer covers; the docs don't enumerate. This is a *real* gap
worth flagging — a diff between mmdr's supported diagrams and current
mermaid.js (flowchart, sequence, class, state, ER, journey, gantt, pie,
quadrant, mindmap, timeline, sankey, xychart, block, packet, kanban,
architecture, ZenUML, gitGraph) almost certainly leaves many out.

**No theme integration.** `MermaidRenderer::renderSvg(source)`
(`libs/core/src/MermaidRenderer.cpp:12-28`) takes only the source string —
no Mermaid theme parameter. Obsidian re-themes mermaid on dark-mode toggle
via `mermaid.initialize({theme: 'dark'})`. Corbomite always renders the same
default mermaid theme regardless of `Theme::isDark`. Visible regression on
dark themes: text becomes near-illegible against the SVG's default light
backdrop (no automated check found).

**No async or caching.** `Corbomite::Core::MermaidRenderer::renderSvg` is
synchronous and uncached. Each scroll-into-view of a mermaid block re-runs
the Rust SVG generator. Obsidian's mermaid renders are cached per source
hash. Mitigated only by `SectionRecyclePool` reuse of the QGraphicsItemGroup
once it's built, but that doesn't survive `setTheme` / `setContentWidth`
(both of which `clear()` the pool — `ReadingView.cpp:279, 292, 302`).

**Pre-render block detection.** Tree-sitter AST identifies fenced blocks with
`mermaid` language tag; `SectionLayout` invokes the registered processor at
mount time. Looks correct.

**Behaviour if Rust binary missing.** The static archive is built into the
binary — there's no missing-binary fallback path because there's no separate
binary. If the Rust build fails on a future cargo update, the whole
Corbomite link fails. This is fine for now but worth noting: a runtime
fallback to "[mermaid not available]" would be more graceful than a link
error.

### PDF (none)

**Missing entirely.** No PDF view in `src/`. `ExportToPdf.cpp` exports the
*current note* to PDF but does not *render* an existing `.pdf`. The
`EmbedRenderer` registers `pdf` as a `mediaPlaceholderFactory` — the
embedded child carries the literal text `"PDF preview not yet available —
<filename>"` (`EmbedRenderer.cpp:393-396`). Equivalent of Obsidian's
`loadPdfJs()` / PDF embed view: not implemented. This is the single biggest
rendering-domain capability gap, since `.pdf` files are common in research
vaults.

### Prism (preview syntax highlighting → KSyntaxHighlighting)

**Mostly parity.** KSH covers ~300 languages — comparable to Prism's coverage.
The Markoff theme's `isDark` flag drives the KSH built-in palette (Breeze
Light / Breeze Dark), so dark-mode users get appropriate code colours. The
header acknowledges per-token colour overrides from Markoff theme's
`CodeKeyword`/`String`/`Comment` element styles are a follow-up
(`CodeBlockHighlighter.h:32-34`) — currently the KSH built-in palette is
used as-is, so user themes that try to recolour code blocks via Markoff
theme JSON are silently ignored. Minor divergence; acceptable.

**Language-name aliasing not visible.** Obsidian accepts `py`, `python`,
`python3` for the same Prism grammar via internal aliases. KSH has its own
alias system; whether `CodeBlockHighlighter` exercises both lookup tables
or only the canonical name is not clear from the header — would be worth a
test note exercising `py`/`js`/`ts`/`md` aliases. Same theme as Source mode
likely yes (both use KSH) but the audit doesn't independently verify.

## Missing

- **PDF view + `![[file.pdf]]` embed render.** Above. No Poppler-Qt6 or Okular
  kpart wiring; no `loadPdfJs`-equivalent.
- **`htmlToMarkdown` (paste from browser).** The Markoff `TextControl` paste
  path (`libs/markoff-family/libs/markoff-live/src/TextControl.cpp:1667-1677`)
  only checks `source->hasText()` — it inserts the *plain-text* representation
  of the clipboard. There is no `source->hasHtml()` branch and no Turndown-
  equivalent HTML→Markdown converter anywhere in the tree
  (`grep -rn htmlToMarkdown .` returns only documentation references). Pasting
  formatted content from a browser drops all markdown affordance: bold/italic
  spans become plain text, links lose their URLs, lists lose structure. This
  is a glaring gap — Obsidian users *expect* "copy a Wikipedia paragraph,
  paste, get markdown bullets/links" behaviour. High-priority.
- **`sanitizeHTMLToDom` (DOMPurify).** Plugin-supplied HTML sanitisation
  chokepoint. No `Markoff::sanitizeHtml` helper exists; `QTextDocument::setHtml`
  is the closest implicit substitute but it silently drops `<details>`, `<svg>`,
  forms, etc. via Qt's XHTML 1.0 subset. As long as Corbomite has no plugin
  that supplies HTML to render, this is dormant — but Cluster N (plugin API)
  shipping makes this a security item, not a feature item.
- **`displayTooltip` rich-tooltip primitive.** Corbomite uses Qt's `setToolTip` /
  `QToolTip::showText` exclusively (search of `src/` for `QToolTip`/`setToolTip`
  shows ~20 hits, all simple text). No equivalent of Obsidian's
  placement-adapting, viewport-clamping, recently-dismissed-cooldown
  `displayTooltip`. The `HoverPopover` does cover the *rich content* case (it's
  arguably nicer than Obsidian's popover), so this gap matters mostly for icons
  and small chrome elements where a richer tooltip would matter (placement
  hints, multi-line, classes for theming).
- **Callouts (`> [!note]` / `[!warning]` / `[!info]` / etc.).** The parser does
  *not* split blockquotes by callout-type prefix — `grep -in "callout" libs/markoff-family/libs/markoff-parser/`
  returns zero hits. `SectionLayout.cpp:747` paints all blockquotes through the
  `Blockquote` paragraph style; there's no per-type accent colour, no
  Lucide-icon, no foldable `> [!note]-` chrome. `Markoff::Theme::calloutColor`
  (`libs/markoff-family/libs/markoff-core/src/Theme.cpp:18-21`) and
  `Theme::Element::Callout` (line 107) *exist in the theme schema* — the data
  model is there — but no rendering path consumes them. This is a **major
  visible gap**: callouts are arguably Obsidian's most distinctive
  reading-mode chrome and a flagship Obsidian-Help-vault feature
  (per `markoff-live/docs/appendix-D-official-help-sandbox-vault.md:43`,
  callouts are taught front-and-centre in the help vault).
- **Footnote hover tooltip + jump-link.** The parser numbers footnotes and
  rewrites `[^N]` to `<sup>N</sup>` (`Document.cpp:121`), but the rendered
  superscript is *not* hyperlinked — there's no jump-link to the definition
  block, no hover popover showing the definition, and the definition list
  itself (`[^N]: ...`) is *removed* from the markdown before parsing
  (`Document.cpp:94-95`). The footnote definitions list nowhere appears in
  the rendered output. So footnotes are visible as numbers but functionally
  inert — no navigation, no definition rendering, no hover preview. Major
  gap.
- **Centralised inline-primitive renderer (`RenderContext` analogue).**
  `LinkRenderer` (`libs/markoff-family/libs/markoff-reading/src/LinkRenderer.cpp`)
  is a signal emitter, not a renderer. No central
  `Markoff::RenderContext::renderFileLink(target, label, parentEl)`
  /`renderTag(tag, parentEl)`/`renderExternalLink(url, parentEl)` that other
  surfaces (search rows, future Bases cells, suggesters, plugin-supplied DOM)
  can call. Each surface re-implements link affordances. The contract Obsidian
  uses to keep "click a wikilink in a Bases cell" and "click a wikilink in
  reading mode" behaviourally identical is not architecturally enforceable
  here. (For tags specifically: ReadingView never even *calls* `renderTag`
  — `grep -n emitTagLink libs/markoff-family/libs/markoff-reading/src/`
  returns no consumer.)
- **Bidi inline isolates.** Mixed-script paragraphs (e.g. English with Arabic
  inserts) get only Qt's per-paragraph `QTextOption` direction, never the
  per-inline-run isolate spans Obsidian inherits from CM6's `bidiIsolate`.
  Symptom: punctuation between scripts can flow into the wrong direction.
  Obsidian's `mW.rtl/.ltr/.auto` decoration table has no analogue
  (already flagged in the audit doc §11).
- **`Value` typed-cell tower for plugin-extensible cell rendering.** Belongs
  to a future Bases audit; not implemented here.
- **Pre-warming of math/mermaid on cold start.** No
  `MathRenderer::warmCache()` or similar. Cold open of an equation-dense
  note shows a brief flicker as each unique equation typesets for the first
  time. Obsidian batches these via `finishRenderMath`; we have nothing
  equivalent.

## Notable translation successes

- **HoverPopover-with-real-ReadingView.** `src/editor/HoverPopover.cpp:60-69`
  embeds a `Markoff::Reading::ReadingView` *as the popover content*. So link
  previews, math, mermaid, syntax-highlighting, and embeds all render in the
  popover via the same pipeline as the full view — guaranteeing visual parity
  between hover preview and full open. Obsidian achieves the same thing via a
  sub-`MarkdownPreviewView`; we get there via composition. This is a clean
  translation.
- **Cache-keyed math rendering.** Process-wide `QHash` keyed on
  `(latex, displayMode, fontSize, dpr)` with a `QMutex`
  (`MathRenderer.cpp:35-46, 79-100`). Idiomatic Qt; no bridge to a JS heap.
  Lossless re-render performance.
- **DI seam via Phase C1 setters.** `ReadingView::setEmbedRegistry`,
  `setVaultLinkResolver`, `setVaultMetadataCache`, `setVaultMetadataParser`,
  `setMermaidRenderer` (header lines 70-85, source lines 161-186) plus
  per-default-getter `resolveOrDefault` (lines 198-210) lets the standalone
  Markoff build run with `Default*` no-ops while the host injects real
  vault-aware concretes. This is *better* than Obsidian's monolithic `app`
  ambient instance — testable, swappable, plugin-friendly.
- **CodeBlockProcessorRegistry as a public extension surface.**
  `Markoff::CodeBlockProcessorRegistry` (header) lets a plugin claim a fenced
  language and produce arbitrary output. Same role as Obsidian's
  `Plugin.registerMarkdownCodeBlockProcessor` — already plumbed.
- **Section recycling pool.** Better-than-Obsidian section reuse keyed on
  rendered shape (`SectionRecyclePool`).

## Notable concerns / suspected bugs

- **Footnotes are functionally dead.** As above — numbered superscripts that
  go nowhere, definitions removed silently. A user who pastes `[^1]` and
  `[^1]: Source: ...` will see `1` and lose the source text without warning.
  This is closer to a regression than a "missing feature" — the work has
  been half-done.
- **Mermaid renders as light theme on dark themes.** As above.
- **`RenderContext` substrate gap will bite the plugin API.** The plugin-API
  surface (Cluster N, recently shipped per the project memory) ports a
  surface that Obsidian plugins call as `app.getMarkdownPostProcessors()` and
  hand DOM to. Without a `RenderContext` analogue, plugins ported from
  Obsidian have no canonical entry point to render an internal link with the
  right context-menu / drag / hover-link behaviours. Each plugin would
  re-implement this. The repercussions are deferred but real.
- **`Theme::calloutColor` exists in JSON, no callout consumer.** The data
  model anticipates callouts but the rendering pipeline ignores them. Theme
  authors who edit `Light.json` to set `calloutAccents.warning` will see no
  effect — silent dead-letter.
- **`SectionRecyclePool::clear()` on theme switch / contentWidth change.**
  `ReadingView::setTheme` (`ReadingView.cpp:287-296`) clears the pool, the
  layout, and `m_lastMarkdown`, then triggers a full rebuild. For very large
  notes this can cause a visible reload pause; mitigations would be to
  reuse text geometry while only restamping colours.
- **`registerBuiltinCodeBlockProcessors` registers a `default` lang stub.**
  `ReadingView.cpp:256-264` registers a processor for the literal language
  tag `default` whose body just constructs a `CodeBlockHighlighter` and does
  nothing with it. This looks like a placeholder that was committed without
  hookup — `hasLanguage("default")` will return true, giving plugins a
  surprising behaviour. Looks like dead code or a never-finished hook.
- **`MathRenderer` always wraps in `$...$`.** Display-mode math gets only
  the font-size boost, not display-mode parser semantics
  (`MathRenderer.cpp:56-58` + comment). Equations using `\begin{align}`
  or `\\` line breaks may render with wrong spacing.
- **`mmdr` is uncached + sync.** Every paint of a mermaid block re-runs Rust
  SVG generation if the section has been recycled out and back in. Add a
  `QHash<QString, QByteArray>` cache to `Corbomite::Core::MermaidRenderer`.
- **Paste-HTML drops all formatting silently.** The Live editor's
  `insertFromMimeData` uses `hasText()` only — formatting silently lost. This
  is the kind of gap users blame on the markdown engine, not the paste
  handler.
- **`linkHovered` 300ms delay is hard-coded.**
  (`ReadingView.cpp:125, HoverPopover.cpp:19`). Two callsites both happen to
  use 300ms but they're independent constants. If the user later tunes the
  Page-Preview delay setting, only one gets it.

## RenderContext-equivalent: ambient renderer API for plugin extensibility

Corbomite has the *raw materials* — `LinkRenderer`, `EmbedRenderer`,
`SpanRenderer`, `MathRenderer`, `MermaidRenderer`, `CodeBlockHighlighter` — but
no central object that plugins / non-ReadingView surfaces (search rows,
future Bases cells, suggesters, hover popovers in non-markdown contexts) can
construct and hand DOM to. Obsidian's `RenderContext`
(`/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/rendering/RenderContext.js`)
serves three load-bearing roles:

1. **Behavioural consistency:** every wikilink span anywhere in the app
   gets the same context-menu sections, drag handler, hover-link emission,
   and click semantics. Achieved in Obsidian by a single 80-line method
   (`renderFileLink`); Corbomite would currently have to re-implement that
   in each surface (search delegate paint, future Bases cell paint, etc).
2. **Plugin DI:** plugins get a `ctx: RenderContext` parameter into
   `renderTo(el, ctx)`-shaped callbacks. They don't need to import
   `app.workspace`/`app.metadataCache`/`hoverLinkSources` separately. We
   have nothing analogous; Cluster N's `VaultProxy` / `FileManagerProxy`
   pattern is closer to a service-locator triplet.
3. **`hover-link` source-id contract:** Obsidian hard-codes
   `source: "bases"` on the emit. Markoff's `LinkRenderer` already does
   the right thing here — a `sourceId: QString` field on
   `FileLinkRequest` (header line 35) explicitly says "no `bases` hardcode
   — compat aliasing is a Cluster N shim concern". Forward-thinking
   choice; flagged appreciatively.

The cleanest path: extract a `Markoff::RenderContext` (or
`Corbomite::RenderContext`) class composing `LinkResolver`, `EmbedRenderer`,
a hover-link emitter, and exposing `renderFileLink(parentItem,
targetWidget, linkText, sourceId)`,
`renderExternalLink(parentItem, url, sourceId)`,
`renderTag(parentItem, tag, sourceId)` returning `QGraphicsItem *` /
`QWidget *` per surface. ReadingView's SpanRenderer would call into it
instead of stamping `WikiLinkTargetProperty` directly; the search delegate
and the future Bases cell could call into it too. Until that exists,
"render an internal link" is a per-surface re-implementation.

---

**Summary:** The translation strategy is sound and the underlying primitives
(math via JKQTMathText, mermaid via `mmdr`, syntax highlighting via KSH,
embed registry, code-block processor registry, hover popover with real
ReadingView) are in good shape. The most user-visible regressions are
**callouts (data model exists but no renderer)**, **footnotes (numbered but
non-navigating, definitions silently dropped)**, **PDF view (entirely
absent)**, and **paste-from-HTML (`htmlToMarkdown` missing)**. The most
architectural gap is the absence of a `RenderContext` analogue for cross-
surface inline-link consistency — a debt that grows once Bases lands and
plugins start handing DOM to Corbomite via the Cluster N API. The math /
mermaid pipelines work but need theme-awareness (mermaid) and AMS coverage
diff (math) before they reach unqualified parity.
