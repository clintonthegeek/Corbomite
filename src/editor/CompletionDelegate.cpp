// SPDX-License-Identifier: GPL-3.0-or-later
#include "CompletionDelegate.h"

#include <KFuzzyMatcher>
#include <QPainter>
#include <QApplication>

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

    if (!m_pattern.isEmpty()) {
        auto ranges = KFuzzyMatcher::matchedRanges(m_pattern, text);

        int x = textRect.left();
        int y = textRect.center().y() + option.fontMetrics.ascent() / 2 - 1;

        for (int i = 0; i < text.length(); ++i) {
            bool highlighted = false;
            for (const auto &range : ranges) {
                if (i >= range.start && i < range.start + range.length) {
                    highlighted = true;
                    break;
                }
            }

            QFont charFont = option.font;
            if (highlighted) {
                charFont.setBold(true);
                painter->setPen(option.palette.color(QPalette::Link));
            } else {
                painter->setPen(option.palette.color(QPalette::Text));
            }
            painter->setFont(charFont);
            QString ch = text.mid(i, 1);
            painter->drawText(x, y, ch);
            x += QFontMetrics(charFont).horizontalAdvance(ch);
        }
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
