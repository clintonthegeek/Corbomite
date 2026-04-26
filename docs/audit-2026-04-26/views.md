# Views domain audit

Audit date: 2026-04-26.
Spec: `/home/clinton/dev/Corbomite/docs/obsidian-audit/domains/views.md`.
Reference impl: `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/views/`.

## Architecture fit

The Corbomite view hierarchy is, modulo small naming/structural deltas, a
*direct* port of the Obsidian one. The five-class chain
`View → ItemView → FileView → EditableFileView → TextFileView` exists 1:1 with
matching file names under `libs/core/include/corbomite/core/` and
`libs/core/src/` (View.h:19, ItemView.h:16, FileView.h:11,
EditableFileView.h:13, TextFileView.h:13). The hierarchy is rooted in
`Component` (Obsidian's `Component`, ported to a Qt-friendly aggregator at
`libs/core/include/corbomite/core/Component.h`) and consumed by
`WorkspaceLeaf` (`libs/core/include/corbomite/core/WorkspaceLeaf.h:23`),
`Workspace` (`libs/core/include/corbomite/core/Workspace.h`), and
`ViewRegistry` (`libs/core/include/corbomite/core/ViewRegistry.h:16`). This is
a *much* better fit than the audit's mapping table predicted (the table at
`docs/obsidian-audit/domains/views.md:316` still has all five rows marked
"Missing"/"Partial" — the audit doc itself is stale relative to where the
codebase landed during Cluster G).

The translation strategy is: `containerEl.createDiv('workspace-leaf-content')`
becomes a `QWidget` parent with a tight `QVBoxLayout` (View.cpp:21–26). The
`HTMLElement` `data-type` attribute is reproduced via `QObjectName` only on
the inner header in `ItemView` (`view-header`), not on the outer container
(View.cpp:25 *does not* call `setObjectName(getViewType())`); see the
"Concerns" section. Action-bar/back-forward chrome is reproduced with
`QToolButton`s (`ItemView.cpp:38–69`). Inline rename in `EditableFileView` is
not reproduced as a `contentEditable`-equivalent inline edit; instead it
delegates to a host-injected modal callback (Concerns).

`ViewRegistry` is a complete port of the Obsidian one — same factories,
same extension-table, same atomic-validation invariant, same three signals
emitted as Qt signals. The host registers exactly four built-in factories
(MainWindow.cpp:1527–1551): `markdown`, `canvas`, `bases`, `empty`. A fifth
(`graph`) is added by the `corbomite-graph-view` plugin
(`src/plugins/graph-view/GraphViewPlugin.cpp:41`).

Sidebar plugin panels (Backlinks, Outline, Properties, Outlinks, LocalGraph,
Bookmarks, Search) are *not* `ItemView` subclasses — they are bare
`QWidget`s mounted into KDDockWidgets dock areas (e.g.
`src/plugins/backlinks/BacklinksView.h:23` `class BacklinksView : public
QWidget`). This diverges from Obsidian, where every sidebar pane is an
`ItemView` so it gets the action-bar, the "…" menu, and `setViewState`/
`getViewState` for free. See "ViewRegistry extensibility risk" below.

## Implemented (parity-equivalent)

- **Five-class hierarchy.** Public surface and inheritance match. `View`
  inherits from `QWidget`; `Component` is owned via `unique_ptr` and
  surfaced via `View::component()` rather than mixed-in (View.cpp:19) —
  avoids C++ MI; uses Qt parent-child for widget lifetime.
- **Lifecycle hooks.** `View::open` calls `m_component->load()` then
  `onOpen()` (View.cpp:50–58); `close()` calls `onClose()` then `unload()`
  (View.cpp:60–66). Skips Obsidian's `appendChild` because `WorkspaceLeaf`
  hands the view directly to KDDockWidgets via `m_dockWidget->setWidget`
  (WorkspaceLeaf.cpp:96). load-before-onOpen ordering is preserved.
- **Display contract.** `getViewType`/`getDisplayText`/`getIcon` are
  abstract on `View`, implemented on every concrete subclass (MarkdownView
  .cpp:37/42/49, CanvasFileView .cpp:40/41, EmptyView .cpp:54/56/58, GraphView,
  BasesView). Dock-widget title updated via `displayTextChanged` signal
  (WorkspaceLeaf.cpp:97–101) — Qt-idiomatic substitute for `leaf.updateHeader()`.
- **`addAction(icon, title, callback)`** at ItemView.cpp:79–88; mirrors
  Obsidian. *Append* order vs Obsidian's *prepend* — see Concerns.
- **More-options menu canonical section order.** `MenuSectionHelper` ports
  `Menu.addSections([...])`. ItemView.cpp:120–138 invokes the canonical
  pipeline: subclass `onMoreOptionsMenu(helper)` → back-compat
  `onPaneMenu(menu, "more-options")` → plugin `leaf-menu` emission via
  `MenuEventEmitter` → `helper.finalize()`.
- **`onTabMenu` default close items.** View.cpp:82–120 — Close / Close
  Others / Close All to the Right / Close All. Pinned-tab exclusion missing
  (see Concerns).
- **Back/forward navigation buttons.** ItemView.cpp:38–48 + 96–106 wires
  to `WorkspaceLeaf::goBack/goForward` (WorkspaceLeaf.cpp:277–325).
  `LeafHistory` is a real per-leaf back/forward stack driven by
  `WorkspaceLeaf::navigate()`. Enable/disable refreshed via `viewChanged`.
- **`FileView` file-binding lifecycle.** loadFile (FileView.cpp:19–37)
  reproduces `unload current → null → try onLoadFile → on throw, revert to
  null` exactly. `getState`/`setState` serialise `{file: relativePath}`
  (FileView.cpp:48–69). Resolution via `FileResolver` injected on
  ViewRegistry (ViewRegistry.h:22) — wired to `Vault` by the host.
- **TextFileView.save() pipeline.** TextFileView.cpp:33–81 reproduces the
  loop: `m_saving` re-entry guard with `m_saveAgain` flag (38–42, 77–80),
  `m_lastSavedData` short-circuit (45), `previousLastSaved` rollback (49),
  immediate-mode flush that nulls data + calls `clear()` (51–55),
  backup-on-failure (67, 136–150). `requestSave()` arms a 2000ms `QTimer`
  (.cpp:27–31). Debounce timing matches the invariant exactly.
- **Three-way merge on external modify.** TextFileView.cpp:112–134 +
  `DiffMatchPatch::threeWayMerge`. Gated on `m_saving` (echo-suppression)
  at line 115. `MainWindow.cpp:2071–2086` wires `Vault::modified` →
  walking every leaf → `tfv->onExternalModify` (broadcast vs Obsidian's
  per-view subscription; same effect).
- **ViewRegistry semantics.** ViewRegistry.cpp:18–88. Throws on duplicate
  `registerView` (20–23). `registerExtensions` validates the array before
  mutating (35–43) — atomic invariant. Same for
  `registerViewWithExtensions` (53–66). Signals `viewRegistered`,
  `viewUnregistered`, `extensionsUpdated`.
- **Empty-state pane (`tD`)** at `EmptyView` (.h:13) — three centred
  actions with host-injected handler. Registered MainWindow.cpp:1539–1551.
- **Unknown-view-type fallback.** WorkspaceLeaf.cpp:144–160 — falls back
  to `"empty"` factory when `getViewCreatorByType` returns null. Functionally
  there but visually weaker than Obsidian's `nD` (no "Close all of this
  type" affordance).
- **Deferred-load stub (`eD`).** `WorkspaceLeaf::isDeferred()` /
  `setDeferred()` / `loadIfDeferred()` (.h:65–71, .cpp:209–230). Workspace
  activates a deferred leaf via `loadIfDeferred()` (Workspace.cpp:838–839).
- **Plugin view registration with auto-unregister** via `ViewRegistrar`
  (libs/core/src/proxies/ViewRegistrar.cpp) — RAII per-plugin scope-guard.
  Used by `GraphViewPlugin::onLoad` (.cpp:40–58).

## Partial / divergent

- **`ItemView.addAction` orders left-to-right (append) vs Obsidian's
  right-to-left (prepend).** Obsidian ItemView.js:299–312 does
  `actionsEl.prepend(button)`, so the most-recently-added action sits
  closest to the title. Corbomite ItemView.cpp:87 uses
  `m_actionsLayout->addWidget(btn)` (append). Plugins that call
  `addAction("a"); addAction("b");` get *opposite* visual order vs Obsidian
  — a portability hazard for plugins crossing the two ecosystems.
- **`onMoreOptionsMenu(MenuSectionHelper&)` instead of
  `onMoreOptionsMenu(menu)`.** Subclass-override surface is `helper.addToSection(action, "section-id")` rather than
  `menu.addItem(item => item.setSection("section-id"))`. The semantics
  (canonical section order, deferred separator insertion) match. The API
  shape diverges; plugin-portability cost is non-trivial. View.cpp:75 has
  the no-op base implementation.
- **`onPaneMenu(menu, source)` exists but the source argument is
  underused.** View.h:55 declares it; the default implementation
  (View.cpp:77–80) just forwards to the zero-arg overload. Obsidian's
  `ItemView.onPaneMenu` discriminates on `source` (the `Platform.isPhone &&
  "more-options" === n` branch in View.js:356) to decide whether to inject
  Close+Pin items. Corbomite ignores it — defensible (mobile-only branch),
  but plugin code that ports verbatim will get nothing for the source arg.
- **`getEphemeralState` / `setEphemeralState`.** Exists on `View` (.h:41–42)
  with no-op defaults. `MarkdownView::setEphemeralState` is a stub
  (MarkdownView.cpp:172–175 — body is `Q_UNUSED(state)`). Obsidian uses
  ephemeral state for **scroll position, search match, rename mode, focus,
  subpath**. Corbomite has none of these wired through. Critically:
  - `EditableFileView::setEphemeralState({rename: 'start'|'end'})` to enter
    inline rename mode is **missing** — there is no override of
    `setEphemeralState` on EditableFileView at all. Rename is delegated to a
    host-supplied modal callback (EditableFileView.cpp:225–232).
  - `openLinkText`-style "scroll to heading on open" relies on
    `eState.subpath`/`scroll`/`line` — not wired through. (The audit at
    line 330 of views.md flagged this gap.)
  - `WorkspaceLeaf::navigate` does store `eState` in history entries
    (WorkspaceLeaf.cpp:261, 287, 312) and replay them on goBack/goForward
    (lines 297–298, 322–323), so the *plumbing* is present — the lack is in
    concrete views consuming it.
- **Inline-rename via `contentEditable`.** Obsidian's titleEl is a
  `contentEditable` div with focus/blur/input/paste/keydown listeners
  (EditableFileView.js:25–49 + 51–219). Corbomite's EditableFileView declares
  `m_titleEdit: QLineEdit*` (.h:65) but never actually creates one — the
  `startRename()` method (.cpp:225–232) just calls
  `m_renameCallback(m_file, this)` which delegates to the host's
  modal-rename plugin. Inline rename is not implemented; the field is
  vestigial. The audit at views.md:322 ("EditableFileView inline-rename:
  Missing") is *correct*. Live filename-validation, Escape-to-revert,
  Enter-to-save, ArrowDown-into-frontmatter — none of these are present.
- **`renderBreadcrumbs()`.** Obsidian renders the parent-folder path as
  clickable / right-clickable / draggable spans (FileView.js:12–135).
  Corbomite's `FileView` has *no* breadcrumb method or member at all
  (FileView.h:11–37). The audit at views.md:321 acknowledges this.
- **Tab context menu pinned-tab exclusion.** View.cpp:82–120 implements
  Close / Close Others / Close All to the Right / Close All but does **not**
  filter pinned tabs the way Obsidian does (View.js:104–148 wraps every
  iteration in `.filter(e => !e.pinned)`). The pinned-state plumbing exists
  (`WorkspaceLeaf::pinned()` at .h:57, signal at .h:113) but isn't honoured
  in the close actions. This is a UX regression that will silently close
  pinned tabs.
- **`syncState()` / `receiveSyncState()` (stacked-group sync).**
  `FileView` does not implement these. `WorkspaceLeaf::group()` /
  `setGroup()` exist (.h:61–62) but `FileView::loadFile` (FileView.cpp:19–37)
  doesn't call any peer-sync method. `setState` does not flag a
  `done = () => syncState()` callback. Stacked-group "linked tabs" feature
  is non-functional even though the data plumbing is there.
- **`FileView` does not subscribe to vault rename/delete signals.**
  Obsidian's `FileView.onload` (FileView.js:139–144) subscribes the view to
  `vault.on('rename')` and `vault.on('delete')`. Corbomite's `FileView` has
  no `onload` override and no per-view subscription. Instead,
  `MainWindow.cpp:2088–2101` is a *centralised* handler that forwards to
  `MetadataCache::onFileDeleted/onFileChanged`. This is *functionally* fine
  for metadata but the per-view UX behaviours are missing:
  - On rename of `this->file`, view is supposed to re-render breadcrumbs +
    update title + call `leaf.updateHeader()`. Corbomite does no per-view
    re-render — `displayTextChanged` is only emitted from `loadFile`
    (FileView.cpp:35), not from any external rename event. **Bug:** if a
    file is renamed externally while open, the title bar will show the stale
    name until the leaf is reopened.
  - On delete of `this->file`, view should step back in history, swap to
    empty state, or detach the leaf. Corbomite has no equivalent.
- **`canAcceptExtension` view reuse on open.** MarkdownView.cpp:75–78
  returns `true` for `.md`; CanvasFileView.cpp:43–46 returns true for
  `.canvas`. The infrastructure exists but **`WorkspaceLeaf` does not have
  an `openFile(file)` overload** that consults this. The only entry path is
  `setViewState`, which always destroys the current view (line 162) and
  builds a fresh one. So the optimisation is wired but never exercised —
  every file open is a close-and-reopen.
- **`saveImmediately()` semantics.** Obsidian only calls `save(false)` (the
  *non*-immediate path) when `dirty` (TextFileView.js:53–54). Corbomite
  inverts this: `TextFileView::saveImmediately` calls `save(true)`
  (TextFileView.cpp:83–86), which uses the **immediate-mode flush path** —
  which nulls `m_data`/`m_lastSavedData` and calls `clear()` (lines 51–55).
  Calling `saveImmediately()` while the view is *still in use* (e.g. a
  "save now" command) would tear the editor down. In practice this is
  invoked from `onUnloadFile` (line 107) where the tear-down is intended,
  but the public API name is misleading.
- **Built-in view set.** Of Obsidian's 6+2 built-ins (md, canvas, pdf,
  image, audio, video, base, release-notes) Corbomite ships 4 (md, canvas,
  base, empty) plus 1 plugin-supplied (graph). Pdf/image/audio/video are
  **missing**. The audit at views.md:325 confirms this. Note: `bases` is
  present, which is *not* in the original built-in seed list — Bases has
  graduated from internal-plugin status to a built-in view here.

## Missing

- **`renderBreadcrumbs()`** on `FileView`. No method, no `titleParentEl`,
  no folder-path span rendering, no folder context-menu hookup, no
  drag-folder source. (Audit cite views.md:53.)
- **Inline `contentEditable` title-bar rename** on `EditableFileView`. The
  `m_titleEdit` member is declared but unused; `startRename` delegates to a
  host modal. (Audit cite views.md:67–74.)
- **`setEphemeralState({rename: 'start'|'end'})`** override on
  EditableFileView. (Audit cite views.md:74.)
- **`setEphemeralState({focus, focusMetadata, scroll, line, match,
  subpath})`** consumers on MarkdownView. (Audit cite views.md:151–164.)
- **Ephemeral-state plumbing through `Workspace::openLinkText`.** No
  equivalent of `getLeaf().openFile(file, {eState: {scroll: 12}})`. The
  `WorkspaceController` proxy (libs/core/include/corbomite/core/proxies/
  WorkspaceController.h) is the natural home; it has a `goToLine` method
  but not a generalised eState pass-through.
- **`onHeaderMenu` hook.** The audit notes Obsidian declares
  `View.onHeaderMenu(menu)` as a no-op default with unknown firing site
  (views.md open-question 7). Corbomite has no equivalent declaration.
  Probably safe to defer.
- **`canDropAnywhere` flag + `handleDrop` delegation.** Obsidian's
  `ItemView` accepts header-only drops by default but allows
  `canDropAnywhere = true` for views that want to accept drops anywhere
  (ItemView.js:283–297). Corbomite has nothing equivalent — drag/drop is
  handled per-widget by KDDockWidgets/Qt for tab moves, but per-view content
  drops (e.g. drag-a-file-into-an-editor-to-insert-link) need bespoke wiring.
- **`getSideTooltipPlacement()` for sidedock chrome.** No equivalent.
  Probably acceptable — KDDockWidgets handles tooltip placement on docked
  widgets natively.
- **`handleCut`/`handleCopy`/`handlePaste` hooks.** Not declared on
  Corbomite `View`. Concrete editors (Markoff, SourceEditor) handle their
  own clipboard, but the hook for plugins to override clipboard at the
  view level is gone.
- **Pinned-tab exclusion in default tab-menu close items.** See Partial.
- **`syncState`/`receiveSyncState` for stacked-group linked tabs.** See
  Partial.
- **Per-view `vault.on('rename')` / `on('delete')` subscriptions.** See
  Partial; centralised handling covers metadata but not per-view UX.
- **`SetStateResult` out-param.** Obsidian's `setState(state, result)`
  signature lets the view request `result.history = true`,
  `result.layout = true`, or `result.close = true`. Corbomite's
  `setState(const QJsonObject &)` (View.h:40, FileView.cpp:56) drops the
  result completely. Consequence: a `FileView` that fails to find its file
  cannot signal "close me" to the leaf — Corbomite's FileView.cpp:60
  silently early-returns on missing file path, leaving an empty pane in
  place. Not a parity hazard yet (no caller relies on it), but a missing
  surface area.
- **Distinct unknown-view-type pane (`nD`).** Functionally folded into
  EmptyView — works, but the user sees the new-tab pane instead of an
  "Unknown view type — close all of this type" affordance. Acceptable for
  alpha; track for parity.
- **Built-in `image`/`audio`/`video`/`pdf` view types.** Missing. PDF in
  particular is non-trivial (PDF rendering library); image is trivial
  (`QLabel + QPixmap` or KImageViewer); audio/video need QtMultimedia.
- **`release-notes` view type.** Out of scope unless we ship release
  notes — fine.

## Notable translation successes

- **`MenuSectionHelper`** (libs/core/include/corbomite/core/
  MenuSectionHelper.h). Cleanly maps Obsidian's `Menu.addSections([...])` +
  `setSection("name")` semantics onto a `QMenu` (which has no native
  section concept). The deferred-finalize pattern with submenu support
  (`addSubmenu(sectionId, title, icon)` returning a sub-helper, see
  EditableFileView.cpp:143–146 for the "Copy path" submenu usage) is more
  ergonomic than Obsidian's mid-construction calls. ItemView.cpp:120–138
  shows the canonical pipeline.
- **`MenuEventEmitter`** (libs/core/include/corbomite/core/
  MenuEventEmitter.h). Clean Qt-signal port of `workspace.trigger
  ('leaf-menu', menu, leaf)` and `workspace.trigger('file-menu', menu, file,
  source, leaf)` — an emitter object pluggable per-leaf via
  `WorkspaceLeaf::setMenuEventEmitter` (.h:96). Lets tests inject a no-op
  emitter and lets the host wire a real one.
- **`ViewRegistrar`** (libs/core/src/proxies/ViewRegistrar.cpp). Per-plugin
  scope-guarded registration that auto-unregisters on plugin unload. Mirrors
  Obsidian's `Plugin.registerView` (which auto-unregisters via the
  `Component`-deferred-cleanup pattern), implemented as RAII rather than
  parent-component-deferred. Used by `GraphViewPlugin::onLoad`
  (.cpp:40–58).
- **`FileResolver` injected on the registry**. Decouples
  `FileView::setState`'s path→document resolution from any specific
  vault/storage type (ViewRegistry.h:22). Makes `FileView` testable
  without dragging in `libs/vault`. The dependency-direction discipline
  established in Cluster Q.0 is intact here.
- **`displayTextChanged` signal** as the Qt-idiomatic substitute for
  Obsidian's manual `leaf.updateHeader()` calls. View emits, dock widget
  reacts (WorkspaceLeaf.cpp:97–101). Fewer manual update sites; harder to
  forget.
- **`m_neverLoaded` sentinel** (TextFileView.h:49, .cpp:36). Cleaner
  expression of Obsidian's `lastSavedData === null` "never loaded yet"
  invariant — explicit boolean rather than overloading an empty string for
  two meanings. Matches the audit invariant at views.md:272.
- **`Workspace`-level rebroadcast of vault signals to every TextFileView**
  (MainWindow.cpp:2071–2086). Obsidian's per-view `registerEvent` is
  per-instance subscriptions; Corbomite's central forward is simpler and
  works because there's a single `Vault` instance. Cleaner design when only
  one vault is open at a time.

## Notable concerns / suspected bugs

- **`writeBackup` writes a plain markdown file inside the vault**
  (TextFileView.cpp:140 → `<vaultRoot>/.obsidian/file-recovery/<name>-<ts>.md`).
  The file will show up in the file tree, the tag index, search results,
  graph view, and **trigger another `Vault::modified` signal for the leaf
  that just failed to save**. Recommended: write outside the vault. Obsidian
  stores backups in the file-recovery plugin's `data.json` — not a markdown
  file in the user-visible tree.
- **Title not refreshed on external rename of open file.** No code path
  re-emits `displayTextChanged` from `Vault::renamed`. Tab caption stays
  stale until the leaf is reloaded.
- **Open file deleted externally — leaf becomes orphaned.** No code path
  reacts to `Vault::deletedFile` for an open file. Subsequent saves can
  silently re-create the file because `Vault::modify` writes by relative
  path.
- **`FileView::setState` swallows missing-file silently** (FileView.cpp:60,
  67). No equivalent of Obsidian's `n.close = true`. Probable bug: deleting
  an open file outside Corbomite, then restoring the session, leaves a
  zombie tab.
- **`m_navigation` and `m_allowNoFile` declared but unused** (FileView.h:34–35).
  Without honouring `allowNoFile`, `setState` cannot decide close-vs-empty
  for plugin dashboard views.
- **`ItemView::onOpen` connects `m_leaf->viewChanged` after the signal that
  brought the view into existence has already fired.** ItemView.cpp:102–104
  vs WorkspaceLeaf.cpp:103/168. Initial enable-state of nav buttons may be
  stale until the next nav. Minor.
- **`ItemView::buildHeader()` orphans the inherited `containerWidget()`.**
  ItemView.cpp:30–73 removes it from the outer layout and adds new
  `m_headerWidget` + `m_contentWidget`. `View::containerWidget()` then
  returns a pointer to a widget no longer in any layout. Subclasses must
  use `contentWidget()` (MarkdownView.cpp:25 does), but the View base API
  is silently shadowed. Confusing — document or consolidate.
- **`addAction` appends rather than prepends.** ItemView.cpp:87 vs
  Obsidian ItemView.js:299–312. Plugins using both ecosystems get reversed
  visual action-bar order.
- **`m_titleEdit` declared but never instantiated** (EditableFileView.h:65).
  Dead member.
- **`MarkdownView::onLoadFile` strict ordering invariant** (MarkdownView.cpp:423–427).
  `setNoteDocument(file)` must run before the parent-class
  `TextFileView::onLoadFile` reads the file and calls `setViewData`. The
  *parent* call comes second — opposite of normal override convention.
  Worth a code comment.
- **`MarkdownView::getEphemeralState`/`setEphemeralState` are stubs**
  (MarkdownView.cpp:166–175). Blocks `openLinkText` scroll-to-heading and
  rename-on-create flows.
- **`TextFileView::save` early-return at line 45 doesn't double-check
  `m_neverLoaded`.** Currently safe only because the guard at line 36
  catches it first. Guard reordering would let an unloaded view write `""`
  over the file. Defensive-coding point.

## ViewRegistry extensibility risk

The `ViewRegistry` itself is well-designed and matches the audit. The risks
sit one layer up:

1. **Sidebar plugin panels are not `ItemView` subclasses.** Backlinks,
   Outline, Properties, Outlinks, Search, LocalGraph, Bookmarks all mount
   bare `QWidget`s into KDDockWidgets dock areas. They do *not* go through
   ViewRegistry, so they get no `addAction` chrome, no "…" menu, no
   `setViewState`/`getViewState`, no `leaf-menu` / `file-menu` events, no
   command-dispatch hook. From an Obsidian-plugin-portability standpoint
   this is a structural divergence — Obsidian plugins create sidebar panels
   by `extends ItemView` and `workspace.getRightLeaf(false).setViewState
   ({type})`, with the same lifecycle as a main-area view. To accept
   off-the-shelf Obsidian plugin sidebar panels, Corbomite needs (a) a
   sidebar `ViewRegistry` path that wraps a registered viewType into a
   docked `WorkspaceLeaf`, and (b) a host wiring that bridges the legacy
   QWidget-based panels to `ItemView`-based ones over time. Currently the
   bare-QWidget pattern is propagating (every new plugin under
   `src/plugins/` follows it).
2. **`registerExtensions` is per-extension only** — no MIME, no `(globPattern,
   type)` table, no priority order. Obsidian has the same limitation, so
   parity is fine; just flagging for future PDF/image extension growth.
3. **No "default view type" override.** Obsidian's flow falls back from
   `getTypeByExtension` to `openWithDefaultApp`. Corbomite's
   `WorkspaceLeaf::setViewState` falls back to `"empty"`
   (.cpp:155); there is no platform-handoff fallback. Files of unknown
   extension are clickable from the file tree but produce empty tabs rather
   than launching the system viewer.
4. **`canAcceptExtension` reuse path is dead.** `MarkdownView::canAcceptExtension`
   (MarkdownView.cpp:75) and `CanvasFileView::canAcceptExtension` (.cpp:43)
   both return `true` for their extension, but no caller invokes them. The
   reuse-on-open optimisation is wired but inert. Low risk — but a future
   plugin author who reads the audit and overrides `canAcceptExtension`
   expecting reuse will be surprised.
5. **Bases is hard-wired in MainWindow rather than registered by the
   bases internal-plugin.** MainWindow.cpp:1533–1535 registers
   `bases` directly; in Obsidian this is the bases internal plugin's job
   (audit views.md:255–257). Means: disabling the bases plugin in
   Corbomite cannot remove the `.base` extension binding; a future plugin
   wanting to override the `.base` viewer would hit
   `ViewRegistry::registerExtensions`'s duplicate-throw. Re-locate to the
   bases plugin's `onLoad` for proper plugin-style lifecycle.
6. **Plugin-loaded view types racing session-restore.** MainWindow.cpp:2130–2151
   intentionally loads enabled plugins *before* `m_workspace->deserialize`
   so that, e.g., the `graph` viewType is registered when a restored
   `graph` leaf rehydrates. This works for built-in/core plugins, but if a
   community plugin loads asynchronously (lazy init, network metadata
   fetch, …) any leaf referencing its viewType during restore will fall
   back to `EmptyView`, and there is no `view-registered` listener that
   re-tries deferred restoration the way Obsidian's Workspace does
   (audit views.md:198–199). Risk grows with plugin count.
7. **No `nD`-style "rebuild on viewType registration"** — the
   `ViewRegistry::viewRegistered` signal is emitted (.cpp:24) but no
   subscriber in `Workspace` reacts to it. Obsidian's `Workspace.js:347`
   does. Means: enable-plugin-mid-session won't recover orphaned
   "fell-back-to-empty" leaves; user has to close/reopen them.
