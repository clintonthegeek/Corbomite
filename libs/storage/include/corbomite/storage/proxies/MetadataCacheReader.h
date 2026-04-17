// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace Corbomite {

class MetadataCache;

/// Read-only MetadataCache facade for plugins with the "metadata.read"
/// permission.
///
/// Inherits QObject so plugins can connect to the forwarded change
/// signals; the proxy bridges the host's MetadataCache::cacheChanged /
/// cacheDeleted / linksResolvedFor / allLinksResolved / indexFinished
/// across the plugin boundary without exposing the underlying
/// CachedMetadata payload (that surfaces only via the explicit query
/// methods, so plugins can't bypass the future shape of the proxy by
/// subscribing to raw signals).
class MetadataCacheReader : public QObject
{
    Q_OBJECT
public:
    explicit MetadataCacheReader(MetadataCache *cache, QObject *parent = nullptr);
    ~MetadataCacheReader() override;

    QStringList backlinksFor(const QString &target) const;
    QStringList outlinksFor(const QString &path) const;
    QStringList tagsIn(const QString &path) const;
    QStringList allTags() const;

Q_SIGNALS:
    /// Emitted when the cache entry for `path` changes (insert or update).
    void cacheChanged(const QString &path);

    /// Emitted when the cache entry for `path` is deleted.
    void cacheDeleted(const QString &path);

    /// Emitted after links for `path` have been resolved.
    void linksResolvedFor(const QString &path);

    /// Emitted once after the link-resolver drained its queue.
    void allLinksResolved();

    /// Debounced index-finished signal — emitted after the cache settles.
    void indexFinished();

private:
    MetadataCache *m_cache;
};

} // namespace Corbomite
