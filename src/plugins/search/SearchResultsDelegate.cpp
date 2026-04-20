// SPDX-License-Identifier: GPL-3.0-or-later
#include "SearchResultsDelegate.h"

#include "corbomite/models/SearchResultsModel.h"
#include "corbomite/search/ResultHighlighter.h"

#include <QApplication>
#include <QPainter>
#include <QPair>
#include <QVector>

namespace Corbomite {

SearchResultsDelegate::SearchResultsDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void SearchResultsDelegate::paint(QPainter *painter,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    const QVariant rangesVar = index.data(SearchResultsModel::MatchRangesRole);
    if (!rangesVar.isValid() || !rangesVar.canConvert<QVector<QPair<int, int>>>()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    const auto ranges = rangesVar.value<QVector<QPair<int, int>>>();
    if (ranges.isEmpty()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // Draw the frame / selection background, then blank the text Qt would
    // have rendered so we can overlay our own highlighted run.
    const QString text = opt.text;
    opt.text.clear();
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const QRect textRect = style->subElementRect(
        QStyle::SE_ItemViewItemText, &opt, opt.widget);

    painter->save();
    painter->setClipRect(textRect);

    const bool selected = opt.state & QStyle::State_Selected;
    const QPalette::ColorGroup cg = (opt.state & QStyle::State_Enabled)
        ? QPalette::Normal : QPalette::Disabled;
    const QColor normalColor = opt.palette.color(
        cg, selected ? QPalette::HighlightedText : QPalette::Text);
    // Keep highlight colour in the same text group so selected rows stay
    // readable; Qt's default painter otherwise uses a separate accent.
    const QColor highlightColor = normalColor;

    const QFontMetrics fm(opt.font);
    const int baseline = textRect.top() + (textRect.height() + fm.ascent() - fm.descent()) / 2;

    ResultHighlighter::drawHighlighted(
        painter, textRect.left(), baseline,
        text, ranges, opt.font,
        normalColor, highlightColor);

    painter->restore();
}

} // namespace Corbomite
