# Obsidian core command ids → Corbomite

**GENERATED — do not hand-edit.** Regenerate with:

```bash
node RE/tools/extract-command-ids.js RE/obsidian/<version>/renamed/src \
     docs/obsidian-command-ids.json \
     --md docs/obsidian-command-ids.md \
     --corbomite docs/obsidian-command-ids.corbomite.json
```

Corpus: `RE/obsidian/1.13.7/renamed/src`. Extraction is AST-exact (see the script's header
for why a regex scrape undercounts). Machine form: `docs/obsidian-command-ids.json`.

## Reconciling the audit's 190

`docs/obsidian-audit/addenda/2026-10-03-core-command-and-hotkey-contracts.md`
§B reports **190** ids as 115 app-level + 74 internal-plugin + 1 mobile-only.
Re-extraction reproduces its 115 and its 74 + 1 exactly. The 42-id gap is
the `Workspace` set: `Workspace.registerCommands`
(`app/own/1932-r4.js:311-763`) registers 42 more command ids through the
same `app.commands.addCommand`, and the audit's arithmetic has no bucket
for it. So **232** is the real total, not 190.

Its per-platform split (7 desktop-only, 11 mobile-only, 3 mobile-app-only)
is likewise correct *for the app-level set only* — it was read inside
`App.registerCommands`. Corpus-wide it is 10 / 14 / 3: `Workspace` adds
`workspace:copy-full-path` and `workspace:show-trash` to the desktop-only
list, the canvas plugin adds `canvas:export-as-image`, and the three
mobile-app-only clipboard ids are additionally absent from the app-level
count. Both framings are in the table below: the `Registered by` column
says which owner a row belongs to.

## Counts

| Measure | Value |
|---|---|
| **Distinct core command ids** | **232** |
| App-level (`App.registerCommands`) | 115 |
| Workspace-level | 42 |
| Internal-plugin registrations | 75 |
| Desktop-only | 10 |
| Mobile-only | 14 |
| — of which `mobileOnly: true` in the spec | 10 |
| — of which the site is wrapped in `rd.isMobile &&` | 4 |
| Mobile-app-only | 3 |
| Ids carrying a default hotkey | 41 |
| Registration sites with an unresolvable id | 0 |
| API forwarders (not registrations) | 2 |

Three registration paths carry a `Command` object literal and all are
counted: `app.commands.addCommand({…})` in `App.registerCommands` and in
`Workspace`, and the internal-plugin wrapper's `registerGlobalCommand({…})`
(`app/own/1939-m4.js:663`). A call with a non-literal argument is a
*forwarder* — `Plugin.prototype.addCommand` (`app/own/1938-c4.js:63`) and the
wrapper's bulk re-add loop (`app/own/1939-m4.js:560`) — i.e. plugin API
surface, not a registration, so it is listed separately and not counted.

`app/all.js` is excluded: it is the pre-split concatenated build and every
statement in it is duplicated under `<bundle>/own/*.js`.

## Corbomite coverage

| Status | Obsidian ids |
|---|---|
| `unimplemented` | 202 |
| `implemented` | 26 |
| `stubbed` | 4 |

## Corbomite-native ids (not Obsidian)

Corbomite also registers ids that are **not** Obsidian command ids. They
cannot be named by a vault's `hotkeys.json`, so they are compat-neutral:
nothing breaks, nothing is gained.

| Corbomite id | Note |
|---|---|
| `bookmarks:bookmark-current-file` | alias of bookmarks:bookmark-current-view |
| `corbomite-backlinks:open` | alias of backlink:open |
| `corbomite-bookmarks:open` | alias of bookmarks:open |
| `corbomite-file-explorer:reveal-file` | alias of file-explorer:reveal-active-file |
| `corbomite-graph-view:copy-screenshot` | no Obsidian counterpart |
| `corbomite-local-graph:open-local` | alias of graph:open-local |
| `corbomite-outline:open` | alias of outline:open |
| `corbomite-outlinks:open` | alias of outgoing-links:open |
| `corbomite-properties:open` | alias of properties:open |
| `split_down` | MainWindow → KAction split_down; dispatched by view-header hamburger |
| `split_right` | MainWindow → KAction split_right; dispatched by view-header hamburger |

## Full table

`Flags` carries Obsidian's four undocumented spec fields (addendum §C) plus
the platform split. `gate:` records a platform-conditional *field*; the
desktop-only and mobile-only rows are registrations Obsidian wraps in
`rd.isDesktopApp && …` / `rd.isMobile && …`, which is a property of the
call site rather than of the spec.

The hotkey column is the `hotkeys:` field verbatim. `{1|2|…}` marks a key
templated over a loop variable — one binding serves every id the site
expands to. *when …* marks the platform-conditional defaults; they are the
only ones, so a reader cannot assume one default per id.

| Obsidian command id | i18n name key | Registered by | Flags | Default hotkey | Corbomite |
|---|---|---|---|---|---|
| `app:delete-file` | `bd.commands.deleteCurrentFile` | app | — |  | `unimplemented` |
| `app:go-back` | `bd.commands.navigateBack` | app | — | Mod+Alt `ArrowLeft` | `implemented` → `KAction go_back` |
| `app:go-forward` | `bd.commands.navigateForward` | app | — | Mod+Alt `ArrowRight` | `implemented` → `KAction go_forward` |
| `app:open-another-vault` | `bd.commands.openVault` | app | **desktop-only** gate:site=isDesktopApp |  | `unimplemented` |
| `app:open-help` | `bd.commands.openHelp` | app | — | — `F1` | `unimplemented` |
| `app:open-sandbox-vault` | `bd.commands.openSandboxVault` | app | **desktop-only** gate:site=isDesktopApp |  | `unimplemented` |
| `app:open-settings` | `bd.commands.openSettings` | app | — | Mod `,` | `unimplemented` |
| `app:open-vault` | `bd.commands.manageVaults` | app | — |  | `unimplemented` |
| `app:reload` | `bd.commands.reload` | app | — |  | `unimplemented` |
| `app:show-debug-info` | `bd.commands.showDebugInfo` | app | — |  | `unimplemented` |
| `app:show-release-notes` | `bd.commands.showReleaseNotes` | app | — |  | `unimplemented` |
| `app:show-tab-switcher` | `bd.interface.mobile.actionShowSwitcher` | app | `mobileOnly` gate:site=isMobile |  | `unimplemented` |
| `app:switch-vault` | `bd.commands.changeVault` | app | — |  | `unimplemented` |
| `app:toggle-default-new-pane-mode` | `bd.commands.toggleDefaultNewTabMode` | app | — |  | `unimplemented` |
| `app:toggle-left-sidebar` | `bd.commands.toggleLeftSidebar` | app | — |  | `implemented` → `KAction view_toggle_left_sidebar` |
| `app:toggle-ribbon` | `bd.commands.toggleRibbon` | app | — |  | `unimplemented` |
| `app:toggle-right-sidebar` | `bd.commands.toggleRightSidebar` | app | — |  | `unimplemented` |
| `audio-recorder:start` | `bd.plugins.audioRecorder.actionStart` | internal-plugin | — |  | `unimplemented` |
| `audio-recorder:stop` | `bd.plugins.audioRecorder.actionStop` | internal-plugin | — |  | `unimplemented` |
| `backlink:open` | `bd.plugins.backlinks.actionShow` | internal-plugin | — |  | `implemented` → `BacklinksPlugin revealDockView` |
| `backlink:open-backlinks` | `bd.plugins.backlinks.actionOpenForCurrent` | internal-plugin | — |  | `unimplemented` |
| `backlink:toggle-backlinks-in-document` | `bd.plugins.backlinks.actionToggleBacklinksInDocument` | internal-plugin | — |  | `unimplemented` |
| `bases:add-item` | `c8.commandAddItem` | internal-plugin | — |  | `unimplemented` |
| `bases:add-view` | `c8.commandAddView` | internal-plugin | — |  | `unimplemented` |
| `bases:change-view` | `c8.commandChangeView` | internal-plugin | — |  | `unimplemented` |
| `bases:copy-table` | `c8.commandCopyTable` | internal-plugin | — |  | `unimplemented` |
| `bases:insert` | `c8.commandInsertNew` | internal-plugin | editor |  | `unimplemented` |
| `bases:new-file` | `c8.commandCreateNew` | internal-plugin | — |  | `unimplemented` |
| `bookmarks:bookmark-all-tabs` | `bd.plugins.bookmarks.actionBookmarkAllTabs` | internal-plugin | — |  | `stubbed` → `checkCallback always false` |
| `bookmarks:bookmark-current-heading` | `bd.plugins.bookmarks.actionBookmarkHeading` | internal-plugin | editor |  | `unimplemented` |
| `bookmarks:bookmark-current-search` | `bd.plugins.bookmarks.actionBookmarkSearch` | internal-plugin | — |  | `stubbed` → `checkCallback always false` |
| `bookmarks:bookmark-current-section` | `bd.plugins.bookmarks.actionBookmarkBlock` | internal-plugin | editor |  | `stubbed` → `checkCallback always false` |
| `bookmarks:bookmark-current-view` | `bd.plugins.bookmarks.actionBookmark` | internal-plugin | — |  | `implemented` → `BookmarksPlugin bookmarkFile` |
| `bookmarks:open` | `w8.actionShow` | internal-plugin | — |  | `implemented` → `BookmarksPlugin revealDockView` |
| `bookmarks:unbookmark-current-view` | `bd.plugins.bookmarks.actionRemoveBookmark` | internal-plugin | — |  | `stubbed` → `checkCallback always false` |
| `canvas:convert-to-file` | `p9.commandConvertToFile` | internal-plugin | — |  | `unimplemented` |
| `canvas:export-as-image` | `p9.actionExportPng` | internal-plugin | **desktop-only** gate:site=isDesktopApp |  | `unimplemented` |
| `canvas:jump-to-group` | `p9.actionJumpToGroup` | internal-plugin | — |  | `unimplemented` |
| `canvas:new-file` | `p9.commandCreateNewCanvas` | internal-plugin | — |  | `unimplemented` |
| `command-palette:open` | `bd.plugins.commandPalette.actionOpen` | internal-plugin | `showOnMobileToolbar` | Mod `P` | `implemented` → `KAction command_palette` |
| `daily-notes` | `T9.actionOpen` | internal-plugin | — |  | `unimplemented` |
| `daily-notes:goto-next` | `T9.actionOpenNext` | internal-plugin | — |  | `unimplemented` |
| `daily-notes:goto-prev` | `T9.actionOpenPrevious` | internal-plugin | — |  | `unimplemented` |
| `editor:add-cursor-above` | `bd.commands.addCursorAbove` | app | `repeatable` editor |  | `unimplemented` |
| `editor:add-cursor-below` | `bd.commands.addCursorBelow` | app | `repeatable` editor |  | `unimplemented` |
| `editor:attach-file` | `bd.setting.mobileToolbar.optionAttach` | app | editor |  | `unimplemented` |
| `editor:clear-formatting` | `bd.commands.clearFormatting` | app | editor |  | `unimplemented` |
| `editor:configure-toolbar` | `bd.setting.mobileToolbar.optionConfigureToolbar` | app | `mobileOnly` editor |  | `unimplemented` |
| `editor:context-menu` | `bd.commands.contextMenu` | app | editor |  | `unimplemented` |
| `editor:copy` | `bd.interface.menu.copy` | app | `mobileOnly` **mobile-app-only** editor gate:site=isMobileApp |  | `unimplemented` |
| `editor:cut` | `bd.interface.menu.cut` | app | `mobileOnly` **mobile-app-only** editor gate:site=isMobileApp |  | `unimplemented` |
| `editor:cycle-list-checklist` | `bd.commands.cycleListChecklist` | app | editor |  | `unimplemented` |
| `editor:delete-paragraph` | `bd.commands.deleteParagraph` | app | `repeatable` editor | Mod `D` | `unimplemented` |
| `editor:download-attachments` | `bd.commands.downloadAttachments` | workspace | — |  | `unimplemented` |
| `editor:focus` | `bd.commands.focusEditor` | app | — |  | `unimplemented` |
| `editor:focus-bottom` | `t` | workspace | — |  | `unimplemented` |
| `editor:focus-left` | `t` | workspace | — |  | `unimplemented` |
| `editor:focus-right` | `t` | workspace | — |  | `unimplemented` |
| `editor:focus-top` | `t` | workspace | — |  | `unimplemented` |
| `editor:fold-all` | `bd.commands.foldAll` | app | `allowPreview` `allowProperties` editor |  | `unimplemented` |
| `editor:fold-less` | `bd` | app | editor |  | `unimplemented` |
| `editor:fold-more` | `bd.commands.foldMore` | app | editor |  | `unimplemented` |
| `editor:follow-link` | `bd.commands.followCursorLink` | workspace | — | Alt `Enter` | `unimplemented` |
| `editor:go-end` | `bd.setting.mobileToolbar.optionLastLine` | app | `mobileOnly` editor |  | `unimplemented` |
| `editor:go-start` | `bd.setting.mobileToolbar.optionFirstLine` | app | `mobileOnly` editor |  | `unimplemented` |
| `editor:indent-list` | `bd.setting.mobileToolbar.optionIndentList` | app | editor |  | `unimplemented` |
| `editor:insert-callout` | `bd.commands.insertCallout` | app | editor |  | `unimplemented` |
| `editor:insert-codeblock` | `bd.commands.insertCodeBlock` | app | editor |  | `unimplemented` |
| `editor:insert-embed` | `bd.setting.mobileToolbar.optionInternalEmbed` | app | editor |  | `unimplemented` |
| `editor:insert-footnote` | `bd.commands.insertFootnote` | app | editor |  | `unimplemented` |
| `editor:insert-horizontal-rule` | `bd.commands.insertHorizontalRule` | app | editor |  | `unimplemented` |
| `editor:insert-link` | `bd.commands.insertLink` | app | editor | Mod `K` | `unimplemented` |
| `editor:insert-mathblock` | `bd.commands.insertMathBlock` | app | editor |  | `unimplemented` |
| `editor:insert-table` | `bd.commands.insertTable` | app | editor |  | `unimplemented` |
| `editor:insert-tag` | `bd.setting.mobileToolbar.optionTag` | app | editor |  | `unimplemented` |
| `editor:insert-wikilink` | `bd.setting.mobileToolbar.optionInternalLink` | app | editor |  | `unimplemented` |
| `editor:move-caret-down` | `bd.setting.mobileToolbar.optionMoveCaretDown` | app | `mobileOnly` editor |  | `unimplemented` |
| `editor:move-caret-left` | `bd.setting.mobileToolbar.optionMoveCaretLeft` | app | `mobileOnly` editor |  | `unimplemented` |
| `editor:move-caret-right` | `bd.setting.mobileToolbar.optionMoveCaretRight` | app | `mobileOnly` editor |  | `unimplemented` |
| `editor:move-caret-up` | `bd.setting.mobileToolbar.optionMoveCaretUp` | app | `mobileOnly` editor |  | `unimplemented` |
| `editor:open-link-in-new-leaf` | `bd.commands.openCursorLinkInNewTab` | workspace | — | Mod `Enter` | `unimplemented` |
| `editor:open-link-in-new-split` | `bd.commands.openCursorLinkToTheRight` | workspace | — | Mod+Alt `Enter` | `unimplemented` |
| `editor:open-link-in-new-window` | `bd.commands.openCursorLinkInNewWindow` | workspace | — | Mod+Alt+Shift `Enter` | `unimplemented` |
| `editor:open-search` | `bd.commands.searchCurrentFile` | app | — | Mod `F` | `unimplemented` |
| `editor:open-search-replace` | `bd.commands.searchReplaceCurrentFile` | app | — | Mod+Alt `F` <i>when Zp</i><br>Mod `H` <i>when !Zp</i> | `unimplemented` |
| `editor:paste` | `bd.interface.menu.paste` | app | `mobileOnly` **mobile-app-only** editor gate:site=isMobileApp |  | `unimplemented` |
| `editor:redo` | `bd.setting.mobileToolbar.optionRedo` | app | `mobileOnly` editor |  | `unimplemented` |
| `editor:rename-heading` | `bd.interface.menu.renameHeading` | workspace | editor |  | `unimplemented` |
| `editor:save-file` | `bd.commands.saveFile` | workspace | — | Mod `S` | `implemented` → `KAction file_save` |
| `editor:set-heading` | `bd.setting.mobileToolbar.optionHeading` | app | editor |  | `unimplemented` |
| `editor:set-heading-0` | `bd.commands.removeHeading` | app | editor |  | `unimplemented` |
| `editor:set-heading-1` | `bd.commands.toggleHeading` | app | editor |  | `unimplemented` |
| `editor:set-heading-2` | `bd.commands.toggleHeading` | app | editor |  | `unimplemented` |
| `editor:set-heading-3` | `bd.commands.toggleHeading` | app | editor |  | `unimplemented` |
| `editor:set-heading-4` | `bd.commands.toggleHeading` | app | editor |  | `unimplemented` |
| `editor:set-heading-5` | `bd.commands.toggleHeading` | app | editor |  | `unimplemented` |
| `editor:set-heading-6` | `bd.commands.toggleHeading` | app | editor |  | `unimplemented` |
| `editor:swap-line-down` | `bd.commands.swapLineDown` | app | `repeatable` editor |  | `unimplemented` |
| `editor:swap-line-up` | `bd.commands.swapLineUp` | app | `repeatable` editor |  | `unimplemented` |
| `editor:table-col-after` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-col-align-center` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-col-align-left` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-col-align-right` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-col-before` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-col-copy` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-col-delete` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-col-left` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-col-right` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-row-after` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-row-before` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-row-copy` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-row-delete` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-row-down` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:table-row-up` | `bd.table.labelCommand` | app | editor |  | `unimplemented` |
| `editor:toggle-blockquote` | `bd.setting.mobileToolbar.optionBlockquote` | app | editor |  | `unimplemented` |
| `editor:toggle-bold` | `bd.commands.toggleBold` | app | editor | Mod `B` | `unimplemented` |
| `editor:toggle-bullet-list` | `bd.setting.mobileToolbar.optionBulletList` | app | editor |  | `unimplemented` |
| `editor:toggle-checklist-status` | `bd.commands.toggleChecklist` | app | editor | Mod `l` | `unimplemented` |
| `editor:toggle-code` | `bd.setting.mobileToolbar.optionCode` | app | editor |  | `unimplemented` |
| `editor:toggle-comments` | `bd.commands.toggleComments` | app | editor | Mod `/` | `unimplemented` |
| `editor:toggle-fold` | `bd.commands.toggleFold` | app | `allowProperties` editor |  | `unimplemented` |
| `editor:toggle-fold-properties` | `bd.commands.toggleFoldProperties` | app | — |  | `unimplemented` |
| `editor:toggle-highlight` | `bd.commands.toggleHighlight` | app | editor |  | `unimplemented` |
| `editor:toggle-inline-math` | `bd.setting.mobileToolbar.optionInlineMath` | app | editor |  | `unimplemented` |
| `editor:toggle-italics` | `bd.commands.toggleItalics` | app | editor | Mod `I` | `unimplemented` |
| `editor:toggle-keyboard` | `bd.setting.mobileToolbar.optionToggleKeyboard` | app | `mobileOnly` editor |  | `unimplemented` |
| `editor:toggle-line-numbers` | `bd.commands.toggleLineNumbers` | app | — |  | `unimplemented` |
| `editor:toggle-numbered-list` | `bd.setting.mobileToolbar.optionNumberedList` | app | editor |  | `unimplemented` |
| `editor:toggle-readable-line-length` | `bd.commands.toggleReadableLineLength` | app | — |  | `unimplemented` |
| `editor:toggle-source` | `bd.commands.toggleSourceMode` | app | — |  | `unimplemented` |
| `editor:toggle-spellcheck` | `bd.commands.toggleSpellcheck` | app | — |  | `unimplemented` |
| `editor:toggle-strikethrough` | `bd.setting.mobileToolbar.optionStrikethrough` | app | editor |  | `unimplemented` |
| `editor:undo` | `bd.setting.mobileToolbar.optionUndo` | app | `mobileOnly` editor |  | `unimplemented` |
| `editor:unfold-all` | `bd.commands.unfoldAll` | app | `allowPreview` `allowProperties` editor |  | `unimplemented` |
| `editor:unindent-list` | `bd.setting.mobileToolbar.optionUnindentList` | app | editor |  | `unimplemented` |
| `file-explorer:duplicate-file` | — | app | — |  | `unimplemented` |
| `file-explorer:move-file` | `bd.plugins.fileExplorer.commandMoveFile` | app | — |  | `unimplemented` |
| `file-explorer:new-file` | `bd.plugins.fileExplorer.actionCreateNote` | app | — | Mod `N` | `unimplemented` |
| `file-explorer:new-file-in-current-tab` | `bd.plugins.fileExplorer.actionCreateNoteInCurrentTab` | app | — |  | `unimplemented` |
| `file-explorer:new-file-in-new-pane` | `bd.plugins.fileExplorer.actionCreateNoteToTheRight` | app | — | Mod+Shift `N` | `unimplemented` |
| `file-explorer:new-folder` | `O9.actionCreateFolder` | internal-plugin | — |  | `unimplemented` |
| `file-explorer:open` | `O9.actionShow` | internal-plugin | — |  | `unimplemented` |
| `file-explorer:reveal-active-file` | `O9.actionRevealActiveFile` | internal-plugin | — |  | `implemented` → `FileExplorerPlugin revealPath` |
| `file-recovery:open` | `N9.actionOpen` | internal-plugin | — |  | `unimplemented` |
| `footnotes:open` | `q9.actionShow` | internal-plugin | — |  | `unimplemented` |
| `global-search:open` | `$1.actionOpenSearch` | internal-plugin | — | Mod+Shift `F` | `implemented` → `KAction search_vault` |
| `graph:animate` | `i0.actionTimelapse` | internal-plugin | — |  | `unimplemented` |
| `graph:open` | `i0.actionOpen` | internal-plugin | — | Mod `G` | `implemented` → `KAction graph_view` |
| `graph:open-local` | `i0.actionOpenLocal` | internal-plugin | — |  | `implemented` → `LocalGraphPlugin revealDockView` |
| `insert-current-date` | `bd.plugins.templates.actionInsertCurrentDate` | internal-plugin | editor |  | `unimplemented` |
| `insert-current-time` | `bd.plugins.templates.actionInsertCurrentTime` | internal-plugin | editor |  | `unimplemented` |
| `insert-template` | `bd.plugins.templates.actionInsert` | internal-plugin | — |  | `unimplemented` |
| `insert-unique-link` | `Dne.actionAddLink` | internal-plugin | editor |  | `unimplemented` |
| `markdown-importer:open` | `K9.actionOpen` | internal-plugin | — |  | `unimplemented` |
| `markdown:add-alias` | `bd.commands.addAlias` | app | — |  | `unimplemented` |
| `markdown:add-metadata-property` | `bd.commands.addProperty` | app | — | Mod `;` | `implemented` → `MainWindow inline (real callback)` |
| `markdown:clear-metadata-properties` | `bd.commands.clearProperties` | app | — |  | `unimplemented` |
| `markdown:toggle-preview` | `bd.commands.togglePreview` | app | — | Mod `E` | `unimplemented` |
| `mobile:quick-action` | `bie.optionConfigureQuickAction` | internal-plugin | — |  | `unimplemented` |
| `note-composer:extract-heading` | `tee.commandExtractHeading` | internal-plugin | editor |  | `unimplemented` |
| `note-composer:merge-file` | `tee.commandMergeFile` | internal-plugin | — |  | `unimplemented` |
| `note-composer:split-file` | `tee.commandSplitFile` | internal-plugin | editor |  | `unimplemented` |
| `open-with-default-app:open` | — | app | — |  | `unimplemented` |
| `open-with-default-app:show` | — | app | **desktop-only** gate:site=isDesktopApp |  | `unimplemented` |
| `outgoing-links:open` | `see.actionShow` | internal-plugin | — |  | `implemented` → `OutlinksPlugin revealDockView` |
| `outgoing-links:open-for-current` | `see.actionOpenForCurrent` | internal-plugin | — |  | `unimplemented` |
| `outline:open` | `bd.plugins.outline.actionShow` | internal-plugin | — |  | `implemented` → `OutlinePlugin revealDockView` |
| `outline:open-for-current` | `bd.plugins.outline.actionOpenForCurrent` | internal-plugin | — |  | `unimplemented` |
| `properties:open` | `wee.actionShow` | internal-plugin | — |  | `implemented` → `PropertiesPlugin revealDockView` |
| `properties:open-local` | `wee.actionShowLocal` | internal-plugin | — |  | `unimplemented` |
| `publish:open-in-live-site` | `mte.labelOpenInLiveSite` | internal-plugin | — |  | `unimplemented` |
| `publish:publish-file` | `mte.actionPublishFile` | internal-plugin | — |  | `unimplemented` |
| `publish:view-changes` | `mte.actionPublishChanges` | internal-plugin | — |  | `unimplemented` |
| `random-note` | `bd.plugins.randomNote.actionOpen` | internal-plugin | — |  | `unimplemented` |
| `slides:start` | `bd.plugins.slides.actionStart` | internal-plugin | — |  | `unimplemented` |
| `switcher:open` | `yte.actionOpen` | internal-plugin | — | Mod `O` | `unimplemented` |
| `sync:open-sync-log` | `une.actionActivityLog` | internal-plugin | — |  | `unimplemented` |
| `sync:open-sync-view` | `une.actionViewSyncSidebar` | internal-plugin | — |  | `unimplemented` |
| `sync:setup` | `bd.plugins.sync.actionSetupSync` | internal-plugin | — |  | `unimplemented` |
| `sync:view-version-history` | `bd.plugins.sync.actionViewVersionHistory` | internal-plugin | — |  | `unimplemented` |
| `tag-pane:open` | `mne.actionShow` | internal-plugin | — |  | `unimplemented` |
| `theme:switch` | `bd.commands.changeTheme` | app | — |  | `unimplemented` |
| `theme:toggle-light-dark` | `bd.commands.toggleLightDarkMode` | app | — |  | `unimplemented` |
| `webviewer:focus-address-bar` | `L4.actionFocusAddressBar` | internal-plugin | — |  | `unimplemented` |
| `webviewer:open` | `L4.actionOpen` | internal-plugin | — |  | `unimplemented` |
| `webviewer:open-history` | `L4.actionShowHistory` | internal-plugin | — |  | `unimplemented` |
| `webviewer:save-to-vault` | `L4.actionSaveToVault` | internal-plugin | — |  | `unimplemented` |
| `webviewer:search` | `L4.actionSearchTheWeb` | internal-plugin | — |  | `unimplemented` |
| `webviewer:toggle-reader-mode` | `L4.actionToggleReaderMode` | internal-plugin | — |  | `unimplemented` |
| `webviewer:zoom-in` | `bd.commands.zoomIn` | internal-plugin | — |  | `unimplemented` |
| `webviewer:zoom-out` | `bd.commands.zoomOut` | internal-plugin | — |  | `unimplemented` |
| `webviewer:zoom-reset` | `bd.commands.resetZoom` | internal-plugin | — |  | `unimplemented` |
| `window:reset-zoom` | `bd.commands.resetZoom` | app | **desktop-only** gate:site=isDesktopApp |  | `unimplemented` |
| `window:toggle-always-on-top` | `bd.commands.alwaysOnTop` | app | **desktop-only** gate:site=isDesktopApp |  | `unimplemented` |
| `window:zoom-in` | `bd.commands.zoomIn` | app | **desktop-only** gate:site=isDesktopApp |  | `unimplemented` |
| `window:zoom-out` | `bd.commands.zoomOut` | app | **desktop-only** gate:site=isDesktopApp |  | `unimplemented` |
| `workspace:close` | `bd.commands.closeActiveTab` | workspace | — | Mod `W` | `implemented` → `KAction tab_close` |
| `workspace:close-others` | `bd.commands.closeOtherTabs` | workspace | — |  | `unimplemented` |
| `workspace:close-others-tab-group` | `bd.commands.closeOthersInTabGroup` | workspace | — |  | `unimplemented` |
| `workspace:close-tab-group` | `bd.commands.closeTabGroup` | workspace | — |  | `unimplemented` |
| `workspace:close-window` | `bd.interface.window.closeWindow` | workspace | — | Mod+Shift `W` | `unimplemented` |
| `workspace:copy-full-path` | `bd.commands.copyFullPath` | workspace | **desktop-only** gate:site=isDesktopApp |  | `unimplemented` |
| `workspace:copy-path` | `bd.commands.copyPath` | workspace | — |  | `unimplemented` |
| `workspace:copy-url` | `bd.commands.copyUrl` | workspace | — |  | `unimplemented` |
| `workspace:edit-file-title` | `bd.commands.editFileTitle` | workspace | — | — `F2` | `unimplemented` |
| `workspace:export-pdf` | `bd.commands.exportPdf` | workspace | — |  | `unimplemented` |
| `workspace:goto-last-tab` | `bd.commands.goToLastTab` | workspace | — | Mod `9` | `implemented` → `KAction tab_jump_last` |
| `workspace:goto-tab-1` | `bd.commands.goToNthTab` | workspace | — | Mod <code>{1|2|3|4|5|6|7|8}</code> | `unimplemented` |
| `workspace:goto-tab-2` | `bd.commands.goToNthTab` | workspace | — | Mod <code>{1|2|3|4|5|6|7|8}</code> | `unimplemented` |
| `workspace:goto-tab-3` | `bd.commands.goToNthTab` | workspace | — | Mod <code>{1|2|3|4|5|6|7|8}</code> | `unimplemented` |
| `workspace:goto-tab-4` | `bd.commands.goToNthTab` | workspace | — | Mod <code>{1|2|3|4|5|6|7|8}</code> | `unimplemented` |
| `workspace:goto-tab-5` | `bd.commands.goToNthTab` | workspace | — | Mod <code>{1|2|3|4|5|6|7|8}</code> | `unimplemented` |
| `workspace:goto-tab-6` | `bd.commands.goToNthTab` | workspace | — | Mod <code>{1|2|3|4|5|6|7|8}</code> | `unimplemented` |
| `workspace:goto-tab-7` | `bd.commands.goToNthTab` | workspace | — | Mod <code>{1|2|3|4|5|6|7|8}</code> | `unimplemented` |
| `workspace:goto-tab-8` | `bd.commands.goToNthTab` | workspace | — | Mod <code>{1|2|3|4|5|6|7|8}</code> | `unimplemented` |
| `workspace:move-to-new-window` | `bd.commands.moveToNewWindow` | workspace | — |  | `implemented` → `KAction tab_move_to_new_window` |
| `workspace:new-tab` | `bd.commands.newTab` | workspace | — | Mod `T` | `unimplemented` |
| `workspace:new-window` | `bd.commands.newWindow` | workspace | — |  | `unimplemented` |
| `workspace:next-tab` | `bd.commands.goToNextTab` | workspace | — | Ctrl `Tab`<br>Meta+Shift `]` <i>when isMacOS</i><br>Ctrl `PageDown` <i>when !isMacOS</i> | `implemented` → `KAction tab_next` |
| `workspace:open-in-new-window` | `bd.commands.openInNewWindow` | workspace | — |  | `unimplemented` |
| `workspace:previous-tab` | `bd.commands.goToPrevTab` | workspace | — | Ctrl+Shift `Tab`<br>Meta+Shift `[` <i>when isMacOS</i><br>Ctrl `PageUp` <i>when !isMacOS</i> | `implemented` → `KAction tab_prev` |
| `workspace:show-trash` | `bd.commands.showTrash` | workspace | **desktop-only** gate:site=isDesktopApp |  | `unimplemented` |
| `workspace:split-horizontal` | `bd.commands.splitDown` | workspace | — |  | `implemented` → `KAction split_down` |
| `workspace:split-vertical` | `bd.commands.splitRight` | workspace | — |  | `implemented` → `KAction split_right` |
| `workspace:toggle-pin` | `bd.commands.togglePin` | workspace | — |  | `implemented` → `KAction tab_pin_toggle` |
| `workspace:toggle-stacked-tabs` | `bd.commands.toggleStackedTabs` | workspace | — |  | `implemented` → `KAction tab_toggle_stacked` |
| `workspace:undo-close-pane` | `bd.commands.undoCloseTab` | workspace | — | Mod+Shift `T` | `implemented` → `KAction tab_undo_close` |
| `workspaces:load` | `Rne.actionLoadLayout` | internal-plugin | — |  | `unimplemented` |
| `workspaces:open-modal` | `Rne.actionManageLayouts` | internal-plugin | — |  | `unimplemented` |
| `workspaces:save` | `Rne.actionSave` | internal-plugin | — |  | `unimplemented` |
| `workspaces:save-and-load` | `Rne.actionSaveAndLoadLayout` | internal-plugin | — |  | `unimplemented` |
| `zk-prefixer` | `Dne.actionCreateNote` | internal-plugin | — |  | `unimplemented` |

## Forwarding call sites (excluded from the count)

`addCommand`/`registerGlobalCommand` with a non-literal argument. These
are the plugin API's own entry points, not core registrations.

| Site | Callee | Argument |
|---|---|---|
| `app/own/1938-c4.js:63` | `addCommand` | `e` |
| `app/own/1939-m4.js:560` | `addCommand` | `s` |

