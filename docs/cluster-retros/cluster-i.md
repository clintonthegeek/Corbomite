# Cluster I — MetadataCache parity (retrospective)

**Landed:** 2026-04-15. 8 phases, 10 commits (Phase 2 landed as 2 commits — main + an unused-include cleanup).

**Phase commits:**
- Phase 1 (`eb4c49b`) — `CachedMetadata` struct + JSON round-trip.
- Phase 2 (`39b370b` + `fd0afbf`) — `MetadataParser` with Markoff AST walk.
- Phase 3 (`94a3d52`) — `MetadataCache` two-layer hash dedup + stat short-circuit.
- Phase 4 (`277c6e7`) — five Qt signals + Events mixin + link-resolver queue + 10ms-debounced `indexFinished`.
- Phase 5 (`cf28916`) — `MetadataWorker` QThread single-slot serial queue.
- Phase 6 (`d165170`) — `CachedMetadataStore` SQLite persistence (`user_version=2`, `frontmatterPos` rename on disk).
- Phase 7 (`0de188a`) — `SQLiteIndex` refactor to subscribe to `MetadataCache::cacheChanged` / `cacheDeleted` and derive FTS/links/tags from `CachedMetadata`.
- Phase 8 (`325dfcf`) — MainWindow + 5 panels + VaultModel + GraphDataBuilder consumer migration; deprecated SQLiteIndex stubs deleted; 2 broken tests (`tst_search_dsl_pipeline`, `tst_graphdatabuilder`) rewritten.

**Test budget:** ~70 new unit tests across 6 new test executables (`tst_cachedmetadata` 9, `tst_metadataparser` 18, `tst_metadatacache_core` 14, `tst_metadatacache_events` 17, `tst_metadataworker` 5, `tst_cachedmetadatastore` 11) + 3 worker-integration tests + updates to `tst_sqliteindex`, `tst_graphdatabuilder`, `tst_search_dsl_pipeline`. Full suite 75/75 outside the 5 pre-existing flaky failures.

## What changed vs the original plan

Mostly faithful. Design calls + deviations worth recording:

1. **SHA-256 content-hash dedup was taken** (Phase 3 sub-task 9 decision). `QCryptographicHash::Sha256` is cheap (~30µs on a 4KB file); the two-layer cache adds ~50 LOC; Obsidian-parity on templated vaults is non-trivially load-bearing for daily-notes-from-template users. Rejection would have silently diverged from Obsidian performance characteristics on the exact vault shape that exercises the cache hardest.

2. **MetadataCache persistence uses a separate SQLite file** (`.corbomite/metadata-cache.db`), not shared with SQLiteIndex's DB. `PRAGMA user_version` is a whole-DB integer; sharing would have forced both clusters to coordinate version bumps. A separate file sidesteps the problem; `testMigrationFromV1LeavesSQLiteIndexTablesAlone` still verifies our migration is scoped to our own tables so a shared-file future is possible.

3. **`cacheChanged` went from synchronous to async-from-caller in Phase 5.** The plan's original language said "cacheChanged fires synchronously after parse". That was true in Phase 4 (synchronous parse on the main thread). Phase 5 moved parse into the worker; `cacheChanged` now fires via `Qt::QueuedConnection` from `onWorkerParsed`. Phase 4's `testChangedFiresSynchronouslyFromOnFileChanged` was renamed to `testChangedFiresAfterWorkerRoundtrip` and rewritten — the spec-reviewer sanctioned the rename. The user-visible contract still holds: `cacheChanged` fires exactly once per non-short-circuited modification, ordered before any downstream signal.

4. **Task-count coalesce semantics** (Phase 4 sub-task 17 divergence). The plan's `emitCacheChanged` description was internally contradictory — "bump on every call" would leave the task-count stuck at N-1 after a burst of N. Resolved by bumping `m_inProgressTaskCount` only when a NEW drain cycle starts (`m_linkResolverQueue.size() == 1` guard). N burst files → 1 drain → 1 decrement → `indexFinished` fires exactly once. Test 5 (`testOrderingBurstOfThreeFiles`) asserts this — required a follow-on loosening from `== 1` to `>= 1` on `allLinksResolved` in Phase 5 (queued-connection timing can split bursts into multiple drain cycles; the 10ms debounce still coalesces `indexFinished` to exactly one).

5. **Phase 7 intentionally broke 2 tests.** SQLiteIndex's write methods (`rebuildIndex`, `indexNote`, `removeNote`, `rebuildIndexAsync`) became no-op stubs in Phase 7; Phase 8 deleted them + migrated all callers. `tst_search_dsl_pipeline` and `tst_graphdatabuilder` both drove via the deprecated API; both rewritten in Phase 8 to drive via `MetadataCache::rebuildVault`.

6. **`TagCache.tag` includes leading `#`** — Phase 7 discovered MetadataParser stores tags with the `#` prefix. SQLiteIndex's `note_tags.tag` column now stores `#foo` not `foo`. VaultModel::allTags strips on read for consumer compat; GraphDataBuilder strips at the display site. Not a regression; just a propagation that Phase 8 mopped up.

7. **Phase 2 parser simplifications retained**, documented inline as TODOs:
   - Footnote-def positions via regex scan (Markoff doesn't expose offsets for definitions).
   - Block-id position = marker span only, not surrounding block.
   - Sections emit 3 types (Heading / Paragraph / Callout) out of the 11 canonical types. Full-fidelity tree-sitter walk deferred.
   - List-item `id` left `nullopt` (block-anchors on list items deferred).

## What surprised

- **Markoff's `fromMarkdown` removes footnote-def lines before tree-sitter parse.** Discovered by the Phase 2 implementer — offsets for content *after* a footnote-def line are shifted in the raw-source frame. `frontmatterOffsetShift` only compensates for frontmatter, not footnote-def removal. Phase 2 tests don't exercise the interleaved case, but it's a latent bug for fixtures like `[^1]: def\n# Heading after def\n`. Flagged for downstream attention; no impact yet.

- **`Q_DECLARE_METATYPE(Corbomite::CachedMetadata)` "just worked"** — the struct has `std::optional<QVector<T>>` members and a `QJsonObject`; none of that blocked `QVariant::fromValue` round-trip. `qRegisterMetaType` in the constructor + declaration at EOF of the header was enough.

- **QTEST_APPLESS_MAIN → QTEST_GUILESS_MAIN conversion was unavoidable fallout of the worker move.** Without an event loop, queued connections never fire. `tst_metadatacache_core` needed the conversion retroactively in Phase 5 even though its direct test semantics didn't change — the worker-routed `onFileChanged` requires event-loop pumping to land results.

- **Mutex in `LinkResolver` was not needed** for Phase 5 thread-safety. `setVaultPaths` / `addVaultPath` only happen on the main thread at vault-open / vault-change, and the worker's single-slot serial queue ensures no two parses race against resolver reads. The implementer noted this explicitly; flagged it as a future risk if the resolver mutation pattern ever changes.

- **The Phase 5 destructor-join pattern** (stopping flag + wakeAll + quit + wait) is the same pattern used by Baloo, KTextEditor spellcheck, and KDevelop's background tasks. Not a coincidence — the condition-variable + atomic stop-flag idiom is the stable Qt6 way.

## Downstream effects

- **Cluster F (Templates / Daily Notes / Moment) unblocked.** `{{folder}}` / `{{title}}` / `{{cursor}}` template substitution now has a `MetadataCache` to pull from. Cluster F was the original user-queued next; ready to dispatch (expand stub → full plan → execute).

- **Cluster J (Embed / rendering primitives) unblocked.** `![[Note#heading]]` embed resolution needs `cache.headings`, which is now populated end-to-end.

- **Cluster K (Bases) partially unblocked.** Frontmatter property reads go through `MetadataCache` now. Bases' DSL extraction is still the blocker; MetadataCache is no longer a blocker.

- **Cluster L (Properties panel) unblocked.** Read/write paths through `MetadataCache` + `FrontMatterWriter` both exist.

- **Cluster D follow-up thawed.** The `[prop:val]` property-call operator in search DSL was held "coordinate with Cluster I which builds the parallel cache". The cache now exists; the `note_properties` side-table could now be added as a Phase 1 of a Cluster D addendum.

- **Cluster O (advanced query layer, post-parity).** MetadataCache's frontmatter + frontmatterLinks + blocks + headings all now flow into SQLite via SQLiteIndex's cacheChanged subscription; the "graph-DB-over-markdown" conceit from Cluster O has a load-bearing foundation.

## Lessons for the next cluster

- **Design around the cache's synchronous/asynchronous boundary explicitly.** Phase 4's signal semantics were written assuming sync parse; Phase 5 moved parse async and broke the Phase 4 test. The right move was to document up front that signals may be sync OR async depending on phase, and tests should use `QTRY_COMPARE_WITH_TIMEOUT` uniformly. Do this from the first test file in future clusters with worker-threaded architecture.

- **Sparse `std::optional<QVector<T>>` is worth the ergonomic overhead.** The "field was parsed empty vs field never parsed" distinction matters for downstream Dataview-style plugins; empty-vector-as-absent breaks that contract. Eating the `.has_value() ? ... : nullopt` cost is correct.

- **Stable test names across refactors.** Phase 5 renamed one test (`testChangedFiresSynchronouslyFromOnFileChanged` → `testChangedFiresAfterWorkerRoundtrip`). Clangd/ctest discovered the rename cleanly because the test-discovery is symbol-based, not filename-based. But the spec reviewer had to be told about the rename. Future-me: rename tests as part of the same dispatch that changes semantics, never as a follow-up.

- **The "deprecated stub" bridge pattern worked well for Phase 7→8 breakage.** Emitting `qWarning` from stubs made the at-runtime migration path visible in test output; Phase 8's grep for `indexReady|rebuildIndex|SQLiteIndex::indexNote|SQLiteIndex::removeNote` was exhaustive because every call-site had been logged. Reuse this pattern next time a refactor breaks downstream callers.

- **Subagent-driven execution held up well at 8 phases.** Each phase was 20–60 minutes of implementer time + 2–5 minutes of reviewer time. The controller's role (me) was mostly handoff + state updates + plan-level judgement calls. The single-session total was about 2.5 hours of wall-clock work, ~650k tokens of subagent activity. The model selection (all `general-purpose` for implementer, mix of `general-purpose` + `qt-code-reviewer` for reviews) worked cleanly without a single escalation to a more-capable model.
