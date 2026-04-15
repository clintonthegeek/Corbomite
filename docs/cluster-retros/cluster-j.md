# Cluster J — Embed / rendering primitives (retrospective)

**Landed:** 2026-04-15. 6 phases, 18 commits across the cluster (12 implementation + 6 state-update / phase-marker commits).

**Phase commits (implementation only):**
- Phase 1 — `8498efc` (Core::VaultResourceProvider promotion) + `a96ff74` (MarkdownRenderChild) + `3a8bf9a` (EmbedDepthGuard) + `aaf90d4` (EmbedRegistry).
- Phase 2 — `ef549c4` (PostProcessorRegistry) + `f9612ae` (CodeBlockProcessorRegistry).
- Phase 3 (parallel with 4) — `86e3d44` (Markoff::LinkRenderer) + `c795150` (link-source honesty + wikilink-clickable integration).
- Phase 4 (parallel with 3) — `c2ec977` (ReadingView::LinkRenderer) + `3051146` (ReadingView::EmbedRenderer).
- Phase 5 — `66460d9` (Mermaid via registry) + `a9fe84b` (math via registry) + `40487ab` (default syntax highlighting via registry) + `2f2f143` (`.md` EmbedRegistry built-in) + `66debad` (image wikilink-shim factories) + `92b207d` (PDF + audio + video media-stub placeholders).
- Phase 6 — `df20eda` (HoverPopover renderer swap to EmbedRenderer; ReadingView-hosted preview).

## What changed vs the original plan

Mostly faithful, with two recorded scope adjustments:

1. **Phase 5 SectionLayout-vs-CodeBlockProcessorRegistry shape.** The plan implied that built-in code-block processors (mermaid / math / syntax) would assume scenegraph-emission ownership via the registry. They couldn't: `CodeBlockProcessorRegistry::dispatch(...) -> bool` returns a boolean, while SectionLayout's block loop consumes graphics items directly. Per the plan's "don't work around structurally" guidance, the registry became a *parallel plugin-reachable contract* at the ReadingView level — lambdas invoke the same bridges (MermaidRenderer, JKQTMathText, CodeBlockHighlighter) so dispatch is real and callable by a future plugin layer, but graphics-item emission continues to live in `SectionLayout`. Tests verify dispatch returns `true` for the registered languages and existing `tst_sectionlayout_*` tests still pass — no visible behaviour change. Full restructuring of SectionLayout → registry-driven is logged for Cluster N when a real plugin needs it.

2. **Phase 6 EmbedRenderer late-bind setters.** The plan called `EmbedRenderer::renderInto(parent, ...)` in HoverPopover. The constructor takes registry / cache / resources by pointer; built-in factories capture the renderer reference once at registration. To survive vault open / close cycles without rebuilding the registry, Phase 6 added two-line `setMetadataCache` / `setResources` setters to `EmbedRenderer` (Cluster J's only post-Phase-1 surface change to the readingview library). MainWindow constructs registry + renderer + factory set once at startup and re-points the cache + resources on every `onVaultOpened` / `onVaultClosed`.

## What surprised

- **The two parallel phases (3 + 4) genuinely were independent.** Markoff and ReadingView's link-emission pipelines share no source state; `Markoff::LinkRenderer` and `ReadingView::LinkRenderer` were drafted in the same session window without merge friction. Same pattern that worked for Cluster E Phase 0 holds at Phase 3+4 scale.

- **`EmbedRenderer::render` already produced the right output for HoverPopover.** The recursive embed-expansion + image-shim + heading slice all happen inside `render` — Phase 6 didn't have to add any rendering smarts, only a wiring decision: "feed `child->renderedText()` to a hosted ReadingView". The expanded markdown text has all nested `![[...]]` substitutions done; the ReadingView then handles math / mermaid / syntax via Phase 5's built-in registrations. The two phases' substrates compose without an integration layer.

- **`MarkdownRenderChild::mountInto` is a forward-pointer-only stash today.** It stores the host `QPointer` but doesn't actually reparent / mount a widget. The `renderInto` convenience method on `EmbedRenderer` is therefore mostly a marker for "this is the call site"; Phase 6 took the equivalent path of calling `render` directly + reading `renderedText()`. When a future cluster (probably K — Bases cell rendering — or N — plugin ABI) needs the child to actually own a Qt widget subtree, `mountInto` will need to grow real semantics.

- **The `"bases"` hover-link source quirk really is just plugin-compat baggage.** Audit Pass 1 flagged it; brainstorming decision §6 ratified "honest per-caller source string everywhere"; Phase 3's `Markoff::LinkRenderer` shipped without it. Documented in the audit addenda for the eventual Cluster N `obsidian` shim — no plugin we plan to support depends on the string. If one ever does, the alias is one line.

## Downstream effects

- **Cluster K (Bases — cell rendering substrate) unblocked.** Bases needs a RenderContext-equivalent for cell rendering; J's `LinkRenderer` + `EmbedRenderer` are the substrate. K's *other* blocker (Bases DSL extraction addendum) remains; J alone doesn't fire K's expansion trigger.

- **Cluster L extensions (embedded property values) unblocked.** When a property stores a wikilink or image, the rendering side falls out of J's `EmbedRenderer` directly. L itself is already done; the extensions are a follow-up.

- **Cluster N (plugin-ABI bridge) substrate freshly settled.** J's deliberate "internal registries + plugin-ABI-deferred" split is the candidate shape N would wrap. The internal API is now stable enough that an ABI bridge can target it without re-design.

- **Cluster D follow-up #1 (search-side AST consumers).** The render side of `line:` / `block:` / `section:` / `task*:` operators now ships; the consumer side in search DSL is substrate-ready.

- **Cluster H follow-up #3 (HoverPopover rect-anchoring) NOT unblocked.** Cursor-position anchoring was preserved per design decision §7. Rect-anchoring still requires `Markoff::Editor::hoveredLinkRect()`, which is a named Markoff-API follow-up outside J's scope.

## Lessons for the next cluster

- **When a registry's call signature is structurally incompatible with the existing emission pipeline, build a parallel contract instead of forcing structural refactor in the same cluster.** Phase 5's "lambdas invoke the bridges, SectionLayout still owns scenegraph emission" pattern is the right move when (a) the visible behaviour doesn't change and (b) the structural refactor needs a downstream consumer (here: a plugin) to motivate the design choices. Forcing the refactor blind would have produced over-fitted abstractions.

- **Late-bind setters are a legitimate seam for renderer-style classes whose dependencies have a longer lifetime than the registrations they back.** EmbedRenderer's two-line `setMetadataCache` / `setResources` addition cleanly resolved the registry-factories-capture-by-reference vs cache-lives-per-vault tension. The alternative (rebuild registry per vault) would have re-walked the built-in factory set on every vault open.

- **`Markoff::LinkRenderer` typed-emission surface paid off immediately.** Once Markoff had a single inline-link emitter, Phase 3's "emit honest per-caller source strings" became one line per call site instead of grep-and-edit at every emission point. Build the typed surface first, even when the immediate need is a single source-string change.
