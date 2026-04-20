// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStyledItemDelegate>

namespace Corbomite {

/// Paints leaf (match) rows in SearchView's QTreeView using
/// ResultHighlighter::drawHighlighted so `SearchMatch::matches` ranges
/// render as bolded highlight spans. Group rows fall through to the
/// default QStyledItemDelegate behaviour.
class SearchResultsDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SearchResultsDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

} // namespace Corbomite
