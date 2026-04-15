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
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QElapsedTimer>
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
        // rebuildVault reconciles against the caller-supplied canonical
        // list — anything tracked but missing from the list is treated
        // as implicitly deleted and emits cacheDeleted.
        QCOMPARE(deletedSpy.count(), 1);
        QVERIFY(cache.getFileHash(notePath).isEmpty());

        cache.close();
    }

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
};

QTEST_MAIN(TestCrossSession)
#include "tst_cross_session.moc"
