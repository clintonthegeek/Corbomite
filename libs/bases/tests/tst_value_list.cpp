// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include <QDateTime>

#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

namespace {

ValuePtr N(double x) { return std::make_shared<NumberValue>(x); }
ValuePtr S(const char *s) { return std::make_shared<StringValue>(QString::fromUtf8(s)); }

std::shared_ptr<ListValue> list(std::initializer_list<ValuePtr> xs)
{
    QVector<ValuePtr> v(xs.begin(), xs.end());
    return std::make_shared<ListValue>(v);
}

}  // namespace

class TestListValue : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void testListType()
    {
        QCOMPARE(ListValue().type(), QStringLiteral("List"));
    }

    void testListTruthyByLength()
    {
        QVERIFY(!ListValue().isTruthy());
        auto l = list({N(1)});
        QVERIFY(l->isTruthy());
    }

    void testListLengthObjectAccess()
    {
        auto l = list({N(1), N(2), N(3)});
        auto len = l->objectAccess(QStringLiteral("length"));
        QVERIFY(len);
        QCOMPARE(len->type(), QStringLiteral("Number"));
        QCOMPARE(std::static_pointer_cast<NumberValue>(len)->data(), 3.0);
    }

    void testListGetInBounds()
    {
        auto l = list({N(10), N(20), N(30)});
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(1))->data(), 20.0);
    }

    void testListGetOutOfBounds()
    {
        auto l = list({N(10)});
        auto v = l->get(5);
        QCOMPARE(v->type(), QStringLiteral("Null"));
    }

    void testListIncludesLooseEquals()
    {
        auto l = list({S("a"), S("b")});
        QVERIFY(l->includes(S("a")));
        QVERIFY(!l->includes(S("c")));
    }

    void testIncludesHierarchicalTag()
    {
        QVector<ValuePtr> elems{ std::make_shared<TagValue>(QStringLiteral("#parent/child")) };
        auto l = std::make_shared<ListValue>(elems);
        QVERIFY(l->includes(std::make_shared<TagValue>(QStringLiteral("#parent"))));
        QVERIFY(l->includes(std::make_shared<StringValue>(QStringLiteral("#parent"))));
        QVERIFY(l->includes(std::make_shared<TagValue>(QStringLiteral("#parent/child"))));
        QVERIFY(!l->includes(std::make_shared<TagValue>(QStringLiteral("#parent/other"))));
    }

    void testListConcat()
    {
        auto a = list({N(1), N(2)});
        auto b = list({N(3), N(4)});
        auto c = a->concat(*b);
        QCOMPARE(c->length(), 4);
        QCOMPARE(std::static_pointer_cast<NumberValue>(c->get(3))->data(), 4.0);
    }

    void testListReverse()
    {
        auto l = list({N(1), N(2), N(3)})->reverse();
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(0))->data(), 3.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(2))->data(), 1.0);
    }

    void testListFlattenOneLevel()
    {
        auto inner = list({N(2), N(3)});
        QVector<ValuePtr> v {N(1), inner, N(4)};
        auto l = std::make_shared<ListValue>(v)->flatten();
        QCOMPARE(l->length(), 4);
    }

    void testListUniqueViaLooseEquals()
    {
        auto l = list({S("a"), S("a"), S("b")})->unique();
        QCOMPARE(l->length(), 2);
    }

    // `unique()` buckets by `toString()` but deduplicates with strict
    // `equals` (app/own/1647-tK.js:287-303), so values that stringify alike
    // but are not `equals` both survive. Loose dedup would collapse these.
    void testListUniqueIsStrictNotLoose()
    {
        // A DurationValue and its own source string land in the same bucket
        // ("5 days") and are `looseEquals`, but `DurationValue::equals` only
        // ever matches another DurationValue.
        QVector<ValuePtr> pair {
            std::make_shared<DurationValue>(DurationComponents{.days = 5}),
            std::make_shared<StringValue>(QStringLiteral("5 days"))
        };
        QCOMPARE(std::make_shared<ListValue>(pair)->unique()->length(), 2);

        // Same toString, different `equals` (DateValue also compares its
        // `hasTime` flag) — both kept.
        QVector<ValuePtr> mixed {
            std::make_shared<DateValue>(QDateTime(QDate(2024, 1, 1), QTime(0, 0)), false),
            std::make_shared<DateValue>(QDateTime(QDate(2024, 1, 1), QTime(0, 0)), true)
        };
        QCOMPARE(std::make_shared<ListValue>(mixed)->unique()->length(), 2);
    }

    // The buckets are concatenated in JS object property order, so
    // integer-like keys come back ascending (1647-tK.js:298-303).
    void testListUniqueOrdersIntegerKeysAscending()
    {
        auto l = list({N(10), N(2), N(1)})->unique();
        QCOMPARE(l->length(), 3);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(0))->data(), 1.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(1))->data(), 2.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(2))->data(), 10.0);
    }

    // The comparator is numeric-aware, then localeCompare on toString()
    // (1647-tK.js:304-310) — not a bare `.sort()`.
    void testListSortComparesNumbersNumericallyNotAsText()
    {
        auto l = list({N(10), N(9), N(100)})->sort();
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(0))->data(), 9.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(1))->data(), 10.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(2))->data(), 100.0);
    }

    void testListSortAscending()
    {
        auto l = list({N(3), N(1), N(2)})->sort();
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(0))->data(), 1.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(2))->data(), 3.0);
    }

    void testListSortNullsLast()
    {
        QVector<ValuePtr> v {N(2), NullValue::instance(), N(1)};
        auto l = std::make_shared<ListValue>(v)->sort();
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(0))->data(), 1.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(1))->data(), 2.0);
        QCOMPARE(l->get(2)->type(), QStringLiteral("Null"));
    }

    void testListSlice()
    {
        auto l = list({N(1), N(2), N(3), N(4)})->slice(1, 3);
        QCOMPARE(l->length(), 2);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(0))->data(), 2.0);
    }

    void testListJoin()
    {
        auto l = list({S("a"), S("b"), S("c")});
        QCOMPARE(l->join(QStringLiteral("-")), QStringLiteral("a-b-c"));
    }

    // ----- numeric aggregates -----

    void testListSum()
    {
        auto l = list({N(1), N(2), N(3)});
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->sum())->data(), 6.0);
    }

    void testListMean()
    {
        auto l = list({N(2), N(4), N(6)});
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->mean())->data(), 4.0);
    }

    void testListMin()
    {
        auto l = list({N(3), N(1), N(2)});
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->min())->data(), 1.0);
    }

    void testListMax()
    {
        auto l = list({N(3), N(1), N(2)});
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->max())->data(), 3.0);
    }

    void testListMedianOdd()
    {
        auto l = list({N(1), N(3), N(2)});
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->median())->data(), 2.0);
    }

    void testListMedianEven()
    {
        auto l = list({N(1), N(2), N(3), N(4)});
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->median())->data(), 2.5);
    }

    void testListStddev()
    {
        // Population stddev of [2,4,4,4,5,5,7,9] == 2.0.
        auto l = list({N(2), N(4), N(4), N(4), N(5), N(5), N(7), N(9)});
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->stddev())->data(), 2.0);
    }

    void testListAggregatesFilterNonNumeric()
    {
        // `getNumbers()` filters non-numerics out rather than poisoning the
        // whole aggregate (app/own/1647-tK.js:234-239), but `mean()` divides
        // by the *full* list length (`:343-345`).
        auto l = list({N(1), S("x")});
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->sum())->data(), 1.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->min())->data(), 1.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->max())->data(), 1.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->median())->data(), 1.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->mean())->data(), 0.5);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->stddev())->data(), 0.0);

        auto allStrings = list({S("x"), S("y")});
        QCOMPARE(allStrings->sum()->type(), QStringLiteral("Null"));
        QCOMPARE(allStrings->mean()->type(), QStringLiteral("Null"));
    }

    void testListAggregateEmptyYieldsNull()
    {
        ListValue empty;
        QCOMPARE(empty.min()->type(), QStringLiteral("Null"));
        QCOMPARE(empty.max()->type(), QStringLiteral("Null"));
        QCOMPARE(empty.mean()->type(), QStringLiteral("Null"));
        QCOMPARE(empty.median()->type(), QStringLiteral("Null"));
        QCOMPARE(empty.stddev()->type(), QStringLiteral("Null"));
    }

    // --- Obsidian's per-column-class summary defaults ---
    // (addendum 2026-10-03, "the available defaults depend on the column's
    // inferred class").

    void testListRangeOverNumbers()
    {
        // Obsidian: `values.max() - values.min()`.
        auto l = list({N(2), N(9), N(5)});
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->range())->data(), 7.0);
    }

    void testListRangeOverDates()
    {
        // Obsidian: `(values.latest() - values.earliest())` — a Duration.
        const QDateTime d1 = QDateTime::fromString(QStringLiteral("2024-01-01"), Qt::ISODate);
        const QDateTime d2 = QDateTime::fromString(QStringLiteral("2024-01-08"), Qt::ISODate);
        auto l = list({std::make_shared<DateValue>(d2, false),
                       std::make_shared<DateValue>(d1, false)});
        auto dur = std::dynamic_pointer_cast<DurationValue>(l->range());
        QVERIFY(dur);
        QCOMPARE(dur->totalMilliseconds(),
                 qint64(7) * 24 * 60 * 60 * 1000);
    }

    void testListRangeEmptyIsNull()
    {
        QCOMPARE(ListValue().range()->type(), QStringLiteral("Null"));
    }

    void testListCheckedAndUnchecked()
    {
        // Obsidian: `values.filter(value == true).length` and
        // `values.filter(value == false).length`. A null value is neither.
        auto l = list({std::make_shared<BooleanValue>(true),
                       std::make_shared<BooleanValue>(false),
                       std::make_shared<BooleanValue>(true),
                       NullValue::instance()});
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->checked())->data(), 2.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->unchecked())->data(), 1.0);
    }

    void testListEmptyAndFilled()
    {
        // Obsidian: `values.filter(value.isType("null")).length` and
        // `values.filter(!value.isType("null")).length`. Only the Null type
        // counts as empty — a false boolean is *not* empty.
        auto l = list({N(1), NullValue::instance(), S("x"),
                       std::make_shared<BooleanValue>(false)});
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->empty())->data(), 1.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->filled())->data(), 3.0);
    }

    void testListEmptyAndFilledOfEmptyList()
    {
        ListValue empty;
        QCOMPARE(std::static_pointer_cast<NumberValue>(empty.empty())->data(), 0.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(empty.filled())->data(), 0.0);
    }
};

QTEST_APPLESS_MAIN(TestListValue)
#include "tst_value_list.moc"
