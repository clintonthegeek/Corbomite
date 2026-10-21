# Corbomite ↔ Obsidian Parity Matrix

> **Living document — established 2026-06-10** from a full code-verification audit
> (every status below was confirmed by reading the cited code, not docs).
> This replaces the **Corbomite-status columns** of
> [`obsidian-audit/FEATURE-MATRIX.md`](obsidian-audit/FEATURE-MATRIX.md) and the
> priority tables of [`obsidian-audit/GAP-ANALYSIS.md`](obsidian-audit/GAP-ANALYSIS.md),
> both frozen at 2026-04-14 (pre-foundation-port) and stale in **both directions**
> (they list shipped features as Missing and cite retired classes as implementations).
> The Obsidian-side behavioral content of the obsidian-audit corpus remains the
> compatibility target (verified ~95% accurate 2026-06-10; check
> `obsidian-audit/addenda/README.md` § Corrections first).
>
> **Update discipline:** when you land or verify a feature, update its row (status +
> evidence) in the same commit. Do not let this file become FEATURE-MATRIX 2.0 —
> dead rows are worse than no rows.

**Statuses:** ✅ DONE · 🟡 PARTIAL · ⭕ STUB (code exists, non-functional) · ❌ MISSING · ⚠ DIVERGENT (works differently; note if deliberate)

**Submodule pin note (updated 2026-10-21):** the markoff-family pin is now
`2c58088a` (the `v0.7.1` release tag; was `1c9b9415`, the tag's pre-push
commit, and `e0b96bf8` before that). The collabtext engine it pins is
`16cc48f` (`collabtext-perf-checkpoint-2026-10-21`), installed as a Release
build into `Markoff/build-collabtext-alpha5/install` — the prefix this build
resolves. The 2026-06-10 note below is retained only as history — **all
"post-pin" rows it referenced have long since arrived**, so no row in this file
should carry a *post-pin* marker any more:

> ~~the markoff-family pin `ddf5e9a8` is ~33 commits behind Markoff master. Rows
> marked *post-pin* are fixed upstream and arrive with the contract-v2 re-pin.~~

**2026-10-01 reconciliation pass.** This file was stale by ~11% of its substantive
rows after Clusters K/L/M/N/O/P and the v0.1.0/v0.2.0 releases. Corrected rows are
marked *(Row corrected 2026-10-01)* / *(Row split 2026-10-01)*. Report:
[`audit-2026-10-01-second-sweep-appraisal.md`](audit-2026-10-01-second-sweep-appraisal.md) §6.2.
**Lesson recorded:** the "update its row in the same commit" discipline below was
honoured for the Cluster-M/N/O canvas + clipboard rows and ignored everywhere
else. The rows that went stale were all in areas worked *before* the matrix was
established or *by clusters that didn't touch it*.

---

## 1. Vault & file operations

| Feature | Status | Evidence / notes |
|---|---|---|
| Vault open/load, tree build, `.obsidian`/`.trash` exclusion | ✅ | `libs/vault/src/Vault.cpp:78-105,634-678` |
| read/cachedRead/readBinary + BOM handling | ✅ | `Vault.cpp:166-211`; BOM stripped on read, preserved on readBinary |
| `modify`/`append`/`process` (atomic via QSaveFile, per-path mutex) | ✅ | `Vault.cpp:213-282`; `FileSystemAdapter.cpp:75-97` |
| **`saveDocument` — main editor save path** | ✅ | `51d62910` — routed through `m_adapter->writeBinary`; atomic temp-file-rename on crash. Test `tst_vault_adapter::saveDocument_routesThroughAdapterWriteBinary` |
| `modify()` reconciling open NoteDocuments | ✅ | `d9f1c102`+`ddfacc03` — `reconcileOpenDocument`: refresh-if-clean / signal-if-dirty via `externalReloadConflict`; test `tst_vault_modify_reconcile` |
| Path normalization (NFC etc.) | ✅ | `libs/vault/src/PathNormalization.cpp` |
| Collision naming ("Untitled N") | ✅ | **Fixed 2026-08-19 (`942411bc`)** — `collisionFreeName` starts at `n = 1` and checks collisions via `Vault::existsCaseInsensitive` (`libs/vault/src/FileManager.cpp:594-600`), matching Obsidian and staying portable across case-(in)sensitive filesystems. Test `tst_file_manager_newfile::collisionFreeNaming`. *(Row corrected 2026-10-01 — the row's own citation was a correction that had already been applied.)* |
| Rename + link rewrite (wiki/markdown/frontmatter) | ✅ | `FileManager.cpp:412-580`; 8-slot test. Gaps: percent-encoded URLs, `alwaysUpdateLinks` not consulted |
| Delete/trash (system → local fallback) | 🟡 | `Vault.cpp:563-632`; mode read from KConfig, **ignores vault `app.json` trashOption** |
| External watcher (create/modify/delete/rename) | 🟡 | `Watcher.cpp:120-207`; rename pairing by mtime only; folder-only events missed; O(vault) rescan per burst |
| Self-write echo suppression | ✅ | `Vault.cpp:902-917` mtime ledger + byte-compare |
| Vault.copy for folders | ❌ | `Vault.cpp:497-509` returns false (declared deferral) |

## 2. Metadata, links, cache

| Feature | Status | Evidence / notes |
|---|---|---|
| Links/embeds/tags/headings/blocks/footnotes extraction | ✅ | `MetadataParser.cpp:288-718`; 20-slot test. Sections cover 3 of 11 types |
| Link resolution algorithm (6-step, shortest-path) | ✅ | `LinkResolver.cpp:100-209`; 20 tests. GAP-ANALYSIS P0.1 long fixed |
| **LinkResolver freshness in-session** | ✅ | `4b44b255` — create/rename/delete → add/removeVaultPath; feeds non-`.md` attachments too; test `tst_mainwindow_link_resolver_freshness` |
| Subpath resolution (#heading, #^block) + parseLinktext | ✅ | `LinkUtils.cpp:83+`, markoff parser |
| resolvedLinks/unresolvedLinks aggregate maps | ⚠ | absent as API; SQLiteIndex `links` table serves backlinks/graph instead (deliberate divergence; plugin-API shape missing) |
| Cache events (changed/resolved/finished + debounce) | ✅ | `MetadataCache.cpp:316-435` |
| Cache persistence | ✅ | `41b4f537`+`7fdbb25b` — DBs now under `<AppLocalDataLocation>/index/<vault-id>/` via `PathUtils::vaultLocalDataDir`; legacy in-`.obsidian/` copies cleaned on open; tests `tst_mainwindow_db_paths`, `tst_path_utils` |
| BOM'd files in bulk index | ✅ | `1d7f477f` — BOM strip in `MetadataCache::rebuildVault`; eliminates hash-divergence/re-parse churn (`fromUtf8` already strips before parse, but explicit strip prevents churn); test `tst_metadatacache_bom` |
| Frontmatter parse tolerance | 🟡 | byte-0/`\r\n`/EOF cases OK. **The closing-fence EOL check landed upstream 2026-10-20** (`5c0e5c6a`, pin `e0b96bf8`) — the shared reader is now Obsidian's writer rule (a closer must be a complete `---` line), so `----`/`--- `/`---foo` no longer false-close. The host keeps one reader (upstream's) and the writer **refuses** a frontmatter write it can no longer splice rather than prepending a second `---` block; the properties panel is now exact against Obsidian's for both fence classes, at the cost of those notes' frontmatter tags/links leaving the search index. Addendum: `obsidian-audit/addenda/2026-10-20-strict-frontmatter-fence-and-the-writers-refusal.md` |
| **Blank-line round-trip fidelity** | ⚠ ACCEPTED LIMITATION | **Re-verified 2026-10-20 on pin `e0b96bf8`: the collapse no longer reproduces.** The D2 serializer the 2026-06-10 triage measured is retired — the vNext document keeps the source bytes and re-emits them, so a note with irregular blank-line spacing (and no trailing newline) now round-trips **byte-for-byte** through a save, and a text-only edit leaves the runs around it intact. Pinned by `tst_vault_save_reload::saveDocument_preservesIrregularBlankLineRuns` and `::saveDocument_preservesBlankLineRunsAroundAnEdit`. **Residual:** a *structural* edit (split a paragraph, toggle a list) re-serializes the file canonically — the runs in that file collapse to one blank line and a trailing newline is added. **Disposition 2026-10-20 (user): accept + document that residual too** — no content is lost, only the blank lines change, once, in that one file; the common case (typing) no longer touches them at all. The 2026-06-10 decision was the same disposition for the wider problem (accept + document, not a defect), and the release criterion "no unexplained diffs" is amended to treat blank-line normalization as an expected, documented diff. Steering Markoff for spacing preservation across structural edits remains available as a future enhancement, but it cannot affect the first public cut. See [`handoff/2026-06-10-blank-line-collapse-triage.md`](handoff/2026-06-10-blank-line-collapse-triage.md). |
| processFrontMatter (ordered, append, strip-empty) | ✅ | `FileManager.cpp:308-358`; 7-slot test |
| YAML re-emission byte format | ⚠ | ryml emit ≠ Obsidian js-yaml style; every Properties edit reformats the block; no byte-format test |
| `.obsidian/*.json` read/write + unknown-key preservation | ✅ | `VaultConfig.cpp` + round-trip test. `saveConfig()` partitions `app.json`/`appearance.json` on Obsidian's **16-key** allow-list (2026-10-04) so a key cannot land in the wrong file; `.obsidian/<corePluginId>.json` added as a config class with a traversal guard; `hotkeys.json` now consumed by `HotkeyDispatcher` |

## 3. Editor (Markoff three-leaf host)

| Feature | Status | Evidence / notes |
|---|---|---|
| Three modes (Live/Source/Reading) + Ctrl+E + wire-format `{mode, source}` | ✅ | `NoteEditorWidget.cpp:175-303`; `ViewModeSerializer.cpp` |
| **Link click → navigate** | ✅ | `88ad1b46`+`8c9d8c8d` — shared `DefaultLinkService` wired to Live binding + Styled Reading leaf; MainWindow resolves via LinkResolver → navigates; create-on-click for missing targets; tests `tst_link_activation`, `tst_mainwindow_link_navigation` |
| `[[` note-name completion | ✅ | completion revival A1–A3 (spec `docs/superpowers/specs/2026-06-11-completion-revival-design.md`): `CompletionController` driven by real caretRect across all editable leaves; `WikiLinkSuggest` names ✅ + aliases ✅ (A2) + `[[note#` headings ✅ (A2, resolved-target `HeadingCache` lookup) + `[[note#^` existing-block ids ✅ (A3); block-id *creation* on pick deferred (follow-up — punch-list P3); tests `tst_wikilink_suggest`, `tst_completion_controller`, `tst_note_editor_widget_completion` (live-leaf e2e) |
| `#` tag completion | ✅ | completion revival A1 (same spec); `TagSuggest` v2; `tst_tag_suggest` |
| LivePreview: headings/lists/tables(+cell edit)/code(+highlight)/checkbox toggle | ✅ | **Engine changed 2026-08-18 (Cluster K Phase 5)** — this row used to describe `markoff-live` QML delegates, which are **no longer linked into Corbomite** (`MARKOFF_BUILD_LIVE=OFF`). LivePreview is now solely `Markoff::Canvas::EditorWidget`. *(Row corrected 2026-10-01 — the capability is intact, the engine attribution was stale.)* |
| Reading (styled): tables | ✅ | re-pin `8112833f` (past styled-table arc, outside the `8c13c5d..079ac1f` window); renders real `QTextTable` grids |
| Reading: checkbox toggle, code highlight, math | ❌ | styled leaf is no-KF6. Checkbox-click-to-toggle is **scoped upstream to markoff-styled** (needs a click handler alongside the existing `LinkInteraction` `cursorForPosition()` filter, which already owns click→model-block resolution — doing it Corbomite-side would reimplement that mapping and break over code blocks/tables). Awaiting a Markoff spec + falsifiable test, then a re-pin + Reading-leaf wire. |
| Callouts | ✅ | shipped in markoff-canvas P5.x (`20949498`), adopted/live-load-verified 2026-08-19 (`355c6eb5`) — `BlockPresentation::presentationFor` content-sniffs `> [!type]` on any `BlockQuote` block via `CalloutBlocks::parseCallout`, typed icon+label header + `Theme::Slot::Callout*` colors. Insert Callout dialog is still a placebo (`MainWindow.cpp:543-567`) |
| Mermaid | 🟡 | canvas seam wired 2026-08-19 (`1a516990`) via `CanvasMermaidAdapter` (wraps `Corbomite::Core::MermaidRenderer`/mmdr, rasterizes via `QSvgRenderer`) — renders, but dark-theme SVG output is still light-only (punch-list, still open) |
| Footnotes / embeds `![[..]]` transclusion | ❌ | embed image-node parse fix landed upstream (`9a6a6b74`, Phase 1); canvas `EmbedRegistry` seam wired 2026-08-19 (`1a516990`, `hasExtension()` feeds the placeholder label only) but no `EmbedFactory` is registered for any extension and the canvas leaf never calls `dispatch()` — transclusion *rendering* still absent. Footnotes unverified |
| Images in canvas LivePreview | ✅ | shipped 2026-08-19 (`1a516990`) — `EditorWidget::setImageResourceLookup` wired to `VaultResourceProvider::loadImageBytes`, `QPixmap::loadFromData` + explicit `QSvgRenderer` fallback for `.svg` |
| Format verbs (B/I/strike/code/link/heading) | ✅ | base dispatch via `addEditorActionBase` (`20abc25a`); enabled-state from `hasEditing()` + heading radio from `contextChanged` (`d98c7abd`) |
| Undo/redo | ✅ | all leaves via `MarkdownView` base → `undoD2` (`b5a4b041`); Source dual-stack divergence retired |
| Find in note | ✅ all modes | base `attachFindController` (`bfa2fa16`); in-table matches counted but not painted (Markoff brief §3, known v1 limit) |
| Replace | ✅ | **Shipped 2026-06-12 (`38022056`)** — `KStandardAction::find`/`replace`/`findNext`/`findPrev` bound at `src/app/MainWindow.cpp:1319-1322`; `onFind`/`onReplace` (`:621-652`) drive a real `FindBar` with `replaceRequested`/`replaceAllRequested` (`src/editor/FindBar.cpp:93,95`). One-undo replacement via upstream `MarkoffDocument::replaceMatches`. *(Row corrected 2026-10-01 — it previously said "no UI … connected to nothing", which was true until 2026-06-12 and then never updated.)* |
| Hover preview | ✅ | Re-lit 2026-06-11, eyeball-confirmed 2026-06-12: `HoverPopover` renders the target note via `StyledRenderEngine`+`VaultResourceProvider` into a `QTextBrowser`; trigger wired through the shared `LinkService`. **Reading = plain hover; Live = Ctrl-then-hover** (`LiveView.qml` gates emission on Ctrl — Obsidian-faithful, kept by user decision). Subpath (`#heading`/`#^block`) slicing deferred |
| Word count / Ln,Col statusbar | ✅ | Fixed 2026-06-11 (`fb47120e`) — `NoteEditorWidget` seeds + re-emits word count off `NoteDocument::textChanged`; Ln/Col live in all modes via base `cursorPositionChanged` (`ad0729e7`) |
| goToLine / ephemeral state (cursor+scroll persist) | ✅ | contract-v2 `CursorPos` + 0.0–1.0 scroll fraction via base (`5d7fcc5e`); Live attach-window writes fixed upstream (`8112833f`) |
| Theme propagation to leaves | ✅ | `applyThemeToAllLeaves` + `wireLeaf` at lazy construction (`17b2cd00`) |
| Templates / daily notes | 🟡 / ✅ | **Template-at-cursor fixed 2026-06-11 (`082589ae`)** — `insertTemplate` now calls `NoteEditorWidget::insertAtCursor`, applying the template as one undo-integrated D2 `applyFlatEdit` at the active leaf's caret (propagates to every leaf); `{{cursor}}` positions the caret. Bridges are pure + unit-tested (`LineResolve::globalByteOffsetForCursor`, `caretAfterFlatInsert`; tests `tst_line_resolve`, `tst_note_editor_widget_insert_at_cursor`). Daily notes wired. **Residual:** multi-block templates still land as flat `Paragraph` blocks — `applyFlatEdit` creates every new block as `BlockKind::Paragraph` with no kind re-parse (`MarkoffDocument.cpp:1589,1639,1659`), so a template containing `# Heading` or a fenced code block displays as literal markdown until reload (not corruption — it re-parses correctly on next load). Punch-list P3. *(Row corrected 2026-10-01.)* |
| Paste/drop images, paste HTML→MD | 🟡 | Cluster N CLOSED on `feature/rich-clipboard` (N5 passed, Markoff `bafa3095`). HTML/RTF↔MD + Copy as on all leaves; LO Block Quote round-trip confirmed. Image paste/drop still absent. Plan: [`superpowers/plans/archive/2026-08-20-cluster-n-rich-clipboard.md`](superpowers/plans/archive/2026-08-20-cluster-n-rich-clipboard.md) |
| Vim mode / spellcheck / RTL | ❌ | absent entirely (explicitly post-1.0 candidates) |
| Zoom/font scale | ✅ | all leaves via base `setFontScale` (`c6a7afad`); steps match Live's Ctrl+= shortcuts (1.10) |
| Autosave | ✅ | `AutosaveReactor` (2 s debounce) → saveDocument (see P0 above) |

## 4. Workspace, UI, settings

| Feature | Status | Evidence / notes |
|---|---|---|
| Tabs: open/close/drag/split/undo-close | ✅ | KDDW; `Workspace.cpp:409-544`; Ctrl+Shift+T cap 10. **2026-10-17 soak:** between-group drag works through drop targets, not by dropping on the tab bar. Undo restores into a pane that is still open, at the end of the row, with view state. Closing the last tab collapses the pane and undo does not bring the split back (punch-list `[cluster-l]`). |
| Pin tabs | ✅ | **Shipped, Cluster L Phase L4 D3** — `tab_pin_toggle` KAction at `src/app/MainWindow.cpp:1509-1518` → `leaf->setPinned` + `propagatePinToGroup`; state tracked at `src/app/ActionContextController.cpp:339`; serialized at both levels (`WorkspaceSerializer.cpp:214`, per the 2026-06-10 addendum). Residual: pinned tabs are still recycled by `openLinkText`. *(Row corrected 2026-10-01.)* |
| Tab history back/forward | ✅ | Cluster L Phase L4, live-verified 2026-08-18 — `navigateActiveLeafTo` calls `leaf->navigate()`; Ctrl+Alt+←/→ + mouse buttons 4/5 + tab-frame nav buttons all wired to `goBack()`/`goForward()`; enablement tracks `LeafHistory::canGoBack/canGoForward` per active leaf |
| Stacked tabs | 🟡 | **Toggle shipped** (Cluster L4, `tab_toggle_stacked` at `src/app/MainWindow.cpp:1537-1549`, advisory-only by design decision); flag round-trips. **Still no visual treatment** — the stack isn't rendered distinctly. *(Row corrected 2026-10-01.)* |
| Popout windows | 🟡 | `popoutLeaf` via drag-out **plus** the `tab_move_to_new_window` command (`src/app/MainWindow.cpp:1521-1528`); **persistence shipped in Cluster L2** — `floating` is written and, on 2026-10-17, round-tripped through Obsidian. Closing the popout closes the leaf; dragging it back docks the leaf and closes the empty window. **A restored multi-pane float shows tab titles and no view until a pane is dragged** (punch-list `[cluster-l]`). *(Row corrected 2026-10-17.)* |
| workspace.json fidelity | ✅ | Cluster L2 — `MainWindow::saveSessionState` routes Tier 1 through `Workspace::writeWorkspaceJson` (full main/active/floating/lastOpenFiles + unknown-key passthrough). **2026-10-17 soak:** a three-row Obsidian rearrange and a side-by-side popout restored in Corbomite. `dimension` is still absent (punch-list P3). One quit wrote an empty `tabs` node the loaded file did not contain; Obsidian dropped it (punch-list `[cluster-l]`). A missing `workspace.json` opens two panes, one titled with a raw leaf id. |
| Sidebars (KateMDI tool views) | ✅ | **Persistence shipped, Cluster L2 + L4** — width restore is a live-verified L4 deliverable (user-confirmed 2026-08-18). `MainWindow.cpp:804-806` saves the real `sidebarWidth()` (`CorbomiteMDI.h:439-441` → `Sidebar::lastSize()`, maintained live at `CorbomiteMDI.cpp:1348`); restore wired at `MainWindow.cpp:2426-2429`. *(Row corrected 2026-10-01 — the old text described the pre-L4 state and the "hardcoded 200" no longer exists.)* |
| Ribbon | ⚠ | top KToolBar vs Obsidian's left vertical strip; hiddenItems persistence works, no UI to hide items |
| Command palette | 🟡 | KCommandBar over ~70 KActions (many disabled stubs) + ~13 registry commands; Obsidian lists hundreds |
| Quick switcher | 🟡 | faithful fuzzy scorer (`FuzzyMatcher.cpp`); filename-only — no path/alias match, hidden create flow |
| Hotkeys | ✅ | **Dispatcher shipped 2026-10-04** — `HotkeyDispatcher` (load/bindAction/install/bake/reconcile/dispatch) makes a vault's `hotkeys.json` reach real shortcut handling, and `[]` **unbinds** with no default fallback. Reconciles on external edits via `Vault::configChanged`. Scope stack keeps the flat walk plus an explicit root slot gated on `activeModalWidget`, reproducing Obsidian's "modal first, command table last" ordering *without* its bug (its `Modal` allocates a parentless Scope, so the root catch-all is unreachable while any modal is up). `code` bindings round-trip verbatim; spellings Qt cannot express are refused and reported, never coerced. 22 cases. Residual: no `repeatable` support, no user-visible rendering of a code-only binding |
| Obsidian core command-id coverage | 🟡 | **232** Obsidian core command ids exist (115 app + 42 `Workspace.registerCommands` + 75 plugin/mobile); Corbomite registers **30**. Full generated table: [`obsidian-command-ids.md`](obsidian-command-ids.md), regenerated by `RE/tools/extract-command-ids.js` — cross-validated by reproducing 41 hotkey-bearing ids whose bindings match the hand-written default table 41/41. The 7 previously-inert vault hotkeys (`backlink:open`, `outline:open`, `outgoing-links:open`, `properties:open`, `graph:open-local`, `bookmarks:open`, `file-explorer:reveal-active-file`) now resolve via alias. ~92 remaining are `editor:*`, mostly `editor:table-*`/`editor:set-heading-*`, which need Markoff's table model rather than Corbomite code |
| Settings dialog | 🟡 | Editor/Files/Appearance/Daily-notes/Hotkeys/Plugins pages; missing Files&Links prefs, accent color, fonts, CSS snippets, About |
| Themes | ⭕ | `ThemeService` body is `#if 0` (`libs/core/src/ThemeService.cpp:7-109`). **Escalated to punch-list P1 2026-10-01** — the situation is worse than this row implied: because the only `emit themeChanged` (line 104) is inside the dead block, the whole theme chain never fires, so the Settings theme dropdown renders **empty and inert**, and **opening an Obsidian-authored dark vault does not switch Corbomite to dark** (the `appearance.json` read calls a no-op setter). Five live consequences are itemised in the punch list. `NoteEditorWidget::applyThemeToAllLeaves` still works, but only by reading `currentTheme()` once eagerly — which derives light/dark from the app palette as a stopgap, not the theme port. No CSS theme import. *(Row corrected 2026-10-01.)* |
| File explorer | 🟡 | open/rename/delete/new-note + expansion persistence; no new-folder, drag-move, duplicate, reveal, sort |
| Backlinks / outlinks / outline / properties panels | 🟡 | flat lists; no unlinked mentions, no context snippets; properties panel is the strongest |
| Bookmarks | 🟡✅ | closest to parity: all types + subpaths + unknown-type round-trip |
| Tags pane / fullscreen / Mod+1..9 | ❌ | Tags pane **absent entirely** — `grep -rln "TagPane\|TagsPane\|TagsView"` over `src/`+`libs/` returns zero hits. It is named in the roadmap as "the one wholly-missing core panel" and in a release criterion, but had **no punch-list item and no cluster** until filed 2026-10-01. (`TagSuggest` — `#`-tag completion — exists and is unrelated.) Fullscreen + Mod+1..9 also absent. |
| New-note flow | ⚠ | modal name dialog vs Obsidian's inline "Untitled N" |
| Multi-vault | ⚠ | single window, switch closes current (deliberate for now) |

## 5. Search, graph, canvas, bases

| Feature | Status | Evidence / notes |
|---|---|---|
| Search DSL parse (path/file/content/tag, phrases, regex, OR, -, parens, case) | ✅ | `SearchDSL.cpp`; quirk-faithful |
| DSL backend honor | 🟡 | FTS5 + post-filters; `line:/block:/section:/task*:` parsed but unsupported; bareword is exact-token not substring (⚠) |
| Search results UI | 🟡 | **one snippet per match site since 2026-10-21** — FTS5's `snippet()` gives one window per *note*, so a note with three matches used to come back as one result; the sites now come from `highlight()`, which marks every match in the column, and each site gets its own snippet (`SQLiteIndex.cpp`). The panel side already supported it: `SearchResultsModel` groups by path with a `QVector<SearchMatch>` per group, so the status line now reads "3 matches in 1 file" instead of always "1 matches in 1 file". **Still open:** no history/sort/copy/context toggle. |
| Embedded `query` blocks | ❌ | absent |
| Quick-switcher fuzzy scoring parity | ✅ | penalty rule matched 2026-10-03; `tst_fuzzy_penalty_rule` pins 8 exact score pairs ([addendum](obsidian-audit/addenda/2026-10-03-search-dsl-grammar-and-quick-switcher-scoring.md)) |
| Global graph (layout, sliders, orphans, hover, ctx menu) | ✅ | `libs/forcegraph` + `GraphViewTab.cpp`; solid |
| Graph: tag/attachment filters, color groups, screenshot | ❌ | TODOs in `GraphControlsPanel.cpp:79-80` |
| graph.json persistence | ❌ | nothing persisted; sliders reset every session |
| Local graph | 🟡 | depth hardcoded 2 (`src/plugins/local-graph/LocalGraphView.cpp:69` — note: the file **moved out of `graph-view/` into its own plugin dir**), zero controls panel (unlike the global graph). |
| .canvas round-trip (unknown fields, defaults, V5 self-heal, rounding) | ✅ | **unknown node types and dangling edges now survive verbatim** (`NodeType::Unknown` + `rawData`); edge key order `id,fromNode,fromSide,toNode,toSide`; `Math.round` half-rounding; new-card default 100×100 (was 250×60). Byte-exact via `CanvasJsonPrinter`. `tst_canvas_diskcontract` (24 cases) — fixed 2026-10-03 |
| Canvas node/edge array order | ✅ | **Fixed 2026-08-19 (`a3689732`)** — `m_nodeOrder`/`m_edgeOrder`. **Extended 2026-10-03** — `nodes` is now explicitly a **z-order**: `m_nodeZIndex` is applied in `toOrderedJson()`, so an untrusted file with `zIndex` set round-trips its stacking. Edges keep insertion order. `tst_canvas_diskcontract::nodesArePaintOrderAndEdgesKeepInsertionOrder`. *(Row corrected 2026-10-01 and 2026-10-03.)* |
| Canvas: text/file/group nodes, edge render/label/arrows, export image | ✅ | scene + `CanvasFileView.cpp:82-162` |
| **Canvas: edge creation UX** | ✅ | **Shipped, Cluster M Phase M3 (live-verified 2026-08-19)** — `Graffodil::CreateEdgeTool` instantiated at `libs/canvas/src/CanvasScene.cpp:83` plus a bespoke `CanvasEdgeGestureTool` dispatcher for endpoint reconnect. Draw/label/reconnect/redirect/delete all user-confirmed. *(Row corrected 2026-10-01 — it said "never instantiated", true until 2026-08-19.)* |
| Canvas: file-card path resolution | ✅ | **Fixed 2026-08-19 (`95779618`)** — added `NoteDocument::vaultRoot()`, threaded through `CanvasFileView::onLoadFile` into `CanvasViewTab`, so cards resolve against vault root (falling back to the canvas file's own dir). Test `tst_canvasviewtab_vaultroot_paths`. *(Row corrected 2026-10-01.)* |
| Canvas: edge color, z-order, zoom-to-selection | ✅ | **All shipped.** edge color: `libs/canvas/src/EdgeItem.cpp:47` (`colorFromCanvasColor`); z-order by `-width*height` (`GroupItem.cpp:30`, M4.3); zoom-to-selection: `CanvasView.cpp:88` + wired KAction `canvas_zoom_to_selection` (`CanvasViewActions.cpp:241`, Cluster O O4 — user live-eyeballed). *(Row split 2026-10-01.)* |
| Canvas: image/link nodes, canvas read-only toggle | 🟡 | **Read-only toggle shipped 2026-10-04 (M5.4)** — per-file, machine-local tier-3 store, mirroring Obsidian's `localStorage["canvas-"+path]`; gates resize, Alt-drag, context-menu mutations, inline edits, nudge, cut, undo/redo. Pan/zoom/select still work. **Obsidian gates *less* than we first believed** (not drop, not resize, not undo) — we match it rather than exceed it. Image/link nodes still absent: `CanvasScene.cpp` `case NodeType::Link: break;`. *(Row split 2026-10-01; read-only half closed 2026-10-04.)* |
| Canvas card fidelity | 🟡 | StyledRenderEngine: 6 block kinds; no tables/images/math. **Re-verified 2026-10-20 on the v0.7.1 pin `1c9b9415`:** the canvas leaf's display-math carve-out does not change what we render — our import path already classifies a `$$`-leading block as `BlockKind::Math` (`KindInference::inferBlockKind`), so the carve-out's `Paragraph` case is unreachable for us. The leaf's brush-leak fix (`f68a37c5`) *does* reach the note editor's live preview, which hosts the leaf's `EditorWidget` in non-embedded mode; pinned by `tst_canvas_table_brush`. |
| Canvas: convert text card to file | ✅ | **Shipped 2026-10-04 (M5.5)** — compound `QUndoCommand` (add file card + reconnect each edge + remove text card), so one Ctrl+Z undoes the whole conversion. Card text becomes the file body verbatim (no embed/frontmatter); pos+size preserved. Undo deliberately does not delete the created note (matches Obsidian). `tst_canvas_convert_to_file*` (17 cases) |
| Canvas: jump to group | ✅ | **Shipped 2026-10-04 (M5.5)** — fuzzy group-label list → `zoomToBbox` porting Obsidian's formula incl. the 1.1 margin and log2 [-4,1] clamp. Instant, not eased: M5.1's animation system has not landed and was not invented. `tst_canvas_jump_to_group` (8) |
| Canvas viewport persistence | ✅ | **Shipped 2026-10-04 (M5.3)** — `{x,y,zoom}` in `ViewState.state.viewState`, **not** `eState` (Obsidian's canvas `getEphemeralState()` returns `{}` and eState is never serialised). zoom = log2 of scale clamped [-4,1]; x/y = scene point at viewport centre. Restores to 0.5 device px on x/y because QGraphicsView pans by integer scrollbar value |
| Canvas read-only lock | ✅ | **Shipped 2026-10-04 (M5.4)** — see the read-only row above. `tst_canvas_readonly` (12) + `tst_canvas_readonly_store` (6) |
| .base YAML round-trip + unrecognizedData | ✅ | `BasesQuery.cpp`; `tst_yaml_schema` (35 cases). **Fixed 2026-10-03** — property-id canonicalisation now matches Obsidian's `hQ` on exactly the four key groups it applies to (`order`/`sort`/`groupBy`/`summaries`) while `properties:` keys stay as authored; `not:` confirmed NOR; `summaries`/`formulas` empty-guard asymmetry matched; dead `unrecognizedData` removed from `BasesViewConfig` (views use `.data`) |
| Formula DSL (16 globals, ~58 typed methods, lambdas) | ✅ | `Builtins.cpp:42-196`, `Evaluator.cpp` |
| Table view: group/sort/filter/summaries/edit/undo/+New/export/columns | ✅ | Cluster D; **all UI pending human eyeball** |
| Built-in summaries | ✅ | **All present 2026-10-21.** Range/Earliest/Latest/Checked/Unchecked/Empty/Filled were missing (`BasesQueryResult.cpp:177-187`). The semantics are Obsidian's, from the corpus (addendum `2026-10-03`): the defaults offered depend on the column's inferred class, and the formulas behind them are `values.max() - values.min()` / `(values.latest() - values.earliest())`, `values.filter(value == true).length`, `values.filter(value == false).length`, `values.filter(value.isType("null")).length`, `values.filter(!value.isType("null")).length`. Implemented as five aggregates on `ListValue` next to `sum`/`min`/`max`; the counts use `Value::staticLooseEquals` — exactly what `==` evaluates to — so a native count and Obsidian's formula cannot drift, and the aggregates are not exposed as formula methods, so the formula surface stays Obsidian's. `Range` sniffs the column class from its first non-Null entry, as Obsidian does. Pinned by `tst_value_list` and `tst_bases_entry`. **One divergence stands:** Obsidian offers the defaults *per column class*; our picker offers the union in one flat list, which is how this picker has always been. |
| Cards view / embedded ` ```base ` blocks | ❌ | table-only; no note embedding |
| Icon/Image/markdown cell rendering | ❌ | no paint path in delegate |

## 6. Plugin system (deprioritized for first release)

Proxy/registry surface (11 registrars), permissions, manifest+data.json lifecycle,
external-settings watch: ✅ and ahead of schedule (Cluster B). Editor plugin API
(Cluster E): needs re-scope against D2. Bases plugin API (D.5): unspecced.
Example plugins compile against current API (verified 2026-06-10).

---

## The eight headline holes — **all 8 now closed or accepted** (reconciled 2026-10-01)

> This section was written 2026-06-10 as a to-do list and read as one for two
> months after it stopped being true. **All 8 are now closed or explicitly
> accepted**; item 7 remains an accepted, documented interop difference rather
> than a defect. Kept as the dogfooding entry-point history.

1. ~~**Link clicks do nothing** in every mode (P0, wiring-only fix).~~ **FIXED `88ad1b46`+`8c9d8c8d` (Phase 0)**
2. ~~**No completion** for `[[` or `#` — typed blind.~~ **FIXED — completion revival A1–A3 (`[[` names + `#` tags, all editable leaves); aliases + `[[note#` headings (A2); `[[note#^` existing-block ids (A3). Block-id *creation* on pick deferred (follow-up).**
3. ~~**Editor save path can truncate notes on crash** (P0, ~5-line fix).~~ **FIXED `51d62910` (Phase 0)**
4. ~~**No back/forward navigation** despite a complete LeafHistory engine.~~ **FIXED — Cluster L Phase L4, live-verified 2026-08-18.**
5. ~~**Reading mode is a downgrade** (no tables until re-pin, raw math, inert checkboxes).~~ **FIXED by the contract-v2 re-pin recorded in the row above this list** — Reading is a styled `Markoff::Styled::Editor` leaf (`775fa54e`, 2026-05-29) with `StyledRenderEngine` backing it (`e7a40ae2`, 2026-05-30). Residual: checkbox-click-to-toggle is still upstream-blocked (punch-list, scoped to markoff-styled).
6. ~~**Status bar lies** (Words: 0 forever)~~ **FIXED `fb47120e` (2026-06-11).** *The sidebar-amnesia tail is also fixed* — Cluster L2 + L4 shipped real width persistence, live-verified 2026-08-18. (Row corrected 2026-10-01.)
7. **Blank-line normalization on save** (2+ blank-line runs → 1, Markoff-owned, intentional per B1 §2 — **accepted + documented** 2026-06-10, not corruption; see triage doc). *Re-verified 2026-10-20: the D2 serializer that caused it is retired, so a text-only save now round-trips byte-for-byte; only a structural edit re-normalizes. Still open as a decision, not a defect.*
8. ~~**Canvas can't create edges**; subfolder canvases render empty file cards.~~ **BOTH FIXED** — edge creation shipped in Cluster M Phase M3 (`bfec72c8`, live-verified 2026-08-19); file-card path resolution fixed `95779618` (2026-08-19). *(Row corrected 2026-10-01.)*

The matching fix sequence lives in
[`superpowers/plans/2026-06-10-road-to-dogfood.md`](superpowers/plans/2026-06-10-road-to-dogfood.md).
