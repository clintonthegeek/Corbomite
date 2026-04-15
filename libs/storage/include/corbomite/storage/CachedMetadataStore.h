// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/storage/CachedMetadata.h"

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <memory>

namespace Corbomite {

class MetadataCache;

/// SQLite-backed persistence for MetadataCache state. Stores two tables
/// (file_cache -- path keyed; metadata_cache -- hash keyed with ref-counts).
/// Destructive migration on schema version mismatch (user_version = 2).
///
/// Not thread-safe; call only from the main thread. Uses a separate
/// QSqlDatabase connection per instance (connectionName derived from dbPath).
class CachedMetadataStore {
public:
    CachedMetadataStore();
    ~CachedMetadataStore();

    CachedMetadataStore(const CachedMetadataStore &) = delete;
    CachedMetadataStore &operator=(const CachedMetadataStore &) = delete;

    /// Opens or creates the DB at `dbPath`. On schema-version mismatch, drops
    /// the two tables and recreates (cache-only data; no back-compat contract).
    /// Returns true on success.
    bool open(const QString &dbPath);

    /// Closes the DB connection. Safe to call on never-opened instance (no-op).
    void close();

    /// Reads all rows from file_cache + metadata_cache and populates `cache`
    /// via its internal mutation API. If the schema is empty (fresh DB), leaves
    /// `cache` untouched. Fails (returns false) only on DB corruption.
    bool loadInto(MetadataCache &cache);

    /// Writes the current state of `cache` to file_cache + metadata_cache
    /// in a single transaction. Existing rows are cleared and replaced.
    /// Returns true on success.
    bool persistFrom(const MetadataCache &cache);

    /// True if `open` was called and succeeded and `close` has not been called.
    bool isOpen() const;

    /// The schema version we write (bump + migration path when we change it).
    /// Always returns 2 in this cluster.
    static int currentSchemaVersion();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

}  // namespace Corbomite
