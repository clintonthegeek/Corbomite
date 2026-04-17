// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

class TestDurationValue : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void testType()
    {
        DurationValue d(DurationComponents{.days = 1});
        QCOMPARE(d.type(), QStringLiteral("Duration"));
    }

    void testIsTruthyIfAnyNonZero()
    {
        DurationValue zero({});
        QVERIFY(!zero.isTruthy());
        DurationValue oneDay(DurationComponents{.days = 1});
        QVERIFY(oneDay.isTruthy());
    }

    void testParseIso8601Full()
    {
        auto d = DurationValue::parseFromString(QStringLiteral("P1Y2M3W4DT5H6M7S"));
        QVERIFY(d);
        const auto &c = d->components();
        QCOMPARE(c.years, qint64(1));
        QCOMPARE(c.months, qint64(2));
        QCOMPARE(c.days, qint64(25));  // 3w + 4d
        QCOMPARE(c.hours, qint64(5));
        QCOMPARE(c.minutes, qint64(6));
        QCOMPARE(c.seconds, qint64(7));
    }

    void testParseShorthandDays()
    {
        auto d = DurationValue::parseFromString(QStringLiteral("5 days"));
        QVERIFY(d);
        QCOMPARE(d->components().days, qint64(5));
    }

    void testParseShorthandSingular()
    {
        auto d = DurationValue::parseFromString(QStringLiteral("1 year"));
        QVERIFY(d);
        QCOMPARE(d->components().years, qint64(1));
    }

    void testParseShorthandAbbrevNegative()
    {
        auto d = DurationValue::parseFromString(QStringLiteral("-2h"));
        QVERIFY(d);
        QCOMPARE(d->components().hours, qint64(-2));
    }

    void testParseShorthandMsUndocumented()
    {
        // addendum §14: `ms` is accepted even though help docs don't list it.
        auto d = DurationValue::parseFromString(QStringLiteral("100ms"));
        QVERIFY(d);
        QCOMPARE(d->components().milliseconds, qint64(100));
    }

    void testParseCaseSensitivityBetweenMonthAndMinute()
    {
        // `M` = months, `m` = minutes (addendum §6.1).
        auto months = DurationValue::parseFromString(QStringLiteral("3M"));
        QVERIFY(months);
        QCOMPARE(months->components().months, qint64(3));
        QCOMPARE(months->components().minutes, qint64(0));

        auto minutes = DurationValue::parseFromString(QStringLiteral("3m"));
        QVERIFY(minutes);
        QCOMPARE(minutes->components().minutes, qint64(3));
        QCOMPARE(minutes->components().months, qint64(0));
    }

    void testParseReject()
    {
        QVERIFY(!DurationValue::parseFromString(QStringLiteral("banana")));
        QVERIFY(!DurationValue::parseFromString(QString{}));
    }

    void testAddToDateCalendarAware()
    {
        // Feb 15 + 1 month = Mar 15 (calendar-aware via QDate::addMonths).
        auto base = DateValue::parseFromString(QStringLiteral("2024-02-15"));
        QVERIFY(base);
        DurationValue one(DurationComponents{.months = 1});
        auto shifted = one.addToDate(*base);
        QCOMPARE(shifted->dateTime().date(), QDate(2024, 3, 15));
    }

    void testAddToDateSubtract()
    {
        auto base = DateValue::parseFromString(QStringLiteral("2024-03-15"));
        QVERIFY(base);
        DurationValue one(DurationComponents{.days = 5});
        auto shifted = one.addToDate(*base, /*subtract=*/true);
        QCOMPARE(shifted->dateTime().date(), QDate(2024, 3, 10));
    }

    void testLooseEqualsWithString()
    {
        auto d = DurationValue::parseFromString(QStringLiteral("5 days"));
        QVERIFY(d);
        StringValue s(QStringLiteral("5 days"));
        QVERIFY(d->looseEquals(s));
    }

    void testObjectAccessWeeks()
    {
        DurationValue d(DurationComponents{.days = 14});
        auto w = d.objectAccess(QStringLiteral("weeks"));
        QCOMPARE(std::static_pointer_cast<NumberValue>(w)->data(), 2.0);
    }

    void testTimesScalar()
    {
        DurationValue d(DurationComponents{.hours = 2});
        auto tripled = d.timesScalar(3.0);
        QCOMPARE(tripled.hours, qint64(6));
    }

    void testFromMilliseconds()
    {
        auto d = DurationValue::fromMilliseconds(1500);
        QCOMPARE(d->components().milliseconds, qint64(1500));
    }
};

QTEST_APPLESS_MAIN(TestDurationValue)
#include "tst_value_duration.moc"
