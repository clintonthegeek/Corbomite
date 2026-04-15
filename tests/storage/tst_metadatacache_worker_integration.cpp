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
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>

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

    // 3. Persistence flushes immediately on indexFinished. Populate a vault
    //    of 5 files via rebuildVault against a MetadataCache with an open
    //    store; assert both tables have 5 rows right after indexFinished
    //    fires (no 30s wait for the debounce timer).
    void testPersistOnIndexFinished()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QStringList rel = writeDistinctFiles(dir.path(), 5);
        QCOMPARE(rel.size(), 5);

        const QString dbPath =
            QDir(dir.path()).filePath(QStringLiteral("metadata-cache.db"));

        LinkResolver resolver;
        resolver.setVaultPaths(rel);

        MetadataCache cache(resolver);
        QVERIFY(cache.open(dbPath));

        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);
        cache.rebuildVault(dir.path(), rel);
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);

        // Inspect the DB directly -- must contain 5 rows in each table
        // *now*, not 30s from now.
        const QString probe = QStringLiteral("probe.pif.") +
            QUuid::createUuid().toString();
        {
            QSqlDatabase db =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), probe);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());
            QSqlQuery q(db);

            QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM file_cache")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 5);

            QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM metadata_cache")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 5);

            db.close();
        }
        QSqlDatabase::removeDatabase(probe);

        cache.close();
    }
};

QTEST_MAIN(TestMetadataCacheWorkerIntegration)
#include "tst_metadatacache_worker_integration.moc"
