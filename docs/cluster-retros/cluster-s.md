# Cluster S — Bookmarks core plugin

**Closed:** 2026-04-20
**Plan:** [2026-04-20-cluster-s-bookmarks.md](../superpowers/plans/2026-04-20-cluster-s-bookmarks.md)
**Spec:** [2026-04-19-cluster-s-bookmarks-design.md](../superpowers/specs/2026-04-19-cluster-s-bookmarks-design.md)
**Addendum:** [2026-04-19-bookmarks-core-plugin.md](../obsidian-audit/addenda/2026-04-19-bookmarks-core-plugin.md)

## What shipped

Single-plugin delivery under `src/plugins/bookmarks/` following the Cluster Q / Cluster N playbook (KPluginFactory shared module, permission-gated proxies, vault-scoped lifecycle).

- **Store + JSON round-trip** (task 1.2) — `BookmarksStore` parses/emits `.obsidian/bookmarks.json` with Obsidian-compatible schema, preserves unknown item types and unknown keys on known types for forward compatibility. 7 canonical keys known; everything else rolls through `unknownKeys`.
- **Qt model adapter** (task 1.3) — `BookmarksModel` as `QAbstractItemModel` with Display/Decoration/Type roles, drag-drop mime type for intra-tree reorder, single `changed()` emit per mutation.
- **Plugin shell** (task 2.1) — `BookmarksPlugin` with VaultProxy-driven load/save (debounced 500ms), right-dock `BookmarksView` with `+` header button, session-state serialization of expanded groups. `CommandRegistrar::addCommandRaw` added to `libs/core` to preserve the Obsidian-compat `bookmarks:*` id prefix rather than the `corbomite-bookmarks:*` auto-namespacing.
- **Commands** (task 2.2) — 7 commands registered: `open` + `bookmark-current-file` fully wired; `bookmark-all-tabs`, `bookmark-current-{heading,block,search,graph}` register with `checkCallback→false` pending missing WorkspaceController accessors. 6 static store-mutation helpers extracted to `BookmarksCommands.cpp` so tests link without pulling in the plugin shell.
- **Stale-bookmark handling + context menu** (task 2.3) — `BookmarksStore::renamePath` (folder-aware prefix rewrite, preserves `#subpath`), `markOrphaned` (stamps `unknownKeys["_orphaned"]`, round-trips through JSON), `setTitle`. Wired to `VaultProxy::renamed` and `VaultProxy::deletedFile` in `onLoad`. `BookmarksView` context menu extended with Rename… (`QInputDialog`), Move to group (nested submenu with ancestor-into-self guard), Delete.
- **BookmarkModal** (task 3.1) — Real `QDialog` with pre-filled title (`QLineEdit`, inferred from type), group picker (`QComboBox` walking nested groups as `A / B / C`), Save/Cancel `QDialogButtonBox`. Headlessly driveable via `composedItem()` + `commit()` for tests.
- **Cluster R menu slot live** (task 3.2) — `EditableFileView::setBookmarkCallback`; the disabled "Bookmark" placeholder becomes the enabled "Bookmark…" live action when the plugin is loaded; MainWindow resolves `corbomite-bookmarks` and wires the callback to the plugin's `Q_INVOKABLE openBookmarkModalForFile` slot. Slug map extended with `bookmarks → corbomite-bookmarks` so `bookmarks:open` routes through `revealDockView`.

## Test surface

4 new test binaries (`tst_bookmarks_store`, `_model`, `_commands`, `_modal`) — 40+ test cases in total. `tst_editable_file_view_menu` gained a Cluster S assertion for the callback-enabled path. Full ctest clean except for `tst_benchmark_layout` (pre-existing flaky — tracked in backlog).

## What slipped

- **5 of 7 commands are stubs.** `bookmark-all-tabs`, `bookmark-current-{heading,block,search,graph}` register but gate via `checkCallback→false` because `WorkspaceController` does not expose `openTabPaths()`, `activeHeading()`, `activeBlockId()`, `activeSearchQuery()`, `activeGraphOptions()`. The *mutation helpers* for each exist and are tested — only the workspace-side wiring is missing. Cluster V follow-up (see backlog).
- **Settings tab deferred** (task 3.3). No plugin in the tree today wires a settings tab via `createSettingsTab` / `SettingsTab`, so the single-checkbox "back up bookmarks.json before first write each session" is parked until that infrastructure lands. Backlog entry added.
- **"Open linked view" / WorkspaceController::openLinkText** still not available, so `BookmarksView::onActivated` falls back to `openFile(path)` for file bookmarks (no `#subpath` navigation yet). Cluster G follow-up #3.
- **Auto-insert block ids on `bookmark-current-block`.** Spec default is "show Notice; don't auto-insert." Backlog follow-up to add `MarkdownView::ensureBlockIdAtCursor()` when a second feature demands it.
- **Bulk "Remove all broken bookmarks"** context menu action. Backlog follow-up — the `_orphaned` marker is plumbed through so this is cheap when next touched.

## Architectural notes

- `addCommandRaw` is now the general mechanism for plugins that register commands under canonical Obsidian ids rather than `<pluginId>:<id>`. Bookmarks is the first caller; any future core-plugin migration that needs to match Obsidian's hotkey-settings wire format can reuse it.
- The **static-helpers-in-separate-TU** pattern (`BookmarksCommands.cpp`) kept the command tests free of `KPluginFactory` and view/modal deps. Good precedent for future plugin command test suites.
- `markOrphaned` stores state in `unknownKeys["_orphaned"]` (rather than a first-class struct field) so that round-trip preserves it through any future schema additions without a parser change.

## Unblocks

- Cluster R's "Bookmark…" menu slot (previously a disabled placeholder). Active in every `EditableFileView` hamburger menu.
