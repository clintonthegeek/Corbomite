// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace Corbomite {

// Mirrors Obsidian's PreparedQuery shape — `prepareQuery` (app/own/0780-$y.js)
// for the two-pass matcher and `prepareSimpleSearch` (app/own/0789-ab.js) for the
// substring one, which shares this struct but fills neither `tokens` beyond a
// space split nor `fuzzy` at all.
//
// `query`  : original user input, case preserved (so renderers can echo it).
// `tokens` : lowercase whitespace-split words PLUS 1-char punctuation/CJK
//            singletons, in source order. NOT a word count — "foo-bar" yields
//            ["foo","-","bar"].
// `fuzzy`  : lowercase full string minus literal spaces. The per-character
//            fallback alphabet for the second-pass matcher; empty when `simple`.
// `simple` : true when produced by prepareSimpleSearch.
//
// Build via FuzzyMatcher::prepareQuery() / prepareSimpleSearch().
struct PreparedQuery {
    QString query;
    QStringList tokens;
    QString fuzzy;
    bool simple = false;

    bool isEmpty() const { return query.isEmpty(); }
};

} // namespace Corbomite
