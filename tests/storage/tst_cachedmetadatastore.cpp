// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 6 unit tests for CachedMetadataStore: SQLite persistence for
// MetadataCache state. Covers schema migration, round-trip, dedup ref-count
// preservation, frontmatterPos/frontmatterPosition rename, and coexistence
// with SQLiteIndex's tables.

#include <QTest>
#include <QSignalSpy>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/CachedMetadataStore.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"

using namespace Corbomite;

namespace {

QString makeDbPath(const QTemporaryDir &dir)
{
    return QDir(dir.path()).filePath(QStringLiteral("metadata-cache.db"));
}

// Populate cache with a single file and wait for indexFinished.
void populateOne(MetadataCache &cache, const QString &path, const QByteArray &body)
{
    QSignalSpy spy(&cache, &MetadataCache::indexFinished);
    cache.onFileChanged(path, body, 1234567);
    QVERIFY2(spy.wait(5000), "indexFinished did not fire");
}

} // namespace

class TestCachedMetadataStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testOpenCreatesSchema()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = makeDbPath(dir);

        {
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.isOpen());
            store.close();
        }

        // Inspect the DB directly with a one-off connection.
        const QString probeName = QStringLiteral("probe.") +
            QUuid::createUuid().toString();
        {
            QSqlDatabase db =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), probeName);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());

            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("PRAGMA user_version")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 2);

            QStringList tables;
            QVERIFY(q.exec(QStringLiteral(
                "SELECT name FROM sqlite_master WHERE type='table'")));
            while (q.next()) {
                tables.append(q.value(0).toString());
            }
            QVERIFY(tables.contains(QStringLiteral("file_cache")));
            QVERIFY(tables.contains(QStringLiteral("metadata_cache")));

            db.close();
        }
        QSqlDatabase::removeDatabase(probeName);
    }

    void testRoundTripSingleFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = makeDbPath(dir);

        LinkResolver resolver;
        resolver.setVaultPaths({QStringLiteral("a.md")});

        {
            MetadataCache cache(resolver);
            populateOne(cache, QStringLiteral("a.md"),
                        QByteArrayLiteral("# A heading\n"));

            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.persistFrom(cache));
            store.close();
        }

        {
            MetadataCache fresh(resolver);
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.loadInto(fresh));
            store.close();

            QCOMPARE(fresh.fileCount(), 1);
            const auto maybe = fresh.getFileCache(QStringLiteral("a.md"));
            QVERIFY(maybe.has_value());
            QVERIFY(maybe->headings.has_value());
            QCOMPARE(maybe->headings->at(0).heading,
                     QStringLiteral("A heading"));
        }
    }

    void testRoundTripMultipleFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = makeDbPath(dir);

        LinkResolver resolver;
        QStringList paths;
        for (int i = 0; i < 5; ++i) {
            paths.append(QStringLiteral("f_%1.md").arg(i));
        }
        resolver.setVaultPaths(paths);

        {
            MetadataCache cache(resolver);
            QSignalSpy spy(&cache, &MetadataCache::indexFinished);
            for (int i = 0; i < 5; ++i) {
                const QByteArray body =
                    QByteArrayLiteral("# File ") + QByteArray::number(i) + "\n";
                cache.onFileChanged(paths[i], body,
                                    1000000 + static_cast<qint64>(i));
            }
            QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 10000);
            QCOMPARE(cache.fileCount(), 5);

            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.persistFrom(cache));
            store.close();
        }

        {
            MetadataCache fresh(resolver);
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.loadInto(fresh));
            store.close();

            QCOMPARE(fresh.fileCount(), 5);
            for (int i = 0; i < 5; ++i) {
                const auto maybe = fresh.getFileCache(paths[i]);
                QVERIFY(maybe.has_value());
                QVERIFY(maybe->headings.has_value());
                QCOMPARE(maybe->headings->at(0).heading,
                         QStringLiteral("File %1").arg(i));
            }
        }
    }

    void testDedupPersisted()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = makeDbPath(dir);

        LinkResolver resolver;
        resolver.setVaultPaths({QStringLiteral("a.md"), QStringLiteral("b.md")});

        {
            MetadataCache cache(resolver);
            QSignalSpy spy(&cache, &MetadataCache::indexFinished);
            const QByteArray body = QByteArrayLiteral("# Template\n");
            cache.onFileChanged(QStringLiteral("a.md"), body, 111);
            cache.onFileChanged(QStringLiteral("b.md"), body, 222);
            QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 10000);

            QCOMPARE(cache.fileCount(), 2);
            QCOMPARE(cache.uniqueHashCount(), 1);

            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.persistFrom(cache));
            store.close();
        }

        // Inspect metadata_cache directly: one row with ref_count == 2.
        const QString probe = QStringLiteral("probe.dedup.") +
            QUuid::createUuid().toString();
        {
            QSqlDatabase db =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), probe);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral(
                "SELECT hash, ref_count FROM metadata_cache")));
            int rowCount = 0;
            int refCount = 0;
            while (q.next()) {
                ++rowCount;
                refCount = q.value(1).toInt();
            }
            QCOMPARE(rowCount, 1);
            QCOMPARE(refCount, 2);
            db.close();
        }
        QSqlDatabase::removeDatabase(probe);

        // Reload -> dedup survives.
        {
            MetadataCache fresh(resolver);
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.loadInto(fresh));
            store.close();
            QCOMPARE(fresh.fileCount(), 2);
            QCOMPARE(fresh.uniqueHashCount(), 1);
        }
    }

    void testFrontmatterPosRenamedOnDisk()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = makeDbPath(dir);

        LinkResolver resolver;
        resolver.setVaultPaths({QStringLiteral("fm.md")});

        {
            MetadataCache cache(resolver);
            const QByteArray body =
                QByteArrayLiteral("---\ntitle: Hello\n---\n# Body\n");
            populateOne(cache, QStringLiteral("fm.md"), body);

            // Sanity: in-memory state has frontmatterPosition populated.
            const auto maybe = cache.getFileCache(QStringLiteral("fm.md"));
            QVERIFY(maybe.has_value());
            QVERIFY2(maybe->frontmatterPosition.has_value(),
                     "in-memory frontmatterPosition should be populated for a "
                     "file with frontmatter");

            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.persistFrom(cache));
            store.close();
        }

        // Inspect metadata_cache.json_blob directly.
        const QString probe = QStringLiteral("probe.fm.") +
            QUuid::createUuid().toString();
        {
            QSqlDatabase db =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), probe);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral(
                "SELECT json_blob FROM metadata_cache")));
            QVERIFY(q.next());
            const QString blob = q.value(0).toString();
            db.close();

            const QJsonDocument doc = QJsonDocument::fromJson(blob.toUtf8());
            QVERIFY(doc.isObject());
            const QJsonObject obj = doc.object();
            QVERIFY2(obj.contains(QStringLiteral("frontmatterPos")),
                     "persisted blob should use on-disk key frontmatterPos");
            QVERIFY2(!obj.contains(QStringLiteral("frontmatterPosition")),
                     "persisted blob must not carry the in-memory key");
        }
        QSqlDatabase::removeDatabase(probe);

        // Reload -> in-memory state has frontmatterPosition populated again.
        {
            MetadataCache fresh(resolver);
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.loadInto(fresh));
            store.close();
            const auto maybe = fresh.getFileCache(QStringLiteral("fm.md"));
            QVERIFY(maybe.has_value());
            QVERIFY(maybe->frontmatterPosition.has_value());
        }
    }

    void testMigrationFromV1DropsTables()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = makeDbPath(dir);

        // Pre-seed: user_version=1, file_cache/metadata_cache with rows.
        const QString seed = QStringLiteral("seed.mig.") +
            QUuid::createUuid().toString();
        {
            QSqlDatabase db =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), seed);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("PRAGMA user_version = 1")));
            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE file_cache (path TEXT PRIMARY KEY, "
                "mtime_ms INTEGER, size INTEGER, hash TEXT)")));
            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE metadata_cache (hash TEXT PRIMARY KEY, "
                "json_blob TEXT, ref_count INTEGER)")));
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO file_cache VALUES ('x.md', 1, 1, 'abc')")));
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO metadata_cache VALUES ('abc', '{}', 1)")));
            db.close();
        }
        QSqlDatabase::removeDatabase(seed);

        // Open via CachedMetadataStore -> migrates.
        {
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            store.close();
        }

        // Verify: user_version == 2, both tables empty.
        const QString probe = QStringLiteral("probe.mig.") +
            QUuid::createUuid().toString();
        {
            QSqlDatabase db =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), probe);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("PRAGMA user_version")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 2);

            QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM file_cache")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 0);

            QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM metadata_cache")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 0);

            db.close();
        }
        QSqlDatabase::removeDatabase(probe);
    }

    void testMigrationFromV1LeavesSQLiteIndexTablesAlone()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = makeDbPath(dir);

        // Pre-seed: v1 cache tables + SQLiteIndex tables (links, note_tags)
        // with rows.
        const QString seed = QStringLiteral("seed.coexist.") +
            QUuid::createUuid().toString();
        {
            QSqlDatabase db =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), seed);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("PRAGMA user_version = 1")));
            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE file_cache (path TEXT PRIMARY KEY)")));
            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE metadata_cache (hash TEXT PRIMARY KEY)")));
            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE links (source_path TEXT, target_path TEXT)")));
            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE note_tags (note_path TEXT, tag TEXT)")));
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO links VALUES ('a.md', 'b.md')")));
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO note_tags VALUES ('a.md', 'foo')")));
            db.close();
        }
        QSqlDatabase::removeDatabase(seed);

        {
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            store.close();
        }

        const QString probe = QStringLiteral("probe.coexist.") +
            QUuid::createUuid().toString();
        {
            QSqlDatabase db =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), probe);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM links")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 1);
            QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM note_tags")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 1);
            db.close();
        }
        QSqlDatabase::removeDatabase(probe);
    }

    void testFreshDBIsEmptyCacheAfterLoad()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = makeDbPath(dir);

        LinkResolver resolver;
        MetadataCache fresh(resolver);

        CachedMetadataStore store;
        QVERIFY(store.open(dbPath));
        QVERIFY(store.loadInto(fresh));
        store.close();
        QCOMPARE(fresh.fileCount(), 0);
    }

    void testPersistEmptyCache()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = makeDbPath(dir);

        LinkResolver resolver;
        {
            MetadataCache cache(resolver);
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.persistFrom(cache));
            store.close();
        }

        const QString probe = QStringLiteral("probe.empty.") +
            QUuid::createUuid().toString();
        {
            QSqlDatabase db =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), probe);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM file_cache")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 0);
            db.close();
        }
        QSqlDatabase::removeDatabase(probe);

        {
            MetadataCache fresh(resolver);
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.loadInto(fresh));
            store.close();
            QCOMPARE(fresh.fileCount(), 0);
        }
    }

    void testCloseThenReopenIsIdempotent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = makeDbPath(dir);

        LinkResolver resolver;
        resolver.setVaultPaths({QStringLiteral("a.md")});

        // Round 1: populate + persist + close.
        {
            MetadataCache cache(resolver);
            populateOne(cache, QStringLiteral("a.md"),
                        QByteArrayLiteral("# Round 1\n"));
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.persistFrom(cache));
            store.close();
        }

        // Round 2: open, load, persist (no-op change), close.
        {
            MetadataCache cache(resolver);
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.loadInto(cache));
            QCOMPARE(cache.fileCount(), 1);
            QVERIFY(store.persistFrom(cache));
            store.close();
        }

        // Round 3: confirm state preserved.
        {
            MetadataCache cache(resolver);
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.loadInto(cache));
            store.close();
            QCOMPARE(cache.fileCount(), 1);
            const auto maybe = cache.getFileCache(QStringLiteral("a.md"));
            QVERIFY(maybe.has_value());
            QVERIFY(maybe->headings.has_value());
            QCOMPARE(maybe->headings->at(0).heading,
                     QStringLiteral("Round 1"));
        }
    }

    void testUnsupportedFilePersisted()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = makeDbPath(dir);

        LinkResolver resolver;
        {
            MetadataCache cache(resolver);
            cache.onUnsupportedFile(QStringLiteral("image.png"), 42, 1024);
            QCOMPARE(cache.fileCount(), 1);

            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.persistFrom(cache));
            store.close();
        }

        const QString probe = QStringLiteral("probe.unsup.") +
            QUuid::createUuid().toString();
        {
            QSqlDatabase db =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), probe);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral(
                "SELECT path, hash FROM file_cache WHERE path = 'image.png'")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toString(), QStringLiteral("image.png"));
            QCOMPARE(q.value(1).toString(), QString());
            db.close();
        }
        QSqlDatabase::removeDatabase(probe);

        {
            MetadataCache fresh(resolver);
            CachedMetadataStore store;
            QVERIFY(store.open(dbPath));
            QVERIFY(store.loadInto(fresh));
            store.close();
            const auto maybe = fresh.getFileCache(QStringLiteral("image.png"));
            QVERIFY2(maybe.has_value(),
                     "tracked-unsupported file should round-trip as a "
                     "value-containing optional");
            // Empty CachedMetadata => all optionals empty.
            QVERIFY(!maybe->headings.has_value());
            QVERIFY(!maybe->links.has_value());
        }
    }
};

QTEST_MAIN(TestCachedMetadataStore)
#include "tst_cachedmetadatastore.moc"
