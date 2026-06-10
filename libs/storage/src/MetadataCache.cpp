// SPDX-License-Identifier: GPL-3.0-or-later
//
// MetadataCache — Obsidian-compatible two-layer metadata cache.
//
// Phase 3 introduced the data structures + synchronous mutation core.
// Phase 4 (this file) adds:
//   - Qt signals with strict Obsidian ordering (cacheChanged ->
//     linksResolvedFor -> allLinksResolved -> indexFinished; cacheDeleted
//     is independent of the resolver queue).
//   - A link-resolver queue that drains one path per event-loop tick via
//     QTimer::singleShot(0, ...).
//   - A 10ms debounce on `indexFinished` (restarted when new work arrives).
//   - A Corbomite::Events mixin with Obsidian-named events
//     ("changed"/"deleted"/"resolve"/"resolved"/"finished") for plugin-
//     facing subscribers.
//
// No worker thread yet — Phase 5 adds that. MetadataParser::parse still
// runs synchronously on the calling thread.

#include "corbomite/storage/MetadataCache.h"

#include "corbomite/storage/CachedMetadataStore.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataParser.h"
#include "corbomite/storage/MetadataWorker.h"

#include <QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>
#include <QtCore/QLatin1Char>
#include <QtCore/QMetaType>
#include <QtCore/QTimer>
#include <QtCore/QVariant>

namespace Corbomite {

namespace {

QString sha256Hex(const QByteArray &content)
{
    const QByteArray digest =
        QCryptographicHash::hash(content, QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}

} // namespace

MetadataCache::MetadataCache(const LinkResolver &resolver, QObject *parent)
    : QObject(parent)
    , m_resolver(resolver)
    , m_indexFinishedTimer(new QTimer(this))
    , m_worker(new MetadataWorker(resolver, this))
    , m_persistTimer(new QTimer(this))
{
    qRegisterMetaType<Corbomite::CachedMetadata>("Corbomite::CachedMetadata");
    m_indexFinishedTimer->setSingleShot(true);
    m_indexFinishedTimer->setInterval(10);  // 10ms debounce per audit §4
    connect(m_indexFinishedTimer, &QTimer::timeout,
            this, &MetadataCache::onIndexFinishedTimeout);

    // 30s debounced auto-persist during active indexing. Restarts on every
    // mutation; fires once the cache has been quiescent for 30s. Also
    // flushed immediately on indexFinished and on close().
    m_persistTimer->setSingleShot(true);
    m_persistTimer->setInterval(30000);
    connect(m_persistTimer, &QTimer::timeout,
            this, &MetadataCache::onPersistTimerTimeout);

    // Worker parses run on the worker thread; results arrive here via a
    // Qt::QueuedConnection that the worker wires internally. We just attach
    // to its main-thread `parsed` signal.
    connect(m_worker, &MetadataWorker::parsed,
            this, &MetadataCache::onWorkerParsed);
}

MetadataCache::~MetadataCache()
{
    // Ensure any open store flushes + closes on destruction. Defensive:
    // callers should invoke close() explicitly, but we guard here so a
    // dropped MetadataCache doesn't leave a stale SQLite connection.
    if (m_store) {
        close();
    }
}

void MetadataCache::onFileChanged(const QString &path,
                                  const QByteArray &content,
                                  qint64 mtimeMs)
{
    const qint64 newSize = static_cast<qint64>(content.size());

    auto it = m_pathToFileEntry.find(path);
    if (it != m_pathToFileEntry.end()) {
        // Existing entry. Try stat short-circuit first.
        if (it->mtimeMs == mtimeMs && it->size == newSize) {
            // Stat unchanged -> skip re-parse AND skip hash computation.
            // Audit §4: "no content change, no event" — leave the debounce
            // timer alone; do NOT touch the queue or counters.
            return;
        }

        // Stat changed. Compute the new hash on the main thread (cheap,
        // and must be synchronous to avoid re-enqueuing on identical
        // content).
        const QString newHash = sha256Hex(content);

        if (it->hash == newHash) {
            // Content identical despite stat change. Update stat, no re-parse.
            // Audit §4: hash-unchanged path is silent. No event, no queue, no
            // timer disturbance.
            it->mtimeMs = mtimeMs;
            it->size = newSize;
            return;
        }

        // Hash changed. Route the parse through the worker. The old hash /
        // path-entry stays in place until `onWorkerParsed` replaces it, so
        // short-circuit comparisons for any interleaving `onFileChanged`
        // with the old content still work.
        m_worker->enqueueParse(path, content, mtimeMs, newSize);
        return;
    }

    // New path. Compute hash and enqueue parse.
    m_worker->enqueueParse(path, content, mtimeMs, newSize);
}

// Obsidian spec §3: Vault.read strips a leading UTF-8 BOM (U+FEFF,
// EF BB BF) before returning. rebuildVault reads raw bytes from disk and
// must apply the same strip so the SHA-256 hash and parsed output match
// what Vault::read would produce for the same file. Without this, a BOM'd
// file hashes with the three BOM bytes included, diverging from the hash
// seen by the single-file read path and causing spurious re-parses.
static auto stripUtf8BomMC = [](QByteArray bytes) -> QByteArray {
    if (bytes.size() >= 3
        && static_cast<unsigned char>(bytes[0]) == 0xEF
        && static_cast<unsigned char>(bytes[1]) == 0xBB
        && static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.remove(0, 3);
    }
    return bytes;
};

void MetadataCache::rebuildVault(const QString &vaultRoot,
                                 const QStringList &relativeNotePaths)
{
    const QDir root(vaultRoot);
    const QSet<QString> expected(relativeNotePaths.cbegin(),
                                 relativeNotePaths.cend());

    for (const QString &rel : relativeNotePaths) {
        const QFileInfo info(root.filePath(rel));
        if (!info.exists() || !info.isFile()) {
            continue;
        }
        QFile f(info.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray bytes = stripUtf8BomMC(f.readAll());
        const qint64 mtimeMs = info.lastModified().toMSecsSinceEpoch();
        onFileChanged(rel, bytes, mtimeMs);
    }

    // Reconcile: anything currently tracked that isn't in the caller-
    // supplied canonical list is implicitly deleted. Without this, a file
    // removed while the app was closed leaves a stale FileCacheEntry (and
    // its derived links in SQLiteIndex) after the next open.
    // Snapshot keys first because onFileDeleted mutates m_pathToFileEntry.
    QStringList stale;
    for (auto it = m_pathToFileEntry.cbegin(), end = m_pathToFileEntry.cend();
         it != end; ++it) {
        if (!expected.contains(it.key())) {
            stale.push_back(it.key());
        }
    }
    for (const QString &path : std::as_const(stale)) {
        onFileDeleted(path);
    }
}

void MetadataCache::onFileDeleted(const QString &path)
{
    auto it = m_pathToFileEntry.find(path);
    if (it == m_pathToFileEntry.end()) {
        // Unknown path -> no-op. Do NOT emit cacheDeleted; Obsidian is silent
        // on this path too.
        return;
    }

    // Capture prevCache BEFORE teardown so the signal consumer sees the
    // final state. For tracked-unsupported paths, prevCache is an empty
    // struct — consistent with getFileCache() returning CachedMetadata{}
    // for that state.
    CachedMetadata prevCache;
    if (!it->hash.isEmpty()) {
        auto cacheIt = m_hashToCache.constFind(it->hash);
        if (cacheIt != m_hashToCache.constEnd()) {
            prevCache = *cacheIt;
        }
    }

    const QString hash = it->hash;
    m_pathToFileEntry.erase(it);
    releaseHashRef(hash);

    Q_EMIT cacheDeleted(path, prevCache);
    m_events.trigger(QStringLiteral("deleted"),
                     {path, QVariant::fromValue(prevCache)});
    scheduleDebouncedPersist();
}

void MetadataCache::onUnsupportedFile(const QString &path,
                                      qint64 mtimeMs,
                                      qint64 size)
{
    auto it = m_pathToFileEntry.find(path);
    if (it != m_pathToFileEntry.end()) {
        if (it->hash.isEmpty() && it->mtimeMs == mtimeMs && it->size == size) {
            return;  // Already tracked as unsupported with same stat.
        }
        // Transitioning from a previous state (e.g. was a .md, now renamed to
        // .png) -- release old hash, re-track as unsupported.
        const QString oldHash = it->hash;
        m_pathToFileEntry.erase(it);
        releaseHashRef(oldHash);
    }
    m_pathToFileEntry[path] = FileCacheEntry{mtimeMs, size, QString{}};
    // Note: no entry in m_hashToCache for unsupported files.
    // No signal emission — audit §4 treats tracked-unsupported as silent.
}

std::optional<CachedMetadata> MetadataCache::getFileCache(const QString &path) const
{
    auto it = m_pathToFileEntry.constFind(path);
    if (it == m_pathToFileEntry.constEnd()) {
        return std::nullopt;  // Not in cache.
    }
    if (it->hash.isEmpty()) {
        return CachedMetadata{};  // Tracked but unsupported -> empty struct.
    }
    auto cacheIt = m_hashToCache.constFind(it->hash);
    if (cacheIt == m_hashToCache.constEnd()) {
        // Invariant broken -- entry in path map but missing from hash map.
        // Defensively treat as not-in-cache rather than crash.
        return std::nullopt;
    }
    return *cacheIt;
}

QString MetadataCache::getFileHash(const QString &path) const
{
    auto it = m_pathToFileEntry.constFind(path);
    if (it == m_pathToFileEntry.constEnd()) {
        return QString{};
    }
    return it->hash;
}

QStringList MetadataCache::allPaths() const
{
    return m_pathToFileEntry.keys();
}

int MetadataCache::fileCount() const
{
    return m_pathToFileEntry.size();
}

int MetadataCache::uniqueHashCount() const
{
    return m_hashToCache.size();
}

void MetadataCache::insertWorkerResult(const QString &path,
                                       qint64 mtimeMs,
                                       qint64 size,
                                       const QString &hash,
                                       const CachedMetadata &cache)
{
    if (!m_hashToCache.contains(hash)) {
        // First time we've seen this content -- store the parsed metadata.
        // Subsequent paths with the same hash dedup onto this entry.
        m_hashToCache.insert(hash, cache);
    }
    m_pathToFileEntry[path] = FileCacheEntry{mtimeMs, size, hash};
    ++m_hashRefCount[hash];
}

void MetadataCache::onWorkerParsed(const QString &path,
                                   qint64 mtimeMs,
                                   qint64 size,
                                   const CachedMetadata &cache,
                                   const QString &hash)
{
    // Stat/hash may have moved since enqueue -- if the path currently holds
    // the same hash we just computed, the parse result was for already-known
    // content; still refresh stat and emit cacheChanged so consumers see the
    // mtime update even though dedup kept the same hash entry.
    QString prevHash;
    auto it = m_pathToFileEntry.find(path);
    if (it != m_pathToFileEntry.end()) {
        prevHash = it->hash;
        // Release the old hash's ref before we overwrite the entry.
        const QString oldHash = it->hash;
        m_pathToFileEntry.erase(it);
        releaseHashRef(oldHash);
    }

    insertWorkerResult(path, mtimeMs, size, hash, cache);
    emitCacheChanged(path, prevHash);
    scheduleDebouncedPersist();
}

void MetadataCache::releaseHashRef(const QString &hash)
{
    if (hash.isEmpty()) {
        return;
    }
    auto rcIt = m_hashRefCount.find(hash);
    if (rcIt == m_hashRefCount.end()) {
        return;
    }
    if (--(*rcIt) <= 0) {
        m_hashRefCount.erase(rcIt);
        m_hashToCache.remove(hash);
    }
}

void MetadataCache::emitCacheChanged(const QString &path, const QString &prevHash)
{
    // New work has arrived: if a debounce timer for indexFinished is pending,
    // it's no longer valid — we'll rearm when the queue drains.
    if (m_indexFinishedTimer->isActive()) {
        m_indexFinishedTimer->stop();
    }

    // Look up the current cache for this path so we can emit it.
    // The helper is only called on non-short-circuit paths, so the path is
    // expected to be present with a non-empty hash.
    CachedMetadata current;
    auto pathIt = m_pathToFileEntry.constFind(path);
    if (pathIt != m_pathToFileEntry.constEnd() && !pathIt->hash.isEmpty()) {
        auto cacheIt = m_hashToCache.constFind(pathIt->hash);
        if (cacheIt != m_hashToCache.constEnd()) {
            current = *cacheIt;
        }
    }

    Q_EMIT cacheChanged(path, prevHash, current);
    m_events.trigger(QStringLiteral("changed"),
                     {path, prevHash, QVariant::fromValue(current)});

    m_linkResolverQueue.enqueue(path);

    // Only post a drain continuation if no other drain is outstanding.
    // A drain chains itself via singleShot(0, ...) on every non-empty pop,
    // so one outstanding continuation is enough to fully drain the queue.
    //
    // Bump m_inProgressTaskCount iff we're starting a new drain cycle (i.e.
    // queue was empty and is now non-empty). This pairs 1:1 with the
    // `allLinksResolved` decrement at drain-end so burst coalescing works:
    // N onFileChanged calls in the same sync burst => one bump, drain
    // processes all N paths in one cycle, one `allLinksResolved`, one
    // decrement back to zero, debounce timer arms.
    if (m_linkResolverQueue.size() == 1) {
        ++m_inProgressTaskCount;
        QTimer::singleShot(0, this, &MetadataCache::drainOnePath);
    }
}

void MetadataCache::drainOnePath()
{
    if (m_linkResolverQueue.isEmpty()) {
        return;
    }
    const QString path = m_linkResolverQueue.dequeue();

    // Re-resolve links in place. If the path's hash entry is gone (file was
    // deleted between enqueue and drain), skip silently.
    auto pathIt = m_pathToFileEntry.constFind(path);
    if (pathIt != m_pathToFileEntry.constEnd() && !pathIt->hash.isEmpty()) {
        auto cacheIt = m_hashToCache.find(pathIt->hash);
        if (cacheIt != m_hashToCache.end()) {
            // The `.link` field is a "path#subpath" string; re-resolution
            // splits at the first `#`, asks the resolver about the raw
            // target, and rewrites only when resolved (preserving raw
            // pass-through for misses). Same logic for body links, embeds,
            // and frontmatter links — frontmatter links specifically
            // collectFrontmatterLinks() doesn't run the resolver itself, so
            // this is the *only* place they ever get resolved.
            auto reresolve = [&](QString &linkStr) {
                QString rawTarget = linkStr;
                QString subpath;
                const int hashIdx = rawTarget.indexOf(QLatin1Char('#'));
                if (hashIdx >= 0) {
                    subpath = rawTarget.mid(hashIdx);
                    rawTarget = rawTarget.left(hashIdx);
                }
                ResolvedLink resolved = m_resolver.resolve(path, rawTarget);
                if (resolved.resolved) {
                    linkStr = resolved.path + resolved.subpath;
                }
            };
            if (cacheIt->links) {
                for (LinkCache &link : *cacheIt->links) reresolve(link.link);
            }
            if (cacheIt->embeds) {
                for (LinkCache &link : *cacheIt->embeds) reresolve(link.link);
            }
            if (cacheIt->frontmatterLinks) {
                for (FrontmatterLinkCache &fml : *cacheIt->frontmatterLinks)
                    reresolve(fml.link);
            }
        }
    }

    Q_EMIT linksResolvedFor(path);
    m_events.trigger(QStringLiteral("resolve"), {path});

    if (!m_linkResolverQueue.isEmpty()) {
        // More paths to drain — chain one more per-tick continuation.
        QTimer::singleShot(0, this, &MetadataCache::drainOnePath);
    } else {
        // Queue empty: emit the bulk "all done" + decrement the task count.
        // Coalesce semantics: each onFileChanged bumps the count once, and
        // the fully-drained queue decrements it once. Under sequential
        // Phase-4 semantics a burst of N onFileChanged calls collapses to
        // one allLinksResolved + one indexFinished (after debounce).
        Q_EMIT allLinksResolved();
        m_events.trigger(QStringLiteral("resolved"), QVariantList{});

        if (--m_inProgressTaskCount <= 0) {
            m_inProgressTaskCount = 0;
            m_indexFinishedTimer->start();
        }
    }
}

void MetadataCache::onIndexFinishedTimeout()
{
    Q_EMIT indexFinished();
    m_events.trigger(QStringLiteral("finished"), QVariantList{});

    // Flush the cache state to disk immediately on index completion. This
    // makes the post-bulk-rebuild state durable without waiting the 30s
    // debounce window — survives a crash after bulk rebuild.
    persistNow();
}

void MetadataCache::onPersistTimerTimeout()
{
    persistNow();
}

void MetadataCache::persistNow()
{
    if (m_persistTimer && m_persistTimer->isActive()) {
        m_persistTimer->stop();
    }
    if (!m_store || !m_store->isOpen()) {
        return;
    }
    m_store->persistFrom(*this);
}

void MetadataCache::scheduleDebouncedPersist()
{
    if (!m_store || !m_store->isOpen()) {
        return;
    }
    // setSingleShot(true) + start() restarts the timer on each call, so a
    // burst of mutations coalesces into one persist 30s after the last one.
    m_persistTimer->start();
}

bool MetadataCache::open(const QString &dbPath)
{
    if (m_store) {
        close();
    }
    m_store = std::make_unique<CachedMetadataStore>();
    if (!m_store->open(dbPath)) {
        m_store.reset();
        return false;
    }
    // Load any existing state. If the DB is fresh, this leaves the cache
    // untouched. Note: loaded stats may be stale vs on-disk files; callers
    // that want a post-load reconcile should feed onFileChanged() for each
    // current file -- stat short-circuit handles the no-change path.
    m_store->loadInto(*this);
    return true;
}

void MetadataCache::close()
{
    if (!m_store) {
        return;
    }
    if (m_store->isOpen()) {
        m_store->persistFrom(*this);
        m_store->close();
    }
    m_store.reset();
    if (m_persistTimer && m_persistTimer->isActive()) {
        m_persistTimer->stop();
    }
}

QHash<QString, FileCacheEntry> MetadataCache::pathToFileEntrySnapshot() const
{
    return m_pathToFileEntry;
}

QHash<QString, CachedMetadata> MetadataCache::hashToCacheSnapshot() const
{
    return m_hashToCache;
}

QHash<QString, int> MetadataCache::hashRefCountSnapshot() const
{
    return m_hashRefCount;
}

void MetadataCache::installPersistedState(
    const QHash<QString, FileCacheEntry> &pathEntries,
    const QHash<QString, CachedMetadata> &hashMap,
    const QHash<QString, int> &hashRefCounts)
{
    // Replace whatever was there. No signals, no parse, no queue touch --
    // this is a bulk reinstall of a previously-persisted snapshot.
    m_pathToFileEntry = pathEntries;
    m_hashToCache = hashMap;
    m_hashRefCount = hashRefCounts;
}

}  // namespace Corbomite
