# `obsidian/metadata` — per-file metadata index, link graph, and tag registry

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/metadata/`
**File count:** 4
**Files:** `MetadataCache.js`, `getAllTags.js`, `iterateCacheRefs.js`, `iterateRefs.js`

**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> Background indexer. Spawns `worker.js`, parses every markdown file into a `CachedMetadata` record (links, embeds, tags, headings, sections, frontmatter, footnotes, blocks). Maintains `resolvedLinks` and `unresolvedLinks` reverse maps. Persists to IndexedDB. Emits `changed`, `deleted`, `resolve`, `resolved`, `finished` events that the rest of the app subscribes to.

**De-minifier artifact note:** Four distinct md5 hashes — no duplicate extraction. Each file carries `// public API symbol:` and exact `// source: app.js lines <a>-<b>` comments: `MetadataCache.js` 95633-96725 (1092 declared, 1098 on disk — the trailing `})(Events);` close fits exactly, no bleed-over), `getAllTags.js` 79780-79796, `iterateCacheRefs.js` 79759-79764, `iterateRefs.js` 79773-79779 (the latter three hoisted near `obsidian/vault/getLinkpath.js`). Many short symbols (`xT`, `qL`, `BL`, `NL`, `UL`, `VL`, `HL`, `zL`, `$A`, `ub`, `vT`, `wT`, `kT`, `mT`, `gT`, `yc`, `sc`, `vb`, `wb`, `Cb`, `Zw`, `GL`, `nu`, `au`, `su`, `iu`, `oc`, `Sf`, `kf`, `parseFrontMatterTags`, `parseFrontMatterAliases`, `getLinkpath`, `TFile`, `Events`) are not defined in this domain; owners resolved in §15.

---

## 1. Public API surface

Four exported symbols: one large class, three small helper functions.

### `MetadataCache`

- **Kind:** class, extends `Events` (mixed by the `(function(e){…return t;})(Events)` IIFE wrapper, `metadata/MetadataCache.js:1097`).
- **Exported as:** `MetadataCache` (assigned to `App.metadataCache`).
- **Constructor:** `new MetadataCache(app, vault)` — `metadata/MetadataCache.js:6-44`. Immediately: creates the worker (`new Worker("worker.js", { name: "Metadata Cache Worker" })`), wires `worker.onmessage`, constructs a `BlockCache` via `qL`, starts the link-resolver generator loop, and subscribes itself to its own `finished`/`resolved` events for clean-cache draining.
- **Instance fields:**
  - `app`, `vault`, `worker` (the `worker.js` Worker), `blockCache` (`qL(app)`).
  - `workQueue: vb` — serial promise chain; `uniqueFileLookup: sc` — multi-map `lowercaseFilename → TFile[]`.
  - `fileCache: Record<path, {mtime, size, hash}>`; empty `hash` = "seen but not indexable".
  - `metadataCache: Record<hash, CachedMetadata>` — content-hash-keyed, so duplicate-content files share one entry.
  - `resolvedLinks: Record<src, Record<dst, number>>` (outgoing link counts).
  - `unresolvedLinks: Record<src, Record<normalisedLinkpath, number>>`.
  - `linkUpdaters: Record<ext, LinkUpdaterPlugin>` — per-extension ref/rewrite hooks (canvas, bases).
  - `linkResolverQueue: wb<TFile>` — generator-backed resolve worklist.
  - `inProgressTaskCount: number`; `didFinish = debounce(() => trigger("finished"), 10, leading=true)`.
  - `onCleanCacheCallbacks: (() => void)[]` — deferred "all idle" callbacks.
  - `userIgnoreFilters: RegExp[] | null` + memoisation cache.
  - `initialized`, `preloadPromise`, `db`, `transactionSave`, `workerResolve` — plumbing.
- **Lifecycle:** One instance per open vault, constructed by `App` during startup, before `App.workspace` finishes loading. `preload()` is awaited early so the in-memory `fileCache`/`metadataCache` are hydrated from IndexedDB. `initialize()` then reconciles against the `Vault`'s file list (spawning parse jobs for any file whose `mtime`/`size` differs from the cached entry), installs `vault.on('create'/'modify'/'delete'/'rename'/'config-changed')` listeners via `watchVaultChanges()`, sets `initialized = true`, triggers `finished`, and schedules a `cleanupDeletedCache()` sweep at T+60s plus a recurring one every 10 minutes. No explicit `unload` — when the vault closes, the worker is GC'd with the instance. `.obsidian`-scoped config change for `userIgnoreFilters` calls `updateUserIgnoreFilters`, which invalidates the filter memo.
- **Mixes in:** `Events` — exposes `on(event, cb, ctx)` and `trigger(event, …args)` (thin pass-throughs in `MetadataCache.prototype.on/trigger` at `metadata/MetadataCache.js:1087-1094`).

MetadataCache has ~35 prototype methods, grouped by role:

- **Public reads:** `getCache(path)`, `getFileCache(file)`, `getFileInfo(path)`, `getCachedFiles()`, `isSupportedFile(file)`, `isUserIgnored(path)`, `isUnresolved(linktext, sourcePath)`.
- **Public link resolution:** `getFirstLinkpathDest`, `getLinkpathDest`, `fileToLinktext` — see §2 + §8 for the algorithm.
- **Public aggregate queries:** `getLinks()` (`Record<src, (LinkCache|EmbedCache)[]>`), `getTags()` (`Record<tagString, count>` with subtag-prefix counts), `getBacklinksForFile`, `getLinkSuggestions`, `getAllPropertyInfos`, `getFrontmatterPropertyValuesForKey`.
- **Ref iteration:** `iterateAllRefs(cb)` walks merged refs via `xT(cache, cb)` then every `linkUpdater.iterateReferences`; `iterateRefsForFile(file, cb)` restricts to one file.
- **Mutation plumbing:** `updateInternalLinks(updates: Map<path, Update[]>)` delegates per-extension to `linkUpdaters[ext].applyUpdates` or falls back to `vault.process(file, text => UL(text, patches))`.
- **Persistence:** `preload`, `_preload` (IndexedDB `<appId>-cache` v19, stores `file` + `metadata`; version-bump destroys both stores — no migration), `clear`, `saveFileCache`, `saveMetaCache` (renames `frontmatterPosition → frontmatterPos` via `vT`), `cleanupDeletedCache` (GC orphan meta rows).
- **Vault reconciliation:** `watchVaultChanges`, `onCreate`, `computeFileMetadataAsync` (the main parser entry), `onDelete`, `deletePath`, `onRename`, `onConfigChanged`.
- **Link-resolver machine:** `linkResolver()` (async generator consumer), `queueFileForLinkResolution`, `resolveLinks(path)`, `updateRelatedLinks(changedShortNames)`.
- **Clean-cache latch:** `onCleanCache(fn)`, `checkCleanCache`, `isCacheClean`.
- **Worker protocol:** `work(buf)` posts `{metadataCache: data}` with `[data]` transfer list; returns `Promise<CachedMetadata>`. `computeMetadataAsync(buf)`, `onReceiveMessageFromWorker(e)`.
- **UI:** `showIndexingNotice` — ticks a `<progress>` from `inProgressTaskCount`.

### `getAllTags(cachedMetadata): string[] | null`

- **Kind:** function (exported as `getAllTags`). `metadata/getAllTags.js:5-21`.
- **Signature:** `(cache: CachedMetadata | null | undefined) => string[] | null`.
- **Purpose:** Concatenates all tags for a file: the frontmatter `tags:` values (normalised via `parseFrontMatterTags`, from `parsing/`) followed by the inline `#tag` occurrences from `cache.tags[]` (which yields raw `{tag, position}` entries — `getAllTags` pushes only the `tag` string). Preserves discovery order and **preserves duplicates**. Returns `null` iff `cache` is falsy.
- **Caller contract:** `MetadataCache.getTags()` feeds each result through a subtag-prefix decomposition loop to produce the canonical `Record<string, number>` tag count map. Plugins use it directly on `getFileCache(file)` to produce a per-file tag list.

### `iterateRefs(refs, cb): boolean`

- **Kind:** function (exported as `iterateRefs`). `metadata/iterateRefs.js:5-11`.
- **Signature:** `<T>(refs: T[] | undefined, cb: (ref: T) => boolean | void) => boolean`.
- **Semantics:** iterates `refs` (no-op + returns `false` if falsy). If any `cb(ref) === true`, stops early and returns `true`. Only literal `true` — any other truthy value does **not** break. Plugin authors often write `return true` to short-circuit; returning nothing means "continue".

### `iterateCacheRefs(cache, cb): boolean`

- **Kind:** function (exported as `iterateCacheRefs`). `metadata/iterateCacheRefs.js:5-10`.
- **Semantics:** `iterateRefs(cache.links, cb) || iterateRefs(cache.embeds, cb)`. Useful when a plugin wants to visit every outgoing reference from a file regardless of whether it's a `[[wikilink]]`, `[md](link)`, `![[embed]]`, or `![md](embed)`. **Does not include `tags`, `headings`, `frontmatterLinks`, `listItems`, or `blocks`.**

---

## 2. Data structures

### `FileCacheEntry`

```typescript
// Keyed by vault-relative path (normalised via `nu` — see vault/normalizePath).
{
  mtime: number;   // TFile.stat.mtime at parse-time (ms since epoch)
  size: number;    // TFile.stat.size at parse-time
  hash: string;    // 64-char hex SHA-256 of the file contents (empty "" for unsupported/non-md sentinels)
}
```

Invariants:
- `fileCache[path]` exists ⇒ the path has been observed at least once. Unsupported extensions are stored with `hash: ""` (no metadata) so the cache remembers it has seen the file.
- Two files with the same content hash share one `metadataCache[hash]` row. The fileCache is *N→1* onto `metadataCache`.

### `CachedMetadata`

The shape emitted by `worker.js`. Field usage reconstructed from callers — definitions do **not** live in this directory.

```typescript
// Offsets are 0-based byte offsets; lines/cols 0-based.
interface Pos { line: number; col: number; offset: number; }
interface Position { start: Pos; end: Pos; }

interface LinkCache {
  link: string;           // raw linkpath as parsed (pre-getLinkpath)
  original: string;       // verbatim source substring ("[[foo|bar]]" or "[x](y)")
  displayText?: string;   // alias if any
  position: Position;
}
// EmbedCache has the same shape; lives in cache.embeds not cache.links.

interface TagCache { tag: string; position: Position; }   // tag includes the leading "#"
interface HeadingCache { heading: string; level: 1|2|3|4|5|6; position: Position; }
interface SectionCache {                                   // type: "paragraph" | "heading" | "list" | "code" | "callout" | "yaml" | "table" | "math" | "html" | "blockquote" | "thematicBreak"
  type: string; position: Position; id?: string;           // id = block-id without the leading "^"
}
interface ListItemCache { position: Position; parent: number; task?: string; id?: string; }
interface FrontmatterLinkCache { link: string; original: string; displayText?: string; key: string; } // key: dotted path like "project" or "related.0"

interface CachedMetadata {
  links?: LinkCache[];
  embeds?: LinkCache[];
  tags?: TagCache[];
  headings?: HeadingCache[];
  sections?: SectionCache[];
  listItems?: ListItemCache[];
  footnoteRefs?: { id: string; position: Position }[];   // ^1, ^2 refs
  footnotes?: { id: string; position: Position }[];      // [^1]: definitions
  blocks?: Record<string, { id: string; position: Position }>;  // keyed by block-id
  frontmatter?: Record<string, unknown>;                  // raw YAML tree, original key casing
  frontmatterLinks?: FrontmatterLinkCache[];
  frontmatterPosition?: Position;                         // in-memory only; persisted as `frontmatterPos`
}
```

Footnote fields (`footnoteRefs`, `footnotes`) are inferred from general Obsidian plugin lore — not referenced in this file's code. See §13.

**Normalisation for persistence.** `saveMetaCache` renames `frontmatterPosition → frontmatterPos` via `vT(...)` and recursively prunes uninteresting position objects via `mT(cache, gT)` (`metadata/MetadataCache.js:887-902`); `_preload`'s `wT(a)` reverses the rename on hydrate, so in-memory API always exposes `frontmatterPosition`.

### `ResolvedLinkMap`

```typescript
// MetadataCache.resolvedLinks
// sourcePath → destPath → linkCount
Record<string, Record<string, number>>
```

Both `sourcePath` and `destPath` are vault-relative, normalised paths with extensions (so `Docs/Foo.md`, not `Docs/Foo`). A missing `dst` key means zero resolved links between that pair — not "unresolved". Absent source keys mean the file hasn't been link-resolved yet.

### `UnresolvedLinkMap`

```typescript
// MetadataCache.unresolvedLinks
// sourcePath → normalisedLinkpath → count
Record<string, Record<string, number>>
```

The linkpath is passed through `BL(linkpath)` before being used as the key (lowercase-normalise + strip heading — see §15). Plugins that enumerate this map should expect lowercase keys without `#heading` suffixes. `getLinkSuggestions` uses the presence here to surface "orphan target" autocompletes.

### `LinkSuggestion`

```typescript
{
  file: TFile | null;      // null for pure-unresolved suggestions
  path: string;            // vault-relative path (extension stripped for .md)
  alias?: string;          // frontmatter alias if any
}
```

Returned by `getLinkSuggestions()` — drives the wiki-link completion popup. File entries come first (alias duplicates appended), then unresolved-link paths trimmed to 500 chars max.

### `PropertyInfo`

```typescript
{
  name: string;            // original casing from frontmatter or MetadataTypeManager
  widget: string;          // "text" | "number" | "checkbox" | "date" | "datetime" | "tags" | "aliases" | "multitext" | "list" — the editor widget id
  occurrences: number;     // number of files using this property
}
```

Returned by `getAllPropertyInfos()` — keyed lowercase. The `widget` field defaults to `"text"` when indeterminate; inferred by `NL(value)` (defined outside metadata).

### `BacklinkSet`

Returned by `getBacklinksForFile`. Concrete type is `sc` — a multi-map with `add(source, ref)`, `remove(source, ref)`, `get(source)`, plus iterators. Definition lives outside this domain.

### Worker message protocol

Outbound (main → worker):

```typescript
{ metadataCache: ArrayBuffer }   // raw bytes of the file; passed as the transfer list
```

Inbound (worker → main):

```typescript
CachedMetadata   // whole parsed structure, as described above
```

`work(...)` asserts `workerResolve === null` (`metadata/MetadataCache.js:905-906`) — only one parse request is in flight at a time. Concurrency is bought by `workQueue` (a serial promise chain). Binary content is read via `vault.readBinary(file)` so the worker receives bytes, not already-decoded text — frontmatter YAML parsing happens worker-side.

---

## 3. On-disk contracts

MetadataCache writes **one** persistent store: an IndexedDB database keyed by the application id.

### IndexedDB: `<app.appId>-cache`

- **Opened by:** `MetadataCache._preload()` (`metadata/MetadataCache.js:368-401`) via `Zw(name, version, { upgrade })` — a small idb-like wrapper.
- **Read by:** `_preload()` (bulk hydrate) and `getFileInfo/getCache/getFileCache` (in-memory after hydrate).
- **Written by:** `transactionSave(store, key, value)` (a closure over the open db that coalesces writes into a single `readwrite` transaction in the same tick). Called from `saveFileCache` and `saveMetaCache`.
- **Version:** **19** as of Obsidian 1.12.7. Any version < 19 causes the `upgrade` callback to **drop and recreate both object stores**. There is no migration path — the user re-indexes the entire vault on upgrade.
- **Object stores:**
  - `"file"` — key: `relativePath: string`, value: `FileCacheEntry` (see §2).
  - `"metadata"` — key: `hash: string`, value: `CachedMetadata` with `frontmatterPos` in place of `frontmatterPosition`.
- **Durability:** transactions are opened with `{ durability: "relaxed" }` — writes may linger in the browser buffer through a crash. Corbomite's SQLite store is strictly more durable by default.
- **Lifecycle:** created on first app launch with the current vault id, retained indefinitely, cleared only by explicit user action (`MetadataCache.clear()`, invoked from debug/cache-reset paths) or by the upgrade-wipe at version bumps.
- **Failure mode:** if the `open` call throws, `_preload` logs `"Failed to load cache, unable to open IndexedDB"` and returns. `transactionSave` remains unbound, so in-memory operation continues but nothing is persisted across restarts. The next launch will re-parse everything.

### No vault-disk contract

MetadataCache does **not** read or write anything inside the vault root (neither `.obsidian/cache` nor any other path). The Pass 1 taxonomy's mention of "`.obsidian/cache`" appears to be a conceptual reference to the IndexedDB role, not an on-disk file. Obsidian's cache is browser-local; moving a vault to another machine triggers a full re-index. **This is load-bearing for Corbomite:** Corbomite's equivalent (`libs/storage/SQLiteIndex`) persists to `~/.local/share/corbomite*/index.sqlite` (outside the vault) which mirrors Obsidian's "cache is a host-side concern, not a vault-disk contract" invariant.

### Inputs consulted

MetadataCache reads two pieces of Vault configuration via `app.vault.getConfig(...)`:

- `"userIgnoreFilters"` — `string[]` of path patterns (`/regex/i` literal form or a plain prefix). The trailing `/.../` form is parsed as-is; bare forms are anchored with `^` and `oc(…)`-escaped. Invalid regexes log an error, no crash.
- `"newLinkFormat"` — `"shortest" | "relative" | "absolute"`. Consumed by `fileToLinktext`; re-read on every call (no caching), so user toggles take immediate effect.
- `"showUnsupportedFiles"` — `boolean`. If true, every extension is indexed; otherwise `viewRegistry.isExtensionRegistered(ext)` gates inclusion.

---

## 4. Events emitted

### `MetadataCache` (extends `Events`)

| Event name | Payload (inferred) | Triggered when | Typical consumers |
|---|---|---|---|
| `changed` | `(file: TFile, oldHash: string, cache: CachedMetadata)` | After a file's content has been parsed and its `metadataCache[hash]` entry written. Fired both for cache-hit reuse (same hash as another file) and for fresh worker parses. **Fires before link resolution** — `resolvedLinks` may still reference the stale cache when this fires. | `MarkdownRenderer` (invalidates section cache) at `editor/markdown/MarkdownRenderer.js:31`; plugin authors wanting per-file re-renders. |
| `deleted` | `(file: TFile, prevCache: CachedMetadata \| null)` | On vault `delete` event for a `TFile`. `prevCache` is the last known cache snapshot (read via `getFileCache(file)` **before** the path entry is cleared), so consumers get one-last-look at what was there. | Graph/backlinks plugins pruning references. |
| `resolve` | `(file: TFile)` | After `resolveLinks(path)` rebuilds `resolvedLinks[path]` and `unresolvedLinks[path]` for one file, inside the drain loop of `linkResolverQueue`. Fires **once per file** during a drain, not once per worker-round. | `MarkdownView.onMetadataChanged` at `editor/markdown/MarkdownView.js:480` — re-renders the currently-open file's link decorations and updates the breadcrumbs. |
| `resolved` | `()` | Emitted by `linkResolverQueue.onStop` (`metadata/MetadataCache.js:587`) — the work queue has drained and has no more files waiting. Equivalent to "all currently-pending link resolutions are complete". | `MetadataCache.checkCleanCache` (internal) runs to drain `onCleanCacheCallbacks`. External consumers: graph plugins waiting for a full-graph snapshot. |
| `finished` | `()` | Two sources: (a) the debounced `didFinish` (10 ms leading-edge) whenever `inProgressTaskCount` drops to 0 after a parse burst; (b) a single explicit trigger at the end of `initialize()` (`metadata/MetadataCache.js:516`) to signal "initial index complete". Plugins that want "wait for first-scan to finish" should listen here. | `checkCleanCache` (internal); UI "indexing spinner" teardown (`showIndexingNotice`'s `onCleanCache` callback). |

Cite for each:
- `changed` → `metadata/MetadataCache.js:780`, `:805`.
- `deleted` → `metadata/MetadataCache.js:847`.
- `resolve` → `metadata/MetadataCache.js:609`.
- `resolved` → `metadata/MetadataCache.js:587`.
- `finished` → `metadata/MetadataCache.js:18` (debounce), `:516` (init-done).

**Semantic distinction `resolve` vs `resolved`:** `resolve` fires *per file* during the drain, `resolved` fires *once* when the queue empties. `resolved` is not the plural of `resolve` — it's the terminal event. Plugin code that wants per-file fine-grained updates should listen to `resolve`; code that wants a post-batch snapshot of `resolvedLinks` should listen to `resolved` (or `finished` if it also wants a stable parse state).

**Payload ordering guarantee.** `changed` is emitted inside `computeFileMetadataAsync`'s worker-complete continuation, *before* `queueFileForLinkResolution` pushes the file onto the resolver queue. This means the order for a single-file modify is always: `changed` → (async tick) → `resolve` → (queue drained) → `resolved` → (10 ms debounce) → `finished`.

---

## 5. Events consumed

| Listener file | Subscribes to | Why |
|---|---|---|
| `metadata/MetadataCache.js:41` (self) | `finished` | Drain `onCleanCacheCallbacks` once the debounced "no work in flight" latch fires. |
| `metadata/MetadataCache.js:42` (self) | `resolved` | Same drain trigger — the clean-cache latch requires both no in-progress parses AND no queued link-resolutions. |
| `metadata/MetadataCache.js:340-344` (`watchVaultChanges`) | `vault.on('create')` → `onCreate` | Trigger parse + `updateRelatedLinks` to let previously-unresolved `[[New Note]]` links rebind. |
| same | `vault.on('modify')` → `computeFileMetadataAsync` | Re-parse on file edit. |
| same | `vault.on('delete')` → `onDelete` | Drop cache, emit `deleted`, re-queue link resolution for files that pointed here. |
| same | `vault.on('rename')` → `onRename` | Move cache rows to the new path, update `uniqueFileLookup`, re-queue resolution for files referencing either the old basename or the new basename. |
| same | `vault.on('config-changed')` → `onConfigChanged` | Currently only reacts to `userIgnoreFilters` — rebuilds the regex list and clears the memoised filter cache. |

---

## 6. Commands registered

No commands registered here.

---

## 7. Registries owned

### `MetadataCache.linkUpdaters` (internal-only registry)

- **Stores:** `LinkUpdaterPlugin` instances, one per non-markdown file extension that has link-rewrite support (canvas, bases).
- **Populated by:** other domains via direct property assignment (`app.metadataCache.linkUpdaters[ext] = impl`). Inspection (`grep`) shows the write sites live in canvas and bases built-in plugins — not in `metadata/` itself. No public `registerLinkUpdater` verb, so this is **not** a plugin extension surface.
- **Read by:** `iterateAllRefs`, `iterateRefsForFile`, `updateInternalLinks`, `getLinkSuggestions`, `updateRelatedLinks` (`metadata/MetadataCache.js:242-248, 264-268, 236-240`). Expected interface:

  ```typescript
  interface LinkUpdaterPlugin {
    iterateReferences(cb: (sourcePath, ref) => void): void;
    iterateReferencesForFile(path: string, cb): void;
    applyUpdates(file: TFile, updates: Update[]): Promise<void>;
  }
  ```

- **Persistence:** in-memory only. Repopulated on plugin load.
- **Lifecycle:** Added when a built-in plugin enables the extension type (e.g., canvas plugin initialises). Removed when that plugin unloads. Not subject to `clear()`.

### `MetadataCache.blockCache` (internal-only)

Opaque sub-index (`qL(app)`) handling resolution of `^block-id` anchors across files. Constructed but never exported directly via prototype methods of `MetadataCache`. Its members are reached through the BlockCache class itself, defined in another domain.

### `MetadataCache.uniqueFileLookup` (internal-only)

The lowercase-name multi-map used by `getLinkpathDest`. Populated in `initialize()` for every loaded file and maintained in `computeFileMetadataAsync`/`onDelete`/`onRename`. Keyed by `file.name.toLowerCase()` (full filename including extension). A single key can map to multiple files (e.g., two `Notes.md` in different folders) — the resolver's shortest-path tiebreaker depends on this multi-value semantics.

---

## 8. Invariants

Things Corbomite must uphold for plugin and user-observable compat.

- **Hash-keyed dedup.** `metadataCache` is keyed by SHA-256 content hash, not by path. Two files with identical bytes share one entry. An efficiency contract — templating 500 files with identical content parses once.
- **Mtime+size staleness check.** `computeFileMetadataAsync` skips work only when the stat tuple is unchanged AND the cached hash still resolves. Any stat delta re-hashes; hash-collision with an existing metadata entry reuses it (no worker round-trip) but still emits `changed`.
- **`resolvedLinks[src][dst] ≥ 1`** when the key is present. Missing key = zero, not "unresolved". Unresolved references are in `unresolvedLinks`.
- **`resolvedLinks[src][dst]` is a count**, not a boolean. Three `[[Foo]]` links in one file → value `3`.
- **`unresolvedLinks` keys are normalised** via `BL(getLinkpath(linktext))` — lowercased, heading/block-id stripped. Do not expect original casing to survive.
- **`onRename` keeps the link maps live.** Keys in `resolvedLinks`/`unresolvedLinks` are remapped `oldPath → newPath` immediately (`metadata/MetadataCache.js:868-869`) and the file is re-queued for resolution. Values stabilise on the next `resolve` for that file.
- **`onCleanCache(fn)` is *eventually* called** as long as no new work is enqueued. Fires immediately if already clean, else on the next `finished`/`resolved` edge. `FileManager.runAsyncLinkUpdate` depends on this for snapshot stability.
- **`isCacheClean()` is three-valued.** Needs `inProgressTaskCount === 0` AND `linkResolverQueue.items.length === 0` AND `!linkResolverQueue.runnable.isRunning()`.
- **Worker queue is strictly sequential.** `work()` throws `"Work queue must be sequential!"` on overlap. Use `workQueue.queue(...)` for concurrency.
- **`preload()` runs at most once.** Subsequent calls return the in-flight promise.
- **`deleted` fires with `prevCache`, not `null`.** `onDelete` passes `this.getFileCache(file)` **before** `deletePath` clears the row — so plugins can inspect the final tags/links for cleanup.
- **Unsupported files occupy `fileCache` with `hash: ""`.** `getCache(path)` returns `{}` (empty object, not `null`) for non-`.md` entries seen by the cache; `null` means "not in the cache at all". For `.md` files, `null` also means "parse failed" (logs `"Metadata failed to parse"`), since the result is only written on success.
- **Frontmatter position key is renamed on disk.** In-memory: `frontmatterPosition`. On-disk: `frontmatterPos`. Plugins reading `cache.frontmatterPos` from `getFileCache` see `undefined`.
- **The `finished` event's "initial index complete" variant is indistinguishable** from subsequent debounced `finished` events. If a plugin needs once-on-boot, it must latch on first observation.

### Link resolution algorithm (`getLinkpathDest`)

Called from `getFirstLinkpathDest`, `resolveLinks`, `fileToLinktext`. Takes `(linktext, sourcePath)` and returns `TFile[]` in precedence order.

1. **Empty linktext + sourcePath** → return `[vault.getAbstractFileByPath(sourcePath)]` if it's a file, else `[]`.
2. Lowercase the linktext. If it contains `.`, look up `uniqueFileLookup[name]`; else retry with `name + ".md"` lowercased. No candidates → `[]`.
3. **Exactly one candidate** and the linktext matched with its literal extension → return it alone.
4. **Relative `./` or `../`** — strip `./../`, walk `../` up the source's parent chain, compare each candidate's lowercased path for literal equality against the resolved absolute. Exact match → `[match]`. Miss → fall through.
5. **Leading `/` — rooted absolute only.** Strip slash; require exact path match. If nothing matches and the original started with `/`, return `[]` — do not attempt short-name disambiguation.
6. **Short-name disambiguation.** Partition candidates whose lowercased path `endsWith(linktext)` into `sameFolder` (starts with source's lowercased folder prefix + `/`) and `otherFolder`; sort each by `VL` (shortest-path, alpha tiebreak) and return `sameFolder.concat(otherFolder)`.

Upshot: **shortest-path-wins, with same-folder preference** for ambiguous short names. Corbomite's `resolveWikilink` (`libs/storage/src/SQLiteIndex.cpp:592-609`) is a flat lowercase-name lookup — does not implement 3-6. Under ambiguity it returns whichever copy was hashed last. High-priority compat gap — see §11.

`fileToLinktext` is the inverse: render a `TFile` into the minimal linktext that resolves back. Under `"shortest"` tries `basename` first; widens to full path if `getLinkpathDest(basename, sourcePath) !== [file]`.

### Incremental invalidation

When `vault.on('modify')` fires:
1. `computeFileMetadataAsync(file)` compares `fileCache[path].mtime`/`size` against `file.stat`.
2. If equal **and** the hash still resolves to a metadataCache entry → no work. Only `queueFileForLinkResolution(file)` is called (in case previously-unresolved links now have a target).
3. If different → `readBinary` → `Sf(bytes)` (hash) → if hash matches an existing metadata entry, reuse it and emit `changed` without a worker round. Otherwise post to worker, await, save, emit `changed`.
4. Regardless of reuse vs fresh parse, the file is enqueued for link resolution.

Nothing smaller than "the entire file" is reparsed. No per-block diffing. Corbomite does not need a smarter scheme to match Obsidian.

---

## 9. Observable user features

- **Wikilink auto-completion popup** — populated by `getLinkSuggestions()` (files + aliases + unresolved targets).
- **Unresolved links render italic-underlined** because `isUnresolved(linktext, sourcePath)` returns true. When the target is created, `updateRelatedLinks` re-queues referring files and the italic state clears live.
- **Tag completion** (typing `#` or `tags:`) — from `getTags()` with subtag-prefix counts (typing `#proj` matches `#project/foo/bar` and all its prefix tags).
- **Backlinks pane** — streams `getBacklinksForFile(currentFile)` ordered by the selected sort.
- **Rename-refactor link repair** — `FileManager.renameFile` → `runAsyncLinkUpdate` → awaits `onCleanCache`, snapshots refs via `iterateAllRefs`, executes rename, replays `updateInternalLinks` using old snapshot.
- **Indexing notice toast** — `showIndexingNotice` polls `inProgressTaskCount` every 300 ms.
- **Properties side panel** — lists frontmatter keys from `getAllPropertyInfos()`; value picker uses `getFrontmatterPropertyValuesForKey(key)`.
- **Heading / block-id anchor resolution** (`[[Note#Heading]]`, `[[Note#^block-id]]`) — driven by `cache.headings` and `cache.blocks` position offsets.
- **Zero-latency startup** — `preload()` hydrates the whole cache from IndexedDB before the UI settles; resolved links appear immediately on boot.
- **"Show unused attachments"** — uses `resolvedLinks` plus an orphan scan.

---

## 10. Extension surfaces exposed

| Surface | Registration verb | Consumer call site | What plugins supply |
|---|---|---|---|
| Per-file cache read | `app.metadataCache.getFileCache(file)`, `getCache(path)` | Any plugin inspecting links / tags / headings / frontmatter. | N/A (consume). |
| Backlinks read | `app.metadataCache.getBacklinksForFile(file)` | Graph / backlinks / table plugins. | N/A. |
| Resolved-link graph read | `app.metadataCache.resolvedLinks`, `.unresolvedLinks` | Graph view, network analysis. | N/A — read-only contract; mutation unsupported. |
| Event subscription | `app.metadataCache.on('changed'\|'deleted'\|'resolve'\|'resolved'\|'finished', cb, ctx)` | Any plugin wanting incremental updates. | `(file?, cache?, prev?) => void` |
| Link suggestions | `app.metadataCache.getLinkSuggestions()` | Quick-switcher, `[[` popup. | N/A. |
| Reference walker helpers | `iterateCacheRefs(cache, cb)`, `iterateRefs(refs, cb)` | Plugins iterating links + embeds together. | `cb: (ref) => boolean \| void` |
| Tag helper | `getAllTags(cache)` | Plugins wanting per-file tags. | N/A. |
| Per-extension link updater | `app.metadataCache.linkUpdaters[ext] = impl` | Canvas, bases (built-ins). | `{ iterateReferences, iterateReferencesForFile, applyUpdates }`. No stable `register*` verb — property assignment only. |

The low-level read surface (`resolvedLinks`, `unresolvedLinks`, `fileCache`, `metadataCache`) is documented here because plugins read it directly — any Corbomite-side shape divergence becomes a user-visible bug.

---

## 11. Corbomite mapping

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `MetadataCache` class | `libs/storage/SQLiteIndex` | Partial | Link/backlink/tag query and FTS coverage; no per-file `CachedMetadata` record, no `resolvedLinks`/`unresolvedLinks` in-memory maps, no Qt-signal counterparts to the five events. |
| `CachedMetadata` record | N/A | Missing | Add a `struct NoteMetadata` in `libs/core/` populated by the Markoff parser and mirrored into SQLite — without it, backlinks pane / graph view / properties panel need ad-hoc SQL joins. |
| `resolvedLinks` / `unresolvedLinks` | `SQLiteIndex::allLinks`, `backlinksFor`, `outlinksFor`, `orphanLinks` | Partial | Query-only today. Add a `LinkGraphModel` in `libs/models/` caching last query + signals for graph-view consumers. |
| `fileCache[path]` (mtime/size/hash) | N/A | Missing | `SQLiteIndex::indexNote` re-reads every content without a stat short-circuit. Introduce `notes(relative_path PK, mtime, size, hash, last_parsed)` and skip rebuild when the stat tuple is unchanged. Biggest single throughput lever. |
| Content-hash dedup | N/A | Missing | Keep parse results keyed by `hash`; `notes.hash` joins in. Low priority unless template-heavy vaults show up. |
| Cache-version wipe on bump | SQLite `PRAGMA user_version` | Missing | Implement destructive migration in `SQLiteIndex::open` on mismatch — mirrors Obsidian's v19 behaviour. |
| `getFirstLinkpathDest` / `getLinkpathDest` | `SQLiteIndex::resolveWikilink` (`libs/storage/src/SQLiteIndex.cpp:592`) | Partial | Flat `m_nameToPath` lookup — no shortest-path disambiguation, no same-folder bias, no `./`/`../` relative support. **Highest-priority correctness gap.** Port the 10-step algorithm in §8. |
| `getBacklinksForFile` | `SQLiteIndex::backlinksFor` | Partial | Missing position offsets. Extend `LinkInfo` with `qint64 startOffset, endOffset`. |
| `getAllTags(cache)` / `getTags()` | `SQLiteIndex::allTags` / `notesWithTag` | Partial | No subtag-prefix counting; no frontmatter `tags:` merge (extraction is inline-only — see `extractAndInsertTags`). |
| `getAllPropertyInfos` / `getFrontmatterPropertyValuesForKey` | N/A | Missing | Needed for Bases-style views + property value pickers. Add `frontmatter_properties(key, widget, occurrences)` and a value-enum query. |
| `getLinkSuggestions` | N/A | Missing | Wiki-link popup currently uses `VaultModel`; add alias + orphan-target suggestions. |
| `fileToLinktext` | N/A | Missing | Required for link insertion in editors and future canvas. Add `QString SQLiteIndex::fileToLinktext(target, source, NewLinkFormat) const`. |
| Worker parsing (per-file) | `VaultScanner::rebuildIndexAsync` | Partial | Bulk-rebuild on `QThread` exists; per-file parses still GUI-thread. Move per-file work to `QThreadPool`. |
| The five events | `SQLiteIndex::indexReady` signal | Partial | Single signal covers only bulk-done. Add `noteMetadataChanged(path)`, `noteDeleted(path)`, `linkResolvedFor(path)`, `linksDrained()`, `initialIndexFinished()`. |
| `userIgnoreFilters` | N/A | Missing | Wire a `QList<QRegularExpression>` from config into `VaultScanner`. |
| `linkUpdaters` / canvas-bases link rewrite | N/A | Missing | Defer until canvas / bases land. |
| `onCleanCache(fn)` | N/A | Missing | Rename-refactor needs a "settle then snapshot" primitive — expose as `whenIdle(std::function<void()>)`. |
| `blockCache` (heading / `^block-id`) | N/A | Missing | Required for `![[Note#^abc]]` resolution. Persist `blocks` and `headings` tables with offsets. |
| `footnoteRefs` / `footnotes` | N/A | Missing | Link-to-footnote navigation not yet supported. |

**Top-priority gaps:** shortest-path-wins resolver, mtime/size/hash staleness check, `NoteMetadata` struct with block/heading anchors, frontmatter property registry.

---

## 12. Markoff gap confirmations / discoveries

N/A — no editor/rendering surface in this domain. Indirectly, however, `MetadataCache.getFileCache(file).sections` feeds the preview renderer's section-recycling; this gap is documented in `01-markoff-gaps.md` under the editor/markdown domain, not here.

---

## 13. Open questions

1. **`NL(value)` widget-inference function** — what widget string does it infer from an arbitrary JS value? Used in `getAllPropertyInfos`. Expected to live in the `ui/*` / `core/*` direction near `MetadataTypeManager`; grep did not find the owner in renamed tree. Worth locating to match Corbomite's frontmatter-property panel widget set.
2. **`gT` (position-prune predicate) semantics** — `mT(cache, gT)` rewrites positions in the persisted copy. Does it collapse zero-length ranges? Integer-offset pack? Without seeing `gT` we can't replicate the on-disk shape byte-compatibly, but that's moot for us (SQLite).
3. **Canvas / Bases `linkUpdaters` protocol** — the exact shape of `applyUpdates(file, updates)` patches is inferred, not proven. Need to confirm against the canvas domain audit.
4. **Is `frontmatterPosition.start.offset === 0` always?** — the position covers the `---` fence through its closing `---`. Corner case: files with only trailing whitespace before frontmatter, or files with a BOM. Worth a test.
5. **Footnote shape (`footnoteRefs` vs `footnotes`)** — inferred from general Obsidian plugin lore; not mentioned in this file. Worker-side — confirm when auditing `worker.js` (outside Pass 2 scope).
6. **`getLinks()` includes embeds — should its shape emphasise that?** Shape is `Record<src, (LinkCache | EmbedCache)[]>` — a union. If Corbomite splits `links` and `embeds` tables, the API translation must tag each row with a discriminator.
7. **`resolvedLinks[src]` for a source that is itself non-`.md`.** Can a `.canvas` source appear in `resolvedLinks`? Appears yes via `linkUpdaters`. Needs confirmation after canvas domain audit.
8. **How does `iterateRefsForFile` behave for frontmatter-only changes in `.md`?** `frontmatterLinks` is part of `cache` but `xT`'s iteration set is unclear without the `xT` definition. If `xT` walks only `links` + `embeds` (not `frontmatterLinks`), rename-refactor of a frontmatter link would need `linkUpdaters["md"]` — which is absent — suggesting markdown frontmatter-link rewrite happens via `vault.process` + `UL(text, patches)` not via `updateInternalLinks`. Worth validating.
9. **IndexedDB version 19 was reached over how many real version bumps?** Irrelevant for Corbomite's fresh user_version but worth noting when interpreting historical cache compatibility data.
10. **`BlockCache` (qL) class** — defined outside this directory; where does it live, and what's its persistence strategy (IndexedDB row? Per-file reconstruction?)?

---

## 14. Recommended Pass 3 synthesis input

1. **Promote the Link Resolution Algorithm (§8 steps 1-10) to `VAULT-FORMAT.md`** as a standalone "Obsidian short-link resolution" subsection. It is a compatibility contract. Any app that claims to render `[[Foo]]` identically to Obsidian must obey it. Corbomite's current implementation diverges and needs repair.
2. **Promote the `CachedMetadata` + `resolvedLinks` + `unresolvedLinks` + `fileCache` shapes (§2) to `VAULT-FORMAT.md`** under "Plugin-visible metadata contract". Third-party plugin code reads these directly; any Corbomite plugin-compat shim must reproduce them byte-for-byte.
3. **Promote the `changed` → `resolve` → `resolved` → `finished` event ordering and semantics (§4) to `FEATURE-MATRIX.md` — "MetadataCache events"** as a single matrix row, and add a row to `GAP-ANALYSIS.md` noting that Corbomite exposes only a single `indexReady` Qt signal today; five signals with the specified ordering are required.

---

## 15. Cross-domain references

| Other domain | Reference type | Brief description |
|---|---|---|
| `vault` | upstream producer | `Vault` fires `create`/`modify`/`delete`/`rename`/`config-changed` which `MetadataCache.watchVaultChanges` consumes. `Vault.readBinary(file)` supplies raw bytes to `work()`. `Vault.getConfig("newLinkFormat"|"userIgnoreFilters"|"showUnsupportedFiles")` drives behaviour. `Vault.getFiles/getAllLoadedFiles/getAbstractFileByPath/getFileByPath` + `TFile` type-check underpin every resolver branch. |
| `parsing` | dependency | `parseFrontMatterTags(fm)` and `parseFrontMatterAliases(fm)` from `obsidian/parsing/` are called directly by `getAllTags` and `getLinkSuggestions`. |
| `vault` (utilities) | dependency | `getLinkpath(s)` from `obsidian/vault/getLinkpath.js` strips `#heading`/`#^block-id` suffixes before resolution. |
| `core` | sibling | `App` constructs `MetadataCache(app, vault)` and holds the instance at `app.metadataCache`. `App.metadataTypeManager.assignedWidgets` is read by `getAllPropertyInfos` for the frontmatter-widget defaults layer. `App.appId` keys the IndexedDB name. `App.viewRegistry.isExtensionRegistered(ext)` gates `isSupportedFile`. |
| `views` | consumer | `TextFileView` and `MarkdownView` subscribe to `resolve`/`changed`. `ViewRegistry` also holds the extension-registration state read by `isSupportedFile`. |
| `editor/markdown` | consumer | `MarkdownRenderer.on("changed")` re-renders sections. `MarkdownView.on("resolve")` updates link chrome, breadcrumbs, and frontmatter panel. Preview renderer's `sections[]` shape mirrors `CachedMetadata.sections`. |
| `workspace` | consumer | `Workspace.openLinkText(linktext, source, …)` → `getFirstLinkpathDest`. The quick-switcher/graph-view consume `resolvedLinks`/`unresolvedLinks`. `Workspace.getBacklinks…` delegates. |
| `bases` | consumer | Every `bases/*Value.js` reads frontmatter via `getFileCache(file).frontmatter` and the type-inference helpers. Bases also registers a `linkUpdater` entry. |
| `rendering` | consumer | `RenderContext.renderFileLink` calls `app.metadataCache.getFirstLinkpathDest(linkpath, "")`. |
| `publish` | consumer | `Publish.js` calls `cache.getLinkpathDest(linkpath, path)` on the publish-side cache variant; same algorithm. |
| `canvas` | consumer + extension | Registers a `linkUpdaters[".canvas"]`; stores `caches: Record<string, ...>` and `embeds[]` on canvas nodes that `xT` walks. |
| `worker` (external, `worker.js` in app root) | sibling | The parser. Consumes `{metadataCache: ArrayBuffer}` messages and replies with `CachedMetadata`. Owns the markdown grammar — **not** audited in Pass 2 scope. |

Short-symbol cross-reference:

| Short symbol | Defined in | Used here for |
|---|---|---|
| `Events` | `core` | class mix-in for `on`/`trigger` |
| `TFile` | `vault` | type guard for every `e instanceof TFile` branch |
| `getLinkpath`, `nu`, `au`, `su`, `iu` | `vault` | path + linktext normalisers (strip heading, lowercase, extname, dirname) |
| `parseFrontMatterTags`, `parseFrontMatterAliases` | `parsing` | frontmatter helpers |
| `xT` | `core` | ref walker — walks `links`/`embeds`/`frontmatterLinks`/`blocks` |
| `UL` | `parsing`/`editor` | text mutator applying link-rewrite patches to raw markdown |
| `qL` | `core` or adjacent | `BlockCache` constructor (`blockCache = new qL(app)`) |
| `NL` | `core`/`ui` | widget inference for `getAllPropertyInfos` |
| `BL` | `utils` | linkpath normaliser for `unresolvedLinks` keys |
| `VL` | `utils` | path comparator (shortest-path sort) |
| `HL`, `zL`, `$A`, `ub` | `utils` | set-intersect / non-empty-string / tag-validator / locale-compare predicates |
| `vT`, `wT`, `kT`, `mT`, `gT`, `yc` | `utils` | persist/hydrate filters and position normalisers for `saveMetaCache`/`_preload` |
| `sc`, `vb`, `wb`, `Cb` | `utils` | multi-map, serial-queue, generator-queue, runnable wrapper |
| `Zw`, `GL` | `utils`/`platform` | `idb-open` wrapper and chunked cursor walker for `_preload` |
| `Sf`, `kf`, `oc` | `utils`/`platform` | async SHA-256, byte decode, `escapeRegExp` |
