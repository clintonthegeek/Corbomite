# `obsidian/workspace` — workspace layout, splits, tabs, leaves

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/workspace/`
**File count:** 12
**Files:** `Workspace.js`, `WorkspaceContainer.js`, `WorkspaceFloating.js`, `WorkspaceItem.js`, `WorkspaceLeaf.js`, `WorkspaceParent.js`, `WorkspaceRibbon.js`, `WorkspaceRoot.js`, `WorkspaceSidedock.js`, `WorkspaceSplit.js`, `WorkspaceTabs.js`, `WorkspaceWindow.js`.

**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> **Purpose:** The window-chrome / tabbing / docking layer. `Workspace` is the root `Events`-derived object that owns the split/tab/leaf tree and plumbs menus, hover previews, protocol handlers, and the public "where is the active file/view?" accessors used by every plugin.
> **Key exports / primitives:**
> - `Workspace` — `activeLeaf`, `getActiveFile`, `openLinkText`, `getLeaf`, `getLeavesOfType`, `revealLeaf`, `iterateAllLeaves`, `setActiveLeaf`, `splitActiveLeaf`, `duplicateLeaf`, `requestSaveLayout`, `getLayout`/`changeLayout`, `registerHoverLinkSource`, `registerEditorExtension`, `registerObsidianProtocolHandler`. Events: `active-leaf-change`, `file-open`, `layout-change`, `layout-ready`, `quick-preview`, `resize`, `window-frame-change`, `file-menu`, `url-menu`, `editor-menu`, `swipe`.
> - `WorkspaceLeaf` — `view`, `parent`, `containerEl`, `tabHeaderEl`, `getViewState`, `setViewState`, `setPinned`, `togglePinned`, `setGroupMember`, `detach`. Events: `pinned-change`, `group-change`.
> - `WorkspaceSplit` / `WorkspaceTabs` — split/tab containers, child management, drag-drop reorder, stack mode.
> - `WorkspaceSidedock` — collapsible left/right dock; `expand`/`collapse`/`toggle`.
> - `WorkspaceWindow` — popout window (separate Electron `BrowserWindow`).
> - `WorkspaceFloating` / `WorkspaceContainer` / `WorkspaceItem` / `WorkspaceParent` — abstract bases / floating-window layer.
> - `WorkspaceRoot` — root item.
> - `WorkspaceRibbon` — activity bar (left or right edge ribbon icons + settings cog).
> **On-disk contracts:** persisted layout in `.obsidian/workspace.json` (default) and per-named-workspace in `.obsidian/workspaces.json`.
> **Cross-domain dependencies:** depends on `core`, `vault`, `views`; consumed by `plugin`, every `View`.
> **Corbomite-relevance note:** Partial. Corbomite has split/tab plumbing in `src/editor/EditorViewManager`, `EditorViewSpace`, sidebar `KSelector`-like panels in `src/sidebar/`. Popout-windows, hover-link sources, protocol handlers, named-workspace switching: NOT confirmed. The persisted-layout JSON schema is the compat target for "open a vault and have your layout restored".
> **Pass 2 focus:** the layout-JSON schema (most interop-critical part); `getLeaf` modes; `openLinkText` resolution; the **complete event list** (this is a primary plugin extension surface — see `02-extension-surfaces.md`).

**De-minifier artifact note:** 12 files, but only **four canonical extractions**. MD5 pre-flight + header comparison shows three duplicate groups (only the `// public API symbol:` line differs):
- **Group A** (`app.js 81826–83101`, ~50 KB): `WorkspaceItem`/`Parent`/`Split`/`Sidedock`/`Container` hierarchy. Duplicated across `WorkspaceItem.js`/`Parent.js`/`Split.js`/`Sidedock.js`/`Container.js`. Canonical: `WorkspaceSplit.js`.
- **Group B** (`app.js 163528–163910`, ~13 KB): `WorkspaceRibbon`/`Root`/`Floating`/`Window` + `F0`/`N0` filename constants. Duplicated across `WorkspaceFloating.js`/`Ribbon.js`/`Root.js`/`Window.js`. Canonical: `WorkspaceFloating.js`.
- **Group C** (`app.js 83142–85559`, ~97 KB): `WorkspaceTabs` + `qD` (history helper) + `WorkspaceLeaf` + `WD` (mobile-drawer). Duplicated across `WorkspaceLeaf.js`/`WorkspaceTabs.js`. Canonical: `WorkspaceLeaf.js`.
- **Workspace.js** (`app.js 164034–167934`, 153 KB) is unique.

The **first ~607 lines of every Group-A file are bleed-over** from an adjacent extraction window: they define `FileView`, the `SD`/`xD`/`TD` vault-chooser modal cluster, and `AD`/`PD` animation constants. These belong to `views/` and `ui/popups/` and are not audited here per TEMPLATE rule 2. Real Group-A content starts at line 607 with `AD = 200, PD = {…}, WorkspaceItem = (function(e){…})(Events)`.

---

## 1. Public API surface

Ten exported classes plus two module-local constants. Single-rooted hierarchy:

```
Events → WorkspaceItem → WorkspaceParent → {
  WorkspaceSplit ("split") → { WorkspaceSidedock, WorkspaceContainer → { WorkspaceRoot, WorkspaceWindow ("window") } }
  WorkspaceFloating ("floating"), WorkspaceTabs ("tabs"), WD ("mobile-drawer", internal), WorkspaceLeaf ("leaf")
}
Standalone: WorkspaceRibbon (one per side), Workspace (extends Events; owns the tree)
```

### `Workspace`

- Class, extends `Events`. Assigned to `App.workspace`.
- **Constructor `(app, containerEl)`:** nulls all layout fields, `layoutReady=false`, sets `undoHistory=[]` (cap 10), `protocolHandlers=new Map()`, `hoverLinkSources={}`, `editorExtensions=[]`, `onLayoutReadyCallbacks=[]`. Creates child `Scope` bound to Escape (refocus active Markdown editor). Instantiates `recentFileTracker` (`B0`, outside this domain). Installs window listeners (`resize`/`popstate`/`copy`/`paste`/`cut`/`focus`/`fullscreenchange`/iOS `statusTap`/Android `backButton`); overrides `window.history.{forward,back,go}` to proxy to active leaf. Subscribes to `vault.on("rename")` (rewrites `state.state.file` in every leaf + undo history) and `viewRegistry.on("view-registered"|"view-unregistered")` (force-rebuild matching leaves). Registers built-in menu handlers, all commands (Section 6), seven `obsidian://` protocol handlers, four hover-link sources.
- **Plugin-facing fields:** `app`, `activeLeaf`, `activeEditor` (getter→`getActiveViewOfType(MarkdownView)` fallback; setter rejects MarkdownView), `rootSplit`/`leftSplit`/`rightSplit`/`floatingSplit`/`leftRibbon`/`rightRibbon`, `layoutReady`, `containerEl`, `hoverLinkSources`, `protocolHandlers`, `editorExtensions`, `undoHistory`.
- **Method groups:**
  - **Layout I/O.** `readWorkspaceFile()` returns `{}` on any error. `loadLayout()`: read → `recentFileTracker.load(lastOpenFiles)` → `setLayout(json)` → fire `layout-ready` → drain `onLayoutReadyCallbacks` with `sleep(0)` between plugin callbacks. `setLayout(json)` deserialises `.main`/`.left`/`.right`/`.floating`/`['left-ribbon']`; falls back to fresh `WorkspaceRoot("vertical")` with one leaf opening `lastOpenFiles[0]` if no `.main`; selects `json.active`; awaits `loadIfDeferred` for visible leaves; sets `layoutReady=true`. `clearLayout()` closes leaves + detaches splits + unloads components + closes popouts. `changeLayout(json) = clearLayout(); setLayout(json)`. `getLayout() → {main, left, right, ['left-ribbon'], floating?, active?}`. `saveLayout()` writes `JSON.stringify(getLayout()+{lastOpenFiles}, null, 2)`. `requestSaveLayout = debounce(saveLayout, 1000)`.
  - **Leaf lookup/iteration.** `getLeafById`, `getActiveViewOfType(Class)`, `getActiveFile()` (prefers `activeEditor.file`), `getActiveFileView()`, `getGroupLeaves`, `getMostRecentLeaf(scope?)`, `getLeavesOfType`, `detachLeavesOfType`, `iterateLeaves(parentOrArr, cb)` DFS short-circuit, `iterateRootLeaves`/`iterateAllLeaves`/`iterateTabs`, `getFocusedContainer`, `isAttached`/`isInSidebar`, `getAdjacentLeafInDirection(leaf, side)`.
  - **Leaf creation/opening.** `getLeaf(mode?, dir?)` factory: `"split"`→`splitActiveLeaf`, `"tab"`/`true`→`createLeafInTabGroup` (focus iff `focusNewTab`), `"window"`→`openPopoutLeaf`, else→`getUnpinnedLeaf`. Plus `splitActiveLeaf`, `duplicateLeaf` (clones state+ephemeral+history), `createLeafInTabGroup`, `createLeafInParent`, `createLeafBySplit`, `openPopout`, `openPopoutLeaf`, `moveLeafToPopout` (preserves zoom+size), `openLinkText(linktext, source, mode, opts?) = getLeaf(mode).openLinkText(...)`.
  - **Active-leaf plumbing.** `setActiveLeaf(leaf, {focus?})` updates state + schedules debounced `activeLeafEvents` → `active-leaf-change(leaf)` + iff file-changed `recentFileTracker.onFileOpen(new, old)` + `file-open(new)`. `focusLeaf` (mobile auto-collapses sidedocks). `revealLeaf` expands collapsed dock + selects tab + scrolls + awaits `loadIfDeferred`. `ensureSideLeaf(viewType, side, opts)`.
  - **Drag-drop.** `onDragLeaf(event, leaf)` cross-window drag preview + drop overlay; `getDropLocation`/`getDropDirection`.
  - **Frameless chrome.** `updateFrameless()` reflows sidebar toggle buttons into leftmost/rightmost `WorkspaceTabs`; emits `window-frame-change`.
  - **Menu plumbing.** `handleLinkContextMenu(menu, linktext, source, hoverParent?)` adds link-context items + emits `file-menu` source=`"link-context-menu"`. `handleExternalLinkContextMenu(menu, href)` + emits `url-menu`.
  - **Undo history.** `pushUndoHistory(leaf, parentId, rootId)` unshifts (cap 10). `hasUndoHistory()`. Consumer: `workspace:undo-close-pane`.
  - **Registries.** `registerObsidianProtocolHandler` (throws on dup), `registerHoverLinkSource`, `registerOperatorFuncConfigs`, `registerEditorExtension` (last calls `updateOptions()` reconfiguring every MarkdownView CM `Compartment`). Each has `unregister*`.
  - **URI handling.** `registerUriHook()` sets `window.OBS_ACT = args => protocolHandlers.get(args.action)?.(args) ?? Notice(...)`. Mobile: Capacitor `appUrlOpen`; URL vault ≠ current → stash in `sessionStorage` + `location.reload()`.
  - **Layout-change bookkeeping.** `onLayoutChange(item?)` queues + schedules `requestUpdateLayout` (`Cc(updateLayout)`). `updateLayout()` drains queue, synthesises default tab group in empty rootSplit, re-anchors `activeLeaf`, prunes single-leaf groups, then `updateFrameless`+`updateTitle`+`requestSaveLayout`+`requestResize`+`requestLayoutChangeEvents` (debounced 10 ms `trigger("layout-change")`).
  - **Quit.** Self-subscribes `"quit"`: cancels `requestSaveLayout`, flushes `vault.requestSaveConfig`, pushes dirty `TextFileView.save()` promises into `event.addPromise`.
  - **Quick-preview / resize.** `onQuickPreview(file, data)` → `quick-preview`; `onResize()` → `resize`; `requestResize = debounce(onResize, 0)`.

### `WorkspaceItem`

- Class, extends `Events`. Constructor `(parentOrWorkspace, id?)`; `id` defaults to 16-char random (`cc(16)`). Creates `containerEl` with `<hr class="workspace-leaf-resize-handle">` → `onResizeStart` → parent `WorkspaceSplit.onChildResizeStart`.
- Fields: `containerEl`, `dimension: number | null` (flex-grow `(0, 100)`), `component: Component`, `app`, `workspace`, `id`, `parent`, `resizeHandleEl`.
- Methods: `setParent`, `getRoot`, `getContainer`, `serialize() → {id, type, dimension?}`, `setDimension(n|null)` (clamps `[0,100]` else null; sets `flexGrow`), `detach()`, `getIcon() → "lucide-file"` (overridden by leaf), `parentSplit` alias.
- Mixes in `Events` — used by `WorkspaceLeaf` for `pinned-change`/`group-change`/`history-change`.

### `WorkspaceParent`

- Abstract, extends `WorkspaceItem`. Adds `children: WorkspaceItem[]`, `allowSingleChild` (default `false` → at 1 child, dissolves; remaining promoted up inheriting `dimension`), `autoManageDOM` (default `true` → child `containerEl` auto-appended/detached).
- Methods: `insertChild(idx, child)` (out-of-range appends), `removeChild(child)` (dissolves to 1 when `!allowSingleChild`; detaches at 0), `replaceChild(idx, child)`, `recomputeChildrenDimensions()` hook, `serialize()` adds `{children}`.

### `WorkspaceSplit`

- Class, extends `WorkspaceParent`. Constructor `(workspace, direction, id?)`; direction `"horizontal"` (top-to-bottom) or `"vertical"` (left-to-right).
- Fields: `type = "split"`, `direction`, `isResizing`, `resizeStartPos`, `originalSizes`. Serialisation: `{id, type:"split", direction, dimension?, children:[…]}`.
- Methods: `setDirection(dir)`, `recomputeChildrenDimensions()` (normalises non-null sums to 100), `onChildResizeStart(child, event)` (drag-resize, 200 px min, null-dimensioned stays flexible), `finishResize()` (pixels → %, emits `requestSaveLayout` + `requestResize`).

### `WorkspaceSidedock`

- Class, extends `WorkspaceSplit`. Adds `side: "left"|"right"`. Fields: `size` (px, default 300), `collapsed` (default false), `side`, `emptyStateEl`.
- Serialisation: parent's + `{width: size, collapsed?: true}` (still `type:"split"`; `width` ≠ `dimension`).
- Methods: `setSize(px)`, `toggle/expand/collapse` (animated 140 ms; `collapse` reassigns `activeLeaf` to `rootSplit` if active is inside dock), `onSidedockResizeStart` (200 px min, 80% workspace max, snap-collapse below 50 px), `recomputeChildrenDimensions()` (auto-collapse + empty-state at 0 children; right docks hide right ribbon).

### `WorkspaceContainer`

- Class, extends `WorkspaceSplit`. `allowSingleChild = true` (never dissolves). `onFocus()` (100 ms debounced; sets most-recent leaf active on document focus unless modal open). `focus()` (desktop: restore-if-min + focus electronWindow). `win`/`doc` getters.

### `WorkspaceRoot`

- Class, extends `WorkspaceContainer`. Adds `mod-root` class. `win = window`, `doc = document`.

### `WorkspaceWindow`

- **Kind:** class, extends `WorkspaceContainer`. Electron popout hosting a detached subtree.
- **Constructor `new WorkspaceWindow(workspace, id?, opts?)`:** opts `{x, y, width, height, size, maximize, zoom}`. Opens via `window.open("about:blank", "_blank", "popup,...")`, injects `<base href=…>`, shares `app` global, wires the workspace containerEl into the new doc body, forwards `history.forward/back/go` to main window. Listeners: `focus` (rebinds `activeWindow`/`activeDocument`), `beforeunload` (close unless app quitting), debounced `resize` → leaf onResize + `requestSaveLayout` + `updateSize`. Subscribes to workspace `quit` (closes popout) and `layout-change` (updates title). Fires `window-open`.
- **Serialisation:** parent's `{id, direction, dimension?, children:[…]}` + `{type:"window", x, y, width, height, size, maximize, zoom}`.
- **`close()`** reparents `activeLeaf` off, detaches all subtree leaves, unloads `Component`, removes document listeners, `win.close()`, emits `window-close`, nulls `win`. `removeChild` auto-closes at 0 children.

### `WorkspaceFloating`

- **Kind:** class, extends `WorkspaceParent`. `type = "floating"`, `allowSingleChild = true`, `autoManageDOM = false`. Children may only be `WorkspaceWindow` (others stripped during deserialise).

### `WorkspaceTabs`

- **Kind:** class, extends `WorkspaceParent`. Constructor adds `workspace-tabs` class; creates `tabHeaderContainerEl`/`tabsInnerEl`/`tabsContainerEl`, "+" new-tab button, chevron-down tablist dropdown (emits `tab-group-menu(menu, tabs)`), spacer. Click delegation selects tab.
- **Fields:** `type = "tabs"`, `allowSingleChild = true`, `autoManageDOM = false`, `tabHeaderEls`, `currentTab` (default 0), `hasLockedTabWidths`, `isStacked`.
- **Serialisation:** `{id, type:"tabs", dimension?, children:[…], currentTab?, stacked?: true}` (defaults omitted).
- **Methods:** `setStacked(bool)` (`canStackTabs` only), `selectTabIndex(idx)`/`selectTab(child)`, `updateTabDisplay()` (stacked = side-by-side, else only current visible), `getTabInsertLocation(clientX)` (drag-drop split + index), `lockTabWidths`/`unlockTabWidths` (pins during close-hover), `updateSlidingTabs()` (stacked offsets), `scrollIntoView(idx)`. `insertChild`/`removeChild` capture `workspace.lastTabGroupStacked = isStacked` on dissolve so the next tab-group inherits. `static createFrom(workspace, leaf)` factory (used by `moveLeafToPopout`/`openPopoutLeaf`).

### `WorkspaceLeaf`

- **Kind:** class, extends `WorkspaceItem`. Constructor adds `workspace-leaf` class, creates draggable `tabHeaderEl` (icon+title+status+close), wires `ResizeObserver` → `onResize` (debounce 20 ms). Starts with `view = _empty = new tD(this)` (empty-view, in `views/`).
- **Fields:** `type = "leaf"`, `activeTime`, `history: qD`, `hoverPopover`, `group: string | null`, `pinned`, `width/height`, `resizeObserver`, `working` (single-flight guard around `setViewState`), `view`, `_empty`, plus DOM refs (`tabHeaderEl`, etc.).
- **Serialisation:** `{id, type:"leaf", dimension?, state: ViewState, pinned?: true, group?: ...}`.
- **Method groups:**
  - *View control:* `openFile(file, opts?)` resolves `viewType` via `viewRegistry.getTypeByExtension(ext)` (keeps current FileView if it accepts the new ext); unknown type → `app.openWithDefaultApp(path)`. Copies `file.path` into `state.file`. `openLinkText(linktext, source, opts?)` — `metadataCache.getFirstLinkpathDest`; creates via `fileManager.getNewFileParent`+`createNewFile` if unresolved and slashless; subpath → `eState.subpath`. `open(newView | null)`, `rebuildView()`, `loadIfDeferred()`.
  - *State:* `getViewState()`, `setViewState(state, eState?)` — single-flight; uses deferred placeholder when icon+title cached AND leaf not yet visible; honours `popstate`/`sync` (skip history), `state.close`, `state.done` post-hook, `state.active === true` → `setActiveLeaf({focus:true})`. `getEphemeralState()`/`setEphemeralState(e)` — `{focus:true}` first blurs foreign element + clears selection.
  - *Pinning + grouping:* `setPinned(bool)` propagates to every linked-pane group member; fires `pinned-change`. `setGroup(id | null)` fires `group-change`. `setGroupMember(other | null)` creates/reuses target's group id.
  - *Detach:* `detach()` calls `workspace.pushUndoHistory(this, parentId, rootId)`, closes view, swaps in `_empty`, disconnects observer.
  - *Headers:* `updateHeader()`. `onOpenTabHeaderMenu(event, el)` builds 13-section menu (Section 10), calls `view.onTabMenu(menu)` + `view.onPaneMenu(menu, source)`, emits `leaf-menu`.
  - *Navigation:* `canNavigate()` = `view.navigation && !pinned`. `isVisible`, `highlight`/`unhighlight`, `getHistoryState`/`recordHistory(prev)` (push only on JSON-different state).
  - *Drag-drop:* `handleDrop(event, data, dryRun)` — file/link/bookmark drops.
  - *Event mix:* `trigger`/`on` Events pass-through.

### `qD` (LeafHistory, module-local; attached to `WorkspaceLeaf.history`)

Per-leaf navigation back/forward stacks powering tab-header back/forward + `workspace:undo-close-pane`. Constructor `new qD(ownerLeaf)`. Fields: `backHistory[]`, `forwardHistory[]`. Methods: `pushState(state)` (caps back at 20, clears forward, fires `history-change`), `back/forward/go(n)` (await-based, shows Notice if `owner.working`), `updateState(state)`, `serialize()/deserialize(s)`.

### `WD` (MobileDrawer, module-local; replaces `WorkspaceSidedock` on `Platform.isMobile`)

Extends `WorkspaceParent`, `type = "mobile-drawer"` (persisted only when mobile). Same `expand`/`collapse`/`toggle`/`setPinned` API as `WorkspaceSidedock`, plus swipe-gesture handling, backdrop element, tabs dropdown chooser, mobile-specific pinning (`Platform.canPinSidebar`).

### `WorkspaceRibbon`

- **Kind:** plain class (NOT a `WorkspaceItem` subclass). One per side.
- **Constructor `new WorkspaceRibbon(workspace, "left"|"right")`:** creates `workspace-ribbon side-dock-ribbon mod-<side>` container; left side also gets `side-dock-actions` (plugin buttons) + `side-dock-settings` (settings cog).
- **Fields:** `items: RibbonItem[]`.
- **Methods:** `addRibbonItemButton(id, icon, title, cb)` (re-call same id updates), `removeRibbonAction(id)`, `setCollapsedState(bool)`, `hide`/`show`, `onChange(persist?)` (rebuilds visible children), `load({hiddenItems})` (applies hidden + order), `serialize() → {hiddenItems}`, `onContextMenu(event)` (per-item visibility menu + "Hide ribbon" toggling vault-config `showRibbon`). Reorder: drag-drop via `Gc` (utils sortable).

### Module-local constants

- `F0 = "workspace.json"` — the desktop layout filename.
- `N0 = "workspace-mobile.json"` — the mobile layout filename.
- `AD = 200` — minimum pane size in px (also the min-drag threshold for sidedock collapse).
- `PD = { duration: 140, fn: "var(--anim-motion-swing)" }` — the default sidedock animation descriptor.
- `UD = 200` — mobile-drawer open/close animation duration in ms.

---

## 2. Data structures

### `LayoutJson` (root of `.obsidian/workspace.json`)

```typescript
{
  main?: SplitNode;                 // root tab/split tree for the main window
  left?: SplitNode;                 // left WorkspaceSidedock tree
  right?: SplitNode;                // right WorkspaceSidedock tree
  floating?: FloatingNode;          // omitted when no popouts open
  'left-ribbon'?: { hiddenItems: Record<string, boolean> };  // ribbon visibility + order (key order = item order)
  active?: string;                  // active leaf id (16-char random token)
  lastOpenFiles?: string[];         // recentFileTracker — most-recent-first vault-relative paths
}
```

### `SplitNode` (any `WorkspaceItem.serialize()` value)

```typescript
type SplitNode =
  | { id; type: "split"; direction: "horizontal"|"vertical"; dimension?; children: SplitNode[];
      width?; collapsed?: true; }                            // sidedock variant: type still "split", + width + collapsed
  | { id; type: "tabs"; dimension?; children: SplitNode[];
      currentTab?: number;          // omitted when 0
      stacked?: true; }
  | { id; type: "leaf"; dimension?; state: ViewState;
      pinned?: true; group?: string; }
  | { id; type: "mobile-drawer"; width?; collapsed?: true;
      children: SplitNode[]; currentTab?; pinned?: true; }
  | { id; type: "window"; direction; children: SplitNode[];
      x?; y?; width?; height?;
      size?: {width, height, x, y};
      maximize?: boolean; zoom?: number; };
```

- `dimension`: flex-grow ratio in `(0, 100)`. Sibling `dimension`s must sum to 100 when all set; `null`/missing = flexible.
- `width`: absolute px (sidedock + window).
- `direction` on `WorkspaceContainer` (root/window) is always `"vertical"`; nested splits alternate.
- `children` ordering = visual left-to-right / top-to-bottom.
- `FloatingNode = { id, type: "floating", children: WindowNode[] }`. Non-window children are filtered out during deserialise.

### `ViewState` (leaf `state` payload)

```typescript
{
  type: string;                    // viewType key (registered via app.viewRegistry.registerView)
  state?: Record<string, unknown>; // view-specific (MarkdownView: {file, mode: "source"|"preview", source?, backlinks?})
  icon?: string;                   // cached icon for deferred-placeholder
  title?: string;                  // cached title for deferred-placeholder
  active?: boolean;                // runtime: triggers setActiveLeaf
}
```

- `state.file` for `FileView`-descended is vault-relative `/`-separated.
- `state.mode` for `MarkdownView` is `"source"` (live-preview is `source: false` sub-state) or `"preview"`.

### `UndoHistoryEntry` (leaf-close undo, cap 10, unshift+pop)

```typescript
{ leafId; state: ViewState; eState; parentId?; rootId?; leafHistory: { backHistory, forwardHistory } }
```

### `LeafHistoryState` (per-entry in `qD` back/forward stacks, back capped at 20)

```typescript
{ title; icon; state: ViewState; eState }
```

### `HoverLinkSource`

```typescript
{ display: string; defaultMod: boolean }    // defaultMod=true → preview needs Mod key; false → hover alone triggers
```

Keyed by id (`"search"`, `"preview"`, `"editor"`, `"tab-header"`, plus plugin-supplied).

### `RibbonItem`

```typescript
{ id; icon; title; callback: (event) => void; hidden: boolean; buttonEl?: HTMLElement }
```

### `ObsidianProtocolHandler`

```typescript
(args: { action: string; [key: string]: string }) => void
```

Stored in `Workspace.protocolHandlers: Map<string, ObsidianProtocolHandler>`. `args` is the query-string dict from `obsidian://<action>?…`.

---

## 3. On-disk contracts

Two files, both under `<configDir>` (default `.obsidian/`).

### `.obsidian/workspace.json` (or `workspace-mobile.json` on `Platform.isMobile`)

- **Path:** `<configDir>/workspace.json` (constant `F0`); `workspace-mobile.json` on mobile (`N0`).
- **Written by:** `saveLayout()` debounced 1 s via `requestSaveLayout`. Triggered from: split/duplicate (via `onLayoutChange` → `updateLayout`), `setPinned`, `setGroup`, `selectTabIndex`, `finishResize`, sidedock `expand/collapse/onSidedockResizeStart`, popout window resize, `WorkspaceRibbon.onChange(persist=true)`. Quit handler calls `requestSaveLayout.cancel()` and relies on save-on-every-mutation having caught up.
- **Read by:** `readWorkspaceFile()` → `loadLayout()`, once per vault open.
- **Format:** `JSON.stringify(obj, null, 2)`. **Schema:** `LayoutJson` (Section 2).
- **Lifecycle:** created on first save; absence is **valid** — read returns `{}`, `setLayout` falls back to a fresh `WorkspaceRoot("vertical") > WorkspaceTabs > empty WorkspaceLeaf` opening `lastOpenFiles[0]` if extant.
- **Migration:** no version field; unknown `type` values cause `deserializeLayout` to return `null` and the parent drops the node — future-version workspace.json silently degrades on older Obsidian. Desktop and mobile layouts persist to separate files so they don't interfere.
- **Sandbox-vault special-case:** `loadLayout()` detects "Obsidian Sandbox" vault (by name + `ipc("get-sandbox-vault-path")`), detaches every leaf in `rootSplit + floatingSplit`, opens "Start here" in a fresh leaf, shows a sandbox-banner Notice — overrides any persisted state.

### `.obsidian/workspaces.json` (named workspaces, **NOT this domain**)

Owned by the internal **Workspaces** plugin (`_internal.js`), which calls `workspace.changeLayout(json)` to switch. Likely shape: `Record<workspaceName, LayoutJson>` snapshots from `workspace.getLayout()`. Confirm in `plugin/` audit.

No other files in the vault are touched by this domain.

---

## 4. Events emitted

### `Workspace` (extends `Events`)

| Event name | Payload (inferred) | Triggered when | Typical consumers |
|---|---|---|---|
| `layout-ready` | `()` | First load completes; `layoutReady=true`. Fires **once**. Before, `onLayoutReady(cb)` queues; after, fires synchronously. `:1863`. | Plugins deferring init. |
| `layout-change` | `()` | Any layout mutation (insert/remove/pin/group, tab select, resize-end, sidedock expand/collapse, window open/close, ribbon reorder, stacked toggle). Debounced 10 ms. **Only when `layoutReady`**. `:27`. | Each `WorkspaceWindow` updates title; UI chrome. |
| `active-leaf-change` | `(leaf \| null)` | Debounced 0 ms after `activeLeaf` changes. **Single most-subscribed hook.** `:3080`. | Outline / backlinks / graph; "act on current file". |
| `file-open` | `(file \| null)` | Only when `getActiveFile()` differs from `lastActiveFile`; `null` when active leaf has no backing file. Fires **after** `active-leaf-change`. `:3086`. | Daily Notes, Templates, Dataview. |
| `quick-preview` | `(file, data: string)` | Editor-markdown calls `onQuickPreview` on every debounced edit; other panes' preview re-renders without save. `:3698`. | Other `MarkdownView`s on same file in preview mode. |
| `resize` | `()` | Debounced 0 after `window.resize` or any leaf `onResize`. `:3701`. | Views that measure container: graph, canvas, charts. |
| `window-frame-change` | `()` | After `updateFrameless()`, every `updateLayout`. Frame CSS classes may have shifted. `:3694`. | Frameless-titlebar component (`frameDom`). |
| `swipe` | `(gesture)` | Touch-handler on `containerEl`. `gesture = {direction:"x"\|"y", points, x, y, startX, startY, targetEl, registerCallback({move,cancel,finish})}`. `:1543`. | Mobile pull-down/drawer-open/trackpad. |
| `file-menu` | `(menu, file, source, leaf?)` | **Mid-construction:** `addSections` + built-ins added; plugins inject via `menu.addItem(...).setSection(...)`. `source` ∈ `"file-explorer-context-menu"`, `"link-context-menu"`, `"more-options"`, `"pane-more-options"`, `"sidebar-context-menu"`, `"tab-header"`. May fire multiple times — be idempotent. Built-in (`:349`) adds copy-path/URL/full-path, open-default-app, show-in-folder, move, open-in-new-window. | Tags, Templates, OmniSearch, Dataview, Workspaces. |
| `files-menu` | `(menu, files[])` | Multi-selection. **Only emitted by File Explorer internal plugin** (no emitter here); subscribed `:462` for "Move multiple". | Batch-file plugins. |
| `url-menu` | `(menu, href)` | After built-in "Copy URL / Open in default browser". `:3795`. | External-link plugins. |
| `editor-menu` | `(menu, editor, view)` | Editor-markdown emits after its items, **before** menu shown. Built-ins (`:540`, `:569`) add "Rename heading"/"block id". Emitter: `editor/markdown/MarkdownView.js:1296`. | Templates, Advanced Slides, Tasks. |
| `markdown-viewport-menu` | `(menu, view, mode, path?)` | Right-click on empty viewport; editor-markdown emits. Built-in (`:477`) adds Readable line length / Line numbers (source) / Inline title. | Whole-note toggle plugins. |
| `leaf-menu` | `(menu, leaf)` | Tab-header menu after `view.onTabMenu` + `view.onPaneMenu(source)`. `WorkspaceLeaf.js:1427`. | Per-leaf-action plugins. |
| `tab-group-menu` | `(menu, tabs)` | Tab-group dropdown after built-in stack/close-all, **before** per-tab list. `WorkspaceLeaf.js:117`. | Per-tab-group plugins (rare). |
| `hover-link` | `({event, source, hoverParent, targetEl, linktext, sourcePath?})` | Tab-header (`source:"tab-header"`, `WorkspaceLeaf.js:874`); editor/preview wikilink (`source:"editor"`/`"preview"`, from editor-markdown); search/bases/canvas. `source` MUST be `registerHoverLinkSource`-registered. | Page-preview internal plugin opens popover based on `source.defaultMod` vs current modifier. |
| `window-open` | `(window, win)` | `WorkspaceWindow` ctor after browser window opens. `WorkspaceFloating.js:298`. | Per-window save-on-quit tracking. |
| `window-close` | `(window, win)` | `WorkspaceWindow.close()` before nulling `win`. `WorkspaceFloating.js:376`. | Same. |
| `css-change` | `()` | **Not triggered here.** Emitter: `core/App.js` (theme/snippet). Subscribed `:1669` (iOS status-bar), `:1700` (macOS frameless redraw). | Theme-aware plugins. |
| `quit` | `({addPromise})` | **Not triggered here.** Emitter: `App` (BeforeUnload/quit IPC). Subscribed `:528` to flush saves. | App teardown. |
| `post-processor-change` | `()` | **Not triggered here.** Emitted by `plugin/Plugin.js:144/147/162/166` on every `register/unregisterMarkdownPostProcessor`. Consumed by `editor/markdown/MarkdownPreviewRenderer` to invalidate caches. Pass 1 attributed to Workspace; workspace-scoped but Workspace doesn't trigger. | All preview renderers. |

### `WorkspaceLeaf` (extends `WorkspaceItem` which extends `Events`)

| Event name | Payload | Triggered when | Typical consumers |
|---|---|---|---|
| `pinned-change` | `(pinned: boolean)` | `setPinned(bool)` changes the value. Cite: `workspace/WorkspaceLeaf.js:1214`. | UI code that wants to react to pin-state of a specific leaf. |
| `group-change` | `(group: string \| null)` | `setGroup(id)` changes the value. Cite: `workspace/WorkspaceLeaf.js:1254`. | Linked-pane propagation logic. |
| `history-change` | `()` | `qD.pushState` / `qD.deserialize` / `qD.updateState` complete. Cite: `workspace/WorkspaceLeaf.js:728`, `:754`, `:790`. | Tab-header back/forward arrow enablement. |

`WorkspaceItem`, `WorkspaceParent`, `WorkspaceSplit`, `WorkspaceSidedock`, `WorkspaceContainer`, `WorkspaceRoot`, `WorkspaceFloating`, `WorkspaceWindow`, `WorkspaceTabs`, `WorkspaceRibbon`: no events emitted.

---

## 5. Events consumed

| Listener | Subscribes to | Why |
|---|---|---|
| `Workspace.js:289` | `vault.on("rename", (file, oldPath))` | Rewrite `state.state.file` in every leaf's back/forward history + every `undoHistory` entry. Without this, "undo close tab" for renamed files would break. |
| `Workspace.js:347` | `viewRegistry.on("view-registered"\|"view-unregistered")` | Force-rebuild every leaf of the affected type (re-open with deferred placeholder, re-apply `setViewState`). Propagates `Plugin.registerView` to live leaves. |
| `Workspace.js:528` | `self.on("quit")` | Flush pending `requestSaveLayout` + vault `requestSaveConfig` + every dirty `TextFileView.save()` promise via `event.addPromise`. |
| `Workspace.js:349, 462, 477, 540, 569` | `self.on("file-menu"\|"files-menu"\|"markdown-viewport-menu"\|"editor-menu")` | Inject built-in items (Copy path, Copy URL, Move, Show in folder, Open in new window, Move multiple, Readable line length / Line numbers / Inline title, Rename heading, Rename block id). |
| `Workspace.js:1545, 1603` | `self.on("swipe")` ×2 | Mobile pull-down action + two-finger back/forward trackpad gesture. |
| `Workspace.js:1669, 1700` | `self.on("css-change")` (iOS, macOS) | iOS status-bar background sync; macOS frameless redraw across popouts. |
| `Workspace.js:537` | `xm.appStateChange` (Capacitor) | Save dirty text-file views on app background. |
| `WorkspaceFloating.js:279, 284` | `workspace.on("quit"\|"layout-change")` per popout | Close popout with app; update popout title on layout-change. |
| `WorkspaceLeaf.js:1589` | `workspace.on("swipe")` (mobile drawer) | Drawer expand/collapse swipe. |

---

## 6. Commands registered

All registered via `app.commands.addCommand` from the `Workspace` ctor. Hotkeys via `Fb(mods, key)` for platform-normalisation.

| ID | Display | Hotkey | Effect |
|---|---|---|---|
| `editor:save-file` | Save current file | `Mod+S` | `view.save()` if `TextFileView`. |
| `editor:download-attachments` | Download attachments | — | `fileManager.downloadAttachmentsForNote(activeFile)` for `.md`. |
| `editor:follow-link` | Follow link under cursor | `Alt+Enter` | `editMode.triggerClickableToken(token, false)`; else dispatch `"open-link"` CustomEvent. |
| `editor:open-link-in-new-leaf`/`-window`/`-split` | Open link in new tab/window/split | `Mod+Enter` / `Mod+Alt+Shift+Enter` / `Mod+Alt+Enter` | Same with target `"tab"`/`"window"`/`"split"` (gated `canPopoutWindow`/`canSplit`). |
| `editor:focus-{top,bottom,left,right}` | Focus tab in direction | — | `setActiveLeaf(getAdjacentLeafInDirection(...), {focus:true})`. |
| `workspace:split-vertical`/`-horizontal` | Split right/down | — | `duplicateLeaf(activeLeaf, "vertical"\|"horizontal")` (`canSplit` + ItemView + not sidebar). |
| `workspace:toggle-pin` | Toggle pin | — | `activeLeaf.togglePinned()`. |
| `workspace:toggle-stacked-tabs` | Toggle stacked tabs | — | `parent.setStacked(!isStacked)` (`canStackTabs` + main root). |
| `workspace:edit-file-title` | Rename file title | `F2` | `setEphemeralState({rename:"all"})` on `EditableFileView`. |
| `workspace:copy-path`/`-full-path`/`-url` | Copy path/abs-path/Obsidian URL | — | Clipboard. |
| `workspace:undo-close-pane` | Undo close tab | `Mod+Shift+T` | Shift `undoHistory`, recreate leaf in original `rootId`+`parentId`, restore state+eState+leafHistory. |
| `workspace:export-pdf` | Export to PDF | — | `view.printToPdf()` (`canExportPdf` + MarkdownView). |
| `editor:rename-heading` | Rename heading under cursor | — | Opens rename modal (`pI`). |
| `workspace:open-in-new-window`/`move-to-new-window` | Open/move to new window | — | `openPopoutLeaf().setViewState(...)` / `moveLeafToPopout(activeLeaf)`. |
| `workspace:next-tab`/`previous-tab` | Cycle tabs | `Ctrl+Tab` / `Meta+Shift+]` / `Ctrl+PageDown` (and mirrors) | Wrapping `selectTabIndex`. |
| `workspace:goto-tab-{1..8}`/`goto-last-tab` | Jump to Nth/last tab | `Mod+{1..8}` / `Mod+9` | Tab index N-1 / `length-1`. (Not on phone.) |
| `workspace:new-tab`/`new-window` | New tab/window | `Mod+T` / — | New leaf in most-recent tab group / popout. |
| `workspace:close` | Close current tab | `Mod+W` | Sidebar→retarget main-root; pinned-non-phone→unpin; else `detach()`. |
| `workspace:close-window` | Close window | `Mod+Shift+W` | `activeWindow.close()`. |
| `workspace:close-others`/`-tab-group`/`-others-tab-group` | Close other / tab group / others in group | — | Detach all non-pinned per scope. |
| `workspace:show-trash` | Show trash | — | `app.showInFolder(".trash")` (desktop + `trashOption === "local"`). |

---

## 7. Registries owned

### `Workspace.protocolHandlers` (`Map<string, ObsidianProtocolHandler>`)

`obsidian://<action>?…` handlers. Populated by `registerObsidianProtocolHandler(action, fn)` (throws on dup) / `unregisterObsidianProtocolHandler(action, fn?)`. Built-ins (ctor `:1238`–`:1505`): `open` (resolves linktext, opens via `getLeaf(paneType)`), `search` (forwards to global-search), `new` (`new?file=…&name=…&content=…&clipboard=1&append|prepend|overwrite=1&silent=1&paneType=…`, creates folder+file, writes, opens unless `silent`, honours `x-success`/`x-error`), plus `show-plugin`, `show-theme`, `show-release-notes`, `debug-info`, `publish-sites`, `sync-setup`, `vault-setup`, `hook-get-address`. Read by `registerUriHook`-installed `window.OBS_ACT` + Capacitor `appUrlOpen`. In-memory only.

### `Workspace.hoverLinkSources` (`Record<string, HoverLinkSource>`)

`{display, defaultMod}` per id. Populated by `registerHoverLinkSource/unregisterHoverLinkSource`. Built-ins (ctor `:1720`): `search`/`editor`/`tab-header` (defaultMod:true), `preview` (defaultMod:false). Read by Page-Preview internal plugin (looks up `source` on `hover-link`, decides preview based on `defaultMod` vs current modifier). In-memory only; per-source enable flags in Page-Preview plugin's `data.json`.

### `Workspace.editorExtensions` (`Extension[]`)

Flat list of CodeMirror 6 extensions. `register/unregisterEditorExtension(ext)` both call `updateOptions()` which force-reconfigures every `MarkdownView`'s CM `Compartment`. Read by every `MarkdownView.updateOptions()`.

### `Workspace.operatorFuncConfigs` (Bases-plugin record)

`register/unregisterOperatorFuncConfigs(id, config)`. Read by Bases internal plugin.

### `WorkspaceRibbon.items` (`RibbonItem[]` per side)

Populated by `addRibbonItemButton(id, icon, title, cb)` (wrapped by `Plugin.addRibbonIcon`). **Persistence:** order + `hidden` in `workspace.json` at `["left-ribbon"].hiddenItems` (record). **Key order in `hiddenItems` IS runtime item-order** — `load()` sorts items by `Object.keys(hiddenItems).indexOf(id)`. Items without `buttonEl` (plugins not yet loaded) survive in persisted list and render once registered.

### `Workspace.undoHistory` (`UndoHistoryEntry[]`, cap 10)

Recently-closed leaf snapshots. `pushUndoHistory(leaf, parentId, rootId)` from `WorkspaceLeaf.detach` unshift+pop. Read by `workspace:undo-close-pane`. In-memory only; lost on vault close.

---

## 8. Invariants

- `WorkspaceItem.id` is 16-char random (`cc(16)`); persists across save/load.
- `activeLeaf` is `null` only before `setLayout` (or transiently in `clearLayout`); after `layoutReady`, always attached to one of the four splits.
- `layoutReady` is `false` during `clearLayout`/`setLayout`. **No events fire while false** — `trigger("layout-change")` dropped; `active-leaf-change`/`file-open` gated in `activeLeafEvents`. Corbomite must preserve to avoid cascades during vault switch.
- `setActiveLeaf(leaf, {focus:true})` with `leaf === activeLeaf` re-focuses but does NOT re-fire `active-leaf-change`.
- `getActiveFile()` prefers `activeEditor.file` over `activeLeaf.view.file` — plugins claim "current editor" by setting `activeEditor`. Setter **refuses to store a `MarkdownView`** (no-op); getter still returns one via fallback. Plugins cannot shadow the real MarkdownView.
- `WorkspaceLeaf.setViewState` single-flight via `working: boolean`; re-entrant calls return immediately.
- `WorkspaceLeaf.openFile(file, opts?)` normalises `opts.active` to `this === activeLeaf` when unspecified — current pane preserves focus, new pane doesn't.
- Linked-pane groups: pin on one member pins every member; `setGroup` joining a pinned group forces the new member pinned.
- `WorkspaceParent` with `allowSingleChild === false` (default) **dissolves** at 1 child — remaining child promoted up inheriting `dimension`. `WorkspaceContainer` (root/window) and `WorkspaceTabs` have `allowSingleChild = true` and never dissolve.
- Root `WorkspaceSplit`/`Root`/`Window` always `direction = "vertical"` after layout restore — `setLayout` rewrites otherwise.
- `undoHistory` is unshift+pop cap 10. `qD.backHistory` cap 20.
- `onLayoutReady(cb)` after `layoutReady === true` fires `cb` **synchronously**; callback list nulled after initial drain.
- `getLeaf("tab")` with `vault.getConfig("focusNewTab") === false` does NOT focus the new leaf — only way to open a new tab without stealing focus.
- `registerObsidianProtocolHandler` THROWS on duplicate; never overwrites. Plugins must `unregister` first.
- "In sidebar" = `leaf.getRoot()` is `leftSplit`/`rightSplit` (NOT `leaf.parent`).
- `requestSaveLayout` debounced 1 s trailing. `quit` cancels then relies on every mutator having called.
- Tab-group stacked mode persists through single-tab dissolve via `lastTabGroupStacked` capture in `removeChild`.
- `WorkspaceTabs.selectTabIndex` clamps to `[0, length-1]`. `currentTab: 0` and `undefined` equivalent in JSON (default 0).
- On vault `rename`: every leaf's back/forward history + every undo entry's `state.state.file` rewritten in place; `title` updated to new basename. Transactional inside the `vault.rename` handler before other subscribers.

---

## 9. Observable user features

- Split any tab horizontally/vertically (command palette or drag tab-header to pane edge); convert tab group to **stacked mode** (all tabs side-by-side with scrolling); drag tab to reorder or into sidedock to convert to sidebar panel.
- Pin a tab (never recycled by `getLeaf()`); link-group tabs so navigating one navigates all members.
- Pop tab into new OS window (desktop), duplicating or moving; drag tabs between main window and popouts.
- Collapse sidedocks via toggle buttons; resizable 200 px min to 80% workspace; snap-collapse below 50 px.
- Right-click `[[wikilink]]` for "Open link / new tab / right / Rename" + plugin items (source `"link-context-menu"`).
- Right-click tab header for tab-menu (close, close others, close tab group, pin, pane actions, system items).
- `Mod+Shift+T` undo-close last 10 closed tabs (restored to original container + tab group if live).
- Mobile swipe-down → "mobile pull action" command (vibrates at commit); two-finger horizontal trackpad → back/forward.
- `Mod+1..8`/`Mod+9` jump to Nth/last tab; `Ctrl+Tab`/`Ctrl+Shift+Tab` cycle tabs.
- `Alt+Enter` follow link; `Mod+Enter`/`Mod+Alt+Enter`/`Mod+Alt+Shift+Enter` for new tab/split/window.
- Layout autosaved 1 s after last change. Reopen vault → exact split/tab/pin/group/stacked/sidedock/ribbon/popout state restored.
- `obsidian://open?vault=…&file=…` opens notes from other apps (mobile + desktop).
- Drag ribbon buttons to reorder; right-click ribbon to hide individual buttons or whole ribbon.
- Hover tab header / search result / editor wikilink / preview wikilink (per-source modifier) for live content popover.

---

## 10. Extension surfaces exposed

| Surface | Registration verb | Consumer | Plugins supply |
|---|---|---|---|
| Workspace events (primary plugin hook) | `workspace.on(name, cb)` | `Events` dispatch — `Workspace.js:3895` | listener `(…payload) => void` — Section 4 for shapes |
| Ribbon button | `Plugin.addRibbonIcon(icon, title, cb)` → `leftRibbon.addRibbonItemButton(id, icon, title, cb)` | `WorkspaceFloating.js:51` | `{icon, title, callback: (event) => void}` |
| CodeMirror 6 extension | `Plugin.registerEditorExtension(ext)` → `registerEditorExtension(ext)` | `Workspace.js:3889` | CM6 `Extension` value |
| Obsidian URL handler | `Plugin.registerObsidianProtocolHandler(action, fn)` → `registerObsidianProtocolHandler(action, fn)` | `Workspace.js:3839` | `(args: {action, [k: string]: string}) => void`; throws on duplicate |
| Hover-link source | `Plugin.registerHoverLinkSource(id, info)` → `registerHoverLinkSource(id, info)` | `Workspace.js:3850` | `{display, defaultMod}` |
| Operator-function config (Bases) | `Plugin.registerOperatorFuncConfigs(id, config)` → `registerOperatorFuncConfigs(id, config)` | `Workspace.js:3856` | Bases-specific record |
| Mid-construction menu injection | `workspace.on("file-menu"\|"url-menu"\|"editor-menu"\|"files-menu"\|"leaf-menu"\|"tab-group-menu"\|"markdown-viewport-menu", cb)` | Section 4 emit sites | `(menu, …ctx) => menu.addItem(i => i.setSection(...).setTitle(...).onClick(...))` |
| Leaf-close undo | inherited via `Plugin.registerView` + `workspace:undo-close-pane` | `Workspace.js:876` | — |
| Layout-saved hook | `workspace.on("layout-change"\|"layout-ready")` | `Workspace.js:27, 1863` | — |

**Menu `addSections` ordering.** Section order in the `addSections([...])` call determines item placement. Section conventions:
- File-menu: `["title", "open", "action-primary", "action", "info", "info.copy", "view", "system", "", "danger"]`.
- Tab-header menu: `["title", "close", "pane", "open", "action", "find", "info", "info.copy", "view", "view.linked", "system", "", "danger"]`.
- Tab-group menu: `["action", "close", "", "tablist"]`.
- Small inline menus: `["title", "open", "action", "", "danger"]`.
- `""` = "uncategorised" bucket between named sections and `"danger"`. Plugin items without `setSection` land here.
- Within a section, items render in `addItem` insertion order: **built-ins first** (added before `trigger`), **then plugin items** (added by `file-menu`/`url-menu`/`editor-menu` subscribers).
- `setSectionSubmenu(id, {title, icon})` collapses a section into a nested submenu (used for `info.copy` and `view.linked`).

---

## 11. Corbomite mapping

| Obsidian concept | Corbomite | Status | Notes |
|---|---|---|---|
| `Workspace` class + events + active-leaf plumbing | scattered: `EditorViewManager` (splits), `MainWindow` (chrome), `SessionManager` (persistence) | **Missing major piece** | Need single `Corbomite::Workspace` sibling to `VaultService` with Qt signals named after JS events (`layoutReady`, `layoutChanged`, `activeLeafChanged`, `fileOpened`, `quickPreview`, `windowFrameChanged`, `fileMenuRequested(QMenu*, NoteMeta*, QString, WorkspaceLeaf*)`, `urlMenuRequested`, `editorMenuRequested`, `leafMenuRequested`, `tabGroupMenuRequested`, `markdownViewportMenuRequested`, `hoverLink`, `windowOpened/Closed`, `swiped`, `cssChange`, `postProcessorChange`). |
| `WorkspaceItem`→`Parent`→`Split`/`Tabs`/`Leaf` | QSplitter + bespoke tab widget + `EditorViewSpace`/`NoteEditorWidget` | Partial | No explicit `id`/`dimension` bookkeeping (sizes live in QSplitter). Add 16-char `QUuid` per node so layout restore + undo-close-tab can target specific containers. |
| `WorkspaceSidedock`/`Ribbon` | `src/sidebar/*Panel.{h,cpp}` | Partial | No per-sidedock size persistence, expand/collapse animation, drag-tab-into-sidedock, ribbon drag-reorder. Left-dock width+collapse+panel-order+hidden-items missing from SessionManager. |
| `WorkspaceContainer`/`Root` | `MainWindow` | Partial | One main window; no popout support. |
| `WorkspaceWindow` (popout) | — | **Missing** | `QMainWindow` per popout with its own `EditorViewManager`, cross-window drag-drop, per-window persisted geometry/maximize/zoom. |
| `WorkspaceLeaf.history` (per-leaf back/forward) | QTextEdit document undo only | Partial | Add `Corbomite::LeafHistory` (push/back/forward/go/serialize/deserialize, cap 20, file-rename rewriting). |
| `Workspace.undoHistory` (leaf-close undo) | — | **Missing** | 10-entry stack for `Ctrl+Shift+T`. |
| `.obsidian/workspace.json`/`workspace-mobile.json` | `SessionManager::buildSplitLayoutJson` → `~/.local/share/corbomite[-dev]/<vault>/session.json` | Partial | **Hard compat block.** Wrong directory + wrong field names (`split`+`sizes`+`orientation` vs `split`/`tabs`/`leaf`+`direction`+`dimension`+`children`). Must read/write `<vault>/.obsidian/workspace.json` with Section 2 schema. |
| `getLeaf(mode?, dir?)` | `EditorViewManager::openNote`+`splitActive*` | Partial | Add `enum OpenMode { Same, Tab, Split, Window }` and route through `openInLeaf(doc, mode)`. |
| `openLinkText(linktext, source, mode)` | Split across `NoteEditorWidget` signal + `openNote` | Partial | Single entry: resolve via `SQLiteIndex::getFirstLinkpathDest`, create via `FileManager` if unresolved, open in mode. |
| `setActiveLeaf` + debounce-0 events | `EditorViewManager::activeEditor` | Partial | Tracking exists; debounce-0 + file-changed-only filtering missing. |
| `iterateAllLeaves`, `getLeavesOfType`, `getActiveViewOfType` | — | Missing | Required by countless plugin patterns. |
| `registerEditorExtension` | — | **N/A** | Corbomite uses Qt not CodeMirror; analogous Markoff extension registry future. |
| `registerObsidianProtocolHandler` | — | **Missing** | `KDBusService(Unique)` + `corbomite://`/`obsidian://` URLs. Map built-ins 1:1 (`open`, `new`, `search`, etc.). |
| `registerHoverLinkSource` | — | **Missing** | Plus `HoverPreview` widget; `Workspace::registerHoverLinkSource(QString id, HoverLinkSource)`. |
| `file/url/editor-menu` mid-construction injection | Direct emit, no plugin hook | **Missing** | `Workspace::emit*MenuRequested(QMenu*, …)` signals; adopt Section 10 sections ordering. |
| Ribbon | Toolbar in MainWindow | Partial | No plugin slot, drag-reorder, hide-per-item, persistence. |
| Tab pinning + linked-pane group | — | **Missing** | Per-tab pin + propagation. |
| `onLayoutReady(cb)` | `vaultOpened()` lacks queue+drain | Partial | Add the queued-pre-ready, sleep(0)-yield-between-callbacks semantics. |

**Immediate Corbomite TODOs (priority order):**

1. **`Corbomite::Workspace`** (new `libs/workspace/` or `src/app/`) with Qt signals named after JS events. Owns Section 7 registries.
2. **`.obsidian/workspace.json` reader+writer** matching Section 2 schema exactly; write to `<vault>/.obsidian/`, not `~/.local/share/`.
3. **`Corbomite::LeafHistory`** (back/forward/go, cap 20, file-rename rewriting).
4. **Leaf-close undo** (10 entries, `workspace:undo-close-pane`, `Ctrl+Shift+T`).
5. **`file-menu`/`editor-menu` signals** — emit from right-click handlers; plugins inject via `QMenu::addAction`.
6. **Ribbon plugin API** — `RibbonBar::addButton(id, icon, title, cb)` + drag-reorder + hide-per-item; persisted at `["left-ribbon"].hiddenItems`.
7. Block before plugin-system work: the four registries + menu signals are the full extension surface; every plugin uses at least one.

---

## 12. Markoff gap confirmations / discoveries

This domain plumbs menus, commands, and history through to the editor but doesn't own it.

- **Confirm Pass-1:** "Undo history owned by Workspace". Confirmed `:34, :876, :3865`. Missing in Corbomite — add `LeafCloseUndoStack` (cap 10) to `EditorViewManager`, `Ctrl+Shift+T`, restore in original `rootId`+`parentId`.
- **New:** `editorExtensions` is **flat-mutable, shared across every `MarkdownView`** — `registerEditorExtension` triggers `updateOptions` reconfiguring every leaf's CM `Compartment`. Markoff's analogous system must use the same "one list applied to every editor instance" semantics — plugins can't accidentally scope to one leaf.
- **New:** Editor-menu mid-construction is the primary plugin surface for in-editor actions. Emitter in `editor/markdown/MarkdownView.js` + `MarkdownPreviewView.js`. Markoff needs `emitEditorMenu(QMenu*, Editor*, MarkdownView*)` with sections `"selection"`, `"action"`, `"open"`, `"view"`, `"info"`, `"system"` populated before emit.
- **New:** `quick-preview(file, unsavedContent)` is per-keystroke debounced content-sync across panes — two panes on same file (source + preview), preview re-renders without waiting for save. Markoff must implement if same-file-two-leaves is supported.
- **Confirm Pass-1:** "Drag handles on list/heading blocks". Leaf-level events are here; gutter UX in `editor/markdown/`. Not a workspace gap.
- **New:** `editor:rename-heading` / `workspace:rename-blockid` modals (`pI`/`dI`) are in editor-markdown. Confirmed gap in Corbomite.
- **New:** Hover-previews need 4 pieces: `registerHoverLinkSource` + `trigger("hover-link")` + Page-Preview plugin subscription + `HoverPopover` widget. Markoff's hover is not end-to-end. Minimum Corbomite scope: registry + event so Markoff can emit; popover later.

Appended to `01-markoff-gaps.md` under `## Pass 2 additions — workspace`.

---

## 13. Open questions

1. **`B0` (recent-file tracker) location.** Used as `recentFileTracker = new B0(workspace, vault)`. Exposes `.load(files)`, `.serialize()`, `.onFileOpen(new, old)`, `.getLastOpenFiles()`, `.getRecentFiles(limit)`, `.addRecentFile(file)`. Not in this domain — likely `vault/` or `core/`. Corbomite needs a `RecentFileTracker` with identical serialisation (the `lastOpenFiles` field in workspace.json).
2. **`eD` / `tD` / `nD` classes.** `tD` = empty-view placeholder (`_empty`); `eD` = deferred-view placeholder (used by `setViewState` when icon+title cached); `nD` = unknown-view placeholder (triggers `mod-unknown` CSS). All in `views/`. Corbomite must implement equivalent placeholder views.
3. **`Menu` section-id ordering rule.** Items within a section render in insertion order — but is this documented? Plugin items added by `file-menu` subscribers always land *after* built-ins because `addItem` is called *after* `trigger` returns. Confirm in `ui/menu/Menu.js`.
4. **Ribbon `hiddenItems` post-load order.** `WorkspaceRibbon.load` sorts by `Object.keys(hiddenItems).indexOf(id)`; plugins registered *after* layout-load have `indexOf === -1` and append at the end. Do plugins need to call `onChange(true)` after first render to persist their position?
5. **`workspaces.json` (named workspaces) schema.** Pass 1 claimed both files are this domain's; in 1.12.7, `workspaces.json` is owned by the internal Workspaces plugin. Confirm in `plugin/` audit. Likely `Record<workspaceName, LayoutJson>`.
6. **`tab-group-menu` ordering.** Built-in "stack/close all" items added → emit → per-tab list appended. Plugin items land between built-ins and the per-tab list. Intentional? Probably yes.
7. **`hover-link` payload variants.** Tab-header variant `{event, source, hoverParent, targetEl, linktext}`; preview/editor variants from editor-markdown likely add `sourcePath`. Full shape requires opening editor-markdown — flagged for that audit.
8. **Sandbox-vault detach is destructive** to any persisted layout in the sandbox vault. Each sandbox open resets to "Start here". Probably intentional.
9. **`operatorFuncConfigs` shape.** Bases-plugin owned. Confirm in `bases/` audit.
10. **`mobileFileInfos: []`.** Initialised + appended via `addMobileFileInfo` but never read inside this domain. Mobile-toolbar cross-reference needed.
11. **`Rg` (Scope class) parent-delegate semantics.** Does parent scope delegate every unhandled key or predicate-filtered? Defined in `core/Scope.js`.

---

## 14. Recommended Pass 3 synthesis input

1. **`.obsidian/workspace.json` is the single most compat-critical artefact.** Section 2's `LayoutJson` (5 node variants split/tabs/leaf/floating/window + ribbon block + `lastOpenFiles` + `active`) is what Obsidian users expect. Corbomite writes a different shape to a different directory. Promote the schema verbatim into `VAULT-FORMAT.md`; mark "workspace.json read/write" as a blocker for "open Obsidian vault, layout restored" in `GAP-ANALYSIS.md`.
2. **The 17 Workspace events + 3 WorkspaceLeaf events are the second plugin-facing primary API (after Vault).** `FEATURE-MATRIX.md` records the exact event names + payloads from Section 4 as canonical. Corbomite Qt signals named after the JS event names; menu events emit `QMenu*` for mid-construction injection.
3. **Four plugin registries (`protocolHandlers`, `hoverLinkSources`, `editorExtensions`, `operatorFuncConfigs`) + ribbon surface + 7 menu signals** are the full extension surface here. Pass 3 plugin-API gap list names all by their Obsidian verbs (`registerObsidianProtocolHandler` etc.) so future Corbomite plugin API is a literal translation.

---

## 15. Cross-domain references

| Other domain | Reference type | Brief description |
|---|---|---|
| `vault` | consumer | Subscribes to `vault.on("rename")`; reads `vault.getConfig("focusNewTab"\|"trashOption"\|"uriCallbacks"\|"mobilePullAction"\|"showRibbon"\|"slidingSidebar")`; uses `vault.adapter.read/write(configDir+"/workspace[-mobile].json")`; uses `TFile`/`TFolder`/`FileSystemAdapter` in menus + protocol handlers. |
| `metadata` | consumer | `metadataCache.getFirstLinkpathDest(path, sourcePath)` for `openLinkText` and the `open` protocol handler. |
| `core` | consumer | Uses `App.commands` (every command), `App.keymap`, `App.scope`, `App.viewRegistry` (deferred views + `view-registered`/`view-unregistered`), `App.internalPlugins`, `App.hotkeyManager` (via `Fb`), `App.setting`, `App.dragManager`, `App.plugins.loadingPluginId`, `App.showInFolder`/`openWithDefaultApp`/`copyObsidianUrl`/`showReleaseNotes`/`getObsidianUrl`/`openVaultChooser`. Uses `Scope`/`Rg` from `core/Scope.js`, `Events` base class, `Notice`. |
| `views` | dependency | `WorkspaceLeaf` instantiates `tD` (empty view); `instanceof` checks against `View`/`FileView`/`ItemView`/`EditableFileView`/`TextFileView`/`MarkdownView`. The first ~607 lines of each Group-A file are `FileView` + vault-chooser-modal bleed-over from `views/`. |
| `editor/markdown` | dependency/emitter | Emits `editor-menu`, `markdown-viewport-menu`, `markdown-scroll`, `hover-link`, `post-processor-change` (triggered from `plugin/`). `MarkdownView`/`MarkdownPreviewView` own those emit sites. |
| `plugin` | dependency | `post-processor-change` triggered from `plugin/Plugin.js`. `Plugin.register*` verbs wrap this domain's `register*` methods. |
| `ui/menu` | dependency | `Menu`/`Menu.forEvent`/`addSections`/`setSectionSubmenu`/`setParentElement`. |
| `ui/popups` | dependency | `Notice`, `HoverPopover`. |
| `platform/Keymap` | consumer | `Keymap.isModEvent(event)`. |
| `parsing` | consumer | `parseLinktext(linktext)` from `vault/parseLinktext.js`. |
| `bases` | consumer | `operatorFuncConfigs` registry read by Bases plugin. |
| `canvas` | sibling | Canvas file-menu items + canvas drag-drop wired through workspace events. |
| `utils` | consumer | `debounce`, `cc(16)` (id), `Cc` (idle), `Vm` (swipe), `Sv` (tooltip), `Lv` (keyboard-blur), `Iv` (scroll-into-view), `Gc` (sortable), `yl`/`dl`/`vl` (animations), `Ak`/`Bk`/`Fk` (colour), `ry` (URL), `Ec` (clipboard), `gc` (ellipsis), `iu`/`nu`/`su`/`pu` (path), `jg` (history), `Fb` (hotkey). |
| `platform/capacitor` | consumer | `xm`, `Em` for mobile URL open + state-change + back-button. |
| `vendor/codemirror` | monkey-patch | `CodeMirror.getMode = …` hot-patched in Workspace ctor for lazy mode loading. |

**Short symbols referenced from other domains:**

| Symbol | Defined in | Used for |
|---|---|---|
| `B0` | `vault/` or `core/` (not in this domain) | `recentFileTracker` instance — `.load(files)`, `.serialize()`, `.onFileOpen(new, old)`, `.getLastOpenFiles()`, `.getRecentFiles(n)`, `.addRecentFile(file)`. |
| `R0` | `core/` (not in this domain) | Pane-type normaliser translating `paneType: "tab"\|"split"\|"window"` to `getLeaf`-compatible mode in protocol handlers. |
| `tD` / `eD` / `nD` | `views/` | Empty / deferred / unknown view placeholders. |
| `f0` | `editor/EditorSuggest` (likely) | `Workspace.editorSuggest` global suggest instance. |
| `MarkdownView` | `editor/markdown` | `instanceof` in commands + `getActiveViewOfType` lookups + quick-preview routing. |
| `FileView`/`ItemView`/`EditableFileView`/`TextFileView` | `views/` | `instanceof` in commands (save, split, rename-title, export-pdf). |
| `Menu` | `ui/menu` | Every menu construction. |
| `Notice` | `ui/popups` | Status notifications on protocol-handler failures + sandbox banner. |
| `TFile`/`TFolder`/`TAbstractFile`/`FileSystemAdapter` | `vault` | `instanceof` in file-menu handler + `copy-full-path` + `show-trash` commands. |
| `parseLinktext`/`normalizePath` | `vault/parseLinktext`/`vault/normalizePath` | Linktext → `{path, subpath}` + path normalisation in protocol handlers. |
| `Events` | `core` | Base for `Workspace` + `WorkspaceItem`. |
| `Component` | `ui/components/Component` | `new Component()` per `WorkspaceItem` for lifecycle-scoped event registration. |
| `Scope` (`Rg`) | `core/Scope` | Keymap scope with dynamic child-scope lookup. |
| `Platform` | `platform` | `.isDesktopApp`/`.isMobile`/`.isPhone`/`.isIosApp`/`.isLinux`/`.isMacOS`/`.isTablet`/`.canPopoutWindow`/`.canSplit`/`.canStackTabs`/`.canExportPdf`/`.canPinSidebar`/`.mobileSoftKeyboardVisible`. |

The bleed-over content (`FileView`, `SD`/`xD`/`TD` vault-chooser modals, `AD`/`PD` animation constants in the top of every Group-A file) belongs to `views/` and `ui/popups/` and is intentionally not audited here per TEMPLATE rule 2.
