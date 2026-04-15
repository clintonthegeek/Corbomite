// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace Corbomite {

// Mirrors Obsidian's PreparedQuery shape (search.md §2).
//
// `query`  : original user input, case preserved (so renderers can echo it).
// `tokens` : lowercase whitespace-split words PLUS 1-char punctuation/CJK
//            singletons, in source order. NOT a word count — "foo-bar" yields
//            ["foo","-","bar"].
// `fuzzy`  : lowercase full string, spaces removed. The per-character
//            fallback alphabet for the second-pass matcher.
// `simple` : true when produced by prepareSimpleSearch — disables char-fuzzy
//            fallback and CJK codepoint split.
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
