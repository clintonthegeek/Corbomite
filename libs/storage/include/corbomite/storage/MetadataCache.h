// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/Events.h"
#include "corbomite/storage/CachedMetadata.h"

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

#include <optional>

namespace Corbomite {

class LinkResolver;

struct FileCacheEntry {
    qint64 mtimeMs = 0;
    qint64 size = 0;
    QString hash;  // empty string means "unsupported extension / not-parseable"
};

/// Two-layer path-keyed / hash-keyed metadata cache with stat short-circuit and
/// content-hash dedup.
///
/// Phase 4 wraps the Phase-3 synchronous core in a QObject that emits five
/// Qt signals in strict Obsidian-compatible order:
///
///   1. `cacheChanged(path, prevHash, cache)` — synchronous from inside
///      `onFileChanged` after a successful parse + cache insert.
///   2. `linksResolvedFor(path)` — asynchronous, one per event-loop tick
///      via `QTimer::singleShot(0, ...)` per-path draining.
///   3. `allLinksResolved()` — synchronous from the drain slot once the
///      link-resolver queue empties.
///   4. `indexFinished()` — synchronous from a 10ms one-shot timer that
///      is (re)armed after each `allLinksResolved`. A new `onFileChanged`
///      during the 10ms window stops the timer; it rearms when the new
///      work drains.
///   5. `cacheDeleted(path, prevCache)` — synchronous from inside
///      `onFileDeleted`; does NOT touch the resolver queue (Obsidian
///      doesn't resolve after delete).
///
/// A sibling `Corbomite::Events` mixin (exposed via `events()`) fires the
/// same notifications under Obsidian-named event keys
/// (`changed`/`deleted`/`resolve`/`resolved`/`finished`) so plugin-facing
/// subscribers can use the `on("name", fn)` API unchanged.
///
/// Short-circuit paths (stat unchanged, hash unchanged with stat change)
/// are silent — no signals fire, no queue/counter touched — per
/// audit §4 "no content change, no event".
///
/// Thread-safety: not safe for concurrent mutation. All mutation methods
/// must be called from a single thread (Phase 5 introduces a worker that
/// posts results back via queued connections).
class MetadataCache : public QObject {
    Q_OBJECT

public:
    explicit MetadataCache(const LinkResolver &resolver, QObject *parent = nullptr);
    ~MetadataCache() override;

    MetadataCache(const MetadataCache &) = delete;
    MetadataCache &operator=(const MetadataCache &) = delete;

    /// Mutation — file changed (created or modified on disk).
    /// Computes SHA-256 only on stat change. Re-parses only on hash change.
    /// Empty `content` for a known extension treated as a zero-byte note.
    ///
    /// Emits `cacheChanged` synchronously on the non-short-circuit path
    /// (new path OR hash-change). Enqueues `path` for link-resolve drain.
    void onFileChanged(const QString &path, const QByteArray &content, qint64 mtimeMs);

    /// Mutation — file deleted.
    /// Captures prevCache BEFORE removing the path entry; emits
    /// `cacheDeleted(path, prevCache)` synchronously. Does NOT touch the
    /// resolver queue or `m_inProgressTaskCount`.
    void onFileDeleted(const QString &path);

    /// Register a path as unsupported (non-.md). Entry stored with hash == "".
    /// getFileCache(path) returns an empty CachedMetadata{} for this state
    /// (vs std::nullopt for "not in cache at all").
    ///
    /// Silent — emits no signals (tracking an image is not a metadata event).
    void onUnsupportedFile(const QString &path, qint64 mtimeMs, qint64 size);

    /// Read API.
    std::optional<CachedMetadata> getFileCache(const QString &path) const;
    QString getFileHash(const QString &path) const;  // returns "" if not in cache
    QStringList allPaths() const;

    /// For tests + downstream code that needs to enumerate the dedup structure.
    int fileCount() const;
    int uniqueHashCount() const;

    /// Plugin-facing event bus — Obsidian-named events:
    ///   "changed"  (path, prevHash, CachedMetadata)
    ///   "deleted"  (path, CachedMetadata prevCache)
    ///   "resolve"  (path)
    ///   "resolved" ()
    ///   "finished" ()
    Corbomite::Events &events() { return m_events; }
    const Corbomite::Events &events() const { return m_events; }

Q_SIGNALS:
    void cacheChanged(const QString &path,
                      const QString &prevHash,
                      const Corbomite::CachedMetadata &cache);
    void cacheDeleted(const QString &path,
                      const Corbomite::CachedMetadata &prevCache);
    void linksResolvedFor(const QString &path);
    void allLinksResolved();
    void indexFinished();

private Q_SLOTS:
    void drainOnePath();
    void onIndexFinishedTimeout();

private:
    const LinkResolver &m_resolver;
    QHash<QString /* relative path */, FileCacheEntry> m_pathToFileEntry;
    QHash<QString /* sha-256 hex */, CachedMetadata> m_hashToCache;
    QHash<QString /* sha-256 hex */, int> m_hashRefCount;

    Corbomite::Events m_events;
    QQueue<QString> m_linkResolverQueue;
    QTimer *m_indexFinishedTimer;  // owned via parent (this)
    int m_inProgressTaskCount = 0;

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

    /// Phase 4 helper: emit `cacheChanged`, trigger Obsidian `"changed"` event,
    /// enqueue `path` on the link-resolver queue, bump `m_inProgressTaskCount`,
    /// and post a per-tick drain continuation (only if no outstanding drain is
    /// already posted, i.e. queue size is 1 post-enqueue). Also stops the
    /// `indexFinished` debounce timer since new work arrived.
    void emitCacheChanged(const QString &path, const QString &prevHash);
};

}  // namespace Corbomite

Q_DECLARE_METATYPE(Corbomite::CachedMetadata)
