// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterSpec.h"
#include "corbomite/bases/FilterTree.h"

#include <QtTest>

using namespace Corbomite::Bases;

class TestFilterSpec : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void fromNull_isEmptyAndGroup()
    {
        FilterSpec s = fromFilter(nullptr);
        QCOMPARE(s.kind, FilterSpec::Kind::Group);
        QCOMPARE(s.conj, Conj::And);
        QVERIFY(s.children.isEmpty());
    }
    void emptyGroup_toNull()
    {
        QVERIFY(toFilter(FilterSpec::group(Conj::And)) == nullptr);
    }
    void bareRule_roundTripsAsString()
    {
        // fromFilter wraps a bare rule in an And-group with one leaf...
        auto rule = std::make_shared<FilterRule>(Formula(QStringLiteral("status == \"done\"")));
        FilterSpec s = fromFilter(rule);
        QCOMPARE(s.kind, FilterSpec::Kind::Group);
        QCOMPARE(s.children.size(), 1);
        QCOMPARE(s.children[0].kind, FilterSpec::Kind::Leaf);
        QCOMPARE(s.children[0].expression, QStringLiteral("status == \"done\""));
        // ...and toFilter collapses the single-child And back to a bare rule.
        FilterPtr back = toFilter(s);
        QVERIFY(back != nullptr);
        QCOMPARE(back->serialize().toString(), QStringLiteral("status == \"done\""));
    }
    void blankLeaf_isDropped()
    {
        FilterSpec g = FilterSpec::group(Conj::And, {
            FilterSpec::leaf(QStringLiteral("a > 1")),
            FilterSpec::leaf(QStringLiteral("   ")),   // blank -> dropped
        });
        FilterPtr back = toFilter(g);   // one surviving child -> collapses to bare rule
        QVERIFY(back != nullptr);
        QCOMPARE(back->serialize().toString(), QStringLiteral("a > 1"));
    }
    void orGroup_roundTrips()
    {
        FilterSpec g = FilterSpec::group(Conj::Or, {
            FilterSpec::leaf(QStringLiteral("a == 1")),
            FilterSpec::leaf(QStringLiteral("b == 2")),
        });
        FilterPtr f = toFilter(g);
        const QVariantMap m = f->serialize().toMap();
        QVERIFY(m.contains(QStringLiteral("or")));
        QCOMPARE(m.value(QStringLiteral("or")).toList().size(), 2);
        // and fromFilter of that yields the same spec
        QCOMPARE(fromFilter(f), g);
    }
    void notWithOneChild_notCollapsed()
    {
        FilterSpec g = FilterSpec::group(Conj::Not, {
            FilterSpec::leaf(QStringLiteral("archived == true")),
        });
        FilterPtr f = toFilter(g);
        const QVariantMap m = f->serialize().toMap();
        QVERIFY(m.contains(QStringLiteral("not")));
        QCOMPARE(m.value(QStringLiteral("not")).toList().size(), 1);
    }
    void nestedGroup_roundTrips()
    {
        FilterSpec g = FilterSpec::group(Conj::And, {
            FilterSpec::leaf(QStringLiteral("a == 1")),
            FilterSpec::group(Conj::Or, {
                FilterSpec::leaf(QStringLiteral("b == 2")),
                FilterSpec::leaf(QStringLiteral("c == 3")),
            }),
        });
        QCOMPARE(fromFilter(toFilter(g)), g);
    }
};

QTEST_APPLESS_MAIN(TestFilterSpec)
#include "tst_filter_spec.moc"
