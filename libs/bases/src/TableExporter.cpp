// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/TableExporter.h"

#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/Values.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Corbomite::Bases {

namespace {
QString cellText(const std::shared_ptr<BasesEntry> &entry, const PropertyId &pid)
{
    if (!entry) return {};
    ValuePtr v = entry->getValue(pid);
    return v ? v->toString() : QString{};
}

QString csvField(const QString &s)
{
    const bool needsQuote = s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"'))
                            || s.contains(QLatin1Char('\n')) || s.contains(QLatin1Char('\r'));
    if (!needsQuote) return s;
    QString q = s;
    q.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + q + QLatin1Char('"');
}

QString tsvField(const QString &s)
{
    QString q = s;
    q.replace(QLatin1Char('\t'), QLatin1Char(' '));
    q.replace(QLatin1Char('\n'), QLatin1Char(' '));
    q.replace(QLatin1Char('\r'), QLatin1Char(' '));
    return q;
}

QString mdField(const QString &s)
{
    QString q = s;
    q.replace(QLatin1Char('|'), QStringLiteral("\\|"));
    q.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return q;
}

QString htmlEscape(const QString &s)
{
    QString q = s;
    q.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    q.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    q.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    return q;
}
}  // namespace

TableExporter::TableExporter(const BasesQueryResult &result, DisplayNameFn displayName)
    : m_result(result), m_displayName(std::move(displayName))
{
}

QStringList TableExporter::headerRow() const
{
    QStringList out;
    for (const PropertyId &pid : m_result.properties())
        out << (m_displayName ? m_displayName(pid) : pid.toString());
    return out;
}

QVector<QStringList> TableExporter::bodyRows() const
{
    const QVector<PropertyId> &cols = m_result.properties();
    QVector<QStringList> out;
    out.reserve(m_result.rows().size());
    for (const auto &entry : m_result.rows()) {
        QStringList cells;
        cells.reserve(cols.size());
        for (const PropertyId &pid : cols)
            cells << cellText(entry, pid);
        out << cells;
    }
    return out;
}

QString TableExporter::toCsv() const
{
    QStringList lines;
    QStringList header;
    for (const QString &h : headerRow()) header << csvField(h);
    lines << header.join(QLatin1Char(','));
    for (const QStringList &row : bodyRows()) {
        QStringList cells;
        for (const QString &c : row) cells << csvField(c);
        lines << cells.join(QLatin1Char(','));
    }
    return lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
}

QString TableExporter::toTsv() const
{
    QStringList lines;
    QStringList header;
    for (const QString &h : headerRow()) header << tsvField(h);
    lines << header.join(QLatin1Char('\t'));
    for (const QStringList &row : bodyRows()) {
        QStringList cells;
        for (const QString &c : row) cells << tsvField(c);
        lines << cells.join(QLatin1Char('\t'));
    }
    return lines.join(QLatin1Char('\n'));
}

QString TableExporter::toMarkdown() const
{
    const QStringList header = headerRow();
    QStringList lines;
    QStringList head;
    for (const QString &h : header) head << mdField(h);
    lines << QStringLiteral("| ") + head.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    QStringList sep;
    for (int i = 0; i < header.size(); ++i) sep << QStringLiteral("---");
    lines << QStringLiteral("| ") + sep.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    for (const QStringList &row : bodyRows()) {
        QStringList cells;
        for (const QString &c : row) cells << mdField(c);
        lines << QStringLiteral("| ") + cells.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    }
    return lines.join(QLatin1Char('\n'));
}

QString TableExporter::toHtml() const
{
    QString out = QStringLiteral("<table>\n<thead>\n<tr>");
    for (const QString &h : headerRow())
        out += QStringLiteral("<th>") + htmlEscape(h) + QStringLiteral("</th>");
    out += QStringLiteral("</tr>\n</thead>\n<tbody>\n");
    for (const QStringList &row : bodyRows()) {
        out += QStringLiteral("<tr>");
        for (const QString &c : row)
            out += QStringLiteral("<td>") + htmlEscape(c) + QStringLiteral("</td>");
        out += QStringLiteral("</tr>\n");
    }
    out += QStringLiteral("</tbody>\n</table>");
    return out;
}

QByteArray TableExporter::toObsidianTable() const
{
    const QStringList hdr = headerRow();
    QJsonArray rows;
    QJsonArray headerArr;
    for (const QString &h : hdr) headerArr.append(h);
    rows.append(headerArr);
    for (const QStringList &row : bodyRows()) {
        QJsonArray r;
        for (const QString &c : row) r.append(c);
        rows.append(r);
    }
    QJsonArray alignment;
    for (int i = 0; i < hdr.size(); ++i) alignment.append(QString{});
    QJsonObject obj;
    obj.insert(QStringLiteral("rows"), rows);
    obj.insert(QStringLiteral("alignment"), alignment);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

}  // namespace Corbomite::Bases
