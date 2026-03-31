// SPDX-License-Identifier: GPL-3.0-or-later
#include "QuickSwitcherDelegate.h"
#include "QuickSwitcherModel.h"

#include <KFuzzyMatcher>
#include <QPainter>
#include <QApplication>

namespace Corbomite {

QuickSwitcherDelegate::QuickSwitcherDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void QuickSwitcherDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    painter->save();

    // Draw background (selection highlight)
    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    const QString name = index.data(QuickSwitcherModel::NoteNameRole).toString();
    const QString folder = index.data(QuickSwitcherModel::FolderPathRole).toString();

    const int padding = 6;
    QRect textRect = option.rect.adjusted(padding, 0, -padding, 0);

    // Draw note name with fuzzy match highlighting
    QFont nameFont = option.font;
    nameFont.setPointSize(nameFont.pointSize() + 1);
    QFontMetrics nameFm(nameFont);

    if (!m_pattern.isEmpty()) {
        // Get matched character ranges for highlighting
        auto ranges = KFuzzyMatcher::matchedRanges(m_pattern, name);

        int x = textRect.left();
        int y = textRect.center().y() + nameFm.ascent() / 2 - 1;

        for (int i = 0; i < name.length(); ++i) {
            bool highlighted = false;
            for (const auto &range : ranges) {
                if (i >= range.start && i < range.start + range.length) {
                    highlighted = true;
                    break;
                }
            }

            QFont charFont = nameFont;
            if (highlighted) {
                charFont.setBold(true);
                painter->setPen(option.palette.color(QPalette::Link));
            } else {
                painter->setPen(option.palette.color(QPalette::Text));
            }
            painter->setFont(charFont);
            QString ch = name.mid(i, 1);
            painter->drawText(x, y, ch);
            x += QFontMetrics(charFont).horizontalAdvance(ch);
        }
    } else {
        // No filter — draw name normally
        painter->setFont(nameFont);
        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, name);
    }

    // Draw folder path in muted color on the right
    if (!folder.isEmpty()) {
        QFont folderFont = option.font;
        folderFont.setPointSize(folderFont.pointSize() - 1);
        painter->setFont(folderFont);
        painter->setPen(option.palette.color(QPalette::PlaceholderText));
        painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, folder);
    }

    painter->restore();
}

QSize QuickSwitcherDelegate::sizeHint(const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QSize(option.rect.width(), option.fontMetrics.height() + 16);
}

void QuickSwitcherDelegate::setFilterPattern(const QString &pattern)
{
    m_pattern = pattern;
}

} // namespace Corbomite
