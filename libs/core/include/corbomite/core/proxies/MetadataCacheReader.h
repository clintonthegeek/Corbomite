// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace Corbomite {

class MetadataCache;

/// Read-only MetadataCache facade for plugins with the "metadata.read"
/// permission. Stub — wire-up lands in Cluster Q Task 8.
class MetadataCacheReader
{
public:
    explicit MetadataCacheReader(MetadataCache *cache) : m_cache(cache) {}

    QStringList backlinksFor(const QString &target) const;
    QStringList outlinksFor(const QString &path) const;
    QStringList tagsIn(const QString &path) const;
    QStringList allTags() const;

private:
    MetadataCache *m_cache;
};

} // namespace Corbomite
