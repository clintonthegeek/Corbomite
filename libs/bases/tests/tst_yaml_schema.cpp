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

    void testUnknownTopLevelNestedShapePreserved()
    {
        // A future Obsidian `.base` key whose value is a nested map or list
        // must survive a load→save→load round-trip. Previously the parser's
        // unrecognizedData switch dropped non-scalar shapes (`default: break;`).
        const QString src = QStringLiteral(
            "views:\n"
            "  - type: table\n"
            "    name: A\n"
            "futureMap:\n"
            "  nested: value\n"
            "  count: 3\n"
            "futureList:\n"
            "  - one\n"
            "  - two\n");
        auto q = BasesQuery::fromString(src);

        // Nested map preserved on first parse.
        QVERIFY(q->unrecognizedData.contains(QStringLiteral("futureMap")));
        const QVariant mapVal = q->unrecognizedData.value(QStringLiteral("futureMap"));
        QCOMPARE(mapVal.typeId(), QMetaType::QVariantMap);
        QCOMPARE(mapVal.toMap().value(QStringLiteral("nested")).toString(),
                 QStringLiteral("value"));
        QCOMPARE(mapVal.toMap().value(QStringLiteral("count")).toLongLong(), 3LL);

        // Nested list preserved on first parse.
        QVERIFY(q->unrecognizedData.contains(QStringLiteral("futureList")));
        const QVariant listVal = q->unrecognizedData.value(QStringLiteral("futureList"));
        QCOMPARE(listVal.typeId(), QMetaType::QVariantList);
        QCOMPARE(listVal.toList().size(), 2);
        QCOMPARE(listVal.toList().at(0).toString(), QStringLiteral("one"));

        // ...and survives reparse after emission.
        auto q2 = BasesQuery::fromString(q->toString());
        QVERIFY(q2->unrecognizedData.contains(QStringLiteral("futureMap")));
        QCOMPARE(q2->unrecognizedData.value(QStringLiteral("futureMap"))
                     .toMap().value(QStringLiteral("count")).toLongLong(), 3LL);
        QVERIFY(q2->unrecognizedData.contains(QStringLiteral("futureList")));
        QCOMPARE(q2->unrecognizedData.value(QStringLiteral("futureList"))
                     .toList().size(), 2);
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
    // confirm the writer's own order survives.
    //
    // The order is Obsidian's `getSerializable()` (app/own/1766-lQ.js:204-230):
    // filters → formulas → summaries → properties → views → newItem*. It used
    // to be `properties` before `summaries` here, on the strength of the audit
    // docs; the restored 1.13.7 writer puts `summaries` first.
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
                 "filters must precede formulas (writer order)");
        QVERIFY2(formulasPos < summariesPos,
                 "formulas must precede summaries");
        QVERIFY2(summariesPos < propertiesPos,
                 "summaries must precede properties (writer order, 1766-lQ.js:216-227)");
        QVERIFY2(propertiesPos < viewsPos,
                 "properties must precede views");
        // newItemFolder ('n') alphabetises between formulas and properties,
        // but belongs after views. This catches the bug a purely
        // f/f/p/s/v order would miss (since those keys happen to be in
        // alphabetical sequence).
        QVERIFY2(viewsPos < newItemFolderPos,
                 "newItemFolder must come after views (writer order), not "
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

    // ----- property-id serialisation (Obsidian `hQ`) -----

    void testSerializePropertyIdStripsNotePrefix()
    {
        QCOMPARE(serializePropertyId(parsePropertyId(QStringLiteral("note.status"))),
                 QStringLiteral("status"));
        QCOMPARE(serializePropertyId(parsePropertyId(QStringLiteral("note.priority"))),
                 QStringLiteral("priority"));
        QCOMPARE(serializePropertyId(parsePropertyId(QStringLiteral("status"))),
                 QStringLiteral("status"));
    }

    void testSerializePropertyIdShortensFileFile()
    {
        QCOMPARE(serializePropertyId(parsePropertyId(QStringLiteral("file.file"))),
                 QStringLiteral("file"));
        QCOMPARE(serializePropertyId(parsePropertyId(QStringLiteral("file.name"))),
                 QStringLiteral("file.name"));
    }

    void testSerializePropertyIdKeepsReservedAndFormulaPrefixes()
    {
        // `note.note.x` survives: stripping would re-read the second segment
        // as the type.
        QCOMPARE(serializePropertyId(parsePropertyId(QStringLiteral("note.note.x"))),
                 QStringLiteral("note.note.x"));
        QCOMPARE(serializePropertyId(parsePropertyId(QStringLiteral("note.file.y"))),
                 QStringLiteral("note.file.y"));
        QCOMPARE(serializePropertyId(parsePropertyId(QStringLiteral("formula.ppux"))),
                 QStringLiteral("formula.ppux"));
    }

    // The headline: one `.base` that names the same property in `order:` and
    // in `properties:` must come back with `order:` in Obsidian's short form
    // and `properties:` verbatim — and must be a fixed point, so a second
    // round-trip changes nothing.
    void testSamePropertyInOrderAndPropertiesRoundTripsStably()
    {
        const QString src = QStringLiteral(
            "properties:\n"
            "  note.status:\n"
            "    displayName: Status\n"
            "views:\n"
            "  - type: table\n"
            "    name: All\n"
            "    order:\n"
            "      - note.status\n");
        const QString once = BasesQuery::fromString(src)->toString();
        QVERIFY2(once.contains(QStringLiteral("    - status\n")),
                 qPrintable(QStringLiteral("order: must be canonicalised to `status`; got:\n%1").arg(once)));
        QVERIFY2(once.contains(QStringLiteral("  note.status:\n")),
                 qPrintable(QStringLiteral("properties: key must stay as authored; got:\n%1").arg(once)));
        QCOMPARE(BasesQuery::fromString(once)->toString(), once);
    }

    void testPropertiesKeyPreservedUnprefixed()
    {
        const QString src = QStringLiteral(
            "properties:\n"
            "  status:\n"
            "    displayName: Status\n"
            "views:\n"
            "  - type: table\n"
            "    name: All\n");
        const QString out = BasesQuery::fromString(src)->toString();
        QVERIFY2(out.contains(QStringLiteral("  status:\n")),
                 qPrintable(QStringLiteral("an unprefixed properties: key must not be rewritten; got:\n%1").arg(out)));
        QVERIFY(!out.contains(QStringLiteral("note.status")));
    }

    void testViewPropertyIdSitesAllCanonicalised()
    {
        const QString src = QStringLiteral(
            "views:\n"
            "  - type: table\n"
            "    name: All\n"
            "    groupBy:\n"
            "      property: note.status\n"
            "      direction: ASC\n"
            "    order:\n"
            "      - note.status\n"
            "      - file.file\n"
            "    sort:\n"
            "      - property: note.priority\n"
            "        direction: DESC\n"
            "    summaries:\n"
            "      note.price: Total\n");
        const QString out = BasesQuery::fromString(src)->toString();
        QVERIFY2(out.contains(QStringLiteral("property: status")), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("- status")), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("- file\n")), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("property: priority")), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("price: Total")), qPrintable(out));
        QVERIFY(!out.contains(QStringLiteral("note.")));
        QCOMPARE(BasesQuery::fromString(out)->toString(), out);
    }

    // The asymmetry the addendum flags: `data.columnSize` is written by
    // Obsidian with canonical `note.x` keys while `order` loses the prefix
    // (1972-Z5.js:1021-1029 vs 1766-lQ.js:17). We pass `data` through
    // byte-for-byte, so both survive.
    void testViewDataColumnSizeKeysPassedThroughVerbatim()
    {
        const QString src = QStringLiteral(
            "views:\n"
            "  - type: table\n"
            "    name: All\n"
            "    order:\n"
            "      - note.status\n"
            "    columnSize:\n"
            "      note.status: 240\n");
        const QString out = BasesQuery::fromString(src)->toString();
        QVERIFY2(out.contains(QStringLiteral("      note.status: 240")), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("    - status")), qPrintable(out));
        auto q = BasesQuery::fromString(out);
        QCOMPARE(q->views.front()->data.value(QStringLiteral("columnSize")).toMap()
                     .value(QStringLiteral("note.status")).toInt(), 240);
    }

    void testEmptyPropertiesBlockOmitted()
    {
        // A config with nothing in it serialises to null, so a `properties:`
        // block made only of empty configs disappears entirely
        // (1764-jX.js:267-269 + 1766-lQ.js:223-227).
        const QString src = QStringLiteral(
            "properties:\n"
            "  note.status: {}\n"
            "views:\n"
            "  - type: table\n"
            "    name: All\n");
        const QString out = BasesQuery::fromString(src)->toString();
        QVERIFY2(!out.contains(QStringLiteral("properties:")), qPrintable(out));
    }

    // `formulas:` is length-guarded and vanishes when emptied; `summaries:`
    // is not, and persists as `{}` (1766-lQ.js:206-222).
    void testEmptySummariesPersistsEmptyFormulasDoesNot()
    {
        auto q = BasesQuery::fromString(QStringLiteral(
            "formulas:\n"
            "  a: \"1\"\n"
            "summaries:\n"
            "  s: \"values.sum()\"\n"
            "views:\n"
            "  - type: table\n"
            "    name: All\n"));
        q->formulas.clear();
        q->summaryFormulas.clear();
        const QString out = q->toString();
        QVERIFY2(!out.contains(QStringLiteral("formulas:")), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("summaries: {}")), qPrintable(out));
    }

    void testSummariesBlockNotDoubleHandled()
    {
        // A top-level `summaries:` must be owned by `summaryFormulas` alone.
        // If it also landed in `unrecognizedData` the two writers would fight
        // on serialise (the 2026-06-10 finding, still live in 1.13.7 at
        // 1766-lQ.js:188-202 + :216-222).
        auto q = BasesQuery::fromString(QStringLiteral(
            "summaries:\n"
            "  s: \"values.sum()\"\n"
            "views:\n"
            "  - type: table\n"
            "    name: All\n"));
        QVERIFY(!q->unrecognizedData.contains(QStringLiteral("summaries")));
        QCOMPARE(q->summaryFormulas.value(QStringLiteral("s")).source(),
                 QStringLiteral("values.sum()"));
    }

    void testViewKeyOrderMatchesWriter()
    {
        // 1766-lQ.js:9-32: type, name, filters, groupBy, order, sort, limit,
        // summaries, then free-form data keys.
        const QString src = QStringLiteral(
            "views:\n"
            "  - type: table\n"
            "    name: All\n"
            "    limit: 5\n"
            "    sort:\n"
            "      - property: note.a\n"
            "        direction: ASC\n"
            "    order:\n"
            "      - file.name\n"
            "    groupBy:\n"
            "      property: note.b\n"
            "      direction: ASC\n"
            "    filters:\n"
            "      and:\n"
            "        - 'true'\n");
        const QString out = BasesQuery::fromString(src)->toString();
        const int name = out.indexOf(QStringLiteral("name:"));
        const int filters = out.indexOf(QStringLiteral("filters:"));
        const int groupBy = out.indexOf(QStringLiteral("groupBy:"));
        const int order = out.indexOf(QStringLiteral("order:"));
        const int sort = out.indexOf(QStringLiteral("sort:"));
        const int limit = out.indexOf(QStringLiteral("limit:"));
        QVERIFY(filters > name && groupBy > filters && order > groupBy
                && sort > order && limit > sort);
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

    // `not:` is NOR, not NAND (app/own/1764-jX.js:232-237). The distinction
    // only shows up with two children: NAND would be true when both are false,
    // NOR is false when either is true.
    void testFilterConjunctionNotIsNorNotNand()
    {
        auto trueLeaf  = std::make_shared<FilterRule>(Formula(QStringLiteral("true")));
        auto falseLeaf = std::make_shared<FilterRule>(Formula(QStringLiteral("false")));
        LambdaContext ctx([](const QString &) -> ValuePtr { return nullptr; });

        // NOR: !(true || false) == false, !(false || false) == true.
        QVERIFY(!FilterConjunction(Conj::Not, {trueLeaf, falseLeaf}).test(ctx));
        QVERIFY(!FilterConjunction(Conj::Not, {falseLeaf, trueLeaf}).test(ctx));
        QVERIFY(FilterConjunction(Conj::Not, {falseLeaf, falseLeaf}).test(ctx));
        // NAND would have returned true for {true, false} — that is the
        // reading this test exists to refute.
        QVERIFY2(!FilterConjunction(Conj::Not, {trueLeaf, falseLeaf}).test(ctx),
                 "not: must be NOR, not !(a && b)");
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
