// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

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

    void testListAggregateOnNonNumericYieldsNull()
    {
        auto l = list({N(1), S("x")});
        QCOMPARE(l->sum()->type(), QStringLiteral("Null"));
        QCOMPARE(l->mean()->type(), QStringLiteral("Null"));
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
};

QTEST_APPLESS_MAIN(TestListValue)
#include "tst_value_list.moc"
