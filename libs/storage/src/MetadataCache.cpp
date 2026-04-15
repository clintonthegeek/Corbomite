// SPDX-License-Identifier: GPL-3.0-or-later
//
// MetadataCache — synchronous, signal-less core of the Obsidian-style
// metadata cache. Two layers: path -> FileCacheEntry{mtime,size,hash} and
// hash -> CachedMetadata, with a ref-count table that gates teardown of
// hash entries shared by multiple paths (content dedup).
//
// Phase 3 scope: data structures + onFileChanged / onFileDeleted /
// onUnsupportedFile mutation + read API. No Qt signals, no threading, no
// Events mixin. Phase 4 will layer signals on top of this via composition;
// Phase 5 will move parsing to a worker thread.

#include "corbomite/storage/MetadataCache.h"

#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataParser.h"

#include <QCryptographicHash>

namespace Corbomite {

namespace {

QString sha256Hex(const QByteArray &content)
{
    const QByteArray digest =
        QCryptographicHash::hash(content, QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}

} // namespace

MetadataCache::MetadataCache(const LinkResolver &resolver)
    : m_resolver(resolver)
{
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
            return;
        }

        // Stat changed. Compute the new hash.
        const QString newHash = sha256Hex(content);

        if (it->hash == newHash) {
            // Content identical despite stat change. Update stat, no re-parse.
            it->mtimeMs = mtimeMs;
            it->size = newSize;
            return;
        }

        // Hash changed. Release old hash, re-insert with new hash.
        const QString oldHash = it->hash;
        m_pathToFileEntry.erase(it);
        releaseHashRef(oldHash);

        insertParsed(path, content, mtimeMs, newSize, newHash);
        return;
    }

    // New path. Compute hash and insert.
    const QString newHash = sha256Hex(content);
    insertParsed(path, content, mtimeMs, newSize, newHash);
}

void MetadataCache::onFileDeleted(const QString &path)
{
    auto it = m_pathToFileEntry.find(path);
    if (it == m_pathToFileEntry.end()) {
        return;  // Unknown path -> no-op.
    }
    const QString hash = it->hash;
    m_pathToFileEntry.erase(it);
    releaseHashRef(hash);
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

}  // namespace Corbomite
