// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/SQLiteIndex.h"

namespace Corbomite {

class SQLiteIndex;

/// Permission-gated plugin-facing search / links / tags facade.
///
/// All methods gate on "metadata.read" and return empty collections
/// when the permission is absent. The proxy deliberately hides
/// SQLiteIndex's schema-shaped operations (reconcileWithCache,
/// repairLinks, orphanLinks, internal writes) — only the query
/// surface is stable for plugins.
class SearchProxy
{
public:
    SearchProxy(SQLiteIndex *index, const QSet<QString> &granted,
                QString pluginId);

    SearchProxy(const SearchProxy &) = delete;
    SearchProxy &operator=(const SearchProxy &) = delete;

    // ---- FTS (gated by metadata.read) ----
    QVector<SearchMatch> search(const QString &query,
                                int maxResults = 100) const;
    QVector<SearchMatch> searchCompiled(const QString &fts5Query,
                                        const QStringList &requiredTags,
                                        const QStringList &excludedTags) const;
    QVector<SearchMatch> searchCompiled(const QString &fts5Query,
                                        const QStringList &requiredTags,
                                        const QStringList &excludedTags,
                                        const QStringList &regexPatterns,
                                        const QStringList &caseSensitiveTerms) const;

    // ---- Links (gated by metadata.read) ----
    QVector<LinkInfo> backlinksFor(const QString &targetPath) const;
    QVector<LinkInfo> outlinksFor(const QString &sourcePath) const;
    QVector<LinkInfo> allLinks() const;

    // ---- Tags (gated by metadata.read) ----
    QStringList allTags() const;
    QStringList notesWithTag(const QString &tag) const;

private:
    SQLiteIndex  *m_index;
    QSet<QString> m_granted;
    QString       m_pluginId;

    bool canRead() const
    {
        return m_granted.contains(QStringLiteral("metadata.read"));
    }

    void logDenied(const char *method) const;
};

} // namespace Corbomite
