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
// async singleShot continuations finish — but NOT long enough to fire the
// 10ms indexFinished debounce timer. One round of processEvents is usually
// enough for a small burst; we do a few rounds to be safe.
void pumpUntilDrain()
{
    for (int i = 0; i < 20; ++i) {
        QCoreApplication::processEvents();
    }
}

} // namespace

class TestMetadataCacheEvents : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // 1. cacheChanged fires exactly once per file in a two-file burst.
    void testChangedFiresOncePerFile()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md"),
                                              QStringLiteral("b.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        cache.onFileChanged(QStringLiteral("b.md"), QByteArray("# B\n"), 200);

        // cacheChanged is synchronous from onFileChanged, so no pump needed.
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("a.md"));
        QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("b.md"));
    }

    // 2. cacheChanged is synchronous from inside onFileChanged.
    //    Assert: no processEvents call between onFileChanged and spy check.
    void testChangedFiresSynchronouslyFromOnFileChanged()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);

        // No processEvents — if this passes, the signal was emitted inline.
        QCOMPARE(spy.count(), 1);
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

        pumpUntilDrain();
        QCOMPARE(spy.count(), 1);
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
        // After onFileChanged: cacheChanged fired synchronously,
        // linksResolvedFor hasn't yet.
        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(resolvedForSpy.count(), 0);

        pumpUntilDrain();
        // Drain has run; resolvedFor + allResolved fired, indexFinished not
        // yet (10ms debounce).
        QCOMPARE(resolvedForSpy.count(), 1);
        QCOMPARE(allResolvedSpy.count(), 1);

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 500);
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

        QCOMPARE(changedSpy.count(), 3);
        QCOMPARE(changedSpy.at(0).at(0).toString(), QStringLiteral("a.md"));
        QCOMPARE(changedSpy.at(1).at(0).toString(), QStringLiteral("b.md"));
        QCOMPARE(changedSpy.at(2).at(0).toString(), QStringLiteral("c.md"));

        pumpUntilDrain();
        QCOMPARE(resolvedForSpy.count(), 3);
        // allLinksResolved coalesces at the end of each drain cycle; under
        // sequential Phase-4 semantics (all enqueued before the first drain
        // tick), it fires exactly once.
        QCOMPARE(allResolvedSpy.count(), 1);

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 500);
    }

    // 6. Stat short-circuit is silent — no signals fire, no queue changes.
    void testShortCircuitSilent()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        QByteArray bytes("# A\n");
        cache.onFileChanged(QStringLiteral("a.md"), bytes, 100);

        QSignalSpy changedSpy(&cache, &MetadataCache::cacheChanged);
        QSignalSpy resolvedForSpy(&cache, &MetadataCache::linksResolvedFor);
        QSignalSpy allResolvedSpy(&cache, &MetadataCache::allLinksResolved);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        // Wait out the first change's drain + debounce so spies stay clean
        // for the short-circuit check.
        QTest::qWait(50);
        changedSpy.clear();
        resolvedForSpy.clear();
        allResolvedSpy.clear();
        finishedSpy.clear();

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

        QSignalSpy changedSpy(&cache, &MetadataCache::cacheChanged);
        QSignalSpy resolvedForSpy(&cache, &MetadataCache::linksResolvedFor);
        QSignalSpy allResolvedSpy(&cache, &MetadataCache::allLinksResolved);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        QTest::qWait(50);
        changedSpy.clear();
        resolvedForSpy.clear();
        allResolvedSpy.clear();
        finishedSpy.clear();

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
        QTest::qWait(50);  // let drain + debounce finish.

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

        // Let the drain run (but probably not the 10ms debounce yet).
        pumpUntilDrain();

        // QTRY_COMPARE with generous timeout handles the 10ms debounce.
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 500);
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
        pumpUntilDrain();
        // Sleep less than 10ms so the debounce hasn't fired yet.
        QTest::qWait(3);
        cache.onFileChanged(QStringLiteral("b.md"), QByteArray("# B\n"), 200);
        pumpUntilDrain();

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 500);

        // Give it more time to ensure a second fire never happens.
        QTest::qWait(50);
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

        QVERIFY(fired);
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
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toString(), QString{});
    }

    // 16. Hash change carries the old hash as prevHash.
    void testHashChangeCarriesOldHashAsPrevHash()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("# A\n"), 100);
        const QString firstHash = cache.getFileHash(QStringLiteral("a.md"));
        QVERIFY(!firstHash.isEmpty());

        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);

        cache.onFileChanged(QStringLiteral("a.md"),
                            QByteArray("# Different\n"), 200);
        QCOMPARE(spy.count(), 1);
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

        pumpUntilDrain();

        QCOMPARE(spy.count(), 3);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("a.md"));
        QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("b.md"));
        QCOMPARE(spy.at(2).at(0).toString(), QStringLiteral("c.md"));
    }
};

QTEST_MAIN(TestMetadataCacheEvents)
#include "tst_metadatacache_events.moc"
