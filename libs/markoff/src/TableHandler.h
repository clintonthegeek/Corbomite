// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_TABLEHANDLER_H
#define MARKOFF_TABLEHANDLER_H

#include <QString>
#include <QStringList>
#include <QTextDocument>
#include <QTextTable>
#include <QList>

namespace Markoff {

/// Parsed representation of a markdown pipe table
struct ParsedTable {
    QStringList headers;
    QList<Qt::Alignment> alignments;
    QList<QStringList> rows;
    int firstBlock = -1;   // first QTextBlock of the pipe text
    int lastBlock = -1;    // last QTextBlock of the pipe text
};

/// Handles detection, conversion, and serialization of markdown tables.
class TableHandler {
public:
    /// Detect pipe tables in the document text. Returns parsed tables
    /// with block ranges (before conversion to QTextTable).
    static QList<ParsedTable> detectTables(QTextDocument *doc);

    /// Convert a parsed table to a QTextTable in the document,
    /// replacing the pipe-delimited text blocks.
    static QTextTable *convertToQTextTable(QTextDocument *doc,
                                            const ParsedTable &table);

    /// Serialize a QTextTable back to pipe-delimited markdown.
    static QString serializeToMarkdown(QTextTable *table,
                                        const QList<Qt::Alignment> &alignments);

private:
    /// Parse a single pipe-delimited row into cell strings
    static QStringList parseRow(const QString &line);

    /// Parse alignment from separator row (|---|, |:---:|, |---:|)
    static Qt::Alignment parseAlignment(const QString &cell);
};

} // namespace Markoff

#endif // MARKOFF_TABLEHANDLER_H
