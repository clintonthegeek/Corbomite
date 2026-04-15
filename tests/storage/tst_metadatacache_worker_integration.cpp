// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 5 integration tests: MetadataCache routed through MetadataWorker.
// Covers bulk rebuildVault() + hash-dedup on identical template content.

#include <QTest>
#include <QSignalSpy>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"

using namespace Corbomite;

namespace {

QStringList writeDistinctFiles(const QString &root, int n)
{
    QStringList rel;
    rel.reserve(n);
    for (int i = 0; i < n; ++i) {
        const QString name = QStringLiteral("file_%1.md").arg(i);
        QFile f(QDir(root).filePath(name));
        if (!f.open(QIODevice::WriteOnly)) {
            return {};
        }
        const QByteArray body =
            QByteArrayLiteral("# File ") + QByteArray::number(i) + "\n";
        f.write(body);
        f.close();
        rel.append(name);
    }
    return rel;
}

QStringList writeIdenticalFiles(const QString &root, int n, const QByteArray &body)
{
    QStringList rel;
    rel.reserve(n);
    for (int i = 0; i < n; ++i) {
        const QString name = QStringLiteral("tpl_%1.md").arg(i);
        QFile f(QDir(root).filePath(name));
        if (!f.open(QIODevice::WriteOnly)) {
            return {};
        }
        f.write(body);
        f.close();
        rel.append(name);
    }
    return rel;
}

} // namespace

class TestMetadataCacheWorkerIntegration : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // 1. rebuildVault over 50 distinct files -> fileCount == 50, indexFinished
    //    fires exactly once, spot-check metadata for one entry.
    void testFullVaultRebuild()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QStringList rel = writeDistinctFiles(dir.path(), 50);
        QCOMPARE(rel.size(), 50);

        LinkResolver resolver;
        resolver.setVaultPaths(rel);

        MetadataCache cache(resolver);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        cache.rebuildVault(dir.path(), rel);

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);

        QCOMPARE(cache.fileCount(), 50);

        // Spot-check one entry's metadata.
        const auto maybe = cache.getFileCache(QStringLiteral("file_7.md"));
        QVERIFY(maybe.has_value());
        const CachedMetadata cm = *maybe;
        QVERIFY(cm.headings.has_value());
        QCOMPARE(cm.headings->size(), 1);
        QCOMPARE(cm.headings->at(0).heading, QStringLiteral("File 7"));
    }

    // 2. 10 files with identical content -> fileCount == 10,
    //    uniqueHashCount == 1 (dedup).
    void testDedupOnIdenticalTemplate()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QByteArray body = QByteArrayLiteral("# Template body\n");
        const QStringList rel = writeIdenticalFiles(dir.path(), 10, body);
        QCOMPARE(rel.size(), 10);

        LinkResolver resolver;
        resolver.setVaultPaths(rel);

        MetadataCache cache(resolver);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        cache.rebuildVault(dir.path(), rel);

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);

        QCOMPARE(cache.fileCount(), 10);
        QCOMPARE(cache.uniqueHashCount(), 1);
    }
};

QTEST_MAIN(TestMetadataCacheWorkerIntegration)
#include "tst_metadatacache_worker_integration.moc"
