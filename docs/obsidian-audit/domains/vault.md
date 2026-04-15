# `obsidian/vault` — vault, file tree, filesystem adapters

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/vault/`
**File count:** 10
**Files:** `CapacitorAdapter.js`, `FileManager.js`, `FileSystemAdapter.js`, `getLinkpath.js`, `normalizePath.js`, `parseLinktext.js`, `TAbstractFile.js`, `TFile.js`, `TFolder.js`, `Vault.js`

**De-minifier artifact note:** `TAbstractFile.js`, `TFile.js`, `TFolder.js` and `Vault.js` are near-duplicate extractions of the same source range (`app.js 80065-81476`); all four re-declare the full `TAbstractFile`/`TFile`/`TFolder`/`Vault` class cluster. Only the `// public API symbol:` comment differs. `Vault.js` was read as the canonical copy. The last ~400 lines of each of these files also carry adjacent leftover classes (`eD`, `tD`, `nD` — `View`/`ItemView`-derived empty-state panes); those are *not* part of the vault domain and are audited under `views`. The same pattern is confirmed between `FileSystemAdapter.js` and `CapacitorAdapter.js` (different real sources, but both trail into `lezer` parser code).

**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> The vault layer. `Vault` is the in-memory file tree (`TFile`/`TFolder` nodes rooted at a path), backed by either `FileSystemAdapter` (Electron desktop, uses `original-fs` to bypass ASAR + `btime` for birthtime + case-sensitivity probe) or `CapacitorAdapter` (mobile). `Vault` emits `create`/`modify`/`delete`/`rename`/`closed`/`raw` events and handles `.trash`/system-trash deletion modes. `FileManager` is higher-level: handles file-rename refactoring (link rewriting across the vault), trash, frontmatter rewrite (`processFrontMatter`), generate-markdown-link, copy/move with conflict resolution, attachments folder. `getLinkpath`/`normalizePath`/`parseLinktext` are the path-string utilities.

---

## 1. Public API surface

Ten exported symbols in this domain. Four of them (`TAbstractFile`, `TFile`, `TFolder`, `Vault`) live in the same IIFE; `FileSystemAdapter` and `CapacitorAdapter` are standalone adapter classes; `FileManager` is standalone; the three `*path*.js` files export tiny utility functions.

### `TAbstractFile`

- **Kind:** class (abstract base)
- **Exported as:** `TAbstractFile`
- **Signature:** `new TAbstractFile(vault: Vault, path: string)`
- **Instance fields:** `parent: TFolder | null`, `deleted: boolean`, `vault: Vault`, `path: string`, `name: string`.
- **Methods:**
  - `setPath(path: string): void` — updates `path` and `name` (`name = basename(path)`).
  - `getNewPathAfterRename(newName: string): string` — returns the computed new path after renaming to `newName` within the same parent. Strips control chars `[\x00-\x1F]` and trims. Returns `""` when the file has no parent (i.e., detached).
- **Purpose:** Shared base for both files and folders. Carries path/name identity and parent-linkage.
- **Lifecycle:** Constructed by `Vault.onChange('folder-created' | 'file-created', …)` — never by user code. Destroyed implicitly when `Vault.onChange('*-removed')` deletes it from `fileMap` (sets `deleted = true` as a tombstone).
- **Mixes in:** neither.

### `TFile`

- **Kind:** class, extends `TAbstractFile`
- **Instance fields:** `basename: string` (name without extension), `extension: string` (lowercase, no dot), `stat: { size, mtime, ctime } | null`, `saving: boolean` (true while a `Vault.modify`/`create`/`append`/`process` is in flight — used to suppress cache invalidation).
- **Methods:**
  - `setPath(path)` — overrides to derive `basename`/`extension`.
  - `getShortName()` — returns `basename` for `.md`, else `name`. Used in UI chrome.
  - `getNewPathAfterRename(newName)` — lets user supply just the basename; the adapter reappends the extension.
  - `cache(content: string | null)` — writes to the process-wide `XT: WeakMap<TFile, string>` cache if `content.length <= Vault.cacheLimit`, else deletes. `null` always deletes.
  - `updateCacheLimit()` — drops cache entry if over new limit.
  - `toString()` → `path`.
- **Lifecycle:** Identical to `TAbstractFile`.
- **Mixes in:** neither.

### `TFolder`

- **Kind:** class, extends `TAbstractFile`
- **Instance fields:** `children: (TFile | TFolder)[]`.
- **Methods:**
  - `isRoot()` — true iff `path === "/"`.
  - `getParentPrefix()` — `""` for root else `path + "/"`. Used to build paths for children.
  - `getFileCount()` — recursive count of all `TFile` descendants.
  - `getFolderCount()` — recursive count of all `TFolder` descendants (including self's children).
- **Lifecycle:** Identical to `TAbstractFile`.
- **Mixes in:** neither.

### `Vault`

- **Kind:** class, extends `Events`
- **Exported as:** `Vault` (assigned to `App.vault`)
- **Constructor:** `new Vault(adapter)`. Creates the empty root `TFolder`, debounced `requestSaveConfig` (1 s leading-edge), `fileMap = { "/": root }`, `cacheLimit = 65536` bytes, `configDir = ".obsidian"`.
- **Key methods (grouped):**
  - *Identity / Tree:* `getName()` → `adapter.getName()`. `getRoot()`. `getFileByPath/getFolderByPath/getAbstractFileByPath(path)` — direct `fileMap` lookup. `getAbstractFileByPathInsensitive(path)` — exact first, then case-insensitive scan across equal-length keys. `isEmpty()` — only root present. `getAllLoadedFiles()`, `getAllFolders(includeRoot?)`, `getMarkdownFiles()`, `getFiles()`. `static recurseChildren(root, cb)` — DFS. `checkForDuplicate(file, newName)` — collision check. `checkPath(path)` — throws on illegal chars.
  - *Config / disk:* `static validateConfigDir(s)` (must start with `.`, pass regex `UT`, not equal `.`). `setConfigDir(s)` — falls back to `.obsidian` on invalid. `setupConfig()` — ensures `configDir`, reads `app.json` + `appearance.json`, merges, applies `editorFontFamily → textFontFamily` rename, triggers save. `saveConfig()` — splits config via `AC.contains(key)` allow-list into appearance.json vs app.json. `reloadConfig()` (debounced 500 ms, leading-edge) — stats both files, reloads on mtime change, emits `config-changed` per diff. `getConfig(key)` — returns `config[key]` with fallback to `PC[key]` defaults; deep-clones arrays/objects. `setConfig(key, v)` — no-op on equal; `undefined` deletes; schedules save and fires `config-changed`. `getConfigFile(name)` → `configDir + '/' + name + '.json'`. `readConfigJson/writeConfigJson/deleteConfigJson(name)`. `readPluginData(pluginDir)` / `writePluginData(dir, data, opts)` — `<dir>/data.json`. `readJson(path)` — returns `null` on ENOENT, `undefined` on parse fail (no throw). `writeJson(path, data, opts)` — `JSON.stringify(data, undefined, 2)`, swallows errors.
  - *Lifecycle:* `load()` — starts adapter watcher; on desktop also `watchHiddenRecursive(configDir)`. `setFileCacheLimit(n)` — rewalks fileMap to evict over-limit caches.
  - *Reconciliation:* `onChange(kind, path, oldPath?, stat?)` — **central in-memory reconciler** invoked by the adapter. Mutates `fileMap`, fixes parent-child links, emits the public event. `raw` events inside `configDir/` also trigger watcher re-install and `reloadConfig` when `app.json`/`appearance.json` touched. `getDirectParent(file)`, `addChild(file)`, `removeChild(file)` — internal tree maintenance.
  - *CRUD:* `exists(path, caseInsensitive?)`. `create(path, text, opts?)` / `createBinary(path, buf, opts?)` / `createFolder(path)` — throw on existing destination. `read(file)` — UTF-8, strips BOM, populates `TFile` cache. `cachedRead(file)` — cache-first. `readBinary(file)` — for `.md` under `cacheLimit`, decodes once and populates string cache. `readRaw(path)` — bypasses `TFile`. `getResourcePath(file)` → adapter's `app://local/...?<mtime>` URL. `delete(file, recursive?)` — **permanent**, no trash. `trash(file, useSystem)` — `adapter.trashSystem` then falls through to `adapter.trashLocal`; root ignored. `rename(file, newPath)` — normalize + checkPath + adapter rename; no-op if equal. **Does not rewrite links** — that's `FileManager.renameFile`.
  - *Write-path:* `modify(file, text, opts?)`, `modifyBinary(file, buf, opts?)`, `append(file, text, opts?)`, `appendBinary(file, buf, opts?)` — all toggle `file.saving`, use `opts.immediate` callback for atomic cache sync, and invalidate cache on throw. `process(file, mutator, opts?): Promise<string>` — **atomic read-modify-write**, serialised via adapter write queue; updates cache on success. `copy(file, newPath)` — recursive via adapter.
  - *Paths:* `getAvailablePath(pathNoExt, ext)` — appends ` 2`, ` 3`, … until `getAbstractFileByPathInsensitive` is null. `getAvailablePathForAttachments(basename, ext, activeFile)` — respects `attachmentFolderPath` (`'.'`/`'./'` = same folder, `./sub` = relative under parent, else vault-absolute). Creates folder if missing. Basename is `stripHeadingForLink`'d and truncated to 250 chars.
  - *Desktop helpers:* `resolveFileUrl(url)` (parses `app://local/...` or `file://`) and `resolveFilePath(absPath)` (matches against `basePath`), both return `TFile | null`.
  - *Iteration:* `iterateFiles(files, useCache)` — `.md` only. `generateFiles(asyncFileIter, useCache)` — `.md` and `.canvas`.
  - `trigger(name, …args)`, `on(name, fn, thisArg)` — `Events` pass-through.
- **Lifecycle:** One per open vault. Constructed after `Adapter`. Torn down by dropping references; caller must call `adapter.stopWatch()` first. `closed` event marks end-of-life.
- **Mixes in:** `Events`.

### `FileManager`

- **Kind:** class (no mixins; assigned to `App.fileManager`)
- **Constructor:** `new FileManager(app)`. Registers the default `.md` new-file-parent factory. Creates `fileParentCreatorByType: Record<ext, (hintPath) => TFolder>` and `updateQueue` (single-consumer async queue serialising link-update transactions).
- **Key methods:**
  - *New-file placement:* `getNewFileParent(hintPath, filename?)` — honours `newFileLocation` (`"folder"` with `newFileFolderPath`, `"current"` uses hint's parent, else root). `registerFileParentCreator(ext, fn)` / `unregisterFileCreator(ext)` / `canCreateFileWithExt(ext)` — extension surface. `createNewFile(parent, name, ext?, content?)`, `createNewMarkdownFile(parent, name, content?)`, `createNewMarkdownFileFromLinktext(linkText, hintPath)`, `createNewFolder(parent)` — all with collision-free naming; fall back to localised "Untitled".
  - *Rename with refactor:* `renameFile(file, newPath)` — wraps `vault.rename` in `runAsyncLinkUpdate`. `runAsyncLinkUpdate(body)` awaits `metadataCache.onCleanCache`, snapshots every `{sourceFile, reference, resolvedFile, resolvedPaths[]}` via `metadataCache.iterateAllRefs`, executes `body` (which mutates the vault), then replays `updateAllLinks` against the pre-snapshot so links originally resolving to the moved file are rewritten. The queue prevents concurrent mutations; nested calls during `inProgressUpdates` queue to a drain list. `updateAllLinks(refs)` prompts the user (`alwaysUpdateLinks`-aware: "Always update" / "Just once" / "Don't update"), then delegates to `metadataCache.updateInternalLinks`.
  - *Property edits:* `deleteProperty(key)` / `renameProperty(old, new)` iterate every cached file, mutate frontmatter via `vault.process` + YAML helpers (`bI` parse, `yI` ordered-assign, `wI` stringify). Skip user-ignored files.
  - *Deletion UI:* `promptForDeletion(file)` — respects `promptDelete`; shows trash-mode-specific copy (`trashOption`), non-empty-folder / backlink-count warnings; on confirm calls `trashFile`, then applies `deleteUnlinkedAttachments` policy (`"always"` silent-trashes orphan attachments, `"ask"` shows picker, `"never"` skips). `promptForFolderDeletion`/`promptForFileDeletion` are wrappers. `promptForFileRename(file)` opens the rename modal (validates against `UT`/`WT` reserved-name regexes).
  - *Trash router:* `trashFile(file)` — branches on `trashOption`: `"system"` → `vault.trash(_, true)`, `"local"` → `vault.trash(_, false)`, `"none"` → `vault.delete(_, true)`.
  - *Attachments:* `downloadAttachmentsForNote(file)` — finds remote `http(s)://` and `data:` image URLs in markdown + frontmatter, prompts the user, downloads via `requestUrl`, saves as attachments, rewrites the note. `promptForImageDownload(urls)` is the gallery modal. `getAvailablePathForAttachment(linktext, sourcePathHint?)` — wraps `Vault.getAvailablePathForAttachments`.
  - *Link generation:* `generateMarkdownLink(file, sourcePath, subpath?, displayText?): string` — renders `[[Wiki]]` or `[text](percent-encoded)` based on `useMarkdownLinks`; uses `metadataCache.fileToLinktext` for shortest-unique under `newLinkFormat`; auto-elides alias when equal to linktext.
  - *Open helpers:* `createAndOpenMarkdownFile(name, leafMode)` — creates + opens in `source` mode with `rename: "all"` eState so filename renders editable.
  - *Content merge:* `insertIntoFile(file, content, mode = "append"|"prepend")` — merges frontmatter from both docs (right-biased by mode), serialises via `stringifyYaml`, rewrites in one `vault.process`. Shows `Notice` on invalid YAML.
  - *Frontmatter public API:* `processFrontMatter(file, mut, opts?)` — no-op on non-`.md`; else opens the file, parses frontmatter into a mutable JS object, passes to `mut`, rewrites the YAML block preserving key order via `yI`, and rewrites the body verbatim. Entire op wrapped in `vault.process`.
  - *Backup:* `storeTextFileBackup(file, content)` — calls built-in `file-recovery` plugin's `forceAdd` (tolerates plugin-disabled).
  - *Bulk undo:* `notifyForBulkUndo(backups, ms=30000)` — 30-s "Undo" toast that restores both content and `mtime` of every affected file.
  - *Stub:* `getAllLinkResolutions()` — returns `[]`; kept for forward-compat.
- **Lifecycle:** one per `App`, no `unload`.

### `FileSystemAdapter`

- **Kind:** class (single `handler` callback, no `Events` mixin). Desktop `DataAdapter` impl.
- **Constructor:** `new FileSystemAdapter(basePath)`. Loads Node builtins `original-fs` (bypasses ASAR), `path`, `url`, Electron `ipcRenderer`. Optional `btime` for birthtime (macOS/Windows). Pre-sets `insensitive = darwin || win32`, refines via `.OBSIDIANTEST` probe.
- **Instance fields:** `basePath`, `fs`, `fsPromises`, `path`, `url`, `ipcRenderer`, `btime?`, `insensitive`, `files: Record<path, AdapterFileEntry>`, `promise` (serialisation queue), `watchers: Record<path, {watcher, resolvedPath}>`, `handler` (callback to `Vault.onChange`), `killLastAction`, `thingsHappening` (60-s watchdog).
- **Notable methods** (all ops routed through `.queue` for per-adapter serialisation):
  - Standard CRUD: `mkdir({recursive})`, `rmdir(_, recursive)` (5 retries), `remove`, `read`/`readBinary`/`write`/`writeBinary`/`append`/`appendBinary`/`process`, `rename`, `copy`/`copyRecursive` (uses `COPYFILE_EXCL`), `stat`, `list`, `exists`, `update` (force-reconcile). `rename` rejects on casing-differ collision; case-only renames are allowed on insensitive FS by short-circuiting the exists check. `process` skips the write if `mutator(text) === text`.
  - Trash: `trashSystem` → Electron IPC; `trashLocal` → moves into `<vault>/.trash/` with ` 2`, ` 3`, … collision suffixes (preserves extension).
  - **`applyWriteOptions({ctime, mtime, immediate})`** — `btime` for ctime (silently skipped if unavailable); `utimes` for mtime (atime clobbered to same value); `immediate()` fires synchronously post-write. The `immediate` hook is how `Vault.modify` keeps the file cache in sync atomically with the write.
  - Resource URLs: `getResourcePath` → `app://local/<file-url>?<mtime>` (Windows UNC → prefixed `%5C%5C`). `getFilePath` → `file://` URL.
  - Watching: `watch(handler)` installs a recursive watch from root then runs `listAll()`. macOS/Windows use a single recursive `fs.watch`; Linux installs per-directory via `startWatchPath`. `watchHiddenRecursive(path)` — Linux-only; walks hidden dirs manually (used for `.obsidian`). `stopWatch`/`startWatchPath`/`stopWatchPath`.
  - Reconciliation: `onFileChange(fullPath)` queues a `reconcileFile` on 0-ms setTimeout (coalesces bursts). `reconcileFile` diffs disk vs `files` and fires `raw` + one of `file-created`/`folder-created`/`modified`/`file-removed`/`folder-removed`/`renamed`/`closed`. Sub-cases handle files, folders, symlinks, deletions.
  - Path machinery: `getRealPath` / `getFullPath` / `getFullRealPath` — virtual→real→absolute indirection for symlinked folders.
  - Static helpers: `readLocalFile(absPath)`, `mkdir(absPath)` — any-path, used by import.
- **Lifecycle:** one per open vault; call `stopWatch()` before dropping the reference.

### `CapacitorAdapter`

- **Kind:** class; mobile implementation of the `DataAdapter` interface.
- **Constructor:** `new CapacitorAdapter(basePath: string, fs: CapacitorFileSystem)`. `fs` is a Capacitor plugin object — a subset interface exposing `watchAndStatAll`, `watch`, `readdir`, `mkdir`, `trash`, `rename`, `readFile`, `writeFile`, `appendFile`, `stat`, `open`, `getResourcePath`, `getNativePath`.
- **Divergence from `FileSystemAdapter`:**
  - `insensitive = false` (no probe — mobile file systems are treated case-sensitive).
  - `watch` replaced by `watchAndList` which uses a one-shot "watch + stat everything" native call.
  - `trashLocal` moves to `.trash/` without a preserved extension suffix (name-space-$n scheme instead of `<base>_$n.<ext>`).
  - No `btime` / ctime-setting. No IPC.
  - Has `getNativePath(relPath)` and `open(relPath)` (mobile-only: open via OS share sheet / default handler).
  - No `watchHiddenRecursive` — the native watcher covers `.obsidian` automatically.
  - Errors "Directory exists" are swallowed in `mkdir`.
- **Corbomite note:** Corbomite is desktop-only; this adapter is audited for API shape only. All semantics Corbomite needs to match come from `FileSystemAdapter`.

### `normalizePath(s: string): string`

```js
tu(uu(s)).normalize("NFC")
```

- `uu(s)` — **path-character collapser** (external helper): replaces backslashes with `/`, collapses `//+` to `/`, trims trailing slashes except at root, removes leading `./`. Does **not** resolve `..`.
- `tu(s)` — strips some low-ASCII / line-separator chars (inferred from callers).
- `.normalize("NFC")` — Unicode NFC normalisation.

**Contract:** every path handed to `Vault` / `adapter` passes through this. A vault-format-critical invariant — see Section 8.

### `getLinkpath(linktext: string): string`

Returns the substring before the first `#` (or the whole string if no `#`). Used to strip `#Heading` and `#^block` suffixes.

### `parseLinktext(linktext: string): { path: string; subpath: string }`

Splits at the first `#` (inclusive — `subpath` retains the leading `#`). `subpath` is `""` when there is no `#`.

---

## 2. Data structures

### `FileMap`

```typescript
// Vault.fileMap
type FileMap = {
  "/": TFolder; // always present (root)
  [normalizedPath: string]: TFile | TFolder;
};
```

- Keys are NFC-normalised, `/`-separated, root-relative paths (no leading `/` except the root key).
- Invariant: every `TFolder`'s `children` list contains exactly the entries whose `parent` is that folder; `getDirectParent` assumes both sides are in sync.

### `AdapterFileEntry` (internal to adapters)

```typescript
type AdapterFileEntry =
  | {
      type: 'file';
      realpath: string; // actual on-disk path (may differ from vault path due to symlinks)
      ctime: number;    // ms since epoch, birthtime on FS that support it
      mtime: number;    // ms since epoch (rounded integer)
      size: number;     // bytes
    }
  | {
      type: 'folder';
      realpath: string;
    };
```

Used by both adapters' `files: Record<vaultPath, AdapterFileEntry>`. `realpath` is the mechanism for supporting symlinked sub-folders.

### `FileStat` (public)

```typescript
type FileStat = {
  ctime: number;
  mtime: number;
  size: number;
  // `type: 'file' | 'folder'` is present on adapter.stat() but stripped off on TFile.stat
};
```

Attached to `TFile.stat`. `null` until the first reconcile.

### `DataWriteOptions`

```typescript
type DataWriteOptions = {
  ctime?: number;           // ms. Desktop sets via btime; silently ignored where btime unavailable
  mtime?: number;           // ms. Desktop calls utimes; atime is clobbered to the same value
  immediate?: () => void;   // sync-post-write callback; Vault.modify uses this to sync file.cache
};
```

### `VaultConfig`

Union of keys written to `.obsidian/app.json` and `.obsidian/appearance.json`. The split is driven at `saveConfig` time by `AC.contains(key)` (appearance) vs everything else (app). `AC` lives outside this domain. Keys confirmed via grep of `setConfig`/`getConfig` sites in the whole `obsidian/` tree:

```typescript
// .obsidian/app.json — vault-critical subset; full list grouped:
type AppConfig = {
  // File & Link (vault-format-critical for Corbomite)
  newFileLocation?: 'root' | 'current' | 'folder';
  newFileFolderPath?: string;              // vault-absolute
  attachmentFolderPath?: string;           // '.'/'./' = same folder; './sub' = relative; else vault-absolute
  alwaysUpdateLinks?: boolean;
  useMarkdownLinks?: boolean;              // false=wiki [[...]], true=markdown [](...)
  newLinkFormat?: 'shortest' | 'relative' | 'absolute';
  trashOption?: 'system' | 'local' | 'none';
  promptDelete?: boolean;
  deleteUnlinkedAttachments?: 'always' | 'ask' | 'never';
  showUnsupportedFiles?: boolean;
  userIgnoreFilters?: string[];            // glob patterns

  // Editor / UI / Mobile — documented in detail in respective domain audits
  defaultViewMode?: 'source' | 'preview' | 'live';
  livePreview?: boolean;
  readableLineLength?: boolean; showLineNumber?: boolean; showInlineTitle?: boolean;
  foldHeading?: boolean; foldIndent?: boolean; showIndentGuide?: boolean;
  rightToLeft?: boolean; spellcheck?: boolean; spellcheckLanguages?: string[] | null;
  autoPairBrackets?: boolean; autoPairMarkdown?: boolean; smartIndentList?: boolean;
  useTab?: boolean; tabSize?: number; strictLineBreaks?: boolean;
  propertiesInDocument?: 'visible' | 'hidden' | 'source';
  autoConvertHtml?: boolean; vimMode?: boolean;
  showRibbon?: boolean; showViewHeader?: boolean;
  openBehavior?: 'default' | 'tab' | 'split' | 'window'; focusNewTab?: boolean;
  slidingSidebar?: boolean; floatingNavigation?: boolean; autoFullScreen?: boolean;
  nativeMenus?: boolean; uriCallbacks?: boolean;
  mobileQuickRibbonItem?: string; mobilePullAction?: string; mobileToolbarCommands?: string[];
  pdfExportSettings?: Record<string, unknown>;
};

// .obsidian/appearance.json — routed via AC allow-list at save time
type AppearanceConfig = {
  theme?: 'system' | 'obsidian' | 'moonstone' | string;  // string = community theme id
  cssTheme?: string;
  accentColor?: string;                    // hex or ''
  baseFontSize?: number;
  baseFontSizeAction?: 'increase' | 'decrease' | '';
  textFontFamily?: string;                 // migrated from legacy `editorFontFamily`
};
```

**Invariants on this shape:**

- Defaults come from `PC[key]` (in `settings` domain); missing keys are *valid* — never assume a key is present.
- On first read, `setupConfig` migrates `editorFontFamily` → `textFontFamily` (deletes the old key unless the new one already exists). This is the only rename I observed in this domain.
- **Unknown keys are preserved on write** via `Object.assign({}, appearance, app)` merge; Corbomite writes must do the same to avoid stripping keys added by plugins or future Obsidian versions.

### `VaultEventPayloads`

```typescript
type VaultEvents = {
  'create':         (file: TAbstractFile) => void;
  'modify':         (file: TAbstractFile) => void;
  'delete':         (file: TAbstractFile) => void;    // file.deleted === true, file.path is the last-known path
  'rename':         (file: TAbstractFile, oldPath: string) => void;
  'closed':         () => void;
  'raw':            (path: string) => void;            // fires on any FS event inside vault (incl. .obsidian/)
  'config-changed': (key: string) => void;
};
```

- `rename`'s second arg is the **old path**. The file object's own `path` field reflects the new path at the moment of the callback.
- `raw` is a fine-grained notification intended for plugins that want to watch files inside `.obsidian/` (plugin data, community CSS) that the `fileMap` abstraction ignores.

---

## 3. On-disk contracts

### Vault root (all files under `<vaultRoot>/`)

- **Path convention:** NFC-normalised, `/`-separated, root-relative. `/` is the root; all other paths lack a leading slash. The adapter translates to the host-native separator on Windows via Node's `path.join`.
- **Case sensitivity:** Probed once per session by writing `.OBSIDIANTEST` + reading `.obsidiantest`. The result is cached on the adapter's `insensitive` field; `Vault.getAbstractFileByPathInsensitive` and `adapter.exists(path, caseInsensitiveConfirm=true)` use it.
- **Ignored names:** `ru(path)` (not in this domain; audited as a utility) filters reserved names: everything under `configDir` is excluded from the public `fileMap`, and most dotfiles are hidden except under `configDir`. Corbomite's vault scanner must match the same rule-set to avoid exposing `.git/`, `.DS_Store`, etc. as notes.
- **BOM handling:** `Vault.read` strips a leading U+FEFF before returning. `Vault.modify` does **not** add one back. Corbomite should match.

### `.obsidian/` (the config directory)

- **Path:** defaults to `.obsidian`, overridable per-vault via `Vault.setConfigDir(name)` (which stores nothing itself — it's set at app boot by the settings layer). Must start with `.` and satisfy the safe-path regex `UT`; else reverts to `.obsidian`.
- **Creation:** `setupConfig` and `saveConfig` create it on demand (`mkdir -p`-equivalent).
- **Watched:** `FileSystemAdapter.load` calls `watchHiddenRecursive(configDir)` on Linux (macOS/Windows recursive watcher handles it). Every `raw` event inside `configDir/` re-installs watchers (to cover newly-created subdirs) and, for `app.json`/`appearance.json`, triggers `reloadConfig` (debounced 500 ms) which fires `config-changed` per affected key.

### `.obsidian/app.json` and `.obsidian/appearance.json`

- **Written by:** `Vault.saveConfig` (debounced 1 s leading-edge via `requestSaveConfig`, fired by every `setConfig`).
- **Read by:** `Vault.setupConfig` (boot) and `Vault.reloadConfig` (external-edit detection, debounced 500 ms).
- **Schema:** see `AppConfig` / `AppearanceConfig` types in Section 2. Partition is `AC.contains(key) ? appearance : app` at write time. `AC` is defined outside this domain (likely `settings/SettingTab.js`).
- **Format:** `JSON.stringify(obj, undefined, 2)` — 2-space indent, no trailing newline. Key order is `Object.keys` insertion order (unstable; tolerate any order on read).
- **Lifecycle:** created on first `saveConfig`; absence is valid = all-defaults. No version field; migration is "present-key implies known-key".
- **Migration / unknown keys:** only rename observed here is `editorFontFamily → textFontFamily` in `setupConfig`. **Unknown keys are preserved** on write — `reloadConfig` merges via `Object.assign({}, appearance, app)`; Corbomite writes must do the same.

### `.obsidian/plugins/<pluginId>/data.json`

- **Written by:** `Vault.writePluginData(pluginDir, data, opts)` via `Plugin.saveData(data)`. **Read by:** `Vault.readPluginData(pluginDir)` via `Plugin.loadData()`.
- **Schema:** arbitrary plugin-defined JSON. Absence returns `null`. Path is `normalizePath(pluginDir + '/data.json')`; `pluginDir` = `.obsidian/plugins/<id>` supplied by the plugin loader.

### `.obsidian/<name>.json` (general config files)

- **Interface:** `Vault.readConfigJson(name)` / `Vault.writeConfigJson(name, obj, opts)` / `Vault.deleteConfigJson(name)` — same JSON-formatting invariants as `app.json`.
- **Known consumer names** (schemas defined in their owning domains): `core-plugins.json` (settings), `community-plugins.json` (plugin), `hotkeys.json` (settings), `workspace.json` / `workspace-mobile.json` / `workspaces.json` (workspace), `graph.json` (graph plugin), `bookmarks.json` (bookmarks plugin). Each domain's Pass 2 audit supplies the exact schema.

### `.trash/` (vault-local trash)

- **Written by:** `FileSystemAdapter.trashLocal` and `CapacitorAdapter.trashLocal`.
- **Read by:** nobody in core (it's a plain folder the user can restore from by moving files out manually).
- **Path convention:** `.trash/<basename><suffix>.<ext>`. Suffix is `""` for the first copy; `" 2"`, `" 3"`, … for subsequent collisions. Desktop preserves the original extension; mobile appends ` N` without an extension separator.
- **Lifecycle:** created lazily on first trash operation; never purged by Obsidian.

### `.OBSIDIANTEST` (transient)

- **Written by:** `FileSystemAdapter.testInsensitive` (constructor).
- **Read by:** same function.
- **Lifecycle:** written at construction, unlinked immediately. Best-effort — a stale `.OBSIDIANTEST` after a crash is auto-cleaned by the next boot. Corbomite should adopt either the same probe file name (for cross-tool coexistence) or pick a differently named probe file; the name itself is not load-bearing.

### Note files (`*.md`, `*.canvas`, `*.base`, binary attachments, etc.)

- **Format:** `Vault` is format-agnostic; content schema is owned by the extension's `View` (audited in `editor/markdown`, `canvas` plugin, `bases` respectively).
- **Per-file state:** content cache in `XT: WeakMap<TFile, string>` when `size ≤ cacheLimit`; `stat` (`ctime`/`mtime`/`size`) populated by the adapter's reconciler; `saving` flag suppresses auto-invalidation during self-caused `modified` events.

---

## 4. Events emitted

### `Vault` (extends `Events`)

| Event name | Payload (inferred) | Triggered when | Typical consumers |
|---|---|---|---|
| `create` | `(file: TAbstractFile)` | `onChange('folder-created', path)` (Vault.js:440) and `onChange('file-created', path, _, stat)` (Vault.js:446) | `MetadataCache` to start indexing; `Workspace`; file-explorer plugin |
| `modify` | `(file: TAbstractFile)` | `onChange('modified', …)` after adapter reports stat change (Vault.js:451). Skipped when `file.saving` — but `trigger('modify', ...)` still fires. | `MetadataCache` (re-parse), `MarkdownRenderer` (re-render), `QueryController` (re-run queries), `TextFileView` (detect external edits) |
| `delete` | `(file: TAbstractFile)` — with `file.deleted = true`, `file.parent = null` after `removeChild` | `onChange('file-removed' \| 'folder-removed', path)` (Vault.js:458) | `MetadataCache` (emit its own `deleted` with last cache), `Workspace` (detach leaves), `MarkdownRenderer` |
| `rename` | `(file: TAbstractFile, oldPath: string)` | `onChange('renamed', newPath, oldPath)` (Vault.js:468) | `Workspace` (Workspace.js:289, WorkspaceLeaf.js:2378, WorkspaceTabs.js:2378 — update tab titles), `MetadataCache` (rebuild link tables), `FileManager` (after `rename`, runs link-update queue but that is *driven by `runAsyncLinkUpdate`*, not by listening to this event) |
| `closed` | `()` | `onChange('closed')` at vault-close (Vault.js:472) or when root is deleted underneath us (via adapter `reconcileDeletion` on `/`) | `App` (tear-down plugins, views) |
| `raw` | `(path: string)` | Any FS event reported by the adapter — including `.obsidian/` internals (Vault.js:474). Fires in addition to the semantic events above for paths inside `fileMap`; fires **only** `raw` for paths inside `configDir/` since those are not tracked in `fileMap`. | Plugins that watch their own `data.json` for external edits; sync/publish |
| `config-changed` | `(key: string)` | `setConfig(key, value)` (Vault.js:290); `reloadConfig` after external `app.json`/`appearance.json` edit (Vault.js:163 for changes, Vault.js:167 for deletions) | Settings UI (live refresh), `App` (apply CSS theme, spellcheck lang, ribbon visibility) |

Event ordering contract: `create`/`modify`/`delete`/`rename` fire **after** `fileMap` has been updated. Consumers can safely query `vault.getAbstractFileByPath` inside the handler and see the post-change state. The `file` argument carries the same identity across `rename` (same object, new `path`) and is reused across re-creates only if `path` collides.

### `FileSystemAdapter` / `CapacitorAdapter` (private dispatch)

Adapters use a single `handler` callback, not `Events`. The internal events `raw`/`file-created`/`folder-created`/`modified`/`file-removed`/`folder-removed`/`renamed`/`closed` map 1:1 onto the public `Vault` events via `Vault.onChange` — not a plugin-visible surface. `renamed` is only fired by adapter-initiated `rename` and fires once per child on folder rename (FileSystemAdapter.js:595, 617).

`FileManager` emits **no events** of its own.

---

## 5. Events consumed

| Listener file | Subscribes to | Why |
|---|---|---|
| `Vault.onChange` (internal, bound to adapter via `adapter.watch(this.onChange.bind(this))`) | adapter's `raw`/`file-created`/`folder-created`/`modified`/`file-removed`/`folder-removed`/`renamed`/`closed` | promotes them to public `create`/`modify`/`delete`/`rename`/`closed`/`raw` events after updating `fileMap` |
| `Vault.reloadConfig` (debounced) | own `raw` fires on `.obsidian/app.json` or `.obsidian/appearance.json` | forces a re-read and fires `config-changed` per diff |
| `Vault.load` → `adapter.watchHiddenRecursive(configDir)` (indirect) | FS events inside `.obsidian/` | Linux needs per-dir watchers for dotfiles to fire |
| `FileManager.runAsyncLinkUpdate` | **does not subscribe to `Vault`'s `rename`** — it wraps `vault.rename` inside its own queue and calls `metadataCache.updateInternalLinks` directly | this is the authoritative place to register "I renamed X, fix every pointer". Plugins wishing to hook rename refactoring should call `fileManager.renameFile`, not `vault.rename`. |

No other `Vault` self-subscriptions.

---

## 6. Commands registered

No commands registered in this domain. `FileManager.promptForFileRename`, `promptForDeletion`, etc. are called by commands registered elsewhere (`File Explorer` internal plugin and `core/App.js` bootstrap).

---

## 7. Registries owned

### `FileManager.fileParentCreatorByType`

- **Stores:** `Record<extension, (hintPath: string) => TFolder>`.
- **Populated by:**
  - `FileManager` constructor — registers the `.md` default (`getMarkdownNewFileParent`).
  - Plugin-registered: `Plugin.registerFileParentCreator(ext, fn)` is **not** a documented public API; only internal registration via `fileManager.registerFileParentCreator` exists. The built-in `canvas` and `bases` internal plugins register their own `.canvas` / `.base` creators. External plugins can call `app.fileManager.registerFileParentCreator(ext, fn)` but the symbol is not part of the stable plugin API surface — audit `plugin/Plugin.js` (separate domain) for the thin wrapper.
- **Read by:** `FileManager.getNewFileParent`, `FileManager.createNewFile`.
- **Persistence:** in-memory only.
- **Lifecycle:** entries live for app lifetime; `unregisterFileCreator(ext)` removes.

### `Vault.fileMap`

- **Stores:** `Record<vaultRelativePath, TFile | TFolder>`.
- **Populated by:** `Vault.onChange` reacting to adapter events; initial populate via `adapter.watch` → `listAll` → per-entry `reconcileFile`.
- **Read by:** everything.
- **Persistence:** in-memory; derived deterministically from disk scan on startup. Not persisted.
- **Lifecycle:** cleared on vault close.

### `FileManager.updateQueue`

- **Stores:** a single-consumer serialised queue of link-update transactions.
- **Semantics:** every renameFile call awaits completion of the previous one, eliminating the possibility of two renames both running snapshot → mutate → apply against a common `metadataCache` state.

---

## 8. Invariants

- `normalizePath(p)` is idempotent; apply to every user-supplied path. Corbomite must implement NFC + `\\`→`/` + collapse-slashes + trim + strip-leading-`./` identically or cross-tool interop breaks on non-ASCII names.
- Root `TFolder.path === "/"`; `TFolder.isRoot()` is the only safe root test. `getParentPrefix()` returns `""` for root, else `path + "/"` — use this to build child paths, never string-concat with `/`.
- `TFile.path` is NFC and `/`-separated on all OSes. `TFile.extension` is lowercase, no leading dot (`'md'`, not `'.md'`). `TFile.basename` strips only the *last* `.`-delimited suffix (`Archive.tar.gz` → `basename='Archive.tar'`, `extension='gz'`).
- `TAbstractFile.parent` is `null` only for root or for `deleted` objects. `deleted` is monotonic — once set, the object is unreachable via `fileMap`.
- Vault writes (`modify`/`create*`/`writeBinary`/`append*`/`process`) are **per-adapter queued**; no races. `getConfig`/`setConfig` are synchronous; disk write is debounced 1 s leading-edge.
- `Vault.modify(file, text)` sets `file.saving = true` around the adapter call; the `immediate` hook syncs `file.cache` atomically so the self-caused `modified` event does **not** flush the cache ("save-my-edit doesn't flush my own cache").
- `Vault.read` strips leading U+FEFF BOM and populates cache. `Vault.modify` stores text verbatim — never re-adds a BOM.
- `Vault.delete` is permanent. `Vault.trash` is reversible. Policy selection (`system` / `local` / `none`) is in `FileManager.trashFile`, not `Vault`.
- `Vault.rename` does **not** rewrite links. Link-safe rename requires `FileManager.renameFile`. Corbomite's API must preserve this distinction.
- `Vault.copy` is recursive for folders; collisions throw (`COPYFILE_EXCL`).
- `Vault.getAvailablePath(base, ext)` returns a path whose *case-insensitive* form is free — `Note.md` can't be clobbered by `note.md`.
- `Vault.on('rename', cb)` fires *after* `fileMap` is updated. `cb(file, oldPath)`: `file.path` is the new path; `oldPath` is the second arg. Both are needed.
- `FileManager.processFrontMatter(file, mut)` preserves frontmatter key order via `yI` ordered-assign. Deleted keys disappear; added keys append at the end. Corbomite's frontmatter-write must match.
- `FileManager.generateMarkdownLink`'s output is a pure function of `{useMarkdownLinks, newLinkFormat, MetadataCache filename index}`. Corbomite must match byte-for-byte to produce semantically-equivalent links.
- `configDir` must start with `.`, pass regex `UT`, and not equal `.`. Anything else silently reverts to `.obsidian`.
- `Vault.exists(path, insensitive)` with `insensitive=true` does a `readdir` double-check on insensitive FS; no-op on Linux.
- `adapter.files` is the disk-truth map; `Vault.fileMap` derives from it but filters via `ru(path)`. Any path starting with `configDir + "/"` never enters `fileMap` and fires only `raw` — plugins must subscribe to `raw` to observe `.obsidian/` internals.

---

## 9. Observable user features

- Open a folder as a vault; live file-tree updates when files change in the OS file manager.
- Rename-with-refactor: renaming a note rewrites every `[[Foo]]` / `[Foo](Foo.md)` across the vault; user confirms unless `alwaysUpdateLinks` is on.
- Configurable trash policy (`trashOption`: system / local `.trash/` / permanent). Pre-delete dialog warns on non-empty folders, shows backlink count, "don't ask again" available.
- Orphan-attachment cleanup on note delete (`deleteUnlinkedAttachments`: always / ask / never).
- Attachment-folder policy (`attachmentFolderPath`): "same folder" (`.` / `./`), "subfolder under current note" (`./sub`), or vault-absolute path.
- Wiki-vs-Markdown links toggle (`useMarkdownLinks`) + new-link-format (`shortest` / `relative` / `absolute`).
- Plugin-safe atomic frontmatter edit (`FileManager.processFrontMatter`): mutate-in-place JS object; Obsidian handles YAML round-trip and preserves key order.
- Preserve unknown frontmatter and config keys on edit (never strip them).
- Download remote `http(s)://` / `data:` images in a note and localise them as attachments.
- External-edit compatibility: `.obsidian/*.json` changes (git pull, direct edit) picked up within ~500 ms; `config-changed` refreshes UI live.
- Case-preserving rename on case-insensitive FS (`Foo.md` → `foo.md`).
- Symlinked subfolders inside the vault resolve transparently via `getRealPath`.
- Undoable bulk operations: a 30-s "Undo" toast after any bulk rewrite restores content + mtime.

---

## 10. Extension surfaces exposed

| Surface | Registration verb | Consumer | What plugins supply |
|---|---|---|---|
| New-file parent per extension | `app.fileManager.registerFileParentCreator(ext, fn)` | `FileManager.getNewFileParent` (FileManager.js:510) | `(hintPath) => TFolder` |
| Frontmatter mutate API | `app.fileManager.processFrontMatter(file, mut, opts?)` (method) | FileManager.js:1622 | `(fm: object) => void` mutator; body preserved verbatim |
| Bulk-rewrite Undo toast | `app.fileManager.notifyForBulkUndo(backups, ms?)` | FileManager.js:1693 | `{file, content, mtime}[]` |
| Content cache limit | `vault.setFileCacheLimit(bytes)` | Vault.js:188 | integer |
| File observer | `vault.on('create'\|'modify'\|'delete'\|'rename'\|'raw', cb)` | Primary plugin hook for file-awareness | listener |
| Config observer | `vault.on('config-changed', cb)` | Settings UIs | listener |
| Config read/write | `vault.getConfig(key)` / `vault.setConfig(key, v)` | Plugins writing `.obsidian/*.json` | string + JSON value |
| Plugin data JSON | `vault.readPluginData(dir)` / `vault.writePluginData(dir, data, opts)` | `Plugin.loadData`/`saveData` | JSON-ifiable object |
| Adapter downcast | `vault.adapter instanceof FileSystemAdapter` | Plugins needing `getBasePath`, `getFullPath`, `getResourcePath` | n/a |

**Primary** plugin extension: the event quad `on('create' \| 'modify' \| 'delete' \| 'rename', cb)`. Every file-aware plugin subscribes.

---

## 11. Corbomite mapping

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `TFile` / `TFolder` / `TAbstractFile` | `libs/core/NoteMeta` (flat `.md` struct) | Partial | No general file-node class; no `TFolder`. Need Qt tree with parent linkage. |
| `Vault` (class + events + fileMap + config) | `libs/storage/VaultScanner` + `src/app/VaultService` (no fileMap owner) | **Missing major piece** | No in-memory tree; no event fan-out. Need `QObject`-derived `Vault` with Qt signals mapping 1:1 to JS event names. No `configDir` handling today. |
| `FileSystemAdapter` | `libs/storage/FileSystemAdapter` | Partial | Has `readFile`/`writeFile`/`rename`/`remove`/`moveToTrash`/`exists`/`mkpath`. Missing: `stat`, `list`, `readBinary`/`writeBinary`, `append*`, `process`, `copy`, `rmdir`, `getResourcePath`, `watch(handler)`, `watchHiddenRecursive`, case-sensitivity probe, write queue, `immediate` hook, ctime/mtime preservation. `QFileSystemWatcher` handles most watching. |
| `FileManager` | — (scattered in `VaultService`) | **Missing major piece** | No rename-with-link-refactor, `processFrontMatter`, `generateMarkdownLink`, trash-policy router, attachment-folder resolver, bulk-undo notice. Needs new `libs/filemanager/`. |
| `normalizePath` / `getLinkpath` / `parseLinktext` | Unknown / implicit in `libs/markoff-parser/` | **Missing** | Add `libs/core/PathNormalize.{h,cpp}` implementing `tu(uu(s)).normalize('NFC')` exactly. The two one-liner linktext helpers belong in the same header. |
| `Vault.getConfig`/`setConfig` + `.obsidian/app.json` + `.obsidian/appearance.json` | `KConfig` + `~/.config/corbomiterc` | **Incompatible** | Corbomite's current settings live **outside the vault**. Hard compat block — must read `.obsidian/app.json` for per-vault keys. Introduce a `VaultConfig` layer reading/writing `.obsidian/*.json`; keep `corbomiterc` for Corbomite-only settings. |
| `Vault.on('config-changed', key)` | — | Missing | Need a Qt signal `vaultConfigChanged(QString key)` with live external-edit refresh. |
| `.obsidian/plugins/<id>/data.json` | — | N/A | Corbomite has no plugin system yet; reserve the read/write path. |
| `.obsidian/workspace*.json` | — | Missing | Workspace persistence — covered by the `workspace` domain audit. |
| `.trash/` convention | — | **Missing** | `moveToTrash` only handles system trash. Need `trashOption` equivalent + `.trash/<base><suffix>.<ext>` writer for `local` mode. |
| `TFile.cache` WeakMap | — | Missing | Low priority optimisation, not compat-critical. |
| `.obsidian/` recursive watcher on Linux | `QFileSystemWatcher` (inotify, non-recursive by default) | Partial | Match Obsidian's per-directory install to pick up nested plugin data dirs. |
| `Vault.process(file, mut)` atomic RMW | — | Missing | Required for safe concurrent frontmatter edits. Expose `NoteDocument::processAtomic(std::function<QString(const QString&)>)`. |
| `Vault.modify(file, text, {ctime, mtime, immediate})` | `FileSystemAdapter::writeFile` | Partial | No ctime/mtime preservation, no `immediate` hook. External-edit detection and mtime-sensitive sync won't work correctly. |
| `Vault.getAvailablePath(base, ext)` | — | Missing | Required for paste-attachment + new-untitled-note collision avoidance. |
| Case-sensitivity probe | — | Missing | Without it, `new file.md` vs `New File.md` on macOS confuses the in-memory model. |

**Immediate Corbomite TODOs:**

1. `Corbomite::Vault` (`libs/vault/` new lib or `libs/core/`): NFC-normalised path→node map, Qt signals for `fileCreated`/`fileModified`/`fileDeleted`/`fileRenamed`/`rawChange`/`configChanged`/`vaultClosed`. `QFileSystemWatcher` + initial scan from `VaultScanner`.
2. `PathNormalize::normalize(const QString&)` in `libs/core/` — NFC + slash-collapse + `./`-strip. Used at every library boundary.
3. Split `FileSystemAdapter` into a `DataAdapter` interface; add `stat`, `list`, `readBinary`, `writeBinary`, `process`, `copy`, `rmdir`, `getResourcePath`.
4. `Corbomite::FileManager` for rename-with-refactor, frontmatter mutation, trash-policy routing, attachment-folder resolution. Depends on `libs/storage/SQLiteIndex` (the `MetadataCache` analogue) for link lookup.
5. Per-vault `VaultConfig` reading/writing `.obsidian/app.json` and `.obsidian/appearance.json` with the exact split and defaults from Section 2. **Single biggest compat-critical unit — Pass 3's top recommendation should gate feature work on this.**

---

## 12. Markoff gap confirmations / discoveries

N/A — no editor/rendering surface in this domain. Vault events funnel into Markoff via `MarkdownRenderer` (confirmed subscriptions in `editor/markdown/MarkdownRenderer.js:28-29` — `modify`/`delete` on vault triggers re-render) but that is Markoff's concern, not this domain's.

---

## 13. Open questions

1. **Where does `AC` (appearance-keys allow-list) live?** Inferred from `saveConfig` splitting behaviour but its content never appears in this domain. Likely `settings/SettingTab.js`. Corbomite's `VaultConfig` must partition identically.
2. **Where is the `PC` defaults table?** Returned by `vault.getConfig(key)` on missing keys. Every key in `AppConfig` above needs its default documented.
3. **Regexes `UT` / `WT` / `GT` / `KT`** — governing illegal filename chars, reserved names, linktext validation, path-safety respectively. Exact character sets are required for cross-tool path compat.
4. **`ru(path)` — hidden-path predicate:** does it parameterise on `configDir`? A vault with custom `configDir = '.my-obsidian'` should still have it filtered from `fileMap`; behaviour with the default name is confirmed but the parameterisation isn't.
5. **Config key casing discipline.** `setConfig` is key-sensitive on raw equality; an Obsidian vault authored with `"alwaysUpdateLinks"` vs a Corbomite write of `"AlwaysUpdateLinks"` would produce two entries. Confirm: are all keys documented here the canonical casing?
6. **Folder rename event cardinality.** Adapter emits `renamed` per-child (FileSystemAdapter.js:617), so a folder rename fires **N+1** `rename` events on `Vault`. Consumers must be idempotent. Needs explicit test-vault confirmation.
7. **`.trash/` retention** — no purge path observed. Presumably a user-visible cleanup tool lives in a settings tab; confirm in settings-domain audit.
8. **`Vault.readBinary` for `.md`** populates the cache on every binary read (unconditional `file.cache(...)` inside the try). A prior modify leaving a truncated cache would be overwritten with on-disk view on next binary read — likely desired but worth confirming. Corbomite cache policy should match explicitly.
9. **`Vault.closed` trigger.** `onChange('closed')` fires only when adapter reports `/` deleted. No explicit `Vault.close()` method exists in this domain. Likely pattern: app tears down via `adapter.stopWatch()` then drops references. Corbomite's `VaultService::closeVault` should fire `vaultClosed` independently regardless.
10. **Double-extension semantics of `TFile.basename`/`extension`** — `su(path)` appears to take only the last extension (`Archive.tar.gz` → `extension='gz'`). Confirm against a file with `.md.bak`.
11. **The `250`-byte basename truncation in `getAvailablePathForAttachments`.** Deliberate (NAME_MAX margin) or arbitrary? Corbomite should mirror whichever.
12. **`AC`/`PC` source:** Pass 3 synthesis should open a blocker on these before declaring the on-disk contract complete.

---

## 14. Recommended Pass 3 synthesis input

1. **`.obsidian/app.json` + `.obsidian/appearance.json` schemas.** These two files are the single largest vault-format compat burden in this domain. Pass 3 `VAULT-FORMAT.md` must promote the full `AppConfig`/`AppearanceConfig` table (Section 2) with defaults resolved from the still-to-find `PC` constants table. **Block any Corbomite settings-persistence feature work until Corbomite reads and writes these per open vault.**
2. **`FileManager.renameFile` + `FileManager.processFrontMatter` are the two load-bearing atomic operations plugins rely on.** Pass 3 `GAP-ANALYSIS.md` should mark "rename-with-link-refactor" and "atomic frontmatter mutation" as separate, high-priority compat items — the first for user-facing correctness (renaming a note must not break back-links), the second for plugin-API forward-compatibility.
3. **The `Vault.on('create'|'modify'|'delete'|'rename'|'raw'|'config-changed')` event contract is the plugin-facing primary API.** Pass 3 `FEATURE-MATRIX.md` should record the exact event names, payloads, and fire-order (post-mutation) as canonical — any future Corbomite plugin API shim must preserve them 1:1 even when the underlying implementation uses Qt signals.
