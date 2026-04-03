// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_TABLEBLOCKITEM_H
#define MARKOFF_TABLEBLOCKITEM_H

#include "BlockItem.h"
#include <QStringList>
#include <QFont>

namespace Markoff {

/// Read-only table rendered from pipe-delimited markdown.
class TableBlockItem : public BlockItem {
    Q_OBJECT
public:
    explicit TableBlockItem(const QString &markdown, qreal maxWidth,
                            QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;
    QString toMarkdown() const override;

    void setFont(const QFont &font);

private:
    void parseMarkdown();
    void computeLayout();

    QString m_markdown;
    QStringList m_headers;
    QList<Qt::Alignment> m_alignments;
    QList<QStringList> m_rows;
    QList<qreal> m_colWidths;
    qreal m_maxWidth;
    qreal m_rowHeight = 0;
    qreal m_totalWidth = 0;
    qreal m_totalHeight = 0;
    qreal m_cellPadding = 8.0;
    QFont m_font;
};

} // namespace Markoff

#endif // MARKOFF_TABLEBLOCKITEM_H
