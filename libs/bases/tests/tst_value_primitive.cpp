// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/bases/Values.h"

#include <cmath>
#include <limits>

using namespace Corbomite::Bases;

class TestPrimitiveValues : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    // ----- Boolean -----

    void testBoolType()
    {
        QCOMPARE(BooleanValue(true).type(), QStringLiteral("Boolean"));
    }

    void testBoolTruthy()
    {
        QVERIFY(BooleanValue(true).isTruthy());
        QVERIFY(!BooleanValue(false).isTruthy());
    }

    void testBoolEquals()
    {
        BooleanValue t1(true), t2(true), f1(false);
        QVERIFY(t1.equals(t2));
        QVERIFY(!t1.equals(f1));
    }

    void testBoolToString()
    {
        QCOMPARE(BooleanValue(true).toString(), QStringLiteral("true"));
        QCOMPARE(BooleanValue(false).toString(), QStringLiteral("false"));
    }

    // ----- Number -----

    void testNumberType()
    {
        QCOMPARE(NumberValue(3.14).type(), QStringLiteral("Number"));
    }

    void testNumberTruthy()
    {
        QVERIFY(NumberValue(1).isTruthy());
        QVERIFY(NumberValue(-1).isTruthy());
        QVERIFY(!NumberValue(0).isTruthy());
        QVERIFY(!NumberValue(std::nan("")).isTruthy());
    }

    void testNumberToStringInteger()
    {
        QCOMPARE(NumberValue(42).toString(), QStringLiteral("42"));
        QCOMPARE(NumberValue(-100).toString(), QStringLiteral("-100"));
    }

    void testNumberToStringFractional()
    {
        QCOMPARE(NumberValue(3.14).toString(), QStringLiteral("3.14"));
    }

    void testNumberInfinityRender()
    {
        QCOMPARE(NumberValue(std::numeric_limits<double>::infinity()).toString(),
                 QStringLiteral("∞"));
    }

    void testNumberNanRender()
    {
        QCOMPARE(NumberValue(std::nan("")).toString(), QStringLiteral("NaN"));
    }

    void testNumberLooseEqualsBoolean()
    {
        NumberValue one(1);
        NumberValue two(2);
        BooleanValue t(true);
        QVERIFY(Value::staticLooseEquals(&one, &t));
        QVERIFY(!Value::staticLooseEquals(&two, &t));
    }

    // ----- String -----

    void testStringType()
    {
        QCOMPARE(StringValue(QStringLiteral("hi")).type(), QStringLiteral("String"));
    }

    void testStringTruthyByLength()
    {
        QVERIFY(StringValue(QStringLiteral("x")).isTruthy());
        QVERIFY(!StringValue(QString{}).isTruthy());
    }

    void testStringLengthObjectAccess()
    {
        StringValue s(QStringLiteral("hello"));
        auto len = s.objectAccess(QStringLiteral("length"));
        QVERIFY(len);
        QCOMPARE(len->type(), QStringLiteral("Number"));
        QCOMPARE(len->toString(), QStringLiteral("5"));
    }

    void testStringEquals()
    {
        StringValue a(QStringLiteral("x"));
        StringValue b(QStringLiteral("x"));
        StringValue c(QStringLiteral("y"));
        QVERIFY(a.equals(b));
        QVERIFY(!a.equals(c));
    }
};

QTEST_APPLESS_MAIN(TestPrimitiveValues)
#include "tst_value_primitive.moc"
