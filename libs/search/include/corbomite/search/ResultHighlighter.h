// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QFont>
#include <QPair>
#include <QString>
#include <QVector>

class QPainter;

namespace Corbomite::ResultHighlighter {

// Draw `text` at (x, baseline) into `painter`, painting characters whose index
// falls inside any range of `matches` with `highlightColor` + bold, and the
// rest with `normalColor` + the base font weight.
//
// `matches` is the merge-sorted, non-overlapping range list produced by
// FuzzyMatcher::fuzzySearch. Empty ranges produce a normal-coloured draw.
//
// Returns the next x cursor (so callers can chain follow-on text on the
// same baseline).
int drawHighlighted(QPainter *painter,
                    int x,
                    int baseline,
                    const QString &text,
                    const QVector<QPair<int, int>> &matches,
                    const QFont &baseFont,
                    const QColor &normalColor,
                    const QColor &highlightColor);

} // namespace Corbomite::ResultHighlighter
