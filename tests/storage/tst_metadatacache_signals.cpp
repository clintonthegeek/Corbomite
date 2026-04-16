// SPDX-License-Identifier: GPL-3.0-or-later
//
// Spec-driven signal-contract tests for MetadataCache.
//
// Verifies the five-signal contract documented in:
//   docs/superpowers/plans/2026-04-15-cluster-i-metadatacache-parity.md
//   §"Phase 4: Signal-contract parity"
//
// Claims under test:
//   1. MetadataCache exposes exactly 5 signals:
//        cacheChanged, cacheDeleted, linksResolvedFor, allLinksResolved, indexFinished
//   2. cacheChanged(path, prevHash, cache) fires after a file is parsed.
//      - New path → prevHash is "".
//      - Updated path → prevHash is the old hash (non-empty).
//      - The CachedMetadata payload reflects the parsed content.
//   3. cacheDeleted(path, prevCache) fires after onFileDeleted.
//      - prevCache carries the metadata that was stored before deletion.
//      - At emit-time, getFileCache(path) already returns nullopt.
//   4. indexFinished() fires after work drains through the link-resolver debounce.
//      - Fires exactly once per work batch (not once per file).
//   5. Short-circuit paths (stat unchanged, hash unchanged) are silent.
//   6. Signals coalesce during batch operations: indexFinished fires once, not N times.

#include <QTest>
#include <QSignalSpy>

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QVariantList>

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"

using namespace Corbomite;

namespace {

// Build a LinkResolver pre-loaded with the given vault paths.
LinkResolver makeResolver(const QStringList &paths = {})
{
    LinkResolver r;
    r.setVaultPaths(paths);
    return r;
}

// Pump the event loop long enough for the worker-thread parsed() signal to
// arrive on the main thread and for all QTimer::singleShot(0,...) drain
// continuations to execute. Does NOT wait for the 10ms indexFinished debounce.
void pumpDrain()
{
    for (int i = 0; i < 40; ++i) {
        QCoreApplication::processEvents();
        QTest::qWait(1);
    }
}

} // namespace

class TestMetadataCacheSignals : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // ---------------------------------------------------------------------------
    // Claim 1 — Five distinct signals exist (compile-time check via QSignalSpy).
    // ---------------------------------------------------------------------------

    void testFiveSignalsExist()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("x.md")});
        MetadataCache cache(resolver);

        // If any signal doesn't exist this won't compile / the spy will be invalid.
        QSignalSpy spyChanged    (&cache, &MetadataCache::cacheChanged);
        QSignalSpy spyDeleted    (&cache, &MetadataCache::cacheDeleted);
        QSignalSpy spyResolvedFor(&cache, &MetadataCache::linksResolvedFor);
        QSignalSpy spyAllResolved(&cache, &MetadataCache::allLinksResolved);
        QSignalSpy spyFinished   (&cache, &MetadataCache::indexFinished);

        QVERIFY(spyChanged.isValid());
        QVERIFY(spyDeleted.isValid());
        QVERIFY(spyResolvedFor.isValid());
        QVERIFY(spyAllResolved.isValid());
        QVERIFY(spyFinished.isValid());
    }

    // ---------------------------------------------------------------------------
    // Claim 2 — cacheChanged fires with correct arguments after parse.
    // ---------------------------------------------------------------------------

    // 2a. cacheChanged fires at least once per file on a new path.
    void testCacheChangedFiresForNewFile()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("note.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);

        cache.onFileChanged(QStringLiteral("note.md"),
                            QByteArray("# My Heading\n"), 1000);

        // Worker is async — wait for the signal to arrive.
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
    }

    // 2b. Path argument of cacheChanged matches the file passed to onFileChanged.
    void testCacheChangedPathMatchesFile()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md"),
                                              QStringLiteral("b.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        cache.onFileChanged(QStringLiteral("b.md"), QByteArray("# B\n"), 200);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 3000);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("a.md"));
        QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("b.md"));
    }

    // 2c. prevHash is "" for a brand-new path (it had no prior hash in the cache).
    void testCacheChangedPrevHashEmptyForNewPath()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("fresh.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);

        cache.onFileChanged(QStringLiteral("fresh.md"), QByteArray("hello\n"), 100);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
        // Arg index 1 is prevHash — must be empty for a new path.
        QCOMPARE(spy.at(0).at(1).toString(), QString{});
    }

    // 2d. prevHash is the old hash (non-empty) when a file's content changes.
    void testCacheChangedPrevHashCarriesOldHashOnUpdate()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        // First parse — establishes a hash in the cache.
        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# V1\n"), 100);
        QTRY_VERIFY_WITH_TIMEOUT(!cache.getFileHash(QStringLiteral("a.md")).isEmpty(),
                                 3000);
        const QString firstHash = cache.getFileHash(QStringLiteral("a.md"));

        // Second parse with different content — prevHash should be firstHash.
        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);
        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# V2\n"), 200);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
        QCOMPARE(spy.at(0).at(1).toString(), firstHash);
    }

    // 2e. The CachedMetadata payload reflects the parsed content.
    void testCacheChangedPayloadReflectsContent()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("doc.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);

        cache.onFileChanged(QStringLiteral("doc.md"),
                            QByteArray("# Title\n\nSome text.\n"), 100);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
        const auto cm = spy.at(0).at(2).value<CachedMetadata>();
        QVERIFY(cm.headings.has_value());
        QVERIFY(!cm.headings->isEmpty());
        QCOMPARE(cm.headings->at(0).heading, QStringLiteral("Title"));
        QCOMPARE(cm.headings->at(0).level, 1);
    }

    // ---------------------------------------------------------------------------
    // Claim 3 — cacheDeleted fires with correct arguments.
    // ---------------------------------------------------------------------------

    // 3a. cacheDeleted fires synchronously when onFileDeleted is called.
    void testCacheDeletedFiresOnFileDeleted()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("del.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("del.md"),
                            QByteArray("# Deletable\n#mytag\n"), 100);
        QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), 1, 3000);
        QTest::qWait(50);  // let indexFinished debounce settle

        QSignalSpy spy(&cache, &MetadataCache::cacheDeleted);
        cache.onFileDeleted(QStringLiteral("del.md"));

        // cacheDeleted is synchronous — fires immediately, no event-loop needed.
        QCOMPARE(spy.count(), 1);
    }

    // 3b. The path argument of cacheDeleted matches the file passed to onFileDeleted.
    void testCacheDeletedPathMatches()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("target.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("target.md"), QByteArray("hi\n"), 100);
        QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), 1, 3000);
        QTest::qWait(50);

        QSignalSpy spy(&cache, &MetadataCache::cacheDeleted);
        cache.onFileDeleted(QStringLiteral("target.md"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("target.md"));
    }

    // 3c. prevCache payload carries the metadata that existed before deletion.
    void testCacheDeletedPrevCacheCarriesMetadata()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("rich.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("rich.md"),
                            QByteArray("# Rich Heading\n#alpha\n"), 100);
        QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), 1, 3000);
        QTest::qWait(50);

        QSignalSpy spy(&cache, &MetadataCache::cacheDeleted);
        cache.onFileDeleted(QStringLiteral("rich.md"));

        QCOMPARE(spy.count(), 1);
        const auto prevCache = spy.at(0).at(1).value<CachedMetadata>();
        QVERIFY(prevCache.headings.has_value());
        QCOMPARE(prevCache.headings->at(0).heading, QStringLiteral("Rich Heading"));
        QVERIFY(prevCache.tags.has_value());
        QCOMPARE(prevCache.tags->at(0).tag, QStringLiteral("#alpha"));
    }

    // 3d. Spec audit gotcha: at emit-time, getFileCache(path) already returns
    //     nullopt — the path/hash teardown happens BEFORE the signal fires.
    void testCacheDeletedPathAlreadyClearedAtEmitTime()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("gone.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("gone.md"), QByteArray("# Gone\n"), 100);
        QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), 1, 3000);
        QTest::qWait(50);

        bool slotRan = false;
        bool fileCacheWasNull = false;
        connect(&cache, &MetadataCache::cacheDeleted, this,
                [&](const QString &path, const CachedMetadata &) {
                    slotRan = true;
                    fileCacheWasNull = !cache.getFileCache(path).has_value();
                });

        cache.onFileDeleted(QStringLiteral("gone.md"));
        QVERIFY(slotRan);
        QVERIFY(fileCacheWasNull);
    }

    // 3e. Deleting a path that was never inserted emits no signals.
    void testCacheDeletedSilentForUnknownPath()
    {
        LinkResolver resolver;
        MetadataCache cache(resolver);

        QSignalSpy spyDeleted (&cache, &MetadataCache::cacheDeleted);
        QSignalSpy spyChanged (&cache, &MetadataCache::cacheChanged);
        QSignalSpy spyFinished(&cache, &MetadataCache::indexFinished);

        cache.onFileDeleted(QStringLiteral("nonexistent.md"));
        pumpDrain();
        QTest::qWait(50);

        QCOMPARE(spyDeleted.count(), 0);
        QCOMPARE(spyChanged.count(), 0);
        QCOMPARE(spyFinished.count(), 0);
    }

    // 3f. cacheDeleted does NOT trigger linksResolvedFor or allLinksResolved.
    //     Spec: delete does not touch the resolver queue.
    void testCacheDeletedDoesNotTriggerLinkResolution()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), 1, 3000);
        QTest::qWait(50);  // drain all pending link-resolve events

        QSignalSpy spyResolvedFor(&cache, &MetadataCache::linksResolvedFor);
        QSignalSpy spyAllResolved(&cache, &MetadataCache::allLinksResolved);

        cache.onFileDeleted(QStringLiteral("a.md"));
        pumpDrain();
        QTest::qWait(50);

        QCOMPARE(spyResolvedFor.count(), 0);
        QCOMPARE(spyAllResolved.count(), 0);
    }

    // ---------------------------------------------------------------------------
    // Claim 4 — indexFinished fires after work drains through the debounce.
    // ---------------------------------------------------------------------------

    // 4a. indexFinished fires after a single file change + drain.
    void testIndexFinishedFiresAfterSingleChange()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::indexFinished);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);

        // Allow worker roundtrip (async) + drain + 10ms debounce.
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
    }

    // 4b. allLinksResolved fires before indexFinished.
    void testAllLinksResolvedFiresBeforeIndexFinished()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        QStringList order;
        connect(&cache, &MetadataCache::allLinksResolved, this,
                [&]() { order << QStringLiteral("allLinksResolved"); });
        connect(&cache, &MetadataCache::indexFinished, this,
                [&]() { order << QStringLiteral("indexFinished"); });

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);

        QTRY_VERIFY_WITH_TIMEOUT(order.size() >= 2, 3000);
        QCOMPARE(order.at(0), QStringLiteral("allLinksResolved"));
        QCOMPARE(order.at(1), QStringLiteral("indexFinished"));
    }

    // ---------------------------------------------------------------------------
    // Claim 5 — Short-circuit paths are silent.
    // ---------------------------------------------------------------------------

    // 5a. Identical stat (same mtime+size) → no signals fire at all.
    void testStatShortCircuitIsSilent()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("s.md")});
        MetadataCache cache(resolver);

        const QByteArray content("# Stable\n");
        cache.onFileChanged(QStringLiteral("s.md"), content, 500);
        QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), 1, 3000);
        QTest::qWait(50);

        // Now attach spies AFTER the initial parse is settled.
        QSignalSpy spyChanged    (&cache, &MetadataCache::cacheChanged);
        QSignalSpy spyResolvedFor(&cache, &MetadataCache::linksResolvedFor);
        QSignalSpy spyAllResolved(&cache, &MetadataCache::allLinksResolved);
        QSignalSpy spyFinished   (&cache, &MetadataCache::indexFinished);

        // Same mtime (500), same content → stat short-circuit.
        cache.onFileChanged(QStringLiteral("s.md"), content, 500);
        pumpDrain();
        QTest::qWait(50);

        QCOMPARE(spyChanged.count(), 0);
        QCOMPARE(spyResolvedFor.count(), 0);
        QCOMPARE(spyAllResolved.count(), 0);
        QCOMPARE(spyFinished.count(), 0);
    }

    // 5b. Stat changed but hash unchanged (same content, new mtime) → silent.
    //     Spec §"no content change, no event".
    void testHashShortCircuitIsSilent()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("h.md")});
        MetadataCache cache(resolver);

        const QByteArray content("# Hash Stable\n");
        cache.onFileChanged(QStringLiteral("h.md"), content, 100);
        QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), 1, 3000);
        QTest::qWait(50);

        QSignalSpy spyChanged    (&cache, &MetadataCache::cacheChanged);
        QSignalSpy spyResolvedFor(&cache, &MetadataCache::linksResolvedFor);
        QSignalSpy spyAllResolved(&cache, &MetadataCache::allLinksResolved);
        QSignalSpy spyFinished   (&cache, &MetadataCache::indexFinished);

        // Different mtime (stat changes) but identical bytes → hash unchanged.
        cache.onFileChanged(QStringLiteral("h.md"), content, 999);
        pumpDrain();
        QTest::qWait(50);

        QCOMPARE(spyChanged.count(), 0);
        QCOMPARE(spyResolvedFor.count(), 0);
        QCOMPARE(spyAllResolved.count(), 0);
        QCOMPARE(spyFinished.count(), 0);
    }

    // ---------------------------------------------------------------------------
    // Claim 6 — Signals coalesce in batch: indexFinished fires once, not N times.
    // ---------------------------------------------------------------------------

    // 6a. Three rapid onFileChanged calls produce exactly one indexFinished.
    void testIndexFinishedCoalescesOverBatch()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md"),
                                              QStringLiteral("b.md"),
                                              QStringLiteral("c.md")});
        MetadataCache cache(resolver);
        QSignalSpy spyFinished(&cache, &MetadataCache::indexFinished);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        cache.onFileChanged(QStringLiteral("b.md"), QByteArray("# B\n"), 200);
        cache.onFileChanged(QStringLiteral("c.md"), QByteArray("# C\n"), 300);

        // Wait for the batch to drain and the single debounced finish to arrive.
        QTRY_COMPARE_WITH_TIMEOUT(spyFinished.count(), 1, 3000);

        // Extra safety: give it more time to confirm no second fire.
        QTest::qWait(100);
        QCOMPARE(spyFinished.count(), 1);
    }

    // 6b. Three rapid changes each produce one cacheChanged (total = 3).
    //     Verifies that coalescing does not suppress per-file signals.
    void testCacheChangedFiresOncePerFileInBatch()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("x.md"),
                                              QStringLiteral("y.md"),
                                              QStringLiteral("z.md")});
        MetadataCache cache(resolver);
        QSignalSpy spyChanged (&cache, &MetadataCache::cacheChanged);
        QSignalSpy spyFinished(&cache, &MetadataCache::indexFinished);

        cache.onFileChanged(QStringLiteral("x.md"), QByteArray("# X\n"), 10);
        cache.onFileChanged(QStringLiteral("y.md"), QByteArray("# Y\n"), 20);
        cache.onFileChanged(QStringLiteral("z.md"), QByteArray("# Z\n"), 30);

        QTRY_COMPARE_WITH_TIMEOUT(spyChanged.count(), 3, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(spyFinished.count(), 1, 3000);
    }

    // 6c. Three rapid changes produce exactly three linksResolvedFor emissions.
    void testLinksResolvedForFiresOncePerFileInBatch()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("p.md"),
                                              QStringLiteral("q.md"),
                                              QStringLiteral("r.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::linksResolvedFor);

        cache.onFileChanged(QStringLiteral("p.md"), QByteArray("# P\n"), 1);
        cache.onFileChanged(QStringLiteral("q.md"), QByteArray("# Q\n"), 2);
        cache.onFileChanged(QStringLiteral("r.md"), QByteArray("# R\n"), 3);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 3, 3000);
    }

    // ---------------------------------------------------------------------------
    // Additional ordering constraint: cacheChanged fires before linksResolvedFor
    // for the same path (spec §"cacheChanged → linksResolvedFor ordering").
    // ---------------------------------------------------------------------------

    void testCacheChangedFiresBeforeLinksResolvedFor()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("ord.md")});
        MetadataCache cache(resolver);

        QStringList eventOrder;
        connect(&cache, &MetadataCache::cacheChanged, this,
                [&](const QString &path, const QString &, const CachedMetadata &) {
                    eventOrder << (QStringLiteral("changed:") + path);
                });
        connect(&cache, &MetadataCache::linksResolvedFor, this,
                [&](const QString &path) {
                    eventOrder << (QStringLiteral("resolve:") + path);
                });

        cache.onFileChanged(QStringLiteral("ord.md"), QByteArray("# Ord\n"), 100);

        QTRY_VERIFY_WITH_TIMEOUT(eventOrder.size() >= 2, 3000);
        QCOMPARE(eventOrder.at(0), QStringLiteral("changed:ord.md"));
        QCOMPARE(eventOrder.at(1), QStringLiteral("resolve:ord.md"));
    }
};

QTEST_MAIN(TestMetadataCacheSignals)
#include "tst_metadatacache_signals.moc"
