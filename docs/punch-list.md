# Corbomite punch list

> Audit-derived task tracker. Source: [audit-2026-04-26/](audit-2026-04-26/).
> Each item is a checkbox + severity tag + domain tag + headline + path:line + link to the audit sub-report section.
> Mark items `[x]` when committed; do not delete (history of fixes lives here).
> When picking up new work, scan top-down: P0 first.
> Strategic clusters (multi-phase coordinated work) live separately under `superpowers/plans/`.

**Last refreshed:** 2026-04-26 (initial extraction from audit-2026-04-26).
**Item count:** 58

---

## P0 — Vault-format silent-corruption fixes (FIX FIRST)

- [x] [vault][parsing] `processFrontMatter` reorders YAML keys alphabetically on every metadata edit — `libs/vault/src/FileManager.cpp:138-143` — see [parsing.md](audit-2026-04-26/parsing.md) §"Frontmatter round-trip risks (CRITICAL)"
- [x] [vault][settings] Two parallel `.obsidian/*.json` writers diverge on indent (Bookmarks plugin writes 4-space; route `Vault::writeConfigJson` through `VaultConfig::serializeObsidianStyle`) — `libs/vault/src/Vault.cpp` vs `VaultConfig::serializeObsidianStyle` — see [settings.md](audit-2026-04-26/settings.md) §"On-disk schema compatibility matrix (per-file)"
- [x] [bases][parsing] `.base` YAML emitter alphabetises keys (same root cause as frontmatter sort) — `libs/bases/src/BasesQuery.cpp:108-127` — see [bases.md](audit-2026-04-26/bases.md) §"On-disk `.base` format compatibility" *(top-level + per-view canonical order; user-keyed dicts still alphabetise — see follow-up)*
- [x] [core][metadata] `resolveSubpath` block-id case-sensitivity — lowercase both sides — `libs/core/src/LinkUtils.cpp:121-155` — see [core-and-addenda.md](audit-2026-04-26/core-and-addenda.md) §"Notable concerns / suspected bugs"
- [x] [vault] Folder rename loses descendants (recursive walk + per-descendant `renamed` emission missing) — `libs/vault/src/Vault.cpp:337-373` — see [vault.md](audit-2026-04-26/vault.md) §"Notable concerns / suspected bugs"
- [x] [vault] `FileManager::renameFile` rewrites markdown-style links and full-path forms; should use `MetadataCache` snapshot as source-of-truth — `libs/vault/src/FileManager.cpp:148-216` — see [vault.md](audit-2026-04-26/vault.md) §"Notable concerns / suspected bugs"
- [x] [settings] `appearance.json` `theme` value vocabulary mismatch — write `"obsidian"`/`"moonstone"` not `"system"`/`"light"`/`"dark"` — see [settings.md](audit-2026-04-26/settings.md) §"On-disk schema compatibility matrix (per-file)"
- [x] [settings][plugin] Wire `core-plugins.json` and `community-plugins.json` to `PluginManager` so toggle state transfers Corbomite ⇄ Obsidian — see [settings.md](audit-2026-04-26/settings.md) §"On-disk schema compatibility matrix (per-file)" — design: [`docs/superpowers/specs/2026-04-26-plugin-enable-state-cross-app-compromise.md`](superpowers/specs/2026-04-26-plugin-enable-state-cross-app-compromise.md)
- [x] [vault] `Vault::create` doesn't case-insensitively collision-check — see [vault.md](audit-2026-04-26/vault.md) §"Notable concerns / suspected bugs"
- [x] [vault] `CaseSensitivityProbe` is dead code (no callers) — see [vault.md](audit-2026-04-26/vault.md) §"Notable concerns / suspected bugs"
- [x] [parsing] `processFrontMatter` cannot strip an emptied frontmatter block — see [parsing.md](audit-2026-04-26/parsing.md) §"Notable concerns / suspected bugs"
- [x] [metadata] `SQLiteIndex` doesn't write `frontmatterLinks` rows (SQL backlinks miss every link declared in `related: "[[Foo]]"`) — see [metadata.md](audit-2026-04-26/metadata.md) §"Notable concerns / suspected bugs"
- [x] [metadata] `drainOnePath` only re-resolves `cache.links` (embeds + frontmatterLinks stay raw) — see [metadata.md](audit-2026-04-26/metadata.md) §"Notable concerns / suspected bugs"
- [x] [metadata] `LinkResolver` step 4 dot-relative path with extension fails for folders named `2026.04` — see [metadata.md](audit-2026-04-26/metadata.md) §"Notable concerns / suspected bugs"
- [x] [metadata][parsing] `collectFrontmatterLinks` grabs only the first wikilink per string leaf — see [metadata.md](audit-2026-04-26/metadata.md) §"Notable concerns / suspected bugs"

## P1 — Workspace.json round-trip fixes

- [x] [workspace] Consolidate `Workspace::serialize` and `WorkspaceSerializer::toJson` into one writer — see [workspace.md](audit-2026-04-26/workspace.md) §"Notable concerns / suspected bugs"
- [x] [workspace] Implement nested-split round-trip so opening + saving an Obsidian-authored layout doesn't degrade — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps"
- [x] [workspace] Per-group `currentTab` instead of conflated global active-leaf-index — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps"
- [x] [workspace] Stop blind write-through of `left`/`right` `m_unknownRoot` subtree in `SessionManager` — match Obsidian shape or omit — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps". Spec for the deferred decision: [`specs/2026-04-26-workspace-serializer-consolidation-design.md`](superpowers/specs/2026-04-26-workspace-serializer-consolidation-design.md) §Deferred follow-ups. Three options to pick from: (A) drop on save; (B) pass through unmodified unless Corbomite mutated sidebar state (recommended); (C) translate to/from `CorbomiteMDI::Sidebar`. **Picked B**: `SessionManager` now tracks an `m_sidebarDirty` bit (set on identity-change in `saveSidebarState`); on save, `left`/`right` are dropped iff dirty. `floating` write-through left untouched (separate concern — Corbomite serializes its own floating section but MainWindow currently drops it before handoff).
- [x] [workspace] Repurpose `m_tabGroupOf` against live `Layout::groups()` (cache or eliminate). Update `Workspace.h:267-271` comment claiming KDDW has no public Group enumeration API. Audit: [workspace.md](audit-2026-04-26/workspace.md) §"High severity" #1 — now solvable via the public KDDW API documented in [`obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md`](obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md)
- [x] [workspace] Popout window leak — add `m_windows.removeOne(window)` to the X-close path — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps"
- [x] [workspace] `m_tabGroupOf` lags user drags — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps" — closed jointly with the previous item: tab-navigation primitives now read membership from `DockRegistry::groups()` directly, so user drags are reflected immediately.
- [x] [workspace] `undoCloseLeaf` loses original parent + leafHistory + eState — `closeLeaf` now captures a live tab-group sibling id as `UndoEntry::parentId`; `undoCloseLeaf` resolves it to `createLeafInGroupOf(sibling)`, restores `setEphemeralState(eState)` + overwrites `leaf->history()` with the captured `LeafHistory`, and rekeys `m_leavesById` + KDDW dock-widget uniqueName to the restored leaf id. — `libs/core/src/Workspace.cpp` (closeLeaf, undoCloseLeaf)
- [x] [workspace] `MenuEventEmitter::fileMenu` lacks source discriminator — added `(QString source, QObject* leaf=nullptr)` to the `fileMenu` signal + `emitFileMenu` helper; introduced `Corbomite::FileMenuSource::*` constants for the six Obsidian source values; `MenuInjector::onFileMenuBuilt` now takes a `FileMenuHandler = void(QMenu*, QString filePath, QString source)` so plugin handlers can scope by invocation site. — `libs/core/{include/corbomite/core/MenuEventEmitter.h,src/MenuEventEmitter.cpp,include/corbomite/core/proxies/MenuInjector.h,src/proxies/MenuInjector.cpp}`
- [x] [workspace] `addAction` appends rather than prepends — `ItemView::addAction` now uses `m_actionsLayout->insertWidget(0, btn)` so the most-recent action sits leftmost (closest to the title) and the hamburger stays anchored rightmost, matching Obsidian. — `libs/core/src/ItemView.cpp:87`

## P2 — High-traffic UX correctness

- [ ] [editor-markdown] Reading mode `setCursorLine` is hard-coded false — at minimum scroll to section containing line N (ideally subpath nav with ancestor un-fold) — see [editor-markdown.md](audit-2026-04-26/editor-markdown.md) §"Top suspected bugs"
- [ ] [rendering][parsing] Footnotes are functionally dead — render definitions, hover-popover for refs, scroll-to-definition on click — see [rendering.md](audit-2026-04-26/rendering.md) §"Top gaps"
- [ ] [rendering][parsing] Callouts not parsed/rendered (data model exists in `Theme`, no consumer) — add `> [!note]`/`[!warning]` parser+renderer — see [rendering.md](audit-2026-04-26/rendering.md) §"Top gaps"
- [ ] [ui-bundle][rendering] HoverPopover lacks Mod-key pinning + child-popover chains + anchor-to-mouse + `elementFromPoint` poll — see [ui-bundle.md](audit-2026-04-26/ui-bundle.md) §"Top gaps"
- [x] [ui-bundle] `Notice(text, 0)` should not auto-close (one-line fix) — `src/dialogs/Notice.cpp:48-50` (audit cited the wrong path; lives in src/dialogs/, not libs/ui/) — see [ui-bundle.md](audit-2026-04-26/ui-bundle.md) §"Top suspected bugs"
- [x] [ui-bundle] FileExplorer F2/Delete now route through `FileManagerProxy::promptForFileRename`/`promptForDeletion` (the existing validating `RenameDialog`/`DeleteConfirmDialog`) instead of `QInputDialog`/`QMessageBox::question`. The two prompt helpers were promoted onto the plugin proxy gated on `vault.write` — `libs/vault/include/corbomite/vault/proxies/FileManagerProxy.h`, `src/plugins/file-explorer/FileExplorerView.cpp`
- [x] [views][vault] Title not refreshed on external rename — `NoteDocument` got a `setRelativePath` setter + `pathChanged(oldRelativePath)` signal; `Vault::rename` (programmatic + folder-rename + watcher-driven external) now rekeys `m_docs` and calls `setRelativePath` on the cached doc; `FileView::loadFile` connects to `pathChanged` and re-emits `displayTextChanged` so the leaf's tab caption refreshes via `WorkspaceLeaf`'s existing `m_dockWidget->setTitle` wire. — `libs/core/{include/corbomite/core/{NoteDocument.h,FileView.h},src/{NoteDocument.cpp,FileView.cpp}}`, `libs/vault/src/Vault.cpp`
- [x] [search] Bare `/regex/` query returns zero results — `SQLiteIndex.cpp:397` early-return ignores `postFilter` — see [search.md](audit-2026-04-26/search.md) §"Top suspected bugs"
- [x] [search] Top-level `-foo` query returns zero results — produces invalid FTS5 — `libs/search/src/SearchDSL.cpp:542-545` — see [search.md](audit-2026-04-26/search.md) §"Top suspected bugs". Fix: emit a single top-level negation in the And case when there is no positive sibling, then reroute leading `NOT …` from `compile()` into a new `excludedFts5Query` channel; `SQLiteIndex::searchCompiled` applies it as `path NOT IN (… MATCH excludedFts5Query)`. Also fixes `-foo -bar` (was producing `foo NOT bar`).
- [ ] [rendering] Mermaid renders as light theme on dark themes — `MermaidRenderer::renderSvg` takes only source string, no theme parameter — see [rendering.md](audit-2026-04-26/rendering.md) §"Top suspected bugs"
- [ ] [editor-markdown] Live-preview off-by-one column round-trip — `libs/markoff/src/NoteEditorWidget.cpp:289-292` vs `:323-324` — see [editor-markdown.md](audit-2026-04-26/editor-markdown.md) §"Top suspected bugs"
- [x] [views] `writeBackup` no longer writes inside the vault; backups land at `<AppLocalDataLocation>/file-recovery/<vault-id>/<name>-<ts>.md` (per-vault subdir keyed by basename + 12-char hash of the vault root) so the recovery copy doesn't show up in tree/search/graph or re-trigger `Vault::modified`. Routed through `m_adapter->mkpath`+`write` so the existing MemoryAdapter test still exercises the path. — `libs/core/src/TextFileView.cpp`
- [x] [views] Open file deleted externally + `FileView::setState` missing-file silently — `NoteDocument` got a `deleted()` signal + `markDeleted()`; `Vault` calls it from all four delete sites (programmatic remove, system trash, vault trash, external watcher) before `deleteLater`-ing the cached doc; `FileView::loadFile` subscribes to `deleted()` and nulls `m_file` + schedules `Workspace::closeLeaf` on the next event-loop turn so a subsequent save can't silently re-create the file via `Vault::modify`. `FileView::setState` does the same when its resolveFile lookup misses (and `m_allowNoFile` isn't set). — `libs/core/{include/corbomite/core/{NoteDocument.h,FileView.h},src/{NoteDocument.cpp,FileView.cpp}}`, `libs/vault/src/Vault.cpp`
- [x] [editor-markdown] `setFoldedHeadingLines` line-count invalidation — `NoteEditorWidget` now stashes the capture-time line count in `EphemeralState::extraKeys["corbomite.foldedHeadingsLineCount"]` (Obsidian sees it as an unknown key + round-trips it). On restore, when the saved count != current line count, the fold list is dropped (passes empty vec to `setFoldedHeadingLines`) instead of folding the wrong headings. — `src/editor/NoteEditorWidget.cpp`
- [ ] [editor-markdown] Checkbox-click-to-toggle missing in Reading and broken in Live — see [editor-markdown.md](audit-2026-04-26/editor-markdown.md) §"Top gaps"
- [x] [bases][parsing] User-keyed dicts inside `.base` (`properties`/`formulas`/`summaries`) still alphabetise on round-trip — added `propertyOrder`/`formulaOrder`/`summaryFormulaOrder` companion lists tracked at parse time; `emitMap` extended with a `nestedKeyOrder` arg so per-block insertion order overrides QVariantMap's alphabetical sort. — `libs/bases/{include/corbomite/bases/BasesQuery.h,src/BasesQuery.cpp}`
- [x] [parsing][editor-markdown] Markoff `![[…]]` embed target — tree-sitter routes `![[…]]` through its `image` node in inline contexts where the wiki-link grammar doesn't win, dropping the target (image_description = `[Target]`, link_destination = ""). `TreeSitterParser::collectInlineQueries` now sniffs the raw bytes at the start of the image branch and emits a true `LinkInfo::Embed` with extracted target/display, so `MetadataCache::drainOnePath` actually has a non-empty `original` to resolve. — `libs/markoff-family/libs/markoff-parser/src/TreeSitterParser.cpp`

## P3 — Plugin extension surface (individual items; coordinated proxy work in Cluster B)

- [ ] [editor-markdown][plugin] `MarkdownRenderer.render(...)` static API + `RenderContext` — load-bearing for hover popovers, search snippets, plugin tooltips, future Bases cells — see [editor-markdown.md](audit-2026-04-26/editor-markdown.md) §"Top gaps"
- [ ] [editor-markdown][plugin] `MarkdownRenderChild` framework — parent registry, addChild, mount/unmount signals — see [editor-markdown.md](audit-2026-04-26/editor-markdown.md) §"Top gaps"
- [ ] [plugin][ui-bundle] `addStatusBarItem` — required for any plugin that wants persistent UI — see [plugin.md](audit-2026-04-26/plugin.md) §"Top gaps"
- [ ] [ui-bundle] Lucide icon registry + `Plugin.addIcon(name, svg)` — `lucide-*` strings render blank today — see [ui-bundle.md](audit-2026-04-26/ui-bundle.md) §"Top gaps"
- [ ] [plugin][core] Plugin-side `Events` mixin — Obsidian-shape `vault.on("create", cb)` won't work today — see [plugin.md](audit-2026-04-26/plugin.md) §"Top gaps"
- [ ] [plugin][core] `registerObsidianProtocolHandler` — `obsidian://`/`corbomite://` URL routing — see [plugin.md](audit-2026-04-26/plugin.md) §"Top gaps"

## P4 — Lifecycle / per-vault isolation

- [ ] [core][vault] Dangling adapter pointers in `onVaultClosed` — three `.reset()` calls — `src/MainWindow.cpp:2190-2263` — see [core-and-addenda.md](audit-2026-04-26/core-and-addenda.md) §"Top suspected bugs"
- [ ] [plugin] `onLoad` throw auto-unload — try/catch + `Component::unload()` — see [plugin.md](audit-2026-04-26/plugin.md) §"Top suspected bugs"
- [ ] [plugin][workspace] Detach-leaves-of-type on plugin disable — see [plugin.md](audit-2026-04-26/plugin.md) §"Top gaps"
- [ ] [plugin] `onExternalSettingsChange()` + `data.json` `QFileSystemWatcher` — see [plugin.md](audit-2026-04-26/plugin.md) §"Top suspected bugs"
- [ ] [core][plugin] `appId` for per-vault QtKeychain scoping — see [core-and-addenda.md](audit-2026-04-26/core-and-addenda.md) §"Top gaps"
- [ ] [core][plugin] `quit` event with `Eb`-equivalent collector for plugin async cleanup — see [core-and-addenda.md](audit-2026-04-26/core-and-addenda.md) §"Top gaps"
- [ ] [core][workspace] `css-change` event — bridge `ThemeService::themeChanged` to Workspace — see [core-and-addenda.md](audit-2026-04-26/core-and-addenda.md) §"Top gaps"

## P5 — Missing features (structurally absent; coordinated internal-plugin work in Cluster F)

- [ ] [plugin][rendering] Page Preview plugin — registry is built; tiny plugin-shaped popover orchestrator closes the loop — see [plugin.md](audit-2026-04-26/plugin.md) §"Top gaps"
- [ ] [rendering] PDF view + `![[file.pdf]]` embed — biggest single rendering-domain capability gap — see [rendering.md](audit-2026-04-26/rendering.md) §"Top gaps"
- [ ] [rendering][editor-markdown] Paste-from-HTML → Markdown (`htmlToMarkdown` analogue / Turndown port) — see [rendering.md](audit-2026-04-26/rendering.md) §"Top gaps"
- [ ] [bases][workspace] Bases Phase 2 — wire `QueryController::setCurrentFile` to workspace `file-open` so `this` actually works (UI surfaces are Cluster D) — `libs/bases/src/QueryController.cpp:51-55` — see [bases.md](audit-2026-04-26/bases.md) §"Notable concerns / suspected bugs"
- [ ] [settings][ui-bundle] Hotkeys page — invoke `KShortcutsDialog` from `SettingsDialog` (optional `hotkeys.json` ⇄ `KSharedConfig` bridge) — see [settings.md](audit-2026-04-26/settings.md) §"End-user settings UI parity"
- [ ] [ui-bundle] About page — see [ui-bundle.md](audit-2026-04-26/ui-bundle.md) §"Top gaps"

## P6 — API stability / future-proofing

- [ ] [plugin][core] Move permission tokens from `PluginContext.cpp:21-32` to `corbomite/core/PluginPermissions.h` with `inline constexpr auto` constants — see [plugin.md](audit-2026-04-26/plugin.md) §"Top suspected bugs"
- [ ] [plugin] Document the `apiLevel: 1` ABI before first third-party plugin release — see [plugin.md](audit-2026-04-26/plugin.md) §"Top gaps"
- [ ] [plugin] Document the `X-Corbomite-DockArea`/`DockIcon`/`DockTitle` extension to manifest format — see [plugin.md](audit-2026-04-26/plugin.md) §"Top gaps"
- [ ] [core] MomentFormatter missing tokens (`Y`, `Q`, `gg`/`gggg`, `e`/`E`, `k`/`kk`, `Z`/`ZZ`, locale shortcuts) — vault templates render literal characters — see [core-and-addenda.md](audit-2026-04-26/core-and-addenda.md) §"Top gaps"

---

*Derived from audit dated 2026-04-26. Refresh after each major audit pass.*
