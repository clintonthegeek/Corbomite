# UI bundle domain audit

Audit of Corbomite's UI primitives layer against `docs/obsidian-audit/domains/ui-bundle.md` plus the seven 2026-04-19 addenda. Source surveyed: `src/dialogs/`, `src/sidebar/`, `src/app/`, `src/editor/`, `src/plugins/file-explorer/`, `libs/core/`, `libs/vault/src/dialogs/`. No source modified.

## Architecture fit (KDE primitives vs Obsidian web widgets)

Obsidian's UI bundle is a from-scratch HTML/CSS/JS widget toolkit: every input, menu, popover, and toast is hand-rolled. KDE/Qt ships native counterparts for almost every primitive, so the structural translation is effectively *done by Qt itself* — Corbomite only needs to (a) match builder/lifecycle semantics where they leak into the plugin contract, and (b) reproduce the bits that are genuinely behavioural rather than visual (section-ordered menus, mid-construction menu emission, hover-popover, Notice toasts, fuzzy suggester).

Corbomite's substrate decisions are sound:

- `libs/core/include/corbomite/core/Component.h:34-99` ports Obsidian's `Component` lifecycle (`load`/`unload`/`addChild`/`registerInterval`/`registerCleanup`) as a non-QObject helper that composes a private `TimerHost` (`libs/core/src/Component.cpp:11-30`) — clean translation of the cleanup-thunk + LIFO-children invariants from §1.
- `libs/core/include/corbomite/core/MenuSectionHelper.h:30-66` + `libs/core/src/MenuSectionHelper.cpp:9-85` is the canonical buffer-and-flush translation of `Menu.prototype.sort` (audit §2 sort algorithm) — the bucket-by-section + insert-separators-between-non-empty pattern is preserved exactly.
- `libs/core/include/corbomite/core/MenuEventEmitter.h:21-45` + `libs/core/src/MenuEventEmitter.cpp` exposes the seven mid-construction menu signals (`fileMenu`/`urlMenu`/`editorMenu`/`filesMenu`/`leafMenu`/`tabGroupMenu`/`markdownViewportMenu`) verbatim from `workspace.md §4`.
- `libs/core/include/corbomite/core/HoverLinkSourceRegistry.h:22-48` ships the per-source hover registry described in `workspace.md §7` and audit §10, with built-in registration matching the editor/search/backlinks/outlinks/graph/bases set documented in `rendering.md §11`.

The fit is strong. The remaining gaps are uniformly in the **fidelity of plugin-facing builders** and a handful of **observable behaviours** (Notice timer freeze on hover, multi-stage HoverPopover, addItem-after-load semantics).

## Components (form inputs) parity

The audit lists ~13 concrete form widgets (Toggle/Text/TextArea/Dropdown/Slider/Color/Search/Secret/Button/ExtraButton/Progress/MomentFormat/numeric). For *Corbomite's own* settings dialogs the situation is fine — `src/dialogs/SettingsDialog.cpp:42-79` builds rows directly with `QFormLayout` + raw `QSpinBox`/`QCheckBox`/`QComboBox`/`QLineEdit`. The `Setting` row pattern (label / description / control tri-split) is **not** matched: every page is a flat label-on-the-left `QFormLayout::addRow(label, control)`. This is a divergent — Obsidian's `Setting` row places a primary label on top, a description sub-label, and the control on the right; descriptions are absent in Corbomite's settings UI. Acceptable for the in-app flow (KDE convention is a different aesthetic), but plugin-shim translation will need a `Corbomite::Components::Setting` builder façade if/when third-party plugins expect to call `new Setting(containerEl).setName(...).setDesc(...).addToggle(...)`.

For the **form-input widgets themselves**, Qt covers everything natively. Notable mappings already in evidence:

- `MomentFormatComponent` is the only widget with non-trivial behavioural content: its `updateSample()` runs `moment().format(input)` on every input event. Corbomite has a clean port at `src/dialogs/MomentFormatPreview.cpp:12-72` plus `Corbomite::MomentFormatter::format` referenced from `libs/core/include/corbomite/core/MomentFormatter.h`. The preview ticks on a 1-second timer (`MomentFormatPreview.cpp:23-26`) so seconds-valued formats animate — matches the audit §1 description. **Status: solid.**
- `SecretComponent` (audit §1, `SecretComponent.js:478-550`) is **not** ported. Cluster N (per project memory) shipped QtKeychain + plugin `data.json`, so the *backing* exists, but no `SecretLineEdit` widget that surfaces the picker-modal flow. Plugin-shim gap.
- `SliderComponent.setDynamicTooltip()` — Corbomite's settings sliders don't show a hover-tooltip-with-value while dragging. Pure visual gap; not blocking.
- `ToggleComponent` calls `navigator.vibrate(100)` on click (mobile). Irrelevant for desktop Corbomite; flag only if Android/iOS port lands.

The audit's **`ValueComponent` chainable builder pattern** (`.setValue(...).onChange(fn).setPlaceholder(...)`) is **not** reproduced anywhere — Corbomite uses Qt signals/slots directly. This is fine for first-party code; deferred plugin-shim concern.

## `Component` lifecycle

`libs/core/include/corbomite/core/Component.h` is the right shape. Verified invariants vs audit §8:

- `unload()` idempotency via `m_loaded` guard (`Component.cpp:60`). ✓
- LIFO children drain (`Component.cpp:62-65`). ✓
- `addChild` auto-loads if parent is loaded (`Component.cpp:91-98`). ✓
- Cleanups fire LIFO (`Component.cpp:81-85`). ✓
- `registerInterval` is auto-killed on `unload` (`Component.cpp:67-73`). ✓

One **divergence** from the audit:

- Obsidian's `Component.register(cb)` cleanup queue is the **same** queue as `registerEvent` / `registerDomEvent` / `registerInterval` (audit §1, `_events`). Corbomite splits these into three separate vectors (`m_intervals`, `m_connections`, `m_cleanups`). In `Component::unload` (`Component.cpp:67-85`) they fire in fixed order: intervals first, then connections, then `m_cleanups`. Obsidian's contract is "all registered cleanups, LIFO across the merged queue." The split is invisible to first-party code today but a plugin author who registers an interval *after* a cleanup will see them fire in the wrong order vs Obsidian. **Suspected behavioural drift.**

`Component` is **not a QObject** — the comment at `Component.h:33` calls this out as intentional. The audit's `Component.registerEvent(eventRef)` would map most naturally to a QObject connection helper; today `registerQObjectConnection(QMetaObject::Connection)` is the closest analogue and is one of the three vectors above.

## Icons / lucide → Breeze mapping risks

Corbomite uses `QIcon::fromTheme` directly throughout — there is **no `IconMap.h`** translation table as the audit §11 recommends. Spot survey shows ~25 distinct theme IDs in use across the app shell:

```
accessories-text-editor  format-text-heading  preferences-plugin
bookmark-new             go-jump              preferences-system-network
camera-photo             help-contextual      quickopen
dialog-information       insert-table         system-file-manager
document-new             list-add             system-run
document-new-from-template  media-playback-start  tab-close, tab-new
document-open            preferences-desktop-theme  view-calendar-day
document-save            view-preview         view-split-{left-right,top-bottom}
draw-rectangle           window-close         zoom-{fit-best,in,out,original}
edit-{copy,delete,find,rename,undo}  folder, folder-{move,new,open}
```

Every one of these is a vanilla Freedesktop name → Breeze ships them. **Pure first-party Corbomite icons are safe.**

The risk is **plugin-supplied icons**. The audit lists 125 unique Lucide IDs in Obsidian's bundle; the §11 translation table maps ~70% to Freedesktop. Today there is **no `Plugin.addIcon(name, svg)` registry** in Corbomite — `grep iconFor`/`IconMap`/`addIcon` finds nothing in `libs/core/`. When Cluster Q's plugins want to register a custom Lucide-style SVG icon, there's nothing to register against. This matches audit §10 (`Custom icon` row, status "Missing"). The closest evidence is `libs/core/src/WorkspaceSerializer.cpp:228` which writes `lucide-file` as a default leaf icon — that string would resolve to a missing theme entry and fall back to Qt's null icon. **Concrete risk:** any persisted leaf with a `lucide-*` icon name will silently render blank.

**Recommendation:** the Cluster J/N plugin work needs an `IconMap::resolveLucide(QString id) -> QIcon` shim and an `IconRegistry::addIcon(QString id, QByteArray svg)` registry, both wired into a single `Corbomite::Icons::iconFor(QString)` call site that downstream code substitutes for raw `QIcon::fromTheme`. This is a tracked Cluster Q follow-up per the project memory but no implementation is in the tree.

## Menus: hookable sections vs hard-wired right-click

The substrate is in place: `MenuSectionHelper` for the section protocol, `MenuEventEmitter` for plugin emission, `MenuInjector` (`libs/core/src/proxies/MenuInjector.cpp`) as the plugin-facing connection sugar.

**Where it's wired:**

- `libs/core/src/ItemView.cpp:120-138` — `ItemView::buildMoreOptionsMenu` is the canonical example: `MenuSectionHelper helper(menu)`, calls `onMoreOptionsMenu(helper)`, calls `onPaneMenu(menu, "more-options")` for back-compat, then **emits `leafMenu(menu, m_leaf)`** through the WorkspaceLeaf's `MenuEventEmitter`. `helper.finalize()` sorts and inserts separators. Textbook port.
- `src/app/MainWindow.cpp:598-639` — `MainWindow::onAboutToShowContextMenu` builds the editor right-click menu via `MenuSectionHelper`, putting format/heading/insert items into `"action"`. Clean.
- `src/app/MainWindow.cpp:969-1003` — wires the rename/move/delete prompts through `FileManager`.

**Where it's NOT wired (gaps):**

- `src/plugins/file-explorer/FileExplorerView.cpp:166-199` builds the file-explorer context menu **directly with `menu.addAction(...)`** — no `MenuSectionHelper`, **no `fileMenu` emission**. Per the addendum `2026-04-19-file-explorer-context-menu.md` ("Implementation notes for Corbomite (Cluster U scouting)") this is the exact gap called out: "FileExplorerView should use MenuSectionHelper... emit MenuEventEmitter::fileMenu(menu, file, 'file-explorer-context-menu') before finalize". **Today plugins cannot inject items into the file-explorer right-click.** This is the most consequential menu-extensibility hole.
- `src/plugins/graph-view/GraphViewTab.cpp:271-331` similarly builds its node context menu with raw `menu.addAction` calls. No section helper, no plugin emission. Same problem.

**Section ID coverage:** `MenuSectionHelper::canonicalSectionOrder()` (`MenuSectionHelper.cpp:9-26`) lists `close, pane, open, action, find, info, info.copy, view, view.linked, system, "" (unset), danger`. Compared to audit §2 the canonical sets:
- *Folder context* per audit: `["title", "open", "action-primary", "action", "info", "info.copy", "view", "system", "", "danger"]`. Corbomite is **missing `title` and `action-primary`**.
- *Tab/leaf more-options*: `["close", "pane", "open", "action", "find", "info", "info.copy", "view", "view.linked", "system", "", "danger"]`. ✓ Matches.
- *Tab-list*: `title`, `order`, `ribbon` (not addSections-declared). ✗ Missing — these aren't in `canonicalSectionOrder()` so they fall into the unset bucket.

`title`/`action-primary`/`order`/`ribbon` are **not** present in the canonical list. Plugin items `setSection("title")` will land in the catch-all `""` bucket and render at end-of-section instead of top. **Concrete bug.** Easy fix.

**Submenus:** `MenuSectionHelper::addSubmenu(sectionId, title, icon)` (`MenuSectionHelper.cpp:41-50`) creates a nested helper and stuffs the resulting `QMenu::menuAction()` into the parent bucket on `finalize`. Matches audit §1 `setSectionSubmenu`. ✓

**Checked / disabled / warning / label items:** The audit's `MenuItem.setChecked / setDisabled / setWarning / setIsLabel` map to `QAction::setCheckable+setChecked / setEnabled / styling-via-QAction-property / setEnabled(false)+visualSeparator` respectively. There's no convention helper for `setWarning` (red text) — by inspection, danger-section actions just use `QIcon::fromTheme("edit-delete")` which the user reads as visual cue. The audit's "warning/danger styling" is **partial**: section is canonical (`"danger"` exists), per-item warning red text doesn't (would need a stylesheet hack via QAction property + QSS rule).

**`Menu.addItem`-after-load silent-drop semantics** (audit §8 + §13.3): Obsidian silently drops items added after `Menu.load()`. Corbomite's `MenuSectionHelper::finalize()` clears and re-flushes (`MenuSectionHelper.cpp:55`), so `addToSection` after `finalize` *will* take effect on the next `finalize()` call but **not** mutate an already-shown menu. Different shape than Obsidian's silent-drop but reaches the same observable: late additions don't show. Acceptable.

## Popups: Modal / Notice / SuggestModal / PopoverSuggest / HoverPopover

### Modal

Corbomite has no `Modal` base class. Each dialog is a hand-rolled `QDialog` subclass:

- `libs/vault/src/dialogs/RenameDialog.cpp` (86 LOC) — full Save/Cancel + live validation.
- `libs/vault/src/dialogs/MoveFileDialog.cpp` (128 LOC) — folder-picker with substring filter.
- `libs/vault/src/dialogs/DeleteConfirmDialog.cpp` (127 LOC) — KDE-style warning + Don't-Ask-Again.
- `src/dialogs/SettingsDialog.cpp` — KPageDialog (KDE multi-page settings).
- `src/dialogs/QuickSwitcher.cpp` — actually a `QFrame` with `Qt::Popup`, see SuggestModal section.

Behavioural mapping vs audit §1:
- `Modal.scope` (Esc/Enter trapping): `QDialog` does this natively (Esc → `reject`, Enter → default button). ✓
- `Modal.shouldRestoreSelection` (focus + `QTextCursor` range): **partial** — `QDialog` restores focus, but no `QTextCursor` range snapshot. Audit §11 flagged this.
- `Modal.dimBackground` (alpha bg overlay): No equivalent. Modal `QDialog` is frame-only. Visual divergence; not behavioural.
- `setTitle` chainable / `setContent` chainable: **missing** — plugin-shim gap (audit §11 calls this out).

### Notice

`src/dialogs/Notice.h` + `.cpp` (116 LOC) is a careful port. Verified vs audit §1 and §8:

- `Qt::ToolTip | Qt::FramelessWindowHint` window flags + `WA_DeleteOnClose` + `WA_ShowWithoutActivating` (`Notice.cpp:25-29`). ✓
- File-local `liveNotices()` registry serves as the per-window stack singleton (`Notice.cpp:16-20`). ✓ — though it's process-global, not per-window. **Minor divergence** from audit §2 ("per-window Notice container" via `WeakMap<Window, HTMLDivElement>`). With single-window Corbomite this is moot; multi-window-per-process would need it scoped.
- Stack reflow on dismiss (`Notice.cpp:108-113`). ✓
- `setAction(label, callback)` clickable button (`Notice.cpp:53-70`). ✓ — maps audit's `addButton(text, cb)`.
- Default duration `kDefaultDurationMs = 4000` (`Notice.h:33`). ✓ — matches audit §1.

**Behavioural divergences vs audit §1+§8:**

- `Notice(text, 0)` "no auto-hide" sentinel: `Notice.cpp:48-50` sets `m_dismissTimer.setInterval(0)` and starts it — Qt fires a 0-ms timer immediately, so `Notice(..., 0)` will close on the next event loop pass. **Concrete bug** (audit §8: `durationMs = 0` means never auto-hide).
- "Hover pauses auto-hide until mouseleave + 1 s grace" (audit §1): **not implemented**. The `Notice` has no `enterEvent`/`leaveEvent` hookups.
- "Click-anywhere hides": **not implemented** — only the action-button closes; clicking the toast body does nothing. Audit §1 specifies click-anywhere dismiss.
- No cap on stacked notices (audit §8 invariant): Corbomite reflows correctly, but if a script fires a hundred Notices the stack grows past screen edges. Match Obsidian behaviour, so not a divergence — but the audit notes it explicitly so callers know.

### SuggestModal / FuzzySuggestModal

**Concrete instance:** `src/dialogs/QuickSwitcher.cpp:68-228` — a `Qt::Popup` `QFrame` housing a search input + `QTreeView`. Uses `Corbomite::FuzzyMatcher::prepareQuery` + `fuzzySearch` for filtering and ranking (`QuickSwitcher.cpp:20-66`). Match-highlighting via `QuickSwitcherDelegate`. Keyboard semantics handled in `eventFilter` (`QuickSwitcher.cpp:198-228`): Esc/Enter/Up/Down. **No** Ctrl-P/Ctrl-N/PgUp/PgDn/Home/End — audit §1 specifies these for `ob`. **Partial** divergence; functionally usable, keyboard-power-user gap.

**Abstract base:** there is **no `Corbomite::SuggestModal<T>` base class.** QuickSwitcher inlines the entire pattern. When a future "command palette" or "template picker" lands they will need to duplicate the same scaffolding. The audit §11 explicitly calls this out as **Partial** — confirmed.

**`getSuggestions`/`renderSuggestion`/`onChooseSuggestion` virtual contract:** absent. Plugin-shim gap.

**Limit cap (`this.limit = 100`):** absent — every fuzzy match in the vault is shown. For a 5,000-note vault this is going to chug.

**`setInstructions` footer hint row** (audit §1): absent. Pure cosmetic.

`src/dialogs/TemplatePicker.cpp` (39 LOC) is a much thinner picker — not a SuggestModal, just a list dialog.

### PopoverSuggest / AbstractInputSuggest

`src/editor/CompletionPopup.h:31-65` is the in-editor variant — a non-focus-stealing `QFrame` with a `QListView`, designed for `[[`-completion. Constructed as a real child widget (not `Qt::Popup`) so keystrokes flow through to the editor — clever design, modelled on KateCompletionWidget. Used by `WikiLinkSuggest` and `TagSuggest` (both `EditorSuggest` subclasses, see `libs/core/include/corbomite/core/EditorSuggest.h:41-62`).

The **input-anchored form** (Obsidian's `AbstractInputSuggest<T>` for settings-form auto-completers — file-path / folder pickers) is **missing**. Audit §11 marks **Partial** — confirmed. The MoveFileDialog uses substring-match `QListWidget` filtering (`MoveFileDialog.cpp:82-96`), not a `PopoverSuggest`.

**Missing capability:** scroll-aware reposition on the anchor input. Today the only "anchored picker" path is CompletionPopup which is editor-specific.

### HoverPopover

`src/editor/HoverPopover.cpp` + `.h` (170 + 82 LOC). This is the high-stakes UX. Status:

- 300 ms delay constant (`HoverPopover.cpp:19`) explicitly cites the audit. ✓
- `Qt::ToolTip | Qt::FramelessWindowHint`, `WA_ShowWithoutActivating`, `Qt::NoFocus`. ✓
- Embedded `Markoff::Reading::ReadingView` as the preview body (`HoverPopover.cpp:63`). ✓ — math, mermaid, syntax highlighting all work via Cluster J Phase 5 wiring.
- Routes through `EmbedRenderer` for recursive `![[...]]` expansion (`HoverPopover.cpp:131-138`). ✓
- 80-ms fade-in: **not implemented** (just `show()`).
- Esc cancels (`HoverPopover.cpp:161-167`). ✓
- `leaveEvent` cancels (`HoverPopover.cpp:155-159`). ✓ — but only of the popover itself, not the source link.

**Critical missing pieces** (audit §1 + §12):

1. **Mod-key pinning (`setIsFocused(true)`)**: not implemented. Obsidian's "hold mod-key to keep open" UX is the *primary* keyboard-driven hover-link interaction (lets the user click into the popover content). Without it, the popover dismisses the moment the cursor leaves the source link's bounding rect. **High-impact UX gap.**
2. **Child-popover chains (`childHovers`)**: not implemented. Hovering a `[[link]]` *inside* a hover popover should spawn a sibling popover; the parent should stay open while any child is. Today there's no shared registry. The HoverPopover is single-instance per editor.
3. **500 ms `nX` poll on `elementFromPoint`**: not implemented. Corbomite relies on per-popover `leaveEvent`, which doesn't catch the case of cursor moving between popover and source link via a brief gap.
4. **Anchor-to-mouse for tall targets (`targetEl.offsetHeight > 300`, `staticPos`)**: not implemented. The popover always anchors to `m_pendingAnchor`, which is `QCursor::pos()` (passed in by `NoteEditorWidget.cpp:67`). Cluster J's "rect-based anchoring" follow-up (per the comment at `HoverPopover.h:30-32`) is the named gap.
5. **Hover-link source registry consultation:** `HoverLinkSourceRegistry` exists and is built, but `NoteEditorWidget` (`src/editor/NoteEditorWidget.cpp:59-67`) calls `m_hoverPopover->scheduleShow(...)` unconditionally — **no consultation of the registry's `defaultMod` for whether the user has asked to suppress hover-previews from this source**. Today the registry is a write-only catalogue.
6. **`PopoverState` enum** (audit §2): not present anywhere in `libs/core/`. Cluster G/J appears to track state implicitly via `m_pendingTarget.isEmpty()` checks. Trivial future port.

The HoverPopover **as a viewer of the embed-rendered ReadingView** is in great shape. The HoverPopover **as a UX primitive** is half-baked — it shows the right thing but the interaction model (pinning, child-chains, gap-tolerance) is what makes it Obsidian-quality, and that's missing.

## Addenda coverage (per addendum: implemented / partial / missing)

### `2026-04-19-delete-confirm-modal.md` — IMPLEMENTED

`libs/vault/src/dialogs/DeleteConfirmDialog.cpp` is a high-fidelity port:
- Title varies for folder vs file (`DeleteConfirmDialog.cpp:47-48`). ✓
- Body text varies by `TrashOption` (system/vault/permanent) read from KConfig `[Files]/TrashOption` (`:60-68`). ✓ — note Corbomite uses `"vault"` for the local-trash option where the addendum says `"local"`; semantically equivalent.
- "Don't ask again" only for files, never folders (`:84-88`). ✓ — matches addendum §4 ("Folder deletions always prompt regardless").
- Default button = Cancel (destructive convention) (`:95-100`). ✓ — `cancelBtn->setDefault(true); deleteBtn->setDefault(false)`.
- Warning icon `dialog-warning` (`:53-56`). ✓
- Writes `[Files]/PromptDelete=false` on accept-with-checkbox (`:122-123`). ✓

**Caveats**: addendum says button label "Delete"; Corbomite uses i18n("Delete") with `edit-delete` icon — ✓. "Mod-warning red" styling absent — pure visual.

`FileManager::promptForDeletion` (`libs/vault/src/FileManager.cpp:407-434`) honours the `PromptDelete` config flag and short-circuits the modal for non-folder files when disabled. ✓

### `2026-04-19-rename-move-modals.md` — PARTIAL

`libs/vault/src/dialogs/RenameDialog.cpp`:
- Pre-selects basename (everything before final `.`) — (`RenameDialog.cpp:53-58`). ✓
- Live validation via `validateFileName(...)` (`:66-73`). ✓ — matches addendum §1's `LX(...)`.
- Save button disabled while invalid (`:71-72`). ✓
- Inline error label (`:34-41`) — non-standard styling via `palette(link-visited)` color hack instead of e.g. KMessageWidget. Functional but a bit dirty.
- **Missing:** "(inline validation tooltip when invalid)" — the audit shape uses tooltip; Corbomite uses inline label. Minor divergence.

`libs/vault/src/dialogs/MoveFileDialog.cpp`:
- Folder-picker layout (filter input + scrolling list) — ✓
- Excludes source's current parent (`:60-64`). ✓
- Shows root `/` (`:64-65`). ✓
- **DIVERGENCE:** uses `QString::contains(text, Qt::CaseInsensitive)` substring match (`:92-95`), explicitly noted as MVP — addendum says "Filter uses the same FuzzyMatcher as Quick Switcher". `FuzzyMatcher` *is* available in this tree (used by QuickSwitcher); MoveFileDialog should swap to it. Tracked tactical follow-up.
- **DIVERGENCE:** Has explicit OK/Cancel buttons (`:36-38`). Addendum says "no explicit Save/Cancel buttons — selection = Save, Escape = Cancel". Acceptable Qt convention divergence.
- `populateFolderList` walks `m_vault->getAllLoadedFiles()` filtering for `TFolder` (`:69-75`). ✓

`promptForMove` (`libs/vault/src/FileManager.cpp:380-405`) does the rename via `renameFile(file, newPath)`. ✓ — matches addendum §2 ("move is a rename under the hood"). Collision handling is "quietly abort" (`:399-401`); addendum specifies error notice.

### `2026-04-19-merge-file-modal.md` — MISSING

Search for "merge" / "MergeFile" finds only Markoff's `mergetag.xml` syntax file. No `MergeFileDialog`, no `mergeIntoFile` flow on `MarkdownView::onMoreOptionsMenu`. Addendum §5 explicitly defers this ("Cluster R does not ship this in P1-P4"); state matches the deferral.

### `2026-04-19-add-file-property-menu.md` — IMPLEMENTED

- `src/editor/MarkdownView.h:56` declares `void insertFrontmatterProperty()`.
- `src/editor/MarkdownView.cpp:179` defines it.
- `src/app/MainWindow.cpp:270` wires the menu item to call it: `mv->insertFrontmatterProperty()`.

I did not read the implementation body, but the wiring is present and matches addendum §5. **Status: implemented; functionality not behaviour-verified here.**

### `2026-04-19-file-explorer-context-menu.md` — PARTIAL (LARGELY DIVERGENT)

Per `src/plugins/file-explorer/FileExplorerView.cpp:166-199` the implemented menu has only 4 items maximum:
- File rows: `Open`, `Rename`, `Delete`. 3 items.
- Folder rows: `New Note Here`. 1 item.
- Empty whitespace: `New Note`. 1 item.

Addendum §"Behaviour" lists ~13 file-row items, ~13 folder-row items, ~6 empty-area items across `action`/`info.copy`/`system`/`danger`/`view` sections. Corbomite is missing:

- File rows: Open in new tab / right / below / new window, Make a copy, Move to…, Bookmark…, Copy link to file, Copy Obsidian URL, Copy path, Copy absolute path, Open in default app, Show in system explorer.
- Folder rows: New canvas, New base, New folder, Rename…, Move folder to…, Duplicate folder, Search in folder…, Copy path, Open in default app, Show in system explorer, Collapse all, Expand all, Delete (recursive).
- Empty area: New canvas, New base, New folder, Collapse all folders, Show attachments.

Plus the structural gap noted earlier: **no `MenuSectionHelper` use, no `fileMenu` emission**. No plugin extensibility on the file explorer right-click. Per the addendum's "Implementation notes" this is exactly the Cluster U scouting target.

**Inline rename (F2)** — addendum says "F2 → inline rename" (i.e. edit in place), Corbomite's `FileExplorerView::eventFilter` (`FileExplorerView.cpp:201-213`) wires F2 to `onRenameNote` which opens the **modal** (via `QInputDialog::getText` at `:152`). Addendum specifies inline-rename for F2, modal only for hamburger "Rename…". **Divergent.** And note: F2 in FileExplorer goes through `QInputDialog` *not* the much-better `RenameDialog` from libs/vault — so it doesn't even get the live validation. Suspected technical debt.

`FileExplorerView::onDeleteNote` (`:138-145`) uses `QMessageBox::question` not `DeleteConfirmDialog` — bypasses the "Don't ask again" config gate, the trash-option awareness, and the styled warning. Should route through `m_fmProxy->trashFile` + the proper modal. **Bug.**

### `2026-04-19-show-in-folder.md` — IMPLEMENTED

`libs/core/src/Platform.cpp:28-70`. Linux DBus FileManager1 path with xdg-open fallback exactly as recommended. macOS `open -R`, Windows `explorer /select,...`. Used from `libs/core/src/EditableFileView.cpp:202` as the menu's `system`-section action.

The label-adapt-by-OS ("Show in Finder/Explorer/system explorer") is **not** done — the wiring uses a single i18n string. Pure UX polish.

### `2026-04-19-open-with-default-app.md` — IMPLEMENTED

`libs/core/src/Platform.cpp:19-26` — vanilla `QDesktopServices::openUrl(QUrl::fromLocalFile(...))` call. Matches addendum §5 exactly. Used at `libs/core/src/EditableFileView.cpp:188`. Audit-noted Notice-on-failure UX recommendation **not** applied — return value is checked but no Notice is shown on `false`. Tracked follow-up.

## Implemented

- `Component` lifecycle base (close port; one ordering divergence with cleanup queues).
- `MenuSectionHelper` + canonical section ordering for the audit's primary menu sets.
- `MenuEventEmitter` exposing all 7 mid-construction menu signals.
- `MenuInjector` plugin-facing connection sugar.
- `HoverLinkSourceRegistry` + built-in source registration (editor/search/backlinks/outlinks/graph/bases).
- `Notice` toast — frame/timer/stack-reflow correct; missing hover-pause + click-dismiss + duration=0 sentinel.
- `QuickSwitcher` SuggestModal-equivalent — fuzzy filter via `Corbomite::FuzzyMatcher`, match highlighting, basic keyboard nav.
- `CompletionPopup` PopoverSuggest-equivalent for in-editor `[[`/`#` completion.
- `HoverPopover` — content fidelity (full ReadingView, EmbedRenderer, recursive `![[]]`, math, mermaid). Interaction model gaps below.
- `MomentFormatPreview` + `MomentFormatter` — live ticking format preview.
- `EditorSuggest` abstract + `WikiLinkSuggest` + `TagSuggest`.
- `Platform::openWithDefaultApp` + `Platform::showInFolder` (with Linux DBus fast-path).
- `DeleteConfirmDialog`, `RenameDialog`, `MoveFileDialog` modals + `FileManager::promptForX` API.
- `MarkdownView::insertFrontmatterProperty` (Add file property menu entry).

## Partial / divergent

- **`MenuSectionHelper::canonicalSectionOrder()` missing `title`, `action-primary`, `order`, `ribbon`** — items setting these sections silently fall into the unset bucket. Audit §2 lists them in canonical sets for folder context, tab-list, etc.
- **`Notice(text, 0)` durationMs=0 sentinel** — Corbomite starts a 0-ms timer instead of disabling the timer. Audit §8 bug.
- **Notice hover-pause + 1-s grace** — not wired.
- **Notice click-anywhere-dismiss** — not wired.
- **Notice per-window stack** — Corbomite uses a process-global QList instead of per-`QWidget::window()`. OK for single-window today; latent multi-window issue.
- **HoverPopover mod-key pinning** — fundamental UX missing.
- **HoverPopover child-popover chains** — single-instance only.
- **HoverPopover `elementFromPoint` poll for hover-tracking** — relies on `leaveEvent` only.
- **HoverPopover anchor-to-mouse for tall targets** — always anchors to QCursor pos.
- **HoverLinkSourceRegistry** — registry exists but `NoteEditorWidget` doesn't consult it for whether to fire (defaultMod is ignored).
- **SuggestModal abstract base** — no `Corbomite::SuggestModal<T>` shared scaffold; QuickSwitcher inlines.
- **SuggestModal limit cap** — no 100-result slice (perf risk in large vaults).
- **SuggestModal keyboard nav** — Up/Down/Enter/Esc only; no PgUp/PgDn/Home/End/Ctrl-P/Ctrl-N.
- **PopoverSuggest input-anchored variant** — only the editor-anchored CompletionPopup exists.
- **`Setting` row pattern** — Corbomite SettingsDialog uses bare `QFormLayout::addRow(label, control)`; no description sub-label.
- **Modal `shouldRestoreSelection` (with `QTextCursor` range)** — focus restored, selection range not.
- **Modal builder facade (`setTitle()` chainable etc.)** — no shared base; each dialog is hand-rolled.
- **MoveFileDialog substring filter** — should use `FuzzyMatcher` (which is available in tree).
- **MoveFileDialog collision handling** — silent abort on path collision; should Notice.
- **FileExplorer context menu** — 4 items vs ~13+; no `MenuSectionHelper`, no `fileMenu` emission, no plugin extensibility, no `system`-section actions, no submenus. F2 routes through `QInputDialog` not `RenameDialog`. Delete uses `QMessageBox::question` not `DeleteConfirmDialog`.
- **`Component` cleanup queue ordering** — three separate vectors (intervals/connections/cleanups) fire in fixed order vs Obsidian's single LIFO queue.

## Missing

- **Lucide icon registry / translation map.** No `Corbomite::Icons::*` API; no `IconMap.h`; no `Plugin.addIcon(name, svg)`. Persisted `lucide-file` icon strings will render blank.
- **MergeFile modal** (per addendum, deferred — but explicitly missing).
- **`SecretComponent` widget** — keychain backing exists, picker UI doesn't.
- **`SuggestModal<T>` abstract base** + `FuzzySuggestModal<T>` template.
- **`AbstractInputSuggest<T>` input-anchored suggester** (settings-form completers).
- **`Modal` base class** (Esc/Enter scope, dim-bg, builder facade).
- **`PopoverState` enum** (audit §2).
- **`addItem`-after-load Modal handling** (Obsidian silent-drop).
- **`MenuItem.setWarning` red-text styling** (section is canonical, per-item visual cue absent).
- **SliderComponent dynamic-tooltip** while dragging.
- **HoverLinkSourceRegistry consumption** by editor/search/backlinks/etc (currently write-only registry).
- **Per-OS label adaptation** for "Show in Finder/Explorer/system explorer".
- **Notice-on-`openWithDefaultApp` failure** (audit-noted UX upgrade).

## Notable translation successes (KDE upgrades)

- **`Platform::showInFolder` Linux DBus path** (`Platform.cpp:46-61`) is genuinely *better* than Electron's — direct DBus call to `org.freedesktop.FileManager1.ShowItems` succeeds on Dolphin/Nautilus/Nemo/PCManFM-Qt; the addendum's compat matrix matches. No Electron round-trip overhead.
- **`DeleteConfirmDialog`** + `KConfigGroup` for the `PromptDelete` config gate is a clean KDE-idiomatic translation. The "Don't ask again" wiring goes through the same KConfig store as the Settings page, so toggling either surface flips the other.
- **`KPageDialog`** for `SettingsDialog` is structurally superior to Obsidian's hand-rolled tabbed modal (keyboard nav, native window management, per-page icons all free).
- **`CompletionPopup` parented as a child widget rather than `Qt::Popup`** (`CompletionPopup.h:21-27`) avoids the focus-stealing problem completely. Modelled after KateCompletionWidget — well-trodden ground.
- **`Markoff::Reading::ReadingView` as the HoverPopover preview body** means the popover gets math/mermaid/syntax highlighting/`![[]]`/embeds *for free*. This is far better than Obsidian's preview, which strips most rich rendering.
- **`MenuSectionHelper`** is more declarative than Obsidian's `addSections([...]) + setSection(id) + sort()` dance — you just call `addToSection(action, "foo")` repeatedly and `finalize()` does the right thing.

## Notable concerns / suspected bugs

1. **HoverPopover is not Obsidian-quality without mod-key pinning + child-chains.** Every other gap is paint; these two are *the* hover-link UX. Without them, you cannot click into a popover to navigate, and chained navigation is broken. High-priority follow-up.
2. **`Notice(text, 0)` immediately closes** (`Notice.cpp:48-50`). Should be `if (durationMs > 0) m_dismissTimer.setInterval(durationMs)` and skip `start()` for `0`. One-line fix.
3. **`FileExplorerView` bypasses the proper rename/delete modals** (`FileExplorerView.cpp:138-164`). F2 → `QInputDialog` instead of `RenameDialog`; Delete → `QMessageBox::question` instead of `DeleteConfirmDialog`. Means the "Don't ask again", trash-option awareness, and live-validation are all silently disabled in the file-explorer surface.
4. **`FileExplorerView` context menu has no plugin emission and no section helper.** Plugins cannot inject items. Discoverable via the addendum's "Implementation notes" — known-gap.
5. **`MenuSectionHelper::canonicalSectionOrder()` missing `title`/`action-primary`/`order`/`ribbon`** — items set on these sections silently land in the wrong bucket.
6. **`HoverLinkSourceRegistry` is write-only.** Sources register but nobody reads `defaultMod` to decide whether to suppress the popover. The whole registry has no behavioural effect today.
7. **`MarkdownView` and `GraphViewTab` build menus with raw `addAction` calls** — the substrate exists (`MenuSectionHelper`, `MenuEventEmitter`) but the migration isn't complete. `MarkdownView::onMoreOptionsMenu` chains to `EditableFileView` (per the comment at `MarkdownView.cpp:389`), and that path uses the helper, but standalone right-clicks in graph/canvas/explorer don't.
8. **`Component` cleanup ordering divergence** — three separate vectors vs single LIFO queue. Subtle plugin-compat risk.
9. **`MoveFileDialog` substring match** when `FuzzyMatcher` is available in tree — feels like a TODO that wasn't followed up. Easy win.
10. **No icon registry at all** — when plugins start landing custom icons (Cluster Q follow-up) this needs to exist before any icon-bearing plugin renders correctly.
