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
- [ ] [vault] `FileManager::renameFile` rewrites markdown-style links and full-path forms; should use `MetadataCache` snapshot as source-of-truth — `libs/vault/src/FileManager.cpp:148-216` — see [vault.md](audit-2026-04-26/vault.md) §"Notable concerns / suspected bugs"
- [ ] [settings] `appearance.json` `theme` value vocabulary mismatch — write `"obsidian"`/`"moonstone"` not `"system"`/`"light"`/`"dark"` — see [settings.md](audit-2026-04-26/settings.md) §"On-disk schema compatibility matrix (per-file)"
- [ ] [settings][plugin] Wire `core-plugins.json` and `community-plugins.json` to `PluginManager` so toggle state transfers Corbomite ⇄ Obsidian — see [settings.md](audit-2026-04-26/settings.md) §"On-disk schema compatibility matrix (per-file)"
- [ ] [vault] `Vault::create` doesn't case-insensitively collision-check — see [vault.md](audit-2026-04-26/vault.md) §"Notable concerns / suspected bugs"
- [ ] [vault] `CaseSensitivityProbe` is dead code (no callers) — see [vault.md](audit-2026-04-26/vault.md) §"Notable concerns / suspected bugs"
- [ ] [parsing] `processFrontMatter` cannot strip an emptied frontmatter block — see [parsing.md](audit-2026-04-26/parsing.md) §"Notable concerns / suspected bugs"
- [ ] [metadata] `SQLiteIndex` doesn't write `frontmatterLinks` rows (SQL backlinks miss every link declared in `related: "[[Foo]]"`) — see [metadata.md](audit-2026-04-26/metadata.md) §"Notable concerns / suspected bugs"
- [ ] [metadata] `drainOnePath` only re-resolves `cache.links` (embeds + frontmatterLinks stay raw) — see [metadata.md](audit-2026-04-26/metadata.md) §"Notable concerns / suspected bugs"
- [ ] [metadata] `LinkResolver` step 4 dot-relative path with extension fails for folders named `2026.04` — see [metadata.md](audit-2026-04-26/metadata.md) §"Notable concerns / suspected bugs"
- [ ] [metadata][parsing] `collectFrontmatterLinks` grabs only the first wikilink per string leaf — see [metadata.md](audit-2026-04-26/metadata.md) §"Notable concerns / suspected bugs"

## P1 — Workspace.json round-trip fixes

- [ ] [workspace] Consolidate `Workspace::serialize` and `WorkspaceSerializer::toJson` into one writer — see [workspace.md](audit-2026-04-26/workspace.md) §"Notable concerns / suspected bugs"
- [ ] [workspace] Implement nested-split round-trip so opening + saving an Obsidian-authored layout doesn't degrade — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps"
- [ ] [workspace] Per-group `currentTab` instead of conflated global active-leaf-index — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps"
- [ ] [workspace] Stop blind write-through of `left`/`right` `m_unknownRoot` subtree in `SessionManager` — match Obsidian shape or omit — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps"
- [ ] [workspace] Popout window leak — add `m_windows.removeOne(window)` to the X-close path — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps"
- [ ] [workspace] `m_tabGroupOf` lags user drags — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps"
- [ ] [workspace] `undoCloseLeaf` loses original parent + leafHistory + eState — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps"
- [ ] [workspace] `MenuEventEmitter::fileMenu` lacks source discriminator — see [workspace.md](audit-2026-04-26/workspace.md) §"Notable concerns / suspected bugs"
- [ ] [workspace] `addAction` appends rather than prepends — see [workspace.md](audit-2026-04-26/workspace.md) §"Notable concerns / suspected bugs"

## P2 — High-traffic UX correctness

- [ ] [editor-markdown] Reading mode `setCursorLine` is hard-coded false — at minimum scroll to section containing line N (ideally subpath nav with ancestor un-fold) — see [editor-markdown.md](audit-2026-04-26/editor-markdown.md) §"Top suspected bugs"
- [ ] [rendering][parsing] Footnotes are functionally dead — render definitions, hover-popover for refs, scroll-to-definition on click — see [rendering.md](audit-2026-04-26/rendering.md) §"Top gaps"
- [ ] [rendering][parsing] Callouts not parsed/rendered (data model exists in `Theme`, no consumer) — add `> [!note]`/`[!warning]` parser+renderer — see [rendering.md](audit-2026-04-26/rendering.md) §"Top gaps"
- [ ] [ui-bundle][rendering] HoverPopover lacks Mod-key pinning + child-popover chains + anchor-to-mouse + `elementFromPoint` poll — see [ui-bundle.md](audit-2026-04-26/ui-bundle.md) §"Top gaps"
- [ ] [ui-bundle] `Notice(text, 0)` should not auto-close (one-line fix) — `libs/ui/src/Notice.cpp:48-50` — see [ui-bundle.md](audit-2026-04-26/ui-bundle.md) §"Top suspected bugs"
- [ ] [ui-bundle] FileExplorer F2/Delete should route through `RenameDialog`/`DeleteConfirmDialog`, not `QInputDialog`/`QMessageBox::question` — see [ui-bundle.md](audit-2026-04-26/ui-bundle.md) §"Top suspected bugs"
- [ ] [views][vault] Title not refreshed on external rename — re-emit `displayTextChanged` from `Vault::renamed` — see [views.md](audit-2026-04-26/views.md) §"Top suspected bugs"
- [ ] [search] Bare `/regex/` query returns zero results — `SQLiteIndex.cpp:397` early-return ignores `postFilter` — see [search.md](audit-2026-04-26/search.md) §"Top suspected bugs"
- [ ] [search] Top-level `-foo` query returns zero results — produces invalid FTS5 — `libs/search/src/SearchDSL.cpp:542-545` — see [search.md](audit-2026-04-26/search.md) §"Top suspected bugs"
- [ ] [rendering] Mermaid renders as light theme on dark themes — `MermaidRenderer::renderSvg` takes only source string, no theme parameter — see [rendering.md](audit-2026-04-26/rendering.md) §"Top suspected bugs"
- [ ] [editor-markdown] Live-preview off-by-one column round-trip — `libs/markoff/src/NoteEditorWidget.cpp:289-292` vs `:323-324` — see [editor-markdown.md](audit-2026-04-26/editor-markdown.md) §"Top suspected bugs"
- [ ] [views] `writeBackup` writes plain `.md` inside vault (shows in tree/search/graph; triggers another `Vault::modified`) — see [views.md](audit-2026-04-26/views.md) §"Top suspected bugs"
- [ ] [views] Open file deleted externally orphans leaf; `FileView::setState` swallows missing-file silently — see [views.md](audit-2026-04-26/views.md) §"Top suspected bugs"
- [ ] [editor-markdown] `setFoldedHeadingLines` doesn't invalidate when line count changes (folds against wrong target) — see [editor-markdown.md](audit-2026-04-26/editor-markdown.md) §"Top suspected bugs"
- [ ] [editor-markdown] Checkbox-click-to-toggle missing in Reading and broken in Live — see [editor-markdown.md](audit-2026-04-26/editor-markdown.md) §"Top gaps"
- [ ] [bases][parsing] User-keyed dicts inside `.base` (`properties`/`formulas`/`summaries`) still alphabetise on round-trip — `BasesQuery::properties`/`formulas`/`summaryFormulas` are `QHash` (unstable order). Switch to insertion-ordered containers + track parse order; follow-up to the P0 top-level fix

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
