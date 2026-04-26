# Editor-markdown domain audit

Scope: Obsidian's `MarkdownView` + `MarkdownPreviewView` + `MarkdownPreviewRenderer`
+ `MarkdownPreviewSection` + `MarkdownRenderChild` + `MarkdownRenderer` translated
into Corbomite's `Markoff::*` family + `Corbomite::MarkdownView` /
`Corbomite::NoteEditorWidget`. Source-of-truth for the spec is
`/home/clinton/dev/Corbomite/docs/obsidian-audit/domains/editor-markdown.md`.

## Architecture fit (Markoff family vs MarkdownView)

Corbomite has split Obsidian's monolithic `MarkdownView` (which internally hosts
mode objects plus `MarkdownEditView`/`MarkdownPreviewView`) into a **two-tier**
Qt architecture:

1. **Host shell** — `Corbomite::MarkdownView`
   (`/home/clinton/dev/Corbomite/src/editor/MarkdownView.h:17`,
   `/home/clinton/dev/Corbomite/src/editor/MarkdownView.cpp:23`) is a
   `TextFileView` subclass registered for `.md` (the `getViewType()` returns
   `"markdown"` at `MarkdownView.cpp:39` — this matches Obsidian's `VIEW_TYPE`
   string-id contract for ViewState round-trip). It owns one
   `Corbomite::NoteEditorWidget`
   (`MarkdownView.cpp:25`).

2. **Mode multiplexer** — `Corbomite::NoteEditorWidget`
   (`/home/clinton/dev/Corbomite/src/editor/NoteEditorWidget.h:38`) holds a
   `QStackedWidget` (`NoteEditorWidget.cpp:33`) containing one of three peer
   `Markoff::MarkdownView` subclasses:

   - `Markoff::Editor` (live preview), eager-constructed
     (`NoteEditorWidget.cpp:34`)
   - `Markoff::Source::SourceEditor` (raw source / Qutepart-backed), lazy
     (`NoteEditorWidget.cpp:192`)
   - `Markoff::Reading::ReadingView`, lazy (`NoteEditorWidget.cpp:203`)

3. **Tri-view base** — all three leaves derive from
   `Markoff::MarkdownView`
   (`/home/clinton/dev/Corbomite/libs/markoff-family/libs/markoff-core/include/markoff/MarkdownView.h:27`)
   which is a polymorphic `QWidget` with abstract `setDocument`,
   `scrollPosition`, `cursorPosition`, `ephemeralState`, `searchAdapter`,
   `setReadOnly`, `foldedHeadings`, plus the C7 `showFindBar` /
   `showReplaceBar` / `hideFindBar` virtuals (lines 33, 47, 56–84). Capability
   probes (`hasCursor`/`hasEditing`/`hasFold`) let the host dispatch through
   the contract instead of RTTI.

4. **Canonical document** — `Markoff::MarkoffDocument`
   (`/home/clinton/dev/Corbomite/libs/markoff-family/libs/markoff-core/include/markoff/MarkoffDocument.h:28`)
   holds the shared markdown source + `QUndoStack` + parse cache. Each
   mode-leaf attaches/detaches via `setDocument()`. Mode swap pattern in
   `NoteEditorWidget::setViewMode` (`NoteEditorWidget.cpp:337`):
   capture-eState → detach-leaf → ensureWidget → setCurrentIndex →
   attach-leaf → restore-eState. Step 2's `setDocument(nullptr)` is the
   intended replacement for Obsidian's `await save()` before leaving source —
   but unlike Obsidian, this **does not block on disk write**; it only
   detaches the view from the in-memory canonical buffer (the buffer itself
   carries unsaved content forward via `MarkoffDocument`'s undo stack and the
   Phase C3 contentsChanged subscription).

This is a **structurally cleaner** translation than Obsidian's: where Obsidian
mode-toggles by hide/show + per-mode `set(data, false)` (re-pushing data on
every swap), Corbomite never round-trips content through the leaves — the
canonical buffer is shared and each leaf observes it. The trade-off is that
`Markoff::MarkdownView`'s API surface is narrower than Obsidian's `MarkdownView`
prototype: 33 prototype methods vs. roughly a dozen virtuals. Many of those
33 (frontmatter editor passthroughs, embedded backlinks panel, PDF export
modal, Ctrl+Wheel zoom, `editor-menu` emission, scroll-sync between linked
panes) are missing entirely or live as host-side stubs.

## Implemented (parity-equivalent)

- **Three-mode encoding** — `NoteEditorWidget::ViewMode { Source, LivePreview, Reading }`
  (`NoteEditorWidget.h:47`). Persisted to `getState()` /
  `setState()` (`MarkdownView.cpp:140-164`) using the **exact** Obsidian
  compound shape `{mode: "preview"}` vs `{mode: "source", source: bool}`. The
  `ViewModeSerializer` (`src/editor/ViewModeSerializer.h`,
  `NoteEditorWidget.cpp:268`) encapsulates the conversion both ways. State
  round-trips through workspace.json. **Best-in-class translation here** —
  spec invariant 2 (`getMode()` returns `"source"|"preview"`) is honoured.

- **Mode-toggle idempotence** — `setViewMode` early-returns if
  `m_viewMode == newMode` (`NoteEditorWidget.cpp:339`). Matches spec
  invariant 3.

- **Lazy mode-widget construction** — `ensureWidgetConstructed`
  (`NoteEditorWidget.cpp:187`) creates Source / Reading on first visit and
  caches in the stack. Live is eager because it's the default.

- **Ephemeral-state round-trip across mode switch** — `captureEphemeralStateFor`
  / `restoreEphemeralStateFor` (`NoteEditorWidget.cpp:265, 304`) preserve
  scroll, cursor, and (Reading-only) folded-heading lines. `goToLineAndColumn`
  (`NoteEditorWidget.cpp:324`) preserves the in-line column on Source→Live
  transitions — a small but real UX win documented at the bottom of the
  comment block.

- **Progressive section pipeline (in Reading mode only)** — `ReadingView` has
  the engine Obsidian's `MarkdownPreviewRenderer` is. Constants are pinned in
  `ReadingViewConstants.h` lines 15, 19, 24:
  `kAsyncParseThresholdBytes = 10240`, `kFrameBudgetMs = 5`,
  `kFrameBudgetSections = 10` — all **exact matches** for the Obsidian
  numbers in spec §1 ("`asyncParse` fires iff `text.length >= 10240`",
  "every 10 sections, if `performance.now()-start > 5 ms`, break").
  `mountInitialWindowWithBudget` (`ReadingView.cpp:543`) implements the
  yield-on-budget loop with `QTimer::singleShot(0, …)` resumption —
  parity-equivalent to Obsidian's `Pc(fn, 0)` idle scheduler. The
  `ReadingViewConstants` header even spells the contract out: "These numbers
  are not hints; they are the documented wire contract."

- **Section recycling by HTML-equality-equivalent shape key** — `ReadingSection::renderedShape()`
  (`ReadingSection.h:42`) returns a `QByteArray` that
  `beginMount` (`ReadingView.cpp:417-421`) hashes into `m_oldByShape` for
  reuse and `layoutSectionForController` (`ReadingView.cpp:485-495`) probes.
  Sections with matching shape transplant their `QGraphicsItem` subtree
  verbatim; sections with stale shape route through `m_recyclePool`
  (`SectionRecyclePool::take()` / `offer()`). This is **the Markoff
  equivalent of Obsidian's HTML-string-equality recycle** — though with a
  Qt-graphics-item shape key rather than literal sanitised HTML strings (a
  reasonable architectural divergence; see "Notable concerns" below for a
  caveat).

- **Frontmatter-diff cascade** — `m_pendingFmChanged`
  (`ReadingView.cpp:413` via `ReadingPipeline::detectFrontmatterChange`)
  forces `forceReRender` on any section with `usesFrontMatter()` true
  (`ReadingView.cpp:481`). Matches spec §1 item 3 (`Frontmatter diff via
  JSON.stringify equality; if changed, every section with
  usesFrontMatter=true is force-re-rendered`).

- **Async parse** — `ReadingParseWorker` is a `QThread`-resident
  `ReadingPipeline` (`ReadingParseWorker.cpp:24-42`); `parseAsync` posts
  through `QMetaObject::invokeMethod` with `QueuedConnection`; finished
  result hops back via `parseFinished` signal connected with
  `Qt::QueuedConnection` (`ReadingView.cpp:97-99`). Coalescing via
  `m_requestIdCounter`/`m_lastRequestIdHandled` (`ReadingView.cpp:384-405`)
  drops stale results. Functionally matches Obsidian's `Wz` worker singleton +
  request-id discard pattern.

- **Heading collapse** — Per-section `headingLevel`/`headingCollapsed`
  (`ReadingSection.h:29-33`); `recomputeFoldVisibility` walks linearly
  (`ReadingView.cpp:638-671`) hiding descendants until the next
  same-or-shallower heading — the **same algorithm** as Obsidian's
  `MarkdownPreviewSection.shown` propagation. Persistence via
  `foldedHeadingLines()`/`setFoldedHeadingLines()`
  (`ReadingView.cpp:690-739`); `toggleFold(int sectionIdx)` mutates one
  section and re-evaluates (`ReadingView.cpp:741-773`).

- **Visual-line float scroll position** — `scrollPositionVisualLine()` /
  `setScrollPositionVisualLine(float)` (`ReadingView.cpp:788-802`) interpolates
  pixel offset / `visualLineSpacing()`. Same semantic as Obsidian's
  `getScroll()` returning `42.73`. Matches spec §12 ("Scroll is a visual-line
  FLOAT, not pixel offset").

- **Capability-probe-based feature gating** — `hasCursor` / `hasEditing` /
  `hasFold` overrides (`ReadingView.h:150-152`, `Editor.h:133-135`,
  `SourceEditor.h:50`). Reading is read-only (`isReadOnly()` returns true at
  `ReadingView.cpp:975`; `setReadOnly(false)` refused with `qCWarning` at
  line 968). Matches the intent of Obsidian's preview-vs-source method
  routing (`undo`/`redo` only valid in source).

- **Embed framework** — `Markoff::EmbedRegistry`
  (`/home/clinton/dev/Corbomite/libs/markoff-family/libs/markoff-core/include/markoff/EmbedRegistry.h:29`)
  is a `QHash<extension, EmbedFactory>`, dispatched by file extension
  (`dispatch()`, lines 39–48). `EmbedDepthGuard` (referenced at
  `ReadingView.h:234`) implements the spec's `JZ()` ancestor-counting depth
  cap with a placeholder fallback at depth 5
  (`EmbedRenderer.h:36-38`). `EmbedRenderer::render`
  (`EmbedRenderer.h:73`) returns a `MarkdownRenderChild` for every code path
  including unknown-extension and depth-cap-rejection — the "non-null in all
  paths" contract documented in lines 67–73 closely mirrors Obsidian's
  fallback chain. **Built-in extension factories** at
  `EmbedRenderer.cpp:366-371` register `.md`, image extensions, and a
  PDF/audio/video placeholder set.

- **MarkdownRenderChild lifecycle wrapper** —
  `Markoff::MarkdownRenderChild`
  (`/home/clinton/dev/Corbomite/libs/markoff-family/libs/markoff-core/include/markoff/MarkdownRenderChild.h:12`)
  is a base class with `mountInto(QWidget*)`. **Crucial**: this is the
  Obsidian-spec answer for the "unload-on-recycle" lifecycle wrapper. It is
  thin (no `Component`-style child registry yet — see "Plugin-extensibility
  risks" below) but the type is in place and the embed renderer threads it
  through correctly.

- **Code-block processor registry** — `Markoff::CodeBlockProcessorRegistry`
  (`ReadingView.h:120`, `registerBuiltinCodeBlockProcessors` at
  `ReadingView.cpp:222-265`) registers `mermaid`, `math`, `latex`, `default`.
  Matches spec §10's `Plugin.registerMarkdownCodeBlockProcessor(lang, fn)` —
  a real Cluster J shipping piece.

- **Find/Replace contract** — Each leaf supplies a `Markoff::SearchAdapter`
  via `searchAdapter()` (`MarkdownView.h:61`). `Markoff::SearchController`
  + `ReplaceController` drive any adapter
  (`/home/clinton/dev/Corbomite/libs/markoff-family/libs/markoff-core/include/markoff/SearchController.h`).
  C7-era `showFindBar` / `showReplaceBar` / `hideFindBar` virtuals on
  `MarkdownView` (lines 80–84) let the host dispatch uniformly. In
  `MarkdownView.cpp:333-350` the hamburger menu's `Find...` / `Replace...`
  dispatch through `m_editorWidget->activeLeaf()->showFindBar()`. Live and
  Source override; Reading inherits the no-op default.

- **Hover-link wiring** — both Live and Reading leaves emit a `linkHovered`
  signal carrying global screen position; `NoteEditorWidget` connects each
  to a shared `HoverPopover::scheduleShow` (`NoteEditorWidget.cpp:57-70` for
  Live; `NoteEditorWidget.cpp:214-223` for Reading). `HoverPopover` routes
  through `EmbedRenderer` for non-trivial targets
  (`/home/clinton/dev/Corbomite/src/editor/HoverPopover.h:49`,
  `HoverPopover.cpp:79`). Matches spec §10 hover-link surface.

- **Ctrl+E reading toggle** — bound at `MainWindow.cpp:283` (per audit doc),
  routed through the `view.linked` hamburger items in
  `MarkdownView.cpp:275-303`.

## Partial / divergent

- **Live preview cursor-reveal granularity** — `Markoff::Editor` is a
  QGraphicsView-backed widget where each block (paragraph, list-item,
  heading) renders as a `MarkdownTextItem`, and substitutions
  (`Substitution.cpp`, `MathTextObject.cpp`, `CheckboxTextObject.cpp`,
  `ImageBlockItem.cpp`) replace markdown sigils with rendered glyphs in
  place. The `EditorContextClassifier` (`src/EditorContextClassifier.h`)
  gates context-specific behaviour at the cursor. **However**, the spec's
  per-block reveal — i.e. `**bold**` becomes **bold** when the cursor is in
  another block but flips back to literal when the cursor enters the block
  — is not strictly per-block in this widget. The substitutions appear to
  be unconditional, applied at parse-time by the `MarkdownHighlighter`. The
  Obsidian-style "leave-block replaces text with rendered sub-widget" gating
  is not present in the canonical sense. (This is partly because Markoff's
  substitutions are atomic glyphs — `U+FFFC` text-object placeholders — so
  the source is *always* visible to the cursor at the rune level; what's
  missing is the visual transition itself.) See
  `libs/markoff-family/libs/markoff-live/docs/2026-04-16-codebase-evaluation.md`
  for the team's own assessment that current substitution behaviour collides
  with the live-preview contract.

- **Save-on-leave-source** — Spec invariant 4 says `setMode` `await`s
  `save()` before leaving `"source"` — i.e. switching source → preview on a
  dirty doc triggers a synchronous disk write. Corbomite's `setViewMode`
  (`NoteEditorWidget.cpp:337-371`) does **not** call save explicitly. It
  relies on the canonical `MarkoffDocument` carrying unsaved content forward
  in memory, which means the on-disk file may diverge from preview state
  for any duration. This is functionally OK because both modes read from
  the same canonical buffer — but it's a behavioural deviation from
  Obsidian. Cross-pane `quick-preview` cousins (cf. spec §12) would notice
  the difference because Obsidian's contract assumes save synchronicity.

- **Checkbox round-trip in Reading** — Reading mode does **not** appear to
  ship checkbox-click handling. The grep for `checkbox` in
  `libs/markoff-reading/src/` returns only a comment in `SpanRenderer.h:46`
  noting "collisions with Markoff's math/checkbox property assignments".
  Live-preview has `CheckboxTextObject` (a `QTextObjectInterface` glyph),
  but its `Editor::toggleCheckbox()` keyboard action is documented as
  **broken against the substitution layer** — see
  `libs/markoff-family/libs/markoff-live/docs/2026-04-16-codebase-evaluation.md`
  line 26 ("`toggleCheckbox()` is broken by the U+FFFC substitution
  layer") and line 48 (proposes reading `CheckboxTextObject::CheckedProperty`
  instead). Spec §8 item 12 documents the Obsidian regex
  (`/<li class="task-list-item.../`), the `data-task="?"` preservation,
  and the `setTimeout(0)` mutate-then-edit flow — Corbomite has none of
  this end-to-end.

- **Search adapters are stubs in Reading** —
  `ReadingSearchAdapter::cursorSourceOffset` returns 0,
  `highlightMatches` is a comment "Phase A stub. Out-of-scope for C7;
  tracked as post-C7 follow-up", `clearMatchHighlight`/`scrollMatchIntoView`
  same (`ReadingSearchAdapter.cpp:6-29`). So Find/Replace in Reading mode
  presents a UI but does nothing. Live and Source have working adapters
  (`LiveSearchAdapter.cpp:19-37`, `SourceSearchAdapter.cpp:25`).

- **Heading-collapse-affects-scroll-anchor** — Spec §12 calls out
  `showSection(target)` walking backwards un-collapsing ancestor headings
  so subpath nav (`[[Note#Sub]]`) auto-reveals the target. Corbomite's
  `setFoldedHeadingLines` and `toggleFold` operate by line index, but I
  found no `showSection`-equivalent that walks ancestors during link
  navigation. `setCursorLine`/`goToLine` (`MarkdownView.cpp:80-84`,
  `NoteEditorWidget.cpp:242-263`) hard-codes Reading as a no-op — so even
  the "navigate to a line in Reading" verb is not implemented, let alone
  the un-collapse-ancestors variant. Subpath link navigation in Reading is
  therefore broken if any ancestor heading is folded.

- **Frontmatter properties UI** — There is a `Corbomite::PropertiesView`
  (`src/plugins/properties/PropertiesView.h:20`), but it lives as a plugin
  panel not as the inline-document YAML editor Obsidian renders atop the
  file. `MarkdownView::insertFrontmatterProperty()`
  (`MarkdownView.cpp:179-220`) appends raw YAML by string-splicing on
  `---\n`. That is text-editor behaviour, not the rich `metadataEditor`
  the spec describes. There is no equivalent of Obsidian's
  `propertiesInDocument` config or `canShowProperties()` gate.

- **Embed renderer recursion** — `EmbedRenderer::renderMarkdown` does
  recursive subpath-aware rendering with `splitWikiEmbed` and the depth
  guard (`EmbedRenderer.cpp:143, 289`). Functionally the right shape.
  However, the per-embed widget chrome — Obsidian's
  `markdown-embed`/`inline-embed`/`markdown-embed-title`/`markdown-embed-content`
  classes plus a click-to-navigate header — is not visible from the
  reading-side handler I read. The embed renders as text inside a
  `MarkdownRenderChild` and (for images) is shimmed back through
  `SpanRenderer` as an `![](path)` snippet (`EmbedRenderer.cpp:330` plus
  the `registerBuiltinEmbedFactories` doc string at lines 100-119). Not
  obviously broken, but the **clickable per-embed title bar** is absent.

- **PDF export** — `MarkdownView` has a `setPdfExportTrigger(std::function)`
  injection point (`MarkdownView.h:72`, `MarkdownView.cpp:227`); the
  hamburger-menu action calls it (`MarkdownView.cpp:325-330`). However,
  the *modal* with page-size / landscape / margin / downscale (spec §3
  item 3) is outside this domain — and the trigger is nullable, so when
  no host wires it the action is a silent no-op.

## Missing

### Tri-mode (state machine + UX)

- **Mode-button on the action bar** with `Mod+click → split with opposite
  mode`. Spec §1 `MarkdownView.onSwitchView` at `:1975`. Corbomite's
  hamburger menu has reading/source toggles but no first-class single
  button-icon swap that flips on click and splits on Mod+click.
- **`Mod+A` notice + `Mod+C` copy-all-source in Reading mode**. Spec
  §1.MarkdownPreviewView. Not implemented.
- **Touch double-tap toggles mode** in Reading. Mobile-only; out-of-scope
  for desktop, but worth flagging.
- **`Ctrl+Wheel` zoom of base font** clamped 10–30 px, written through
  `vault.setConfig("baseFontSize")`. Spec §1 line `:1637-1660`. No
  `wheelEvent` override that does this, and no `baseFontSize` config in
  the vault. Each leaf has its own zoom (`zoomIn`/`zoomOut`/`resetZoom`
  in `MarkdownView.cpp:86-138`) but not the chord-suppression / per-vault
  persisted base font size.
- **`strictLineBreaks` config** as a global parser option that re-renders
  every preview pane. Markoff's parser settings appear to be per-engine;
  the global-config wire is absent.

### Progressive (large-doc render)

- **Live preview itself does not progressively section-render.** Only
  Reading uses the `ReadingPipeline`. `Markoff::Editor` rebuilds its
  scene via `rebuildScene()` (declared at `Editor.h:369`) on every
  `MarkoffDocument::parseUpdated`. For >10 KB notes in Live mode the
  user gets a full-document re-layout, not section recycling. This is
  the largest-leverage gap: **a 10 000-line note will be sluggish in
  Live mode** even though Reading mode handles it well.
- **Section recycling pool integration with the live editor scene.**
  `ReadingView` has `SectionRecyclePool`; `Markoff::Editor` does not.
- **Selection-preserved-across-virtualisation** — spec §1
  `updateVirtualDisplay` always includes the Selection range so virt
  doesn't tear selection. Corbomite's `VirtualScrollController` mounts
  by viewport rect alone (no selection peek). For Reading-mode
  text-select-and-scroll, a long selection that crosses an unmounted
  band could tear.

### Lifecycle (plugin-side render-children)

- **`MarkdownRenderer.render(app, markdown, el, sourcePath, component)`
  static API.** No equivalent exists. Plugins / hover popovers / search
  snippet renderers / embed mini-renderers all need a
  "render-this-markdown-fragment-into-this-widget" verb. Today,
  `EmbedRenderer::renderMarkdown` is the closest thing but it only
  produces a `MarkdownRenderChild` carrying sliced raw markdown text;
  the consumer must then route it back through their own pipeline. The
  HoverPopover delegates through `EmbedRenderer::renderInto`
  (`EmbedRenderer.h:78`) — that **is** the closest functional equivalent,
  but it's name-scoped to embeds, not exposed as a generic "render
  arbitrary markdown into el" public API. The
  `console.error`-on-missing-`Component` plugin-stack-detection memory-leak
  guard from spec §1.MarkdownRenderer is therefore impossible to enforce
  because the API doesn't yet require a parent.
- **MarkdownRenderChild as a Component-equivalent.** Spec §1 says
  `MarkdownRenderChild extends Component` and that its descendants get
  `unload()` on recycle/close. Corbomite's `Markoff::MarkdownRenderChild`
  has `mountInto(QWidget*)` and a virtual destructor — but no child
  registry, no `addChild()`, no `onload()`/`onunload()` lifecycle hooks.
  When `SectionRecyclePool` reclaims a section, any plugin-attached
  resource gets the QObject-parent destructor — no explicit unload
  signal. **For dataview/templater-style plugins this means no clean
  way to flush async resources on recycle.**
- **`docId = cc(16)` per-render plugin-state key.** Not surfaced.
  Plugins persisting per-render state (Mermaid theme, MathJax label
  refs) have no anchor.
- **Unload-on-fold-of-ancestor-heading.** When a heading is collapsed
  in Reading, descendants get `setHidden(true)` but their
  `QGraphicsItem`s remain in the scene (`recomputeFoldVisibility`
  doesn't detach). This is OK for the current
  pure-text-and-image render set, but plugin-rendered children with
  active connections / timers continue to consume resources.

### Interactivity

- **Checkbox-click-to-toggle markdown round-trip** — already covered in
  Partial above. Reading: missing entirely. Live: glyph renders, key
  action documented broken.
- **Footnote-link click → scroll-to-target with flash highlight.** Not
  implemented (no footnote link handler that scrolls into view in
  Reading; spec §1.MarkdownPreviewRenderer "Section lookup" / "DOM
  event delegation").
- **Scroll-sync between linked panes.** Spec §1.syncScroll emits
  `markdown-scroll` event; `receiveSyncState` mirrors. No equivalent
  in Corbomite — `grep` for `markdown-scroll`/`syncScroll` returns
  nothing in active source. Cross-pane scroll-sync, the "linked panes"
  feature, is wholly absent.
- **Cross-pane `quick-preview` per-keystroke sync.** Spec §1 line
  `:2225` calls `workspace.onQuickPreview(file, data)` on every edit so
  a sister preview pane refreshes without disk save. Two leaves on the
  same NoteDocument *do* share the canonical buffer (so they update
  through `contentsChanged`), but there is no `quick-preview`
  workspace event for plugins to subscribe to.
- **In-document Find UI** is wired (the hamburger menu opens it) but
  Reading's adapter is stubbed (above).
- **Inline-title rename** at top of document (spec §1). Not present —
  the `View::title()` chain ends at the tab-bar label, not an inline
  contentEditable element.
- **`markdown-viewport-menu` event** (right-click on
  gutter/empty-viewport, plugins can contribute). No emission.

### Other (`MarkdownView` responsibilities not present)

- **Print-to-PDF link `href` strip.** Spec §12 says
  `MarkdownView.printToPdf` strips `href` from every `.internal-link`.
  Even when `setPdfExportTrigger` is wired by a host, no current code
  scrubs links — so a printed PDF with a wikilink would carry a
  potentially-misleading href.
- **Fold-info invalidates on line-count-change** — a sane direction
  Obsidian chose. Corbomite's `setFoldedHeadingLines(QVector<int>)` is
  line-indexed (`ReadingView.cpp:700-739`), but there is no guard that
  drops folds when the line count differs from the saved count. So an
  external edit that adds/removes lines could leave folds pointing at
  the wrong heading. (Worse than Obsidian: Obsidian silently drops, so
  user gets clean state; Corbomite carries them forward against new
  lines, so user gets *wrong* state.) See spec §8 invariant 11.

## Notable translation successes

1. **`{mode, source}` compound state preserved verbatim.** Corbomite's
   `ViewModeSerializer` writes `state.mode = "source"` plus
   `state.source = bool` (`MarkdownView.cpp:140-164`). A
   `workspace.json` from an Obsidian vault with all three modes round-
   trips into Corbomite's three `ViewMode` enum values bit-for-bit. The
   alternative — a single integer enum on disk — would have looked
   tidier in C++ but lost compatibility with vaults synced from
   Obsidian. The `Q_ENUM` retention plus the comment block at
   `NoteEditorWidget.h:42-46` documents the on-the-wire contract
   explicitly. Best-of-class.

2. **Capability-probe virtuals** (`hasCursor`/`hasEditing`/`hasFold`) on
   the abstract `MarkdownView` (`MarkdownView.h:66-68`). This is
   architecturally cleaner than Obsidian's "test the type and switch on
   `view.getMode()` everywhere" sprinkled across the call sites. The
   host writes `if (auto *leaf = activeLeaf(); leaf->hasFold()) leaf->setFoldedHeadings(...)`
   without RTTI.

3. **Frame budget constants externalised as named header values.**
   `kAsyncParseThresholdBytes`, `kFrameBudgetMs`, `kFrameBudgetSections`
   in `ReadingViewConstants.h:15,19,24` — with `tst_frame_budget_constants`
   pinning them per the comment "These numbers are not hints; they are
   the documented wire contract." This is rigour I'd not see in a
   typical Qt translation.

4. **Lazy-default fallback chain for injected dependencies.** The
   `LazyDefaults` struct (`ReadingView.cpp:57-63`) plus the
   `resolveOrDefault<Iface, Default>` helper (lines 198-208) means
   ReadingView is buildable standalone (no host injection) **and**
   correctly delegates to host-provided implementations when wired.
   Lets the standalone test app render real markdown without a fake
   vault.

5. **Section-pool recycle key as `QByteArray` shape, not raw HTML.**
   Avoids the spec's "whitespace-only edit invalidates the whole
   paragraph" trap (cf. spec §12 "Recycle key is HTML-string equality,
   not AST hash"). Whether this is intentional or accidental, the AST-
   shape recycling is more robust to whitespace edits than Obsidian's.

6. **Coalescing parse requests** via `m_requestIdCounter` /
   `m_lastRequestIdHandled` (`ReadingView.cpp:384-405`). Mirrors
   Obsidian's `parsing` flag + `200 ms` reschedule in spec §1.

7. **`MarkdownView::ephemeralState` as opaque JSON blob.** The Markoff
   abstract base treats per-leaf eState as an opaque `QJsonObject`
   serialised through `setEphemeralState`/`ephemeralState`
   (`MarkdownView.h:56-57`). Each leaf packs its own keys (Reading
   stores `{"scroll": float}` only; Live and Source pack cursor + scroll).
   This gives the host a single envelope per leaf without forcing the
   abstract base to know about every leaf's quirks.

## Notable concerns / suspected bugs

1. **`recomputeFoldVisibility` does not detach hidden sections from the
   scene** (`ReadingView.cpp:638-671`). It sets `setHidden(true)` on
   the section but does not remove the `QGraphicsItem` from the scene.
   Lines 712-720 call `m_controller->remountSection(i)` for hidden
   sections that have an item — but `recomputeFoldVisibility` itself
   doesn't trigger that path; it relies on `setFoldedHeadingLines`
   (line 711) and `toggleFold` (line 749) to call it after. A path
   that mutates `headingCollapsed` directly without going through
   those two would leave detached-but-visible items. (Probably no such
   path in current code, but it's a fragile invariant.)

2. **`setFoldedHeadingLines` does not invalidate folds when line count
   has changed.** Compare to spec §8 invariant 11. If a `.md` file
   edits externally and adds 10 lines above a folded heading, the
   stored line index now points at a different heading. Corbomite
   restores the fold against the wrong target.

3. **Reading mode `setCursorLine` is hard-coded false**
   (`NoteEditorWidget.cpp:259-261`). This means subpath link navigation
   into a `.md` opened in Reading mode silently fails. Anything that
   calls `view->setCursorLine(N)` (workspace state restore, search-
   navigate, embed-anchor jump) is broken in Reading mode. The
   matching no-op should at minimum scroll the section containing line
   N to the top.

4. **Live-preview leaf has a 1-based-vs-0-based cursor mismatch
   commented but partially compensated.** `NoteEditorWidget.cpp:289-292`
   subtracts 1 from line **and column** on save; `:323-324` adds 1
   back to line only and passes column through. Comparing capture vs.
   restore: column saves with `-1` (line 291), restores without `+1`
   (line 324). Off-by-one on column round-trip in Live mode. Test
   coverage: `tst_eState_round_trip` is the one to check (not read).

5. **No `markdown-scroll`/`quick-preview` workspace events.** Cross-
   pane sync depends on canonical buffer subscription, which works for
   content but not for scroll. Two split panes on the same `.md` will
   not scroll together; Obsidian's behaviour is that they do.

6. **`MarkdownView::getViewData` returns the raw markdown by reading
   `noteDocument()->markdown()` directly** (`MarkdownView.cpp:54-59`).
   Obsidian's `MarkdownView.getViewData()` returns `currentMode.get()`
   (preview mode's `.get()` returns `renderer.text`, source mode's
   returns the live editor text). Functionally equivalent because the
   canonical buffer is the source of truth, but if someone in the
   future has a leaf-local edit that hasn't propagated to the canonical
   buffer (e.g. an in-progress IME composition in Source), `getViewData`
   would return stale data.

7. **`setViewData` with `clear=true` drops the flag** — it is `Q_UNUSED`
   at `MarkdownView.cpp:66`. Obsidian's contract says when `clear` is
   true, the scroll position is nulled and **all** modes get
   `.set(data, true)` (spec §1.MarkdownView). Corbomite skips both.
   This means a `setViewData(text, true)` that should scroll to the
   top instead leaves scroll where it was.

8. **`getEphemeralState` is a TODO** (`MarkdownView.cpp:166-170`). The
   `Corbomite::MarkdownView` itself returns `{}` — meaning the ItemView
   layer's eState round-trip is broken. The `NoteEditorWidget`
   internally captures eState via `EphemeralState` struct, but
   `MarkdownView` doesn't bridge it through. So workspace-level
   "restore-this-leaf-with-this-scroll" doesn't actually pipe scroll
   into the leaf via the ItemView contract.

9. **Reading-mode wikilink hover position offset compensated by
   `+QPoint(0, 20)`** (`NoteEditorWidget.cpp:67-69, 220-222`). Spec
   §1.MarkdownPreviewView does not document this offset; it's a
   defensive +20 px to avoid the popover landing under the cursor
   triggering `leaveEvent`. Reasonable, but should be a constant /
   theme-token.

10. **Math/Mermaid post-processors register as code-block processors
    (`ReadingView.cpp:226-265`), but the math one renders pixmap at
    paint time** — not at code-block-process time. The processor only
    *probes* whether the source can render (`render()` returns `bool`)
    and the actual `QImage` is computed elsewhere in
    `ReadingMathObject` / `DisplayMath`. A plugin that wants to inspect
    the rendered output cannot — the bool is all it gets. Subtler than
    Obsidian's `(source, el, ctx) → Promise<void>` contract, which
    guarantees the plugin sees the rendered DOM.

## Plugin-extensibility risks

- **No `MarkdownRenderChild` child registry.** This is the load-bearing
  bug. Obsidian's
  `class extends MarkdownRenderChild { onload(){...} onunload(){...} }`
  pattern is the *primary* extension surface for any plugin that needs
  to attach DOM (or in our case, QObjects) for the lifetime of a section.
  Without it, plugin authors have no clean way to clean up subscriptions
  when a section is recycled. The current `Markoff::MarkdownRenderChild`
  is a fine *type*; what's missing is the framework around it: a
  parent registry, an `addChild()`, signals on mount/unmount.

- **No public `MarkdownRenderer.render(...)` static API.** Anything that
  wants to render a markdown fragment ad-hoc — hover popovers, search
  result snippets, plugin-supplied tooltips — has to either reach into
  `EmbedRenderer::renderInto` (semantically narrow), construct a fresh
  `ReadingView`, or roll their own. Obsidian's static API is the choke
  point that lets plugins compose; without it, every plugin re-implements
  partial markdown rendering and they will diverge. Highest-leverage
  missing item.

- **No post-processor registry.** `Markoff::CodeBlockProcessorRegistry`
  exists for fenced-code-block dispatch by language, but the
  generic `(el, ctx) → Promise<void>` post-processor surface is absent.
  The Obsidian spec (§7) lists `postProcessors: Function[]` (the
  sort-ordered general post-processor) and `recyclers: Function[]` —
  neither exists in Corbomite. So a plugin that wants to walk the
  rendered tree and (e.g.) decorate every `.tag` with a colour cannot.

- **No `post-processor-change` event.** Spec §5 says this fires on
  plugin (un)register and triggers a `rerender(true)` of every preview.
  Without it, plugin enable/disable doesn't refresh open notes.

- **No `hover-link` event with `source: "preview"|"editor"`.** The
  `HoverPopover` is host-side; plugins cannot attach their own preview
  widgets per source.

- **Frontmatter properties API.** `PropertiesView` is a plugin panel,
  not the API a third party would extend. The spec's `metadataEditor`
  (the YAML synchroniser) and the `propertiesInDocument` config gate
  are concepts that don't yet have a Markoff-side touch point.

- **`docId` per-render and `getSectionInfo(el)`** missing — plugins
  that need to know "which line range did this DOM element come from"
  have no API.

Summary: the *display* and *recycling* infrastructure is solid; the
*plugin hooks* into that infrastructure are mostly missing. Cluster Q
delivered the proxy plumbing for vault/file-manager but not for the
markdown-render pipeline. Until `MarkdownRenderer.render` + a
post-processor registry + a `MarkdownRenderChild` lifecycle land,
Corbomite cannot host real Obsidian-style markdown plugins.
