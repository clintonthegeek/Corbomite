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
