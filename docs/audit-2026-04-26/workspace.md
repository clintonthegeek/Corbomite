# Workspace + leaf-utilities domain audit

Scope: `obsidian/workspace/*` (12 source files; 4 canonical extractions per the de‑min note in `domains/workspace.md:23‑29`) plus the `obsidian/utils + platform + secrets + network` "leaf utilities" bundle (`domains/leaf-utilities.md`). Corbomite source under audit: `libs/core/{Workspace,WorkspaceLeaf,WorkspaceContainer,WorkspaceRoot,WorkspaceFloating,WorkspaceWindow,WorkspaceSidedock,WorkspaceSerializer,WorkspaceActiveLeafRouter,LeafHistory,HoverLinkSourceRegistry,MenuEventEmitter,View}.{h,cpp}`, `libs/core/include/corbomite/core/proxies/{WorkspaceController,MenuInjector,SecretStorage}.h`, `src/mdi/CorbomiteMDI.{h,cpp}`, `src/app/{MainWindow,SessionManager,RibbonToolBar,RibbonStateController,main,CorbomiteApp}.{h,cpp}`.

---

## Architecture fit (KDDockWidgets vs Obsidian's hand‑rolled tree)

Obsidian builds a hand‑rolled `Events`‑derived class tree: `Workspace` owns `rootSplit`/`leftSplit`/`rightSplit`/`floatingSplit`/`leftRibbon`/`rightRibbon`, all subtypes of `WorkspaceParent`/`WorkspaceItem`, with `containerEl`+`tabHeaderEl` HTMLElements per node and a `dimension` (flex‑grow 0–100) per child. Every layout mutation (insert/remove/resize) is owned by these classes; `WorkspaceParent.removeChild` even *dissolves* a parent when it drops to a single child, with the remaining child promoted up and inheriting `dimension` (`workspace.md:75, 393`). That topology is the on‑disk schema (`SplitNode` discriminated union — `workspace.md:172‑195`).

Corbomite delegates the entire substrate to **KDDockWidgets**. `Workspace::Workspace(...)` (`libs/core/src/Workspace.cpp:69‑111`) instantiates a single `KDDockWidgets::QtWidgets::MainWindow` named `corbomite:<vaultId>` with `MainWindowOption_None`, with `Flag_AlwaysShowTabs | Flag_AllowReorderTabs | Flag_TabsHaveCloseButton` set globally (`libs/core/src/Workspace.cpp:38‑49`). `Workspace::createLeafInGroupOf`/`splitLeaf`/`popoutLeaf` defer to `addDockWidgetAsTab`/`addDockWidgetToContainingWindow`/`setFloating(true)` respectively. Workspace owns no `Split`/`Tabs` *node* objects in main‑area code paths; only the leaf list (`m_leaves`) and a parallel `QHash<WorkspaceLeaf*, QString> m_tabGroupOf` (`libs/core/src/Workspace.cpp:271‑273`) exist. The Obsidian‑shape `WorkspaceContainer`/`WorkspaceRoot`/`WorkspaceFloating`/`WorkspaceSidedock`/`WorkspaceWindow` types (`libs/core/include/corbomite/core/Workspace*.h`) are explicitly documented as Phase 7.5 *bookkeeping shells* with no behavioural content (`WorkspaceContainer.h:11‑16`, `WorkspaceRoot.h:11‑13`, `WorkspaceWindow.h:10‑15`, `WorkspaceSidedock.h:11‑15`).

This is the right architectural call — KDDW owns drag-tab, drag-to-edge-to-split, floating windows, restore-from-binary-blob, and tab reorder for free, all of which are non-trivial in Qt. **But** the Obsidian contract is not just "drag a tab" — it is `workspace.json`'s exact schema (`split` with `direction`/`dimension`/`children`; `tabs` with `currentTab`/`stacked`; `leaf` with `id`/`state`/`pinned`/`group`; `window` with `x`/`y`/`width`/`height`/`maximize`/`zoom`; `floating` wrapper) and the per‑node `WorkspaceItem` API plugins walk. The substrate does not natively produce that JSON, and this gap is reflected in Corbomite's `serialize()`/`deserialize()` flow (see Layout JSON section below).

Critically, KDDW has **no public Group/Frame enumeration API** — Corbomite acknowledges this in `Workspace.h:267‑271` ("KDDW exposes no public Group enumeration API, so Workspace tracks tab grouping itself") — so even after a user drags a tab between groups, `m_tabGroupOf` lags until `WorkspaceActiveLeafRouter` re‑syncs. Until that resync runs, `serialize()` will emit a stale tab grouping.

---

## Implemented (parity-equivalent)

- **`Workspace` aggregate object as a sibling of vault.** `Corbomite::Workspace` (`libs/core/include/corbomite/core/Workspace.h:39`) is the right shape — vault‑scoped, owns the leaf list, exposes activeLeaf + signals. The cluster Y/G refactor created exactly the singular workspace object the audit calls for at `workspace.md:455‑478`.
- **`WorkspaceLeaf` API surface (partial).** `WorkspaceLeaf::id/setId/getViewState/setViewState/setPinned/setGroup/setDeferred/loadIfDeferred/getEphemeralState/setEphemeralState` (`libs/core/include/corbomite/core/WorkspaceLeaf.h:31‑88`) maps to most of Obsidian's leaf surface. `pinnedChanged`/`groupChanged` signals match Obsidian's `pinned-change`/`group-change` events 1:1 (`workspace.md:300‑303`).
- **Per-leaf history (LeafHistory).** `LeafHistory` (`libs/core/include/corbomite/core/LeafHistory.h:22‑40`, cap 20) has the same back/forward/cap‑20 semantics as Obsidian's `qD` (`workspace.md:131`). Push‑on‑navigate is wired in `WorkspaceLeaf::navigate` (`libs/core/src/WorkspaceLeaf.cpp:253‑275`).
- **Leaf‑close undo stack.** `Workspace::m_undoHistory` (cap 10, prepend‑then‑pop in `closeLeaf` at `libs/core/src/Workspace.cpp:298‑325`) matches Obsidian's `undoHistory` (`workspace.md:336, 395`). `undoCloseLeaf()` recreates a leaf and restores `viewState`+`pinned`+`group`. Bound to user‑visible action in `MainWindow.cpp:1334`.
- **Obsidian‑shape `getLeaf(mode, dir)` factory.** `Workspace::getLeaf(LeafMode::{Same,Tab,Split,Window}, LeafDirection::{Horizontal,Vertical})` (`Workspace.cpp:458‑483`) mirrors `Workspace.getLeaf(false|'tab'|'split'|'window'|true)` (`workspace.md:53`). The `LeafMode` enum is even documented on the header against the Obsidian `PaneType` shape.
- **`openLinkText`.** `Workspace::openLinkText(linktext, source, mode, opts)` (`Workspace.cpp:401‑456`) parses `path#heading` / `path^block` correctly using the *first* of either delimiter, runs the optional `LinkResolverFn` (the host installs it in MainWindow with the MetadataCache backing), and routes `subpath` into ephemeral state (matching Obsidian's `eState.subpath` mechanism in `workspace.md:121`).
- **Layout-ready gate / `layoutReady` event.** `Workspace::setLayoutReady(bool)` and the `layoutReady` signal (`Workspace.h:88‑91, 230`) implement the same suppression-while-loading invariant Obsidian uses (`workspace.md:387`). `setActiveLeaf` even early-returns when `!m_layoutReady` (`Workspace.cpp:179`) — this is the right guard.
- **`active-leaf-change` event.** `activeLeafChanged(WorkspaceLeaf*)` signal emitted from `setActiveLeaf` with identity gate (`Workspace.cpp:171‑189`). The "no re-fire on identical leaf" invariant from `workspace.md:388` is enforced.
- **Pin propagation through linked groups.** `Workspace::propagatePinToGroup` (`Workspace.cpp:628‑637`) wires every other group member's pin state to match — matches Obsidian's "pin on one member pins every member" invariant from `workspace.md:391`.
- **Tab navigation primitives.** `nextLeafInActiveGroup`/`previousLeafInActiveGroup`/`leafIndexInGroup`/`leafCountInGroup`/`closeOtherLeavesInGroupOf`/`closeLeavesToRightOf` (`Workspace.cpp:547‑604`) cover the `workspace:next-tab`/`workspace:close-others` command set (`workspace.md:345`).
- **Leaf id format.** `WorkspaceLeaf::generateId()` returns 16 lowercase hex chars (`WorkspaceLeaf.cpp:65‑74`) — matches Obsidian's `cc(16)` exactly (`workspace.md:385`).
- **Hover-link source registry.** `HoverLinkSourceRegistry` (`libs/core/{include,src}/.../HoverLinkSourceRegistry.{h,cpp}`) has `register/unregister/lookup/registerBuiltins`. The built-ins seed `editor`/`search`/`backlinks`/`outlinks`/`graph`/`bases` (`HoverLinkSourceRegistry.cpp:39‑48`).
- **Plugin-facing menu signals.** `MenuEventEmitter` (`libs/core/include/corbomite/core/MenuEventEmitter.h:21‑45`) has signals + emit helpers for `fileMenu`/`urlMenu`/`editorMenu`/`filesMenu`/`leafMenu`/`tabGroupMenu`/`markdownViewportMenu` — the full Obsidian menu-event set from `workspace.md:283‑292`.
- **MainWindow as host for the workspace tree.** Single‑MainWindow ownership (`MainWindow.cpp:1559‑1606`) reparents the KDDW MainWindow into a `m_workspaceContainer` QStackedWidget child. The tab‑select / tab‑close re-emission pattern (`Workspace.cpp:241‑263`) is sound — Workspace stays decoupled from the host.
- **Default‑layout enforcement.** `MainWindow.cpp:1641‑1649` re‑creates an empty leaf any time `m_workspace->allLeaves().isEmpty()` after a `layoutChanged` — matches Obsidian's "auto‑create empty tab in empty rootSplit" rule from `workspace.md:62` (it's in `updateLayout`).
- **Resize event.** `Workspace::resize` signal emitted from `eventFilter` on the KDDW MainWindow's `QEvent::Resize` (`Workspace.cpp:113‑118`) parallels Obsidian's `resize` event (`workspace.md:281`). Note: Obsidian's is debounced; Corbomite's is not (see Concerns).
- **`window-frame-change`.** `Workspace::windowFrameChange` signal emitted on popout create/destroy (`Workspace.cpp:485‑524`). Matches `workspace.md:282`.
- **Plugin proxy facade.** `WorkspaceController` (`libs/core/include/corbomite/core/proxies/WorkspaceController.h:29`) exposes `openFile`/`activeLeafId`/`activeFilePath`/`splitLeaf`/`closeLeaf`/`popoutLeaf`/`getLeavesOfType`/`iterateAllLeaves`/`getActiveViewOfType`/`openLinkText`/`getLeaf` to plugins. Active‑file change is re-emitted as `activeFileChanged(QString)`. This is exactly the "plugins never see raw `WorkspaceLeaf*`" pattern the brainstorm settled on for Cluster Q.
- **Sidebar substrate (Kate-derived).** `CorbomiteMDI::Sidebar`/`MultiTabBar`/`ToolView` (`src/mdi/CorbomiteMDI.cpp:255‑1500+`) is a near‑verbatim port of Kate's `KMultiTabBar`-driven sidedock. Functionally implements the *user-visible* parts of `WorkspaceSidedock`+`WorkspaceRibbon`: collapse/expand, tab reorder, drag tool to other side, persisted width via session.

### `leaf-utilities.md` items

- **`Platform.is{Linux,Win,MacOS}` equivalents.** Corbomite uses `QSysInfo` directly (no shim file matching `Platform`); plugin‑ABI gap is acknowledged in `leaf-utilities.md:530` but not yet plugged.
- **`SecretStorage`.** `libs/core/include/corbomite/core/proxies/SecretStorage.h` exists (Cluster N follow-up) — backed by `QtKeychain` per the project memory note.
- **Byte conversion.** Native `QByteArray::toBase64/fromBase64/toHex/fromHex` covers all four `arrayBuffer*` helpers.

---

## Partial / divergent

- **Tree topology (Root/Split/Tabs/Leaf hierarchy).** `Workspace::serialize()` (`Workspace.cpp:649‑713`) flattens *every* tab group into a single `split` containing N `tabs` children, each of which is a flat list of leaves. There is **no nested split** representation — KDDW's true split tree is collapsed. `WorkspaceSerializer::walkKddwTreeSimple` (`WorkspaceSerializer.cpp:200‑240`) walks `DockRegistry::dockwidgets()` and emits exactly *one* tabs node containing every non-floating dock. Same flattening problem, different writer. Both are documented as "Phase 5/6 introduces a proper tree walker that round-trips KDDW's nested split structure" (`Workspace.cpp:651‑658`) but that has not landed. **Round-trip lossiness:** any Obsidian vault opened in Corbomite that has nested splits will save back as a flat layout, breaking interop on next open in Obsidian.
- **`WorkspaceItem.dimension` (flex‑grow 0–100).** Not represented anywhere. `Corbomite::Workspace::serialize` never emits `dimension` keys; KDDW has its own pixel sizing. Per‑sibling resize ratios from a loaded `workspace.json` are silently dropped.
- **`WorkspaceTabs.stacked`.** The serializer parses `stacked` keys and persists them in a process‑wide `stackedSidecar` map (`WorkspaceSerializer.cpp:28‑32, 112‑114, 233‑236`) but the live KDDW substrate does not enter Obsidian's "all tabs side-by-side" rendering when `stacked: true`. Round‑trip preserves the bit; behaviour does not.
- **`WorkspaceTabs.currentTab`.** Workspace's own `serialize()` writes `currentTab: <index of activeLeaf>` — but this conflates the *globally active leaf* with the *per‑group selected tab*. Obsidian permits one selected tab per group regardless of which group is active. With one active leaf, all non-active groups will round‑trip with `currentTab: 0`, losing per-group tab selection.
- **`Workspace::getLeaf(LeafMode::Window)`.** Implementation calls `createLeafInActiveGroup` then `popoutLeaf` (`Workspace.cpp:474‑480`). Obsidian's `openPopoutLeaf` creates the leaf *inside* the new window — Corbomite creates it in the active group then *moves* it into the float, which transiently emits `tabSelectRequested` against the host group before the float happens. Likely fine but worth noting.
- **`WorkspaceLeaf.detach()`.** No explicit `detach()` method. `Workspace::closeLeaf` does the equivalent (push undo + delete), but a plugin calling `leaf.detach()` (the public Obsidian method) has no Corbomite analogue at the leaf object — only via the workspace facade.
- **`WorkspaceLeaf.containerEl/tabHeaderEl`.** Not exposed. `widget()` returns the KDDW `DockWidget*`, but the per-tab header element (which Obsidian plugins decorate) is owned by KDDW's TabBar internals and not surfaced. Plugins that want to badge the tab header have nowhere to attach.
- **`setViewState` single-flight (`working` flag).** Not implemented. Re-entrant `setViewState` calls during view construction will execute concurrently. Obsidian explicitly guards this (`workspace.md:122, 390`).
- **`WorkspaceLeaf.setGroupMember(other)`.** No equivalent — only `setGroup(id)` and `groupMembers(groupId)`. Auto-creating a group id when joining a non-grouped peer is not implemented.
- **Deferred leaf placeholder semantics.** `WorkspaceLeaf::setDeferred(bool, icon, title)` exists (`WorkspaceLeaf.cpp:211‑221`) and `Workspace::deserialize` defers non-active, non-current-tab leaves (`Workspace.cpp:814‑839`). But the deferred leaf shows the underlying KDDW dock widget *empty* — there is no "deferred placeholder view" with the cached icon+title rendered. Obsidian's `eD` placeholder (audit `workspace.md:506`) renders the cached icon+title until activated. `cachedIcon()`/`cachedTitle()` accessors exist but no `View` consumes them.
- **`onLayoutReady(cb)` semantics.** No queue+drain accessor on `Workspace` — only the `layoutReady` Qt signal. Plugins that subscribe after the signal already fired get nothing; Obsidian's `onLayoutReady` fires synchronously if already ready (`workspace.md:396`).
- **Sidedock = `WorkspaceSidedock`.** `Workspace::leftSplit()`/`rightSplit()` return *literal `nullptr`* (`Workspace.h:220‑221`). The actual sidebars live in `CorbomiteMDI::MainWindow` outside the Workspace tree. Plugin code that walks `workspace.leftSplit().children` will crash; plugin code that calls `workspace.leftRibbon.addRibbonItemButton` has no host (Corbomite uses `RibbonToolBar` separately). The Phase 7.5 stubs at least provide a header for compile-time existence, but they're not callable.
- **Ribbon.** `RibbonToolBar` (`src/app/RibbonToolBar.cpp`) lives in `src/app/`, not `libs/core/`, and is a `KToolBar` rather than the `WorkspaceRibbon`-shaped class. `addRibbonIcon(id, icon, title, cb)` matches Obsidian's `addRibbonItemButton(id, icon, title, cb)` — good. `RibbonStateController` (`src/app/RibbonStateController.cpp`) persists `hiddenItems` to the session via the `left-ribbon` key. **However:** order is taken from `iconIdsInOrder()` (insertion order), not from `Object.keys(hiddenItems)`. Obsidian's invariant is that `hiddenItems` key order *is* the runtime order (`workspace.md:374‑375`). On a layout load with a non‑identity‑order ribbon, Corbomite will silently re-sort to insertion order.
- **`registerEditorExtension` registry.** Absent. Markoff is QGraphicsView-based, not CodeMirror, so the API verb does not translate — but a substitute "extension hook list" would still be needed for plugin parity. Not yet present.
- **Leaf-iteration helpers on `Workspace`.** `allLeaves()` returns the flat insertion-ordered vector. There is no `iterateAllLeaves(cb)` / `iterateRootLeaves(cb)` / `iterateTabs(cb)` / `iterateLeaves(parentOrArr, cb)` *DFS short-circuit* set on `Workspace` itself; only `WorkspaceController::iterateAllLeaves` exists for plugins (`WorkspaceController.h:80`). Internal callers walk `allLeaves()` manually. Acceptable but the missing DFS short-circuit means a plugin could not bail out partway.
- **`getActiveFile()` priority.** Obsidian prefers `activeEditor.file` over `activeLeaf.view.file` (`workspace.md:389`). Corbomite has no `activeEditor` shadow — `WorkspaceController::activeFilePath` reads only the active leaf's view.

---

## Missing

- **`Workspace.activeEditor` getter/setter pair** with the "setter rejects MarkdownView, getter falls back to MarkdownView" invariant (`workspace.md:49, 389`). No analogue in Corbomite.
- **`fileOpened` / `file-open` event.** No `fileOpened(QString)` signal on `Workspace`. `WorkspaceController::activeFileChanged(QString)` is the closest analogue but it fires on every active-leaf transition, not just file-changed (`workspace.md:278`'s rule). The "fire only when `getActiveFile() differs from lastActiveFile`" filter is missing.
- **`quick-preview` event.** No per‑keystroke unsaved‑content sync between two leaves on the same file. Markoff has no second-pane preview so this is partially N/A, but if/when it lands the event must precede the save (`workspace.md:279`).
- **`swipe` event.** Not implemented. No mobile substrate so largely N/A, but "two-finger trackpad → back/forward" is desktop-relevant (`workspace.md:417`).
- **`registerObsidianProtocolHandler`.** Not present — `KDBusService(Unique)` is used (`src/app/main.cpp:37`) for single-instance behaviour but no `obsidian://`/`corbomite://` URL routing exists. The seven built-in handlers Obsidian ships (`open`, `search`, `new`, `show-plugin`, `show-theme`, `show-release-notes`, `debug-info`, etc.) are absent. `PathUtils.h:8‑21` only mentions emitting an `obsidian://open?vault=...&file=...` URL string for documentation purposes.
- **`registerEditorExtension`.** As noted, no analogue.
- **`Workspace.editorExtensions` flat-mutable list.** Even as an empty registry to anchor a future `registerEditorExtension`, it is missing.
- **`registerOperatorFuncConfigs`.** Bases-plugin-owned registry. Bases is implemented as a built-in view (project memory: Cluster K shipped Bases as MVP), but the plugin-API verb is not wired.
- **`WorkspaceSidedock.expand/collapse/toggle/setSize`.** Stubs at `WorkspaceSidedock.{h,cpp}` carry the *fields* (`m_collapsed`, `m_size`) but `Workspace::leftSplit()`/`rightSplit()` return nullptr; they are unreachable. Sidebar collapse animations / 200 px min / 80% workspace max / snap-collapse-below-50px are owned by `CorbomiteMDI::Sidebar` (Kate-port). Not surfaced through `Workspace`.
- **`WorkspaceFloating.children` deserialise filter** (strip non-`window` children — `workspace.md:106`). `WorkspaceFloating` is just an `addWindow`/`removeWindow` shell with no child-type validation.
- **`WorkspaceWindow` popout containing nested splits.** `WorkspaceWindow` is "an identity token" per its header (`WorkspaceWindow.h:10‑15`). Production popouts use `KDDockWidgets::Core::FloatingWindow` directly. The serializer's `materializeFloatingWindow` only handles a single tabs child — explicitly noted at `WorkspaceSerializer.cpp:347‑348` "Phase 3 only handles the single-tabs case; nested splits inside floating windows would extend this similarly to materializeSplit". So a popout with split panes will not round-trip.
- **`WorkspaceWindow.size`/`maximize`/`zoom` persistence.** `WorkspaceWindow` has no `x/y/width/height/maximize/zoom` fields. The serializer stamps live geometry via `fw->geometry()` (`WorkspaceSerializer.cpp:462‑468`) but `zoom` is never read or written.
- **`WorkspaceLeaf.openFile(file, opts?)` (resolves viewType via ViewRegistry, falls back to `openWithDefaultApp` for unknown extensions).** No equivalent on `WorkspaceLeaf`. Corbomite's path is `MainWindow::openFileInWorkspace(relativePath)` (`MainWindow.cpp:713`) which constructs a leaf and calls `setViewState` directly. The "unknown extension → openWithDefaultApp" fallback is absent.
- **`Workspace.handleLinkContextMenu(menu, linktext, source, hoverParent?)`** with the built-in link-context items (Open in new tab/right/Rename) plus mid-construction `file-menu` emit at source `"link-context-menu"`. No equivalent.
- **`Workspace.handleExternalLinkContextMenu(menu, href)` + `url-menu` event.** The signal exists on `MenuEventEmitter` but no caller emits it from a built-in URL right-click handler.
- **`Workspace.duplicateLeaf` ephemeral-state + history clone.** `Corbomite::Workspace::duplicateLeaf` (`Workspace.cpp:369‑394`) does clone view state, ephemeral state, history, pinned, group — *but* it splits via `splitLeaf` always, never as a tab. Obsidian's `duplicateLeaf(leaf, mode)` takes a mode (`tab`/`split`/`window`). Corbomite hardcodes split.
- **Linked-pane group propagation on history navigation.** Obsidian propagates back/forward navigation to every group member. `LeafHistory::goBack/goForward` is per-leaf only.
- **Vault-rename → leaf-history rewriting.** Obsidian's `vault.on("rename")` rewrites `state.state.file` in every leaf's back/forward history *and* every undo entry (`workspace.md:313, 403`). Corbomite has no rename listener that walks `m_undoHistory[*].state` or each leaf's `LeafHistory`.
- **`ViewRegistry.on("view-registered"|"view-unregistered")` → leaf rebuild.** The `Workspace.js:347` subscription that force-rebuilds matching leaves when a plugin registers/unregisters a view type is not present. If a plugin registers a view *after* the workspace loads, leaves whose cached `viewType` matches will not rematerialize.
- **`updateFrameless`.** Frameless-titlebar reflow of the sidebar toggle buttons into leftmost/rightmost tab groups — KDE doesn't typically use frameless windows, so likely intentionally N/A. Not penalising.
- **Sandbox-vault detach behaviour** (`workspace.md:259‑260`). Not present; not relevant for Corbomite.
- **`Mod+1..8` / `Mod+9` jump to Nth/last tab commands.** Per `MainWindow.cpp:1287‑1308` only next/previous tab + close + duplicate (split/window) commands are wired. The numbered‑tab quick‑jump set is absent.
- **`workspace:show-trash` command.** Absent. Trash UX exists separately.
- **`workspace:export-pdf` command.** `ExportToPdf.cpp` exists in `src/app/` but I did not confirm it is wired to a `Workspace.requestCommand` or shipped command id.
- **Tab-stacked toggle command (`workspace:toggle-stacked-tabs`).** Absent — no API to toggle stacked even though the bit round-trips via the sidecar.
- **Mid-construction `file-menu` source discriminator** (`source` ∈ `"file-explorer-context-menu"`, `"link-context-menu"`, `"more-options"`, etc., `workspace.md:283`). `MenuEventEmitter::emitFileMenu(QMenu*, QString filePath)` only carries the file path, not the *source* tag plugins use to scope handlers. Plugin handlers that say "only act in tab-header right-click" can't.

### From `leaf-utilities.md`

- **`apiVersion` / `requireApiVersion`.** Not yet a public constant. Cluster N noted "X-Corbomite-MinVersion / ApiLevel" as plugin manifest keys (project memory) but no runtime `requireApiVersion(min)` accessor.
- **`debounce(fn, delay, immediate)` helper.** No central util; callers spawn `QTimer` ad‑hoc.
- **`stripHeading` / `stripHeadingForLink` / `resolveSubpath`.** Per-`leaf-utilities.md:512‑514` audit, these are missing from `libs/core/`. They are link-format-critical for `[[Note#Heading]]` and `[[Note#^block]]` resolution. **The fact that `Workspace::openLinkText` extracts a `subpath` and passes it through ephemeral state means these helpers' downstream consumers (the View receiving `eState`) need them.**
- **Moment.js compat shim.** Project memory notes ship status; absent for a plugin API.
- **`Keymap.isModEvent` analogue.** Mod‑click → tab, Mod+Alt → split, Mod+Alt+Shift → window. Corbomite's link click handlers in MainWindow likely implement this ad-hoc; no central `Keymap::isModEvent(QMouseEvent*)` helper to share with plugins.

---

## Notable translation successes

1. **The "tab select / tab close re-emission" indirection** (`Workspace.cpp:241‑263`). KDDW's `isCurrentTabChanged`/`isOpenChanged` signals fire from substrate guts at unpredictable times (during construction, during float, during destruction). Workspace re-emits them *as its own signals* and the host runs the policy decision. This decouples KDDW's eager signaling from Workspace's identity-gated state machine. The `m_leavesById.contains(leaf->id())` guard on the re-emit prevents fire-after-close.

2. **The `QSignalBlocker` on `addDockWidgetAsTab`** (`Workspace.cpp:276‑288`). KDDW marks the freshly-docked widget as the current tab and emits `isCurrentTabChanged(true)` *before the caller has set viewState*. Without the blocker, that would cascade into a synthetic `activeFileChanged("")`, blanking sidebar panels (referenced as BUG-20260425). The fix is the right shape — block on the leaf's dock widget specifically, not globally.

3. **Layout-ready gate as a feedback-loop circuit-breaker.** `setLayoutReady(false)` during `deserialize` (`Workspace.cpp:752‑755`) suppresses the per-dock-widget cascade KDDW emits during layout materialization. The "ping through null" trick at `Workspace.cpp:848‑851` (`m_activeLeaf = nullptr; setActiveLeaf(target);`) forces a single post-load `activeLeafChanged` exactly once. Two correctness layers stacked on the same gate, consistent with the audit invariant at `workspace.md:387`.

4. **The destruction-order dance** (`Workspace.cpp:120‑149`). KDDW's `MainWindow` destructor disposes of every docked DockWidget; if leaves are still QObject children of `Workspace`, `~QObject`'s child cleanup will double-free their dock widgets through `~WorkspaceLeaf`'s `delete m_dockWidget`. Workspace snapshots `m_leaves`, releases dock widgets on closed-but-pending children, *then* deletes the MainWindow. The `releaseDockWidget()` API (`WorkspaceLeaf.h:104‑109`) is the right escape hatch.

5. **Vault-scoped DockRegistry namespacing.** `uniqueNameFor(vaultId, leafId)` (`Workspace.cpp:31‑36`) prefixes leaf ids with vault id so two open vaults' KDDW DockRegistry entries don't collide. Future-proofs multi-vault-per-process even though current behaviour is one-vault-per-process.

6. **Plugin-facing facade pattern.** `WorkspaceController` (`proxies/WorkspaceController.h`) takes opaque string leaf ids over the wire instead of raw pointers. Cluster Y/Q's "plugins never see internal types" rule is consistently honoured.

---

## Notable concerns / suspected bugs

### High severity

1. **`m_tabGroupOf` is updated only on programmatic creation, never on user-initiated drag.** `Workspace.h:267‑271` openly admits "until then the model lags those moves" referring to drag-tab-to-other-group via the substrate. Result: after a user drags a tab between two groups, `nextLeafInActiveGroup` will return the wrong leaf (cycling within the *original* group), `serialize()` will emit a stale group structure, and `closeOtherLeavesInGroupOf` will close the wrong tabs. The router (`WorkspaceActiveLeafRouter`) only resyncs *active leaf identity*, not group membership.

2. **`Workspace::serialize()` cannot round-trip nested splits.** `Workspace.cpp:649‑658` flattens to one root split + N flat tabs groups. Any vault opened in Obsidian with `{type: split, children: [{type: split, ...}]}` *will be saved* as a single-level split + flat tabs. Subsequent re-open in Obsidian sees a degraded layout. This is a P0 vault‑interop blocker if Corbomite is ever pointed at an in-use Obsidian vault.

3. **Two competing serializers.** `Workspace::serialize/deserialize` (`Workspace.cpp:649‑852`) and `WorkspaceSerializer::toJson/fromJson` (`WorkspaceSerializer.cpp:407‑477`) both produce/consume `workspace.json`-shaped JSON, but they walk different sources of truth (`m_leaves` vs `DockRegistry`) and emit *different* placeholder defaults (Workspace writes leaf state from `WorkspaceLeaf::serialize`; the serializer reads/writes a sidecar `leafSidecar` map keyed by uniqueName). The `materializeSplit` path even uses static-int counter group ids `g_1, g_2, ...` (`Workspace.cpp:730‑732`) that won't match the persistent group ids on save. **Which one is authoritative?** `MainWindow::openVaultAt` (`MainWindow.cpp:2103‑2106` area) routes through `SessionManager` which uses `Workspace::serialize` indirectly. The standalone `WorkspaceSerializer` appears to be Phase 3 vestige with no live caller in the audited surface — needs confirmation, otherwise it's dead code that drifts from reality.

4. **`SessionManager` writes to `<vault>/.obsidian/workspace.json`** (per `MainWindow.cpp:2106`) — good, this is the right path. But its `m_unknownRoot` round-trip strategy means **Obsidian's `left`/`right`/`floating`/`lastOpenFiles` sub-trees survive blindly without modification** (`SessionManager.cpp:85‑91`). If Corbomite opens an Obsidian-authored workspace.json with a populated `left`/`right` sidedock subtree, those keys re-serialize to disk *but* the in-process state diverges (sidebars are managed by `CorbomiteMDI::Sidebar`, not the persisted `left`/`right` JSON). Result: the on-disk left-pane subtree freezes at whatever Obsidian last wrote, while the user's actual sidebar configuration drifts elsewhere. Subtle data-loss.

5. **`currentTab` per‑group is collapsed into "is‑active‑leaf‑index" globally.** `Workspace.cpp:691‑693` writes `currentTab = i` only for the bucket containing `m_activeLeaf`; all other buckets are written with default `currentTab = 0`. On a vault that legitimately has e.g. two tab groups with the second tab selected in each, only one group's selection survives.

6. **`Workspace::deserialize` rebuilds tab groups but `tabSelectRequested` is not gated by an in-deserialize lock specifically for the *bulk-add* case** — the layout-ready gate covers `setActiveLeaf` re-entry but not the *first* `addDockWidgetAsTab` per group. KDDW's first-tab insert may fire `isCurrentTabChanged(true)` for *each group's first leaf* in turn, re-routing each through `tabSelectRequested → setActiveLeaf` (which is gated, fine). However if any host wires `tabSelectRequested` to *additional* side-effects (e.g., tracking last-selected per group for restore), those side-effects fire spuriously per leaf inserted.

### Medium severity

7. **`undoCloseLeaf` does not restore container/parent.** `Workspace.cpp:329‑346` recreates the leaf in the *active* group, not in the original group / parent split. Obsidian's invariant explicitly says "restored to original container + tab group if live" (`workspace.md:415`). The `parentId` and `rootId` fields in `UndoEntry` are populated but never *consumed* by `undoCloseLeaf`. If you close a tab in the right pane, the active pane is on the left, undo brings it back on the *left*.

8. **`undoCloseLeaf` does not restore `leafHistory` or `eState`.** The captured fields exist in `UndoEntry` (`Workspace.h:28‑37`), but `undoCloseLeaf` (`Workspace.cpp:329‑346`) only restores `id`/`pinned`/`group`/`viewState`. The leaf history is lost.

9. **`Workspace::resize()` not debounced.** Obsidian's is `requestResize = debounce(onResize, 0)` (`workspace.md:281`). Corbomite emits per-Resize-event. A continuous resize drag will fire dozens of `resize()` per second to every connected slot. Plugins that re-layout heavy widgets on resize will jank.

10. **`activeLeafChanged` not debounced.** `Workspace::setActiveLeaf` emits synchronously on every change. Obsidian schedules a debounced 0ms `activeLeafEvents` (`workspace.md:277`). For rapid leaf-flicker (e.g., closing several tabs in succession), Corbomite emits N times where Obsidian emits 1 (the final). Plugins that do work on `active-leaf-change` will run N times.

11. **`WorkspaceLeaf::getViewState()` re-reads from `m_view` *after* `setDeferred(true)` has already cleared the view.** `WorkspaceLeaf.cpp:129‑142` returns `m_deferredViewState` only when `m_deferred && !m_view` — but `setDeferred(true)` calls `closeCurrentView()` which nulls `m_view` (`WorkspaceLeaf.cpp:213‑220`), so the cached path is taken. Good. **But:** `m_deferredViewState` is only updated in `setDeferred(true)` if `m_view` was non-null at that moment (`WorkspaceLeaf.cpp:215‑216`); a deferred-from-construction leaf (e.g., loaded from JSON with `state.icon`/`state.title` cached, `setViewState` not yet called) has empty `m_deferredViewState` and `getViewState()` returns `{}`. The deserialization path in `Workspace::deserialize` (`Workspace.cpp:823‑836`) calls `setDeferred(true, ...)` *after* `setViewState(viewState)`, so this is OK in the live path — but a future caller that defers without first hydrating will hit a corner case.

12. **`closeCurrentView` calls `m_view->deleteLater()` then sets `m_view = nullptr`** (`WorkspaceLeaf.cpp:118‑120`). If a slot connected to `viewChanged(nullptr)` (emitted next by `open(nullptr)`) re-enters `WorkspaceLeaf` and queries `view()`, it will see nullptr correctly — but if a slot connected to a *prior* `viewChanged(view)` is still queued and runs after the deleteLater queue, `m_view.data()` is null (QPointer) so accesses are safe. Looks correct.

13. **`HoverLinkSourceRegistry` is constructed in `MainWindow::MainWindow` (`MainWindow.cpp:309`) and stored on `MainWindow`, not on `Workspace`.** The audit places hoverLinkSources as a `Workspace`-owned registry (`workspace.md:362`). Plugin code expecting to call `app.workspace.registerHoverLinkSource(...)` has no path; a plugin must reach for `MainWindow::m_hoverSources` somehow. This is consistent with the project's "host owns plugin facades" pattern — but the `Workspace::registerHoverLinkSource` plugin-API call site does not exist on `Workspace`.

14. **Popout window cleanup race.** `popoutLeaf` connects `&QObject::destroyed` on the KDDW FloatingWindow to a lambda emitting `windowFrameChange` (`Workspace.cpp:501‑505`). If the user closes the popout via the OS X-button (which destroys the FloatingWindow), `WorkspaceWindow*` (the bookkeeping shell in `m_windows` and `m_floating`) is **never removed** — `reparentToMain(window)` is the only path that calls `m_windows.removeOne(window)`. Result: closing a popout via X leaks a `WorkspaceWindow*` and `m_windows.size()` permanently overstates the popout count.

15. **`Workspace::deserialize` calls `qDeleteAll(m_leaves)` but does not call `releaseDockWidget()` on each before deletion.** `Workspace.cpp:758` deletes leaves while the KDDW MainWindow is still alive. `~WorkspaceLeaf`'s `delete m_dockWidget` will then dispose KDDW dock widgets while KDDW still has references in DockRegistry. This may or may not fault depending on KDDW's tolerance — not ruling out a sporadic crash on vault reload.

16. **`MenuEventEmitter::emitFileMenu` signature carries only the file path, not a "source" discriminator.** Plugins cannot scope handlers to a specific menu invocation (`workspace.md:283`'s six source values). The `View::onPaneMenu(QMenu*, QString source)` overload (`View.h:55`) is the model — `MenuEventEmitter` should match. Without it, a plugin handler fires for both file-explorer-context-menu and tab-header right-clicks indiscriminately.

17. **`WorkspaceLeaf::setViewState` calls `setState` *after* `open(newView)` emits `viewChanged(m_view)` once** (`WorkspaceLeaf.cpp:163‑169`). `viewChanged` then re-emits inside the `if (!viewState.isEmpty())` block. So consumers see two `viewChanged` per setViewState — once with empty view state, once with state applied. Consumers reading view title/icon on the first emission see stale values. Either coalesce (build view+state then emit once) or document.

### Low severity

18. **`generateTabGroupId` uses `QRandomGenerator::global()` which is per-process** — collisions across re-loads are possible (probability ~1 in 2^64) but harmless; the ids are not durable.

19. **`Workspace::pushLastOpenFile` caps at 50** (`Workspace.cpp:206‑212`), but the cap is not surfaced or configurable. Obsidian's recent-file tracker uses different caps; harmless divergence.

20. **No `Workspace::on(string, cb)` event-string-API** for plugin parity — every consumer must connect to a specific Qt signal. The plugin layer (`MenuInjector`, `WorkspaceController`) wraps individual signals as named methods, which is *better* design than Obsidian's stringly-typed `on(name, cb)`, but doesn't satisfy plugin authors who literally search for `workspace.on("active-leaf-change")`.

---

## Layout JSON compatibility risks

Cumulative compat hazard reading or re‑saving an Obsidian-authored `workspace.json`:

| Field / shape | Obsidian | Corbomite behavior | Risk |
|---|---|---|---|
| Path: `<vault>/.obsidian/workspace.json` | yes | yes (`MainWindow.cpp:2106`, `Workspace.cpp:856`) | — |
| `main` root with nested splits | preserved | flattened to one split + N flat tabs (`Workspace.cpp:651‑658`) | **HIGH** — round-trip lossy |
| `dimension` flex-grow per child | preserved | dropped on read, never written | HIGH — pane sizes lost |
| `tabs.currentTab` per group | per-group | only active-leaf's group writes nonzero | HIGH — per-group tab selection lost |
| `tabs.stacked` | semantic | persisted via sidecar but UI behaviour absent | MEDIUM — round-trips but doesn't render |
| `leaf.id` (16 hex) | random/persisted | matches format (`WorkspaceLeaf.cpp:65‑74`) | — |
| `leaf.state` (ViewState) | view-typed payload | round-trips | — |
| `leaf.pinned`/`leaf.group` | optional | round-trips (`WorkspaceLeaf.cpp:336‑339`) | — |
| `left`/`right` (sidedock subtrees) | full split tree | passed through `m_unknownRoot` blindly (`SessionManager.cpp:85‑91`) | **HIGH** — write-through divergence: on-disk shape doesn't reflect actual sidebar state |
| `floating` wrapper | array of `window` nodes | partially generated in `WorkspaceSerializer` (`WorkspaceSerializer.cpp:450‑473`); but `Workspace::serialize` itself doesn't emit `floating` | HIGH — popouts lost in primary serialize path |
| `window.x/y/width/height/maximize/zoom` | full | `x/y/w/h/maximize` only via `WorkspaceSerializer` (`WorkspaceSerializer.cpp:462‑468`); no `zoom`; not emitted by `Workspace::serialize` | MEDIUM |
| `['left-ribbon'].hiddenItems` | order-preserving | `RibbonStateController` writes a flat `{id: true}` map, order from `iconIdsInOrder()` not from key order (`RibbonStateController.cpp:65‑81`) | MEDIUM — order not respected on load |
| `lastOpenFiles` | most-recent-first | exposed via `SessionManager::lastOpenFiles()` from `m_unknownRoot`, populated by `Workspace::pushLastOpenFile` | LOW — split ownership; SessionManager round-trips Obsidian's value but Workspace writes its own |
| `active` (leaf id) | yes | yes (`Workspace.cpp:701‑703`) | — |
| Unknown leaf keys | preserved | preserved in `WorkspaceSerializer` only (`WorkspaceSerializer.cpp:94‑99, 164‑166`); `Workspace::serialize` discards | MEDIUM — new Obsidian fields lost on save through primary path |
| `workspace-mobile.json` (mobile) | separate file | not implemented | N/A — desktop-only |

The combined risk: **Corbomite is currently safe to *read* an Obsidian workspace.json, but unsafe to *write back* — round-tripping degrades the layout in 5+ ways simultaneously.** A user who pointed Corbomite at an in-use Obsidian vault, opened a single file, and exited would re-save a structurally simpler workspace.json. Their next Obsidian session would show a flattened layout with lost pane sizes, lost per-group tab selections, lost popouts (if the primary serialize path runs), and a frozen-in-time `left`/`right` sidedock tree.

The two-serializer split (`Workspace::serialize` vs `WorkspaceSerializer::toJson`) suggests an in-flight transition. Until consolidated, every code path needs to be checked individually for which writer it routes through.

---

## Summary

Corbomite has the *right architectural skeleton* for `Workspace` — a single Qt-signal-driven aggregate object owning leaves, an undo stack, a layout-ready gate, a deferred-loading mechanism, and a plugin-facing controller proxy. The KDDockWidgets substrate covers tab/split/float interaction at a level that would have taken months to hand-roll. Per-leaf state (`pinned`, `group`, `LeafHistory`, `viewState`) round-trips cleanly. The hover-link-source registry, menu-event emitter, and `WorkspaceController` plugin facade are in place ahead of plugins consuming them.

But the substrate translation has **leaky seams**. KDDW exposes no Group enumeration API, so `m_tabGroupOf` lags user drags. The serialize path flattens nested splits and emits one global `currentTab`. Two parallel serializers (`Workspace`, `WorkspaceSerializer`) produce different shapes from different sources of truth — an unfinished transition. The Obsidian-shape `WorkspaceSidedock`/`WorkspaceRibbon`/`WorkspaceFloating`/`WorkspaceWindow` types are deliberately documented as Phase 7.5 stubs returning nullptr, with the real sidebar living outside the workspace tree (`CorbomiteMDI::Sidebar`).

Highest-impact gaps: (1) nested-split round-trip; (2) `m_unknownRoot` write-through silently freezing Obsidian's `left`/`right` JSON; (3) `undoCloseLeaf` losing original parent + leafHistory + eState; (4) popout-window leak on X-close; (5) absent `obsidian://` protocol handlers and `apiVersion`/`requireApiVersion`; (6) `MenuEventEmitter::fileMenu` lacking the source discriminator. Layout-JSON compatibility is read-safe but write-lossy.
