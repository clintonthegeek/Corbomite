// SPDX-License-Identifier: GPL-3.0-or-later
#include "TableBlockItem.h"
#include <markoff-parser/TableHandler.h>
#include <QPainter>
#include <QFontMetricsF>

namespace Markoff {

TableBlockItem::TableBlockItem(const QString &markdown, qreal maxWidth,
                               QGraphicsItem *parent)
    : BlockItem(parent)
    , m_markdown(markdown)
    , m_maxWidth(maxWidth)
{
    parseMarkdown();
    computeLayout();
}

void TableBlockItem::parseMarkdown()
{
    const QStringList lines = m_markdown.split(QLatin1Char('\n'));
    if (lines.size() < 2) return;

    m_headers = TableHandler::parseRow(lines[0]);

    // Line 1 is the separator — parse alignments
    m_alignments.clear();
    QStringList sepCells = TableHandler::parseRow(lines[1]);
    for (const QString &cell : sepCells)
        m_alignments.append(TableHandler::parseAlignment(cell));

    // Remaining lines are data rows
    m_rows.clear();
    for (int i = 2; i < lines.size(); ++i) {
        if (lines[i].trimmed().isEmpty()) continue;
        m_rows.append(TableHandler::parseRow(lines[i]));
    }
}

void TableBlockItem::computeLayout()
{
    QFontMetricsF fm(m_font);
    m_rowHeight = fm.height() + m_cellPadding * 2;

    int cols = m_headers.size();
    if (cols == 0) return;

    // Compute column widths from content
    m_colWidths.resize(cols);
    for (int c = 0; c < cols; ++c) {
        qreal w = fm.horizontalAdvance(m_headers[c]);
        for (const auto &row : m_rows) {
            if (c < row.size())
                w = qMax(w, fm.horizontalAdvance(row[c]));
        }
        m_colWidths[c] = w + m_cellPadding * 2;
    }

    // Scale columns to fit maxWidth if needed
    qreal natural = 0;
    for (qreal w : m_colWidths) natural += w;
    if (natural > m_maxWidth && natural > 0) {
        qreal scale = m_maxWidth / natural;
        for (qreal &w : m_colWidths) w *= scale;
    }

    m_totalWidth = 0;
    for (qreal w : m_colWidths) m_totalWidth += w;

    int totalRows = 1 + m_rows.size(); // header + data
    m_totalHeight = totalRows * m_rowHeight;
}

void TableBlockItem::setFolded(bool folded, const QString &language, int lineCount)
{
    if (m_folded == folded
        && m_foldedLanguage == language
        && m_foldedLineCount == lineCount) return;
    prepareGeometryChange();
    m_folded = folded;
    m_foldedLanguage = language;
    m_foldedLineCount = lineCount;
    update();
}

QString TableBlockItem::summaryForTesting() const
{
    const QString countPart = m_foldedLineCount == 1
        ? QStringLiteral("(1 line)")
        : QStringLiteral("(%1 lines)").arg(m_foldedLineCount);
    if (m_foldedLanguage.isEmpty())
        return QStringLiteral("``` %1").arg(countPart);
    return QStringLiteral("```%1 %2").arg(m_foldedLanguage, countPart);
}

QRectF TableBlockItem::boundingRect() const
{
    if (m_folded) {
        const qreal h = QFontMetrics(m_font).lineSpacing() + 6;
        return QRectF(0, 0, m_maxWidth, h);
    }
    return {0, 0, m_totalWidth, m_totalHeight};
}

void TableBlockItem::paint(QPainter *painter,
                           const QStyleOptionGraphicsItem * /*option*/,
                           QWidget * /*widget*/)
{
    if (m_folded) {
        painter->save();
        painter->fillRect(boundingRect(), QColor(245, 245, 245));  // match code bg
        painter->setPen(Qt::darkGray);
        painter->setFont(m_font);
        const QString text = summaryForTesting();
        painter->drawText(boundingRect().adjusted(8, 0, -4, 0),
                          Qt::AlignVCenter | Qt::AlignLeft, text);
        painter->restore();
        return;
    }

    if (m_headers.isEmpty()) return;

    painter->save();
    painter->setFont(m_font);
    painter->setRenderHint(QPainter::Antialiasing, false);

    QFontMetricsF fm(m_font);
    int cols = m_headers.size();

    // Header background
    QRectF headerBg(0, 0, m_totalWidth, m_rowHeight);
    painter->fillRect(headerBg, QColor(0xf0, 0xf0, 0xf0));

    // Draw cells
    auto drawRow = [&](int row, const QStringList &cells) {
        qreal x = 0;
        qreal y = row * m_rowHeight;
        for (int c = 0; c < cols; ++c) {
            QRectF cellRect(x, y, m_colWidths[c], m_rowHeight);
            QRectF textRect = cellRect.adjusted(m_cellPadding, 0,
                                                 -m_cellPadding, 0);
            Qt::Alignment align = Qt::AlignVCenter;
            if (c < m_alignments.size()) {
                if (m_alignments[c] == Qt::AlignCenter)
                    align |= Qt::AlignHCenter;
                else if (m_alignments[c] == Qt::AlignRight)
                    align |= Qt::AlignRight;
                else
                    align |= Qt::AlignLeft;
            } else {
                align |= Qt::AlignLeft;
            }
            QString text = (c < cells.size()) ? cells[c] : QString();
            painter->setPen(Qt::black);
            painter->drawText(textRect, align, text);
            x += m_colWidths[c];
        }
    };

    // Header row (bold)
    QFont boldFont = m_font;
    boldFont.setBold(true);
    painter->setFont(boldFont);
    drawRow(0, m_headers);
    painter->setFont(m_font);

    // Data rows
    for (int r = 0; r < m_rows.size(); ++r)
        drawRow(r + 1, m_rows[r]);

    // Grid lines
    painter->setPen(QPen(QColor(0xd0, 0xd0, 0xd0), 1));

    // Horizontal lines
    for (int r = 0; r <= 1 + m_rows.size(); ++r) {
        qreal y = r * m_rowHeight;
        painter->drawLine(QPointF(0, y), QPointF(m_totalWidth, y));
    }
    // Thicker line under header
    painter->setPen(QPen(QColor(0xa0, 0xa0, 0xa0), 2));
    painter->drawLine(QPointF(0, m_rowHeight), QPointF(m_totalWidth, m_rowHeight));

    // Vertical lines
    painter->setPen(QPen(QColor(0xd0, 0xd0, 0xd0), 1));
    qreal x = 0;
    for (int c = 0; c <= cols; ++c) {
        painter->drawLine(QPointF(x, 0), QPointF(x, m_totalHeight));
        if (c < cols) x += m_colWidths[c];
    }

    painter->restore();

    paintSelectionOverlay(painter, boundingRect());
}

QString TableBlockItem::toMarkdown() const
{
    return m_markdown;
}

void TableBlockItem::setFont(const QFont &font)
{
    m_font = font;
    prepareGeometryChange();
    computeLayout();
    update();
}

} // namespace Markoff
