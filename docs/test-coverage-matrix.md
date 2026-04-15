# Test Coverage Matrix — Seams × Lifecycles

> **Living document.** Rows = component seams (places where two components share state through disk, signals, or persistent objects). Columns = lifecycle scenarios. Cells = tested? (`✓` covered by named test, `~` partial, blank = gap, `BUG-…` = known-broken with regression test).

Refresh this matrix at the start of every test enrichment cycle. The **blanks are the work**; pick the highest-risk subset and fill them.

## Lifecycle dimensions

- **L1 — Fresh:** vault has never been opened by Corbomite (no `.corbomite/`).
- **L2 — Reopen:** second-or-later open of the same vault, persisted state present, disk unchanged.
- **L3 — Reopen-with-edit:** reopen after the vault was edited *outside* Corbomite between sessions (mtime/size changed).
- **L4 — Reopen-with-schema-bump:** reopen after a Corbomite schema-version bump dropped/altered persisted tables.
- **L5 — Vault switch:** open vault A, then open vault B without closing the process.
- **L6 — Mid-session mutation:** edit/create/delete a note inside Corbomite during a single session.
- **L7 — External mutation:** another process modifies a tracked file while Corbomite is running.
- **L8 — Crash recovery:** process killed mid-write; reopen and verify state coherence.

## Seams (Cycle 1 starter set — expand each cycle)

| Seam | L1 | L2 | L3 | L4 | L5 | L6 | L7 | L8 |
|---|---|---|---|---|---|---|---|---|
| MetadataCache ↔ CachedMetadataStore (persistence round-trip) | ✓ tst_cachedmetadatastore | | | | | ✓ tst_metadatacache_events | | |
| MetadataCache ↔ SQLiteIndex (cacheChanged → FTS/links/tags) | ✓ tst_sqliteindex | | | BUG-20260415-000 | | ✓ tst_sqliteindex | | |
| SQLiteIndex schema migration ↔ persisted index.sqlite | | | | | | | | |
| MetadataCache ↔ LinkResolver (resolver seeded before parse) | ✓ tst_metadatacache_worker_integration | | | | | | | |
| VaultModel ↔ disk (vault scan) | ✓ tst_vaultscanner | | | n/a | | ~ tst_filesystemadapter | | |
| MainWindow.loadVault — full wiring | | | | | ~ tst_vault_switch | | | |
| EditorViewManager ↔ session.json | ✓ tst_workspacestate | | | | | | | |
| Backlinks/OutgoingLinks panel ↔ SQLiteIndex (UI displays cache) | (none — Tier A scope) | | | | | | | |

## Notes

- A `~` means partial coverage — the seam is touched but the lifecycle isn't fully exercised. Treat as still-a-gap when prioritising.
- New seams discovered during a cycle: add a row at the end with empty cells.
- New lifecycle dimensions discovered: add a column; existing rows backfill blank.
