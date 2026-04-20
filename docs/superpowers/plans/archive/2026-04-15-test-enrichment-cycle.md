# Test Enrichment Cycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stand up a recurring "test enrichment" ritual that systematically hunts for bugs in cross-component, cross-session, and UI-observable behavior — the blind spots of our current per-class unit suite. Each cycle catalogues seams × lifecycles, picks the highest-risk uncovered cells, and lands failing tests + a markdown bug inventory. **This plan is the template for the ritual; cycle 1 (this run) is its first execution.**

**Architecture:** Two-tier test strategy on top of the existing `tests/` tree:
- **Tier B — Cross-session scenarios** (in `tests/integration/`): drive `Storage + Models + Core` (no widgets, `QT_QPA_PLATFORM=offscreen`), simulate sequences like `open vault → mutate → close → reopen`, persistence schema bumps, vault switching with stale state. Catches the "second-open after schema migration" class of bug (the `links`-table-empty bug we just hit).
- **Tier A — UI smoke** (in `tests/e2e/`): launch real `MainWindow` on the developer display, open a known vault, assert that critical panels (Backlinks, Outgoing Links, Properties, Outline) display populated state. Marked as *display-required* per existing convention; not run in CI.

Bug findings land in two places per cycle:
- **Failing test** in the appropriate suite, marked with `QEXPECT_FAIL` (Qt Test) and a `BUG-<id>` reference.
- **Inventory entry** in `docs/test-coverage-bug-hunt.md` — one row per bug, status-tracked across cycles.

**Tech Stack:** Qt6 Test (`QTest`, `QSignalSpy`, `QTemporaryDir`, `QEXPECT_FAIL`), C++20, existing CTest integration via `ctest --output-on-failure`. No new dependencies.

---

## Audit references

This plan does not implement Obsidian behavior — it tests Corbomite's existing implementation against documented expectations. Relevant audit/state references:

- `docs/obsidian-audit/FEATURE-MATRIX.md` — defines what the system is supposed to do; rows here become test rows.
- `docs/obsidian-audit/VAULT-FORMAT.md` — defines on-disk persistence shape; informs the "schema migration" lifecycle dimension.
- `docs/obsidian-audit/GAP-ANALYSIS.md` — known divergences. Each gap is *not* a bug, but the boundary between "gap" and "bug" is itself worth testing.
- `CLAUDE.md` § "Long-term project state" — defines the cluster machinery; this plan slots beside the cluster plans as a recurring-maintenance project, not a cluster.

## Target classes (Cycle 1 scope)

The seams catalogue (Phase 1) will identify all seams; Cycle 1 commits to driving these classes from cross-session scenario tests:

- `Corbomite::MetadataCache` (libs/storage) — persistent cache, debounced persist, stat short-circuit
- `Corbomite::CachedMetadataStore` (libs/storage) — SQLite-backed metadata persistence
- `Corbomite::SQLiteIndex` (libs/storage) — FTS5 + links + tags index, derived from MetadataCache events
- `Corbomite::LinkResolver` (libs/storage) — vault path resolution
- `Corbomite::VaultModel` + `Corbomite::NoteService` (libs/models) — vault scan, note CRUD
- `Corbomite::MainWindow::loadVault` / `closeVault` (src/app) — full wiring, the seam-of-seams
- `Corbomite::EditorViewManager` + `session.json` persistence (src/editor) — workspace state restore

## KDE/Qt prior art

- `QEXPECT_FAIL` — Qt Test's idiomatic way to mark known-failing assertions without breaking the build. Reference: `~/src/qtbase/src/testlib/qtestcase.h`. Use `QEXPECT_FAIL("data tag", "BUG-xxx: short reason", Continue)` for soft expectations and `QEXPECT_FAIL("", "BUG-xxx", Abort)` when subsequent asserts can't safely run.
- `QSignalSpy` for asserting signal emission/order — already used heavily in `tst_metadatacache_events.cpp`.
- `QTemporaryDir` + a tiny `createFile` helper — pattern already in `tests/e2e/tst_vault_switch.cpp`. Reuse it in new tests instead of reinventing.
- `ctest` test-fixture support: see `cmake --help-policy CMP0110`. Future cycles may use fixtures for "set up populated vault, share across tests in a fixture group" — Cycle 1 keeps each test self-contained.

---

## Work breakdown — Cycle 1

Cycles always have the same shape:
1. **Catalogue** — refresh seams × lifecycles matrix.
2. **Prioritise** — pick N highest-risk uncovered cells (Cycle 1: `N=6`).
3. **Hunt** — for each cell, write a failing scenario.
4. **Inventory** — log each finding to `docs/test-coverage-bug-hunt.md`.
5. **Hand off** — list bugs by ID for the next implementation pass.

The plan below executes that shape once. **Re-running the cycle = re-executing this plan against an updated catalogue.**

---

### Task 1: Bootstrap the bug inventory + catalogue files

**Files:**
- Create: `docs/test-coverage-bug-hunt.md`
- Create: `docs/test-coverage-matrix.md`

- [ ] **Step 1: Create the bug inventory file**

```markdown
# Test Coverage Bug Hunt — Inventory

> **Living document.** One row per bug discovered during a test enrichment cycle. Status flows: `Open` → `Test landed` → `Fixed` → `Verified`. Bugs stay in the table forever (do not delete fixed rows; they document regression coverage).

## Convention

- **ID:** `BUG-YYYYMMDD-NNN` (date of discovery + sequence). Stable across cycles.
- **Failing test:** `<suite>::<class>::<method>` — the `QEXPECT_FAIL` test that codifies the bug.
- **Cycle:** which enrichment cycle found it.

## Bugs

| ID | Title | Discovered | Cycle | Failing test | Severity | Status | Notes |
|----|-------|------------|-------|--------------|----------|--------|-------|
| BUG-20260415-000 | SQLiteIndex links stay empty after schema migration on stat-unchanged vault | 2026-04-15 | (pre-cycle) | n/a — fixed pre-test | High | Fixed | Root cause + fix in commit landing this plan; codified by Task 4 as regression test. |

## Cycle log

- **Cycle 1 (2026-04-15 — in progress):** First execution. See `docs/superpowers/plans/2026-04-15-test-enrichment-cycle.md`.
```

- [ ] **Step 2: Create the seams × lifecycles catalogue**

```markdown
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
```

- [ ] **Step 3: Commit the bootstrap files**

```bash
git add docs/test-coverage-bug-hunt.md docs/test-coverage-matrix.md
git commit -m "$(cat <<'EOF'
docs(test-enrichment): bootstrap bug inventory + seams matrix

Cycle 1 of the test enrichment ritual. The inventory tracks bugs
discovered during cross-session/UI testing across cycles; the matrix
tracks which seams × lifecycles have coverage so each cycle picks
the highest-risk gaps.
EOF
)"
```

---

### Task 2: Add the test enrichment ritual to CONTRIBUTING-OPS

**Files:**
- Modify: `docs/CONTRIBUTING-OPS.md` (append a new ritual section)

- [ ] **Step 1: Append Ritual 4 to CONTRIBUTING-OPS**

Open `docs/CONTRIBUTING-OPS.md`, locate the `## Ritual 3 — Cluster done` section (search for "## Ritual 3"), and append after the entire Ritual 3 section (after its `---` divider if present, or at end of file if not):

```markdown
---

## Ritual 4 — Test enrichment cycle (recurring)

Goal: after a cluster lands or after a multi-session push of code, hunt for the bugs that per-class unit tests miss — cross-component, cross-session, and UI-observable behaviour.

**When to run:** after Ritual 3 (cluster done), or any time a multi-day implementation push has merged ≥10 commits without a coverage check, or on a fixed cadence (e.g. "every other cluster"). The human triggers the cycle; the agent executes it.

Execute every step in order.

1. **Open the matrix.** `docs/test-coverage-matrix.md`. Skim the seams and lifecycle columns. Add any new seam introduced by recent work as a new row (blank cells); add any new lifecycle as a new column.
2. **Refresh the test inventory.** Walk the existing tests in `tests/`, `libs/*/tests/` — for each test, identify which seam × lifecycle cell(s) it covers. Update the matrix cells (`✓ tst_<name>`).
3. **Pick the cycle's targets.** Choose N highest-risk blank cells (default `N=6`). "Highest-risk" = recently-touched code, persistence-heavy, or known to interact with multiple subsystems. Document the picks in a new "Cycle M" section in `docs/test-coverage-bug-hunt.md` under "Cycle log".
4. **For each target cell, write a failing scenario test.** Use Tier B (cross-session, in `tests/integration/`) by default. Use Tier A (`tests/e2e/`) only when the bug is UI-display-only. Each test:
   - Sets up state representing the lifecycle (e.g. for L4 "schema bump": pre-populate persisted DBs at version N, simulate version-N+1 open).
   - Asserts the expected behaviour.
   - If it passes: the cell wasn't a gap after all — update the matrix to `✓` and move on.
   - If it fails: this is a bug. Mint a `BUG-YYYYMMDD-NNN` ID, wrap the asserts with `QEXPECT_FAIL("", "BUG-xxx: <short reason>", Continue)`, append a row to the inventory table, update the matrix cell to the BUG-ID.
5. **Do NOT fix bugs in the same cycle.** The cycle is for hunting. Filing the bug = the deliverable. Fixes are scheduled separately by the human.
6. **Cycle close-out.** In `docs/test-coverage-bug-hunt.md`, update the cycle log entry with: cells targeted, bugs filed (by ID), tests landed (by name).
7. **Update PROJECT-STATE.** Add a "Recent decisions" bullet noting the cycle ran, link to the cycle's log entry.

**Do NOT** mark a cell `✓` based on any test that doesn't actually exercise the lifecycle dimension. "Tested in isolation" ≠ "tested in this lifecycle." When in doubt, leave the cell blank and add a partial-coverage `~` only if a test definitively covers part of the cell.
```

- [ ] **Step 2: Commit the ritual**

```bash
git add docs/CONTRIBUTING-OPS.md
git commit -m "$(cat <<'EOF'
docs(ops): add Ritual 4 — recurring test enrichment cycle

Codifies the test-enrichment workflow as a recurring ritual: refresh
the seams matrix, pick highest-risk gaps, write failing scenarios,
file bugs to the inventory. Hunt only — fixes scheduled separately.
EOF
)"
```

---

### Task 3: Create the cross-session scenario test target

**Files:**
- Create: `tests/integration/tst_cross_session.cpp`
- Modify: `tests/integration/CMakeLists.txt`

The cross-session test target is where Cycle 1's Tier B scenarios live. We register it once now; subsequent tasks add `private Q_SLOTS:` methods.

- [ ] **Step 1: Write the empty test scaffold**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cross-session scenario tests — Tier B of the test enrichment cycle.
// Each test method drives a sequence like "open → mutate → close → reopen"
// against the storage + models stack (no widgets). Targets seams × lifecycle
// cells from docs/test-coverage-matrix.md.
//
// Bugs discovered during a cycle are wrapped with QEXPECT_FAIL and a
// BUG-YYYYMMDD-NNN reference into docs/test-coverage-bug-hunt.md.

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "corbomite/storage/CachedMetadataStore.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"

using namespace Corbomite;

class TestCrossSession : public QObject {
    Q_OBJECT

private:
    static void writeFile(const QString &path, const QByteArray &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    }

    // Wait for a QSignalSpy to receive at least `target` emissions, polling
    // the event loop. Returns true on success, false on timeout.
    static bool waitForSpy(QSignalSpy &spy, int target, int timeoutMs = 3000)
    {
        QElapsedTimer timer;
        timer.start();
        while (spy.count() < target && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents();
            QTest::qWait(20);
        }
        return spy.count() >= target;
    }

private Q_SLOTS:
    void initTestCase()
    {
        // No app-wide setup needed; each test owns its QTemporaryDir.
    }
};

QTEST_MAIN(TestCrossSession)
#include "tst_cross_session.moc"
```

- [ ] **Step 2: Wire it into CMake**

Open `tests/integration/CMakeLists.txt`. After the `tst_search_dsl_pipeline` block (after line 25 — the `set_tests_properties(... tst_search_dsl_pipeline ...)` line), append:

```cmake

add_executable(tst_cross_session tst_cross_session.cpp)
add_test(NAME tst_cross_session COMMAND tst_cross_session)
target_link_libraries(tst_cross_session PRIVATE
    Qt6::Test Qt6::Sql Corbomite::Models Corbomite::Storage Corbomite::Core)
set_tests_properties(tst_cross_session PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Configure + build the empty target**

Run:
```bash
cmake --build build --target tst_cross_session 2>&1 | tail -10
```

Expected: build succeeds; `tst_cross_session` binary exists at `build/bin/tst_cross_session` (or equivalent).

- [ ] **Step 4: Run the empty test to confirm framework**

Run:
```bash
cd build && ctest -R tst_cross_session --output-on-failure
```

Expected: PASS with `0 tests` reported (only `initTestCase` / `cleanupTestCase` ran). This confirms the binary, MOC, linkage, and ctest registration all work before adding scenarios.

- [ ] **Step 5: Commit the scaffold**

```bash
git add tests/integration/tst_cross_session.cpp tests/integration/CMakeLists.txt
git commit -m "$(cat <<'EOF'
test(integration): scaffold tst_cross_session for Tier B scenarios

Empty test target wired into CMake + ctest. Subsequent Cycle-1 tasks
add private Q_SLOTS test methods for specific seams × lifecycle cells.
EOF
)"
```

---

### Task 4: Hunt — schema migration drops index but cache is stat-clean (regression for BUG-20260415-000)

This codifies the bug we just fixed as a permanent regression test. The fix landed pre-cycle so the test should PASS (not `QEXPECT_FAIL`). Inventory row already exists with status `Fixed`.

**Cell targeted:** `MetadataCache ↔ SQLiteIndex × L4 (schema bump)`.

**Files:**
- Modify: `tests/integration/tst_cross_session.cpp` (add a `Q_SLOTS:` method)

- [ ] **Step 1: Add the regression test**

In `tst_cross_session.cpp`, replace the `private Q_SLOTS:` block (currently containing only `initTestCase`) with:

```cpp
private Q_SLOTS:
    void initTestCase()
    {
        // No app-wide setup needed; each test owns its QTemporaryDir.
    }

    // BUG-20260415-000 (FIXED): SQLiteIndex's `links` and `note_tags` rows
    // were dropped by the user_version=1 migration in createTables(); on the
    // next vault open MetadataCache loaded its persisted state silently
    // (no cacheChanged), so SQLiteIndex stayed empty for any path whose
    // stat matched disk. Fix: SQLiteIndex::reconcileWithCache(), called
    // explicitly after MetadataCache::open() in MainWindow::loadVault, and
    // also from setMetadataCache() for direct-call scenarios.
    //
    // This test simulates Session 1 (populates both stores), drops the
    // index's `links` table to mimic a schema bump, then runs Session 2
    // (no on-disk file changes) and asserts that links re-populate.
    void linksRepopulateAfterSchemaBumpOnStatCleanReopen()
    {
        QTemporaryDir vaultDir;
        QVERIFY(vaultDir.isValid());

        const QString notePath = QStringLiteral("Note A.md");
        const QString linkedPath = QStringLiteral("Note B.md");
        writeFile(vaultDir.path() + QLatin1Char('/') + notePath,
                  QByteArrayLiteral("# Note A\n\nLink to [[Note B]] here.\n"));
        writeFile(vaultDir.path() + QLatin1Char('/') + linkedPath,
                  QByteArrayLiteral("# Note B\n"));

        const QString configDir = vaultDir.path() + QStringLiteral("/.corbomite");
        QDir().mkpath(configDir);
        const QString cacheDb = configDir + QStringLiteral("/metadata-cache.db");
        const QString indexDb = configDir + QStringLiteral("/index.sqlite");

        // ----- Session 1: populate both stores -----
        {
            LinkResolver resolver;
            resolver.setVaultPaths({notePath, linkedPath});

            MetadataCache cache(resolver);
            cache.open(cacheDb);

            SQLiteIndex index;
            QVERIFY(index.open(indexDb));
            index.setVaultRoot(vaultDir.path());
            index.setMetadataCache(&cache);

            QSignalSpy doneSpy(&cache, &MetadataCache::indexFinished);
            cache.rebuildVault(vaultDir.path(), {notePath, linkedPath});
            QVERIFY(waitForSpy(doneSpy, 1));

            // Sanity: link row exists after Session 1.
            const auto outlinks = index.outlinksFor(notePath);
            QCOMPARE(outlinks.size(), 1);
            QCOMPARE(outlinks.first().targetPath, linkedPath);

            cache.close();
            index.close();
        }

        // ----- Simulate schema bump: drop the links table out from under
        // the next-session SQLiteIndex. This mimics the on-disk effect of
        // a future user_version bump that wipes one table.
        {
            const QString conn = QStringLiteral("test_drop_links");
            {
                QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
                db.setDatabaseName(indexDb);
                QVERIFY(db.open());
                QSqlQuery q(db);
                QVERIFY(q.exec(QStringLiteral("DROP TABLE IF EXISTS links")));
                db.close();
            }
            QSqlDatabase::removeDatabase(conn);
        }

        // ----- Session 2: reopen, no disk mutation. Stat short-circuit
        // means MetadataCache emits no cacheChanged. The fix's
        // reconcileWithCache() must rebuild the dropped table.
        {
            LinkResolver resolver;
            resolver.setVaultPaths({notePath, linkedPath});

            MetadataCache cache(resolver);
            cache.open(cacheDb);  // Loads persisted state silently.

            SQLiteIndex index;
            QVERIFY(index.open(indexDb));  // Re-creates `links` (empty).
            index.setVaultRoot(vaultDir.path());
            index.setMetadataCache(&cache);  // Calls reconcileWithCache().

            // No rebuildVault — explicit; we want to prove reconcile alone
            // is sufficient. (loadVault calls both, but that's belt-and-braces.)

            const auto outlinks = index.outlinksFor(notePath);
            QCOMPARE(outlinks.size(), 1);
            QCOMPARE(outlinks.first().targetPath, linkedPath);

            const auto backlinks = index.backlinksFor(linkedPath);
            QCOMPARE(backlinks.size(), 1);
            QCOMPARE(backlinks.first().sourcePath, notePath);

            cache.close();
            index.close();
        }
    }
```

- [ ] **Step 2: Run the new test, verify PASS**

Run:
```bash
cmake --build build --target tst_cross_session 2>&1 | tail -5 && \
  cd build && ctest -R tst_cross_session --output-on-failure
```

Expected: `linksRepopulateAfterSchemaBumpOnStatCleanReopen` PASSES. (If it fails, the BUG-20260415-000 fix isn't actually working — investigate before moving on.)

- [ ] **Step 3: Update the matrix cell**

In `docs/test-coverage-matrix.md`, replace the cell `BUG-20260415-000` (row "MetadataCache ↔ SQLiteIndex", column L4) with:

```
✓ tst_cross_session::linksRepopulateAfterSchemaBumpOnStatCleanReopen (BUG-20260415-000 regression)
```

- [ ] **Step 4: Update the inventory row**

In `docs/test-coverage-bug-hunt.md`, in the `BUG-20260415-000` row, replace `Failing test: n/a — fixed pre-test` with:

```
tst_cross_session::linksRepopulateAfterSchemaBumpOnStatCleanReopen
```

and update Status from `Fixed` to `Verified`.

- [ ] **Step 5: Commit**

```bash
git add tests/integration/tst_cross_session.cpp \
        docs/test-coverage-matrix.md docs/test-coverage-bug-hunt.md
git commit -m "$(cat <<'EOF'
test(integration): regression for BUG-20260415-000 (schema-bump links drop)

Codifies the recently-fixed bug as a permanent regression test. Drives
two sessions against on-disk SQLite + metadata-cache files, drops the
links table between sessions to mimic a future schema bump, asserts
SQLiteIndex::reconcileWithCache() rebuilds it on the stat-clean reopen.
EOF
)"
```

---

### Task 5: Hunt — MetadataCache reopens cleanly with no signal flood (L2 baseline)

**Cell targeted:** `MetadataCache ↔ CachedMetadataStore × L2 (reopen)`. Likely-passes baseline test that proves silent reload, ensures we have a reference point for L2 across other seams.

**Files:**
- Modify: `tests/integration/tst_cross_session.cpp`

- [ ] **Step 1: Add the test method**

Add inside the `private Q_SLOTS:` block, after `linksRepopulateAfterSchemaBumpOnStatCleanReopen`:

```cpp
    // L2 baseline: reopening a vault whose disk hasn't changed must not
    // emit any cacheChanged or cacheDeleted signals. The audit's
    // "no content change, no event" rule. If this fails, the stat
    // short-circuit is broken and downstream FTS/index work will spuriously
    // reindex on every startup.
    void reopenWithStatCleanIsSilent()
    {
        QTemporaryDir vaultDir;
        QVERIFY(vaultDir.isValid());

        const QString notePath = QStringLiteral("Quiet.md");
        writeFile(vaultDir.path() + QLatin1Char('/') + notePath,
                  QByteArrayLiteral("# Quiet\n\nNo links here.\n"));

        const QString cacheDb =
            vaultDir.path() + QStringLiteral("/.corbomite/metadata-cache.db");
        QDir().mkpath(QFileInfo(cacheDb).absolutePath());

        // Session 1.
        {
            LinkResolver resolver;
            resolver.setVaultPaths({notePath});
            MetadataCache cache(resolver);
            cache.open(cacheDb);

            QSignalSpy doneSpy(&cache, &MetadataCache::indexFinished);
            cache.rebuildVault(vaultDir.path(), {notePath});
            QVERIFY(waitForSpy(doneSpy, 1));

            cache.close();
        }

        // Session 2 — stat unchanged.
        LinkResolver resolver;
        resolver.setVaultPaths({notePath});
        MetadataCache cache(resolver);
        cache.open(cacheDb);  // Silent.

        QSignalSpy changedSpy(&cache, &MetadataCache::cacheChanged);
        QSignalSpy deletedSpy(&cache, &MetadataCache::cacheDeleted);

        cache.rebuildVault(vaultDir.path(), {notePath});
        // Process pending events to drain any potential async work.
        QTest::qWait(50);
        QCoreApplication::processEvents();

        QCOMPARE(changedSpy.count(), 0);
        QCOMPARE(deletedSpy.count(), 0);

        cache.close();
    }
```

- [ ] **Step 2: Run, observe outcome**

Run:
```bash
cmake --build build --target tst_cross_session 2>&1 | tail -5 && \
  cd build && ctest -R tst_cross_session --output-on-failure
```

Expected outcome: PASS. If it FAILS, that's a bug — file it as `BUG-20260415-001`, wrap the failing assertion with `QEXPECT_FAIL`, and add an inventory row. Do NOT investigate or fix; just file and continue.

- [ ] **Step 3: Update the matrix**

In `docs/test-coverage-matrix.md`, in the row "MetadataCache ↔ CachedMetadataStore", column L2: set the cell to `✓ tst_cross_session::reopenWithStatCleanIsSilent` (or `BUG-20260415-001` if it failed).

- [ ] **Step 4: Commit**

```bash
git add tests/integration/tst_cross_session.cpp docs/test-coverage-matrix.md
git commit -m "$(cat <<'EOF'
test(integration): L2 silence — reopen-with-stat-clean emits no signals

Asserts the audit's "no content change, no event" rule across a real
session boundary. Baseline for L2 lifecycle coverage.
EOF
)"
```

---

### Task 6: Hunt — external mutation while closed is detected on reopen (L3)

**Cell targeted:** `MetadataCache ↔ CachedMetadataStore × L3 (reopen-with-edit)`.

**Files:**
- Modify: `tests/integration/tst_cross_session.cpp`

- [ ] **Step 1: Add the test method**

Add inside the `private Q_SLOTS:` block:

```cpp
    // L3: a file edited *outside* Corbomite between sessions must trigger
    // re-parse on reopen, with cacheChanged carrying the new content's hash.
    void externalEditBetweenSessionsTriggersReparse()
    {
        QTemporaryDir vaultDir;
        QVERIFY(vaultDir.isValid());

        const QString notePath = QStringLiteral("Edited.md");
        const QString fullPath = vaultDir.path() + QLatin1Char('/') + notePath;
        writeFile(fullPath, QByteArrayLiteral("# v1\n\nOriginal body.\n"));

        const QString cacheDb =
            vaultDir.path() + QStringLiteral("/.corbomite/metadata-cache.db");
        QDir().mkpath(QFileInfo(cacheDb).absolutePath());

        QString session1Hash;
        {
            LinkResolver resolver;
            resolver.setVaultPaths({notePath});
            MetadataCache cache(resolver);
            cache.open(cacheDb);

            QSignalSpy doneSpy(&cache, &MetadataCache::indexFinished);
            cache.rebuildVault(vaultDir.path(), {notePath});
            QVERIFY(waitForSpy(doneSpy, 1));
            session1Hash = cache.getFileHash(notePath);
            QVERIFY(!session1Hash.isEmpty());
            cache.close();
        }

        // Edit outside Corbomite — change content + bump mtime.
        // Sleep briefly to ensure mtime granularity advances.
        QTest::qWait(1100);
        writeFile(fullPath, QByteArrayLiteral("# v2\n\nDifferent body now.\n"));

        // Session 2.
        LinkResolver resolver;
        resolver.setVaultPaths({notePath});
        MetadataCache cache(resolver);
        cache.open(cacheDb);

        QSignalSpy changedSpy(&cache, &MetadataCache::cacheChanged);
        QSignalSpy doneSpy(&cache, &MetadataCache::indexFinished);
        cache.rebuildVault(vaultDir.path(), {notePath});
        QVERIFY(waitForSpy(doneSpy, 1));

        QCOMPARE(changedSpy.count(), 1);
        const QString session2Hash = cache.getFileHash(notePath);
        QVERIFY(!session2Hash.isEmpty());
        QVERIFY(session2Hash != session1Hash);

        cache.close();
    }
```

- [ ] **Step 2: Run, observe outcome**

Run:
```bash
cmake --build build --target tst_cross_session 2>&1 | tail -5 && \
  cd build && ctest -R tst_cross_session --output-on-failure
```

Expected: PASS. If FAILS, file as `BUG-20260415-NNN` (next available NNN), wrap assertion with `QEXPECT_FAIL`, add inventory row, do not fix.

- [ ] **Step 3: Update matrix + commit**

Update `docs/test-coverage-matrix.md` row "MetadataCache ↔ CachedMetadataStore" column L3.

```bash
git add tests/integration/tst_cross_session.cpp docs/test-coverage-matrix.md \
        docs/test-coverage-bug-hunt.md  # (only if a bug was filed)
git commit -m "$(cat <<'EOF'
test(integration): L3 — external edit between sessions triggers reparse

Verifies stat-change → hash-recompute → cacheChanged path on real
mtime changes from outside Corbomite.
EOF
)"
```

---

### Task 7: Hunt — file deleted externally between sessions (L3, deletion arm)

**Cell targeted:** `MetadataCache ↔ CachedMetadataStore × L3 (deletion variant)`.

**Files:**
- Modify: `tests/integration/tst_cross_session.cpp`

- [ ] **Step 1: Add the test**

Add inside the `private Q_SLOTS:` block:

```cpp
    // L3 deletion arm: a file removed from disk between sessions. On reopen,
    // rebuildVault should observe its absence and *eventually* the cache
    // should drop the entry (or at least not crash and not re-parse stale
    // content). This test documents the expected behaviour; if Corbomite
    // doesn't currently trigger cacheDeleted on a missing file passed via
    // the persisted file_cache, that's a bug worth filing.
    void externalDeleteBetweenSessionsObservedOnReopen()
    {
        QTemporaryDir vaultDir;
        QVERIFY(vaultDir.isValid());

        const QString notePath = QStringLiteral("Doomed.md");
        const QString fullPath = vaultDir.path() + QLatin1Char('/') + notePath;
        writeFile(fullPath, QByteArrayLiteral("# Doomed\n"));

        const QString cacheDb =
            vaultDir.path() + QStringLiteral("/.corbomite/metadata-cache.db");
        QDir().mkpath(QFileInfo(cacheDb).absolutePath());

        // Session 1: index it.
        {
            LinkResolver resolver;
            resolver.setVaultPaths({notePath});
            MetadataCache cache(resolver);
            cache.open(cacheDb);
            QSignalSpy doneSpy(&cache, &MetadataCache::indexFinished);
            cache.rebuildVault(vaultDir.path(), {notePath});
            QVERIFY(waitForSpy(doneSpy, 1));
            QVERIFY(!cache.getFileHash(notePath).isEmpty());
            cache.close();
        }

        // Delete the file outside Corbomite.
        QVERIFY(QFile::remove(fullPath));

        // Session 2: rebuildVault is told *only* about files that exist;
        // VaultModel::allNotes() in real loadVault would NOT include the
        // deleted path. So reconcile must come from comparing persisted
        // cache state against the path list passed in.
        LinkResolver resolver;
        resolver.setVaultPaths({});  // No notes left in vault.
        MetadataCache cache(resolver);
        cache.open(cacheDb);  // Loads persisted hash for `Doomed.md`.

        QSignalSpy deletedSpy(&cache, &MetadataCache::cacheDeleted);

        // Pass empty list — vault scan found nothing.
        cache.rebuildVault(vaultDir.path(), {});
        QTest::qWait(100);
        QCoreApplication::processEvents();

        // Expectation: persisted entry for the missing file is reaped.
        // If MetadataCache::rebuildVault doesn't currently do reconciliation
        // against the passed-in path list, this fails — file BUG-NNN.
        QCOMPARE(deletedSpy.count(), 1);
        QVERIFY(cache.getFileHash(notePath).isEmpty());

        cache.close();
    }
```

- [ ] **Step 2: Run, observe outcome**

Run:
```bash
cmake --build build --target tst_cross_session 2>&1 | tail -5 && \
  cd build && ctest -R tst_cross_session --output-on-failure
```

This one is **likely to FAIL** — `rebuildVault` documentation says "Missing files and unreadable files are skipped silently" but doesn't say it reconciles against the passed-in path list. If it FAILS, file `BUG-20260415-002` (or next NNN), wrap the asserts with:

```cpp
QEXPECT_FAIL("", "BUG-20260415-002: MetadataCache::rebuildVault doesn't reap entries for files no longer in the path list", Continue);
QCOMPARE(deletedSpy.count(), 1);
QEXPECT_FAIL("", "BUG-20260415-002: same — persisted hash survives implicit deletion", Continue);
QVERIFY(cache.getFileHash(notePath).isEmpty());
```

Add a row to `docs/test-coverage-bug-hunt.md`:

```
| BUG-20260415-002 | MetadataCache::rebuildVault doesn't reap entries for files no longer in the path list (silent stale state across sessions) | 2026-04-15 | 1 | tst_cross_session::externalDeleteBetweenSessionsObservedOnReopen | Medium | Open | Persisted FileCacheEntry survives even though the file is gone. Risk: stale links in SQLiteIndex pointing at deleted source paths until a per-file event fires. |
```

- [ ] **Step 3: Update matrix + commit**

Update `docs/test-coverage-matrix.md` row "MetadataCache ↔ CachedMetadataStore" column L3 — append `+ BUG-20260415-002 deletion-reap`.

```bash
git add tests/integration/tst_cross_session.cpp docs/test-coverage-matrix.md \
        docs/test-coverage-bug-hunt.md
git commit -m "$(cat <<'EOF'
test(integration): L3 deletion — persisted entries for vanished files

Documents the rebuildVault contract: when a previously-indexed file is
absent from the new path list, the persisted entry should be reaped.
File: BUG-20260415-002 if it fails — hunt-only, no fix in this cycle.
EOF
)"
```

---

### Task 8: Hunt — vault switch leaves no SQLiteIndex rows from vault A in vault B's session (L5)

**Cell targeted:** `MainWindow.loadVault × L5 (vault switch)`. Tier B can drive `MetadataCache` + `SQLiteIndex` directly without `MainWindow`; we test the contract those two stores must hold for the switch to be safe.

**Files:**
- Modify: `tests/integration/tst_cross_session.cpp`

- [ ] **Step 1: Add the test**

Add inside the `private Q_SLOTS:` block:

```cpp
    // L5: simulating a vault switch — pointing the same SQLiteIndex +
    // MetadataCache pair at a new vault root. The new vault's queries must
    // not see any row from the old vault. (In real MainWindow.loadVault we
    // delete and recreate both objects, but the contract is worth proving:
    // changing setVaultRoot mid-flight + rewiring should not leak.)
    void vaultSwitchDoesNotLeakLinksFromPreviousVault()
    {
        QTemporaryDir dirA;
        QTemporaryDir dirB;
        QVERIFY(dirA.isValid() && dirB.isValid());

        const QString aNote = QStringLiteral("In A.md");
        const QString aTarget = QStringLiteral("Target A.md");
        writeFile(dirA.path() + QLatin1Char('/') + aNote,
                  QByteArrayLiteral("# A\n\n[[Target A]]\n"));
        writeFile(dirA.path() + QLatin1Char('/') + aTarget,
                  QByteArrayLiteral("# Target A\n"));

        const QString bNote = QStringLiteral("In B.md");
        writeFile(dirB.path() + QLatin1Char('/') + bNote,
                  QByteArrayLiteral("# B\n\nNo links.\n"));

        const QString aCacheDb =
            dirA.path() + QStringLiteral("/.corbomite/metadata-cache.db");
        const QString aIndexDb =
            dirA.path() + QStringLiteral("/.corbomite/index.sqlite");
        const QString bCacheDb =
            dirB.path() + QStringLiteral("/.corbomite/metadata-cache.db");
        const QString bIndexDb =
            dirB.path() + QStringLiteral("/.corbomite/index.sqlite");
        QDir().mkpath(QFileInfo(aCacheDb).absolutePath());
        QDir().mkpath(QFileInfo(bCacheDb).absolutePath());

        // Vault A.
        {
            LinkResolver resolver;
            resolver.setVaultPaths({aNote, aTarget});
            MetadataCache cache(resolver);
            cache.open(aCacheDb);
            SQLiteIndex index;
            QVERIFY(index.open(aIndexDb));
            index.setVaultRoot(dirA.path());
            index.setMetadataCache(&cache);
            QSignalSpy doneSpy(&cache, &MetadataCache::indexFinished);
            cache.rebuildVault(dirA.path(), {aNote, aTarget});
            QVERIFY(waitForSpy(doneSpy, 1));
            QCOMPARE(index.outlinksFor(aNote).size(), 1);
            cache.close();
            index.close();
        }

        // Vault B — fresh objects per real loadVault behaviour.
        LinkResolver resolverB;
        resolverB.setVaultPaths({bNote});
        MetadataCache cacheB(resolverB);
        cacheB.open(bCacheDb);
        SQLiteIndex indexB;
        QVERIFY(indexB.open(bIndexDb));
        indexB.setVaultRoot(dirB.path());
        indexB.setMetadataCache(&cacheB);
        QSignalSpy doneSpyB(&cacheB, &MetadataCache::indexFinished);
        cacheB.rebuildVault(dirB.path(), {bNote});
        QVERIFY(waitForSpy(doneSpyB, 1));

        // Vault B should know about its own note and nothing else.
        QVERIFY(indexB.outlinksFor(aNote).isEmpty());        // A's note absent.
        QVERIFY(indexB.backlinksFor(aTarget).isEmpty());     // A's target absent.
        QCOMPARE(indexB.outlinksFor(bNote).size(), 0);       // B has no links.

        cacheB.close();
        indexB.close();
    }
```

- [ ] **Step 2: Run, observe**

Run:
```bash
cmake --build build --target tst_cross_session 2>&1 | tail -5 && \
  cd build && ctest -R tst_cross_session --output-on-failure
```

Expected: PASS (separate DB files per vault → no leak path). If FAILS, file `BUG-20260415-NNN` and `QEXPECT_FAIL` per pattern.

- [ ] **Step 3: Update matrix + commit**

Update `docs/test-coverage-matrix.md` row "MainWindow.loadVault" column L5: change `~ tst_vault_switch` to `✓ tst_cross_session::vaultSwitchDoesNotLeakLinksFromPreviousVault + ~ tst_vault_switch (full UI)`.

```bash
git add tests/integration/tst_cross_session.cpp docs/test-coverage-matrix.md
git commit -m "$(cat <<'EOF'
test(integration): L5 — vault switch isolation at the storage layer

Proves SQLiteIndex + MetadataCache pair instantiated against vault B
sees zero rows from vault A. Complements the UI-level tst_vault_switch.
EOF
)"
```

---

### Task 9: Hunt — orphan-link reporting after target file is deleted (L6 mid-session)

**Cell targeted:** `MetadataCache ↔ SQLiteIndex × L6 (mid-session mutation)`. Specifically: the orphan-link query path that PropertiesPanel/OutlinksPanel rely on.

**Files:**
- Modify: `tests/integration/tst_cross_session.cpp`

- [ ] **Step 1: Add the test**

Add inside the `private Q_SLOTS:` block:

```cpp
    // L6: link to a file that gets deleted mid-session. SQLiteIndex's
    // orphanLinks() should report the dangling target after the deletion
    // event propagates. Critical for OutlinksPanel "(create)" markers.
    void orphanLinkAppearsAfterTargetDeleted()
    {
        QTemporaryDir vaultDir;
        QVERIFY(vaultDir.isValid());

        const QString src = QStringLiteral("Source.md");
        const QString tgt = QStringLiteral("Target.md");
        writeFile(vaultDir.path() + QLatin1Char('/') + src,
                  QByteArrayLiteral("# Source\n\nLink to [[Target]].\n"));
        writeFile(vaultDir.path() + QLatin1Char('/') + tgt,
                  QByteArrayLiteral("# Target\n"));

        const QString cacheDb =
            vaultDir.path() + QStringLiteral("/.corbomite/metadata-cache.db");
        const QString indexDb =
            vaultDir.path() + QStringLiteral("/.corbomite/index.sqlite");
        QDir().mkpath(QFileInfo(cacheDb).absolutePath());

        LinkResolver resolver;
        resolver.setVaultPaths({src, tgt});
        MetadataCache cache(resolver);
        cache.open(cacheDb);
        SQLiteIndex index;
        QVERIFY(index.open(indexDb));
        index.setVaultRoot(vaultDir.path());
        index.setMetadataCache(&cache);

        QSignalSpy doneSpy(&cache, &MetadataCache::indexFinished);
        cache.rebuildVault(vaultDir.path(), {src, tgt});
        QVERIFY(waitForSpy(doneSpy, 1));

        // Sanity: no orphans yet.
        QCOMPARE(index.orphanLinks().size(), 0);

        // Mid-session delete of the target.
        cache.onFileDeleted(tgt);
        QTest::qWait(50);
        QCoreApplication::processEvents();

        const auto orphans = index.orphanLinks();
        QCOMPARE(orphans.size(), 1);
        QCOMPARE(orphans.first(), tgt);

        cache.close();
        index.close();
    }
```

- [ ] **Step 2: Run, observe, file-or-pass**

Run:
```bash
cmake --build build --target tst_cross_session 2>&1 | tail -5 && \
  cd build && ctest -R tst_cross_session --output-on-failure
```

Expected outcome: unknown — this is genuine bug-hunting. If PASS, update matrix `✓`. If FAIL, file `BUG-20260415-NNN`, `QEXPECT_FAIL`, inventory.

- [ ] **Step 3: Commit**

```bash
git add tests/integration/tst_cross_session.cpp docs/test-coverage-matrix.md \
        docs/test-coverage-bug-hunt.md  # (only if a bug was filed)
git commit -m "$(cat <<'EOF'
test(integration): L6 — orphan-link reporting on mid-session delete
EOF
)"
```

---

### Task 10: Tier A — UI smoke test for Backlinks panel populated on vault open

This is the first Tier A test. Following the existing convention in `tests/e2e/`, it uses a real display (do NOT set `QT_QPA_PLATFORM=offscreen`). Marked as developer-machine-only; skipped in CI.

**Cell targeted:** `Backlinks/Outgoing Links panel ↔ SQLiteIndex × L2 (reopen with populated cache)`. This is the exact UI symptom the user originally reported.

**Files:**
- Create: `tests/e2e/tst_panels_populated.cpp`
- Modify: `tests/e2e/CMakeLists.txt`

- [ ] **Step 1: Write the UI test**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tier A — UI smoke: launch MainWindow on a real display, point it at a
// vault with known link structure, assert the right-sidebar panels show
// non-zero counts. Catches the user-visible class of bug where storage
// is silently empty (e.g. BUG-20260415-000).
//
// Requirements: a running display server (Wayland/X11). DO NOT set
// QT_QPA_PLATFORM=offscreen.

#include <QTest>
#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include <KAboutData>
#include <KLocalizedString>

#include "app/MainWindow.h"
#include "app/VaultService.h"
#include "editor/EditorViewManager.h"
#include "editor/NoteEditorWidget.h"
#include "sidebar/BacklinksPanel.h"
#include "sidebar/OutlinksPanel.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/NoteService.h"
#include "corbomite/core/NoteDocument.h"

using namespace Corbomite;

class TestPanelsPopulated : public QObject {
    Q_OBJECT

private:
    VaultService *m_vaultService = nullptr;
    MainWindow *m_mainWindow = nullptr;
    QTemporaryDir m_vaultDir;

    static void writeFile(const QString &path, const QByteArray &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    }

    void settle(int ms = 400) { QTest::qWait(ms); }

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("panels populated"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);

        QVERIFY(m_vaultDir.isValid());
        writeFile(m_vaultDir.path() + "/Hub.md",
                  QByteArrayLiteral("# Hub\n\nRefers to [[Spoke]].\n"));
        writeFile(m_vaultDir.path() + "/Spoke.md",
                  QByteArrayLiteral("# Spoke\n\nNo outgoing links.\n"));

        m_vaultService = new VaultService(this);
        m_mainWindow = new MainWindow(m_vaultService);
        m_mainWindow->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow));
        settle(300);
    }

    void cleanupTestCase()
    {
        delete m_mainWindow;
        m_mainWindow = nullptr;
        delete m_vaultService;
        m_vaultService = nullptr;
    }

    // Open vault, open Hub.md, assert OutlinksPanel header reads "Outgoing Links (1)".
    void hubNoteShowsOneOutgoingLink()
    {
        QVERIFY(m_vaultService->openVault(m_vaultDir.path()));
        settle(800);  // Allow rebuildVault + cache reconcile + panel refresh.

        // Open Hub.md.
        auto *vault = m_vaultService->vault();
        QVERIFY(vault);
        auto *hubDoc = vault->cachedDocument(QStringLiteral("Hub.md"));
        if (!hubDoc) {
            // Fall back to creating-via-noteService if vault doesn't auto-cache.
            hubDoc = vault->cachedDocument(QStringLiteral("Hub.md"));
        }
        QVERIFY(hubDoc);

        // Walk MainWindow's children to find the OutlinksPanel header label.
        auto *outlinks = m_mainWindow->findChild<OutlinksPanel *>();
        QVERIFY(outlinks);
        outlinks->setCurrentNote(hubDoc);
        settle(200);

        auto *header = outlinks->findChild<QLabel *>();
        QVERIFY(header);
        // The header text contains the count in parentheses.
        QVERIFY2(header->text().contains(QStringLiteral("(1)")),
                 qPrintable(QStringLiteral("Header was: ") + header->text()));
    }

    void spokeNoteShowsOneBacklink()
    {
        // Vault already open from previous test (test order matters here;
        // QTest runs methods in declaration order).
        auto *vault = m_vaultService->vault();
        QVERIFY(vault);
        auto *spokeDoc = vault->cachedDocument(QStringLiteral("Spoke.md"));
        QVERIFY(spokeDoc);

        auto *backlinks = m_mainWindow->findChild<BacklinksPanel *>();
        QVERIFY(backlinks);
        backlinks->setCurrentNote(spokeDoc);
        settle(200);

        auto *header = backlinks->findChild<QLabel *>();
        QVERIFY(header);
        QVERIFY2(header->text().contains(QStringLiteral("(1)")),
                 qPrintable(QStringLiteral("Header was: ") + header->text()));
    }
};

QTEST_MAIN(TestPanelsPopulated)
#include "tst_panels_populated.moc"
```

- [ ] **Step 2: Wire into CMake**

In `tests/e2e/CMakeLists.txt`, after the `tst_completion_popup` block (after line 36), append:

```cmake

add_executable(tst_panels_populated tst_panels_populated.cpp)
add_test(NAME tst_panels_populated COMMAND tst_panels_populated)
target_link_libraries(tst_panels_populated PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
)
```

- [ ] **Step 3: Build + run on dev display**

Run:
```bash
cmake --build build --target tst_panels_populated 2>&1 | tail -5 && \
  cd build && ctest -R tst_panels_populated --output-on-failure
```

Expected: PASS (we fixed BUG-20260415-000 already, so the panel headers should read "(1)"). If FAIL on a fresh vault, that's a *new* bug — file it.

- [ ] **Step 4: Update matrix + commit**

Update `docs/test-coverage-matrix.md` row "Backlinks/OutgoingLinks panel ↔ SQLiteIndex" — replace `(none — Tier A scope)` with `✓ tst_panels_populated::hubNoteShowsOneOutgoingLink, ::spokeNoteShowsOneBacklink (Tier A — display required)`.

```bash
git add tests/e2e/tst_panels_populated.cpp tests/e2e/CMakeLists.txt \
        docs/test-coverage-matrix.md
git commit -m "$(cat <<'EOF'
test(e2e): Tier A smoke — Backlinks/Outlinks panels show non-zero counts

Catches the UI-observable class of bug where storage is silently empty
(BUG-20260415-000 was invisible to per-class unit tests). Display-required;
not run in CI per existing tests/e2e/ convention.
EOF
)"
```

---

### Task 11: Cycle 1 close-out

**Files:**
- Modify: `docs/test-coverage-bug-hunt.md` (cycle-log section)
- Modify: `docs/PROJECT-STATE.md` (Recent decisions)

- [ ] **Step 1: Update the Cycle 1 log entry**

In `docs/test-coverage-bug-hunt.md`, replace the placeholder Cycle 1 log line with a concrete summary:

```markdown
- **Cycle 1 (2026-04-15):** First execution. Targeted 6 cells across `MetadataCache ↔ CachedMetadataStore`, `MetadataCache ↔ SQLiteIndex`, `MainWindow.loadVault`, and `Backlinks/OutgoingLinks panel ↔ SQLiteIndex`. Lifecycles covered: L2, L3 (edit + delete arms), L4, L5, L6. Tests landed: `tst_cross_session::{linksRepopulateAfterSchemaBumpOnStatCleanReopen, reopenWithStatCleanIsSilent, externalEditBetweenSessionsTriggersReparse, externalDeleteBetweenSessionsObservedOnReopen, vaultSwitchDoesNotLeakLinksFromPreviousVault, orphanLinkAppearsAfterTargetDeleted}` + `tst_panels_populated::{hubNoteShowsOneOutgoingLink, spokeNoteShowsOneBacklink}`. Bugs filed: <list BUG-IDs from this cycle>.
```

(Substitute the actual BUG-IDs that were filed during Tasks 5–10.)

- [ ] **Step 2: Add a PROJECT-STATE entry**

In `docs/PROJECT-STATE.md`, find the "Recent decisions" section. Add at the top:

```markdown
- **2026-04-15 — Test enrichment Cycle 1 ran.** First execution of Ritual 4. 8 new tests landed (1 regression, 5 cross-session scenarios, 2 UI smoke); N bugs filed in `docs/test-coverage-bug-hunt.md`. Reason: SQLiteIndex link-table bug (BUG-20260415-000) escaped per-class unit tests; need cross-session and UI-observable coverage going forward. See `docs/superpowers/plans/2026-04-15-test-enrichment-cycle.md`.
```

(Substitute `N` with the actual bug count.)

- [ ] **Step 3: Final test run**

Confirm everything still builds and runs:

```bash
cmake --build build 2>&1 | tail -5 && \
  cd build && ctest -R "tst_cross_session|tst_panels_populated|tst_sqliteindex|tst_metadatacache" --output-on-failure
```

Expected: all tests in scope either PASS or are `QEXPECT_FAIL`-marked with bug references — no unexpected failures.

- [ ] **Step 4: Commit cycle close-out**

```bash
git add docs/test-coverage-bug-hunt.md docs/PROJECT-STATE.md
git commit -m "$(cat <<'EOF'
docs(test-enrichment): close out Cycle 1

8 new tests landed (1 regression, 5 cross-session, 2 UI smoke).
Bugs filed in docs/test-coverage-bug-hunt.md. Next cycle picks up
remaining matrix gaps after the next cluster lands.
EOF
)"
```

---

## Definition of done — Cycle 1

- [x] `docs/test-coverage-bug-hunt.md` exists, has BUG-20260415-000 row.
- [x] `docs/test-coverage-matrix.md` exists with seams + lifecycle dimensions.
- [x] `docs/CONTRIBUTING-OPS.md` has Ritual 4.
- [x] `tests/integration/tst_cross_session.cpp` exists with ≥6 test methods, all built and registered with ctest.
- [x] `tests/e2e/tst_panels_populated.cpp` exists with ≥2 test methods.
- [x] All new tests run; failures are wrapped with `QEXPECT_FAIL` + a `BUG-` reference; `ctest` exits 0 for the relevant targets.
- [x] PROJECT-STATE has a "Recent decisions" entry for Cycle 1.

## Blocks / enables

- **Blocks nothing** — this is a maintenance project, not a feature.
- **Enables:** future cycles execute this same plan as a template, picking the next-highest-risk cells from `docs/test-coverage-matrix.md`. Bug fixes for filed BUG-IDs are scheduled separately by the human (as normal-priority tasks, not as part of this plan).

## Preserved-compat quirks

None — this plan adds tests; it does not change runtime behaviour.

## Cycle template (for re-runs)

A future Cycle M re-executes this plan with these substitutions:
- Skip Tasks 1–3 (bootstrap is one-time).
- Refresh the matrix (Ritual 4 step 2).
- Replace Tasks 4–10 with N tasks targeting the next batch of highest-risk cells; same shape (one test method per task, file-or-pass-and-update-matrix).
- Replace Task 11 substitutions with Cycle M's actuals.

Each cycle's plan file lives at `docs/superpowers/plans/YYYY-MM-DD-test-enrichment-cycle-M.md` (this file is Cycle 1; the filename itself documents the cycle number implicitly via date).
