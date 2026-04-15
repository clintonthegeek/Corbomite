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
| BUG-20260415-001 | MetadataCache::rebuildVault doesn't reap entries for files no longer in the path list | 2026-04-15 | 1 | tst_cross_session::externalDeleteBetweenSessionsObservedOnReopen | Medium | Verified | Fixed 2026-04-15: `rebuildVault` now snapshots tracked paths, computes the set difference against the caller-supplied canonical list, and invokes `onFileDeleted` for each stale entry. QEXPECT_FAIL wrappers removed; test passes naturally. |

## Cycle log

- **Cycle 1 (2026-04-15):** First execution of Ritual 4. Targeted 6 cells across `MetadataCache ↔ CachedMetadataStore`, `MetadataCache ↔ SQLiteIndex`, `MainWindow.loadVault`, and `Backlinks/OutgoingLinks panel ↔ SQLiteIndex`. Lifecycles covered: L2, L3 (edit + delete arms), L4, L5, L6. Tests landed: `tst_cross_session::{linksRepopulateAfterSchemaBumpOnStatCleanReopen, reopenWithStatCleanIsSilent, externalEditBetweenSessionsTriggersReparse, externalDeleteBetweenSessionsObservedOnReopen, vaultSwitchDoesNotLeakLinksFromPreviousVault, orphanLinkAppearsAfterTargetDeleted}` + `tst_panels_populated::{hubNoteShowsOneOutgoingLink, spokeNoteShowsOneBacklink}`. Bugs filed: BUG-20260415-001. All 6 cycle-scope tests pass (BUG-20260415-001's assertions are wrapped with `QEXPECT_FAIL`). See `docs/superpowers/plans/2026-04-15-test-enrichment-cycle.md`.
