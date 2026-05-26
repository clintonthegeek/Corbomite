# Cluster D.4b — Bases export/copy + +New entry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Bases' "Results menu" (Copy table in 4 MIME formats + Export CSV) and a "+New entry" toolbar button that creates a filter-satisfying note, opens it, and prompts for a rename.

**Architecture:** Two pure, widget-free helpers (`TableExporter`, `NewItemSeed`) carry the correctness logic under full TDD; thin `BasesView` widget wiring reuses the `FileManager` service and the `m_openInNewTab`/`m_promptRename` callbacks already injected in D.4a. Matches the D.x split used throughout this cluster.

**Tech Stack:** C++20, Qt6 Widgets, KDE Frameworks (`i18n`), QtTest, CMake. Library `Corbomite::Bases` (`libs/bases`).

---

## Background the implementer needs

- **Build/run/test** (from repo root):
  ```bash
  cmake --build --preset dev -j 10
  cd build-dev && ctest --output-on-failure -j 10 -R tst_bases
  ```
  Run one test: `ctest --output-on-failure -R tst_table_exporter` from `build-dev/`.
- **Cell display text** is `entry->getValue(prop)` (returns `ValuePtr`) then `->toString()`. A null `ValuePtr` means render as empty string. This is exactly what `BasesTreeModel` does at `libs/bases/src/BasesTreeModel.cpp:157`.
- **Column set** for a result is `BasesQueryResult::properties()` (`const QVector<PropertyId>&`) — the union of the view's configured order plus `note.*` keys seen in rows. Rows are `BasesQueryResult::rows()` (`const QVector<std::shared_ptr<BasesEntry>>&`), already sorted/limited.
- **Display name** for a `PropertyId` is resolved by the existing private helper `BasesView::displayNameFor(const PropertyId&)`. Pure helpers take a `std::function<QString(const PropertyId&)>` so they stay decoupled.
- **Filter tree types** (`libs/bases/include/corbomite/bases/FilterTree.h`): `FilterNode` (abstract) → `FilterRule` (holds a `Formula`, accessor `rule()`) and `FilterConjunction` (`conj()` returns `Conj::And|Or|Not`, `children()` returns `const QVector<FilterPtr>&`). `FilterPtr = std::shared_ptr<FilterNode>`.
- **Formula AST** (`libs/bases/include/corbomite/bases/Ast.h`): `Expr` base with subclasses `LiteralExpr` (`ValuePtr value`), `IdentExpr` (`QString name`), `BinaryExpr` (`BinOp op; ExprPtr left, right`), `MemberExpr` (`ExprPtr object; QString member`), `UnaryExpr`, `CallExpr`, etc. `BinOp::Eq` is `==`; `BinOp::AndAnd` is `&&`. `Formula`'s AST is currently private — Task 2 exposes it.
- **File creation**: `FileManager::createMarkdownNote(const QString &name, const QString &folder)` returns `TFile*` (auto-suffixes on collision, empty folder = vault root, nullptr on failure). Frontmatter is written via `m_fm->processFrontMatter(file, [&](QVariantMap &fm){ ... })` — synchronous, same path as `BasesTreeModel.cpp:183`.
- **Template frontmatter**: `query.newItemTemplate` is an optional *path string*, not inline frontmatter. Resolve it via `m_cache->getFileCache(path)` → `std::optional<CachedMetadata>`; `CachedMetadata::frontmatter` is `std::optional<QJsonObject>`.
- **TFile path**: a created `TFile*` exposes its vault-relative path via `->path()` (used elsewhere in `BasesView` for `m_openInNewTab`).
- **i18n**: wrap every user-visible string in `i18n("…")`. **Icons**: `QIcon::fromTheme(...)`.

---

## File structure

| File | Responsibility |
|---|---|
| `libs/bases/include/corbomite/bases/TableExporter.h` (create) | Pure serializer interface. |
| `libs/bases/src/TableExporter.cpp` (create) | CSV/TSV/Markdown/HTML/`obsidian/table` serialization. |
| `libs/bases/tests/tst_table_exporter.cpp` (create) | Exporter unit tests. |
| `libs/bases/include/corbomite/bases/NewItemSeed.h` (create) | Pure seed-computation interface. |
| `libs/bases/src/NewItemSeed.cpp` (create) | Filter-tree walk for equality seeds + template merge. |
| `libs/bases/tests/tst_new_item_seed.cpp` (create) | Seed unit tests. |
| `libs/bases/include/corbomite/bases/Formula.h` (modify) | Add public `const Expr *ast() const`. |
| `libs/bases/src/BasesView.cpp` + `.h` (modify) | Results-menu button + "+New" button wiring. |
| `libs/bases/CMakeLists.txt` (modify) | Register the two new sources. |
| `libs/bases/tests/CMakeLists.txt` (modify) | Register the two new test executables. |

---

## Task 1: `TableExporter` pure helper

**Files:**
- Create: `libs/bases/include/corbomite/bases/TableExporter.h`
- Create: `libs/bases/src/TableExporter.cpp`
- Create: `libs/bases/tests/tst_table_exporter.cpp`
- Modify: `libs/bases/CMakeLists.txt`, `libs/bases/tests/CMakeLists.txt`

- [ ] **Step 1: Create the header**

`libs/bases/include/corbomite/bases/TableExporter.h`:
```cpp
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
```

- [ ] **Step 2: Write the failing tests**

`libs/bases/tests/tst_table_exporter.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesEntry.h"
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
// Build a BasesEntry from a frontmatter object. BasesEntry's public ctor takes
// a TFile* and frontmatter; tests use the frontmatter-only convenience already
// exercised in tst_basesentry.cpp — mirror that construction here.
std::shared_ptr<BasesEntry> makeEntry(const QJsonObject &fm)
{
    return BasesEntry::fromFrontmatter(fm);  // see tst_basesentry.cpp for this helper's usage
}

BasesViewConfig configWithOrder(const QVector<PropertyId> &order)
{
    BasesViewConfig cfg;
    cfg.type = QStringLiteral("table");
    cfg.name = QStringLiteral("Table");
    cfg.order = order;
    return cfg;
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

void TestTableExporter::emptyResult()
{
    auto cfg = configWithOrder({PropertyId{QStringLiteral("note.title")}});
    BasesQueryResult result(cfg, {});
    TableExporter exp(result);
    // Header row only, no body rows.
    QCOMPARE(exp.toCsv(), QStringLiteral("title\r\n"));
}

void TestTableExporter::csvQuotesSpecials()
{
    auto cfg = configWithOrder({PropertyId{QStringLiteral("note.title")}});
    QVector<std::shared_ptr<BasesEntry>> rows{
        makeEntry(QJsonObject{{"title", "a,b"}}),
        makeEntry(QJsonObject{{"title", "he said \"hi\""}}),
        makeEntry(QJsonObject{{"title", "line1\nline2"}}),
    };
    BasesQueryResult result(cfg, rows);
    TableExporter exp(result);
    const QString csv = exp.toCsv();
    QVERIFY(csv.contains(QStringLiteral("\"a,b\"")));
    QVERIFY(csv.contains(QStringLiteral("\"he said \"\"hi\"\"\"")));
    QVERIFY(csv.contains(QStringLiteral("\"line1\nline2\"")));
}

void TestTableExporter::tsvSanitizesTabs()
{
    auto cfg = configWithOrder({PropertyId{QStringLiteral("note.title")}});
    QVector<std::shared_ptr<BasesEntry>> rows{
        makeEntry(QJsonObject{{"title", "a\tb"}}),
    };
    BasesQueryResult result(cfg, rows);
    TableExporter exp(result);
    const QString tsv = exp.toTsv();
    // No literal tab inside a cell value (only the column separator structure).
    QVERIFY(!tsv.contains(QStringLiteral("a\tb")));
    QVERIFY(tsv.contains(QStringLiteral("a b")));
}

void TestTableExporter::markdownEscapesPipes()
{
    auto cfg = configWithOrder({PropertyId{QStringLiteral("note.title")}});
    QVector<std::shared_ptr<BasesEntry>> rows{
        makeEntry(QJsonObject{{"title", "a|b"}}),
    };
    BasesQueryResult result(cfg, rows);
    TableExporter exp(result);
    QVERIFY(exp.toMarkdown().contains(QStringLiteral("a\\|b")));
}

void TestTableExporter::htmlEscapes()
{
    auto cfg = configWithOrder({PropertyId{QStringLiteral("note.title")}});
    QVector<std::shared_ptr<BasesEntry>> rows{
        makeEntry(QJsonObject{{"title", "a<b>&c"}}),
    };
    BasesQueryResult result(cfg, rows);
    TableExporter exp(result);
    const QString html = exp.toHtml();
    QVERIFY(html.contains(QStringLiteral("a&lt;b&gt;&amp;c")));
    QVERIFY(html.contains(QStringLiteral("<table")));
}

void TestTableExporter::obsidianTableShape()
{
    auto cfg = configWithOrder({
        PropertyId{QStringLiteral("note.title")},
        PropertyId{QStringLiteral("note.status")},
    });
    QVector<std::shared_ptr<BasesEntry>> rows{
        makeEntry(QJsonObject{{"title", "T1"}, {"status", "active"}}),
    };
    BasesQueryResult result(cfg, rows);
    TableExporter exp(result);
    const auto doc = QJsonDocument::fromJson(exp.toObsidianTable());
    QVERIFY(doc.isObject());
    const auto obj = doc.object();
    const auto jrows = obj.value(QStringLiteral("rows")).toArray();
    const auto alignment = obj.value(QStringLiteral("alignment")).toArray();
    QCOMPARE(jrows.size(), 2);                       // header + 1 body row
    QCOMPARE(jrows.at(0).toArray().size(), 2);       // 2 columns
    QCOMPARE(jrows.at(0).toArray().at(0).toString(), QStringLiteral("title"));
    QCOMPARE(alignment.size(), 2);                   // one entry per column
    QCOMPARE(alignment.at(0).toString(), QString());  // left = ""
}

QTEST_MAIN(TestTableExporter)
#include "tst_table_exporter.moc"
```

> **Note on `makeEntry`:** before writing the implementation, open `libs/bases/tests/tst_basesentry.cpp` and copy its exact construction idiom for a frontmatter-only `BasesEntry`. If the factory is spelled differently than `BasesEntry::fromFrontmatter`, use whatever that test uses and adjust the `makeEntry` helper above to match. Do **not** invent a new BasesEntry constructor.

- [ ] **Step 3: Register the test in CMake, build, verify it fails**

Add to `libs/bases/tests/CMakeLists.txt` (alongside the other `add_executable` blocks):
```cmake
add_executable(tst_table_exporter tst_table_exporter.cpp)
add_test(NAME tst_table_exporter COMMAND tst_table_exporter)
target_link_libraries(tst_table_exporter PRIVATE Qt6::Test Corbomite::Bases)
```
Add the source to `libs/bases/CMakeLists.txt` library SOURCES list (near `src/SortCycle.cpp`):
```cmake
    src/TableExporter.cpp
    include/corbomite/bases/TableExporter.h
```
Create a stub `libs/bases/src/TableExporter.cpp` with the class methods returning `{}` so it links, then:
Run: `cmake --build --preset dev -j 10 && (cd build-dev && ctest --output-on-failure -R tst_table_exporter)`
Expected: builds, test FAILS (empty returns don't match assertions).

- [ ] **Step 4: Implement `TableExporter.cpp`**

`libs/bases/src/TableExporter.cpp`:
```cpp
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
    QJsonArray rows;
    QJsonArray headerArr;
    for (const QString &h : headerRow()) headerArr.append(h);
    rows.append(headerArr);
    for (const QStringList &row : bodyRows()) {
        QJsonArray r;
        for (const QString &c : row) r.append(c);
        rows.append(r);
    }
    QJsonArray alignment;
    for (int i = 0; i < headerRow().size(); ++i) alignment.append(QString{});
    QJsonObject obj;
    obj.insert(QStringLiteral("rows"), rows);
    obj.insert(QStringLiteral("alignment"), alignment);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

}  // namespace Corbomite::Bases
```

- [ ] **Step 5: Build and run the test**

Run: `cmake --build --preset dev -j 10 && (cd build-dev && ctest --output-on-failure -R tst_table_exporter)`
Expected: PASS (all 6 slots).

- [ ] **Step 6: Commit**

```bash
git add libs/bases/include/corbomite/bases/TableExporter.h libs/bases/src/TableExporter.cpp \
        libs/bases/tests/tst_table_exporter.cpp libs/bases/CMakeLists.txt libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): TableExporter — CSV/TSV/Markdown/HTML/obsidian-table serialization"
```

---

## Task 2: Expose Formula AST + `NewItemSeed` pure helper

**Files:**
- Modify: `libs/bases/include/corbomite/bases/Formula.h`
- Create: `libs/bases/include/corbomite/bases/NewItemSeed.h`
- Create: `libs/bases/src/NewItemSeed.cpp`
- Create: `libs/bases/tests/tst_new_item_seed.cpp`
- Modify: `libs/bases/CMakeLists.txt`, `libs/bases/tests/CMakeLists.txt`

- [ ] **Step 1: Expose the AST on `Formula`**

In `libs/bases/include/corbomite/bases/Formula.h`, add a public accessor after `parseError()`:
```cpp
    /// Root of the parsed expression tree (nullptr only if never parsed).
    /// Exposed for static analysis (e.g. NewItemSeed extracting equality
    /// constraints); the evaluator still drives execution.
    const Expr *ast() const { return m_ast.get(); }
```
(`Expr` is already visible via the `#include "Ast.h"` at the top of `Formula.h`.)

- [ ] **Step 2: Create the `NewItemSeed` header**

`libs/bases/include/corbomite/bases/NewItemSeed.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FilterTree.h"

#include <QPair>
#include <QString>
#include <QVector>

namespace Corbomite::Bases {

/// Computes the frontmatter to seed into a new note so it satisfies a view's
/// filter. Pure: no vault, no widgets. Walks the filter tree collecting
/// top-level AND-context equality constraints (`prop == literal`); OR /
/// negation / non-equality subtrees and `file.*` properties contribute
/// nothing. Equality-derived values override colliding template values.
namespace NewItemSeed {

using SeedList = QVector<QPair<QString, QString>>;  ///< ordered: template keys first, then equality keys.

/// `templateProps` is the (already resolved) frontmatter from the view's
/// newItemTemplate, in source order; empty if no template. `filter` may be
/// null (no filter → template verbatim).
SeedList compute(const FilterPtr &filter, const SeedList &templateProps);

}  // namespace NewItemSeed
}  // namespace Corbomite::Bases
```

- [ ] **Step 3: Write the failing tests**

`libs/bases/tests/tst_new_item_seed.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterTree.h"
#include "corbomite/bases/Formula.h"
#include "corbomite/bases/NewItemSeed.h"

#include <QtTest>

using namespace Corbomite::Bases;

namespace {
FilterPtr rule(const QString &src) { return std::make_shared<FilterRule>(Formula(src)); }
FilterPtr conj(Conj c, QVector<FilterPtr> kids) { return std::make_shared<FilterConjunction>(c, std::move(kids)); }

bool hasPair(const NewItemSeed::SeedList &s, const QString &k, const QString &v)
{
    for (const auto &p : s) if (p.first == k && p.second == v) return true;
    return false;
}
bool hasKey(const NewItemSeed::SeedList &s, const QString &k)
{
    for (const auto &p : s) if (p.first == k) return true;
    return false;
}
}  // namespace

class TestNewItemSeed : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void noFilterNoTemplate();
    void singleEquality();
    void andChain();
    void orSkipped();
    void negationSkipped();
    void inequalitySkipped();
    void filePropertySkipped();
    void templateVerbatim();
    void equalityOverridesTemplate();
};

void TestNewItemSeed::noFilterNoTemplate()
{
    QVERIFY(NewItemSeed::compute(nullptr, {}).isEmpty());
}

void TestNewItemSeed::singleEquality()
{
    auto s = NewItemSeed::compute(rule(QStringLiteral("status == \"active\"")), {});
    QVERIFY(hasPair(s, QStringLiteral("status"), QStringLiteral("active")));
}

void TestNewItemSeed::andChain()
{
    auto f = conj(Conj::And, {rule(QStringLiteral("status == \"active\"")),
                              rule(QStringLiteral("kind == \"note\""))});
    auto s = NewItemSeed::compute(f, {});
    QVERIFY(hasPair(s, QStringLiteral("status"), QStringLiteral("active")));
    QVERIFY(hasPair(s, QStringLiteral("kind"), QStringLiteral("note")));
}

void TestNewItemSeed::orSkipped()
{
    auto f = conj(Conj::Or, {rule(QStringLiteral("status == \"active\"")),
                             rule(QStringLiteral("status == \"done\""))});
    QVERIFY(NewItemSeed::compute(f, {}).isEmpty());
}

void TestNewItemSeed::negationSkipped()
{
    auto f = conj(Conj::Not, {rule(QStringLiteral("status == \"active\""))});
    QVERIFY(NewItemSeed::compute(f, {}).isEmpty());
}

void TestNewItemSeed::inequalitySkipped()
{
    QVERIFY(!hasKey(NewItemSeed::compute(rule(QStringLiteral("count > 3")), {}), QStringLiteral("count")));
    QVERIFY(!hasKey(NewItemSeed::compute(rule(QStringLiteral("status != \"x\"")), {}), QStringLiteral("status")));
}

void TestNewItemSeed::filePropertySkipped()
{
    QVERIFY(!hasKey(NewItemSeed::compute(rule(QStringLiteral("file.name == \"x\"")), {}), QStringLiteral("file.name")));
    QVERIFY(!hasKey(NewItemSeed::compute(rule(QStringLiteral("file.name == \"x\"")), {}), QStringLiteral("name")));
}

void TestNewItemSeed::templateVerbatim()
{
    NewItemSeed::SeedList tmpl{{QStringLiteral("tags"), QStringLiteral("inbox")}};
    auto s = NewItemSeed::compute(nullptr, tmpl);
    QVERIFY(hasPair(s, QStringLiteral("tags"), QStringLiteral("inbox")));
}

void TestNewItemSeed::equalityOverridesTemplate()
{
    NewItemSeed::SeedList tmpl{{QStringLiteral("status"), QStringLiteral("draft")}};
    auto s = NewItemSeed::compute(rule(QStringLiteral("status == \"active\"")), tmpl);
    QVERIFY(hasPair(s, QStringLiteral("status"), QStringLiteral("active")));
    // Exactly one entry for the key (override, not duplicate).
    int n = 0;
    for (const auto &p : s) if (p.first == QStringLiteral("status")) ++n;
    QCOMPARE(n, 1);
}

QTEST_MAIN(TestNewItemSeed)
#include "tst_new_item_seed.moc"
```

> **Before implementing, verify the equality formula source parses to a `BinaryExpr` with `BinOp::Eq`** by reading `libs/bases/src/Parser.cpp` (or `tst_parser.cpp`) for how `==` and member access (`note.status`, `file.name`) lower into `Ast.h` nodes. A bare `status` should parse to `IdentExpr{"status"}`; `note.status` to `MemberExpr{IdentExpr{"note"}, "status"}`; `file.name` to `MemberExpr{IdentExpr{"file"}, "name"}`. Adjust the extraction in Step 5 if the parser shapes these differently.

- [ ] **Step 4: Register in CMake, build, verify failure**

Add to `libs/bases/CMakeLists.txt` SOURCES:
```cmake
    src/NewItemSeed.cpp
    include/corbomite/bases/NewItemSeed.h
```
Add to `libs/bases/tests/CMakeLists.txt`:
```cmake
add_executable(tst_new_item_seed tst_new_item_seed.cpp)
add_test(NAME tst_new_item_seed COMMAND tst_new_item_seed)
target_link_libraries(tst_new_item_seed PRIVATE Qt6::Test Corbomite::Bases)
```
Create a stub `libs/bases/src/NewItemSeed.cpp` returning `templateProps` unchanged so it links, then:
Run: `cmake --build --preset dev -j 10 && (cd build-dev && ctest --output-on-failure -R tst_new_item_seed)`
Expected: builds; the equality/skip slots FAIL (stub ignores the filter).

- [ ] **Step 5: Implement `NewItemSeed.cpp`**

`libs/bases/src/NewItemSeed.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/NewItemSeed.h"

#include "corbomite/bases/Ast.h"
#include "corbomite/bases/Formula.h"
#include "corbomite/bases/Values.h"

namespace Corbomite::Bases {
namespace {

/// If `e` is a writable note-frontmatter property reference, return its key;
/// else return an empty QString. Accepts a bare identifier (`status`) or a
/// `note.<key>` member access. Rejects `file.*` and anything else.
QString propertyKey(const Expr *e)
{
    if (auto *id = dynamic_cast<const IdentExpr *>(e)) {
        const QString &n = id->name;
        if (n == QLatin1String("file") || n == QLatin1String("formula")
            || n == QLatin1String("this") || n == QLatin1String("note"))
            return {};
        return n;
    }
    if (auto *mem = dynamic_cast<const MemberExpr *>(e)) {
        auto *obj = dynamic_cast<const IdentExpr *>(mem->object.get());
        if (obj && obj->name == QLatin1String("note"))
            return mem->member;
    }
    return {};
}

/// Walk one formula AST node, appending (key,value) for each equality
/// constraint found in AND context. `&&` descends; everything else stops.
void collectFromExpr(const Expr *e, QVector<QPair<QString, QString>> &out)
{
    if (!e) return;
    if (auto *bin = dynamic_cast<const BinaryExpr *>(e)) {
        if (bin->op == BinOp::AndAnd) {
            collectFromExpr(bin->left.get(), out);
            collectFromExpr(bin->right.get(), out);
            return;
        }
        if (bin->op == BinOp::Eq) {
            // Accept (prop == literal) or (literal == prop).
            const Expr *lhs = bin->left.get();
            const Expr *rhs = bin->right.get();
            auto *litR = dynamic_cast<const LiteralExpr *>(rhs);
            auto *litL = dynamic_cast<const LiteralExpr *>(lhs);
            QString key;
            ValuePtr lit;
            if (litR) { key = propertyKey(lhs); lit = litR->value; }
            else if (litL) { key = propertyKey(rhs); lit = litL->value; }
            if (!key.isEmpty() && lit)
                out.append({key, lit->toString()});
        }
    }
    // Unary (Not), Or-binary, comparisons, calls, etc. contribute nothing.
}

void collectFromFilter(const FilterPtr &f, QVector<QPair<QString, QString>> &out)
{
    if (!f) return;
    if (auto *r = std::dynamic_pointer_cast<FilterRule>(f) ? f.get() : nullptr) {
        auto *rule = static_cast<FilterRule *>(f.get());
        collectFromExpr(rule->rule().ast(), out);
        return;
    }
    if (auto conj = std::dynamic_pointer_cast<FilterConjunction>(f)) {
        if (conj->conj() == Conj::And) {
            for (const auto &child : conj->children())
                collectFromFilter(child, out);
        }
        // Or / Not: contribute nothing.
    }
}
}  // namespace

NewItemSeed::SeedList NewItemSeed::compute(const FilterPtr &filter, const SeedList &templateProps)
{
    SeedList seeds = templateProps;  // template first, source order preserved.

    QVector<QPair<QString, QString>> equalities;
    collectFromFilter(filter, equalities);

    for (const auto &eq : equalities) {
        bool overrode = false;
        for (auto &existing : seeds) {
            if (existing.first == eq.first) { existing.second = eq.second; overrode = true; break; }
        }
        if (!overrode) seeds.append(eq);
    }
    return seeds;
}

}  // namespace Corbomite::Bases
```

> The `collectFromFilter` `FilterRule` branch above is deliberately written with `static_cast` after a `dynamic_pointer_cast` guard. Simplify to the idiomatic form if you prefer:
> ```cpp
> if (auto r = std::dynamic_pointer_cast<FilterRule>(f)) {
>     collectFromExpr(r->rule().ast(), out);
>     return;
> }
> ```
> Use whichever compiles cleanly; the behavior must be identical.

- [ ] **Step 6: Build and run**

Run: `cmake --build --preset dev -j 10 && (cd build-dev && ctest --output-on-failure -R tst_new_item_seed)`
Expected: PASS (all 9 slots). If `singleEquality`/`andChain` fail, re-check (per Step 3 note) how the parser shapes `==` and `note.`/`file.` access and fix `propertyKey`/`collectFromExpr` accordingly.

- [ ] **Step 7: Commit**

```bash
git add libs/bases/include/corbomite/bases/Formula.h \
        libs/bases/include/corbomite/bases/NewItemSeed.h libs/bases/src/NewItemSeed.cpp \
        libs/bases/tests/tst_new_item_seed.cpp libs/bases/CMakeLists.txt libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): NewItemSeed — AND-context equality seeding + Formula::ast() accessor"
```

---

## Task 3: `BasesView` Results menu — Copy table + Export CSV

**Files:**
- Modify: `libs/bases/include/corbomite/bases/BasesView.h`
- Modify: `libs/bases/src/BasesView.cpp`

No headless test (clipboard/file-dialog widget path). Verification is build + launch smoke + the close-out "pending user eyeball" list.

- [ ] **Step 1: Declare the button and slot**

In `BasesView.h`, add to the private member block (near `m_drawerBtn`):
```cpp
    QToolButton *m_resultsBtn = nullptr;
```
Add a private slot:
```cpp
    void onCopyTable();
    void onExportCsv();
```
(`m_newBtn` from Task 4 will be added separately.)

- [ ] **Step 2: Build the button in the toolbar**

In `BasesView.cpp`, in the toolbar-construction block (after the `m_drawerBtn` setup, ~line 89), add:
```cpp
    m_resultsBtn = new QToolButton(this);
    m_resultsBtn->setIcon(QIcon::fromTheme(QStringLiteral("document-export")));
    m_resultsBtn->setToolTip(i18n("Export / copy table"));
    m_resultsBtn->setPopupMode(QToolButton::InstantPopup);
    {
        auto *menu = new QMenu(m_resultsBtn);
        menu->addAction(i18n("Copy table"), this, &BasesView::onCopyTable);
        menu->addAction(i18n("Export CSV…"), this, &BasesView::onExportCsv);
        m_resultsBtn->setMenu(menu);
    }
    toolbar->addWidget(m_resultsBtn);
```
Ensure `#include <QMenu>` (already present per the existing context menu) and add `#include <QToolButton>` (already present).

- [ ] **Step 3: Implement `onCopyTable`**

Add includes at the top of `BasesView.cpp`:
```cpp
#include "corbomite/bases/TableExporter.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMimeData>
#include <QSaveFile>
```
Implement:
```cpp
void BasesView::onCopyTable()
{
    if (!m_controller || !m_controller->result()) return;
    TableExporter exp(*m_controller->result(),
                      [this](const PropertyId &pid) { return displayNameFor(pid); });

    auto *mime = new QMimeData();
    mime->setText(exp.toTsv());  // text/plain = TSV (spreadsheet-friendly default).
    mime->setData(QStringLiteral("text/markdown"), exp.toMarkdown().toUtf8());
    mime->setHtml(exp.toHtml());
    mime->setData(QStringLiteral("obsidian/table"), exp.toObsidianTable());
    QApplication::clipboard()->setMimeData(mime);
}
```

- [ ] **Step 4: Implement `onExportCsv`**

```cpp
void BasesView::onExportCsv()
{
    if (!m_controller || !m_controller->result()) return;

    QString suggested = QStringLiteral("table.csv");
    if (m_query && !m_query->filePath.isEmpty()) {
        const QString stem = QFileInfo(m_query->filePath).completeBaseName();
        if (!stem.isEmpty()) suggested = stem + QStringLiteral(".csv");
    }
    const QString path = QFileDialog::getSaveFileName(
        this, i18n("Export table as CSV"), suggested,
        i18n("CSV files (*.csv);;All files (*)"));
    if (path.isEmpty()) return;

    TableExporter exp(*m_controller->result(),
                      [this](const PropertyId &pid) { return displayNameFor(pid); });
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(exp.toCsv().toUtf8()) < 0 || !f.commit()) {
        m_errorBanner->setText(i18n("Failed to write CSV: %1", path));
        m_errorBanner->show();
    }
}
```
Add `#include <QFileInfo>` to the includes.

> Check that `m_errorBanner` is a `QLabel*` that the existing code shows/hides (it is — see the D.2/D.3 error-banner usage). If its show/hide idiom differs, mirror the existing call sites instead of `setText`/`show`.

- [ ] **Step 5: Build and smoke**

Run: `cmake --build --preset dev -j 10`
Expected: clean build.
Run: `./build-dev/Corbomite` — open a vault containing a `.base`, confirm the export button appears in the toolbar with both menu items, and the app does not crash on clicking them. (Clipboard/file-dialog correctness is for user verification.)

- [ ] **Step 6: Commit**

```bash
git add libs/bases/include/corbomite/bases/BasesView.h libs/bases/src/BasesView.cpp
git commit -m "feat(bases): BasesView results menu — Copy table (4 MIME) + Export CSV"
```

---

## Task 4: `BasesView` +New entry button

**Files:**
- Modify: `libs/bases/include/corbomite/bases/BasesView.h`
- Modify: `libs/bases/src/BasesView.cpp`

No headless test (file-create + dialog widget path). Build + smoke + close-out list.

- [ ] **Step 1: Declare button and slot**

In `BasesView.h` private members:
```cpp
    QToolButton *m_newBtn = nullptr;
```
Private slot:
```cpp
    void onNewItem();
```
Private helper:
```cpp
    /// Resolve the newItemTemplate path to a (key,value) frontmatter list via
    /// the metadata cache. Empty if no template or no frontmatter.
    NewItemSeed::SeedList resolveTemplateProps() const;
```
Add `#include "corbomite/bases/NewItemSeed.h"` to `BasesView.h`.

- [ ] **Step 2: Build the button**

In `BasesView.cpp` toolbar block, add the "+" button *before* `m_propsBtn` (leftmost action after the view selector/search, matching Obsidian's placement — adjust if the existing layout reads better with it elsewhere):
```cpp
    m_newBtn = new QToolButton(this);
    m_newBtn->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_newBtn->setToolTip(i18n("New entry"));
    connect(m_newBtn, &QToolButton::clicked, this, &BasesView::onNewItem);
    toolbar->addWidget(m_newBtn);
```

- [ ] **Step 3: Implement `resolveTemplateProps`**

```cpp
NewItemSeed::SeedList BasesView::resolveTemplateProps() const
{
    NewItemSeed::SeedList out;
    if (!m_query || !m_query->newItemTemplate.has_value() || !m_cache) return out;
    const QString tmplPath = m_query->newItemTemplate.value();
    if (tmplPath.isEmpty()) return out;
    const auto cache = m_cache->getFileCache(tmplPath);
    if (!cache || !cache->frontmatter.has_value()) return out;
    const QJsonObject fm = cache->frontmatter.value();
    for (auto it = fm.constBegin(); it != fm.constEnd(); ++it)
        out.append({it.key(), it.value().toVariant().toString()});
    return out;
}
```
Add includes: `#include "corbomite/storage/CachedMetadata.h"`, `#include "corbomite/storage/MetadataCache.h"` (use the actual include paths these headers live at — confirm with the existing `#include`s in `BasesView.cpp` / `QueryController.cpp`), and `#include <QJsonObject>`.

- [ ] **Step 4: Implement `onNewItem`**

```cpp
void BasesView::onNewItem()
{
    if (!m_fm) return;

    // 1. Folder: newItemFolder, else vault default (empty string = root).
    QString folder;
    if (m_query && m_query->newItemFolder.has_value())
        folder = m_query->newItemFolder.value();

    // 2. Seed = template frontmatter + AND-context equality constraints.
    BasesViewConfig *view = m_activeView;
    FilterPtr filter = view ? view->filters : FilterPtr{};
    if (!filter && m_query) filter = m_query->filters;
    const NewItemSeed::SeedList seed = NewItemSeed::compute(filter, resolveTemplateProps());

    // 3. Create the note.
    Corbomite::TFile *file = m_fm->createMarkdownNote(QStringLiteral("Untitled"), folder);
    if (!file) {
        m_errorBanner->setText(i18n("Failed to create new note"));
        m_errorBanner->show();
        return;
    }

    // 4. Write seed frontmatter (synchronous, same path as inline edits).
    if (!seed.isEmpty()) {
        m_fm->processFrontMatter(file, [&seed](QVariantMap &fm) {
            for (const auto &p : seed) fm.insert(p.first, p.second);
        });
    }

    // 5. Open + prompt rename via the host callbacks wired in D.4a.
    const QString path = file->path();
    if (m_openInNewTab) m_openInNewTab(path);
    if (m_promptRename) m_promptRename(path);
}
```

> Confirm the active-view filter field name (`BasesViewConfig::filters`) and the global field (`BasesQuery::filters`) against the headers — both are `FilterPtr` per `FilterTree.h`/`BasesViewConfig.h`/`BasesQuery.h`. Confirm `TFile::path()` is the right accessor by grepping existing `m_openInNewTab` call sites in `BasesView.cpp` (D.4a) and reuse whatever path expression they use.

- [ ] **Step 5: Build and smoke**

Run: `cmake --build --preset dev -j 10`
Expected: clean build.
Run: `./build-dev/Corbomite` — open a `.base`, click "+", confirm a new note is created, opened in a tab, and the rename dialog appears. (Seeding/folder correctness is for user verification.)

- [ ] **Step 6: Commit**

```bash
git add libs/bases/include/corbomite/bases/BasesView.h libs/bases/src/BasesView.cpp
git commit -m "feat(bases): BasesView +New entry — seeded create, open, rename"
```

---

## Task 5: Full-suite verification + close-out

**Files:**
- Modify: `docs/PROJECT-STATE.md`, `docs/superpowers/plans/INDEX.md`, `docs/decisions-archive.md`

- [ ] **Step 1: Run the full bases suite + a clean build**

Run: `cmake --build --preset dev -j 10 && (cd build-dev && ctest --output-on-failure -j 10 -R tst_bases; ctest --output-on-failure -R 'tst_table_exporter|tst_new_item_seed')`
Expected: every bases test green, including the two new suites (21 total: 19 pre-existing + 2 new).

- [ ] **Step 2: Launch smoke**

Run: `./build-dev/Corbomite`
Expected: opens, a `.base` view shows the "+" and export buttons, no crash. Confirm clean exit.

- [ ] **Step 3: Update tracking docs**

- `docs/PROJECT-STATE.md`: in §"Recent decisions" add a dated **2026-05-26 — Cluster D.4b shipped** entry (≤3 sentences in the spirit of the slim rule); update the §"Active strategic clusters" Cluster D row to list D.4b done and drop it from "remaining"; update §"Last touched".
- `docs/superpowers/plans/INDEX.md`: update the Cluster D row — mark D.4b done, link this plan + spec.
- `docs/decisions-archive.md`: append a full close-out paragraph under a new `## 2026-05-26 — Cluster D.4b (Bases export/copy + +New entry)` H2: helpers shipped, MIME formats, AND-only seeding boundary, the items left in D (D.4c undo, formula editor, filter builder, D.5), and the **pending-user-eyeball** caveat for the widget paths.

- [ ] **Step 4: Commit the close-out**

```bash
git add docs/PROJECT-STATE.md docs/superpowers/plans/INDEX.md docs/decisions-archive.md
git commit -m "docs(tracking): close out Cluster D.4b (Bases export/copy + +New entry)"
```

---

## Self-review notes (for the implementer)

- **Spec coverage:** Task 1 = export/copy (4 MIME + CSV); Task 2 = NewItemSeed (AND-equality + template merge) + the AST accessor it needs; Tasks 3–4 = the two toolbar controls reusing existing service/callback seams; Task 5 = verification + tracking. All spec sections map to a task.
- **Two judgment calls confirmed in brainstorm:** export is flat-in-sort-order ignoring groups (Task 1 `bodyRows` iterates `result.rows()` directly); seeding is AND-context equality only (Task 2 `collectFromFilter` descends only `Conj::And` / `BinOp::AndAnd`).
- **Verify-against-source flags** are embedded where the plan depends on un-inspected details: the `BasesEntry` test-construction idiom (Task 1 Step 2), the parser's `==`/member-access node shapes (Task 2 Step 3), the `m_errorBanner` show idiom (Task 3 Step 4), and `TFile::path()` + filter field names (Task 4 Step 4). Resolve each by reading the cited existing code before implementing — do not guess.
- **Deferred (not in this plan):** undo/redo (D.4c), formula editor, filter builder, D.5 plugin API, OR/negation seeding, per-layout export.
