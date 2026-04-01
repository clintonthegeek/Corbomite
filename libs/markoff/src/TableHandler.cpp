// SPDX-License-Identifier: GPL-3.0-or-later
#include "TableHandler.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QHeaderView>
#include <QRegularExpression>
#include <QFont>

namespace Markoff {

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

QStringList TableHandler::parseRow(const QString &line)
{
    QStringList cells;
    QString trimmed = line.trimmed();
    if (trimmed.startsWith(QLatin1Char('|')))
        trimmed = trimmed.mid(1);
    if (trimmed.endsWith(QLatin1Char('|')))
        trimmed.chop(1);
    const QStringList parts = trimmed.split(QLatin1Char('|'));
    for (const QString &part : parts)
        cells.append(part.trimmed());
    return cells;
}

Qt::Alignment TableHandler::parseAlignment(const QString &cell)
{
    QString trimmed = cell.trimmed();
    bool left = trimmed.startsWith(QLatin1Char(':'));
    bool right = trimmed.endsWith(QLatin1Char(':'));
    if (left && right) return Qt::AlignCenter;
    if (right) return Qt::AlignRight;
    return Qt::AlignLeft;
}

QList<ParsedTable> TableHandler::detectTables(QTextDocument *doc)
{
    QList<ParsedTable> tables;

    static const QRegularExpression pipeRowRe(
        QStringLiteral(R"(^\s*\|.*\|\s*$)"));
    static const QRegularExpression separatorRe(
        QStringLiteral(R"(^\s*\|[\s:]*-+[\s:]*(\|[\s:]*-+[\s:]*)*\|\s*$)"));

    QTextBlock block = doc->begin();
    while (block.isValid()) {
        if (!pipeRowRe.match(block.text()).hasMatch()) {
            block = block.next();
            continue;
        }

        QTextBlock sepBlock = block.next();
        if (!sepBlock.isValid() || !separatorRe.match(sepBlock.text()).hasMatch()) {
            block = block.next();
            continue;
        }

        ParsedTable table;
        table.firstBlock = block.blockNumber();
        table.headers = parseRow(block.text());

        QStringList sepCells = parseRow(sepBlock.text());
        for (const QString &cell : sepCells)
            table.alignments.append(parseAlignment(cell));
        while (table.alignments.size() < table.headers.size())
            table.alignments.append(Qt::AlignLeft);

        QTextBlock dataBlock = sepBlock.next();
        table.lastBlock = sepBlock.blockNumber();

        while (dataBlock.isValid() && pipeRowRe.match(dataBlock.text()).hasMatch()) {
            table.rows.append(parseRow(dataBlock.text()));
            table.lastBlock = dataBlock.blockNumber();
            dataBlock = dataBlock.next();
        }

        tables.append(table);
        block = dataBlock;
    }

    return tables;
}

// ---------------------------------------------------------------------------
// TableWidget
// ---------------------------------------------------------------------------

TableWidget::TableWidget(const ParsedTable &table, QWidget *parent)
    : QTableWidget(parent)
    , m_alignments(table.alignments)
{
    int numRows = table.rows.size();
    int numCols = table.headers.size();
    setRowCount(numRows);
    setColumnCount(numCols);

    // Headers
    setHorizontalHeaderLabels(table.headers);

    // Data
    for (int r = 0; r < numRows; ++r) {
        for (int c = 0; c < numCols && c < table.rows[r].size(); ++c) {
            auto *item = new QTableWidgetItem(table.rows[r][c]);
            if (c < table.alignments.size())
                item->setTextAlignment(table.alignments[c] | Qt::AlignVCenter);
            setItem(r, c, item);
        }
    }

    // Styling
    setFrameShape(QFrame::NoFrame);
    setGridStyle(Qt::SolidLine);
    setShowGrid(true);
    setAlternatingRowColors(false);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    verticalHeader()->setVisible(false);
    setSelectionMode(QAbstractItemView::SingleSelection);

    // Size to content
    resizeRowsToContents();

    // Style to blend with the editor
    setStyleSheet(QStringLiteral(
        "QTableWidget { background: transparent; border: 1px solid #e0e0e0; }"
        "QTableWidget::item { padding: 4px 8px; border-bottom: 1px solid #f0f0f0; }"
        "QHeaderView::section { background: #f5f5f5; border: none; "
        "  border-bottom: 2px solid #e0e0e0; padding: 4px 8px; font-weight: bold; }"
    ));
}

QString TableWidget::toMarkdown() const
{
    return TableHandler::serializeToMarkdown(
        const_cast<QTableWidget *>(static_cast<const QTableWidget *>(this)),
        m_alignments);
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

QString TableHandler::serializeToMarkdown(QTableWidget *table,
                                           const QList<Qt::Alignment> &alignments)
{
    if (!table) return {};

    int rows = table->rowCount();
    int cols = table->columnCount();

    // Collect text and compute widths
    QList<int> colWidths(cols, 3);

    // Headers
    for (int c = 0; c < cols; ++c) {
        QString h = table->horizontalHeaderItem(c)
                        ? table->horizontalHeaderItem(c)->text() : QString();
        if (h.length() > colWidths[c])
            colWidths[c] = h.length();
    }
    // Data
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            QString t = table->item(r, c) ? table->item(r, c)->text() : QString();
            if (t.length() > colWidths[c])
                colWidths[c] = t.length();
        }
    }

    QString md;

    // Header
    md += QLatin1Char('|');
    for (int c = 0; c < cols; ++c) {
        QString h = table->horizontalHeaderItem(c)
                        ? table->horizontalHeaderItem(c)->text() : QString();
        md += QLatin1Char(' ') + h.leftJustified(colWidths[c]) + QStringLiteral(" |");
    }
    md += QLatin1Char('\n');

    // Separator
    md += QLatin1Char('|');
    for (int c = 0; c < cols; ++c) {
        Qt::Alignment align = c < alignments.size() ? alignments[c] : Qt::AlignLeft;
        QString sep(colWidths[c] + 2, QLatin1Char('-'));
        if (align == Qt::AlignCenter) {
            sep[0] = QLatin1Char(':');
            sep[sep.size() - 1] = QLatin1Char(':');
        } else if (align == Qt::AlignRight) {
            sep[sep.size() - 1] = QLatin1Char(':');
        }
        md += sep + QLatin1Char('|');
    }
    md += QLatin1Char('\n');

    // Data
    for (int r = 0; r < rows; ++r) {
        md += QLatin1Char('|');
        for (int c = 0; c < cols; ++c) {
            QString t = table->item(r, c) ? table->item(r, c)->text() : QString();
            md += QLatin1Char(' ') + t.leftJustified(colWidths[c]) + QStringLiteral(" |");
        }
        md += QLatin1Char('\n');
    }

    return md;
}

} // namespace Markoff
