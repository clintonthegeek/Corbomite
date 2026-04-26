// SPDX-License-Identifier: GPL-3.0-or-later
#include <QSignalSpy>
#include <QTest>

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>

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

// Phase 5: parsing is async via a worker thread, so tests have to pump the
// event loop and wait for the queued parsed() signal to bump fileCount().
// This helper blocks for up to `timeoutMs` waiting for fileCount to reach
// `expected`.
void waitForFileCount(MetadataCache &cache, int expected, int timeoutMs = 2000)
{
    QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), expected, timeoutMs);
}

} // namespace

class TestMetadataCacheCore : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // 1. First-time file -> cache is populated, counts bump, parse produces
    //    the heading we expect.
    void testFirstTimeParseInsertsCache()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("note.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("note.md"),
                            QByteArray("# Hi\n"), 100);
        waitForFileCount(cache, 1);

        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.uniqueHashCount(), 1);

        auto got = cache.getFileCache(QStringLiteral("note.md"));
        QVERIFY(got.has_value());
        QVERIFY(got->headings.has_value());
        QCOMPARE(got->headings->size(), 1);
        QCOMPARE(got->headings->at(0).heading, QStringLiteral("Hi"));
    }

    // 2. Stat unchanged (same mtime+size) -> short-circuit wins even if
    //    content bytes differ. This is Obsidian's speculative optimisation.
    void testStatUnchangedShortCircuits()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        // Construct A: 5-byte md with heading "A".
        QByteArray bytesA("# A\n\n");  // 5 bytes
        // Construct B: 5-byte md with heading "B".
        QByteArray bytesB("# B\n\n");  // 5 bytes
        QCOMPARE(bytesA.size(), bytesB.size());

        cache.onFileChanged(QStringLiteral("a.md"), bytesA, 100);
        waitForFileCount(cache, 1);

        auto gotA = cache.getFileCache(QStringLiteral("a.md"));
        QVERIFY(gotA.has_value());
        QCOMPARE(gotA->headings->at(0).heading, QStringLiteral("A"));

        // Same mtime + same size but different bytes. Short-circuit should win.
        cache.onFileChanged(QStringLiteral("a.md"), bytesB, 100);
        // Short-circuit is synchronous -- no wait needed. Pump briefly anyway
        // to make sure no stray worker result lands.
        QTest::qWait(50);

        auto gotAfter = cache.getFileCache(QStringLiteral("a.md"));
        QVERIFY(gotAfter.has_value());
        // Key assertion: the cache still reflects bytesA -- short-circuit wins.
        QCOMPARE(gotAfter->headings->at(0).heading, QStringLiteral("A"));
        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.uniqueHashCount(), 1);
    }

    // 3. mtime changed but content identical -> hash matches -> skip re-parse,
    //    just update stat. `uniqueHashCount` stays at 1 which is our proxy
    //    for "no re-parse occurred".
    void testMtimeChangedSameHashShortCircuits()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        QByteArray bytes("# Same\n");
        cache.onFileChanged(QStringLiteral("a.md"), bytes, 100);
        waitForFileCount(cache, 1);
        const QString hashBefore = cache.getFileHash(QStringLiteral("a.md"));
        QCOMPARE(cache.uniqueHashCount(), 1);

        // Same bytes, new mtime -> stat differs, hash unchanged. Synchronous
        // short-circuit updates stat without touching the worker.
        cache.onFileChanged(QStringLiteral("a.md"), bytes, 200);
        QTest::qWait(50);

        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.uniqueHashCount(), 1);
        QCOMPARE(cache.getFileHash(QStringLiteral("a.md")), hashBefore);
    }

    // 4. Stat changed AND content changed -> old hash released, new parsed.
    void testHashChangedReparses()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataCache cache(resolver);

        QByteArray bytesA("# AAAA\n");
        QByteArray bytesB("# BBBBBB\n");  // different size -> stat differs
        cache.onFileChanged(QStringLiteral("a.md"), bytesA, 100);
        waitForFileCount(cache, 1);
        const QString hashA = cache.getFileHash(QStringLiteral("a.md"));
        QVERIFY(!hashA.isEmpty());

        cache.onFileChanged(QStringLiteral("a.md"), bytesB, 200);
        QTRY_VERIFY_WITH_TIMEOUT(
            cache.getFileHash(QStringLiteral("a.md")) != hashA, 2000);

        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.uniqueHashCount(), 1);  // old released, new inserted
        const QString hashB = cache.getFileHash(QStringLiteral("a.md"));
        QVERIFY(!hashB.isEmpty());
        QVERIFY(hashA != hashB);

        auto got = cache.getFileCache(QStringLiteral("a.md"));
        QVERIFY(got.has_value());
        QVERIFY(got->headings.has_value());
        QCOMPARE(got->headings->at(0).heading, QStringLiteral("BBBBBB"));
    }

    // 5. Two paths, identical bytes -> single hash entry, two path entries.
    void testContentDedupTwoFiles()
    {
        LinkResolver resolver =
            makeResolver({QStringLiteral("a.md"), QStringLiteral("b.md")});
        MetadataCache cache(resolver);

        QByteArray bytes("# Shared\n");
        cache.onFileChanged(QStringLiteral("a.md"), bytes, 100);
        cache.onFileChanged(QStringLiteral("b.md"), bytes, 200);
        waitForFileCount(cache, 2);

        QCOMPARE(cache.fileCount(), 2);
        QCOMPARE(cache.uniqueHashCount(), 1);
        QCOMPARE(cache.getFileHash(QStringLiteral("a.md")),
                 cache.getFileHash(QStringLiteral("b.md")));
    }

    // 6. Delete one of two paths sharing a hash -> ref-count drops from 2 to
    //    1 but the hash entry survives. Delete the second -> entry is gone.
    void testDedupTeardownOnDelete()
    {
        LinkResolver resolver =
            makeResolver({QStringLiteral("a.md"), QStringLiteral("b.md")});
        MetadataCache cache(resolver);

        QByteArray bytes("# Shared\n");
        cache.onFileChanged(QStringLiteral("a.md"), bytes, 100);
        cache.onFileChanged(QStringLiteral("b.md"), bytes, 200);
        waitForFileCount(cache, 2);
        QCOMPARE(cache.fileCount(), 2);
        QCOMPARE(cache.uniqueHashCount(), 1);

        cache.onFileDeleted(QStringLiteral("a.md"));
        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.uniqueHashCount(), 1);

        cache.onFileDeleted(QStringLiteral("b.md"));
        QCOMPARE(cache.fileCount(), 0);
        QCOMPARE(cache.uniqueHashCount(), 0);
    }

    // 7. getFileCache for unknown path -> std::nullopt.
    void testGetFileCacheNullOptForUnknown()
    {
        LinkResolver resolver;
        MetadataCache cache(resolver);

        auto got = cache.getFileCache(QStringLiteral("nonexistent.md"));
        QVERIFY(!got.has_value());
    }

    // 8. Unsupported file -> tracked but cache is an empty struct.
    void testGetFileCacheEmptyStructForUnsupported()
    {
        LinkResolver resolver;
        MetadataCache cache(resolver);

        cache.onUnsupportedFile(QStringLiteral("img.png"), 100, 2048);

        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.uniqueHashCount(), 0);

        auto got = cache.getFileCache(QStringLiteral("img.png"));
        QVERIFY(got.has_value());
        QVERIFY(!got->links.has_value());
        QVERIFY(!got->embeds.has_value());
        QVERIFY(!got->tags.has_value());
        QVERIFY(!got->headings.has_value());
        QVERIFY(!got->sections.has_value());
        QVERIFY(!got->listItems.has_value());
        QVERIFY(!got->footnoteRefs.has_value());
        QVERIFY(!got->footnotes.has_value());
        QVERIFY(!got->blocks.has_value());
        QVERIFY(!got->frontmatter.has_value());
        QVERIFY(!got->frontmatterLinks.has_value());
        QVERIFY(!got->frontmatterPosition.has_value());
    }

    // 9. Unsupported file registered twice with same stat -> no corruption.
    void testUnsupportedStatShortCircuit()
    {
        LinkResolver resolver;
        MetadataCache cache(resolver);

        cache.onUnsupportedFile(QStringLiteral("img.png"), 100, 2048);
        cache.onUnsupportedFile(QStringLiteral("img.png"), 100, 2048);

        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.uniqueHashCount(), 0);
    }

    // 10. Unsupported -> supported transition.
    void testUnsupportedToSupportedTransition()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("note")});
        MetadataCache cache(resolver);

        cache.onUnsupportedFile(QStringLiteral("note"), 100, 0);
        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.uniqueHashCount(), 0);

        cache.onFileChanged(QStringLiteral("note"),
                            QByteArray("# Body\n"), 200);
        QTRY_COMPARE_WITH_TIMEOUT(cache.uniqueHashCount(), 1, 2000);
        QCOMPARE(cache.fileCount(), 1);

        auto got = cache.getFileCache(QStringLiteral("note"));
        QVERIFY(got.has_value());
        QVERIFY(got->headings.has_value());
        QCOMPARE(got->headings->at(0).heading, QStringLiteral("Body"));
    }

    // 11. Supported -> unsupported transition (e.g. renamed to .png).
    void testSupportedToUnsupportedTransition()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("note.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("note.md"),
                            QByteArray("# Body\n"), 100);
        waitForFileCount(cache, 1);
        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.uniqueHashCount(), 1);

        cache.onUnsupportedFile(QStringLiteral("note.md"), 200, 42);
        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.uniqueHashCount(), 0);  // old hash released

        auto got = cache.getFileCache(QStringLiteral("note.md"));
        QVERIFY(got.has_value());
        // Empty struct -- no fields populated.
        QVERIFY(!got->headings.has_value());
    }

    // 12. Deleting unknown path -> no-op, no crash.
    void testOnFileDeletedUnknownPathIsNoOp()
    {
        LinkResolver resolver;
        MetadataCache cache(resolver);

        cache.onFileDeleted(QStringLiteral("never-heard.md"));
        QCOMPARE(cache.fileCount(), 0);
        QCOMPARE(cache.uniqueHashCount(), 0);
    }

    // 13. allPaths() returns the registered paths.
    void testAllPathsReturnsRegisteredPaths()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md"),
                                              QStringLiteral("b.md"),
                                              QStringLiteral("c.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("a.md"), QByteArray("a"), 1);
        cache.onFileChanged(QStringLiteral("b.md"), QByteArray("b"), 2);
        cache.onFileChanged(QStringLiteral("c.md"), QByteArray("c"), 3);
        waitForFileCount(cache, 3);

        QStringList paths = cache.allPaths();
        QCOMPARE(paths.size(), 3);
        paths.sort();
        QCOMPARE(paths,
                 QStringList({QStringLiteral("a.md"),
                              QStringLiteral("b.md"),
                              QStringLiteral("c.md")}));
    }

    // Regression: drainOnePath only re-resolved cache.links —
    // cache.frontmatterLinks were never resolved at all because
    // collectFrontmatterLinks doesn't run the resolver itself. After
    // indexing, frontmatter wikilinks must hold the resolver-mapped paths
    // so backlinks/search/the graph see consistent state with body links.
    // (Embeds parse via Markoff's "image" path and currently lose their
    // target on `![[…]]` syntax — separate issue, see embed-parser
    // follow-up.)
    void testDrainResolvesFrontmatterLinks()
    {
        LinkResolver resolver = makeResolver({
            QStringLiteral("source.md"),
            QStringLiteral("Target.md"),
        });
        MetadataCache cache(resolver);

        QSignalSpy resolved(&cache, &MetadataCache::linksResolvedFor);

        cache.onFileChanged(QStringLiteral("source.md"),
            QByteArray(
                "---\n"
                "related: \"[[Target]]\"\n"
                "---\n"
                "[[Target]]\n"
            ), 100);
        waitForFileCount(cache, 1);
        QTRY_VERIFY_WITH_TIMEOUT(resolved.count() >= 1, 5000);

        auto got = cache.getFileCache(QStringLiteral("source.md"));
        QVERIFY(got.has_value());

        QVERIFY(got->links.has_value());
        QVERIFY(!got->links->isEmpty());
        QCOMPARE(got->links->at(0).link, QStringLiteral("Target.md"));

        QVERIFY(got->frontmatterLinks.has_value());
        QVERIFY(!got->frontmatterLinks->isEmpty());
        QCOMPARE(got->frontmatterLinks->at(0).link,
                 QStringLiteral("Target.md"));
    }

    // 14. Zero-byte file is still tracked with a hash (the empty-SHA-256).
    void testZeroByteFileIsTracked()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("empty.md")});
        MetadataCache cache(resolver);

        cache.onFileChanged(QStringLiteral("empty.md"), QByteArray{}, 100);
        waitForFileCount(cache, 1);

        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.uniqueHashCount(), 1);
        // Empty-SHA-256 is well-known.
        QCOMPARE(cache.getFileHash(QStringLiteral("empty.md")),
                 QStringLiteral(
                     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    }
};

QTEST_GUILESS_MAIN(TestMetadataCacheCore)
#include "tst_metadatacache_core.moc"
