// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/proxies/SearchProxy.h"

#include <QLoggingCategory>

namespace {
Q_LOGGING_CATEGORY(lcSearchProxy, "corbomite.plugin.search-proxy")
}

namespace Corbomite {

SearchProxy::SearchProxy(SQLiteIndex *index, const QSet<QString> &granted,
                         QString pluginId)
    : m_index(index), m_granted(granted), m_pluginId(std::move(pluginId)) {}

void SearchProxy::logDenied(const char *method) const
{
    qCDebug(lcSearchProxy) << m_pluginId << "denied" << method
                           << "- missing metadata.read";
}

QVector<SearchMatch> SearchProxy::search(const QString &query, int maxResults) const
{
    if (!canRead()) { logDenied("search"); return {}; }
    if (!m_index)   return {};
    return m_index->search(query, maxResults);
}

QVector<SearchMatch> SearchProxy::searchCompiled(const QString &fts5Query,
                                                 const QStringList &requiredTags,
                                                 const QStringList &excludedTags) const
{
    if (!canRead()) { logDenied("searchCompiled"); return {}; }
    if (!m_index)   return {};
    return m_index->searchCompiled(fts5Query, requiredTags, excludedTags);
}

QVector<SearchMatch> SearchProxy::searchCompiled(const QString &fts5Query,
                                                 const QStringList &requiredTags,
                                                 const QStringList &excludedTags,
                                                 const QStringList &regexPatterns,
                                                 const QStringList &caseSensitiveTerms) const
{
    if (!canRead()) { logDenied("searchCompiled+postfilter"); return {}; }
    if (!m_index)   return {};
    return m_index->searchCompiled(fts5Query, requiredTags, excludedTags,
                                   regexPatterns, caseSensitiveTerms);
}

QVector<LinkInfo> SearchProxy::backlinksFor(const QString &targetPath) const
{
    if (!canRead()) { logDenied("backlinksFor"); return {}; }
    if (!m_index)   return {};
    return m_index->backlinksFor(targetPath);
}

QVector<LinkInfo> SearchProxy::outlinksFor(const QString &sourcePath) const
{
    if (!canRead()) { logDenied("outlinksFor"); return {}; }
    if (!m_index)   return {};
    return m_index->outlinksFor(sourcePath);
}

QVector<LinkInfo> SearchProxy::allLinks() const
{
    if (!canRead()) { logDenied("allLinks"); return {}; }
    if (!m_index)   return {};
    return m_index->allLinks();
}

QStringList SearchProxy::allTags() const
{
    if (!canRead()) { logDenied("allTags"); return {}; }
    if (!m_index)   return {};
    return m_index->allTags();
}

QStringList SearchProxy::notesWithTag(const QString &tag) const
{
    if (!canRead()) { logDenied("notesWithTag"); return {}; }
    if (!m_index)   return {};
    return m_index->notesWithTag(tag);
}

} // namespace Corbomite
