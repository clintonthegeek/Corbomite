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

**Submodule pin note (2026-06-10):** the markoff-family pin `ddf5e9a8` is ~33 commits
behind Markoff master. Rows marked *post-pin* are fixed upstream and arrive with the
contract-v2 re-pin (see the adoption brief).

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
| Collision naming ("Untitled N") | ⚠ | `FileManager.cpp:583-597` starts at 2, case-sensitive; Obsidian starts at " 1" (addendum 2026-06-10) |
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
| Frontmatter parse tolerance | 🟡 | byte-0/`\r\n`/EOF cases OK; closing-fence `\n---` matched without EOL check (false-close on `----`) — Markoff-owned (steered `69162236`+`7a87daf7`) |
| **Blank-line round-trip fidelity** | ⚠ ACCEPTED LIMITATION | Markoff D2 serializer collapses consecutive blank-line runs (2+→1) on save — intentional (B1 spec §2). **Decision 2026-06-10: accept + document** (not a defect); release criterion "no unexplained diffs" amended to treat blank-line normalization as an expected, documented diff. See [`handoff/2026-06-10-blank-line-collapse-triage.md`](handoff/2026-06-10-blank-line-collapse-triage.md). |
| processFrontMatter (ordered, append, strip-empty) | ✅ | `FileManager.cpp:308-358`; 7-slot test |
| YAML re-emission byte format | ⚠ | ryml emit ≠ Obsidian js-yaml style; every Properties edit reformats the block; no byte-format test |
| `.obsidian/*.json` read/write + unknown-key preservation | ✅ I/O · ⭕ consumption | `VaultConfig.cpp` + round-trip test; **but `app.json` and `hotkeys.json` are read by nothing** |

## 3. Editor (Markoff three-leaf host)

| Feature | Status | Evidence / notes |
|---|---|---|
| Three modes (Live/Source/Reading) + Ctrl+E + wire-format `{mode, source}` | ✅ | `NoteEditorWidget.cpp:175-303`; `ViewModeSerializer.cpp` |
| **Link click → navigate** | ✅ | `88ad1b46`+`8c9d8c8d` — shared `DefaultLinkService` wired to Live binding + Styled Reading leaf; MainWindow resolves via LinkResolver → navigates; create-on-click for missing targets; tests `tst_link_activation`, `tst_mainwindow_link_navigation` |
| `[[` note-name completion | ✅ | completion revival A1–A3 (spec `docs/superpowers/specs/2026-06-11-completion-revival-design.md`): `CompletionController` driven by real caretRect across all editable leaves; `WikiLinkSuggest` names ✅ + aliases ✅ (A2) + `[[note#` headings ✅ (A2, resolved-target `HeadingCache` lookup) + `[[note#^` existing-block ids ✅ (A3); block-id *creation* on pick deferred (follow-up — punch-list P3); tests `tst_wikilink_suggest`, `tst_completion_controller`, `tst_note_editor_widget_completion` (live-leaf e2e) |
| `#` tag completion | ✅ | completion revival A1 (same spec); `TagSuggest` v2; `tst_tag_suggest` |
| Live: headings/lists/tables(+cell edit)/code(+highlight)/checkbox toggle | ✅ | markoff-live delegates; `TableEditBinding` |
| Reading (styled): tables | ✅ | re-pin `8112833f` (past styled-table arc, outside the `8c13c5d..079ac1f` window); renders real `QTextTable` grids |
| Reading: checkbox toggle, code highlight, math | ❌ | styled leaf is no-KF6; math Live-only (`MathRenderer.cpp`) |
| Callouts | ✅ | shipped in markoff-canvas P5.x (`20949498`), adopted/live-load-verified 2026-08-19 (`355c6eb5`) — `BlockPresentation::presentationFor` content-sniffs `> [!type]` on any `BlockQuote` block via `CalloutBlocks::parseCallout`, typed icon+label header + `Theme::Slot::Callout*` colors. Insert Callout dialog is still a placebo (`MainWindow.cpp:543-567`) |
| Mermaid | 🟡 | canvas seam wired 2026-08-19 (`1a516990`) via `CanvasMermaidAdapter` (wraps `Corbomite::Core::MermaidRenderer`/mmdr, rasterizes via `QSvgRenderer`) — renders, but dark-theme SVG output is still light-only (punch-list, still open) |
| Footnotes / embeds `![[..]]` transclusion | ❌ | embed image-node parse fix landed upstream (`9a6a6b74`, Phase 1); canvas `EmbedRegistry` seam wired 2026-08-19 (`1a516990`, `hasExtension()` feeds the placeholder label only) but no `EmbedFactory` is registered for any extension and the canvas leaf never calls `dispatch()` — transclusion *rendering* still absent. Footnotes unverified |
| Images in canvas LivePreview | ✅ | shipped 2026-08-19 (`1a516990`) — `EditorWidget::setImageResourceLookup` wired to `VaultResourceProvider::loadImageBytes`, `QPixmap::loadFromData` + explicit `QSvgRenderer` fallback for `.svg` |
| Format verbs (B/I/strike/code/link/heading) | ✅ | base dispatch via `addEditorActionBase` (`20abc25a`); enabled-state from `hasEditing()` + heading radio from `contextChanged` (`d98c7abd`) |
| Undo/redo | ✅ | all leaves via `MarkdownView` base → `undoD2` (`b5a4b041`); Source dual-stack divergence retired |
| Find in note | ✅ all modes | base `attachFindController` (`bfa2fa16`); in-table matches counted but not painted (Markoff brief §3, known v1 limit) |
| Replace | ❌ | no UI; hamburger Find…/Replace… actions connected to nothing (`MarkdownView.cpp:298-312`) |
| Hover preview | ✅ | Re-lit 2026-06-11, eyeball-confirmed 2026-06-12: `HoverPopover` renders the target note via `StyledRenderEngine`+`VaultResourceProvider` into a `QTextBrowser`; trigger wired through the shared `LinkService`. **Reading = plain hover; Live = Ctrl-then-hover** (`LiveView.qml` gates emission on Ctrl — Obsidian-faithful, kept by user decision). Subpath (`#heading`/`#^block`) slicing deferred |
| Word count / Ln,Col statusbar | ✅ | Fixed 2026-06-11 (`fb47120e`) — `NoteEditorWidget` seeds + re-emits word count off `NoteDocument::textChanged`; Ln/Col live in all modes via base `cursorPositionChanged` (`ad0729e7`) |
| goToLine / ephemeral state (cursor+scroll persist) | ✅ | contract-v2 `CursorPos` + 0.0–1.0 scroll fraction via base (`5d7fcc5e`); Live attach-window writes fixed upstream (`8112833f`) |
| Theme propagation to leaves | ✅ | `applyThemeToAllLeaves` + `wireLeaf` at lazy construction (`17b2cd00`) |
| Templates / daily notes | 🟡 / ✅ | template body still appends at END, but the `{{cursor}}` marker now moves the caret via `goToLine` (`5d7fcc5e`); daily notes wired |
| Paste/drop images, paste HTML→MD | ❌ | no host hooks; Live clipboard is plain-text only |
| Vim mode / spellcheck / RTL | ❌ | absent entirely (explicitly post-1.0 candidates) |
| Zoom/font scale | ✅ | all leaves via base `setFontScale` (`c6a7afad`); steps match Live's Ctrl+= shortcuts (1.10) |
| Autosave | ✅ | `AutosaveReactor` (2 s debounce) → saveDocument (see P0 above) |

## 4. Workspace, UI, settings

| Feature | Status | Evidence / notes |
|---|---|---|
| Tabs: open/close/drag/split/undo-close | ✅ | KDDW; `Workspace.cpp:409-544`; Ctrl+Shift+T cap 10 |
| Pin tabs | 🟡 | model+serialization done; **no UI/command**; pinned tabs still recycled by openLinkText |
| Tab history back/forward | ✅ | Cluster L Phase L4, live-verified 2026-08-18 — `navigateActiveLeafTo` calls `leaf->navigate()`; Ctrl+Alt+←/→ + mouse buttons 4/5 + tab-frame nav buttons all wired to `goBack()`/`goForward()`; enablement tracks `LeafHistory::canGoBack/canGoForward` per active leaf |
| Stacked tabs | ⭕ | flag round-trips; no toggle, no visual treatment |
| Popout windows | 🟡 | `popoutLeaf` works via drag-out; no command; **not persisted** (see next row) |
| workspace.json fidelity | ✅ | Cluster L2 — `MainWindow::saveSessionState` routes Tier 1 through `Workspace::writeWorkspaceJson` (full main/active/floating/lastOpenFiles + unknown-key passthrough); split/tabs `dimension` round-trip still gapped (punch-list P3, `[cluster-l]`) |
| Sidebars (KateMDI tool views) | ✅ UI · ❌ persistence | save/restore code exists (`CorbomiteMDI.cpp:1351-1460`) but never called; width hardcoded 200 |
| Ribbon | ⚠ | top KToolBar vs Obsidian's left vertical strip; hiddenItems persistence works, no UI to hide items |
| Command palette | 🟡 | KCommandBar over ~70 KActions (many disabled stubs) + ~13 registry commands; Obsidian lists hundreds |
| Quick switcher | 🟡 | faithful fuzzy scorer (`FuzzyMatcher.cpp`); filename-only — no path/alias match, hidden create flow |
| Hotkeys | 🟡 | KShortcutsEditor page works (KConfig); `hotkeys.json` parsed but **never applied** |
| Settings dialog | 🟡 | Editor/Files/Appearance/Daily-notes/Hotkeys/Plugins pages; missing Files&Links prefs, accent color, fonts, CSS snippets, About |
| Themes | ⭕ | KColorScheme light/dark works; `ThemeService` body `#if 0` — theme combo empty, appearance.json apply no-ops, no CSS theme import |
| File explorer | 🟡 | open/rename/delete/new-note + expansion persistence; no new-folder, drag-move, duplicate, reveal, sort |
| Backlinks / outlinks / outline / properties panels | 🟡 | flat lists; no unlinked mentions, no context snippets; properties panel is the strongest |
| Bookmarks | 🟡✅ | closest to parity: all types + subpaths + unknown-type round-trip |
| Tags pane / fullscreen / Mod+1..9 | ❌ | absent |
| New-note flow | ⚠ | modal name dialog vs Obsidian's inline "Untitled N" |
| Multi-vault | ⚠ | single window, switch closes current (deliberate for now) |

## 5. Search, graph, canvas, bases

| Feature | Status | Evidence / notes |
|---|---|---|
| Search DSL parse (path/file/content/tag, phrases, regex, OR, -, parens, case) | ✅ | `SearchDSL.cpp`; quirk-faithful |
| DSL backend honor | 🟡 | FTS5 + post-filters; `line:/block:/section:/task*:` parsed but unsupported; bareword is exact-token not substring (⚠) |
| Search results UI | 🟡 | **one snippet per file** (`SQLiteIndex.cpp:461`); no history/sort/copy/context toggle |
| Embedded `query` blocks | ❌ | absent |
| Quick-switcher fuzzy scoring parity | ✅ | constants match audit exactly |
| Global graph (layout, sliders, orphans, hover, ctx menu) | ✅ | `libs/forcegraph` + `GraphViewTab.cpp`; solid |
| Graph: tag/attachment filters, color groups, screenshot | ❌ | TODOs in `GraphControlsPanel.cpp:79-80` |
| graph.json persistence | ❌ | nothing persisted; sliders reset every session |
| Local graph | 🟡 | depth hardcoded 2, zero controls (`LocalGraphView.cpp:72`) |
| .canvas round-trip (unknown fields, defaults, V5 self-heal, rounding) | ✅ | `CanvasDocument.cpp` + 481-line test |
| Canvas node/edge array order | ⚠ | `QHash` storage reorders on save — diff churn (`CanvasDocument.h:57-58`) |
| Canvas: text/file/group nodes, edge render/label/arrows, export image | ✅ | scene + `CanvasFileView.cpp:82-162` |
| **Canvas: edge creation UX** | ⭕ | `CreateEdgeTool`/`CreateCardTool` fully written, **never instantiated** (`CanvasScene.cpp:36`) |
| Canvas: file-card path resolution | ⚠ BUG | relative to canvas dir, not vault root (`CanvasViewTab.cpp:26-33`) — subfolder canvases show empty cards |
| Canvas: image/link nodes, edge color, z-order, readonly toggle, zoom-to-sel | ❌ | link nodes parsed but invisible |
| Canvas card fidelity | 🟡 | StyledRenderEngine: 6 block kinds; no tables/images/math |
| .base YAML round-trip + unrecognizedData | ✅ | `BasesQuery.cpp:256-363`; `tst_yaml_schema` |
| Formula DSL (16 globals, ~58 typed methods, lambdas) | ✅ | `Builtins.cpp:42-196`, `Evaluator.cpp` |
| Table view: group/sort/filter/summaries/edit/undo/+New/export/columns | ✅ | Cluster D; **all UI pending human eyeball** |
| Built-in summaries | 🟡 | missing Range/Earliest/Latest/Checked/Unchecked/Empty/Filled (`BasesQueryResult.cpp:177-187`) |
| Cards view / embedded ` ```base ` blocks | ❌ | table-only; no note embedding |
| Icon/Image/markdown cell rendering | ❌ | no paint path in delegate |

## 6. Plugin system (deprioritized for first release)

Proxy/registry surface (11 registrars), permissions, manifest+data.json lifecycle,
external-settings watch: ✅ and ahead of schedule (Cluster B). Editor plugin API
(Cluster E): needs re-scope against D2. Bases plugin API (D.5): unspecced.
Example plugins compile against current API (verified 2026-06-10).

---

## The eight headline holes (what a dogfooder hits in hour one)

1. ~~**Link clicks do nothing** in every mode (P0, wiring-only fix).~~ **FIXED `88ad1b46`+`8c9d8c8d` (Phase 0)**
2. ~~**No completion** for `[[` or `#` — typed blind.~~ **FIXED — completion revival A1–A3 (`[[` names + `#` tags, all editable leaves); aliases + `[[note#` headings (A2); `[[note#^` existing-block ids (A3). Block-id *creation* on pick deferred (follow-up).**
3. ~~**Editor save path can truncate notes on crash** (P0, ~5-line fix).~~ **FIXED `51d62910` (Phase 0)**
4. ~~**No back/forward navigation** despite a complete LeafHistory engine.~~ **FIXED — Cluster L Phase L4, live-verified 2026-08-18.**
5. **Reading mode is a downgrade** (no tables until re-pin, raw math, inert checkboxes).
6. ~~**Status bar lies** (Words: 0 forever)~~ **FIXED `fb47120e` (2026-06-11).** Sidebar layout amnesia on every launch remains open (punch-list P2 — `Sidebar::saveSession/restoreSession` exist but are never called).
7. **Blank-line normalization on save** (2+ blank-line runs → 1, Markoff-owned, intentional per B1 §2 — **accepted + documented** 2026-06-10, not corruption; see triage doc).
8. **Canvas can't create edges**; subfolder canvases render empty file cards.

The matching fix sequence lives in
[`superpowers/plans/2026-06-10-road-to-dogfood.md`](superpowers/plans/2026-06-10-road-to-dogfood.md).
