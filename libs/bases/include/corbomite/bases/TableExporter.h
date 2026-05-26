// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesQueryResult.h"
#include "PropertyId.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>

namespace Corbomite::Bases {

/// Pure, widget-free serializer for the current query result. Emits the flat
/// row set in the result's current sort order (grouping ignored — matches
/// Obsidian's export). Columns come from BasesQueryResult::properties(); cell
/// text is `entry->getValue(prop)->toString()` (null value -> empty string).
///
/// Intended as a stack-allocated temporary: the referenced BasesQueryResult
/// must outlive the exporter (the member is a const reference, not a copy).
class TableExporter
{
public:
    using DisplayNameFn = std::function<QString(const PropertyId &)>;

    /// `displayName` maps a column's PropertyId to its header text. If null,
    /// the PropertyId's own toString() is used.
    TableExporter(const BasesQueryResult &result, DisplayNameFn displayName = {});

    QString toCsv() const;               ///< RFC-4180, CRLF line endings.
    QString toTsv() const;               ///< tab-separated, tabs/newlines sanitized.
    QString toMarkdown() const;          ///< GFM pipe table.
    QString toHtml() const;              ///< <table> with thead/tbody.
    QByteArray toObsidianTable() const;  ///< JSON {"rows":[[...]],"alignment":[...]}.

private:
    QStringList headerRow() const;            ///< column header strings.
    QVector<QStringList> bodyRows() const;    ///< one QStringList of cell text per row.

    const BasesQueryResult &m_result;
    DisplayNameFn m_displayName;
};

}  // namespace Corbomite::Bases
