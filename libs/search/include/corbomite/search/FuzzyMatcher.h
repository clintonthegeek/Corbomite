// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include <QString>
#include <QVector>

#include "corbomite/search/FuzzyMatch.h"
#include "corbomite/search/PreparedQuery.h"

namespace Corbomite {

// Obsidian 1.13.7 `prepareQuery` / `prepareSimpleSearch` / `fuzzySearch` /
// `sortSearchResults`, ported from app/own/0778-Gy.js, 0779-Yy.js, 0780-$y.js,
// 0782-Jy.js, 0783-eb.js, 0784-tb.js, 0785-nb.js, 0787-rb.js, 0788-ob.js and
// 0789-ab.js. See
// docs/obsidian-audit/addenda/2026-10-03-search-dsl-grammar-and-quick-switcher-scoring.md
// for the corpus citations.
namespace FuzzyMatcher {

// Two-pass token matcher input (word-token pass, then strict-adjacency
// char-fuzzy pass).
PreparedQuery prepareQuery(const QString &query);

// Obsidian's *separate* matcher, not a flag on prepareQuery: space-split tokens,
// ordered substring-AND, no char-fuzzy fallback, no punctuation/CJK split, and
// a hardcoded case-fidelity penalty of 0. It ranks a candidate list materially
// differently from prepareQuery, not just faster.
//
// Obsidian picks between the two at the quick-switcher call site on candidate
// count — `m.length < 1e4 ? Zy(e) : ab(e)` (app/own/1951-e3.js:89). That switch
// is deliberately NOT mirrored here: it is keyed on the caller's candidate list,
// which a PreparedQuery cannot see, so putting it in the library would make
// ranking depend on vault size invisibly and with no way for a caller to opt in
// or out. A caller holding the candidate list should choose the matcher itself.
PreparedQuery prepareSimpleSearch(const QString &query);

std::optional<FuzzyMatch> fuzzySearch(const PreparedQuery &query, const QString &haystack);

void sortSearchResults(QVector<FuzzyMatch> &results);

} // namespace FuzzyMatcher

} // namespace Corbomite
