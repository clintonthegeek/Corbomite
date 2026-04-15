# `obsidian/views` — View / ItemView / FileView / TextFileView hierarchy + ViewRegistry

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/views/`
**File count:** 6
**Files:** `EditableFileView.js`, `FileView.js`, `ItemView.js`, `TextFileView.js`, `View.js`, `ViewRegistry.js`

**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> The view-class hierarchy mounted into `WorkspaceLeaf`s. `View` is the abstract base (lifecycle, container, `getViewType`, `getDisplayText`, `getIcon`). `ItemView` adds an action-bar (header buttons). `FileView` is a `View` bound to a `TFile` (with breadcrumbs, file-menu integration, navigation). `EditableFileView` adds inline-rename. `TextFileView` adds debounced auto-save (`requestSave` → 2s debounce → `save`) for text-content views. `ViewRegistry` maps viewType↔factory and extension↔viewType; emits `view-registered`/`view-unregistered`.

**De-minifier artifact note:** `View.js` and `ItemView.js` are byte-identical except for line 2's `// public API symbol:` comment (same declared source range `app.js` 61287–62329, same md5-minus-one-comment). Treat `View.js` as canonical for *both* the `View` and `ItemView` class pair; `ItemView.js` was not audited separately. In both files, lines 456–1047 declare `Qg` (platform-key map) and `Xg` (macOS native menu-bar manager) — adjacent leftover within the declared window, **not** audited here. `FileView.js` in-scope body is lines 5–343; the rest is vault-switcher modal (`SD`, `xD`, `TD`), `DD` vault-profile strip, and `WorkspaceItem`/`WorkspaceSplit`/`WorkspaceSidedock`/`WorkspaceContainer` (workspace domain). `TextFileView.js` in-scope body is lines 5–169; trailing `HX` is `BasesView` (bases domain). `EditableFileView.js` (249 lines) and `ViewRegistry.js` lines 5–104 are in-range and clean; `ViewRegistry.js:105+` is a footnote suggester and community-plugin browser (out of domain). The `eD`/`tD`/`nD` classes the `vault.md` note flagged as "`View`-derived empty-state panes" **are** view-domain primitives (deferred-load stub, "empty/new tab" pane, "unknown view-type" fallback) that were physically bundled into `vault/Vault.js:1118–1520` by the extractor; documented here because every Corbomite WorkspaceLeaf-equivalent must handle the same three corner cases.

---

## 1. Public API surface

Six public symbols plus three near-public internals (`eD`, `tD`, `nD`) documented for completeness. Declaration order below follows the inheritance chain rather than file order.

### `View`

- **Kind:** class, extends `Component` (from `ui/components`).
- **Constructor:** `new View(leaf: WorkspaceLeaf)`. Sets `app = leaf.app`, `leaf`, `containerEl = leaf.containerEl.createDiv('workspace-leaf-content')` (carries `data-type="<viewType>"`), `icon = 'lucide-file'`, `navigation = false`.
- **Purpose:** Abstract base for anything mounted inside a `WorkspaceLeaf`. Owns the DOM root, lifecycle hooks, and state/ephemeral-state contract that `Workspace.openLinkText`, session restore, and `setViewState` depend on.
- **Required overrides:** `getViewType(): string`, `getDisplayText(): string`, `getIcon(): string` (defaults to `this.icon || 'lucide-file'`), `onOpen()`/`onClose(): Promise<void>`.
- **Lifecycle flow:** `WorkspaceLeaf` calls `view.open(parentEl)` → `parentEl.appendChild(containerEl)` → `this.load()` (Component.load, fires `onload()`) → `await onOpen()`. `view.close()`: `containerEl.detach()` → `this.unload()` → `await onClose()`. **At `onOpen` time the container is in the DOM and `onload` has fired — subscriptions wired in `onload` are live.** Convention: `onload` for subscriptions, `onOpen` for async DOM fill.
- **State:** `getState() / setState(state, result): Promise<void>` — persistent, serialised to `workspace.json`. `getEphemeralState() / setEphemeralState(state)` — transient, carries scroll position, search match, rename mode. `result` is a mutable `{history, layout, close}` out-param (see Section 2). Defaults all no-op / `{}`.
- **Context menu hooks:** `onPaneMenu(menu, source)`, `onHeaderMenu(menu)`, `onTabMenu(menu)` — the last has a default implementation that adds Close / Close Others / Close After / Close All keyed off the parent tab-group, excluding pinned tabs.
- **Other:** `onResize()`, `handleCut/Copy/Paste(e)` (all no-op defaults), `getSideTooltipPlacement()` for sidedock chrome.
- **Mixes in:** `Component` — `registerEvent`, `registerDomEvent`, `registerInterval`, `addChild`, `load`/`unload` inherited with auto-cleanup on unload.

### `ItemView`

- **Kind:** class, extends `View`.
- **Constructor:** same signature as `View`. Decorates `containerEl` with `headerEl` (`.view-header`, containing `view-header-left` with back/forward nav buttons and — on mobile — a sidebar-toggle that vibrates 100 ms, plus `titleContainerEl` wrapping `titleParentEl`+`titleEl`, plus `actionsEl` for the right-side icon strip) and `contentEl` (`.view-content`). Auto-adds the `lucide-more-vertical` "…" button.
- **Purpose:** The subclass for any mounted view that wants header chrome with title, navigation, and action buttons. Most plugin views extend it; also the base for `FileView`.
- **Added API:**
  - `addAction(icon, title, callback): HTMLElement` — prepends `.clickable-icon.view-action` to `actionsEl`. Left- and middle-click fire `callback`; mousedown is `preventDefault`'d.
  - `onMoreOptions(event)` — builds a `Menu` with the canonical section order `['close','pane','open','action','find','info','info.copy','view','view.linked','system','','danger']`, wires `info.copy` (Copy Path) and `view.linked` (Open linked view) as submenus, calls `onPaneMenu(menu, 'more-options')` then `onMoreOptionsMenu(menu)`, and triggers `workspace.trigger('leaf-menu', menu, leaf)`.
  - `onMoreOptionsMenu(menu)` — **primary subclass hook for the "…" menu**.
  - `onGroupChange()` — reacts to `leaf.on('group-change')` for stacked-leaf sync.
  - `handleDrop(event, draggable, dragOver)` — delegates to `leaf.handleDrop`; accepts drops onto the header unless `canDropAnywhere = true`.
- **Lifecycle:** `load()` registers `leaf.on('group-change', ...)` and `leaf.on('history-change', updateNavButtons, ...)`. Nav-button `ariaDisabled` updates on every `history-change`.

### `FileView`

- **Kind:** class, extends `ItemView`.
- **Constructor:** sets `allowNoFile = false`, `navigation = true`. Current file in `this.file: TFile | null`.
- **Purpose:** Any view "about" one `TFile`. Handles breadcrumbs, rename/delete reaction, pane-group sync, and the Vault↔View binding.
- **Key added API:**
  - `onLoadFile(file): Promise<void>` — **subclass override** (MarkdownView reads content, ImageView sets `<img src>`, …). Called *after* `this.file` is set.
  - `onUnloadFile(file): Promise<void>` — save/flush before the file reference is cleared.
  - `loadFile(file | null)` — orchestrates unload-current → null `this.file` → try `onLoadFile(file)`; on throw, reverts to `null` and shows `msgFailedToLoadFile` Notice. On success, calls `renderBreadcrumbs()`, updates `titleEl`, requests active-leaf events. Returns `true` if swapped.
  - `canAcceptExtension(ext): boolean` — defaults `false`. When `true`, `WorkspaceLeaf.openFile` routes that extension into *this* view rather than instantiating a new one.
  - `renderBreadcrumbs()` — rebuilds `titleParentEl` from parent folder path. Each breadcrumb is clickable (reveals in file-explorer internal plugin), context-menuable (fires `workspace.trigger('file-menu', menu, folder, 'file-explorer-context-menu')`), and draggable (folder drag-source).
  - `getDisplayText()` — defaults `file.basename || i18n('noFile')`.
  - `getState()` / `setState(state, result)` — serialises `{file: file.path}`. On `setState`, resolves `vault.getAbstractFileByPath(state.file)` → `loadFile(…)`. If file missing and `!allowNoFile`, sets `result.close = true`. On actual swap, flags `result.layout = result.history = true` and attaches `result.done = () => syncState()`.
  - `syncState() / receiveSyncState(peer)` — stacked-group sync: iterate peer FileViews in same group, call `receiveSyncState`; default impl opens peer's file if different.
- **Vault reactions (subscribed in `onload`):**
  - `vault.on('rename', onRename)` — if our file: re-render breadcrumbs, update `titleEl`, `workspace.onLayoutChange()`, `leaf.updateHeader()`. Content is not re-fetched (the TFile reference stays valid).
  - `vault.on('delete', onDelete)` — `allowNoFile` → `loadFile(null)`; else step back in history if possible, else `leaf.open(null)`. If the resulting view is `tD` (empty state) and the tab-group has siblings, `detach()` the leaf.
- **Close:** `onClose()` → `contentEl.empty()` → `await loadFile(null)`.

### `EditableFileView`

- **Kind:** class, extends `FileView`.
- **Purpose:** `FileView` + inline-rename on `titleEl`. Every user-editable file view should inherit from this.
- **Instance fields:** `fileBeingRenamed: TFile | null` (in-flight rename target; prevents focus-loss races).
- **Lifecycle additions (`onOpen`):** flips `titleEl.contentEditable = 'true'` (mobile: only on first `touchstart`, so tab-click doesn't summon the keyboard); wires focus/blur/input/paste/keydown listeners.
- **Title handlers:**
  - `onTitleFocus` — snapshots `fileBeingRenamed = this.file`, enables spellcheck from `vault.getConfig('spellcheck')`, fades the breadcrumb prefix out (150 ms via `dl` animator).
  - `onTitleBlur` — calls `saveTitle(titleEl)` which validates via `LX(app, file, newName, isFinal)`, computes `file.getNewPathAfterRename(basename)`, and delegates to `app.fileManager.renameFile`. On validation fail shows an inline tooltip via `IX`. Animates breadcrumbs back in.
  - `onTitlePaste` / `onTitleChange` — `vg` sanitises pasted text; `mg` + `LX` validate live, surfacing a `WT`-reserved-name tooltip if invalid.
  - `onTitleKeydown` — `Escape` reverts and blurs; `Enter`/`Tab` save-then-blur with `{focus: true}` eState; `ArrowDown` on last title line blurs with `{focus: true, focusMetadata: !altKey}` (lets MarkdownView route into frontmatter).
- **Context menu:** `onPaneMenu` pushes "Rename…" (`action`, `lucide-edit-3`) and "Delete" (`danger`, `lucide-trash-2`, warning) before firing `workspace.trigger('file-menu', menu, file, source, leaf)` — **primary file-menu emission point for views**.
- **Ephemeral state:** `setEphemeralState({rename: 'start' | 'end'})` — focuses/blurs `titleEl` for inline rename when visible; falls back to `fileManager.promptForFileRename(file)` modal when not. Driven by `workspace.openLinkText(..., {eState: {rename: 'start'}})` and `fileManager.createAndOpenMarkdownFile(..., 'all')`.

### `TextFileView`

- **Kind:** class, extends `EditableFileView`.
- **Purpose:** All text-file editors (MarkdownView, BasesView, canvas-JSON view, plugin code-views) extend this. Adds debounced auto-save, three-way-merge on external modify, and the `getViewData`/`setViewData`/`clear` contract Corbomite's editor widgets must mirror.
- **Instance fields:** `data`, `dirty = false`, `saving = false`, `saveAgain = false`, `isPlaintext = true` (BasesView flips to `false`), `lastSavedData = null`.
- **Constructor:** wraps `save.bind(this)` with `debounce(fn, 2000)` and exposes it as `this.requestSave`. `requestSave()` sets `dirty = true` and kicks the debouncer — **2000 ms trailing-edge debounce**.
- **Required overrides:** `getViewData(): string`, `setViewData(data, clear): void`, `clear(): void`.
- **Save pipeline (`save(immediate?)`):**
  1. Early-exit on `!this.file || file.deleted`.
  2. If `this.saving`: unless `immediate`, set `saveAgain = true` and return.
  3. `currentData = getViewData()`. Skip-write if `lastSavedData === currentData` or `lastSavedData === null` (not hydrated yet).
  4. Snapshot `previousLastSaved`. If `immediate` (the `onUnloadFile` "close and flush" path), null out `data`/`lastSavedData` and call `clear()`; otherwise update `data = lastSavedData = currentData`.
  5. `await Mb(vault.adapter.promise)` (drain adapter serialisation queue) → `await vault.modify(file, currentData)`.
  6. On throw: restore `lastSavedData = previousLastSaved`, show `msgFailToSaveFile` Notice, call `fileManager.storeTextFileBackup(file.path, currentData)` to preserve the content.
  7. `finally`: clear `saving`; if `saveAgain && !immediate`, re-run `save()` to catch edits made during the write.
- **External-modify (`onModify`, subscribed to `vault.on('modify')`):** Skipped when `this.saving` (our own write echoing). Otherwise `loadFileInternal(file, false)` reads fresh (prefers `cachedRead` when the file is mid-save to dodge races):
  - `lastSavedData === freshDisk` → no-op.
  - `dirty && getViewData() === freshDisk` → no-op (caught up).
  - Else **three-way merge**: `merged = FX(previousLastSaved, currentViewData, freshDiskData)` (a diff-match-patch `diffMain → patchMake → patchApply` helper at `_internal.js:461203`). Show `msgFileChanged` Notice, `setViewData(merged, false)`.
  - Merge only when `isPlaintext`; BasesView (`isPlaintext = false`) accepts disk verbatim.
- **`onLoadFile(file)`:** `loadFileInternal(file, true)` → `vault.read` → `lastSavedData = contents` → `setData(contents, true)` → `setViewData(contents, true)`.
- **`onUnloadFile()`:** `await save(true)` — synchronous, not debounced.
- **`saveImmediately()`:** public "if dirty, save now" shortcut for quit handlers.

### `ViewRegistry`

- **Kind:** class, extends `Events`. Assigned at boot to `app.viewRegistry`.
- **Constructor:** seeds the six **core built-in views** (exhaustive table in Section 7): MarkdownView for `.md`, ImageView (`xZ`) for `['bmp','png','jpg','jpeg','gif','svg','webp','avif']`, AudioView (`MZ`) for `['mp3','wav','m4a','3gp','flac','ogg','oga','opus']`, VideoView (`XZ`) for `['mp4','webm','ogv','mov','mkv']`, PdfView (`KZ`) for `['pdf']`, plus `"release-notes"` (`h0`) type-only.
- **Instance fields:** `viewByType: Record<string, (leaf) => View>`, `typeByExtension: Record<string, string>`.
- **Methods:**
  - `registerView(type, factory)` — throws on duplicate; fires `view-registered`.
  - `unregisterView(type)` — silent no-op if absent; fires `view-unregistered` on success.
  - `registerExtensions(exts, type)` — pre-scans for duplicates and throws before any mutation; fires one `extensions-updated` at the end.
  - `unregisterExtensions(exts)` — fires one `extensions-updated`.
  - `registerViewWithExtensions(exts, type, factory)` — atomic convenience (used for all six built-ins).
  - `getViewCreatorByType(type)` / `getTypeByExtension(ext)` / `isExtensionRegistered(ext)` — reads.
- **Lifecycle:** One per `App`. Created before `Workspace` so built-ins exist at layout-restore. Never destroyed; plugin-registered entries come/go via `register`/`unregister`.

### `eD` — deferred-load view stub (physically at `vault/Vault.js:1118`, functionally view-domain)

- **Kind:** class, extends `View`.
- **Purpose:** Workspace-restore optimisation. Instead of instantiating every tab's real view at startup (many of which read files + parse markdown + build CodeMirror), Workspace constructs `new eD(leaf, type, icon, title)` for any leaf not immediately visible — a lightweight stub that paints the tab header from cached `icon`/`title` and defers the real construction. On first interaction (`click` or `onNodeInserted`), `rerender()` swaps the stub via `leaf.setViewState({type, state}, estate)`.
- **Public-ish API:** `WorkspaceLeaf.isDeferred` getter (`view instanceof eD`); `leaf.loadIfDeferred()` forces the rerender. Plugins routinely gate with `if (leaf.isDeferred) return;`.

### `tD` — "empty" / "new tab" view (`vault/Vault.js:1190`)

- **Kind:** class, extends `ItemView`. `viewType === 'empty'`.
- **Purpose:** The New Tab / "what would you like to do?" pane. Rendered for leaves with no file, or when the last file is deleted with `!allowNoFile` and no history.
- **Content:** `.empty-state-container` with "Create new file" (`file-explorer:new-file`), "Go to file" (`switcher:open`), conditional "Open web viewer" (only when `webviewer` internal plugin enabled), and "Close" (`leaf.detach()`). Each action shows hotkey hint via `hotkeyManager.printHotkeyForCommand(id)`.
- **Drag-drop:** accepts file drops anywhere inside the empty-state div.

### `nD` — "unknown view type" fallback (`vault/Vault.js:1310`)

- **Kind:** class, extends `tD`.
- **Purpose:** When `workspace.json` references an unregistered viewType (plugin disabled, newer-Obsidian vault, etc.), `setViewState` builds `new nD(leaf, type)` instead of throwing. Shows "unknownPaneTitle"/"unknownPaneDesc" with Westworld easter-egg tooltip *"This pane doesn't look like anything to me."* and `lucide-ghost` icon.
- **Behaviour:** subscribes to `workspace.on('layout-change', onLayoutChange)` — rebuilds itself if the missing plugin loads later. Offers "Close" + "Close all of this type" (the latter revealed only when `workspace.getLeavesOfType(type).length > 1`).

---

## 2. Data structures

### `ViewState` (persisted per leaf in `.obsidian/workspace.json`; built by `WorkspaceLeaf.getViewState`)

```typescript
{
  type: string;                // View.getViewType() or "empty"
  state: Record<string, any>;  // View.getState() — view-defined
  active?: boolean;            // transient; only set on command-driven open
  group?: string;              // stacked-tab group id (16-char random)
  pinned?: boolean;
  icon?: string;               // cached View.getIcon() — enables deferred eD tabs to paint without instantiation
  title?: string;              // cached View.getDisplayText() — same rationale
}
```

### `EphemeralState` (not persisted; passed via `WorkspaceLeaf.setViewState(_, eState)`)

```typescript
{
  focus?: boolean;             // EditableFileView.onTitleKeydown
  focusOnMobile?: boolean;
  focusMetadata?: boolean;     // MarkdownView routes into frontmatter
  rename?: 'start' | 'end';    // EditableFileView: enter/leave title-rename
  scroll?: number;             // line for scroll-to-match
  line?: number;               // alias
  match?: { matches: [number, number][]; content: string }; // search highlight
  subpath?: string;            // "#heading" or "#^block" — BasesView uses for sub-view
}
```

Open dictionary; subclasses add their own keys without schema check.

### `SetStateResult` (mutable out-param passed by `WorkspaceLeaf` to `View.setState`)

```typescript
{
  history: boolean;  // "record a history entry"
  layout: boolean;   // "fire layout-change"
  close: boolean;    // "I can't recover; close the leaf"
  done?: () => void; // "call me after layout-change fires"
}
```

`FileView.setState` flips `history`/`layout` on actual file swap and sets `done = () => syncState()` for stacked-group sync post-layout.

---

## 3. On-disk contracts

No direct on-disk reads/writes in this domain. `TextFileView.save` is the only writer, and it goes through `Vault.modify(file, data)` → `FileSystemAdapter.write` — the adapter owns the filesystem contract.

`TextFileView` does however touch one adjacent file indirectly: on save failure it calls `fileManager.storeTextFileBackup(file.path, content)`, which writes into the `file-recovery` internal plugin's backup store (at `.obsidian/plugins/file-recovery/` under `data.json` or similar). That schema belongs to the `plugin/internal-plugins/file-recovery` domain, not here.

---

## 4. Events emitted

### `ViewRegistry` (extends `Events`)

| Event | Payload | Triggered when | Typical consumers |
|---|---|---|---|
| `view-registered` | `(type: string)` | `registerView` succeeds (cite `views/ViewRegistry.js:47`) | `Workspace.js:347` rebuilds `nD` fallback leaves |
| `view-unregistered` | `(type: string)` | `unregisterView` finds and removes (cite `:52`) | `Workspace.js:348` detaches/replaces leaves |
| `extensions-updated` | `()` | Any `registerExtensions`/`unregisterExtensions` call — one emission per call, N exts coalesce (cite `:73`, `:80`) | `MetadataCache.js:994` re-evaluates indexability; FileExplorer refreshes icons |

No other class in this domain emits events. Two workspace events are **triggered from this domain** (plugin extension surface, see Section 10):

- `workspace.trigger('leaf-menu', menu, leaf)` — `views/View.js:340` (inside `ItemView.onMoreOptions`).
- `workspace.trigger('file-menu', menu, file, source, leaf?)` — `views/EditableFileView.js:245` (right-click file view) and `views/FileView.js:106` (breadcrumb folder context).

---

## 5. Events consumed

| Listener file | Subscribes to | Why |
|---|---|---|
| `views/FileView.js:142` | `vault.on('rename', onRename)` | Update breadcrumb + `titleEl` + `leaf.updateHeader` when our `this.file` is renamed |
| `views/FileView.js:143` | `vault.on('delete', onDelete)` | Close the leaf / revert to history / swap to `tD` empty-state |
| `views/ItemView.js:276` (`views/View.js:276` — same source) | `leaf.on('group-change', onGroupChange, this)` | Stacked-group peer sync |
| `views/ItemView.js:279` (`views/View.js:279`) | `leaf.on('history-change', updateNavButtons, this)` | Enable/disable back/forward arrows in the action bar |
| `views/TextFileView.js:27` | `vault.on('modify', onModify, this)` | Detect external-to-us writes; run three-way merge via `FX` |

`nD` (empty-state fallback) additionally subscribes to `workspace.on('layout-change', onLayoutChange)` so it can rebuild itself when its missing view type is finally registered.

---

## 6. Commands registered

No commands registered here. This domain only provides the class hierarchy; commands are registered by each concrete view's plugin (e.g. MarkdownView registers `markdown:toggle-preview`; CanvasView registers `canvas:new`).

---

## 7. Registries owned

### `ViewRegistry` — viewType↔factory and extension↔viewType tables

- **Stores:** `viewByType: Record<string, (leaf) => View>` (factory closures; factory is called with the hosting `WorkspaceLeaf`) and `typeByExtension: Record<string, string>` (lowercase ext, no dot → viewType; one type can claim many exts).
- **Populated by:** constructor seeds six core entries (see below); `Plugin.registerView`/`registerExtensions` at `plugin/Plugin.js:120`/`:135` delegate with auto-unregister on plugin unload; internal plugins (`canvas`, `bases`, `webviewer`, `release-notes`, …) register their own types during `internalPlugins.loadPlugin(id)`.
- **Read by:** `WorkspaceLeaf.setViewState` (`workspace/WorkspaceLeaf.js:1094`) via `getViewCreatorByType` — falls back to `_empty` (cached `tD`) for `"empty"`/missing, to `new nD(leaf, type)` for unknown-nonempty. `WorkspaceLeaf.openFile` (`workspace/WorkspaceLeaf.js:926`) via `getTypeByExtension` — reuses the current view if `canAcceptExtension(ext)` is true. `MetadataCache` (`metadata/MetadataCache.js:994`) via `isExtensionRegistered` to decide indexability. `Workspace` (`workspace/Workspace.js:347–348`) listens to `view-registered`/`view-unregistered` to patch up `nD` fallback leaves.
- **Persistence:** in-memory only, rebuilt every boot. `.obsidian/workspace.json` persists viewType *strings*; factory lookup re-resolves at restore.
- **Lifecycle:** created at app boot before `Workspace`. Plugin-added entries live until plugin unload. Vault switch rebuilds `App` → registry reseeds.

#### Built-in registrations

Six entries fire from `ViewRegistry`'s constructor at `views/ViewRegistry.js:11–32`. Canvas, Bases, Outline, Backlinks, etc. are **not** here — they register from their internal-plugin init path.

| Extension(s) | View type | Class | Base |
|---|---|---|---|
| `md` | `"markdown"` (`MarkdownView.VIEW_TYPE`) | `MarkdownView` (`editor/markdown/MarkdownView.js`) | `TextFileView` |
| `bmp`, `png`, `jpg`, `jpeg`, `gif`, `svg`, `webp`, `avif` | `"image"` | `xZ` (ImageView, `_internal.js:580641`) | `FileView` |
| `mp3`, `wav`, `m4a`, `3gp`, `flac`, `ogg`, `oga`, `opus` | `"audio"` | `MZ` (AudioView, `_internal.js:580596`) | `FileView` |
| `mp4`, `webm`, `ogv`, `mov`, `mkv` | `"video"` | `XZ` (VideoView, `_internal.js:601401`) | `FileView` |
| `pdf` | `"pdf"` | `KZ` (PdfView, `_internal.js:601226`, uses `rendering/loadPdfJs.js`) | `FileView` |
| — (no ext) | `"release-notes"` | `h0` | `ItemView` (registered via `registerView` only; opened via command) |

Additionally, internal plugins register (confirmed via consumer grep, not source-read in this pass):

| Ext | Type | Registered by |
|---|---|---|
| `canvas` | `"canvas"` | Canvas internal plugin |
| `base` | `"bases"` | Bases internal plugin (`VX = "bases"` at `views/TextFileView.js:170`; `BasesView` = class `HX`) |

**Corbomite implication:** 6 + 2 = 8 built-in file-type viewers define the expected surface. Corbomite needs equivalents for at minimum `md`, `canvas`, and `pdf`; image/audio/video are `QMediaPlayer`/`QLabel` trivialities but still need the extension-dispatch wiring.

---

## 8. Invariants

- A `View`'s `containerEl` is attached to `leaf.containerEl` between `open()` and `close()`; detached before `onClose` runs. Subclasses must not reparent `containerEl`.
- `containerEl.getAttribute('data-type') === getViewType()` — constant for the instance's lifetime. CSS selectors depend on it.
- `View.load()` runs exactly once per instance, between `open()` and `close()`. Re-opening requires a fresh instance.
- `onload` fires before `onOpen`; `onOpen` runs with `containerEl` already in the DOM. Convention: subscriptions in `onload`, async DOM work in `onOpen`.
- `FileView.file` is `null` until the first `loadFile` succeeds. On `onLoadFile` throw, `this.file` is rolled back to `null` and a Notice is shown; the leaf stays usable.
- `TextFileView.requestSave()` debounce is exactly 2000 ms trailing-edge. N calls inside the window coalesce to one `save()`.
- `TextFileView.save()` re-entry sets `saveAgain = true`; the first call's `finally` re-runs save once — exactly one extra write regardless of how many `requestSave`s piled up.
- `TextFileView.save(true)` (immediate mode — the `onUnloadFile` flush path) bypasses debounce, skips `saveAgain`, and afterwards nulls `data`/`lastSavedData` and calls `clear()`.
- `TextFileView.lastSavedData === null` post-construction means "never loaded yet"; `save()` early-returns even when `dirty` is set, because there's no baseline to compare against. `onLoadFile` populates it before the user can edit.
- `ViewRegistry.registerView` is **not idempotent** — throws on duplicate type. Plugin wrappers `unregisterView` on unload.
- `ViewRegistry.registerExtensions` is **atomic across the array**: any existing ext → throw with no mutation. Partial failures impossible.
- A `WorkspaceLeaf` with `view instanceof eD` is "deferred"; its view is a stub. Plugins must `leaf.loadIfDeferred()` before reading state.
- Leaves with `view instanceof nD` mean "workspace.json referenced an unregistered viewType". Recoverable; `nD` listens for `layout-change` in case the plugin loads later.
- Factories passed to `registerView` are called exactly once per leaf instantiation; never cache or share `View` instances across leaves.

---

## 9. Observable user features

- Opening any file (File Explorer click, `[[link]]` cmd-click, Quick-Switcher) picks the correct view type via extension dispatch; unknown extensions fall back to `openWithDefaultApp`.
- Opening a file whose view is already live and returns `true` from `canAcceptExtension` reuses that view rather than instantiating a new one.
- Title-bar inline rename: click title → live validation as you type, Enter/Tab saves, Escape reverts, ArrowDown off the last line refocuses editor. Mobile first-touch gates contenteditable (no keyboard on every tab visit).
- Renaming a file externally updates open views' breadcrumbs and title live; deleting the open file steps back in history, shows the empty/new-tab pane, or detaches the leaf.
- 2-second debounced save + three-way-merge when the file changes on disk mid-edit: user gets a "File on disk changed" notice and **keeps their in-progress edits**. Only applies to `isPlaintext` views (BasesView accepts disk verbatim).
- Save failures surface a toast with the error message; content is snapshotted into the `file-recovery` plugin's backup store for manual restore.
- Tab context menu (default `View.onTabMenu`): Close / Close others / Close after / Close all. Pinned tabs are excluded from Close-others/Close-all.
- Deferred tabs (not focused since boot) still paint correct icon + title (via `workspace.json`-cached `icon`/`title`) despite the underlying `eD` stub.
- Vaults referencing disabled-plugin view types show the `nD` "Unknown view type" pane with Close / Close-all-of-type actions instead of crashing.
- The "…" menu in every view header uses the canonical section order; plugins hook via `workspace.on('leaf-menu', ...)`.
- Mobile: left-side sidebar-toggle + 100 ms tap-vibration; "…" contextmenu expands right sidedock.
- ItemView back/forward nav buttons enable/disable based on leaf history state.
- FileView breadcrumbs are clickable (reveal folder in File Explorer), right-clickable (folder context menu with New note etc.), and draggable (folder drag-source).

---

## 10. Extension surfaces exposed

| Surface | Registration verb | Consumer site | Plugin supplies |
|---|---|---|---|
| Custom view type | `Plugin.registerView(type, factory)` → `ViewRegistry.registerView` | `WorkspaceLeaf.setViewState` at `workspace/WorkspaceLeaf.js:1094` | `(leaf) => View` factory |
| Extension → view association | `Plugin.registerExtensions(exts, type)` → `ViewRegistry.registerExtensions` | `WorkspaceLeaf.openFile` at `:926`; `MetadataCache.js:994` | `exts: string[]`, `type: string` (must be pre-registered) |
| `View`/`ItemView`/`FileView`/`EditableFileView`/`TextFileView` subclassing | JS `class X extends Y` | instantiated by factory | Overrides of `getViewType`, `getDisplayText`, `getIcon`, `onOpen`/`onClose`, `getState`/`setState`, `getEphemeralState`/`setEphemeralState`, `onLoadFile`/`onUnloadFile` (FileView), `getViewData`/`setViewData`/`clear` (TextFileView), `onMoreOptionsMenu`, `onPaneMenu`, `onTabMenu`, `canAcceptExtension`, `receiveSyncState` |
| `leaf-menu` event | `workspace.on('leaf-menu', cb)` | fired at `views/View.js:340` | `(menu, leaf) => void` callback, called mid-construction so plugins can `menu.addItem(...)` |
| `file-menu` event | `workspace.on('file-menu', cb)` | fired at `views/EditableFileView.js:245` + `views/FileView.js:106` | `(menu, file, source, leaf?) => void`; `source` is caller id ("more-options", "file-explorer-context-menu", …) |

Class-subclassing pattern notes:

- **`TextFileView` subclassing** is how plugins add new text-backed file types (`.kanban`, `.excalidraw`, `.drawio.svg`, …): override `getViewType`, `getIcon`, `getViewData`, `setViewData`, `clear`; 2 s debounced save comes free.
- **`ItemView` subclassing** is how plugins add sidebar panels (outline, calendar, backlinks): override `getViewType`, `getDisplayText`, `getIcon`, `onOpen`; host via `workspace.getRightLeaf(false).setViewState({type})`.

---

## 11. Corbomite mapping

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `View` / `ItemView` base hierarchy | — | **Missing** | No hierarchy. `EditorViewSpace` is a `QTabBar`+`QStackedWidget` that branches on extension in `openNote`/`openCanvas`/`openGraphView`. |
| `FileView` (file-bound view; rename/delete reactions; breadcrumbs) | `src/editor/NoteEditorWidget.h/cpp` | **Partial** | Note-only; no breadcrumbs; rename/delete wired ad-hoc in `EditorViewManager`. |
| `EditableFileView` inline-rename | — | **Missing** | Rename is modal-only. |
| `TextFileView` 2000 ms debounced save + three-way-merge on external modify + `storeTextFileBackup` | `NoteEditorWidget` auto-save | **Partial** | Auto-save exists; debounce timing and on-disk-merge semantics need verification. No backup-on-failure equivalent. |
| `ViewRegistry` (type↔factory, ext↔type) | — | **Missing** | `EditorViewSpace.cpp:119` `endsWith(".canvas")` branching is the ad-hoc analogue. **Highest-leverage refactor target from this domain.** A proper `Corbomite::ViewRegistry` (Qt class emitting `viewRegistered`/`viewUnregistered`/`extensionsUpdated` signals; `registerView(QString, factory)` / `registerExtensions(QStringList, QString)` API) unblocks canvas/pdf/base/plugin-supplied-view work simultaneously. |
| Built-in view set (md, canvas, pdf, image, audio, video, release-notes) | `NoteEditorWidget` (md), `CanvasViewTab` (canvas), `GraphViewTab` (graph, not an Obsidian-leaf view) | **Partial** | md and canvas present; pdf/image/audio/video absent. |
| `eD` deferred-load stub | — | **Missing** | Corbomite loads all tabs eagerly. Large vaults will stutter on session restore. |
| `tD` empty-state pane | — | **Missing** | Empty `EditorViewSpace` shows nothing; no "New tab" action pane. UX gap. |
| `nD` unknown-view-type fallback | — | **Missing** | Unknown tab-types in session JSON have undefined behaviour. |
| `ViewState { type, state, pinned, icon, title }` JSON | `EditorViewManager::buildSessionState`/`restoreFromSession` | **Partial** | Shape differs; convergence needed for Obsidian-vault interop. |
| `setEphemeralState({ scroll, match, rename, subpath, focus })` | — | **Missing** | No ephemeral-state concept. `openLinkText`-style "scroll to heading" and `rename: 'start'` need distinct entry points. |
| `canAcceptExtension(ext)` view reuse | — | **Missing** | Always closes-and-reopens on extension change. |
| `onTabMenu` default close-group actions | `EditorViewSpace::showTabContextMenu` | **Partial** | Verify exact items and pinned-tab exclusion. |
| `leaf-menu` / `file-menu` events (mid-construction menu mutation) | — | **Missing** | No analogue. For plugin API: adopt `Corbomite::MenuEvent` signal over `QMenu` construction. |
| `Component.registerEvent` auto-cleanup | Qt parent-child + `QObject::connect` auto-disconnect | **Partial** | Qt covers signals; plugin-API needs a `Component`-equivalent lifecycle wrapper for DOM events and timers. |

---

## 12. Markoff gap confirmations / discoveries

N/A — `views/` is the mounting/registration surface, not the editor/rendering engine. Markoff-gap signals live in `editor/markdown` and `rendering`. One adjacent item worth flagging (propagating to `01-markoff-gaps.md` from this pass):

**New signal — three-way-merge on external file modification.** `TextFileView.loadFileInternal(file, false)` calls `FX(previousLastSaved, currentViewData, freshDiskData)` which is a `diff-match-patch` wrapper (diff_main → patch_make → patch_apply). The user sees a `msgFileChanged` Notice but **does not lose their in-progress edits**. Corbomite's `NoteEditorWidget` auto-save behaviour in this scenario is unverified; if Corbomite simply reloads the disk version it will clobber unsaved edits. This is a correctness gap. Adding to `01-markoff-gaps.md` as a new "Pass 2 additions — views" entry.

---

## 13. Open questions

1. `DataTransfer` shape for `handleDrop(event, draggable, dragOver)` on text-selection drags (as opposed to file drags) — referenced by `dragManager.handleDrop` but not fully traced in this domain.
2. `View.handleCut`/`handleCopy`/`handlePaste` — default no-ops; which concrete view(s) override, and what's the DOM contract (clipboard vs synthetic)? Likely `MarkdownView`; confirm in `editor/markdown` Pass 2.
3. `Xg` macOS native menu-bar class is bundled into `View.js` by the de-minifier. Is it expected to be in `views` or should a cleaner taxonomy split it into `platform/macMenu`?
4. `_empty` cached `tD` instance on `WorkspaceLeaf` — singleton reused for every empty state on that leaf, or recreated per `open(null)`? Lifetime not traced.
5. `msgFailToSaveFile` Notice — does the toast expose a "Retry" action or is `storeTextFileBackup` the only recovery? Need locale-bundle grep.
6. `FX` three-way-merge gate: `if (dirty && viewData !== freshDisk && viewData !== lastSaved)` — confirmed as the only case. But: `isPlaintext = false` views (BasesView) accept disk verbatim on external modify. Is this a known data-loss risk for Bases files edited externally mid-session?
7. `View.onHeaderMenu(menu)` default no-op — where is it fired? Not seen in `View.js`/`ItemView.js` default chrome; possibly mobile-only or external.
8. Canvas and Bases built-in view registrations — inferred registered by internal plugins (`app.internalPlugins.plugins.canvas`, `.bases`) via `registerViewWithExtensions`. Confirm in `bases`/`canvas` Pass 2.

---

## 14. Recommended Pass 3 synthesis input

1. **`ViewRegistry` is the central file-type-dispatch primitive.** Corbomite's ad-hoc extension branching (`EditorViewSpace.cpp` `endsWith(".canvas")`) must be refactored into a proper registry class before further file-type support (PDF, images, bases, plugin types) can proceed. Promote to `GAP-ANALYSIS.md` as high-leverage.
2. **Eight built-in extension→view pairings define the minimum viewer set for Obsidian-compatible behaviour.** Pass 3 should flag md (have), canvas (have), pdf (missing), image (missing), audio (missing), video (missing), base (missing — tracked under `bases`), release-notes (low priority) in `FEATURE-MATRIX.md`.
3. **`TextFileView.save` contract — 2-second debounce, three-way-merge on external modify, backup-on-save-failure via `file-recovery` plugin — is the compatibility target for any "edit a text file" Corbomite view.** Promote the contract to `VAULT-FORMAT.md` ("file-write behaviour") so any future Corbomite adapter exporting to Obsidian-compatible workflows honours it.
4. **`ViewState` JSON shape (`type`, `state`, `pinned`, `icon`, `title`) is what `.obsidian/workspace.json` persists per leaf.** That's the interop format for session-restore in an Obsidian-compatible vault. Track as a `VAULT-FORMAT.md` entry alongside `workspace` domain's full layout schema.

---

## 15. Cross-domain references

| Other domain | Reference type | Brief description |
|---|---|---|
| `workspace` | primary consumer | `WorkspaceLeaf.openFile`/`setViewState` look up factories via `ViewRegistry`; `Workspace` listens to `view-registered`/`view-unregistered`; every view takes a `WorkspaceLeaf` constructor arg. `eD`/`tD`/`nD` are consumed only by `WorkspaceLeaf`. |
| `core` | dependency | `App` owns `app.viewRegistry`. Views reach `app` via `leaf.app`. `Events` mixed into `ViewRegistry`; `Component` (ui/components) is `View`'s base. |
| `vault` | consumer + physical cohabitant | `FileView` subscribes to `vault.on('rename'|'delete')`; `TextFileView` subscribes to `vault.on('modify')` and writes via `vault.modify`. `TextFileView.save` failure path calls `fileManager.storeTextFileBackup`. `eD`/`tD`/`nD` physically extracted into `vault/Vault.js` by the de-minifier. |
| `editor/markdown` | primary consumer | `MarkdownView extends TextFileView`; first-registered view (for `.md`). The debounced-save + three-way-merge semantics are its save contract. |
| `bases` | consumer | `BasesView` (class `HX` trailing in `TextFileView.js`) extends `TextFileView`. Registered for `.base` via internal plugin (confirm in `bases` Pass 2). |
| `metadata` | consumer | `MetadataCache.isExtensionRegistered` consults `typeByExtension` for indexability. |
| `plugin` | delegate | `Plugin.registerView` / `Plugin.registerExtensions` are 1:1 wrappers with auto-unregister on unload. |
| `ui/components` | dependency | `Component` base class; `Notice`, `setIcon`, `setTooltip`, `displayTooltip`, `Keymap`. |
| `ui/menu` | dependency | `Menu` built by `onMoreOptions`; `onTabMenu`, `onPaneMenu` pass it through. |
| `ui/popups` | consumer | Hover-previews wired via `workspace.registerHoverLinkSource(viewType, …)`. |
| `settings` | consumer | `PluginSettingTab.js:120–137` wraps `registerView`/`registerExtensions` for settings-exposed views. |

**Short symbols referenced from other domains:**

| Symbol | Defined in | Used here for |
|---|---|---|
| `Component` | `ui/components` | `View extends Component` |
| `Events` | `core` | `ViewRegistry extends Events` |
| `Menu` | `ui/menu` | `onMoreOptions`/`onTabMenu`/`onPaneMenu` |
| `Notice` | `ui/components` | `TextFileView.save` + `FileView.loadFile` error paths |
| `TFile` / `TFolder` | `vault` | `FileView.file`; breadcrumb folder lookup |
| `vault` / `fileManager` / `workspace` | `core` (via `App`) | Accessed via `this.app.*` |
| `WorkspaceLeaf` | `workspace` | Constructor arg; `leaf` field |
| `FX` | `utils` (defined `_internal.js:461203`) | Three-way-merge in `TextFileView.loadFileInternal` |
| `debounce` | `utils` | 2000 ms save debouncer |
| `jy`, `Vy`, `Hy`, `zy`, `qy` | `_internal` constants | Extension arrays for md/image/audio/video/pdf |
| `s0` | `_internal` | `"release-notes"` viewType literal |
| `xZ`, `MZ`, `XZ`, `KZ`, `h0` | internal plugins | Image / Audio / Video / Pdf / ReleaseNotes view classes |
| `MarkdownView` | `editor/markdown` | First registered view |
| `gm` | locale bundle | All i18n strings |
| `Platform` | `platform` | `isMobile`/`isPhone`/`isMacOS`/`canSplit`/`canPopoutWindow`/`hasPhysicalKeyboard` |
| `LX`, `IX`, `WT`, `jT`, `UT` | `vault` / `utils` | Filename validation + inline tooltip in `EditableFileView` |
| `Keymap` | `ui/components` | `Keymap.isModEvent(e)` |
