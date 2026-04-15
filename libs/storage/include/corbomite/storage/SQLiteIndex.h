// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/LinkResolver.h"

namespace Corbomite {

class MetadataCache;

struct SearchMatch {
    QString notePath;
    QString snippet;
    double score = 0.0;
    // Highlight spans over `snippet`, expressed as merge-sorted, non-overlapping
    // [start, end) UTF-16 code-unit ranges. Empty in Phase 1 — populated in Phase 2
    // when FuzzyMatcher takes over ranking.
    QVector<QPair<int, int>> matches;
};

struct LinkInfo {
    QString sourcePath;
    QString targetPath;
    QString linkType;       // "wiki", "markdown", "embed"
    QString displayText;    // alias, if any
    QString subpath;        // "#heading" or "#^block", empty if none
};

/// SQLite-backed FTS5 + links + tags index.
///
/// Cluster I phase 7+8: this class no longer parses markdown. Instead it
/// subscribes to `MetadataCache::cacheChanged` / `cacheDeleted` and
/// derives its FTS / links / tags rows from the already-parsed
/// `CachedMetadata`. Wire it up with:
///
///     index.open(dbPath);
///     index.setVaultRoot(vaultRoot);
///     index.setMetadataCache(cache);
///
/// All writes are driven by `MetadataCache` events. Consumers that need to
/// trigger a rebuild call `MetadataCache::rebuildVault(...)`, not any method
/// on this class. The former write API (`rebuildIndex`, `rebuildIndexAsync`,
/// `indexNote`, `removeNote`, `isRebuilding`) and the legacy `indexReady`
/// signal were removed in Phase 8.
class SQLiteIndex : public QObject {
    Q_OBJECT

public:
    explicit SQLiteIndex(QObject *parent = nullptr);
    ~SQLiteIndex() override;

    bool open(const QString &dbPath);
    void close();

    /// Set the vault root used when the cache-changed slot reads raw file
    /// body to populate the FTS `content` column. Must be set before
    /// MetadataCache starts firing `cacheChanged`.
    void setVaultRoot(const QString &vaultRoot);

    /// Wire this index to a MetadataCache. Subscribes to `cacheChanged`
    /// and `cacheDeleted`. Replaces any previous subscription. Passing
    /// `nullptr` disconnects.
    void setMetadataCache(MetadataCache *cache);

    // --- Read API (UNCHANGED — consumers rely on these) ---

    // Full-text search
    QVector<SearchMatch> search(const QString &query, int maxResults = 100) const;

    // Compiled DSL search — accepts an FTS5 fragment plus tag include/exclude
    // lists from libs/search's SearchDSL::compile(). The FTS5 fragment runs
    // over the same notes_fts(path,title,content) virtual table; the tag
    // filters intersect/except via the note_tags side-table. An empty
    // fts5Query with non-empty tag filters is also a valid plan (returns all
    // notes matching the tag predicates).
    QVector<SearchMatch> searchCompiled(const QString &fts5Query,
                                        const QStringList &requiredTags,
                                        const QStringList &excludedTags,
                                        int maxResults = 100) const;

    // Link queries
    QVector<LinkInfo> backlinksFor(const QString &targetPath) const;
    QVector<LinkInfo> outlinksFor(const QString &sourcePath) const;
    QVector<QString> orphanLinks() const;
    QVector<LinkInfo> allLinks() const;

    // Tag queries
    QStringList allTags() const;
    QStringList notesWithTag(const QString &tag) const;

    // Link repair
    int repairLinks(const QString &oldTargetPath, const QString &newTargetPath,
                    const QString &vaultRoot);

private Q_SLOTS:
    void onMetadataCacheChanged(const QString &path,
                                const QString &prevHash,
                                const Corbomite::CachedMetadata &cache);
    void onMetadataCacheDeleted(const QString &path,
                                const Corbomite::CachedMetadata &prevCache);

private:
    void createTables();

    /// Derive `notes_fts` / `links` / `note_tags` rows from a parsed
    /// `CachedMetadata` and write them atomically. Deletes any previous
    /// rows for `path` first (idempotent update).
    void writeRowsFromCache(const QString &path, const Corbomite::CachedMetadata &cache);

    /// Delete all SQLiteIndex-owned rows for `path`. Called from both
    /// the cache-changed slot (idempotency on update) and the
    /// cache-deleted slot.
    void deleteRowsForPath(const QString &path);

    QString m_connectionName;
    QString m_dbPath;
    QString m_vaultRoot;
    LinkResolver m_resolver;
    bool m_isOpen = false;
    QPointer<MetadataCache> m_cache;
};

} // namespace Corbomite
