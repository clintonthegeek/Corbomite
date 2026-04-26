# Metadata domain audit

## Architecture fit

Corbomite's metadata stack (Cluster I) maps onto Obsidian's `MetadataCache`
in a recognisable shape: a `Corbomite::MetadataCache` `QObject`
(`libs/storage/include/corbomite/storage/MetadataCache.h:63`) owns two
hashes — `m_pathToFileEntry` (path → `FileCacheEntry{mtime,size,hash}`,
`MetadataCache.h:178`) and `m_hashToCache` (sha-256 hex →
`CachedMetadata`, `MetadataCache.h:179`) — plus a per-hash refcount
(`m_hashRefCount`, `MetadataCache.h:180`). Both are 1:1 with Obsidian's
`fileCache` and `metadataCache` records, including the SHA-256 content
hash dedup (`MetadataCache.cpp:267-274`) and the empty-hash sentinel for
unsupported files (`MetadataCache.cpp:199-217`).

Persistence translates IndexedDB → SQLite. `CachedMetadataStore`
(`libs/storage/src/CachedMetadataStore.cpp:1-120`) keeps two tables —
`file_cache` and `metadata_cache` — under `PRAGMA user_version = 2`,
mirroring Obsidian's destructive-on-bump policy
(`CachedMetadataStore.cpp:92-100`). The schema lives in the *host*
config dir (`metadata-cache.db`, `MainWindow.cpp:2028`), not inside the
vault — preserving Obsidian's "cache is a host-side concern" invariant
(spec §3 "No vault-disk contract"). A 30 s debounce coalesces autosave
→ persist (`MetadataCache.cpp:445-453`); `indexFinished` flushes
immediately (`MetadataCache.cpp:418-427`), which is strictly more
durable than Obsidian's `{ durability: "relaxed" }`.

Worker offloading: `MetadataWorker`
(`libs/storage/include/corbomite/storage/MetadataWorker.h`) owns its
own `QThread`; `enqueueParse` posts to a FIFO and the worker emits
`parsed(...)` via `Qt::QueuedConnection`, exactly matching Obsidian's
"Work queue must be sequential!" invariant. Markdown→`CachedMetadata`
runs on the worker thread (`MetadataParser.cpp:286-716`).

Downstream, `SQLiteIndex` is no longer the writer — it subscribes to
`MetadataCache::cacheChanged` / `cacheDeleted` and derives FTS / links /
note_tags rows (`SQLiteIndex.h:38-86`, `SQLiteIndex.cpp:202-310`). This
is the right "shape, not storage" translation: web/IndexedDB →
SQLite + Qt; the in-memory `CachedMetadata` is the ground truth and
SQLite is a derived materialised view for fast queries.

## Implemented (parity-equivalent)

- **`CachedMetadata` shape (`CachedMetadata.h:115-152`).** All thirteen
  spec-§2 fields are present: `links`, `embeds`, `tags`, `headings`,
  `sections`, `listItems`, `footnoteRefs`, `footnotes`, `blocks`,
  `frontmatter`, `frontmatterLinks`, `frontmatterPosition`. The
  in-memory ↔ on-disk rename of `frontmatterPosition` ↔
  `frontmatterPos` is honoured via `toPersistedJson` /
  `fromPersistedJson` (`CachedMetadata.h:163-169`).
- **Two-layer hash-keyed dedup.** Identical content shares one
  `m_hashToCache` row (`MetadataCache.cpp:267-274`); refcount drives
  GC on overwrite/delete (`MetadataCache.cpp:301-314`).
- **Stat short-circuit.** `onFileChanged` skips re-parse when
  mtime+size match (`MetadataCache.cpp:96-102`); on stat-only delta the
  hash is recomputed and on hash-match only the stat row is touched
  (`MetadataCache.cpp:106-116`). Matches spec §8 "Mtime+size staleness
  check" exactly.
- **Five-event lifecycle, ordered.** `cacheChanged` →
  (per-tick drain) `linksResolvedFor` → `allLinksResolved` →
  (10 ms debounce) `indexFinished`; `cacheDeleted` is independent of
  the resolver queue (`MetadataCache.h:36-49`,
  `MetadataCache.cpp:316-427`). The 10 ms debounce constant
  (`MetadataCache.cpp:59`) matches Obsidian's `didFinish = debounce(…,
  10, leading=true)`.
- **Plugin-facing event-bus mirror.** `Corbomite::Events m_events`
  fires Obsidian-named `"changed"` / `"deleted"` / `"resolve"` /
  `"resolved"` / `"finished"` strings alongside the typed Qt signals
  (`MetadataCache.cpp:336-339, 396-409`). Plugin code can use
  `events().on("changed", fn)` directly.
- **Worker offloading.** `MetadataWorker` runs parses on a dedicated
  `QThread`; results delivered main-thread via queued connection
  (`MetadataWorker.h:31-66`).
- **Persistence + destructive migration.**
  `CachedMetadataStore::open` bumps `PRAGMA user_version` and drops
  its two tables on mismatch, mirroring Obsidian's IndexedDB v19 wipe
  (`CachedMetadataStore.cpp:92-100`). `MetadataCache::open` /
  `close()` / `installPersistedState` plug in a clean
  load/persist/snapshot cycle (`MetadataCache.cpp:455-513`).
- **Link-resolution algorithm.** `LinkResolver::resolve`
  (`LinkResolver.cpp:100-199`) implements the spec §8 6-step
  algorithm: empty linktext → self; rooted-`/` exact match-only;
  `./..` relative resolution; basename lookup w/ `.md` retry;
  single-candidate-with-explicit-extension shortcut; short-name
  disambiguation with same-folder bias and shortest-path /
  alpha-tiebreak sort (`LinkResolver.cpp:32-38, 172-198`). This
  closes the spec §11 "highest-priority correctness gap".
- **Unsupported files in fileCache.** `onUnsupportedFile` stores
  `hash == ""` so `getCache(path)` distinguishes "tracked unsupported"
  (returns empty `CachedMetadata{}`) from "not in cache"
  (`std::nullopt`) — matches spec §8 ‘Unsupported files occupy
  fileCache’ (`MetadataCache.cpp:199-217`, `:219-235`).
- **`prevCache` on delete.** `onFileDeleted` captures cache *before*
  teardown so subscribers see the final state
  (`MetadataCache.cpp:181-193`).
- **Tag aggregation (in-cache).** `MetadataParser` merges inline `#tag`
  occurrences with frontmatter `tags:` (array or string) and
  singular `tag:` (string or array); both are normalised to the
  `#name` form and pushed onto `cache.tags` with the frontmatter
  position (`MetadataParser.cpp:407-461`). This is `getAllTags(cache)`
  + `parseFrontMatterTags` collapsed into the parse step.
- **Frontmatter dotted-key link walker.**
  `collectFrontmatterLinks` does the recursive YAML walk producing
  `FrontmatterLinkCache{ link, original, displayText, key="a.b.0" }`
  (`MetadataParser.cpp:187-250`).
- **`userIgnoreFilters`.** `IgnoreFilter::fromPatterns` honours the
  `/regex/` form vs plain prefix anchoring + escaping
  (`IgnoreFilter.cpp:6-43`); `VaultConfig::userIgnoreFilters` reads it
  from `app.json` (`VaultConfig.cpp:263`); `VaultScanner`
  consumes it via `setIgnoreFilter`
  (`VaultScanner.h:22`). Invalid regexes are silently skipped — spec
  §3 says Obsidian logs and continues; behavioural parity within
  acceptable tolerance.
- **Vault → MetadataCache wiring.** All five vault signals are
  routed: `created`/`modified`/`renamed` → `onFileChanged`;
  `deletedFile`/`renamed` → `onFileDeleted`
  (`MainWindow.cpp:2062-2101`). `rebuildVault` performs the initial
  reconciliation and reaps stale paths
  (`MetadataCache.cpp:130-166`).
- **`SQLiteIndex` as derived view.** `setMetadataCache` plus
  `reconcileWithCache` keep FTS / links / note_tags consistent on
  schema migration (`SQLiteIndex.cpp:91-110`,
  `MainWindow.cpp:2025-2030`). Tag rows are written verbatim including
  the leading `#` (`SQLiteIndex.cpp:299-307`), preserving Obsidian's
  `cache.tags[].tag` shape.

## Partial / divergent (with gap description)

- **No `resolvedLinks` / `unresolvedLinks` reverse maps.**
  Obsidian publishes `Record<src, Record<dst, count>>` for both
  resolved and unresolved links, queryable in O(1)
  (spec §2 "ResolvedLinkMap" / "UnresolvedLinkMap"). Corbomite has
  none of this on `MetadataCache` directly. Backlinks are computed
  two ways:
  - `MetadataCacheReader::backlinksFor` (proxy) does an O(N)
    walk of `allPaths()`, consulting each cache's
    `links` / `embeds` / `frontmatterLinks`
    (`MetadataCacheReader.cpp:69-96`). Correct but O(NotesInVault)
    per query.
  - `SQLiteIndex::backlinksFor` is O(1) via the `links` table
    (`SQLiteIndex.cpp:488-510`), but only consults the rows derived
    from `cache.links` + `cache.embeds`. The proxy walks
    `frontmatterLinks` too; the SQL view does not (see "Notable
    concerns").
  No equivalent of `unresolvedLinks` exists at all — the unresolved
  state is not stored as a reverse map. This degrades the wiki-link
  popup (no "orphan target" suggestions; see "Missing").
- **`drainOnePath` only re-resolves `cache.links`.**
  `MetadataCache::drainOnePath` walks `*cacheIt->links` and rewrites
  `link.link` from raw target through `m_resolver.resolve`, but does
  *not* touch `embeds` or `frontmatterLinks`
  (`MetadataCache.cpp:368-393`, with a TODO acknowledging the gap).
  When a `[[New Note]]` is created, embeds and frontmatterLinks
  pointing at it stay in the pre-resolve "raw target" state until the
  source file is itself re-parsed. Spec §8 "onRename keeps the link
  maps live" expects all link kinds to re-resolve on each per-file
  drain.
- **Tag aggregation lacks subtag-prefix counts.**
  `MetadataCacheReader::allTags` and `SQLiteIndex::allTags` produce a
  flat sorted list (`MetadataCacheReader.cpp:149-166`,
  `SQLiteIndex.cpp:577-585`). Obsidian's `getTags()` returns
  `Record<tag, count>` *with subtag-prefix decomposition* — a tag
  `#proj/foo/bar` increments `#proj`, `#proj/foo`, and
  `#proj/foo/bar`. Tag autocomplete relies on this — typing `#proj`
  should match `#project/foo` even if no exact `#project` tag exists.
  Corbomite's tag completion will under-report.
- **Tag normalisation: leading `#` is stripped at the reader layer.**
  `MetadataCacheReader::stripHash` removes the `#`
  (`MetadataCacheReader.cpp:54-58`). Plugins receive `bare` tags. But
  `cache.tags[].tag` includes the `#` (per spec §2). Result: the
  plugin-API and the in-cache shapes disagree on whether `#` is part
  of the tag string. Document the convention in the proxy header.
- **Heading/block anchor resolution is partial.**
  `Workspace::openLinkText` splits `#`/`^` anchors and routes them
  through the link resolver (`Workspace.cpp:401-456`), and writes the
  subpath into `eState`. But there is no in-`MetadataCache`
  `BlockCache` (Obsidian's `qL`) that resolves `[[Note#^abc]]`
  cross-file anchors. `cache.blocks` *is* parsed
  (`MetadataParser.cpp:545-570`) and stored, so the data is there;
  no consumer does the lookup. Heading anchors similarly: `cache.headings`
  is populated but no API translates `#Heading` → byte offset for
  scroll-on-open.
- **Worker offloading is per-file only; bulk rebuild reads on the main
  thread.** `MetadataCache::rebuildVault` opens and reads each file
  synchronously (`MetadataCache.cpp:137-149`) before enqueuing into
  the worker. With a large vault the I/O blocks the GUI even though
  parsing is offloaded. Spec §11 calls this out as a partial
  ("Bulk-rebuild on QThread exists; per-file parses still GUI-thread")
  but the description is now inverted: per-file parses are offloaded;
  bulk-vault file *reads* are still GUI-thread.
- **`linksResolvedFor` cadence.** Obsidian fires `resolve` once per
  file during the drain. Corbomite fires `linksResolvedFor` once per
  drain *tick* via `QTimer::singleShot(0, ...)` per path
  (`MetadataCache.cpp:354, 401`). Cadence is right; ordering may
  differ if the queue receives new entries mid-drain. Worth a behaviour
  test.
- **`renameFile` link-rewrite is basename-only and not anchor-aware
  for markdown links.** `FileManager::renameFile`
  (`FileManager.cpp:148-216`) only rewrites `[[oldBase]]`,
  `[[oldBase|`, `[[oldBase#`. It does not touch
  `[text](oldPath.md)` markdown-style links or wiki-links by full
  path. Inline comment acknowledges spec §11 follow-ups. Compared to
  Obsidian's `updateInternalLinks(updates)` which patches both link
  kinds and respects `newLinkFormat`, the current implementation is a
  starter slice.

## Missing

- **`getFirstLinkpathDest` / `getLinkpathDest` as a public
  `MetadataCache` method.** The algorithm exists in
  `LinkResolver::resolve`, but it is *not* exposed from
  `MetadataCache` (no method, no proxy entry). Workspaces use a
  private `LinkResolverFn` lambda (`Workspace.cpp:396-432`); the
  proxy `MetadataCacheReader` has no resolver entry point at all
  (`MetadataCacheReader.h`). Plugins cannot resolve a `[[Foo]]` link.
  Required for spec §10 "Per-file cache read" extension surface.
- **`getLinkSuggestions`.** No equivalent. `WikiLinkSuggest`
  (`src/editor/WikiLinkSuggest.cpp:39-56`) iterates
  `vault->getMarkdownFiles()` directly and returns basenames only —
  no aliases, no orphan targets, no path disambiguation, no 500-char
  truncation. Obsidian's algorithm includes frontmatter `aliases` as
  separate entries and surfaces unresolved-link paths so users can
  preflight a new note's target before creation.
- **`fileToLinktext`.** Spec §2 says the inverse-resolver renders a
  `TFile` into the minimal linktext respecting `newLinkFormat`. None
  exists. `FileManager::generateMarkdownLink`
  (`FileManager.cpp:306-324`) hardcodes the `.md` basename branch;
  doesn't query `getLinkpathDest(basename, sourcePath) == [file]` to
  widen on ambiguity. Two notes with the same basename in different
  folders will collide silently.
- **`getAllPropertyInfos` / `getFrontmatterPropertyValuesForKey`.**
  No `frontmatter_properties(key, widget, occurrences)` table; no
  enum-value query. `PropertyType.h` declares the enum but nothing
  populates a registry. Required for the Properties side panel
  (spec §9) and the Bases value picker (spec §15 cross-ref).
- **Subtag-prefix tag count map.** As above — no `getTags() →
  Record<tag, count>` exists.
- **`getBacklinksForFile` returning Obsidian's `BacklinkSet`
  multi-map shape.** Both back-end queries return only source-path
  *lists*; neither carries the per-link references with positions
  (`MetadataCacheReader::backlinksFor` returns `QStringList`,
  `SQLiteIndex::backlinksFor` returns `LinkInfo[]` *without*
  positions — `LinkInfo` has no offset fields). Spec §11 calls this
  out; gap remains.
- **`iterateRefs` / `iterateCacheRefs` helpers.** Not exposed at all
  for plugin code. The internal `xT(cache, cb)` walker is
  reimplemented inline at every call site. Consider adding free
  functions in the storage public header so plugins can walk
  `links` + `embeds` (Obsidian) or `links` + `embeds` +
  `frontmatterLinks` (extended) without duplicating logic.
- **`onCleanCache(fn)` "settle then snapshot" primitive.** Spec §11
  "Top-priority gaps". Obsidian's `runAsyncLinkUpdate` waits on this
  before snapshotting refs for a rename. Corbomite's
  `FileManager::renameFile` snapshots immediately
  (`FileManager.cpp:160-188`) — under burst load it could race with
  an in-flight parse. Add `whenIdle(std::function<void()>)`.
- **`isUnresolved(linktext, sourcePath)` query.** Used in Obsidian to
  italicise broken links live. Not present.
- **`getCachedFiles()` and `getFileInfo(path)` parity getters.**
  Corbomite has `allPaths()` and `getFileHash(path)`; the
  `FileCacheEntry` struct (mtime/size/hash) is not exposed via a
  read API.
- **Notice / progress UI for `inProgressTaskCount`.** No equivalent
  to Obsidian's `showIndexingNotice` polling — the status bar shows
  "Indexing complete" once on `indexFinished`
  (`MainWindow.cpp:2033-2035`) but no live percentage.
- **`linkUpdaters` per-extension registry.** Spec §7. Defer until
  canvas/bases need it (per spec).
- **`uniqueFileLookup` semantics: case-insensitive multi-map.**
  Implemented, but only in `LinkResolver` — not exposed publicly.
  Plugins that need to enumerate files-by-basename have no API.
- **`config-changed` hook.** No mechanism for `userIgnoreFilters` to
  rebuild on config change while the app is running. Filter is loaded
  once at scanner construction.
- **`showUnsupportedFiles` config gate.** Not honoured anywhere; all
  non-`.md` extensions are silently treated as unsupported via
  `onUnsupportedFile`, with no toggle.

## Notable translation successes

- **In-memory cache is the source of truth; SQLite is derived.**
  Cluster I's pivot — `SQLiteIndex` subscribing to
  `MetadataCache::cacheChanged` rather than parsing markdown itself
  (`SQLiteIndex.cpp:91-110`) — is the right shape: it eliminates the
  prior "two parsers, two truths" problem and makes the contract
  one-directional.
- **`Corbomite::Events` mixin alongside Qt signals.**
  `MetadataCache::events()` exposes Obsidian-string-keyed event names
  for plugin authors who want the JS-style API
  (`MetadataCache.h:148-149`); native consumers get typed
  `Q_SIGNALS`. Both fire from the same code path
  (`MetadataCache.cpp:336-339, 396-409`).
- **Hash-refcount GC.** `releaseHashRef` cleanly garbage-collects
  unreferenced metadata rows (`MetadataCache.cpp:301-314`),
  preventing the "two duplicate notes deleted in sequence leaves a
  dangling cache row" failure mode that an N→1 path-keyed scheme
  would otherwise have.
- **Destructive schema migration via `PRAGMA user_version = 2`.**
  Direct equivalent of Obsidian's IndexedDB v19 wipe; no migration
  scaffolding to maintain (`CachedMetadataStore.cpp:92-100`).
- **30 s autosave coalesce + immediate flush on `indexFinished`.**
  Beats Obsidian's `{ durability: "relaxed" }` — Corbomite is
  durable-by-default for completed bulk indexes
  (`MetadataCache.cpp:418-453`).
- **`LinkResolver` algorithm fidelity.** The 6-step rewrite
  (`LinkResolver.cpp:100-198`) is a faithful translation of spec §8.
  The same-folder bias for root-source files
  (`LinkResolver.cpp:184-185`) handles a corner case the JS code
  gets right via `startsWith(prefix)` on an empty prefix.

## Notable concerns / suspected bugs

- **`SQLiteIndex` does not write `frontmatterLinks` rows.**
  `writeRowsFromCache` indexes `cache.links` and `cache.embeds`
  (`SQLiteIndex.cpp:256-294`) but never iterates
  `cache.frontmatterLinks`. Consequences: SQL-driven backlinks
  (`SQLiteIndex::backlinksFor`) and orphan-link queries
  (`SQLiteIndex::orphanLinks`) miss every link declared in a
  property like `related: "[[Foo]]"`. The
  `MetadataCacheReader::backlinksFor` proxy walks
  `frontmatterLinks` (`MetadataCacheReader.cpp:87-91`), so two
  call paths return divergent results for the same query.
- **`SQLiteIndex::writeRowsFromCache` re-reads the file body from
  disk on every cache-change event** for the FTS `content` column
  (`SQLiteIndex.cpp:209-216`). For a vault save this means: parse →
  cache-change signal → re-read file. The `MetadataWorker::parsed`
  signal already carries the content bytes upstream
  (`MetadataWorker.h:51-62`), but the bytes are not threaded through
  `cacheChanged`. The audit suggests not threading them ("to keep the
  signal lean"); however it costs every save an extra disk read.
  Worth measuring on large vaults.
- **`drainOnePath` rewrites resolved targets in place.** When
  `link.link` arrives already in `path#subpath` form (parser stored
  that), it splits on `#` and re-resolves. But the parse-time
  resolver already set `link.link` to the resolved path
  (`MetadataParser.cpp:362-367`). So the re-resolve in drain is
  redundant for the common case. The only case where it matters is
  when the resolver state changes between parse and drain (e.g.
  another file appeared mid-burst). Worth checking if this can
  loop indefinitely if a resolver-mutation re-enqueues parses.
- **Resolved cache rewrite is destructive.**
  `drainOnePath` mutates `*cacheIt` directly — but `m_hashToCache`
  is keyed by content hash and shared across paths via dedup. If two
  paths share content hash and have different folder contexts, the
  resolution result depends on which path drains last. This is a
  correctness bug under hash-collision dedup with location-sensitive
  resolution. Suspected by inspection; needs a test to confirm.
- **Frontmatter link extraction grabs only the first wikilink/md-link
  per string leaf.** `collectFrontmatterLinks`
  (`MetadataParser.cpp:213-249`) calls `wikiRe.match(s)` and returns
  on the first hit. A YAML scalar like
  `summary: "see [[Foo]] and [[Bar]]"` produces only one
  `FrontmatterLinkCache` entry. Obsidian extracts every link in the
  string. Bug.
- **Ignored regex patterns silently dropped.**
  `IgnoreFilter::fromPatterns` skips invalid `/regex/`s without
  surfacing them (`IgnoreFilter.cpp:24-25`). User mistake → silent
  unfilteredness. Spec §3 notes Obsidian logs an error; Corbomite
  should at least `qWarning()`.
- **Footnote definitions: regex over body misses indented
  definitions and definitions inside callouts/blockquotes.**
  `MetadataParser.cpp:485-500` uses `^\[\^([^\]]+)\]:` MultilineOption.
  GFM/Obsidian permit indented continuation but the *first* line
  must start at column 0 — so this is mostly correct; however the
  regex captures the rest of the *line* only (`[^\n]*`), losing
  multi-line definition bodies. Position fidelity reduced.
- **Block-anchor regex matches anywhere `^id` appears at end-of-line**
  (`MetadataParser.cpp:548-550`). It does not exclude in-code blocks
  or YAML strings. False positives in code samples that mention
  `^myid` will pollute `cache.blocks`.
- **`getMarkdownFiles()` snapshot in `WikiLinkSuggest`.**
  `WikiLinkSuggest::getSuggestions` walks `m_vault->getMarkdownFiles()`
  on every keystroke (`WikiLinkSuggest.cpp:42-55`). For a 10k-note
  vault, this re-scans each frame. Should subscribe to vault events
  and cache.
- **`renameFile` does not re-emit `cacheChanged` for affected sources.**
  After patching the wikilinks of N source files,
  `FileManager::renameFile` (`FileManager.cpp:194-212`) calls
  `m_vault->process(...)` which writes to disk. The Vault's `modified`
  signal will eventually re-trigger `MetadataCache::onFileChanged`,
  but the round-trip is asynchronous. During the window, in-memory
  `CachedMetadata.links` for the source files still references the
  old path. Backlink panes will flicker.
- **`MainWindow.cpp:2033-2035` connects `indexFinished` after every
  vault load without disconnect.** The lambda captures `this` and
  re-attaches each call. Multiple vault-open cycles will show the
  status message multiple times per index-finish.

## Cross-cutting risks (event-shape, link-resolution algorithm differences)

1. **Event payload shape mismatch with Obsidian.** Obsidian's
   `changed` fires `(file, oldHash, cache)` — `oldHash` is a *string*.
   Corbomite's `cacheChanged` matches: `(path, prevHash, cache)`
   (`MetadataCache.h:152-154`). Good. But the plugin-event-bus
   variant uses `QVariantList` packing
   (`MetadataCache.cpp:337-339`), and `CachedMetadata` is wrapped in
   `QVariant::fromValue<CachedMetadata>`. Plugin authors writing
   `events.on("changed", [](path, prevHash, cache){...})` need a
   `qvariant_cast<CachedMetadata>` step — undocumented friction.
2. **`resolve` vs `resolved` semantic distinction.** Spec §4 makes
   the distinction explicit: per-file vs terminal. Corbomite names
   them `linksResolvedFor` and `allLinksResolved` — clearer than
   Obsidian's English-pluralisation collision but plugin authors
   coming from JS need a translation table; document it in
   `MetadataCacheReader.h` (currently absent).
3. **`finished` "initial-index complete" indistinguishability.**
   Inherited from Obsidian. Corbomite plugins that need
   "first-scan-done-once" must latch on first observation
   (`MainWindow.cpp:2033-2035` shows the host-side equivalent).
   Acceptable, but the plugin-development docs should call this out.
4. **Link-resolution algorithm: dot-relative path with extension.**
   `LinkResolver::resolve` step 4 (`LinkResolver.cpp:139-153`) does
   `if (!resolved.contains('.')) resolved += ".md"` — this differs
   from Obsidian, which appends `.md` only when the *original*
   linktext lacked an extension. Corbomite's check is on the
   resolved-folder concatenation, which can contain `.` in folder
   names (`my.folder/Note`). Suspected bug: a vault with a folder
   named `2026.04` plus a `Note` link inside it will fail to append
   `.md` and miss the resolution.
5. **Rooted-`/` path with extension.** Step 5
   (`LinkResolver.cpp:120-130`) has the same `contains('.')` check.
   Same bug class.
6. **Same-folder bias is wrong for root-source.** Step 6
   (`LinkResolver.cpp:184-185`) treats a root-folder source +
   root-folder candidate (no `/` in candidate) as same-folder. But
   that branch is *only* reached when `sourcePrefix.isEmpty()` — so
   any candidate-with-`/` falls into `otherFolder`. Correct.
   However, if `sourcePath` itself is empty (no source context, e.g.
   resolver called with `""`), `folderOf("") == ""` →
   `sourcePrefix == ""` → same logic; behaviour matches Obsidian's
   "no source path" → all candidates go to `otherFolder` except
   root-only ones. OK.
7. **Case sensitivity.** `LinkResolver` lowercases everywhere
   (`LinkResolver.cpp:75-80, 144, 159, 173, 180`). Obsidian likewise.
   But the actual file-system may be case-sensitive — there is no
   hookup to `CaseSensitivityProbe`
   (`libs/storage/include/corbomite/storage/CaseSensitivityProbe.h`).
   Two files `Notes.md` and `notes.md` on a case-sensitive vault
   would both be inserted into the same lowercase bucket and the
   resolver would return whichever appears first in the bucket
   (`m_nameToPaths.value(...)` first element). Spec §8 doesn't make
   `case-sensitive vault` a first-class concern but Corbomite ships
   on Linux where this matters.
8. **`unresolvedLinks` absence cascades.** Without a reverse map of
   unresolved links, the spec §9 user features "wikilink
   auto-completion popup", "unresolved links render
   italic-underlined", "live-relink on target creation" all break.
   `MetadataCache::drainOnePath` re-resolves in place, so when a
   target appears, the source's `link.link` updates on next drain —
   but no signal fires for "Foo just resolved", so consumers that
   render link state must subscribe to every `linksResolvedFor` and
   diff. That's wasteful and prone to UI staleness.
9. **`SQLiteIndex.repairLinks` is dead code in steady state.** It
   does a synchronous `UPDATE links SET target_path = ? WHERE
   target_path = ?` (`SQLiteIndex.cpp:680-682`) marked as a
   pre-Cluster-I vestige in the source comment
   (`SQLiteIndex.cpp:609-617`). Risk: if anything still calls it,
   it will desync the SQL view from the in-memory cache. Audit
   call sites and either delete or wire to re-emit `cacheChanged`.
