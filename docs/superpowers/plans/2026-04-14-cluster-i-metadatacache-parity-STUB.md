# Cluster I — MetadataCache parity (STUB)

> **Living-status note:** This file is the *plan* (stub). Live status (Stub plan / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file. Edit this file only when the plan itself changes (or when expanded to a full plan — at which point rename to drop `-STUB` suffix and update INDEX).

**Plan written:** 2026-04-14 (stub). Expand to full plan when Cluster A has landed (this cluster depends on `LinkUtils`/`LinkResolver` from A) and Cluster C is in flight (this cluster's events ride on the `Events<T>` mixin from C).

**Covers:** P2.8 (five distinct MetadataCache event signals from one), P2.9 (`CachedMetadata` shape exposure), P2.10 (headings / sections / blocks / footnotes cache), P3.14 (plugin-visible metadata contract).

## Goal

Bring Corbomite's `SQLiteIndex` + `VaultModel` into structural and event-ordering parity with Obsidian's `MetadataCache`. The audit (`domains/metadata.md`) gave the exact `CachedMetadata` shape, the five event names with strict ordering (`changed` → `resolve` → `resolved` → `finished` + `deleted`), and the SHA-256 content-hash dedup architecture. Most of this cluster is mechanical implementation against a fully-specified contract; one architectural decision — whether to implement content-hash dedup — is the only design call.

## Audit references

- **Five event signals + ordering:** `domains/metadata.md §4` — `changed` (per-file parse done, before link resolution) → `resolve` (per-file link resolution done) → `resolved` (link-resolver queue drained) → `finished` (10ms debounced idle). `deleted` carries `prevCache` not `null`.
- **`CachedMetadata` shape:** `domains/metadata.md §2` — `{links, embeds, tags, headings, sections, listItems, frontmatter, frontmatterLinks, blocks, frontmatterPosition, ...}`. Every field with inferred TypeScript shape.
- **SHA-256 content-hash dedup:** `domains/metadata.md §3` — `metadataCache` keyed by SHA-256 of file content, `fileCache[path].hash` points into it. Duplicates share one parse. Efficiency contract, not correctness.
- **Worker-driven incremental parse:** `domains/metadata.md §1` (worker protocol section) — single-slot sequential queue, binary content transferred to worker, YAML parsing worker-side.
- **IndexedDB persistence:** `domains/metadata.md §3` — `<appId>-cache` DB, version 19, object stores `file` + `metadata`. Persisted variant renames `frontmatterPosition → frontmatterPos`. Obsidian does NOT write a vault-disk cache file — only IndexedDB.
- **Link resolution algorithm:** `domains/metadata.md §8` (consumed via Cluster A's `LinkResolver`).

## Target classes

| Class | File | Notes |
|---|---|---|
| `Corbomite::CachedMetadata` | `libs/storage/include/corbomite/storage/CachedMetadata.h` | Struct mirroring Obsidian shape. Fields: `links`, `embeds`, `tags`, `headings`, `sections`, `listItems`, `frontmatter` (QJsonObject), `frontmatterLinks`, `blocks`, `frontmatterPosition` |
| `Corbomite::MetadataCache` | `libs/storage/src/MetadataCache.{h,cpp}` | Owns: SQLite-backed cache (replaces vault-disk dependency), Qt signals for the 5 events, link-resolver queue, debounced-idle timer |
| `Corbomite::MetadataParser` | `libs/storage/src/MetadataParser.{h,cpp}` | Worker thread that parses one note → CachedMetadata. Single-slot serial queue. Inputs: raw markdown bytes + path. Outputs: CachedMetadata + content hash |
| `Corbomite::MetadataWorker` | `libs/storage/src/MetadataWorker.{h,cpp}` | Qt thread wrapper around MetadataParser; serial promise-chain semantics |
| `Corbomite::SQLiteIndex` (extend) | `libs/storage/src/SQLiteIndex.{h,cpp}` | Migrate to consume `MetadataCache` rather than re-parse |

`VaultModel` exposes the new signals; `BacklinksPanel`, `OutlinksPanel`, `OutlinePanel`, `LocalGraphPanel`, `GraphViewTab` migrate from the existing `indexReady` single signal to the appropriate fine-grained signal.

## Sub-tasks (when expanded)

1. **Define `CachedMetadata` struct** matching `metadata.md §2` exactly. Build from existing per-file parse output in `SQLiteIndex` (it already extracts most of these — formalise into a struct).
2. **Add headings / sections / blocks / footnotes parsers** if missing from current `SQLiteIndex` parse path. `libs/markoff-parser/` (tree-sitter) likely exposes the ranges already; thin wrapper to extract.
3. **Define five Qt signals** on `MetadataCache`: `cacheChanged(QString path, CachedMetadata)`, `linksResolved(QString path, CachedMetadata)`, `allLinksResolved()`, `indexFinished()`, `cacheDeleted(QString path, CachedMetadata prevCache)`. Match Obsidian event names where possible (`changed`/`resolve`/`resolved`/`finished`/`deleted`).
4. **Implement strict event ordering**: `cacheChanged` fires synchronously after parse; per-file link resolution then emits `linksResolved`; when the resolver queue drains, `allLinksResolved`; 10ms idle debounce → `indexFinished`. Test orderings by emitting cross a 100-file vault and asserting sequence.
5. **Decide content-hash dedup** (one architectural call): is the SHA-256-keyed dedup worth implementing in Corbomite v1? Pro: exact Obsidian parity, faster on vaults with duplicate files. Con: code complexity for a marginal vault-format edge case. Defer with a comment if no.
6. **Worker-thread architecture**: `MetadataWorker` owns a `QThread`, `MetadataParser` runs there, communication via signals. Single-slot serial queue (next parse waits for previous). Match Obsidian's "Work queue must be sequential!" assertion.
7. **Migrate `SQLiteIndex` consumers** (Backlinks, Outlinks, Outline, LocalGraph, GraphView panels) to the new fine-grained signals. Backlinks subscribes to `linksResolved`; Outlinks too; LocalGraph + GraphView listen for `allLinksResolved`; status bar listens for `indexFinished` for the "Indexing complete" notice.
8. **Persist cache to SQLite** (not IndexedDB — but same architectural intent: cache lives outside the vault, keyed by content hash if dedup landed; otherwise by path). Schema migration handler for version bumps.

## Explore prompts

> *(None required — this cluster is direct port from a fully-specified audit. Skip exploration; expand straight to full plan when dependencies thaw.)*

If content-hash dedup is taken: one prompt to evaluate Qt's available SHA-256 implementations (`QCryptographicHash::Sha256` is built-in — likely answer is "use it directly").

## Definition of done

- `CachedMetadata` struct exposed via `libs/storage/include/corbomite/storage/`; all fields populated for every parsed note.
- Five distinct Qt signals on `MetadataCache` fire in the documented order; existing single `indexReady` signal deprecated and removed once consumers migrate.
- All five panel widgets (Backlinks, Outlinks, Outline, LocalGraph, GraphView) consume the new signals.
- Headings / sections / blocks / footnotes are present in CachedMetadata (cross-check against a sample vault via Obsidian's exposed `app.metadataCache.getFileCache(file)` API output if a sample is procurable).
- Worker-thread parse: 10k-file vault initial index does not block the UI thread.
- (Optional, if taken) content-hash dedup: a vault with duplicated files parses each unique content once.

## Blocks / enables

- **Depends on:** Cluster A (`LinkResolver`, `LinkUtils`, `FrontMatter` — used by parser), Cluster C (`Events<T>` mixin facade for the five signals).
- **Blocks:** Cluster F (`{{folder}}` / `{{title}}` template substitution wants metadata access), Cluster J (`![[Note#heading]]` embed needs heading cache), Cluster K (Bases reads frontmatter through MetadataCache), Cluster L (Properties panel reads/writes frontmatter via MetadataCache).
- **Enables:** every plugin-visible MetadataCache feature; backlinks/outlinks faithful update timing; large-vault indexing without UI block.
- **Estimated effort:** 2–3 weeks one engineer. Sub-task 5 (dedup decision) and sub-task 7 (consumer migration) are the largest sub-projects.

## Notes on expansion

When expanding to full plan:
- Read `libs/storage/src/SQLiteIndex.cpp` exhaustively first — much of the parse work probably already exists in some form; this cluster mostly restructures and signals more granularly.
- Read `src/sidebar/{BacklinksPanel,OutlinksPanel,OutlinePanel}.cpp` and `src/graph/{LocalGraphPanel,GraphViewTab}.cpp` to inventory consumers and the exact signal-wiring changes.
- Decide dedup explicitly with a one-paragraph rationale rather than punting.
