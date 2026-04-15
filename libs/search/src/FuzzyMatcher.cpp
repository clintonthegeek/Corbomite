// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/search/FuzzyMatcher.h"

#include <algorithm>

namespace Corbomite::FuzzyMatcher {

PreparedQuery prepareQuery(const QString &query)
{
    PreparedQuery prepared;
    prepared.query = query.toLower();
    prepared.fuzzy = true;
    return prepared;
}

PreparedQuery prepareSimpleSearch(const QString &query)
{
    PreparedQuery prepared;
    prepared.query = query.toLower();
    prepared.fuzzy = false;
    return prepared;
}

std::optional<FuzzyMatch> fuzzySearch(const PreparedQuery &query, const QString &haystack)
{
    Q_UNUSED(query);
    Q_UNUSED(haystack);
    return std::nullopt;
}

void sortSearchResults(QVector<FuzzyMatch> &results)
{
    std::stable_sort(results.begin(), results.end(),
                     [](const FuzzyMatch &a, const FuzzyMatch &b) { return a.score > b.score; });
}

} // namespace Corbomite::FuzzyMatcher
