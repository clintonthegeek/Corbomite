// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/bases/BasesQuery.h"
#include "corbomite/bases/FilterTree.h"
#include "corbomite/bases/PropertyId.h"

using namespace Corbomite::Bases;

class TestYamlSchema : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    // ----- PropertyId -----

    void testParseNoteProperty()
    {
        auto p = parsePropertyId(QStringLiteral("note.status"));
        QCOMPARE(static_cast<int>(p.kind), static_cast<int>(PropertyKind::Note));
        QCOMPARE(p.name, QStringLiteral("status"));
    }

    void testParseFileProperty()
    {
        auto p = parsePropertyId(QStringLiteral("file.name"));
        QCOMPARE(static_cast<int>(p.kind), static_cast<int>(PropertyKind::File));
        QCOMPARE(p.name, QStringLiteral("name"));
    }

    void testParseFormulaProperty()
    {
        auto p = parsePropertyId(QStringLiteral("formula.priority"));
        QCOMPARE(static_cast<int>(p.kind), static_cast<int>(PropertyKind::Formula));
    }

    void testParseUnprefixedDefaultsNote()
    {
        auto p = parsePropertyId(QStringLiteral("status"));
        QCOMPARE(static_cast<int>(p.kind), static_cast<int>(PropertyKind::Note));
        QCOMPARE(p.name, QStringLiteral("status"));
    }

    void testPropertyIdRoundTrip()
    {
        auto p = parsePropertyId(QStringLiteral("note.status"));
        QCOMPARE(buildPropertyId(p), QStringLiteral("note.status"));
    }

    // ----- BasesQuery -----

    void testEmptyYieldsDefaultTableView()
    {
        auto q = BasesQuery::fromString(QString{});
        QCOMPARE(q->views.size(), size_t(1));
        QCOMPARE(q->views.front()->type, QStringLiteral("table"));
    }

    void testSimpleQueryParse()
    {
        const QString src = QStringLiteral(
            "views:\n"
            "  - type: table\n"
            "    name: \"All notes\"\n"
            "formulas:\n"
            "  priority: \"note.urgent\"\n");
        auto q = BasesQuery::fromString(src);
        QCOMPARE(q->views.size(), size_t(1));
        QCOMPARE(q->views.front()->name, QStringLiteral("All notes"));
        QCOMPARE(q->formulas.size(), 1);
        QVERIFY(q->formulas.contains(QStringLiteral("priority")));
    }

    void testRoundTrip()
    {
        const QString src = QStringLiteral(
            "views:\n"
            "  - type: table\n"
            "    name: A\n"
            "  - type: table\n"
            "    name: B\n");
        auto q1 = BasesQuery::fromString(src);
        const QString out = q1->toString();
        auto q2 = BasesQuery::fromString(out);
        QCOMPARE(q2->views.size(), size_t(2));
        QCOMPARE(q2->views.front()->name, QStringLiteral("A"));
    }

    void testLegacyDisplayMigratesToProperties()
    {
        const QString src = QStringLiteral(
            "display:\n"
            "  note.status: Status\n");
        auto q = BasesQuery::fromString(src);
        const auto id = parsePropertyId(QStringLiteral("note.status"));
        QVERIFY(q->properties.contains(id));
        QCOMPARE(q->properties.value(id).displayName, QStringLiteral("Status"));
    }

    void testUnknownTopLevelKeyPreserved()
    {
        const QString src = QStringLiteral(
            "views:\n"
            "  - type: table\n"
            "    name: A\n"
            "futureKey: someValue\n");
        auto q = BasesQuery::fromString(src);
        QVERIFY(q->unrecognizedData.contains(QStringLiteral("futureKey")));
        const QString out = q->toString();
        // Reparse and confirm the preserve survives.
        auto q2 = BasesQuery::fromString(out);
        QVERIFY(q2->unrecognizedData.contains(QStringLiteral("futureKey")));
    }

    void testLimitZeroIsUnlimited()
    {
        const QString src = QStringLiteral(
            "views:\n"
            "  - type: table\n"
            "    name: A\n"
            "    limit: 0\n");
        auto q = BasesQuery::fromString(src);
        QCOMPARE(q->views.front()->limit, 0);
        // limit: 0 is the "unlimited" default, so toString omits it.
        const QString out = q->toString();
        QVERIFY(!out.contains(QStringLiteral("limit:")));
    }

    // Regression: emitMap previously iterated QVariantMap (alphabetical), so
    // every save shuffled top-level keys (filters/formulas/properties/views)
    // out of canonical doc shape. Round-trip a fully-populated `.base` and
    // confirm the canonical doc-order (filters → formulas → properties →
    // summaries → views) survives.
    void testTopLevelKeyOrderPreserved()
    {
        const QString src = QStringLiteral(
            "filters:\n"
            "  and:\n"
            "    - 'true'\n"
            "formulas:\n"
            "  prio: \"note.urgent\"\n"
            "properties:\n"
            "  note.status:\n"
            "    displayName: Status\n"
            "summaries:\n"
            "  total: \"values.count()\"\n"
            "views:\n"
            "  - type: table\n"
            "    name: All\n"
            "newItemFolder: \"Inbox\"\n");
        auto q = BasesQuery::fromString(src);
        const QString out = q->toString();

        const int filtersPos       = out.indexOf(QStringLiteral("filters:"));
        const int formulasPos      = out.indexOf(QStringLiteral("formulas:"));
        const int propertiesPos    = out.indexOf(QStringLiteral("properties:"));
        const int summariesPos     = out.indexOf(QStringLiteral("summaries:"));
        const int viewsPos         = out.indexOf(QStringLiteral("views:"));
        const int newItemFolderPos = out.indexOf(QStringLiteral("newItemFolder:"));

        QVERIFY2(filtersPos >= 0,       "filters: present");
        QVERIFY2(formulasPos >= 0,      "formulas: present");
        QVERIFY2(propertiesPos >= 0,    "properties: present");
        QVERIFY2(summariesPos >= 0,     "summaries: present");
        QVERIFY2(viewsPos >= 0,         "views: present");
        QVERIFY2(newItemFolderPos >= 0, "newItemFolder: present");

        QVERIFY2(filtersPos < formulasPos,
                 "filters must precede formulas (canonical order)");
        QVERIFY2(formulasPos < propertiesPos,
                 "formulas must precede properties");
        QVERIFY2(propertiesPos < summariesPos,
                 "properties must precede summaries");
        QVERIFY2(summariesPos < viewsPos,
                 "summaries must precede views");
        // newItemFolder ('n') alphabetises between formulas and properties,
        // but canonically belongs after views. This catches the bug a purely
        // f/f/p/s/v order would miss (since those keys happen to be in
        // alphabetical sequence).
        QVERIFY2(viewsPos < newItemFolderPos,
                 "newItemFolder must come after views (canonical), not "
                 "alphabetically between formulas and properties");
    }

    // Regression: per-view config alphabetised too — so a user-authored view
    // with `type` / `name` / `filters` / `order` ended up `filters` / `name`
    // / `order` / `type`. Confirm canonical view-shape (type → name → ...)
    // survives a round-trip.
    void testViewConfigKeyOrderPreserved()
    {
        const QString src = QStringLiteral(
            "views:\n"
            "  - type: table\n"
            "    name: All\n"
            "    limit: 10\n"
            "    order:\n"
            "      - file.name\n");
        auto q = BasesQuery::fromString(src);
        const QString out = q->toString();

        const int typePos  = out.indexOf(QStringLiteral("type:"));
        const int namePos  = out.indexOf(QStringLiteral("name:"));
        const int limitPos = out.indexOf(QStringLiteral("limit:"));
        const int orderPos = out.indexOf(QStringLiteral("order:"));

        QVERIFY(typePos >= 0 && namePos >= 0 && limitPos >= 0 && orderPos >= 0);
        QVERIFY2(typePos < namePos, "type must precede name");
        QVERIFY2(namePos < orderPos || namePos < limitPos,
                 "name must precede order/limit");
    }

    void testPropertyOrderPreservedNotAlphabetised()
    {
        // Audit: bases.md §"On-disk `.base` format compatibility" — user-
        // keyed dicts (properties/formulas/summaries) used to alphabetise
        // on round-trip because their backing QHash spilled into a
        // QVariantMap (alphabetical). Insertion order is now tracked so
        // round-trip preserves the author's source ordering.
        const QString src = QStringLiteral(
            "properties:\n"
            "  note.zeta:\n"
            "    displayName: Z\n"
            "  note.alpha:\n"
            "    displayName: A\n"
            "  note.mike:\n"
            "    displayName: M\n");
        auto q = BasesQuery::fromString(src);
        const QString out = q->toString();

        const int zetaPos  = out.indexOf(QStringLiteral("note.zeta:"));
        const int alphaPos = out.indexOf(QStringLiteral("note.alpha:"));
        const int mikePos  = out.indexOf(QStringLiteral("note.mike:"));

        QVERIFY(zetaPos >= 0 && alphaPos >= 0 && mikePos >= 0);
        QVERIFY2(zetaPos < alphaPos,
                 "zeta must precede alpha (insertion order, not alphabetical)");
        QVERIFY2(alphaPos < mikePos,
                 "alpha must precede mike (insertion order, not alphabetical)");
    }

    void testFormulaOrderPreservedNotAlphabetised()
    {
        const QString src = QStringLiteral(
            "formulas:\n"
            "  zoo: \"1\"\n"
            "  apple: \"2\"\n"
            "  middle: \"3\"\n");
        auto q = BasesQuery::fromString(src);
        const QString out = q->toString();

        const int zooPos    = out.indexOf(QStringLiteral("zoo:"));
        const int applePos  = out.indexOf(QStringLiteral("apple:"));
        const int middlePos = out.indexOf(QStringLiteral("middle:"));

        QVERIFY(zooPos >= 0 && applePos >= 0 && middlePos >= 0);
        QVERIFY2(zooPos < applePos, "zoo must precede apple");
        QVERIFY2(applePos < middlePos, "apple must precede middle");
    }

    void testSummaryOrderPreservedNotAlphabetised()
    {
        const QString src = QStringLiteral(
            "summaries:\n"
            "  zsum: \"1\"\n"
            "  asum: \"2\"\n");
        auto q = BasesQuery::fromString(src);
        const QString out = q->toString();

        const int zPos = out.indexOf(QStringLiteral("zsum:"));
        const int aPos = out.indexOf(QStringLiteral("asum:"));
        QVERIFY(zPos >= 0 && aPos >= 0);
        QVERIFY2(zPos < aPos, "zsum must precede asum");
    }

    void testSortRoundTrip()
    {
        const QString src = QStringLiteral(
            "views:\n"
            "  - type: table\n"
            "    name: A\n"
            "    sort:\n"
            "      - property: note.due\n"
            "        direction: ASC\n");
        auto q = BasesQuery::fromString(src);
        const auto *v = q->views.front().get();
        QCOMPARE(v->sort.size(), 1);
        QCOMPARE(v->sort[0].direction, QStringLiteral("ASC"));
    }

    // ----- FilterTree -----

    void testFilterLeafTest()
    {
        // Filter leaf parses a formula string and evaluates as predicate.
        FilterRule r(Formula(QStringLiteral("true")));
        LambdaContext ctx([](const QString &) -> ValuePtr { return nullptr; });
        QVERIFY(r.test(ctx, nullptr));
    }

    void testFilterConjunctionAnd()
    {
        auto trueLeaf = std::make_shared<FilterRule>(Formula(QStringLiteral("true")));
        auto falseLeaf = std::make_shared<FilterRule>(Formula(QStringLiteral("false")));
        FilterConjunction c(Conj::And, {trueLeaf, falseLeaf});
        LambdaContext ctx([](const QString &) -> ValuePtr { return nullptr; });
        QVERIFY(!c.test(ctx));

        FilterConjunction c2(Conj::And, {trueLeaf, trueLeaf});
        QVERIFY(c2.test(ctx));
    }

    void testFilterConjunctionOr()
    {
        auto trueLeaf = std::make_shared<FilterRule>(Formula(QStringLiteral("true")));
        auto falseLeaf = std::make_shared<FilterRule>(Formula(QStringLiteral("false")));
        FilterConjunction c(Conj::Or, {falseLeaf, trueLeaf});
        LambdaContext ctx([](const QString &) -> ValuePtr { return nullptr; });
        QVERIFY(c.test(ctx));
    }

    void testFilterConjunctionNot()
    {
        auto trueLeaf = std::make_shared<FilterRule>(Formula(QStringLiteral("true")));
        FilterConjunction c(Conj::Not, {trueLeaf});
        LambdaContext ctx([](const QString &) -> ValuePtr { return nullptr; });
        QVERIFY(!c.test(ctx));
    }

    void testFilterOptimizeCollapsesSingleChild()
    {
        auto leaf = std::make_shared<FilterRule>(Formula(QStringLiteral("true")));
        FilterConjunction c(Conj::And, {leaf});
        auto optimized = c.optimize();
        QVERIFY(std::dynamic_pointer_cast<FilterRule>(optimized));
    }
};

QTEST_APPLESS_MAIN(TestYamlSchema)
#include "tst_yaml_schema.moc"
