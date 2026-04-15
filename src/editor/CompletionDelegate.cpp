// SPDX-License-Identifier: GPL-3.0-or-later
#include "CompletionDelegate.h"

#include "corbomite/search/FuzzyMatcher.h"
#include "corbomite/search/ResultHighlighter.h"

#include <QApplication>
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
    const int padding = 4;
    QRect textRect = option.rect.adjusted(padding, 0, -padding, 0);
    const int baseline = textRect.center().y() + option.fontMetrics.ascent() / 2 - 1;

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
    Q_UNUSED(index)
    return QSize(option.rect.width(), option.fontMetrics.height() + 8);
}

void CompletionDelegate::setFilterPattern(const QString &pattern)
{
    m_pattern = pattern;
}

} // namespace Corbomite
