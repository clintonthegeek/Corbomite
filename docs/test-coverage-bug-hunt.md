# Test Coverage Bug Hunt — Inventory

> **Living document.** One row per bug discovered during a test enrichment cycle. Status flows: `Open` → `Test landed` → `Fixed` → `Verified`. Bugs stay in the table forever (do not delete fixed rows; they document regression coverage).

## Convention

- **ID:** `BUG-YYYYMMDD-NNN` (date of discovery + sequence). Stable across cycles.
- **Failing test:** `<suite>::<class>::<method>` — the `QEXPECT_FAIL` test that codifies the bug.
- **Cycle:** which enrichment cycle found it.

## Bugs

| ID | Title | Discovered | Cycle | Failing test | Severity | Status | Notes |
|----|-------|------------|-------|--------------|----------|--------|-------|
| BUG-20260415-000 | SQLiteIndex links stay empty after schema migration on stat-unchanged vault | 2026-04-15 | (pre-cycle) | tst_cross_session::linksRepopulateAfterSchemaBumpOnStatCleanReopen | High | Verified | Root cause + fix in commit landing this plan; codified by Task 4 as regression test. |
| BUG-20260415-001 | MetadataCache::rebuildVault doesn't reap entries for files no longer in the path list | 2026-04-15 | 1 | tst_cross_session::externalDeleteBetweenSessionsObservedOnReopen | Medium | Open | Persisted FileCacheEntry survives even though the file is gone. Risk: stale links in SQLiteIndex pointing at deleted source paths until a per-file event fires. |

## Cycle log

- **Cycle 1 (2026-04-15 — in progress):** First execution. See `docs/superpowers/plans/2026-04-15-test-enrichment-cycle.md`.
