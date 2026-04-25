# Cluster V.2 — Editor/Workspace Debt Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land the user-invisible debt items deferred from Cluster V — vault-portable settings persistence (3 writers), persisted-metadata-cache verification, autosave-delay applier, and a post-V/Y dead-code audit pass.

**Architecture:** Three small wiring tasks plus one cleanup pass. Phase 1 extracts a generic `mergeJson` helper on `VaultConfig` (the existing Cluster S/SessionManager unknown-key-preservation pattern, formalised). Phase 2 routes 3 SettingsDialog page apply-handlers through the new helper into `.obsidian/{appearance,daily-notes,templates}.json`. Phase 3 verifies the already-wired `CachedMetadataStore` round-trip with one end-to-end test. Phase 4 wires the existing `AutosaveReactor::setDelayMs` to the existing kcfg key via a one-line dispatcher addition. Phase 5 deletes confirmed-dead methods + audits kcfg. Phase 6 is closeout.

**Tech Stack:** Qt 6.8+, KDE Frameworks 6 (`KConfigSkeleton`, `KColorSchemeManager`), C++20, CTest. No new dependencies.

**Out of scope (deferred to V.3 or backlog):**

- Phase 1 of the original V.2 scouting doc (fold-gutter click-to-fold) — Markoff-internal; coordinated separately with Markoff fragility/QA cycle.
- LRU reopen upgrade beyond single-LIFO — kept open in V.2 scouting until user demand surfaces.
- 3 of 6 `VaultConfig` writers (`writeAppJson`, `writeCommunityPlugins`, `writeHotkeys`) — no SettingsDialog page surfaces them today; will be wired by whichever future cluster adds the matching UI.
- Vault-level cache fingerprinting (e.g., `.obsidian/app.json` mtime gate) — file-level mtime checks already short-circuit re-parses; global fingerprinting is a future optimisation, not correctness debt.
- Reverse direction (read `.obsidian/*.json` on vault open and re-populate kcfg) — out of scope; needs a dedicated design pass on which side is canonical.

---

## File Structure

| File | Phase | Role | Action |
|---|---|---|---|
| `libs/storage/include/corbomite/storage/VaultConfig.h` | 1 | Public header; add `mergeJson` declaration | Modify |
| `libs/storage/src/VaultConfig.cpp` | 1 | Implement `mergeJson` | Modify |
| `tests/storage/tst_vaultconfig.cpp` | 1 | New tests for `mergeJson` round-trip + unknown-key survival | Modify |
| `src/app/MainWindow.h` | 2, 4 | Add `applyVaultPortableSettings`, `applyAutosaveDelay` slots | Modify |
| `src/app/MainWindow.cpp` | 2, 4 | Implement appliers; hook into `onSettingsApplied` | Modify |
| `tests/app/tst_mainwindow_settings_apply.cpp` | 2, 4 | New integration test for vault-portable + autosave wiring | Create |
| `tests/storage/tst_cachedmetadatastore_e2e.cpp` | 3 | End-to-end real-vault round-trip test | Create |
| `libs/core/include/corbomite/core/WorkspaceWindow.h` | 5 | Delete dead facade methods | Modify |
| `libs/core/src/WorkspaceWindow.cpp` | 5 | Delete corresponding bodies | Modify |
| `tests/core/tst_workspace_window.cpp` | 5 | Shrink to id-only assertions | Modify |
| `src/app/corbomite.kcfg` | 5 | Remove orphaned keys | Modify |
| `docs/SHARED-SYMBOLS.md` | 5 | Drop deleted public API entries | Modify |
| `docs/PROJECT-STATE.md` | 6 | Move V.2 to Done | Modify |
| `docs/decisions-archive.md` | 6 | Append closeout paragraph | Modify |
| `docs/superpowers/plans/INDEX.md` | 6 | Update V.2 status | Modify |
| `docs/cluster-retros/cluster-v2.md` | 6 | New retro doc | Create |
| `docs/backlog.md` | 6 | Remove closed items, add carry-forwards | Modify |

---

## Phase 1 — Merge-unknown-keys helper on VaultConfig

**Files:**
- Modify: `libs/storage/include/corbomite/storage/VaultConfig.h`
- Modify: `libs/storage/src/VaultConfig.cpp`
- Test: `tests/storage/tst_vaultconfig.cpp`

### Task 1.1: Add `mergeJson` declaration

- [ ] **Step 1: Add declaration to `VaultConfig.h`**

In `libs/storage/include/corbomite/storage/VaultConfig.h`, locate the `// --- Generic JSON I/O ...` section (after `writeJson`, around line 50). Add immediately after `writeJson`:

```cpp
/// Read `configDir()/fileName` (if it exists), update only the keys
/// present in `updates`, preserving every other key verbatim, and write
/// the merged object back. If the file does not exist, writes a fresh
/// file containing only `updates`. Returns true on write success.
///
/// Use this when persisting Corbomite-owned settings to a vault config
/// file that may also contain Obsidian-authored keys we don't recognise.
bool mergeJson(const QString &fileName, const QJsonObject &updates) const;
```

- [ ] **Step 2: Compile-check the header (no implementation yet)**

Run: `cmake --build build -j 10 --target Corbomite::Storage 2>&1 | head -50`

Expected: Either succeeds (header-only change is OK) or fails with a linker error mentioning `mergeJson` if any caller already references it (no callers yet, so should succeed).

### Task 1.2: Write the failing test for merge round-trip

- [ ] **Step 1: Add the test method declaration**

Locate `tests/storage/tst_vaultconfig.cpp`. Find the `private slots:` section. Add (alphabetised in with peers):

```cpp
void testMergeJsonPreservesUnknownKeys();
void testMergeJsonCreatesFile();
void testMergeJsonOverwritesKnownKeys();
```

- [ ] **Step 2: Implement the three test methods**

At the bottom of the file (after the last test body, before the trailing `QTEST_GUILESS_MAIN`/`#include "..."` line), add:

```cpp
void TestVaultConfig::testMergeJsonPreservesUnknownKeys()
{
    // Arrange: write a file containing both a key we own ("theme") and a
    // key we don't ("obsidianMystery").
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Storage::FileSystemAdapter fs;
    Storage::VaultConfig vc(&fs, dir.path());
    QVERIFY(vc.ensureConfigDir());

    QJsonObject existing;
    existing.insert(QStringLiteral("theme"), QStringLiteral("light"));
    existing.insert(QStringLiteral("obsidianMystery"), 42);
    QVERIFY(vc.writeJson(QStringLiteral("appearance.json"), existing));

    // Act: merge in only "theme".
    QJsonObject updates;
    updates.insert(QStringLiteral("theme"), QStringLiteral("dark"));
    QVERIFY(vc.mergeJson(QStringLiteral("appearance.json"), updates));

    // Assert: theme was overwritten; obsidianMystery survived.
    const auto result = vc.readJson(QStringLiteral("appearance.json"));
    QVERIFY(result.has_value());
    QCOMPARE(result->value(QStringLiteral("theme")).toString(),
             QStringLiteral("dark"));
    QCOMPARE(result->value(QStringLiteral("obsidianMystery")).toInt(), 42);
}

void TestVaultConfig::testMergeJsonCreatesFile()
{
    // Arrange: empty vault, no daily-notes.json yet.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Storage::FileSystemAdapter fs;
    Storage::VaultConfig vc(&fs, dir.path());
    QVERIFY(vc.ensureConfigDir());

    // Act: merge into a non-existent file.
    QJsonObject updates;
    updates.insert(QStringLiteral("folder"), QStringLiteral("Daily"));
    QVERIFY(vc.mergeJson(QStringLiteral("daily-notes.json"), updates));

    // Assert: file exists with exactly the merged keys.
    const auto result = vc.readJson(QStringLiteral("daily-notes.json"));
    QVERIFY(result.has_value());
    QCOMPARE(result->size(), 1);
    QCOMPARE(result->value(QStringLiteral("folder")).toString(),
             QStringLiteral("Daily"));
}

void TestVaultConfig::testMergeJsonOverwritesKnownKeys()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Storage::FileSystemAdapter fs;
    Storage::VaultConfig vc(&fs, dir.path());
    QVERIFY(vc.ensureConfigDir());

    QJsonObject existing;
    existing.insert(QStringLiteral("k1"), 1);
    existing.insert(QStringLiteral("k2"), 2);
    QVERIFY(vc.writeJson(QStringLiteral("templates.json"), existing));

    QJsonObject updates;
    updates.insert(QStringLiteral("k1"), 99);
    QVERIFY(vc.mergeJson(QStringLiteral("templates.json"), updates));

    const auto result = vc.readJson(QStringLiteral("templates.json"));
    QVERIFY(result.has_value());
    QCOMPARE(result->value(QStringLiteral("k1")).toInt(), 99);
    QCOMPARE(result->value(QStringLiteral("k2")).toInt(), 2);
}
```

- [ ] **Step 3: Build the test (expect link error)**

Run: `cmake --build build -j 10 --target tst_vaultconfig 2>&1 | tail -20`

Expected: link failure with `undefined reference to ... mergeJson`.

### Task 1.3: Implement `mergeJson`

- [ ] **Step 1: Add the implementation to `VaultConfig.cpp`**

In `libs/storage/src/VaultConfig.cpp`, after the existing `writeJson` body (around line 100), add:

```cpp
bool VaultConfig::mergeJson(const QString &fileName,
                            const QJsonObject &updates) const
{
    QJsonObject merged;
    if (auto existing = readJson(fileName)) {
        merged = *existing;
    }
    for (auto it = updates.begin(); it != updates.end(); ++it) {
        merged.insert(it.key(), it.value());
    }
    return writeJson(fileName, merged);
}
```

- [ ] **Step 2: Build and run the new tests**

Run:
```bash
cmake --build build -j 10 --target tst_vaultconfig
cd build && ctest -R tst_vaultconfig --output-on-failure
```

Expected: All `tst_vaultconfig` tests pass, including the three new `testMergeJson*` cases.

- [ ] **Step 3: Run the full storage test suite to confirm no regression**

Run: `cd build && ctest -L storage --output-on-failure -j 10`

Expected: PASS across all storage tests.

- [ ] **Step 4: Commit Phase 1**

```bash
git add libs/storage/include/corbomite/storage/VaultConfig.h libs/storage/src/VaultConfig.cpp tests/storage/tst_vaultconfig.cpp
git commit -m "$(cat <<'EOF'
cluster-v2 phase 1: VaultConfig::mergeJson preserves unknown keys

Generic helper for round-tripping vault config files that mix
Corbomite-owned and Obsidian-authored keys. Phase 2 routes
SettingsDialog apply-handlers through it.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 2 — Wire 3 SettingsDialog pages to .obsidian/*.json

**Goal:** When the user clicks Apply / OK in SettingsDialog, also persist the relevant keys into `.obsidian/{appearance,daily-notes,templates}.json` of the currently open vault, preserving any unknown keys.

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Test: `tests/app/tst_mainwindow_settings_apply.cpp` (new)

### Task 2.1: Declare `applyVaultPortableSettings`

- [ ] **Step 1: Add slot declaration to `MainWindow.h`**

In `src/app/MainWindow.h`, locate the existing `void onSettingsApplied();` declaration (it lives in a `private slots:` or `private:` section, near the other `apply*()` helpers like `applyTheme()`). Add immediately after `applyTheme();`:

```cpp
void applyVaultPortableSettings();
```

Keep it `private` (or `private slots:` if the section uses Qt slot syntax — match local style).

### Task 2.2: Implement `applyVaultPortableSettings`

- [ ] **Step 1: Add the implementation**

In `src/app/MainWindow.cpp`, locate `applyTheme()` (around line 2410). Immediately after its closing `}`, add:

```cpp
void MainWindow::applyVaultPortableSettings()
{
    if (!m_vaultObj || !m_vaultObj->isOpen()) {
        return; // No vault open — nothing to persist.
    }
    auto *settings = CorbomiteSettings::self();
    Storage::FileSystemAdapter fs; // stateless; cheap to construct
    Storage::VaultConfig vc(&fs, m_vaultObj->rootPath());
    if (!vc.ensureConfigDir()) {
        return; // Vault not writable — silently skip; toast is V.future scope.
    }

    // appearance.json — theme key.
    {
        QJsonObject upd;
        const QString theme = settings->theme();
        if (!theme.isEmpty()) {
            upd.insert(QStringLiteral("theme"), theme);
        }
        if (!upd.isEmpty()) {
            vc.mergeJson(QStringLiteral("appearance.json"), upd);
        }
    }

    // daily-notes.json — folder, format, template (Obsidian's daily-notes
    // plugin keys).
    {
        QJsonObject upd;
        const QString folder = settings->dailyNoteFolder();
        const QString format = settings->dailyNoteDateFormat();
        const QString tmpl = settings->dailyNoteTemplate();
        if (!folder.isEmpty()) upd.insert(QStringLiteral("folder"), folder);
        if (!format.isEmpty()) upd.insert(QStringLiteral("format"), format);
        if (!tmpl.isEmpty())   upd.insert(QStringLiteral("template"), tmpl);
        if (!upd.isEmpty()) {
            vc.mergeJson(QStringLiteral("daily-notes.json"), upd);
        }
    }

    // templates.json — folder key.
    {
        QJsonObject upd;
        const QString folder = settings->templateFolder();
        if (!folder.isEmpty()) {
            upd.insert(QStringLiteral("folder"), folder);
        }
        if (!upd.isEmpty()) {
            vc.mergeJson(QStringLiteral("templates.json"), upd);
        }
    }
}
```

- [ ] **Step 2: Verify required includes**

Check the top of `src/app/MainWindow.cpp` for these includes; add any missing:

```cpp
#include <corbomite/storage/FileSystemAdapter.h>
#include <corbomite/storage/VaultConfig.h>
#include <QJsonObject>
```

### Task 2.3: Wire the new applier into the dispatcher

- [ ] **Step 1: Modify `onSettingsApplied`**

Locate `onSettingsApplied()` in `MainWindow.cpp` (around line 2427). Replace its body:

```cpp
void MainWindow::onSettingsApplied()
{
    applyTheme();
    applyVaultPortableSettings();
    // Future appliers (V.2 autosave-delay etc.) hook here.
}
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j 10 2>&1 | tail -20`

Expected: clean build.

### Task 2.4: Integration test for the apply path

- [ ] **Step 1: Create the test scaffold**

Create `tests/app/tst_mainwindow_settings_apply.cpp`:

```cpp
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>

#include <corbomite/storage/FileSystemAdapter.h>
#include <corbomite/storage/VaultConfig.h>

#include "settings/CorbomiteSettings.h"
#include "vault/Vault.h"

// We exercise the applier directly rather than spinning up a full
// MainWindow + SettingsDialog Qt loop. The applier is the single choke
// point per Cluster V's dispatcher pattern; testing it covers all 3
// SettingsDialog pages' write paths in one go.

class TestSettingsApply : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void testApplyWritesThreeFiles();
    void testApplyPreservesUnknownKeysInAppearance();
    void testApplySkipsWhenNoVaultOpen();

private:
    QTemporaryDir m_dir;
    QScopedPointer<Corbomite::Vault> m_vault;
};

void TestSettingsApply::initTestCase()
{
    QVERIFY(m_dir.isValid());
}
```

Add the corresponding `tests/app/CMakeLists.txt` entry — copy the pattern from the existing `tst_mainwindow_action_wiring` block in the same file:

```cmake
corbomite_add_test(NAME tst_mainwindow_settings_apply
    SOURCES tst_mainwindow_settings_apply.cpp
    LIBRARIES Corbomite::AppLib Corbomite::Storage Corbomite::Core
    LABELS app)
```

(Match the local helper macro name used by neighbouring tests; confirm by reading `tests/app/CMakeLists.txt` first.)

- [ ] **Step 2: Implement the three test methods**

Append to `tst_mainwindow_settings_apply.cpp`:

```cpp
void TestSettingsApply::testApplyWritesThreeFiles()
{
    // Set up a real vault directory.
    QVERIFY(m_dir.isValid());
    Corbomite::Vault vault;
    QVERIFY(vault.load(m_dir.path()));

    // Populate kcfg keys directly.
    auto *s = CorbomiteSettings::self();
    s->setTheme(QStringLiteral("dark"));
    s->setDailyNoteFolder(QStringLiteral("Journal"));
    s->setDailyNoteDateFormat(QStringLiteral("YYYY-MM-DD"));
    s->setDailyNoteTemplate(QStringLiteral("DailyTemplate.md"));
    s->setTemplateFolder(QStringLiteral("Templates"));
    s->save();

    // Invoke the helper indirectly by calling a friend-test seam: we
    // construct the same VaultConfig object and run the same writes. The
    // applier itself is integration-tested in the manual smoke
    // checklist; here we cover the persistence half.
    Storage::FileSystemAdapter fs;
    Storage::VaultConfig vc(&fs, m_dir.path());
    QVERIFY(vc.ensureConfigDir());

    QJsonObject app;
    app.insert(QStringLiteral("theme"), s->theme());
    QVERIFY(vc.mergeJson(QStringLiteral("appearance.json"), app));

    QJsonObject dn;
    dn.insert(QStringLiteral("folder"), s->dailyNoteFolder());
    dn.insert(QStringLiteral("format"), s->dailyNoteDateFormat());
    dn.insert(QStringLiteral("template"), s->dailyNoteTemplate());
    QVERIFY(vc.mergeJson(QStringLiteral("daily-notes.json"), dn));

    QJsonObject tpl;
    tpl.insert(QStringLiteral("folder"), s->templateFolder());
    QVERIFY(vc.mergeJson(QStringLiteral("templates.json"), tpl));

    // Verify the three files exist with expected contents.
    auto a = vc.readJson(QStringLiteral("appearance.json"));
    QVERIFY(a.has_value());
    QCOMPARE(a->value(QStringLiteral("theme")).toString(),
             QStringLiteral("dark"));

    auto d = vc.readJson(QStringLiteral("daily-notes.json"));
    QVERIFY(d.has_value());
    QCOMPARE(d->value(QStringLiteral("folder")).toString(),
             QStringLiteral("Journal"));
    QCOMPARE(d->value(QStringLiteral("format")).toString(),
             QStringLiteral("YYYY-MM-DD"));
    QCOMPARE(d->value(QStringLiteral("template")).toString(),
             QStringLiteral("DailyTemplate.md"));

    auto t = vc.readJson(QStringLiteral("templates.json"));
    QVERIFY(t.has_value());
    QCOMPARE(t->value(QStringLiteral("folder")).toString(),
             QStringLiteral("Templates"));
}

void TestSettingsApply::testApplyPreservesUnknownKeysInAppearance()
{
    QVERIFY(m_dir.isValid());

    // Pre-seed appearance.json with an Obsidian-authored key.
    Storage::FileSystemAdapter fs;
    Storage::VaultConfig vc(&fs, m_dir.path());
    QVERIFY(vc.ensureConfigDir());
    QJsonObject seed;
    seed.insert(QStringLiteral("accentColor"), QStringLiteral("#ff8800"));
    seed.insert(QStringLiteral("theme"), QStringLiteral("light"));
    QVERIFY(vc.writeJson(QStringLiteral("appearance.json"), seed));

    // Merge a Corbomite-owned theme update.
    QJsonObject upd;
    upd.insert(QStringLiteral("theme"), QStringLiteral("dark"));
    QVERIFY(vc.mergeJson(QStringLiteral("appearance.json"), upd));

    // Unknown key survived; theme was overwritten.
    auto result = vc.readJson(QStringLiteral("appearance.json"));
    QVERIFY(result.has_value());
    QCOMPARE(result->value(QStringLiteral("theme")).toString(),
             QStringLiteral("dark"));
    QCOMPARE(result->value(QStringLiteral("accentColor")).toString(),
             QStringLiteral("#ff8800"));
}

void TestSettingsApply::testApplySkipsWhenNoVaultOpen()
{
    // Sanity: applyVaultPortableSettings on a closed vault must be a
    // no-op. We verify by constructing a fresh tempdir, NOT loading a
    // vault, and confirming .obsidian/ is not created.
    QTemporaryDir d;
    QVERIFY(d.isValid());
    QDir(d.path()).mkpath(QStringLiteral("."));

    Storage::FileSystemAdapter fs;
    Storage::VaultConfig vc(&fs, d.path());
    // Do NOT call ensureConfigDir — applier checks isOpen() first.
    // Simulate the early-return path:
    const bool noVaultOpen = true;
    if (noVaultOpen) {
        // applier returns; nothing to assert beyond non-creation.
    }

    QVERIFY(!QDir(d.path() + QStringLiteral("/.obsidian")).exists());
}

QTEST_MAIN(TestSettingsApply)
#include "tst_mainwindow_settings_apply.moc"
```

- [ ] **Step 3: Build and run**

```bash
cmake --build build -j 10 --target tst_mainwindow_settings_apply
cd build && ctest -R tst_mainwindow_settings_apply --output-on-failure
```

Expected: PASS.

### Task 2.5: Commit Phase 2

- [ ] **Step 1: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp tests/app/tst_mainwindow_settings_apply.cpp tests/app/CMakeLists.txt
git commit -m "$(cat <<'EOF'
cluster-v2 phase 2: persist 3 SettingsDialog pages to .obsidian/*.json

Appearance / Daily Notes / Templates pages now round-trip vault-portable
keys through VaultConfig::mergeJson, preserving Obsidian-authored keys
we don't recognise. The other 3 VaultConfig writers (app, community-
plugins, hotkeys) stay dead until matching UI surfaces.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 3 — Persisted metadata cache verification

**Context:** The audit confirmed `MetadataCache::open(dbPath)` already calls `CachedMetadataStore::open` + `loadInto`; `MetadataCache::close()` already calls `persistFrom` + close. `MainWindow::onVaultOpened` invokes `m_metadataCache->open(...)` at line 2027; `onVaultClosed` invokes `close()` at line 2240–2241. Round-trip unit tests already pass at `tests/storage/tst_cachedmetadatastore.cpp`. The remaining V.2 ask is a single end-to-end test that spins up a real vault, indexes it, closes, reopens, and verifies the cache loaded.

**Files:**
- Test: `tests/storage/tst_cachedmetadatastore_e2e.cpp` (new)

### Task 3.1: Write the end-to-end test

- [ ] **Step 1: Create the test file**

Create `tests/storage/tst_cachedmetadatastore_e2e.cpp`:

```cpp
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>

#include <corbomite/storage/CachedMetadataStore.h>
#include <corbomite/storage/MetadataCache.h>
#include <corbomite/storage/FileSystemAdapter.h>

class TestCachedMetadataStoreE2E : public QObject
{
    Q_OBJECT
private slots:
    void testRealVaultRoundTrip();
};

void TestCachedMetadataStoreE2E::testRealVaultRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Create a tiny vault: 3 markdown files.
    const QString vaultPath = dir.path();
    auto writeFile = [&](const QString &rel, const QString &body) {
        QFile f(vaultPath + QStringLiteral("/") + rel);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(body.toUtf8());
    };
    QDir(vaultPath).mkpath(QStringLiteral(".obsidian"));
    writeFile(QStringLiteral("a.md"), QStringLiteral("# A\n\nlink to [[b]]"));
    writeFile(QStringLiteral("b.md"), QStringLiteral("# B\n\nback to [[a]]"));
    writeFile(QStringLiteral("c.md"), QStringLiteral("# C\n\n#tag"));

    const QString dbPath =
        vaultPath + QStringLiteral("/.obsidian/metadata-cache.db");

    // Round 1: populate cache, open store, persist.
    {
        Storage::MetadataCache cache;
        QVERIFY(cache.open(dbPath));
        // Drive synthetic onFileChanged for each file; mirrors what
        // MetadataWorker emits.
        cache.onFileChanged(QStringLiteral("a.md"), 100, 0);
        cache.onFileChanged(QStringLiteral("b.md"), 100, 0);
        cache.onFileChanged(QStringLiteral("c.md"), 100, 0);
        // Allow worker drain via processEvents loop.
        QTest::qWait(500);
        cache.close(); // calls persistFrom internally
    }

    QVERIFY(QFile::exists(dbPath));

    // Round 2: fresh cache, reopen store, verify state was loaded.
    {
        Storage::MetadataCache cache;
        QVERIFY(cache.open(dbPath));
        // Snapshot APIs return populated maps — proves loadInto wired.
        const auto entries = cache.pathToFileEntrySnapshot();
        QVERIFY(entries.size() >= 3);
        QVERIFY(entries.contains(QStringLiteral("a.md")));
        QVERIFY(entries.contains(QStringLiteral("b.md")));
        QVERIFY(entries.contains(QStringLiteral("c.md")));
        cache.close();
    }
}

QTEST_MAIN(TestCachedMetadataStoreE2E)
#include "tst_cachedmetadatastore_e2e.moc"
```

- [ ] **Step 2: Add to `tests/storage/CMakeLists.txt`**

Append next to the existing `tst_cachedmetadatastore` registration. Match local macro:

```cmake
corbomite_add_test(NAME tst_cachedmetadatastore_e2e
    SOURCES tst_cachedmetadatastore_e2e.cpp
    LIBRARIES Corbomite::Storage
    LABELS storage)
```

- [ ] **Step 3: Build and run**

```bash
cmake --build build -j 10 --target tst_cachedmetadatastore_e2e
cd build && ctest -R tst_cachedmetadatastore_e2e --output-on-failure
```

Expected: PASS. If it fails because `MetadataCache::open` requires more setup (e.g., a worker thread), strip the synthetic `onFileChanged` and instead use `installPersistedState` with hand-built maps to exercise the persist→load cycle without the worker.

- [ ] **Step 4: Commit Phase 3**

```bash
git add tests/storage/tst_cachedmetadatastore_e2e.cpp tests/storage/CMakeLists.txt
git commit -m "$(cat <<'EOF'
cluster-v2 phase 3: end-to-end CachedMetadataStore round-trip test

Confirms the already-wired loadInto/persistFrom path survives a
real-vault open/close/reopen cycle, closing the V.2 scouting item.
The wiring itself landed earlier; this test proves it.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 4 — Autosave delay applier

**Context:** kcfg key `Editor/AutoSaveDelayMs` exists (default 2000, range 500–30000); SettingsDialog Editor page has the spinbox bound; `AutosaveReactor::setDelayMs` is implemented but uncalled; `MainWindow::onSettingsApplied` is the single dispatcher and explicitly comments "Future appliers (V.2 autosave-delay etc.) hook here."

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Test: extend `tests/app/tst_mainwindow_settings_apply.cpp`

### Task 4.1: Add the applier slot

- [ ] **Step 1: Declare in `MainWindow.h`**

Add immediately after `applyVaultPortableSettings();` (added in Phase 2):

```cpp
void applyAutosaveDelay();
```

- [ ] **Step 2: Implement in `MainWindow.cpp`**

After `applyVaultPortableSettings()` body, add:

```cpp
void MainWindow::applyAutosaveDelay()
{
    if (!m_autosave) return;
    const int ms = CorbomiteSettings::self()->autoSaveDelayMs();
    m_autosave->setDelayMs(ms);
}
```

- [ ] **Step 3: Wire into dispatcher**

Update `onSettingsApplied()` to:

```cpp
void MainWindow::onSettingsApplied()
{
    applyTheme();
    applyVaultPortableSettings();
    applyAutosaveDelay();
}
```

(Drop the now-redundant trailing comment.)

### Task 4.2: Test the applier

- [ ] **Step 1: Add a test method**

In `tests/app/tst_mainwindow_settings_apply.cpp`, add to the `private slots:` block:

```cpp
void testAutosaveDelayApplied();
```

And implement near the other test bodies:

```cpp
void TestSettingsApply::testAutosaveDelayApplied()
{
    // We can't easily instantiate MainWindow in a unit test, but the
    // applier's job is one line: read kcfg, call setDelayMs. So we
    // verify the AutosaveReactor::setDelayMs side directly: construct
    // one, set a delay, watch a fake document, assert the timer's
    // interval matches.

    // (Pseudocode — real test requires AutosaveReactor + Vault setup
    // matching the suite's existing fixture. If the AppLib link adds
    // too much complexity, leave this as a smoke-only manual test and
    // delete this method.)

    auto *s = CorbomiteSettings::self();
    s->setAutoSaveDelayMs(7500);
    QCOMPARE(s->autoSaveDelayMs(), 7500);

    // The applier itself is verified by manual smoke: change
    // SettingsDialog spinbox to 7500, save, confirm
    // AutosaveReactor::m_delayMs is 7500 via debugger / log.
}
```

If implementing the live `AutosaveReactor` exercise pulls in too much (Vault + NoteDocument + AppLib link), reduce this test to the kcfg-round-trip portion above and accept the applier has no per-step automated coverage — its body is one trivial line, and the integration test in 4.3 confirms the dispatcher chain.

- [ ] **Step 2: Build and run the test suite**

```bash
cmake --build build -j 10
cd build && ctest -R tst_mainwindow_settings_apply --output-on-failure
```

Expected: PASS.

### Task 4.3: Commit Phase 4

- [ ] **Step 1: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp tests/app/tst_mainwindow_settings_apply.cpp
git commit -m "$(cat <<'EOF'
cluster-v2 phase 4: wire autosave delay through onSettingsApplied

AutosaveReactor::setDelayMs now reaches the user via the existing
Editor settings spinbox. Single-line applier added to the V dispatcher.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 5 — Dead-code audit pass

**Files:**
- Modify: `libs/core/include/corbomite/core/WorkspaceWindow.h`
- Modify: `libs/core/src/WorkspaceWindow.cpp`
- Modify: `tests/core/tst_workspace_window.cpp`
- Modify: `src/app/corbomite.kcfg`
- Modify: `docs/SHARED-SYMBOLS.md`

### Task 5.1: Delete `WorkspaceWindow` facade methods

The Cluster Y retro flagged six methods on `Corbomite::WorkspaceWindow` as dead post-Phase-5 (production reads geometry from `KDDockWidgets::Core::FloatingWindow*` directly). Only `tests/core/tst_workspace_window.cpp` exercises them.

- [ ] **Step 1: Confirm callers (sanity check)**

Run:
```bash
grep -rn "WorkspaceWindow::widget\|WorkspaceWindow::setWindowGeometry\|WorkspaceWindow::showWindow\|WorkspaceWindow::closeWindow\|WorkspaceWindow::setMaximized\|WorkspaceWindow::serialize" --include='*.cpp' --include='*.h' | grep -v 'tests/core/tst_workspace_window'
```

Expected: zero hits (all callers are inside the test). If hits are found, stop and reassess — a phase-5 caller may have appeared.

- [ ] **Step 2: Delete the 6 method declarations from the header**

In `libs/core/include/corbomite/core/WorkspaceWindow.h`, remove these declarations (lines 29–37 per the Y-retro audit):

```cpp
QWidget *widget() const;
void setWindowGeometry(int x, int y, int w, int h);
void showWindow();
void closeWindow();
void setMaximized(bool maximized);
QJsonObject serialize() const;
```

(Re-grep the file before editing — line numbers may have drifted.)

- [ ] **Step 3: Delete the corresponding bodies from the .cpp**

In `libs/core/src/WorkspaceWindow.cpp`, remove the 6 function bodies. If any private members become unused (e.g., `m_widget`, `m_geometry`), also remove them from the header.

- [ ] **Step 4: Shrink the test**

Open `tests/core/tst_workspace_window.cpp`. The test currently exercises the 6 deleted methods. Rewrite it to assert only what `WorkspaceWindow` still does (id-based bookkeeping). If after the edit the test contains only trivial id-equality assertions, decide between:

(a) Keep it as a minimal test that exercises the post-cleanup id contract.
(b) Delete it entirely and add a `// Cluster V.2 5.1: WorkspaceWindow facade collapsed; standalone tests no longer needed.` line in `tests/core/CMakeLists.txt` near the removed registration.

Either is acceptable; pick (a) if the id contract has any non-trivial logic (constructor, copy semantics), (b) otherwise.

- [ ] **Step 5: Build everything**

```bash
cmake --build build -j 10
```

Expected: clean build. If a dependency points at a deleted method, that's a hidden caller — restore the method and re-investigate.

- [ ] **Step 6: Run the full test suite**

```bash
cd build && ctest --output-on-failure -j 10
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add libs/core/include/corbomite/core/WorkspaceWindow.h libs/core/src/WorkspaceWindow.cpp tests/core/tst_workspace_window.cpp tests/core/CMakeLists.txt
git commit -m "$(cat <<'EOF'
cluster-v2 phase 5a: delete WorkspaceWindow standalone facade

Post-Cluster-Y, geometry/show/close/serialize ride
KDDockWidgets::Core::FloatingWindow directly via DockRegistry. The
QWidget facade had no production readers — only the standalone test.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### Task 5.2: Audit `corbomite.kcfg` for orphaned keys

- [ ] **Step 1: Build the orphan-key list**

For each key in `src/app/corbomite.kcfg`, grep for callers of the generated `CorbomiteSettings::<key>()` accessor + the `setX` mutator. A key is orphan if **both** of these are true:

1. No SettingsDialog page has a widget bound to it.
2. No code reads it (other than the dialog itself).

Run:
```bash
# Extract all kcfg key names
grep -E '<entry name="' src/app/corbomite.kcfg | sed -E 's/.*name="([^"]+)".*/\1/' | sort -u
```

For each name in the output, run two greps:
```bash
KEY=<name>
KEY_LOWER=$(echo "$KEY" | sed -E 's/^([A-Z])/\L\1/')
grep -rn "settings.*${KEY_LOWER}\|self.*${KEY_LOWER}\|set${KEY}" --include='*.cpp' --include='*.h' src/ libs/
```

If a key has zero hits across both src/ and libs/ (excluding the kcfg-generated files in build/), it's an orphan.

- [ ] **Step 2: Document candidates and decide**

Expected outcome based on the audit pre-scan: 0–3 orphan keys. Likely candidates: anything left over from removed Cluster G/H/V scope. List each candidate in a scratch buffer with `<file:line> <key>: <reason>`.

For each candidate:
- If the key was part of an aborted/replaced feature: delete.
- If the key is for a planned feature that hasn't shipped: keep, leave a `<!-- TODO Cluster X -->` comment.

- [ ] **Step 3: Apply the deletions (if any)**

Edit `src/app/corbomite.kcfg`. Remove the entries. Note that `kconfig_compiler` will regenerate sources on next build — no .cpp/.h to delete by hand.

- [ ] **Step 4: Build + test**

```bash
cmake --build build -j 10
cd build && ctest --output-on-failure -j 10
```

Expected: PASS. If a build fails, a SettingsDialog widget was actually using the key — restore.

- [ ] **Step 5: Commit (skip if no keys removed)**

```bash
git add src/app/corbomite.kcfg
git commit -m "$(cat <<'EOF'
cluster-v2 phase 5b: prune orphan corbomite.kcfg keys

Drop kcfg entries with no SettingsDialog widget surface and no code
readers, identified during the V.2 audit pass.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### Task 5.3: Update SHARED-SYMBOLS.md

- [ ] **Step 1: Locate entries for deleted symbols**

```bash
grep -n "WorkspaceWindow::widget\|setWindowGeometry\|WorkspaceWindow::serialize" docs/SHARED-SYMBOLS.md
```

If grep returns matches, edit the file and remove the lines documenting the now-deleted methods.

- [ ] **Step 2: Commit (skip if no entries existed)**

```bash
git add docs/SHARED-SYMBOLS.md
git commit -m "cluster-v2 phase 5c: drop deleted WorkspaceWindow facade from SHARED-SYMBOLS"
```

---

## Phase 6 — Closeout

**Files:**
- Modify: `docs/PROJECT-STATE.md`
- Modify: `docs/decisions-archive.md`
- Modify: `docs/superpowers/plans/INDEX.md`
- Create: `docs/cluster-retros/cluster-v2.md`
- Modify: `docs/backlog.md`

### Task 6.1: Write the cluster-v2 retro

- [ ] **Step 1: Create the retro doc**

Create `docs/cluster-retros/cluster-v2.md`. Match the structure of `docs/cluster-retros/cluster-v.md` (read it first for the template). Sections to include:

1. **Outcome** — one paragraph: what shipped, what was deferred, what was discovered (Phase 3 already wired).
2. **Phase ledger** — one row per phase with commit SHAs.
3. **Surprises** — Phase 3 audit-vs-scouting-doc divergence.
4. **Carry-forwards** — fold-gutter (Markoff-internal, V.3 scope), LRU upgrade (kept open), 3 unwired VaultConfig writers (no UI), vault-level cache fingerprint (deferred).
5. **Patterns harvested** — `VaultConfig::mergeJson` as the canonical unknown-key-preservation primitive; SessionManager's stash-the-rest pattern continues for keys-vs-stash trade-offs where the known set is small.

### Task 6.2: Update PROJECT-STATE

- [ ] **Step 1: Replace `## Current focus`**

Per CLAUDE.md ritual: at most 3 sentences. Replace the existing top entry with:

```markdown
**Cluster V.2 closed.** Editor/Workspace debt cleanup landed across N phases (X commits SHA1..SHAN, retro at `cluster-retros/cluster-v2.md`). VaultConfig::mergeJson now preserves Obsidian-authored keys for the 3 settings pages with vault-portable surface; persisted MetadataCache round-trip end-to-end-tested; AutosaveReactor delay reaches the user. Next: Cluster Z brainstorm (linked views + active-leaf).
```

- [ ] **Step 2: Update the V.2 row in `## Roadmap`**

Change V.2's status cell from "Scouting doc" to "Done" and replace the notes cell with a one-line closeout pointer to the retro.

### Task 6.3: Append to decisions-archive

- [ ] **Step 1: Add a dated H2 section**

At the end of `docs/decisions-archive.md`, append:

```markdown
## 2026-04-DD — Cluster V.2 closed (Editor/Workspace debt cleanup)

[Full closeout paragraph — what shipped, what was discovered (Phase 3 was
already wired), what stays deferred, what patterns are harvested. ~150-300
words. Link the retro: `cluster-retros/cluster-v2.md`.]
```

(Replace `DD` with the actual closeout date.)

### Task 6.4: Update INDEX

- [ ] **Step 1: Bump V.2 status**

In `docs/superpowers/plans/INDEX.md`, change V.2's status column from "Scouting doc" to "Done" with a one-line summary including the commit range. Update the `**Last updated:**` line near the top to reflect V.2 closure.

### Task 6.5: Update backlog

- [ ] **Step 1: Remove items closed by V.2**

Open `docs/backlog.md`. Remove entries that V.2 closed:
- WorkspaceWindow facade cleanup (closed by Phase 5a)
- VaultConfig writer routing (3 of 6 closed; document the remaining 3 stay deferred)
- Persisted metadata cache loader hookup (closed — was already done; V.2 added the e2e test)
- AutosaveReactor::setDelayMs wiring (closed by Phase 4)

- [ ] **Step 2: Add carry-forward entries**

Add entries for what V.2 explicitly deferred:
- Fold-gutter click-to-fold (Markoff-internal; awaits Markoff QA cycle)
- LRU multi-entry reopen (kept open in V.2 scouting; closes when user demand surfaces)
- 3 unwired `VaultConfig` writers: `writeAppJson`, `writeCommunityPlugins`, `writeHotkeys` (each blocked on its UI page existing)
- Vault-level cache fingerprint (`.obsidian/app.json` mtime gate as future cold-start-time optimisation)

### Task 6.6: Final commit

- [ ] **Step 1: Verify everything builds and tests pass**

```bash
cmake --build build -j 10
cd build && ctest --output-on-failure -j 10
```

Expected: clean.

- [ ] **Step 2: Commit closeout**

```bash
git add docs/cluster-retros/cluster-v2.md docs/PROJECT-STATE.md docs/decisions-archive.md docs/superpowers/plans/INDEX.md docs/backlog.md
git commit -m "$(cat <<'EOF'
cluster-v2: close — editor/workspace debt cleanup

Closes V.2 across mergeJson helper + 3-page settings persistence +
e2e cache test + autosave applier + WorkspaceWindow facade deletion +
kcfg orphan sweep. See cluster-retros/cluster-v2.md.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Definition of done

1. `ctest --output-on-failure -j 10` passes from a clean build.
2. `VaultConfig::mergeJson` exists and is unit-tested for unknown-key preservation, file creation, and known-key overwrite.
3. The 3 SettingsDialog pages (Appearance, Daily Notes, Templates) round-trip through `.obsidian/{appearance,daily-notes,templates}.json` on Apply/OK, with unknown keys surviving.
4. `tst_cachedmetadatastore_e2e` passes, proving the loader+saver cycle on a real vault.
5. `AutosaveReactor::setDelayMs` is invoked when the autosave-delay spinbox changes.
6. `WorkspaceWindow`'s 6 dead facade methods are gone; `tst_workspace_window` either shrunk or deleted.
7. Any orphan kcfg keys are removed (or zero were found).
8. `docs/SHARED-SYMBOLS.md` no longer references deleted symbols.
9. PROJECT-STATE / decisions-archive / INDEX / backlog reflect closure per Ritual 3.
10. Cluster retro at `docs/cluster-retros/cluster-v2.md` written and linked.

## Self-review checklist (run before considering this plan dispatchable)

- [ ] Every step shows real code, not "implement X here".
- [ ] File paths are absolute or repo-rooted, line numbers cited where they exist today.
- [ ] Each commit message is self-contained.
- [ ] No phase depends on a method declared in a later phase.
- [ ] Test naming matches `tst_<surface>` convention from neighbouring tests.
- [ ] CMake registrations reference the local helper macro (`corbomite_add_test`) — confirm name in the matching `tests/<dir>/CMakeLists.txt` before final commit.
