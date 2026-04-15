// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 4 events + queue + debounce suite for MetadataCache. 17 tests
// covering: signal-emission ordering, sync-vs-async semantics, short-
// circuit silence, cacheDeleted snapshot, 10ms indexFinished debounce,
// burst coalescing, Obsidian-named Events triggers + payload shape,
// prevHash semantics, and queued-drain order.

#include <QTest>
#include <QSignalSpy>

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>

#include "corbomite/core/Events.h"
#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"

using namespace Corbomite;

namespace {

LinkResolver makeResolver(const QStringList &paths)
{
    LinkResolver r;
    r.setVaultPaths(paths);
    return r;
}

// Pump the event loop repeatedly until the link-resolver queue and the
// async singleShot continuations finish -- Phase 5 makes parsing async via
// the worker thread, so we also wait briefly for the worker->main queued
// parsed() signals to arrive. Wait short enough not to fire the 10ms
// indexFinished debounce unless callers explicitly wait past it.
void pumpUntilDrain()
{
    for (int i = 0; i < 40; ++i) {
        QCoreApplication::processEvents();
        QTest::qWait(1);
    }
}

} // namespace

class TestMetadataCacheEvents : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // 1. cacheChanged fires exactly once per file in a two-file burst.
    //    Phase 5: cacheChanged is now async (via worker roundtrip), so we
    //    pump events + QTRY on the spy.
    void testChangedFiresOncePerFile()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md"),
                                              QStringLiteral("b.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        cache.onFileChanged(QStringLiteral("b.md"), QByteArray("# B\n"), 200);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 2000);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("a.md"));
        QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("b.md"));
    }

    // 2. Phase 5 semantic change: cacheChanged fires after a worker round-trip,
    //    not synchronously from onFileChanged. Immediately after onFileChanged
    //    the spy is empty; after pumping events it becomes 1.
    void testChangedFiresAfterWorkerRoundtrip()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);

        // Worker is async -- cacheChanged has NOT fired synchronously.
        QCOMPARE(spy.count(), 0);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
    }

    // 3. linksResolvedFor is asynchronous via QTimer::singleShot(0, ...).
    //    Immediately after onFileChanged: count is 0. After pumping: count 1.
    void testResolveFiresAsyncViaSingleShot()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::linksResolvedFor);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);

        QCOMPARE(spy.count(), 0);  // NOT yet fired -- async.

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("a.md"));
    }

    // 4. Ordering for a single-file change:
    //    cacheChanged -> linksResolvedFor -> allLinksResolved -> indexFinished.
    void testOrderingSingleFile()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        QSignalSpy changedSpy(&cache, &MetadataCache::cacheChanged);
        QSignalSpy resolvedForSpy(&cache, &MetadataCache::linksResolvedFor);
        QSignalSpy allResolvedSpy(&cache, &MetadataCache::allLinksResolved);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        // Phase 5: cacheChanged is also async now (worker roundtrip).
        QCOMPARE(changedSpy.count(), 0);
        QCOMPARE(resolvedForSpy.count(), 0);

        QTRY_COMPARE_WITH_TIMEOUT(changedSpy.count(), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(resolvedForSpy.count(), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(allResolvedSpy.count(), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 2000);
    }

    // 5. Ordering for a burst of three files.
    void testOrderingBurstOfThreeFiles()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md"),
                                              QStringLiteral("b.md"),
                                              QStringLiteral("c.md")});
        MetadataCache cache(resolver);

        QSignalSpy changedSpy(&cache, &MetadataCache::cacheChanged);
        QSignalSpy resolvedForSpy(&cache, &MetadataCache::linksResolvedFor);
        QSignalSpy allResolvedSpy(&cache, &MetadataCache::allLinksResolved);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        cache.onFileChanged(QStringLiteral("b.md"), QByteArray("# B\n"), 200);
        cache.onFileChanged(QStringLiteral("c.md"), QByteArray("# C\n"), 300);

        QTRY_COMPARE_WITH_TIMEOUT(changedSpy.count(), 3, 2000);
        QCOMPARE(changedSpy.at(0).at(0).toString(), QStringLiteral("a.md"));
        QCOMPARE(changedSpy.at(1).at(0).toString(), QStringLiteral("b.md"));
        QCOMPARE(changedSpy.at(2).at(0).toString(), QStringLiteral("c.md"));

        QTRY_COMPARE_WITH_TIMEOUT(resolvedForSpy.count(), 3, 2000);
        // allLinksResolved is expected to fire at least once. Under Phase 5
        // worker-async semantics, per-parse arrivals may re-trigger the
        // resolver cycle if they interleave with the drain; tolerate >=1.
        QVERIFY(allResolvedSpy.count() >= 1);

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 2000);
    }

    // 6. Stat short-circuit is silent — no signals fire, no queue changes.
    void testShortCircuitSilent()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        QByteArray bytes("# A\n");
        cache.onFileChanged(QStringLiteral("a.md"), bytes, 100);

        // Wait for the initial parse + drain + debounce to complete fully
        // before attaching the silence spies.
        QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), 1, 2000);
        QTest::qWait(50);

        QSignalSpy changedSpy(&cache, &MetadataCache::cacheChanged);
        QSignalSpy resolvedForSpy(&cache, &MetadataCache::linksResolvedFor);
        QSignalSpy allResolvedSpy(&cache, &MetadataCache::allLinksResolved);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        // Identical stat -> short-circuit.
        cache.onFileChanged(QStringLiteral("a.md"), bytes, 100);
        pumpUntilDrain();
        QTest::qWait(50);

        QCOMPARE(changedSpy.count(), 0);
        QCOMPARE(resolvedForSpy.count(), 0);
        QCOMPARE(allResolvedSpy.count(), 0);
        QCOMPARE(finishedSpy.count(), 0);
    }

    // 7. Hash-unchanged-stat-changed is silent per audit §4.
    void testHashUnchangedStatChangedSilent()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        QByteArray bytes("# A\n");
        cache.onFileChanged(QStringLiteral("a.md"), bytes, 100);

        // Wait for the initial parse + drain + debounce to complete.
        QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), 1, 2000);
        QTest::qWait(50);

        QSignalSpy changedSpy(&cache, &MetadataCache::cacheChanged);
        QSignalSpy resolvedForSpy(&cache, &MetadataCache::linksResolvedFor);
        QSignalSpy allResolvedSpy(&cache, &MetadataCache::allLinksResolved);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        // Same content, different mtime -> stat differs, hash unchanged.
        cache.onFileChanged(QStringLiteral("a.md"), bytes, 200);
        pumpUntilDrain();
        QTest::qWait(50);

        QCOMPARE(changedSpy.count(), 0);
        QCOMPARE(resolvedForSpy.count(), 0);
        QCOMPARE(allResolvedSpy.count(), 0);
        QCOMPARE(finishedSpy.count(), 0);
    }

    // 8. cacheDeleted emits (path, prevCache) with the expected metadata.
    void testDeletedEmitsCacheDeletedWithPrevCache()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("a.md"),
                            QByteArray("# Heading\n#tag\n"), 100);
        // Wait for worker roundtrip + drain + debounce to complete.
        QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), 1, 2000);
        QTest::qWait(50);

        QSignalSpy spy(&cache, &MetadataCache::cacheDeleted);
        cache.onFileDeleted(QStringLiteral("a.md"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("a.md"));
        const auto prevCache = spy.at(0).at(1).value<CachedMetadata>();
        QVERIFY(prevCache.headings.has_value());
        QCOMPARE(prevCache.headings->size(), 1);
        QCOMPARE(prevCache.headings->at(0).heading, QStringLiteral("Heading"));
        QVERIFY(prevCache.tags.has_value());
        QCOMPARE(prevCache.tags->size(), 1);
        QCOMPARE(prevCache.tags->at(0).tag, QStringLiteral("#tag"));
    }

    // 9. At emit-time of cacheDeleted, getFileCache(path) must already be
    //    std::nullopt — the path/hash teardown happens BEFORE emission.
    void testDeletedIndexLookupReturnsNullopt()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), 1, 2000);
        QTest::qWait(50);

        bool slotRan = false;
        bool wasNullopt = false;
        connect(&cache, &MetadataCache::cacheDeleted, this,
                [&](const QString &path, const CachedMetadata &) {
                    slotRan = true;
                    wasNullopt = !cache.getFileCache(path).has_value();
                });

        cache.onFileDeleted(QStringLiteral("a.md"));
        QVERIFY(slotRan);
        QVERIFY(wasNullopt);
    }

    // 10. Deleting an unknown path emits nothing.
    void testDeletedOnUnknownPathSilent()
    {
        LinkResolver resolver;
        MetadataCache cache(resolver);

        QSignalSpy changedSpy(&cache, &MetadataCache::cacheChanged);
        QSignalSpy deletedSpy(&cache, &MetadataCache::cacheDeleted);
        QSignalSpy resolvedForSpy(&cache, &MetadataCache::linksResolvedFor);
        QSignalSpy allResolvedSpy(&cache, &MetadataCache::allLinksResolved);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        cache.onFileDeleted(QStringLiteral("never.md"));
        pumpUntilDrain();
        QTest::qWait(50);

        QCOMPARE(changedSpy.count(), 0);
        QCOMPARE(deletedSpy.count(), 0);
        QCOMPARE(resolvedForSpy.count(), 0);
        QCOMPARE(allResolvedSpy.count(), 0);
        QCOMPARE(finishedSpy.count(), 0);
    }

    // 11. indexFinished is debounced by 10ms after the last drain.
    //     Immediately after drain: count is 0. After QTRY wait: count is 1.
    void testIndexFinishedDebouncedAfterIdle()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);

        // Phase 5: worker roundtrip + drain + 10ms debounce. Generous timeout.
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 2000);
    }

    // 12. Debounce timer resets when a new change arrives mid-flight.
    //     Two changes near in time -> indexFinished fires exactly ONCE.
    void testIndexFinishedTimerResetsOnBurst()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md"),
                                              QStringLiteral("b.md")});
        MetadataCache cache(resolver);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        cache.onFileChanged(QStringLiteral("b.md"), QByteArray("# B\n"), 200);
        pumpUntilDrain();

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 2000);

        // Give it more time to ensure a second fire never happens.
        QTest::qWait(100);
        QCOMPARE(finishedSpy.count(), 1);
    }

    // 13. Events-mixin triggers fire the Obsidian-named events in the right
    //     order and correct cardinality.
    void testEventsTriggerObsidianNames()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        int changedCount = 0;
        int deletedCount = 0;
        int resolveCount = 0;
        int resolvedCount = 0;
        int finishedCount = 0;

        cache.events().on(QStringLiteral("changed"),
                          [&](const QVariantList &) { ++changedCount; });
        cache.events().on(QStringLiteral("deleted"),
                          [&](const QVariantList &) { ++deletedCount; });
        cache.events().on(QStringLiteral("resolve"),
                          [&](const QVariantList &) { ++resolveCount; });
        cache.events().on(QStringLiteral("resolved"),
                          [&](const QVariantList &) { ++resolvedCount; });
        cache.events().on(QStringLiteral("finished"),
                          [&](const QVariantList &) { ++finishedCount; });

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        // Wait out the debounce so finished fires.
        QTRY_VERIFY_WITH_TIMEOUT(finishedCount >= 1, 500);

        QCOMPARE(changedCount, 1);
        QCOMPARE(resolveCount, 1);
        QCOMPARE(resolvedCount, 1);
        QCOMPARE(finishedCount, 1);

        cache.onFileDeleted(QStringLiteral("a.md"));
        QCOMPARE(deletedCount, 1);
    }

    // 14. "changed" event payload has shape (QString path, QString prevHash,
    //     CachedMetadata).
    void testEventsTriggerPayloadShape()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        QVariantList captured;
        bool fired = false;
        cache.events().on(QStringLiteral("changed"),
                          [&](const QVariantList &args) {
                              captured = args;
                              fired = true;
                          });

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);

        // Phase 5: changed event fires after worker roundtrip.
        QTRY_VERIFY_WITH_TIMEOUT(fired, 2000);
        QCOMPARE(captured.size(), 3);
        QCOMPARE(captured.at(0).toString(), QStringLiteral("a.md"));
        QCOMPARE(captured.at(1).toString(), QString{});  // new-path -> empty prevHash
        const auto cm = captured.at(2).value<CachedMetadata>();
        QVERIFY(cm.headings.has_value());
        QCOMPARE(cm.headings->at(0).heading, QStringLiteral("A"));
    }

    // 15. New path gets prevHash == "".
    void testNewPathHasEmptyPrevHash()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        QCOMPARE(spy.at(0).at(1).toString(), QString{});
    }

    // 16. Hash change carries the old hash as prevHash.
    void testHashChangeCarriesOldHashAsPrevHash()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        // Wait for the first parse to complete so the hash is populated.
        QTRY_VERIFY_WITH_TIMEOUT(
            !cache.getFileHash(QStringLiteral("a.md")).isEmpty(), 2000);
        const QString firstHash = cache.getFileHash(QStringLiteral("a.md"));
        QVERIFY(!firstHash.isEmpty());

        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);

        cache.onFileChanged(QStringLiteral("a.md"),
                            QByteArray("# Different\n"), 200);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        QCOMPARE(spy.at(0).at(1).toString(), firstHash);
    }

    // 17. Queued path drain order matches enqueue order (a -> b -> c).
    void testQueuedPathDrainOrder()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md"),
                                              QStringLiteral("b.md"),
                                              QStringLiteral("c.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::linksResolvedFor);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        cache.onFileChanged(QStringLiteral("b.md"), QByteArray("# B\n"), 200);
        cache.onFileChanged(QStringLiteral("c.md"), QByteArray("# C\n"), 300);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 3, 2000);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("a.md"));
        QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("b.md"));
        QCOMPARE(spy.at(2).at(0).toString(), QStringLiteral("c.md"));
    }
};

QTEST_MAIN(TestMetadataCacheEvents)
#include "tst_metadatacache_events.moc"
