// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace Corbomite {

// Mirrors Obsidian's PreparedQuery shape (search.md §2).
// Built via FuzzyMatcher::prepareQuery().
struct PreparedQuery {
    QString query;        // original, lower-cased
    QStringList tokens;   // whitespace/punctuation split, CJK codepoints split per char
    bool fuzzy = true;    // false for prepareSimpleSearch (literal substring)

    bool isEmpty() const { return tokens.isEmpty(); }
};

} // namespace Corbomite
