// SPDX-License-Identifier: GPL-3.0-or-later
#include "TableHandler.h"

#include <QTextCursor>
#include <QTextTableFormat>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QRegularExpression>

namespace Markoff {

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

QStringList TableHandler::parseRow(const QString &line)
{
    QStringList cells;
    QString trimmed = line.trimmed();

    // Strip leading and trailing pipes
    if (trimmed.startsWith(QLatin1Char('|')))
        trimmed = trimmed.mid(1);
    if (trimmed.endsWith(QLatin1Char('|')))
        trimmed.chop(1);

    // Split on pipes
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
    return Qt::AlignLeft; // default
}

QList<ParsedTable> TableHandler::detectTables(QTextDocument *doc)
{
    QList<ParsedTable> tables;

    // A pipe table is:
    // 1. A header row with | separators
    // 2. A separator row with |---| (with optional : for alignment)
    // 3. One or more data rows with | separators
    static const QRegularExpression pipeRowRe(
        QStringLiteral(R"(^\s*\|.*\|\s*$)"));
    static const QRegularExpression separatorRe(
        QStringLiteral(R"(^\s*\|[\s:]*-+[\s:]*(\|[\s:]*-+[\s:]*)*\|\s*$)"));

    QTextBlock block = doc->begin();
    while (block.isValid()) {
        // Look for a potential header row
        if (!pipeRowRe.match(block.text()).hasMatch()) {
            block = block.next();
            continue;
        }

        // Check if the next line is a separator row
        QTextBlock sepBlock = block.next();
        if (!sepBlock.isValid() || !separatorRe.match(sepBlock.text()).hasMatch()) {
            block = block.next();
            continue;
        }

        // Found a table! Parse it.
        ParsedTable table;
        table.firstBlock = block.blockNumber();
        table.headers = parseRow(block.text());

        // Parse alignment from separator
        QStringList sepCells = parseRow(sepBlock.text());
        for (const QString &cell : sepCells)
            table.alignments.append(parseAlignment(cell));

        // Ensure alignment count matches header count
        while (table.alignments.size() < table.headers.size())
            table.alignments.append(Qt::AlignLeft);

        // Parse data rows
        QTextBlock dataBlock = sepBlock.next();
        table.lastBlock = sepBlock.blockNumber();

        while (dataBlock.isValid() && pipeRowRe.match(dataBlock.text()).hasMatch()) {
            table.rows.append(parseRow(dataBlock.text()));
            table.lastBlock = dataBlock.blockNumber();
            dataBlock = dataBlock.next();
        }

        tables.append(table);

        // Skip past the table
        block = dataBlock;
    }

    return tables;
}

// ---------------------------------------------------------------------------
// Conversion to QTextTable
// ---------------------------------------------------------------------------

QTextTable *TableHandler::convertToQTextTable(QTextDocument *doc,
                                               const ParsedTable &table)
{
    // Find the text range to replace
    QTextBlock firstBlock = doc->findBlockByNumber(table.firstBlock);
    QTextBlock lastBlock = doc->findBlockByNumber(table.lastBlock);
    if (!firstBlock.isValid() || !lastBlock.isValid())
        return nullptr;

    int startPos = firstBlock.position();
    int endPos = lastBlock.position() + lastBlock.length();

    // Select and delete the pipe text
    QTextCursor cursor(doc);
    cursor.setPosition(startPos);
    cursor.setPosition(endPos - 1, QTextCursor::KeepAnchor); // -1 to not eat next block's newline
    cursor.removeSelectedText();

    // Insert the QTextTable
    int numRows = 1 + table.rows.size(); // header + data rows
    int numCols = table.headers.size();
    if (numRows < 1 || numCols < 1)
        return nullptr;

    QTextTableFormat tableFormat;
    tableFormat.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
    tableFormat.setBorder(1);
    tableFormat.setBorderBrush(QColor(0xcc, 0xcc, 0xcc));
    tableFormat.setCellSpacing(0);
    tableFormat.setCellPadding(6);
    tableFormat.setWidth(QTextLength(QTextLength::PercentageLength, 100));

    QTextTable *textTable = cursor.insertTable(numRows, numCols, tableFormat);

    // Header formatting
    QTextCharFormat headerFormat;
    headerFormat.setFontWeight(QFont::Bold);

    // Fill header cells
    for (int c = 0; c < numCols; ++c) {
        QTextTableCell cell = textTable->cellAt(0, c);
        QTextCursor cellCursor = cell.firstCursorPosition();
        cellCursor.insertText(c < table.headers.size() ? table.headers[c] : QString());
        cellCursor.select(QTextCursor::BlockUnderCursor);
        cellCursor.mergeCharFormat(headerFormat);

        // Apply alignment
        if (c < table.alignments.size()) {
            QTextBlockFormat blockFmt;
            blockFmt.setAlignment(table.alignments[c]);
            cellCursor.mergeBlockFormat(blockFmt);
        }
    }

    // Fill data cells
    for (int r = 0; r < table.rows.size(); ++r) {
        const QStringList &row = table.rows[r];
        for (int c = 0; c < numCols; ++c) {
            QTextTableCell cell = textTable->cellAt(r + 1, c);
            QTextCursor cellCursor = cell.firstCursorPosition();
            cellCursor.insertText(c < row.size() ? row[c] : QString());

            if (c < table.alignments.size()) {
                QTextBlockFormat blockFmt;
                blockFmt.setAlignment(table.alignments[c]);
                cellCursor.mergeBlockFormat(blockFmt);
            }
        }
    }

    return textTable;
}

// ---------------------------------------------------------------------------
// Serialization back to markdown
// ---------------------------------------------------------------------------

QString TableHandler::serializeToMarkdown(QTextTable *table,
                                           const QList<Qt::Alignment> &alignments)
{
    if (!table) return {};

    int rows = table->rows();
    int cols = table->columns();

    // Collect all cell text and compute column widths
    QList<QStringList> allRows;
    QList<int> colWidths(cols, 3); // minimum width 3 (for ---)

    for (int r = 0; r < rows; ++r) {
        QStringList row;
        for (int c = 0; c < cols; ++c) {
            QTextTableCell cell = table->cellAt(r, c);
            QString text;
            QTextCursor cur = cell.firstCursorPosition();
            cur.setPosition(cell.lastCursorPosition().position(), QTextCursor::KeepAnchor);
            text = cur.selectedText().trimmed();
            row.append(text);
            if (text.length() > colWidths[c])
                colWidths[c] = text.length();
        }
        allRows.append(row);
    }

    QString md;

    // Header row
    md += QLatin1Char('|');
    for (int c = 0; c < cols; ++c) {
        md += QLatin1Char(' ');
        md += allRows[0][c].leftJustified(colWidths[c]);
        md += QStringLiteral(" |");
    }
    md += QLatin1Char('\n');

    // Separator row
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

    // Data rows
    for (int r = 1; r < rows; ++r) {
        md += QLatin1Char('|');
        for (int c = 0; c < cols; ++c) {
            md += QLatin1Char(' ');
            md += allRows[r][c].leftJustified(colWidths[c]);
            md += QStringLiteral(" |");
        }
        md += QLatin1Char('\n');
    }

    return md;
}

} // namespace Markoff
