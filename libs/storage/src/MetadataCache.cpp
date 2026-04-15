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

#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataParser.h"

#include <QCryptographicHash>
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
{
    qRegisterMetaType<Corbomite::CachedMetadata>("Corbomite::CachedMetadata");
    m_indexFinishedTimer->setSingleShot(true);
    m_indexFinishedTimer->setInterval(10);  // 10ms debounce per audit §4
    connect(m_indexFinishedTimer, &QTimer::timeout,
            this, &MetadataCache::onIndexFinishedTimeout);
}

MetadataCache::~MetadataCache() = default;

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

        // Stat changed. Compute the new hash.
        const QString newHash = sha256Hex(content);

        if (it->hash == newHash) {
            // Content identical despite stat change. Update stat, no re-parse.
            // Audit §4: hash-unchanged path is silent. No event, no queue, no
            // timer disturbance.
            it->mtimeMs = mtimeMs;
            it->size = newSize;
            return;
        }

        // Hash changed. Release old hash, re-insert with new hash.
        const QString oldHash = it->hash;
        m_pathToFileEntry.erase(it);
        releaseHashRef(oldHash);

        insertParsed(path, content, mtimeMs, newSize, newHash);
        emitCacheChanged(path, oldHash);
        return;
    }

    // New path. Compute hash and insert.
    const QString newHash = sha256Hex(content);
    insertParsed(path, content, mtimeMs, newSize, newHash);
    emitCacheChanged(path, QString{});
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

void MetadataCache::insertParsed(const QString &path,
                                 const QByteArray &content,
                                 qint64 mtimeMs,
                                 qint64 size,
                                 const QString &hash)
{
    if (!m_hashToCache.contains(hash)) {
        // Parse the content and store the result.
        ParsedNote parsed = MetadataParser::parse(content, path, m_resolver);
        m_hashToCache.insert(hash, std::move(parsed.cache));
    }
    m_pathToFileEntry[path] = FileCacheEntry{mtimeMs, size, hash};
    ++m_hashRefCount[hash];
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
        if (cacheIt != m_hashToCache.end() && cacheIt->links) {
            for (LinkCache &link : *cacheIt->links) {
                // The `link.link` field is the already-resolved
                // "path#subpath" string; to re-resolve, treat the part
                // before the first '#' as the raw target.
                QString rawTarget = link.link;
                QString subpath;
                int hashIdx = rawTarget.indexOf(QLatin1Char('#'));
                if (hashIdx >= 0) {
                    subpath = rawTarget.mid(hashIdx);
                    rawTarget = rawTarget.left(hashIdx);
                }
                ResolvedLink resolved = m_resolver.resolve(path, rawTarget);
                if (resolved.resolved) {
                    link.link = resolved.path + resolved.subpath;
                }
                // If not resolved, leave as-is (pass-through preserved).
            }
            // TODO: also re-resolve embeds + frontmatterLinks. For Phase 4
            // the `links` vector is sufficient to exercise the ordering +
            // debounce semantics; resolution fidelity for embeds and
            // frontmatterLinks is a Phase 4.1 follow-up (or moves into
            // Phase 5 when parsing goes onto a worker thread).
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
}

}  // namespace Corbomite
