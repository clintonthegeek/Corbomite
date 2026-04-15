// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPair>
#include <QVector>

namespace Corbomite {

// Mirrors Obsidian's FuzzyMatch shape (search.md §2).
// `matches` is merge-sorted, non-overlapping [start, end) ranges over the haystack.
// `score` is unbounded; higher = better. Callers sort descending.
struct FuzzyMatch {
    double score = 0.0;
    QVector<QPair<int, int>> matches;
};

} // namespace Corbomite
