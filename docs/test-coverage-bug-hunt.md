# Test Coverage Bug Hunt — Inventory

> **Living document.** One row per bug discovered during a test enrichment cycle. Status flows: `Open` → `Test landed` → `Fixed` → `Verified`. Bugs stay in the table forever (do not delete fixed rows; they document regression coverage).

## Convention

- **ID:** `BUG-YYYYMMDD-NNN` (date of discovery + sequence). Stable across cycles.
- **Failing test:** `<suite>::<class>::<method>` — the `QEXPECT_FAIL` test that codifies the bug.
- **Cycle:** which enrichment cycle found it.

## Bugs

| ID | Title | Discovered | Cycle | Failing test | Severity | Status | Notes |
|----|-------|------------|-------|--------------|----------|--------|-------|
| BUG-20260415-000 | SQLiteIndex links stay empty after schema migration on stat-unchanged vault | 2026-04-15 | (pre-cycle) | n/a — fixed pre-test | High | Fixed | Root cause + fix in commit landing this plan; codified by Task 5 below as regression test. |

## Cycle log

- **Cycle 1 (2026-04-15 — in progress):** First execution. See `docs/superpowers/plans/2026-04-15-test-enrichment-cycle.md`.
