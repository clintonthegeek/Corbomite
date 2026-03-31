// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStyledItemDelegate>

namespace Corbomite {

class CompletionDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit CompletionDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    void setFilterPattern(const QString &pattern);

private:
    QString m_pattern;
};

} // namespace Corbomite
