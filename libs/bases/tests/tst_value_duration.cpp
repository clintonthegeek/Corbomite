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

    void testParseShorthandMsIsUnreachable()
    {
        // The unit alternation is `[dhmswMy]|(?:second|minute|hour|day|week|
        // month|year)[s]?` — no `ms`, and the three millisecond branches
        // below it in app/own/1649-dK.js:245-267 are dead code. Refutes
        // 2026-04-17 §6.1/§14.
        for (const QString &s : {QStringLiteral("100ms"), QStringLiteral("100 millisecond"),
                                 QStringLiteral("100 milliseconds")})
            QVERIFY2(!DurationValue::parseFromString(s),
                     qPrintable(QStringLiteral("duration(\"%1\") must not parse").arg(s)));
    }

    void testIsoBranchRequiresTAndIsUnanchored()
    {
        // The ISO regex has no ^/$ and demands a literal `T`
        // (app/own/1649-dK.js:246), so "P1D" is not an ISO duration at all.
        QVERIFY(!DurationValue::parseFromString(QStringLiteral("P1D")));
        auto embedded = DurationValue::parseFromString(QStringLiteral("foo P1DT bar"));
        QVERIFY(embedded);
        QCOMPARE(embedded->components().days, qint64(1));
        // `,` is accepted as a decimal separator, and the seconds group lands
        // in the seconds slot with milliseconds hard-coded to 0.
        auto comma = DurationValue::parseFromString(QStringLiteral("PT1,5S"));
        QVERIFY(comma);
        QCOMPARE(comma->components().seconds, qint64(1));
        QCOMPARE(comma->components().milliseconds, qint64(0));
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
