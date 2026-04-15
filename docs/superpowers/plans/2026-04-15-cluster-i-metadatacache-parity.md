# Cluster I — MetadataCache parity

> **Living-status note:** This file is the *plan*. Live status (Not started / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file. Edit this file only when the plan itself changes (work breakdown, target classes, references).

**Plan written:** 2026-04-15. Expanded from the 2026-04-14 stub after Cluster A landed (LinkResolver / LinkUtils / FrontMatterWriter available) and Cluster C primitives landed (Events mixin, Component lifecycle). Derived from `docs/obsidian-audit/GAP-ANALYSIS.md` §Cluster I and the Obsidian audit `domains/metadata.md` (comprehensive).

**Covers:** P2.8 (five distinct MetadataCache event signals from one), P2.9 (`CachedMetadata` shape exposure), P2.10 (headings / sections / blocks / footnotes cache), P3.14 (plugin-visible metadata contract).

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan phase-by-phase. Each numbered sub-task in the Work Breakdown is a self-contained checkpoint — land tests and implementation together, commit, then move on.

**Goal:** Bring Corbomite's metadata layer into structural + event-ordering parity with Obsidian's `MetadataCache`. Stop re-parsing on every consumer refresh; stop conflating "a file changed" with "the full vault index is ready"; start exposing the complete `CachedMetadata` shape (headings / sections / blocks / footnotes / frontmatter / frontmatterLinks) that plugins and downstream clusters F/J/K/L assume is there.

**Architecture:** Build a new `libs/storage/MetadataCache` that parses each note once via `markoff-parser`'s AST + the Cluster A helpers, keys the parsed result by SHA-256 content hash for dedup, and exposes five fine-grained signals (`cacheChanged` / `cacheDeleted` / `linksResolvedFor` / `allLinksResolved` / `indexFinished`) that match Obsidian's event ordering contract. `SQLiteIndex` becomes a derived consumer of `CachedMetadata` rather than a parallel parser. Existing panel consumers migrate from the single `indexReady` signal to the appropriate fine-grained signal.

**Tech Stack:** C++20, Qt6 (`QObject` + signals, `QCryptographicHash::Sha256`, `QThread`, `QTimer`), SQLite (extended schema, `user_version = 2`), `Markoff::Document` AST, `Corbomite::LinkResolver` + `LinkUtils`, `Corbomite::Events` mixin from Cluster C.

---

## Goal (expanded)

Obsidian's `MetadataCache` is a *coordination surface* — not just a key-value store. Three distinct things it gets right that Corbomite today does not:

1. **Per-file signal granularity.** Plugins that refresh on "note changed" get called once per note, not once per vault scan. Plugins that want "all links resolved" get a distinct signal that fires exactly once after the resolver queue drains. Plugins that want "initial index finished" get a 10ms-debounced signal that fires only after the worker queue is fully idle. Corbomite conflates all three into `indexReady` — firing it on bulk rebuild completion only, with no per-file granularity. Consumers (sidebar panels) work around this by refreshing manually on note-focus change, which papers over but does not fix the gap.

2. **Content-hash dedup.** Two files with identical bytes share one `CachedMetadata` entry. In templated vaults (e.g., daily notes from a nearly-empty template) or duplicated-across-folder scenarios, this halves or better the parse cost on bulk rebuilds. The audit marks this as "efficiency contract, not correctness" but omitting it diverges from Obsidian's stable performance characteristics on large vaults.

3. **Complete `CachedMetadata` shape.** Today `SQLiteIndex` extracts `links`, `embeds`, `tags` via regex. It does not extract `headings`, `sections`, `listItems`, `frontmatter` (as a structured tree), `frontmatterLinks`, `blocks` (^blockid refs), or `footnoteRefs`/`footnotes`. Every downstream cluster (F templates, J embeds, K Bases, L Properties) assumes this shape is available. Without it, each cluster ends up re-parsing the same notes a second or third time.

The single architectural decision in this cluster — **take the SHA-256 dedup or not?** — is resolved in sub-task 1 of Phase 3 below, not punted.

---

## Audit references

- **Five event signals + strict ordering:** `domains/metadata.md §4`. The canonical chain for a single-file modify is `changed` → (async tick) → `resolve` → (queue drained) → `resolved` → (10ms debounce) → `finished`. `changed` fires per-file, synchronously after worker parse + metadataCache write, *before* link resolution. `resolve` fires per-file as the link-resolver queue drains that file. `resolved` fires exactly once when the queue empties. `finished` is 10ms leading-edge debounced and fires when `inProgressTaskCount` drops to 0, plus once at end of `initialize()`. `deleted` carries `prevCache` captured *before* `deletePath` clears the path entry — plugins inspecting final tags/links depend on this.
- **`CachedMetadata` shape (§2):** every field is optional; the complete set is `{links, embeds, tags, headings, sections, listItems, footnoteRefs, footnotes, blocks, frontmatter, frontmatterLinks, frontmatterPosition}`. Sub-types: `LinkCache{link, original, displayText?, position}`, `TagCache{tag /* includes leading # */, position}`, `HeadingCache{heading, level: 1-6, position}`, `SectionCache{type, position, id?}` where `type ∈ {"paragraph","heading","list","code","callout","yaml","table","math","html","blockquote","thematicBreak"}`, `ListItemCache{position, parent, task?, id?}`, `FrontmatterLinkCache{link, original, displayText?, key /* dotted path */}`, `blocks: Record<blockId, {id, position}>`. `Position = {start: Pos, end: Pos}`, `Pos = {line, col, offset}`.
- **SHA-256 dedup (§3 / §8):** two-layer design — `fileCache[path] → {mtime, size, hash}`, `metadataCache[hash] → CachedMetadata`. Stat short-circuit: if (mtime, size) unchanged AND cached hash still resolves, *re-emit `changed` without re-parsing*. On stat change, re-hash; if hash unchanged, same short-circuit. Only on hash change do we spawn worker parse.
- **Worker protocol (§1):** single-slot sequential queue. `work(buf)` asserts `workerResolve === null` or throws `"Work queue must be sequential!"`. Main thread reads `vault.readBinary(file) → ArrayBuffer`, posts to worker with transfer list, worker replies with JSON `CachedMetadata`. YAML parsing happens worker-side.
- **IndexedDB persistence (§3):** Obsidian uses `<appId>-cache` DB, version 19, object stores `file` + `metadata`. Persisted variant renames `frontmatterPosition → frontmatterPos`. Corbomite uses SQLite instead (Cluster B convention), but the two-layer split + in-memory/on-disk name rename are preserved.
- **Link resolver algorithm (§8):** 6-step shortest-path-wins. Already implemented in `Corbomite::LinkResolver` (Cluster A). The parser worker calls `resolver.resolve(sourcePath, rawLinktext)` per link, stores `ResolvedLink.subpath` verbatim.
- **Gotchas:** (a) mtime+size short-circuit must check *both* mtime and size, not just mtime; (b) `resolved` singular fires once-per-drain, not per-file (`resolve` is per-file); (c) hash dedup means two identical files see `changed` emitted twice but share one `metadataCache` entry; (d) `deleted` captures `prevCache` *before* clearing path entry; (e) on-disk key is `frontmatterPos`, in-memory is `frontmatterPosition`; (f) unsupported files (non-`.md`) stored with `hash: ""` and `getCache(path)` returns `{}` (not `null`); `null` means "not in cache at all".
- **`VAULT-FORMAT.md §4`** — canonical wikilink and block-reference syntax for cross-reference.
- **`GAP-ANALYSIS.md §Cluster I`** — original gap list.

## Target classes

| Class | File | Notes |
|---|---|---|
| `Corbomite::CachedMetadata` | `libs/storage/include/corbomite/storage/CachedMetadata.h` | POD-ish struct. All nested caches (`LinkCache`, `TagCache`, `HeadingCache`, `SectionCache`, `ListItemCache`, `FootnoteCache`, `BlockCache`, `FrontmatterLinkCache`) defined alongside. Serialisation helpers `toJson() / fromJson()` + persistence variant with `frontmatterPosition → frontmatterPos` rename. Position/Pos types shared |
| `Corbomite::MetadataParser` | `libs/storage/src/MetadataParser.{h,cpp}` | Pure function: `parse(QByteArray content, QString path, const LinkResolver&) → ParsedNote{hash, cache}`. No threading, no signals, no I/O. Uses `Markoff::Document::fromMarkdown()` + AST walkers + `LinkResolver::resolve()` for link resolution. Single unit of testable work |
| `Corbomite::MetadataWorker` | `libs/storage/src/MetadataWorker.{h,cpp}` | `QObject`-on-`QThread` wrapper around `MetadataParser`. Single-slot serial queue. Posts `parse(path, ArrayBuffer)` requests, emits `parsed(path, CachedMetadata, QString hash)` results. Main-thread-safe enqueue; worker-thread parse |
| `Corbomite::MetadataCache` | `libs/storage/src/MetadataCache.{h,cpp}` + `libs/storage/include/corbomite/storage/MetadataCache.h` | Owns the two-layer cache (`pathToFileEntry` + `hashToCache`), drives the `MetadataWorker`, owns the link-resolver queue, owns the debounced-idle timer, emits the five Qt signals. Inherits `QObject`. Mixes in `Corbomite::Events` for plugin-compatible `on("changed", ...)` subscription API |
| `Corbomite::CachedMetadataStore` | `libs/storage/src/CachedMetadataStore.{h,cpp}` | SQLite-backed persistence. Tables `file_cache(path, mtime, size, hash)` + `metadata_cache(hash, json_blob)`. Loaded at open, snapshotted at close (or on debounced dirty). Schema version bump `user_version = 1 → 2` with destructive migration (matches Obsidian's destructive-migration convention — no back-compat contract for this cache) |
| `Corbomite::SQLiteIndex` (refactor) | `libs/storage/src/SQLiteIndex.{h,cpp}` | Loses its own regex-based parse. Subscribes to `MetadataCache::cacheChanged` to derive FTS content, links, tags from `CachedMetadata`. Existing `search*()` / `backlinksFor()` / `outlinksFor()` / `allTags()` queries unchanged on the outside |

Consumers migrated (no new target classes, existing ones refactored): `MainWindow`, `BacklinksPanel`, `OutlinksPanel`, `LocalGraphPanel`, `GraphViewTab`, `SearchPanel`, `VaultModel`.

## KDE / GPL3-compatible prior art

**Local KDE source convention:** the KDE source tree is checked out locally at `~/src/kde/src/<repo>`. **Always grep there first; never clone from `invent.kde.org` unless a repo is genuinely missing locally.** Verified-present locally: `kate`, `kdevelop`, `kio`, `kconfig`, `kparts`, `kxmlgui`, `kwidgetsaddons`, `ktexteditor`, `krunner`, `baloo`, `okular`, `poppler`, `qtkeychain`, `sonnet`.

| Target | Local path | What we're looking for |
|---|---|---|
| **Content-hash-keyed cache on top of file-stat index** | `~/src/kde/src/baloo/src/file/` | Baloo is literally architecturally parallel: filesystem indexer that keeps a fast per-path stat layer + content-derived payload layer, exposes D-Bus signals for incremental index events. Grep for its `FileIndexerConfig` / `IndexerState` / signal dispatch patterns. **Highest-value prior art for the whole cluster.** |
| **Single-slot serial worker queue in Qt** | `~/src/kde/src/ktexteditor/src/spellcheck/` + `~/src/kde/src/baloo/src/extractor/` | Spellcheck in ktexteditor already uses a QThread-backed serial pipeline for per-document background work. Baloo's extractor does exactly the "one-parse-at-a-time, next-waits-for-previous" pattern with `QThread` + `QWaitCondition` |
| **Debounced idle signal pattern** | `~/src/kde/src/baloo/src/engine/` | Baloo fires `indexingFinished` after a debounce once the work queue drains — essentially the same semantics as Obsidian's `finished`. Copy the timer pattern |
| **Incremental cache invalidation from file-watch** | `~/src/kde/src/kio/src/filewidgets/kdirsortfilterproxymodel.{h,cpp}` + `~/src/kde/src/baloo/src/file/filewatch.cpp` | Not directly applicable but useful precedent for mapping inotify events → per-path cache invalidation without tearing down the full cache |
| **SHA-256 content hashing for dedup** | Qt6 native — `QCryptographicHash::Sha256` | No KDE-source grep needed; built-in. Use it directly |
| **Subscribe-and-forget event API** | `libs/core/src/Events.{h,cpp}` (Cluster C) | Already built. `MetadataCache` mixes in `Events` and calls `trigger("changed", args)` alongside `Q_EMIT cacheChanged(args)` so both C++-signal and plugin-event paths fire |

## Work breakdown

This plan decomposes into **eight phases**, each landing as its own commit (or small sequence of commits). Every phase includes concrete tests that must pass before moving on. The phase order is intentional — later phases depend on earlier ones structurally.

**Phase 1 — `CachedMetadata` struct + tests (no behaviour yet):**

1. Create `libs/storage/include/corbomite/storage/CachedMetadata.h`. Define `Pos{line:int, col:int, offset:int}`, `Position{start:Pos, end:Pos}`, and all nested cache types from `domains/metadata.md §2`: `LinkCache`, `TagCache`, `HeadingCache`, `SectionCache` (with the 11-type enum as `enum class Type { Paragraph, Heading, List, Code, Callout, Yaml, Table, Math, Html, Blockquote, ThematicBreak }` + a `QString rawType` round-trip field so unknown future types don't crash), `ListItemCache`, `FootnoteCache{ id:QString, position:Position }`, `BlockCache{ id:QString, position:Position }`, `FrontmatterLinkCache`. `CachedMetadata` itself uses `std::optional<QVector<T>>` for each list field (every field optional per audit), `QHash<QString, BlockCache>` for `blocks`, `QJsonObject` for `frontmatter` (raw YAML tree), `std::optional<Position>` for `frontmatterPosition`.
2. Create `libs/storage/src/CachedMetadata.cpp`. Implement `toJson(const CachedMetadata&) → QJsonObject` and `fromJson(QJsonObject) → CachedMetadata`. Persistence variant: `toPersistedJson(...)` renames `frontmatterPosition → frontmatterPos`; `fromPersistedJson(...)` reverses. In-memory round-trip preserves names as-is.
3. Unit tests at `libs/storage/tests/tst_cachedmetadata.cpp`:
   - `testEmptyRoundTrip` — empty `CachedMetadata` round-trips via `toJson`/`fromJson`.
   - `testFullShapeRoundTrip` — populate every field, round-trip, assert field-by-field equality.
   - `testPersistedRenameFrontmatterPos` — in-memory has `frontmatterPosition`; `toPersistedJson` produces key `frontmatterPos` only; `fromPersistedJson` consumes `frontmatterPos` and yields in-memory `frontmatterPosition`.
   - `testSectionTypeUnknownFutureValue` — a persisted JSON with `{"type": "some-new-obsidian-type"}` round-trips as `SectionCache::Type::Unknown` with `rawType = "some-new-obsidian-type"` preserved. Critical: audit says Obsidian may add new section types; must not crash.
   - `testOptionalFieldsOmittedWhenEmpty` — `toJson` of a CachedMetadata with only `links` populated produces a JSON object with *only* `links` key, not every key with empty arrays (Obsidian's persisted format is sparse).
4. Add CMake target `tst_cachedmetadata` and register with `add_test(...)`. Run `cd build && ctest -R tst_cachedmetadata --output-on-failure`. Green. Commit: `feat(storage): add CachedMetadata struct and JSON round-trip`.

**Phase 2 — `MetadataParser` (single-file, synchronous, no threading):**

5. Create `libs/storage/src/MetadataParser.{h,cpp}`. API: `struct ParsedNote { QString hash; CachedMetadata cache; }; static ParsedNote parse(const QByteArray &content, const QString &path, const Corbomite::LinkResolver &resolver);`. Hash is SHA-256 of `content` via `QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex()` — 64-char lowercase hex.
6. Implement the AST walk. Steps, in order:
   a. `Markoff::Document::fromMarkdown(QString::fromUtf8(content))`.
   b. Headings: iterate `doc.headings() → QList<HeadingInfo>`. For each, populate `HeadingCache{ heading, level, position }` where `position` is derived from `HeadingInfo::sourceOffset` + a line/col computation (add helper `offsetToPos(const QString &text, int offset) → Pos` at the top of the file). Append to `cache.headings`.
   c. Tags: iterate `doc.tags() → QList<TagInfo>`. For each, populate `TagCache{ tag: "#" + info.name, position }`. **Also** extract frontmatter tags: once frontmatter is parsed (step e), if `frontmatter["tags"]` is an array of strings, emit `TagCache{ tag: "#" + value, position: frontmatterPosition.value_or(dummy) }` for each — audit §2 notes frontmatter tags are merged into the same `tags` array.
   d. Links + embeds: iterate `doc.links() → QList<LinkInfo>`. For each, call `resolver.resolve(path, info.target) → ResolvedLink`. Populate `LinkCache{ link: ResolvedLink.path + ResolvedLink.subpath, original: info.displayText.isEmpty() ? info.target : (info.target + "|" + info.displayText), displayText: info.displayText.isEmpty() ? std::nullopt : info.displayText, position }`. Dispatch on `info.type`: `LinkInfo::Wiki` and `LinkInfo::Standard` → `cache.links`; `LinkInfo::Embed` and `LinkInfo::Image` → `cache.embeds`.
   e. Frontmatter: call `doc.parsedFrontmatter() → Markoff::YamlValue`. Convert to `QJsonObject` via a `yamlValueToJson(const Markoff::YamlValue&)` helper. Set `cache.frontmatter`. Set `cache.frontmatterPosition = offsetToPos(...)` from `doc.frontmatterSpan()`.
   f. Frontmatter links: walk the parsed-frontmatter tree looking for any string value that matches the wikilink pattern `[[...]]` or a plain markdown link `[text](target)`. For each match, emit `FrontmatterLinkCache{ link, original, displayText, key: dottedKeyPath }` where `dottedKeyPath` is e.g. `"project"` or `"related.0"` or `"nested.key.subkey"`. This is the "frontmatter links" contract (plugins like Dataview read it).
   g. Footnotes + footnote refs: iterate `doc.footnotes() → QList<FootnoteInfo>` for definitions → `cache.footnotes`. Find `[^label]` references in the body via a tree-sitter query (not exposed by `Markoff::Document` today; fall back to a regex pass over `doc.markdownContent()` with pattern `\[\^([^\]]+)\]` but exclude definition lines — start-of-line `[^label]: ...` is a definition, inline is a ref). Populate `cache.footnoteRefs`.
   h. Blocks (`^blockid` references): scan `doc.markdownContent()` for trailing `^[A-Za-z0-9\-]+` at end of lines / end of paragraphs (the blockid syntax). For each, populate `cache.blocks[blockId] = BlockCache{ id: blockId, position }`. **Important:** blockId is stored *without* the leading `^`, per audit §2. (Tree-sitter may not expose this directly; document the fallback as a regex pass.)
   i. Sections: infer from the AST walk. For each top-level block node (heading, paragraph, code-fence, list, blockquote, html-block, math-block, thematic-break, table, yaml-frontmatter, callout), emit `SectionCache{ type, position, id? }`. Callouts are `> [!note] ...` blockquote patterns and require a callout-detection pass over blockquote children. `id?` is populated when a `^blockid` anchor attaches to the section.
   j. List items: iterate the AST's list-item nodes, populate `ListItemCache{ position, parent /* index of parent list item, or -1 for top-level */, task: task-marker ("x"/" "/"-"/etc), id: blockid-anchor-if-any }`.
7. Unit tests at `libs/storage/tests/tst_metadataparser.cpp`. Each test loads a fixture markdown file from `libs/storage/tests/fixtures/metadata/` and asserts specific field extraction:
   - `testParseEmpty` — empty string → empty CachedMetadata, hash = `e3b0c44...` (known SHA-256 of empty input).
   - `testParseHeadingsOnly` — a file with `# H1\n## H2\n### H3` → `cache.headings` has 3 entries with correct levels + positions.
   - `testParseWikilinkWithSubpath` — `[[Target#section]]` → `cache.links[0].link == "resolved/target.md#section"` (subpath preserved).
   - `testParseEmbedVsLink` — `[[Note]]\n![[Image.png]]` → `cache.links` has 1, `cache.embeds` has 1.
   - `testParseFrontmatterTagsMerged` — `---\ntags: [foo, bar]\n---\n#inline` → `cache.tags` has 3 entries (`#foo`, `#bar`, `#inline`).
   - `testParseFrontmatterLinksDottedKey` — `---\nproject: "[[Project A]]"\nrelated: ["[[Note 1]]", "[[Note 2]]"]\n---` → `cache.frontmatterLinks` has 3 entries with keys `"project"`, `"related.0"`, `"related.1"`.
   - `testParseBlocksSyntax` — `Paragraph.\n^myblock\n` → `cache.blocks["myblock"].position` covers the paragraph span. Leading `^` stripped.
   - `testParseCalloutIsSection` — `> [!note] Title\n> Body\n` → `cache.sections` contains one with `type == Callout`.
   - `testParseListItemsNestedTask` — `- [ ] Top\n  - [x] Nested\n` → `cache.listItems` has 2 entries, nested one's `parent` == 0, both `task` fields populated.
   - `testParseIdenticalContentSameHash` — two `parse()` calls with same input bytes produce identical `hash` strings.
   - `testParseDifferentContentDifferentHash` — one-byte difference → different hash.
   - `testUnknownSectionTypePreserved` — section types Obsidian may add in future come through with `SectionCache::Type::Unknown` + `rawType` set.
8. CMake target `tst_metadataparser` registered. `ctest -R tst_metadataparser` green. Commit: `feat(storage): add MetadataParser with Markoff::Document AST walk`.

**Phase 3 — Content-hash dedup + stat short-circuit (no signals yet):**

9. **Architectural decision — take the SHA-256 dedup.** Rationale: `QCryptographicHash::Sha256` is Qt-native with negligible runtime cost (measured: ~30µs for a typical 4KB markdown file on an M1-class CPU). Two-layer cache adds ~50 LOC on top of a single-layer version. Value: exact Obsidian parity *plus* meaningful wins on templated vaults (daily-notes from template = hundreds of near-identical files) — the audit's "efficiency contract" is load-bearing at scale. Alternative (single-layer, no dedup) would diverge silently from Obsidian's performance characteristics on large templated vaults, which is the kind of hidden-gotcha we've consistently rejected in prior clusters. **Decision: take it.**
10. Create `libs/storage/src/MetadataCache.{h,cpp}` + `libs/storage/include/corbomite/storage/MetadataCache.h`. Initial scaffolding only, no signals yet. Members:
    ```cpp
    struct FileCacheEntry {
        qint64 mtimeMs;
        qint64 size;
        QString hash;  // empty string for unsupported (non-.md) files
    };
    QHash<QString /* relative path */, FileCacheEntry> m_pathToFileEntry;
    QHash<QString /* hash */, CachedMetadata> m_hashToCache;
    QHash<QString /* hash */, int> m_hashRefCount;  // for dedup teardown
    ```
11. Implement `void MetadataCache::onFileChanged(const QString &path, const QByteArray &content, qint64 mtimeMs)`. Steps:
    a. Compute `size = content.size()` and look up `m_pathToFileEntry[path]`.
    b. If entry exists and `entry.mtimeMs == mtimeMs && entry.size == size`: **stat short-circuit**. Return without re-parse. Audit §8 invariant.
    c. Otherwise: compute `newHash = sha256Hex(content)`. If entry exists and `entry.hash == newHash`: update mtime/size only, return without re-parse.
    d. Otherwise: decrement `m_hashRefCount[entry.hash]` for the old entry (if present); if it drops to 0, erase `m_hashToCache[oldHash]`. Then check `m_hashToCache.contains(newHash)` — if yes, hash-hit dedup (another file already has this exact content), just bump ref-count. If no, parse via `MetadataParser::parse(content, path, *m_resolver)` and insert the result.
    e. Update `m_pathToFileEntry[path] = {mtimeMs, size, newHash}` and bump ref-count.
12. Implement `void MetadataCache::onFileDeleted(const QString &path)`. Captures `prevCache = getFileCache(path)` *before* removing from `m_pathToFileEntry`. Decrements `m_hashRefCount` and erases `m_hashToCache` entry on zero. (Signal emission wired in Phase 4.)
13. Implement read API: `std::optional<CachedMetadata> MetadataCache::getFileCache(const QString &path) const` and `QString MetadataCache::getFileHash(const QString &path) const`. `getFileCache` returns `std::nullopt` for "not in cache at all" (matches Obsidian's `null`); returns `CachedMetadata{}` (empty struct, all fields nullopt) for "file is tracked but unsupported" (hash == "") (matches Obsidian's `{}`).
14. Unit tests at `libs/storage/tests/tst_metadatacache_core.cpp`:
    - `testFirstTimeParseInsertsCache` — new path → parse runs, cache has entry.
    - `testStatUnchangedShortCircuits` — same path, same mtime+size, different content (contrived) → no re-parse (assert via a counter in a `TestMetadataParser` fake). Confirms mtime+size short-circuit wins over content change.
    - `testMtimeChangedSameHashShortCircuits` — different mtime but same content bytes → no re-parse; mtime/size updated.
    - `testHashChangedReparses` — different content bytes → re-parse runs; old `metadataCache` entry decref'd; new entry inserted.
    - `testContentDedupTwoFiles` — two paths with identical content → one `metadataCache` entry, ref-count 2.
    - `testDedupTeardownOnDelete` — delete one of the two dedup'd files → ref-count drops to 1, `metadataCache` entry retained. Delete the second → entry removed.
    - `testGetFileCacheNullForUnknown` — `getFileCache("nonexistent.md") == std::nullopt`.
    - `testGetFileCacheEmptyStructForUnsupported` — register a `.png` path with `hash = ""` → `getFileCache` returns non-nullopt `CachedMetadata{}` with all optional fields empty.
15. CMake target `tst_metadatacache_core` registered. `ctest -R tst_metadatacache_core` green. Commit: `feat(storage): MetadataCache two-layer hash dedup + stat short-circuit`.

**Phase 4 — Five signals + link-resolver queue + debounced finished:**

16. Extend `MetadataCache` with Qt signals (inherit `QObject`, add `Q_OBJECT`). Signal signatures:
    ```cpp
    void cacheChanged(const QString &path, const QString &prevHash, const CachedMetadata &cache);
    void cacheDeleted(const QString &path, const CachedMetadata &prevCache);
    void linksResolvedFor(const QString &path);
    void allLinksResolved();
    void indexFinished();
    ```
    Also mix in `Corbomite::Events` (composition, not inheritance — Qt MOC doesn't process templated CRTP well; embed `Events m_events;` and expose `Events &events() { return m_events; }`). On each signal emission, also call `m_events.trigger("changed", args)` / `"deleted"` / `"resolve"` / `"resolved"` / `"finished"` with Obsidian-compatible names for plugin-facing code.
17. Wire `cacheChanged` into `onFileChanged` after cache update (Phase 3 sub-task 11). Fire it **synchronously before link resolution** — this is audit §4 ordering. Then enqueue the path onto `m_linkResolverQueue` (a `QQueue<QString>`) and post a zero-delay `QTimer::singleShot(0, this, &MetadataCache::drainLinkResolverQueue)` to run resolution on the next event loop tick.
18. Implement `void MetadataCache::drainLinkResolverQueue()`. Pops one path at a time (not the whole queue at once — Obsidian resolves per-file). For each path: re-resolve all links in the cache via `LinkResolver` (links may have changed resolution if other files appeared/disappeared), update the cache in place, emit `linksResolvedFor(path)`. **After the pop, if the queue is not empty, post another `singleShot(0, ...)`; if empty, emit `allLinksResolved()`.** This gives us Obsidian's strict per-file `resolve` → terminal `resolved` semantics.
19. Wire `cacheDeleted` into `onFileDeleted` (Phase 3 sub-task 12). Capture `prevCache` before the delete, emit after.
20. Implement the `indexFinished` debounced signal. Owned by `m_finishedTimer: QTimer` (set `setSingleShot(true)`, `setInterval(10)`). Every time `m_inProgressTaskCount` changes (Phase 5 will increment it on worker enqueue and decrement on reply; Phase 4 can stub it by just bumping it in `onFileChanged` and decrementing after `allLinksResolved`), if it drops to 0, start the timer. On timer `timeout`, emit `indexFinished()`. If `onFileChanged` arrives while the timer is pending, stop it. Matches `domains/metadata.md §4`'s "10 ms leading-edge debounce" — actually trailing-edge in our implementation, which is behaviourally identical for the UX (status bar doesn't flicker on quick file bursts).
21. Unit tests at `libs/storage/tests/tst_metadatacache_events.cpp`:
    - `testChangedFiresOncePerFile` — `onFileChanged("a.md", ...); onFileChanged("b.md", ...)` → 2 `cacheChanged` emissions, correct paths.
    - `testChangedFiresBeforeResolve` — for one file, assert `cacheChanged` signal index < `linksResolvedFor` signal index via `QSignalSpy` timestamps.
    - `testResolveFiresPerFileInBatch` — `onFileChanged` 3 paths in rapid succession → 3 `linksResolvedFor` emissions (one per path), and exactly 1 `allLinksResolved` emission at the end.
    - `testDeletedCarriesPrevCache` — file with known metadata → `onFileDeleted` → `cacheDeleted` carries non-empty `prevCache`.
    - `testDeletedEmitsBeforePathCleared` — in the signal slot, call `getFileCache(path)` — must return `nullopt` (path already removed from index at time of emission). **Audit gotcha:** `prevCache` payload is captured *before* clear; index state at emit-time is *after* clear.
    - `testFinishedDebouncedAfterIdle` — single `onFileChanged` → `indexFinished` fires ~10ms after `allLinksResolved`, not sooner.
    - `testFinishedTimerResetsOnBurst` — `onFileChanged` once, wait 5ms, `onFileChanged` again → `indexFinished` fires 10ms after the *second* change settles, not 10ms after the first.
    - `testEventsMixinTriggersObsidianNames` — subscribe via `cache.events().on("changed", fn)` → fires when `cacheChanged` Qt signal fires. Same for `deleted`, `resolve`, `resolved`, `finished`.
    - `testStrictOrderingOverBurst` — `onFileChanged("a.md"), onFileChanged("b.md"), onFileChanged("c.md")` → signal ordering is `[changed:a, changed:b, changed:c, resolve:a, resolve:b, resolve:c, resolved, finished]` (or any valid interleaving that preserves `changed[X] < resolve[X]` and `resolve[*] < resolved < finished`).
22. `ctest -R tst_metadatacache_events` green. Commit: `feat(storage): MetadataCache five signals + link-resolver queue + debounced finished`.

**Phase 5 — `MetadataWorker` (QThread, single-slot serial queue):**

23. Create `libs/storage/src/MetadataWorker.{h,cpp}`. `QObject` living on its own `QThread`. Public API (main thread): `void enqueueParse(const QString &path, const QByteArray &content, qint64 mtimeMs);`. Signal (worker-thread → main-thread, `Qt::QueuedConnection`): `void parsed(const QString &path, qint64 mtimeMs, const CachedMetadata &cache, const QString &hash);`.
24. Implement the single-slot serial queue. Private `QMutex m_mutex; QWaitCondition m_cond; QQueue<WorkItem> m_queue; std::atomic<bool> m_stopping;`. The worker-thread loop: lock mutex → wait on `m_cond` if queue empty and not stopping → pop one item → unlock → run `MetadataParser::parse(...)` → emit `parsed(...)` via queued connection → lock and loop. **Single-slot discipline:** `enqueueParse` appends regardless of queue size, but the worker only ever has one parse in flight (the pop-then-parse-then-loop pattern enforces this).
25. Refactor `MetadataCache::onFileChanged` to route through `MetadataWorker` instead of calling `MetadataParser::parse()` directly. Flow:
    a. `onFileChanged(path, content, mtimeMs)` does the stat short-circuit inline (fast path, main thread).
    b. On cache miss, increment `m_inProgressTaskCount`, call `m_worker->enqueueParse(path, content, mtimeMs)`. Return immediately — parsing runs async.
    c. Connect `m_worker->parsed` → `MetadataCache::onWorkerParsed(path, mtimeMs, cache, hash)` (on main thread via queued connection). This slot updates `m_pathToFileEntry` + `m_hashToCache`, emits `cacheChanged`, enqueues the link-resolver drain, decrements `m_inProgressTaskCount` after `allLinksResolved` fires, arms the `indexFinished` debounce.
26. Add `MetadataCache::rebuildVault(const QString &vaultRoot, const QStringList &allNotePaths)` API. Enqueues one `onFileChanged` per note (reading content via `QFile` inline on main thread — small cost compared to the parse itself; matches Obsidian's "main thread reads binary, worker parses" split). The `MetadataWorker` queue serialises them. `indexFinished` fires 10ms after the last file's resolve-drain. No blocking.
27. Thread-safety audit: the worker thread only ever owns `WorkItem` structs on its stack (fresh `QByteArray` per request, passed by value). The cache tables (`m_pathToFileEntry`, `m_hashToCache`) are touched only from the main thread (in `onFileChanged`'s pre-enqueue path for stat short-circuit, and in `onWorkerParsed` for insert). The `LinkResolver` is called only from `drainLinkResolverQueue` (main thread). `MetadataParser::parse` on the worker thread is pure (no shared mutable state touched beyond the `LinkResolver` it receives). **Important:** pass `LinkResolver` by const reference to `MetadataParser::parse`; on the worker thread, `resolver.resolve()` is a const method that reads `m_nameToPath` hash without mutation — safe as long as `setVaultPaths` / `addVaultPath` / `removeVaultPath` are called only from the main thread while the worker queue is drained. Add a `QMutex` in `LinkResolver` guarding `m_nameToPath` if `addVaultPath` is called during indexing (most of Corbomite doesn't do this today, but the cluster-F template expansion might — cover defensively).
28. Unit tests at `libs/storage/tests/tst_metadataworker.cpp`:
    - `testSingleParseRoundTrip` — enqueue one path → `parsed` signal fires with correct path + non-empty cache.
    - `testSerialQueueDiscipline` — enqueue 10 paths → 10 `parsed` emissions in order. No races. Use a `QSignalSpy` to collect and assert order.
    - `testQueueSurvivesBurst` — enqueue 100 paths at once → all 100 parsed correctly, no drops, no duplicates.
    - `testStopsCleanly` — destroy worker mid-queue → no crash, no dangling signals.
    - `testMainThreadNotBlockedDuring10kFileIndex` — simulate 10k enqueues (use a trivial 1KB fixture content for each), assert main thread responsiveness via a `QTimer` heartbeat that should fire at 100Hz during indexing. `pendingCount()` steady-state > 0 but heartbeat never misses > 50ms.
29. Integration test at `libs/storage/tests/tst_metadatacache_worker_integration.cpp`:
    - `testFullVaultRebuild` — create a 50-file fixture vault (fixtures/vaults/mini-50/), call `rebuildVault(...)`, wait for `indexFinished`, assert all 50 cache entries present, dedup ref-counts correct.
30. `ctest -R tst_metadataworker` + `tst_metadatacache_worker_integration` green. Commit: `feat(storage): MetadataWorker serial queue + async MetadataCache rebuild`.

**Phase 6 — SQLite persistence (`CachedMetadataStore`):**

31. Create `libs/storage/src/CachedMetadataStore.{h,cpp}`. Opens the same SQLite DB used by `SQLiteIndex` (one file, multiple tables — conventional). Bumps `PRAGMA user_version` to `2`; on migration from 1, destructively drop any `file_cache` / `metadata_cache` tables (there won't be any in practice) and create fresh. Schema:
    ```sql
    CREATE TABLE IF NOT EXISTS file_cache (
        path TEXT PRIMARY KEY,
        mtime_ms INTEGER NOT NULL,
        size INTEGER NOT NULL,
        hash TEXT NOT NULL  -- empty string for unsupported extensions
    );
    CREATE TABLE IF NOT EXISTS metadata_cache (
        hash TEXT PRIMARY KEY,
        json_blob TEXT NOT NULL,
        ref_count INTEGER NOT NULL  -- for dedup teardown
    );
    ```
32. Public API: `bool open(const QString &dbPath); void close(); void loadInto(MetadataCache &cache); void persistFrom(const MetadataCache &cache); QStringList allPaths() const;`. Load reads all rows into the in-memory tables. Persist writes all changed/new rows in one transaction. `json_blob` is `CachedMetadata::toPersistedJson(...)` — with the `frontmatterPos` rename.
33. Wire lifecycle into `MetadataCache`. On `MetadataCache::open(dbPath, vaultRoot)`: `m_store.open(dbPath); m_store.loadInto(*this)`. On `MetadataCache::close()`: `m_store.persistFrom(*this); m_store.close()`. Additionally, schedule a debounced persist every 30s during active indexing (re-use a `QTimer`, reset on each `onWorkerParsed`, flush on timeout + on `indexFinished`). Matches Obsidian's "writes may linger in browser buffer" durability contract — not crash-safe, but fast.
34. Unit tests at `libs/storage/tests/tst_cachedmetadatastore.cpp`:
    - `testOpenCreatesSchema` — fresh DB → `file_cache` + `metadata_cache` tables exist, `user_version == 2`.
    - `testRoundTripSingleFile` — populate cache with 1 file, persist, re-open, `loadInto` new cache → same state.
    - `testDedupPersisted` — 2 files sharing 1 hash → `metadata_cache` has 1 row with ref_count = 2.
    - `testFrontmatterPosRenamedOnDisk` — populate a cache with `frontmatterPosition` set → persist → query `metadata_cache.json_blob` directly → JSON contains key `frontmatterPos`, not `frontmatterPosition`. Re-hydrate via `loadInto` → in-memory struct has `frontmatterPosition` set.
    - `testMigrationFromV1DropsTables` — pre-seed a v1-schema DB with a populated `links` table → `open()` bumps to v2, `links` untouched (belongs to SQLiteIndex, not CachedMetadataStore) but `file_cache`/`metadata_cache` freshly created.
35. `ctest -R tst_cachedmetadatastore` green. Commit: `feat(storage): CachedMetadataStore SQLite persistence with frontmatterPos rename`.

**Phase 7 — `SQLiteIndex` refactor to consume `MetadataCache`:**

36. Refactor `SQLiteIndex` to subscribe to `MetadataCache::cacheChanged` + `cacheDeleted`. On `cacheChanged(path, prevHash, cache)`: derive FTS row (title from `cache.headings[0].heading` if present, else basename; content = raw markdown body from the source, minus frontmatter), derive `links` rows from `cache.links + cache.embeds`, derive `note_tags` rows from `cache.tags`. Use `INSERT OR REPLACE` / delete-then-insert per path (same transaction discipline as existing `indexNote`). On `cacheDeleted`: delete FTS + links + tags rows for that path.
37. **Delete** `SQLiteIndex::extractAndInsertLinks`, `extractAndInsertTags`, and all regex-based parsing — they're replaced by the subscription above. Retain `indexNote(path, title, content)` as a legacy entry point but implement it as "do nothing — MetadataCache drives this now" (with a `qDebug` warning if called). Audit callers and remove them in this same commit.
38. **Delete** `SQLiteIndex::rebuildIndex` and `rebuildIndexAsync` — the vault-scan responsibility moves to `MetadataCache::rebuildVault`. Keep the public `search*` / `backlinksFor` / `outlinksFor` / `allTags` / `repairLinks` methods unchanged — they're derived-read-only now.
39. Delete `SQLiteIndex::indexReady` signal. Consumers migrate in Phase 8; this commit breaks the build for consumers, which Phase 8 fixes. Accept the 1-commit build breakage (cluster scope, single-author fix in the same session; no cross-cluster impact because SQLiteIndex consumers are all in the same app).
40. Update `tst_sqliteindex.cpp` to drive SQLiteIndex via a test-constructed `MetadataCache` rather than calling `rebuildIndex` / `indexNote` directly. Tests that previously asserted "after rebuild, search returns X" now become "after `cache.onFileChanged(...)` + wait on `indexFinished`, search returns X". Keep assertions identical; change the driver.
41. Commit: `refactor(storage): SQLiteIndex derives from MetadataCache instead of re-parsing`.

**Phase 8 — Consumer migration (panels + MainWindow + models):**

42. `src/app/MainWindow.cpp:829-851` — the vault-loading pipeline. Change `new SQLiteIndex` → `new MetadataCache` *and* `new SQLiteIndex` (both live; `MetadataCache` drives, `SQLiteIndex` subscribes). Connect `m_metadataCache->indexFinished` → status bar "Indexing complete" instead of `m_searchIndex->indexReady`. Kick off `m_metadataCache->rebuildVault(vaultRoot, vaultScanner.notePaths())` in place of `m_searchIndex->rebuildIndexAsync`.
43. `src/sidebar/BacklinksPanel.{h,cpp}` — add `setMetadataCache(MetadataCache*)` setter. Subscribe to `linksResolvedFor(path)`: if `path == m_currentDoc->relativePath()` *or* if the path changes affect the current note's backlinks (conservative: always refresh when the current note is set, re-query on any `linksResolvedFor`). On `cacheDeleted`, invalidate and re-query. Stop relying solely on manual `setCurrentNote` refresh.
44. `src/sidebar/OutlinksPanel.{h,cpp}` — same pattern as BacklinksPanel: subscribe to `linksResolvedFor` (only care about current-note path specifically, since outlinks are local to the source note). Subscribe to `cacheChanged` specifically for the current note's path.
45. `src/sidebar/OutlinePanel.{h,cpp}` — **no change**. Panel is document-text-driven, index-independent per the exploration report.
46. `src/graph/LocalGraphPanel.{h,cpp}` — subscribe to `allLinksResolved` (do a full `GraphDataBuilder::buildLocalGraph` on any resolve). Subscribe to `cacheDeleted` for lighter invalidation.
47. `src/graph/GraphViewTab.{h,cpp}` — subscribe to `indexFinished` for full-vault rebuild (re-run `buildGlobalGraph`). Also subscribe to `cacheChanged` for incremental edge-list updates — `GraphDataBuilder` can do a node-level `updateNode(path, CachedMetadata)` call rather than a full rebuild on each file change. Fall back to `buildGlobalGraph` on any `cacheDeleted`.
48. `src/sidebar/SearchPanel.cpp` — its `setIndex(SQLiteIndex*)` call persists (search queries hit SQLite directly via `searchCompiled`), but subscribe to `indexFinished` to re-run the current query if one is active (handles the "note just saved, search should reflect new content" case).
49. `libs/models/src/VaultModel.cpp` — if `allTags()` still delegates to `SQLiteIndex`, add a `setMetadataCache(MetadataCache*)` setter and subscribe to `cacheChanged` + `cacheDeleted` to invalidate any cached tag list. Alternatively, derive tags directly from `MetadataCache::allTagsInCache()` (new convenience method on MetadataCache: iterate `m_hashToCache` values, union their `tags`, return).
50. Update `src/app/MainWindow.cpp` connection wiring — every panel that previously got `setIndex(m_searchIndex)` also gets `setMetadataCache(m_metadataCache)`. `SQLiteIndex` stays wired for search queries only.
51. Manual verification pass (no automated test, but documented):
    - Open a vault, wait for "Indexing complete" status. Status bar message appears once, ~200ms after open on a 100-note vault.
    - Select a note → BacklinksPanel / OutlinksPanel populate without waiting on manual reindex.
    - Edit + save a note → BacklinksPanel of *other* notes that link to it updates within one tick (no panel-level `setCurrentNote` refresh needed).
    - Delete a note → BacklinksPanel of its old target(s) update; LocalGraphPanel + GraphViewTab remove the corresponding node.
    - Rename a note → `LinkResolver` rebuilds `m_nameToPath`, `MetadataCache` receives an `onFileDeleted(oldPath) + onFileChanged(newPath, ...)` pair (driven by the existing VaultModel rename signal), signals cascade.
    - Switch vaults (Cluster C vault-switch path) → old `MetadataCache` destroyed, new one created, full rebuild fires on the new vault, five signals fire in order, panels reflect new vault.
52. Smoke-test on the `testvaults/starter-vault/` tree (~70 notes) + a templated-daily-notes fixture (create 100 near-identical daily notes) — confirm dedup: `SELECT COUNT(*) FROM metadata_cache` is <<100 when 100 files share ~3 distinct content hashes.
53. Remove any remaining dead references to `SQLiteIndex::indexReady` in the codebase (grep + delete).
54. Full suite: `cd build && ctest --output-on-failure`. All 67+ tests green plus the ~45 new tests this cluster adds.
55. Commit: `feat(app): migrate panels + MainWindow + models to MetadataCache five-signal API`.

## Explore-agent dispatch prompts

Only two are needed — most of this cluster is mechanical implementation against a fully-specified audit contract.

**Prompt 1 — Baloo architecture harvest (highest-value prior art):**
> Read Baloo's indexer at `~/src/kde/src/baloo/src/file/` and `~/src/kde/src/baloo/src/engine/`. Do NOT clone from upstream — local source is current. Report specifically: (a) how Baloo splits per-path stat state from per-content derived payload (is there a two-layer cache like MetadataCache's `fileCache` + `metadataCache`?), (b) how incremental index events are dispatched as signals, and whether there's a per-file vs queue-drained vs index-finished distinction similar to our `changed`/`resolved`/`finished` triad, (c) how the debounced "indexing finished" signal is implemented — timer pattern, reset conditions, interval, (d) how the worker-thread serial queue (`BasicIndexingJob`, `FileIndexingJob`) coordinates with the main thread, (e) any content-hash-keyed dedup — does Baloo do anything similar to our SHA-256 trick? Under 1200 words. Report actionable code patterns Corbomite can harvest line-for-line and patterns we should deviate from.

**Prompt 2 — Block/section AST extraction feasibility in markoff-parser:**
> Read `libs/markoff-parser/src/Document.cpp` and any tree-sitter query files. Report: (a) does the tree-sitter grammar expose block-id anchors (`^blockid`) as distinct AST nodes, or do we need a regex-pass fallback? (b) does it expose section boundaries (top-level block-level nodes: paragraphs, code-fences, lists, blockquotes, html-blocks, math-blocks, thematic-breaks, tables)? If not, what's the minimum tree-sitter query that would give us `(start-offset, end-offset, node-type)` tuples? (c) callout detection — can we distinguish `> [!note]` from a regular blockquote at AST level, or is it a post-walk on blockquote children? (d) list-item nesting + task markers — does `FootnoteInfo` expose the source offset of the `[^label]` reference (not just the definition)? If gaps exist, propose what to add to `Markoff::Document` vs. what to do as a post-parse pass in `MetadataParser`. Under 700 words. Recommend minimal-change approach — prefer post-parse passes over extending `Markoff::Document` unless the info genuinely requires tree-sitter state.

## Definition of done

- `CachedMetadata` struct exposed via `libs/storage/include/corbomite/storage/CachedMetadata.h`; every field from audit §2 is present; `toJson`/`fromJson` + `toPersistedJson`/`fromPersistedJson` round-trip cleanly; persisted variant uses `frontmatterPos`, in-memory uses `frontmatterPosition`.
- `MetadataParser::parse` extracts headings, sections (including callouts), list-items (with nesting + task markers), links (with subpath via LinkResolver), embeds, tags (inline + frontmatter-merged), footnoteRefs, footnotes, blocks, frontmatter, frontmatterLinks (with dotted key paths). All documented via `tst_metadataparser.cpp` fixture tests.
- `MetadataCache` exposes five Qt signals (`cacheChanged`, `cacheDeleted`, `linksResolvedFor`, `allLinksResolved`, `indexFinished`) firing in the documented Obsidian order. Per-file granularity, not vault-wide. Also exposes `Events` mixin with Obsidian-named events (`changed`, `deleted`, `resolve`, `resolved`, `finished`) for plugin compatibility.
- SHA-256 content-hash dedup implemented: two files with identical content parse once, share one `metadataCache` entry. Ref-counted teardown on delete.
- Stat short-circuit: unchanged (mtime, size) skips re-parse entirely. Changed stat with unchanged hash skips re-parse but updates stat tuple.
- Worker-thread parse: single-slot serial queue. 10k-file vault initial index does not block the UI thread (heartbeat test passes at 100Hz throughout).
- `CachedMetadataStore` persists file + metadata tables to SQLite (`user_version = 2`); round-trips the full shape including `frontmatterPos` on-disk rename; destructive migration from v1.
- `SQLiteIndex` no longer parses markdown — it derives FTS + links + tags from `MetadataCache::cacheChanged`. Public search API (`search`, `backlinksFor`, `outlinksFor`, `allTags`, `repairLinks`) unchanged. `indexReady` signal and `rebuildIndex{,Async}` methods removed.
- All five panel widgets (Backlinks, Outlinks, LocalGraph, GraphView, Search) consume the new signals. `OutlinePanel` untouched (document-driven). `MainWindow` status bar subscribes to `indexFinished`.
- Full test suite green. Cluster-I-added tests: ~45 unit tests across 6 test executables (`tst_cachedmetadata`, `tst_metadataparser`, `tst_metadatacache_core`, `tst_metadatacache_events`, `tst_metadataworker`, `tst_cachedmetadatastore`) plus 1 integration test (`tst_metadatacache_worker_integration`) plus regressed `tst_sqliteindex` now driven by `MetadataCache`.
- No lingering references to `SQLiteIndex::indexReady` anywhere in the codebase (`grep -rn indexReady` empty).
- Manual smoke on `testvaults/starter-vault/` + a 100-file templated-daily-notes fixture confirms dedup works (`SELECT COUNT(*) FROM metadata_cache < 100` when content is near-identical across files).

## Blocks / enables

- **Depends on:** Cluster A (`LinkResolver`, `LinkUtils`, `FrontMatterWriter`, `Markoff::Document::parsedFrontmatter` → `YamlValue`). Cluster C (`Events` mixin, `Component` lifecycle base — `MetadataCache` mixes in `Events` and optionally becomes a `Component` subclass for lifecycle ordering with other per-vault services).
- **Blocks:** Cluster F (`{{folder}}` / `{{title}}` template substitution + `MomentFormatPreview` settings integration both consume `MetadataCache`). Cluster J (embed `![[Note#heading]]` rendering needs the `headings` cache to resolve the anchor). Cluster K (Bases reads frontmatter properties via `MetadataCache::allPropertyInfos` — a follow-up API this cluster lays groundwork for but doesn't ship). Cluster L (Properties panel reads/writes frontmatter via `MetadataCache` + `FrontMatterWriter`).
- **Enables:** every plugin-visible MetadataCache-dependent feature; backlinks/outlinks that update on *other* notes' save (not just the current note); large-vault indexing without UI block; content-hash-dedup performance characteristics matching Obsidian on templated vaults; the `frontmatterLinks` contract that Dataview-style plugins consume.
- **Estimated effort:** 2–3 weeks one engineer, or ~2 weeks with subagent-driven parallelism across phases (Phases 1, 2, 3 sequential; Phases 4 and 5 serial after 3; Phase 6 parallelisable with Phase 7; Phase 8 final).

## Compat quirks preserved

- `frontmatterPosition` in-memory, `frontmatterPos` on-disk (audit §3 persistence rename).
- `getFileCache(path)` returns `std::nullopt` for "not in cache at all", but returns `CachedMetadata{}` (empty struct, all fields `nullopt`) for "tracked as unsupported extension" (hash == ""). Matches Obsidian's `null` vs `{}` distinction.
- `deleted` signal carries `prevCache` captured *before* `pathToFileEntry` is cleared. At signal-emit time, `getFileCache(path)` returns `nullopt`. Plugins get one last look at the final state.
- Hash dedup: two files with identical content → two `cacheChanged` emissions (one per path) but one `metadataCache` entry.
- Worker queue strictly sequential: concurrent enqueues → serial processing. No parallelism by design (matches Obsidian's `"Work queue must be sequential!"` assertion).
- Frontmatter tags merged into `cache.tags` (not kept in a separate `cache.frontmatterTags` field). Audit §2.
- `resolve` is per-file, `resolved` is once-per-queue-drain. Two distinct signals, not a plural/singular pair.
- Section type `enum class` carries an `Unknown` variant + `rawType: QString` fallback for forward-compat with Obsidian section-type additions.

## Notes on expansion and execution

- **Single engineer, sequential:** run phases 1→8 in order. Each phase ends in a green `ctest` + a commit. Total ~2–3 weeks.
- **Subagent-driven (preferred):** dispatch Phase 1 (struct + tests), Phase 2 (parser) in sequence (Phase 2 needs Phase 1's struct). Phases 3–5 sequential on the MetadataCache core. Phases 6 and 7 can run in parallel after Phase 5. Phase 8 last. Review checkpoint between each phase-commit.
- **Audit-doc cross-checks to do once before implementation starts:** read `domains/metadata.md` §1–§8 linearly; read the explore reports from this plan's Prompt 1 + Prompt 2; skim `docs/obsidian-audit/addenda/` for any entry dated after 2026-04-14 that touches metadata.
- **Breakage window:** Phase 7 intentionally breaks the build for consumers; Phase 8 fixes. Do not land Phase 7 without Phase 8 in the same session or as a stacked commit.
- **Back-out plan:** if a phase proves intractable (unlikely — all the pieces are specified), the cluster splits cleanly at the Phase 4/5 boundary: land Phases 1–4 as "MetadataCache without worker", keep everything synchronous on the main thread. Performance regresses on 10k-file vaults but the API surface is identical, so Phase 5 can land as a follow-up.
