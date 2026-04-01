// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_TABLEHANDLER_H
#define MARKOFF_TABLEHANDLER_H

#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QList>

class QTextDocument;
class QWidget;

namespace Markoff {

/// Parsed representation of a markdown pipe table
struct ParsedTable {
    QStringList headers;
    QList<Qt::Alignment> alignments;
    QList<QStringList> rows;
    int firstBlock = -1;
    int lastBlock = -1;
};

/// A QTableWidget styled for embedding in the Markoff editor.
class TableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit TableWidget(const ParsedTable &table, QWidget *parent = nullptr);

    /// Serialize the table back to pipe-delimited markdown
    QString toMarkdown() const;

    QList<Qt::Alignment> alignments() const { return m_alignments; }

private:
    QList<Qt::Alignment> m_alignments;
};

/// Handles detection and parsing of markdown tables.
class TableHandler {
public:
    static QList<ParsedTable> detectTables(QTextDocument *doc);
    static QString serializeToMarkdown(QTableWidget *table,
                                        const QList<Qt::Alignment> &alignments);
private:
    static QStringList parseRow(const QString &line);
    static Qt::Alignment parseAlignment(const QString &cell);
};

} // namespace Markoff

#endif // MARKOFF_TABLEHANDLER_H
