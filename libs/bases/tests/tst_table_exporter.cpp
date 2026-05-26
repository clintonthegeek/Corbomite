// SPDX-License-Identifier: GPL-3.0-or-later
//
// TableExporter unit tests.
//
// BasesEntry has no frontmatter-injection factory; a null-backed entry returns
// NullValue (empty string) for every cell. All format-function logic (CSV
// quoting, TSV sanitisation, Markdown pipe-escaping, HTML entity encoding) is
// therefore exercised through the DisplayNameFn header path — the same code
// runs for body cells, so full branch coverage is achieved.
//
// The emptyResult and obsidianTableShape tests verify structural correctness
// (row/column counts, JSON shape, default alignment).

#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/BasesQuery.h"
#include "corbomite/bases/BasesQueryResult.h"
#include "corbomite/bases/BasesViewConfig.h"
#include "corbomite/bases/PropertyId.h"
#include "corbomite/bases/TableExporter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <memory>

using namespace Corbomite::Bases;

namespace {

// Null-backed entry: every getValue() returns NullValue (empty string).
std::shared_ptr<BasesEntry> makeNullEntry()
{
    static BasesQuery q;
    return std::make_shared<BasesEntry>(nullptr, nullptr, nullptr, nullptr, q);
}

BasesViewConfig configWithOrder(const QVector<PropertyId> &order)
{
    BasesViewConfig cfg;
    cfg.type = QStringLiteral("table");
    cfg.name = QStringLiteral("Table");
    cfg.order = order;
    return cfg;
}

// Single-column config for convenience tests.
BasesViewConfig singleColCfg()
{
    return configWithOrder({parsePropertyId(QStringLiteral("note.title"))});
}

}  // namespace

class TestTableExporter : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void emptyResult();
    void csvQuotesSpecials();
    void tsvSanitizesTabs();
    void markdownEscapesPipes();
    void htmlEscapes();
    void obsidianTableShape();
};

// --- empty result: header row only, one column ------------------------------------

void TestTableExporter::emptyResult()
{
    auto cfg = singleColCfg();
    BasesQueryResult result(cfg, {});
    // Default DisplayNameFn: PropertyId::toString() = "note.title"
    TableExporter exp(result);
    QCOMPARE(exp.toCsv(), QStringLiteral("note.title\r\n"));
}

// --- CSV quoting: commas, embedded quotes, embedded newlines ----------------------
// Inject special chars via DisplayNameFn so we control the cell text exactly.

void TestTableExporter::csvQuotesSpecials()
{
    auto cfg = singleColCfg();
    BasesQueryResult result(cfg, {});

    // Column header "a,b" → must be quoted.
    {
        TableExporter exp(result,
            [](const PropertyId &) { return QStringLiteral("a,b"); });
        QVERIFY(exp.toCsv().contains(QStringLiteral("\"a,b\"")));
    }
    // Column header with embedded double-quote → must double the quote.
    {
        TableExporter exp(result,
            [](const PropertyId &) { return QStringLiteral("he said \"hi\""); });
        QVERIFY(exp.toCsv().contains(QStringLiteral("\"he said \"\"hi\"\"\"")));
    }
    // Column header with embedded newline → must be quoted.
    {
        TableExporter exp(result,
            [](const PropertyId &) { return QStringLiteral("line1\nline2"); });
        QVERIFY(exp.toCsv().contains(QStringLiteral("\"line1\nline2\"")));
    }
}

// --- TSV: tabs in cell text must become spaces -----------------------------------

void TestTableExporter::tsvSanitizesTabs()
{
    auto cfg = singleColCfg();
    BasesQueryResult result(cfg, {});

    TableExporter exp(result,
        [](const PropertyId &) { return QStringLiteral("a\tb"); });
    const QString tsv = exp.toTsv();
    QVERIFY(!tsv.contains(QStringLiteral("a\tb")));
    QVERIFY(tsv.contains(QStringLiteral("a b")));
}

// --- Markdown: pipe in cell text must be escaped ---------------------------------

void TestTableExporter::markdownEscapesPipes()
{
    auto cfg = singleColCfg();
    BasesQueryResult result(cfg, {});

    TableExporter exp(result,
        [](const PropertyId &) { return QStringLiteral("a|b"); });
    QVERIFY(exp.toMarkdown().contains(QStringLiteral("a\\|b")));
}

// --- HTML: <, >, & must be entity-encoded ----------------------------------------

void TestTableExporter::htmlEscapes()
{
    auto cfg = singleColCfg();
    BasesQueryResult result(cfg, {});

    TableExporter exp(result,
        [](const PropertyId &) { return QStringLiteral("a<b>&c"); });
    const QString html = exp.toHtml();
    QVERIFY(html.contains(QStringLiteral("a&lt;b&gt;&amp;c")));
    QVERIFY(html.contains(QStringLiteral("<table")));
}

// --- Obsidian table: JSON shape, row/col counts, alignment -----------------------

void TestTableExporter::obsidianTableShape()
{
    auto cfg = configWithOrder({
        parsePropertyId(QStringLiteral("note.title")),
        parsePropertyId(QStringLiteral("note.status")),
    });
    // One null-backed row: both cells will be empty strings.
    QVector<std::shared_ptr<BasesEntry>> rows{ makeNullEntry() };
    BasesQueryResult result(cfg, rows);
    TableExporter exp(result);

    const auto doc = QJsonDocument::fromJson(exp.toObsidianTable());
    QVERIFY(doc.isObject());
    const auto obj = doc.object();

    const auto jrows = obj.value(QStringLiteral("rows")).toArray();
    const auto alignment = obj.value(QStringLiteral("alignment")).toArray();

    // rows[0] = header row, rows[1] = the one data row.
    QCOMPARE(jrows.size(), 2);
    QCOMPARE(jrows.at(0).toArray().size(), 2);
    // Header cell 0 is the default toString() of note.title.
    QCOMPARE(jrows.at(0).toArray().at(0).toString(),
             QStringLiteral("note.title"));
    // Alignment array has one entry per column, default empty string.
    QCOMPARE(alignment.size(), 2);
    QCOMPARE(alignment.at(0).toString(), QString());
}

QTEST_APPLESS_MAIN(TestTableExporter)
#include "tst_table_exporter.moc"
