// SPDX-License-Identifier: GPL-3.0-or-later
#include "QuickSwitcherDelegate.h"
#include "QuickSwitcherModel.h"

#include "corbomite/search/FuzzyMatcher.h"
#include "corbomite/search/ResultHighlighter.h"

#include <QApplication>
#include <QPainter>

namespace Corbomite {

QuickSwitcherDelegate::QuickSwitcherDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void QuickSwitcherDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    painter->save();

    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    const QString name = index.data(QuickSwitcherModel::NoteNameRole).toString();
    const QString folder = index.data(QuickSwitcherModel::FolderPathRole).toString();

    const int padding = 6;
    QRect textRect = option.rect.adjusted(padding, 0, -padding, 0);

    QFont nameFont = option.font;
    nameFont.setPointSize(nameFont.pointSize() + 1);
    QFontMetrics nameFm(nameFont);
    const int baseline = textRect.center().y() + nameFm.ascent() / 2 - 1;

    if (!m_pattern.isEmpty()) {
        const auto prepared = FuzzyMatcher::prepareQuery(m_pattern);
        const auto matchOpt = FuzzyMatcher::fuzzySearch(prepared, name);
        const QVector<QPair<int, int>> ranges =
            matchOpt ? matchOpt->matches : QVector<QPair<int, int>>{};
        ResultHighlighter::drawHighlighted(painter,
                                            textRect.left(),
                                            baseline,
                                            name,
                                            ranges,
                                            nameFont,
                                            option.palette.color(QPalette::Text),
                                            option.palette.color(QPalette::Link));
    } else {
        painter->setFont(nameFont);
        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, name);
    }

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
