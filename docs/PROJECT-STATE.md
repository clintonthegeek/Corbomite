# Project state

> Slim session-start orientation. **Reset 2026-04-26** after a full audit (`docs/audit-2026-04-26/`) regrouped everything into a fresh A-onwards cluster scheme + a flat punch list. Pre-reset state archived at `docs/archive-2026-04-26/PROJECT-STATE-pre-reset.md`.

## Two tracks

Work flows through **two parallel tracks**. Both must be checked at session start.

1. **Punch list** — flat severity-ranked list of small fixes. File: [`docs/punch-list.md`](punch-list.md). Pick from top (P0 first). 58 items at reset.
2. **Strategic clusters** — multi-phase coordinated initiatives. Index: [`docs/superpowers/plans/INDEX.md`](superpowers/plans/INDEX.md). 10 active at reset (A–J).

Most P0/P1 punch-list items are **silent vault-format corruption risks**. Drain them before strategic-cluster work unless explicitly redirected.

**P2 drain in flight** (2026-04-27, third autonomous session of the day). Latest pass (this session) closed five more: FileExplorer F2/Delete now route through `FileManagerProxy::promptForFileRename`/`promptForDeletion` (the validating Rename/DeleteConfirm dialogs, gated on `vault.write`); `writeBackup` moved out of the vault into `<AppLocalDataLocation>/file-recovery/<vault-id>/…` so the recovery copy can't pollute tree/search/graph or re-trigger `Vault::modified`; `NoteDocument::deleted()` + `Vault` cleanup at every delete site so `FileView` nulls its file pointer + closes the leaf instead of orphaning the tab (covers `setState` missing-file too); `NoteEditorWidget` invalidates persisted Reading-mode folds on line-count drift via a corbomite-prefixed extraKeys field; `TreeSitterParser` detects the `![[…]]` shape at the start of the image-node branch so the embed target survives when tree-sitter routes wiki-embeds through its image grammar. Open P2 items: footnotes, callouts, HoverPopover Mod-pinning, live-preview off-by-one, checkbox-click toggle (Reading + Live), reading-mode `setCursorLine`, Mermaid-theme (still deferred).

## Active strategic clusters (snapshot)

| Cluster | Title | Status | Source |
|---|---|---|---|
| D | Bases UI completion | Plan-needed (stub) | Audit |
| E | Markoff Editor API parity | Plan-needed (stub) | Audit |
| F | Internal-plugin gap fill | Plan-needed (stub) | Audit |
| G | Markoff Phase C8 (inline-ORC coherence) | In-flight | Carried (was Phase C8 plan) |
| H | Block-substitution widgets | Scouting (blocked on G) | Carried (was Cluster X) |
| I | Editor & Workspace UI surfacing | In-flight | Carried (was Cluster V) |
| J | Qutepart-Corbomite Fork | In-flight (Phases 1+2 done) | Carried (was parallel refactor) |

Closed (post-reset): A (2026-04-27), B (2026-04-28), C (2026-04-27).

Full table + plan-file links: [`docs/superpowers/plans/INDEX.md`](superpowers/plans/INDEX.md).

## Recent decisions

- **2026-04-28 — P4 plugin-lifecycle batch (3 items).** `MainWindow::onVaultClosed` now drops the embed-renderer's cached `metadataParser` pointer alongside the existing `metadataCache`/`resources` nulls, then `.reset()`s the three Markoff adapter shims (`m_linkResolverAdapter`/`m_metadataCacheAdapter`/`m_metadataParserImpl`) before deleting their wrapped `m_metadataCache`/`m_linkResolver` so the next vault-open's `make_unique` re-creation can't transiently destroy a still-referenced shim. `PluginManager::enablePlugin` wraps `plugin->load(ctx)` in try/catch — on throw it calls (idempotent) `plugin->unload()`, deletes instance + context, and records `LoadState::OnLoadThrew` so `PluginsPage` surfaces a "raised an exception while loading" notice; persisted enable-state is left intact so a fixed plugin re-enables next launch. New `Workspace::cssChange()` Q_SIGNAL + `emitCssChange()` re-entry; MainWindow wires `ThemeService::themeChanged` → `m_workspace->emitCssChange()`; `WorkspaceController::cssChange` mirrors it onto the plugin proxy surface, giving plugins Obsidian's `app.workspace.on("css-change")` shape. New tests: `tst_plugin_manager_lifecycle::onLoadThrowAutoUnloads`, `tst_proxy_workspace::cssChange_reEmitsFromWorkspace`.
- **2026-04-28 — Cluster B closed.** 16-item plugin-API-surface completion shipped across 4 phases. 11 new proxy/registry pairs (Hover, Suggest, PostProcessor, Ribbon, Embed, CodeBlock, StatusBar, LucideIcon, MarkdownRenderer, DecorationProvider, ProtocolHandler) following the existing `CommandRegistrar` pattern; 5 new permission tokens (`ui.rendering`, `ui.editor`, `ui.statusbar`, `ui.icons`, `protocol`) in a public `corbomite/core/PluginPermissions.h`; `Vault::raw` + `Vault::configChanged` signals with expanded `.obsidian/` watcher coverage; `Plugin::onExternalSettingsChange` virtual + per-plugin `data.json` watcher in `PluginManager`; permission reference docs at `docs/plugin-development/permissions.md`. 29 new test cases in `tst_proxy_extensions` + 3 in `tst_vault_watcher` + 3 in `tst_plugin_external_settings`. Spec: [`specs/2026-04-28-cluster-b-plugin-api-surface-design.md`](superpowers/specs/2026-04-28-cluster-b-plugin-api-surface-design.md). Plan: [`plans/2026-04-28-cluster-b-plugin-api-surface.md`](superpowers/plans/2026-04-28-cluster-b-plugin-api-surface.md). Follow-ups tracked in punch list. Active cluster count: 7.
- **2026-04-27 — Cluster A & Cluster C closed inline.** Both stubs drained via the P0/P1 punch-list sweeps + the 2026-04-26 serializer-consolidation work-unit. Final A item (BOM strip on read in `Vault::read` / `readRaw`; `readBinary` preserves bytes verbatim) shipped with `tst_vault_read::readStripsLeadingUtf8Bom`. Residuals reassigned: `Vault.raw` + `Vault.config-changed` events → Cluster B (#15–#16, plugin event surface); Workspaces internal plugin (`workspaces.json`) + sidedock-as-tree substrate → Cluster F (#9–#10). Active cluster count: 8.
- **2026-04-26 — Workspace serializer consolidation done.** P1 #1, #2, #3 closed. `Workspace::serialize`/`deserialize` delegate to `WorkspaceSerializer::toJson`/`fromJson`. KDDW `LayoutSaver::serializeLayout()` JSON drives split topology; `Workspace::findLeafById` drives per-leaf state; the two join on `DockWidget::uniqueName`. Per-group `currentTab` round-trips via `Core::Group::currentTabIndex()`. Audit addendum [`obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md`](obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md) corrects the stale "no public Group enumeration API" claim. Spec: [`specs/2026-04-26-workspace-serializer-consolidation-design.md`](superpowers/specs/2026-04-26-workspace-serializer-consolidation-design.md). Plan: [`plans/2026-04-26-workspace-serializer-consolidation.md`](superpowers/plans/2026-04-26-workspace-serializer-consolidation.md).
- **2026-04-26 — P0 sweep complete.** All 15 P0 silent-corruption items closed. Last two: `FileManager::renameFile` rewritten to drive surgical edits from MetadataCache positions (covers markdown-style + full-path + frontmatter link forms via a single `rewriteLinkLiteral` helper); `PluginManager` ⇄ `.obsidian/{core,community}-plugins.json` cross-app sync landed per [`specs/2026-04-26-plugin-enable-state-cross-app-compromise.md`](superpowers/specs/2026-04-26-plugin-enable-state-cross-app-compromise.md) — JSON wins on vault-open, KConfig authoritative thereafter, dual-write gated by `X-Obsidian-Id` (manifest field + 7-entry internal alias dict).
- **2026-04-26 — Tracking system reset.** Audit produced 58 punch-list items + 6 audit-derived clusters (A–F). 4 in-flight plans re-lettered (G–J). 8 SCOUTING/one-shot plans archived. `backlog.md` retired. PROJECT-STATE slimmed. Old state at `docs/archive-2026-04-26/`. See [`docs/audit-2026-04-26/README.md`](audit-2026-04-26/README.md) for the audit synthesis.

## Open questions

- Next strategic cluster to brainstorm? Recommendation: **B** (Plugin API surface) — unblocks third-party plugins and feeds Cluster F. **D** and **F** can run in parallel after B has shape.
- Cluster-A precedent (drained inline rather than expanded to a full plan): worth applying to **C/D/F** stubs first to see if any have already been drained too. Both A and C closeouts confirmed most scope items had already shipped via the punch list.

## Last touched

2026-04-28 — P4 plugin-lifecycle batch shipped: onVaultClosed adapter-pointer reset, Plugin onLoad-throw auto-unload, css-change event bridged from ThemeService through Workspace + WorkspaceController.
