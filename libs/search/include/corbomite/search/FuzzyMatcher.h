// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include <QString>
#include <QVector>

#include "corbomite/search/FuzzyMatch.h"
#include "corbomite/search/PreparedQuery.h"

namespace Corbomite {

// Phase 1 stub. Real implementation in Phase 2 (FuzzyMatcher port from
// Obsidian search.md §1-§2 + _internal.js:83133-83143).
namespace FuzzyMatcher {

PreparedQuery prepareQuery(const QString &query);
PreparedQuery prepareSimpleSearch(const QString &query);

std::optional<FuzzyMatch> fuzzySearch(const PreparedQuery &query, const QString &haystack);

void sortSearchResults(QVector<FuzzyMatch> &results);

} // namespace FuzzyMatcher

} // namespace Corbomite
