// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStyledItemDelegate>

class QFontMetrics;

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

    /// Force every row to this pixel width so rows fill the popup and the
    /// dim detail right-aligns to a consistent column. <=0 falls back to
    /// the cell's own width. The popup sets this from its content width.
    void setRowWidth(int width);

    /// Natural pixel width a row needs to show display + gap + detail
    /// without eliding (used by the popup to size itself).
    static int rowNaturalWidth(const QFontMetrics &fm, const QString &display,
                               const QString &detail);

    static constexpr int kCellPadding = 6;   ///< left/right inset inside a row
    static constexpr int kColumnGap = 24;    ///< min gap between display and detail

private:
    QString m_pattern;
    int m_rowWidth = -1;
};

} // namespace Corbomite
