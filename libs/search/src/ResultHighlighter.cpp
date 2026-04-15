// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/search/ResultHighlighter.h"

#include <QFontMetrics>
#include <QPainter>

namespace Corbomite::ResultHighlighter {

namespace {

bool insideAnyRange(int i, const QVector<QPair<int, int>> &ranges)
{
    for (const auto &r : ranges) {
        if (i >= r.first && i < r.second) return true;
        if (r.first > i) return false;  // ranges are sorted
    }
    return false;
}

} // namespace

int drawHighlighted(QPainter *painter,
                    int x,
                    int baseline,
                    const QString &text,
                    const QVector<QPair<int, int>> &matches,
                    const QFont &baseFont,
                    const QColor &normalColor,
                    const QColor &highlightColor)
{
    QFont boldFont = baseFont;
    boldFont.setBold(true);
    const QFontMetrics normalFm(baseFont);
    const QFontMetrics boldFm(boldFont);

    int cursor = x;
    for (int i = 0; i < text.length(); ++i) {
        const bool hl = !matches.isEmpty() && insideAnyRange(i, matches);
        if (hl) {
            painter->setFont(boldFont);
            painter->setPen(highlightColor);
        } else {
            painter->setFont(baseFont);
            painter->setPen(normalColor);
        }
        const QString ch = text.mid(i, 1);
        painter->drawText(cursor, baseline, ch);
        cursor += (hl ? boldFm : normalFm).horizontalAdvance(ch);
    }
    return cursor;
}

} // namespace Corbomite::ResultHighlighter
