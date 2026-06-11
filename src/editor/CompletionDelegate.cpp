// SPDX-License-Identifier: GPL-3.0-or-later
#include "CompletionDelegate.h"

#include "corbomite/search/FuzzyMatcher.h"
#include "corbomite/search/ResultHighlighter.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>

namespace Corbomite {

CompletionDelegate::CompletionDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void CompletionDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    painter->save();

    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    const QString text = index.data(Qt::DisplayRole).toString();
    const QString detail = index.data(Qt::UserRole + 2).toString();
    QRect textRect = option.rect.adjusted(kCellPadding, 0, -kCellPadding, 0);
    const int baseline = textRect.center().y() + option.fontMetrics.ascent() / 2 - 1;

    // Dim right-hand detail (path / target note) — disambiguates rows that
    // share a display (e.g. Beta vs sub/Beta) and gives aliases/headings
    // their context. The popup sizes itself to fit this, so it normally
    // draws in full; we only elide if the row is clamped at the max width,
    // and always keep room for the display.
    if (!detail.isEmpty()) {
        const int natural = option.fontMetrics.horizontalAdvance(detail);
        const int minDisplay =
            qMin(option.fontMetrics.horizontalAdvance(text), 96) + kColumnGap;
        const int detailW = qMin(natural, qMax(0, textRect.width() - minDisplay));
        QRect detailRect = textRect;
        detailRect.setLeft(textRect.right() - detailW);
        const QString shown = (detailW < natural)
            ? option.fontMetrics.elidedText(detail, Qt::ElideLeft, detailW)
            : detail;
        painter->setPen(option.palette.color(QPalette::Disabled, QPalette::Text));
        painter->drawText(detailRect, Qt::AlignRight | Qt::AlignVCenter, shown);
        textRect.setRight(detailRect.left() - kColumnGap);   // reserve room for display
    }

    painter->setClipRect(textRect);   // never bleed the display under the detail

    if (!m_pattern.isEmpty()) {
        const auto prepared = FuzzyMatcher::prepareQuery(m_pattern);
        const auto matchOpt = FuzzyMatcher::fuzzySearch(prepared, text);
        const QVector<QPair<int, int>> ranges =
            matchOpt ? matchOpt->matches : QVector<QPair<int, int>>{};
        ResultHighlighter::drawHighlighted(painter,
                                            textRect.left(),
                                            baseline,
                                            text,
                                            ranges,
                                            option.font,
                                            option.palette.color(QPalette::Text),
                                            option.palette.color(QPalette::Link));
    } else {
        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
    }

    painter->restore();
}

QSize CompletionDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    // Width: the popup-forced row width (so rows fill the popup and the
    // detail right-aligns to a stable column); fall back to natural width.
    const int w = m_rowWidth > 0
        ? m_rowWidth
        : rowNaturalWidth(option.fontMetrics,
                          index.data(Qt::DisplayRole).toString(),
                          index.data(Qt::UserRole + 2).toString());
    return QSize(w, option.fontMetrics.height() + 8);
}

int CompletionDelegate::rowNaturalWidth(const QFontMetrics &fm,
                                        const QString &display, const QString &detail)
{
    int w = fm.horizontalAdvance(display);
    if (!detail.isEmpty())
        w += kColumnGap + fm.horizontalAdvance(detail);
    return w + 2 * kCellPadding;
}

void CompletionDelegate::setRowWidth(int width)
{
    m_rowWidth = width;
}

void CompletionDelegate::setFilterPattern(const QString &pattern)
{
    m_pattern = pattern;
}

} // namespace Corbomite
