// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"

using namespace Corbomite;

namespace {

void pumpUntilDrain()
{
    for (int i = 0; i < 40; ++i) {
        QCoreApplication::processEvents();
        QTest::qWait(1);
    }
}

void feed(MetadataCache &cache, const QString &path, const QByteArray &body)
{
    cache.onFileChanged(path, body, 1);
    pumpUntilDrain();
}

} // namespace

class TestProxyMetadata : public QObject
{
    Q_OBJECT
private slots:
    void readerNullPassthroughReturnsEmpty();
    void outlinksFromFileCacheLinks();
    void tagsInFileCacheTags();
    void allTagsUnionsAcrossFiles();
    void backlinksMatchLinkedSourcePaths();
};

void TestProxyMetadata::readerNullPassthroughReturnsEmpty()
{
    MetadataCacheReader reader(nullptr);
    QCOMPARE(reader.outlinksFor(QStringLiteral("x.md")), QStringList{});
    QCOMPARE(reader.tagsIn(QStringLiteral("x.md")), QStringList{});
    QCOMPARE(reader.allTags(), QStringList{});
    QCOMPARE(reader.backlinksFor(QStringLiteral("x.md")), QStringList{});
}

void TestProxyMetadata::outlinksFromFileCacheLinks()
{
    LinkResolver resolver;
    resolver.setVaultPaths({QStringLiteral("a.md"), QStringLiteral("b.md")});
    MetadataCache cache(resolver);

    feed(cache, QStringLiteral("a.md"),
         QByteArrayLiteral("See [[b]] and [[c]].\n"));

    MetadataCacheReader reader(&cache);
    const QStringList links = reader.outlinksFor(QStringLiteral("a.md"));
    // LinkResolver rewrites resolved links to full vault paths; unresolved
    // ones pass through unchanged. See tst_metadataparser.cpp tests 3 + 17.
    QVERIFY(links.contains(QStringLiteral("b.md")));
    QVERIFY(links.contains(QStringLiteral("c")));
    QCOMPARE(links.size(), 2);
}

void TestProxyMetadata::tagsInFileCacheTags()
{
    LinkResolver resolver;
    resolver.setVaultPaths({QStringLiteral("a.md")});
    MetadataCache cache(resolver);

    feed(cache, QStringLiteral("a.md"),
         QByteArrayLiteral("#alpha #beta\n"));

    MetadataCacheReader reader(&cache);
    const QStringList tags = reader.tagsIn(QStringLiteral("a.md"));
    QVERIFY(tags.contains(QStringLiteral("alpha")));
    QVERIFY(tags.contains(QStringLiteral("beta")));
}

void TestProxyMetadata::allTagsUnionsAcrossFiles()
{
    LinkResolver resolver;
    resolver.setVaultPaths({QStringLiteral("a.md"), QStringLiteral("b.md")});
    MetadataCache cache(resolver);

    feed(cache, QStringLiteral("a.md"), QByteArrayLiteral("#alpha\n"));
    feed(cache, QStringLiteral("b.md"), QByteArrayLiteral("#beta #alpha\n"));

    MetadataCacheReader reader(&cache);
    const QStringList tags = reader.allTags();
    QVERIFY(tags.contains(QStringLiteral("alpha")));
    QVERIFY(tags.contains(QStringLiteral("beta")));
    QCOMPARE(tags.count(QStringLiteral("alpha")), 1); // deduped
}

void TestProxyMetadata::backlinksMatchLinkedSourcePaths()
{
    LinkResolver resolver;
    resolver.setVaultPaths(
        {QStringLiteral("a.md"), QStringLiteral("b.md"), QStringLiteral("c.md")});
    MetadataCache cache(resolver);

    feed(cache, QStringLiteral("a.md"), QByteArrayLiteral("[[target]]\n"));
    feed(cache, QStringLiteral("b.md"), QByteArrayLiteral("[[target|alias]]\n"));
    feed(cache, QStringLiteral("c.md"), QByteArrayLiteral("[[other]]\n"));

    MetadataCacheReader reader(&cache);
    const QStringList sources = reader.backlinksFor(QStringLiteral("target"));
    QVERIFY(sources.contains(QStringLiteral("a.md")));
    QVERIFY(sources.contains(QStringLiteral("b.md")));
    QVERIFY(!sources.contains(QStringLiteral("c.md")));
}

QTEST_MAIN(TestProxyMetadata)
#include "tst_proxy_metadata.moc"
