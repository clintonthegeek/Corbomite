// SPDX-License-Identifier: GPL-3.0-or-later
//
// CachedMetadataStore — SQLite-backed persistence for MetadataCache state.
//
// Two tables under a bumped schema (PRAGMA user_version = 2):
//   file_cache      (path TEXT PK, mtime_ms INTEGER, size INTEGER, hash TEXT)
//   metadata_cache  (hash TEXT PK, json_blob TEXT, ref_count INTEGER)
//
// Destructive migration from any prior version: drops + recreates own tables
// only. Unsupported-file rows (hash == "") live in file_cache with no
// corresponding metadata_cache row.
//
// Per-instance connection name (derived from dbPath) lets multiple stores
// target the same DB file from different tests without collisions.

#include "corbomite/storage/CachedMetadataStore.h"

#include "corbomite/storage/MetadataCache.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

namespace Corbomite {

struct CachedMetadataStore::Impl {
    QString connectionName;
    QString dbPath;
    bool open = false;
};

CachedMetadataStore::CachedMetadataStore()
    : d(std::make_unique<Impl>())
{
}

CachedMetadataStore::~CachedMetadataStore()
{
    close();
}

int CachedMetadataStore::currentSchemaVersion()
{
    return 2;
}

bool CachedMetadataStore::isOpen() const
{
    return d && d->open;
}

bool CachedMetadataStore::open(const QString &dbPath)
{
    if (d->open) {
        close();
    }

    // Unique connection name per instance + DB path. Adding the object
    // pointer guards against the same process opening the same path twice
    // simultaneously (distinct test instances).
    d->connectionName = QStringLiteral("CachedMetadataStore.")
        + dbPath
        + QStringLiteral(".")
        + QString::number(reinterpret_cast<qulonglong>(this), 16);
    d->dbPath = dbPath;

    QSqlDatabase db =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), d->connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        QSqlDatabase::removeDatabase(d->connectionName);
        d->connectionName.clear();
        d->dbPath.clear();
        return false;
    }

    // Read current schema version.
    int schemaVersion = 0;
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral("PRAGMA user_version")) && q.next()) {
            schemaVersion = q.value(0).toInt();
        }
    }

    QSqlQuery q(db);
    if (schemaVersion != currentSchemaVersion()) {
        // Destructive migration: drop our two tables only; leave any
        // co-tenant tables (SQLiteIndex's links / note_tags, FTS5 virtual
        // tables, etc.) alone. Then bump user_version.
        q.exec(QStringLiteral("DROP TABLE IF EXISTS file_cache"));
        q.exec(QStringLiteral("DROP TABLE IF EXISTS metadata_cache"));
        q.exec(QStringLiteral("PRAGMA user_version = %1")
                   .arg(currentSchemaVersion()));
    }

    // Idempotent table creation — safe on fresh DB + post-migration DB.
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS file_cache ("
            "path TEXT PRIMARY KEY, "
            "mtime_ms INTEGER NOT NULL, "
            "size INTEGER NOT NULL, "
            "hash TEXT NOT NULL)"))) {
        db.close();
        QSqlDatabase::removeDatabase(d->connectionName);
        d->connectionName.clear();
        d->dbPath.clear();
        return false;
    }
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS metadata_cache ("
            "hash TEXT PRIMARY KEY, "
            "json_blob TEXT NOT NULL, "
            "ref_count INTEGER NOT NULL)"))) {
        db.close();
        QSqlDatabase::removeDatabase(d->connectionName);
        d->connectionName.clear();
        d->dbPath.clear();
        return false;
    }

    d->open = true;
    return true;
}

void CachedMetadataStore::close()
{
    if (!d || !d->open) {
        return;
    }
    {
        QSqlDatabase db = QSqlDatabase::database(d->connectionName);
        if (db.isValid() && db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(d->connectionName);
    d->connectionName.clear();
    d->dbPath.clear();
    d->open = false;
}

bool CachedMetadataStore::loadInto(MetadataCache &cache)
{
    if (!d->open) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(d->connectionName);

    // Read metadata_cache first so we have the hashMap + refCounts available.
    QHash<QString, CachedMetadata> hashMap;
    QHash<QString, int> hashRefCounts;
    {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral(
                "SELECT hash, json_blob, ref_count FROM metadata_cache"))) {
            return false;
        }
        while (q.next()) {
            const QString hash = q.value(0).toString();
            const QString blob = q.value(1).toString();
            const int refCount = q.value(2).toInt();
            QJsonParseError err;
            const QJsonDocument doc =
                QJsonDocument::fromJson(blob.toUtf8(), &err);
            if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                // Skip corrupt row rather than abort the load.
                continue;
            }
            hashMap.insert(hash, fromPersistedJson(doc.object()));
            hashRefCounts.insert(hash, refCount);
        }
    }

    // Read file_cache. Unsupported-extension rows (hash == "") survive
    // even though they have no metadata_cache entry.
    QHash<QString, FileCacheEntry> pathEntries;
    {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral(
                "SELECT path, mtime_ms, size, hash FROM file_cache"))) {
            return false;
        }
        while (q.next()) {
            FileCacheEntry entry;
            const QString path = q.value(0).toString();
            entry.mtimeMs = q.value(1).toLongLong();
            entry.size = q.value(2).toLongLong();
            entry.hash = q.value(3).toString();

            // Defensive: if the row references a hash that's missing from
            // metadata_cache, skip it rather than install a broken invariant.
            if (!entry.hash.isEmpty() && !hashMap.contains(entry.hash)) {
                continue;
            }
            pathEntries.insert(path, entry);
        }
    }

    cache.installPersistedState(pathEntries, hashMap, hashRefCounts);
    return true;
}

bool CachedMetadataStore::persistFrom(const MetadataCache &cache)
{
    if (!d->open) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(d->connectionName);

    const auto pathEntries = cache.pathToFileEntrySnapshot();
    const auto hashMap = cache.hashToCacheSnapshot();
    const auto refCounts = cache.hashRefCountSnapshot();

    if (!db.transaction()) {
        return false;
    }

    auto rollbackAndFail = [&db]() {
        db.rollback();
        return false;
    };

    {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("DELETE FROM file_cache"))) {
            return rollbackAndFail();
        }
        if (!q.exec(QStringLiteral("DELETE FROM metadata_cache"))) {
            return rollbackAndFail();
        }
    }

    // Prepared INSERT for file_cache.
    {
        QSqlQuery q(db);
        if (!q.prepare(QStringLiteral(
                "INSERT INTO file_cache (path, mtime_ms, size, hash) "
                "VALUES (?, ?, ?, ?)"))) {
            return rollbackAndFail();
        }
        for (auto it = pathEntries.constBegin(); it != pathEntries.constEnd(); ++it) {
            // Normalise null QString -> empty literal so NOT NULL holds.
            // Unsupported-file entries carry an empty hash (QString{}) which
            // otherwise binds as SQL NULL and trips the schema constraint.
            const QString hash =
                it.value().hash.isNull() ? QStringLiteral("") : it.value().hash;
            q.bindValue(0, it.key());
            q.bindValue(1, it.value().mtimeMs);
            q.bindValue(2, it.value().size);
            q.bindValue(3, hash);
            if (!q.exec()) {
                return rollbackAndFail();
            }
        }
    }

    // Prepared INSERT for metadata_cache.
    {
        QSqlQuery q(db);
        if (!q.prepare(QStringLiteral(
                "INSERT INTO metadata_cache (hash, json_blob, ref_count) "
                "VALUES (?, ?, ?)"))) {
            return rollbackAndFail();
        }
        for (auto it = hashMap.constBegin(); it != hashMap.constEnd(); ++it) {
            const QJsonObject obj = toPersistedJson(it.value());
            const QByteArray blob =
                QJsonDocument(obj).toJson(QJsonDocument::Compact);
            const int refCount = refCounts.value(it.key(), 0);
            q.bindValue(0, it.key());
            q.bindValue(1, QString::fromUtf8(blob));
            q.bindValue(2, refCount);
            if (!q.exec()) {
                return rollbackAndFail();
            }
        }
    }

    if (!db.commit()) {
        return rollbackAndFail();
    }
    return true;
}

}  // namespace Corbomite
