// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/storage/CachedMetadata.h"

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <optional>

namespace Corbomite {

class LinkResolver;

struct FileCacheEntry {
    qint64 mtimeMs = 0;
    qint64 size = 0;
    QString hash;  // empty string means "unsupported extension / not-parseable"
};

/// Two-layer path-keyed / hash-keyed metadata cache with stat short-circuit and
/// content-hash dedup. Phase 3 is synchronous + signal-less; Phase 4 adds the
/// five Qt signals (cacheChanged / cacheDeleted / linksResolvedFor /
/// allLinksResolved / indexFinished) and the link-resolver queue.
///
/// Thread-safety: not safe for concurrent mutation. All mutation methods must
/// be called from a single thread (Phase 5 introduces a worker that posts
/// results back to this thread via queued connections).
class MetadataCache {
public:
    explicit MetadataCache(const LinkResolver &resolver);
    ~MetadataCache();

    MetadataCache(const MetadataCache &) = delete;
    MetadataCache &operator=(const MetadataCache &) = delete;

    /// Mutation — file changed (created or modified on disk).
    /// Computes SHA-256 only on stat change. Re-parses only on hash change.
    /// Empty `content` for a known extension treated as a zero-byte note.
    void onFileChanged(const QString &path, const QByteArray &content, qint64 mtimeMs);

    /// Mutation — file deleted.
    /// Captures prevCache BEFORE removing the path entry (Phase 4 will expose
    /// it via the cacheDeleted signal; Phase 3 just handles the ref-count
    /// teardown).
    void onFileDeleted(const QString &path);

    /// Register a path as unsupported (non-.md). Entry stored with hash == "".
    /// getFileCache(path) returns an empty CachedMetadata{} for this state
    /// (vs std::nullopt for "not in cache at all").
    void onUnsupportedFile(const QString &path, qint64 mtimeMs, qint64 size);

    /// Read API.
    std::optional<CachedMetadata> getFileCache(const QString &path) const;
    QString getFileHash(const QString &path) const;  // returns "" if not in cache
    QStringList allPaths() const;

    /// For tests + downstream code that needs to enumerate the dedup structure.
    int fileCount() const;
    int uniqueHashCount() const;

private:
    const LinkResolver &m_resolver;
    QHash<QString /* relative path */, FileCacheEntry> m_pathToFileEntry;
    QHash<QString /* sha-256 hex */, CachedMetadata> m_hashToCache;
    QHash<QString /* sha-256 hex */, int> m_hashRefCount;

    /// Helper: insert a parsed (or deduped) entry for `path`. If the hash is
    /// already present in `m_hashToCache`, skips the parse and just bumps the
    /// ref-count. Otherwise calls `MetadataParser::parse` and stores the
    /// result.
    void insertParsed(const QString &path,
                      const QByteArray &content,
                      qint64 mtimeMs,
                      qint64 size,
                      const QString &hash);

    /// Helper: decrement the ref-count for `hash`; erase m_hashToCache entry if
    /// count drops to 0. No-op if hash is empty or not found.
    void releaseHashRef(const QString &hash);
};

}  // namespace Corbomite
