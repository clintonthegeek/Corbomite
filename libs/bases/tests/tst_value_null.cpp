// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

class TestNullValue : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void testNullIsSingleton()
    {
        auto a = NullValue::instance();
        auto b = NullValue::instance();
        QCOMPARE(a.get(), b.get());
    }

    void testNullType()
    {
        QCOMPARE(NullValue::instance()->type(), QStringLiteral("Null"));
    }

    void testNullIsNotTruthy()
    {
        QVERIFY(!NullValue::instance()->isTruthy());
    }

    void testNullIsEmpty()
    {
        QVERIFY(NullValue::instance()->isEmpty());
    }

    void testNullToString()
    {
        QCOMPARE(NullValue::instance()->toString(), QString{});
    }

    void testStaticEqualsBothNull()
    {
        QVERIFY(Value::staticEquals(NullValue::instance(),
                                    NullValue::instance()));
    }

    void testStaticEqualsNullAgainstNonNull()
    {
        // staticEquals(ptr, nullptr) is false.
        QVERIFY(!Value::staticEquals(NullValue::instance().get(), nullptr));
    }
};

QTEST_APPLESS_MAIN(TestNullValue)
#include "tst_value_null.moc"
