// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/proxies/MetadataCacheReader.h"

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/MetadataCache.h"

#include <QSet>

namespace Corbomite {

MetadataCacheReader::MetadataCacheReader(MetadataCache *cache, QObject *parent)
    : QObject(parent), m_cache(cache)
{
    if (!m_cache) return;
    connect(m_cache, &MetadataCache::cacheChanged, this,
            [this](const QString &p, const QString &,
                   const Corbomite::CachedMetadata &) {
                Q_EMIT cacheChanged(p);
            });
    connect(m_cache, &MetadataCache::cacheDeleted, this,
            [this](const QString &p, const Corbomite::CachedMetadata &) {
                Q_EMIT cacheDeleted(p);
            });
    connect(m_cache, &MetadataCache::linksResolvedFor,
            this, &MetadataCacheReader::linksResolvedFor);
    connect(m_cache, &MetadataCache::allLinksResolved,
            this, &MetadataCacheReader::allLinksResolved);
    connect(m_cache, &MetadataCache::indexFinished,
            this, &MetadataCacheReader::indexFinished);
}

MetadataCacheReader::~MetadataCacheReader() = default;

namespace {

/// Strip `#heading` / `#^block` subpath suffix from a raw link target.
QString stripSubpath(QString target)
{
    const int hash = target.indexOf(QLatin1Char('#'));
    if (hash >= 0) target.truncate(hash);
    return target;
}

/// Strip trailing `.md` extension from a raw link target.
QString stripMdSuffix(QString target)
{
    if (target.endsWith(QLatin1String(".md"), Qt::CaseInsensitive))
        target.chop(3);
    return target;
}

/// Strip the leading `#` from a tag string (CachedMetadata stores `#foo`,
/// plugin API convention is bare `foo`).
QString stripHash(QString tag)
{
    if (tag.startsWith(QLatin1Char('#'))) tag.remove(0, 1);
    return tag;
}

bool linkMatchesTarget(const QString &rawLink, const QString &target)
{
    const QString linkStripped = stripMdSuffix(stripSubpath(rawLink));
    const QString targetStripped = stripMdSuffix(target);
    return linkStripped.compare(targetStripped, Qt::CaseInsensitive) == 0;
}

} // namespace

QStringList MetadataCacheReader::backlinksFor(const QString &target) const
{
    if (!m_cache) return {};
    QStringList sources;
    for (const QString &path : m_cache->allPaths()) {
        const auto entry = m_cache->getFileCache(path);
        if (!entry) continue;
        bool matched = false;
        if (entry->links) {
            for (const LinkCache &l : *entry->links) {
                if (linkMatchesTarget(l.link, target)) { matched = true; break; }
            }
        }
        if (!matched && entry->embeds) {
            for (const LinkCache &l : *entry->embeds) {
                if (linkMatchesTarget(l.link, target)) { matched = true; break; }
            }
        }
        if (!matched && entry->frontmatterLinks) {
            for (const FrontmatterLinkCache &l : *entry->frontmatterLinks) {
                if (linkMatchesTarget(l.link, target)) { matched = true; break; }
            }
        }
        if (matched) sources.append(path);
    }
    sources.sort();
    return sources;
}

QStringList MetadataCacheReader::outlinksFor(const QString &path) const
{
    if (!m_cache) return {};
    const auto entry = m_cache->getFileCache(path);
    if (!entry) return {};

    QSet<QString> seen;
    QStringList out;
    auto add = [&](const QString &t) {
        const QString stripped = stripSubpath(t);
        if (stripped.isEmpty() || seen.contains(stripped)) return;
        seen.insert(stripped);
        out.append(stripped);
    };
    if (entry->links) {
        for (const LinkCache &l : *entry->links) add(l.link);
    }
    if (entry->embeds) {
        for (const LinkCache &l : *entry->embeds) add(l.link);
    }
    if (entry->frontmatterLinks) {
        for (const FrontmatterLinkCache &l : *entry->frontmatterLinks) add(l.link);
    }
    return out;
}

QStringList MetadataCacheReader::tagsIn(const QString &path) const
{
    if (!m_cache) return {};
    const auto entry = m_cache->getFileCache(path);
    if (!entry || !entry->tags) return {};

    QSet<QString> seen;
    QStringList tags;
    for (const TagCache &t : *entry->tags) {
        const QString bare = stripHash(t.tag);
        if (bare.isEmpty() || seen.contains(bare)) continue;
        seen.insert(bare);
        tags.append(bare);
    }
    return tags;
}

QStringList MetadataCacheReader::allTags() const
{
    if (!m_cache) return {};
    QSet<QString> seen;
    QStringList tags;
    for (const QString &path : m_cache->allPaths()) {
        const auto entry = m_cache->getFileCache(path);
        if (!entry || !entry->tags) continue;
        for (const TagCache &t : *entry->tags) {
            const QString bare = stripHash(t.tag);
            if (bare.isEmpty() || seen.contains(bare)) continue;
            seen.insert(bare);
            tags.append(bare);
        }
    }
    tags.sort();
    return tags;
}

} // namespace Corbomite
