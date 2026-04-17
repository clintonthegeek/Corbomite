// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QDate>
#include <QDateTime>
#include <QTime>

#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

class TestDateValue : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void testDateType()
    {
        DateValue d(QDateTime(QDate(2024, 1, 15), QTime(0, 0)), false);
        QCOMPARE(d.type(), QStringLiteral("Date"));
    }

    void testDateIsAlwaysTruthy()
    {
        DateValue d(QDateTime(QDate(2024, 1, 15), QTime(0, 0)), false);
        QVERIFY(d.isTruthy());
        QVERIFY(!d.isEmpty());
    }

    void testParseDateOnly()
    {
        auto d = DateValue::parseFromString(QStringLiteral("2024-01-15"));
        QVERIFY(d);
        QVERIFY(!d->hasTime());
        QCOMPARE(d->dateTime().date(), QDate(2024, 1, 15));
    }

    void testParseDateTimeWithSeconds()
    {
        auto d = DateValue::parseFromString(QStringLiteral("2024-01-15T14:30:00"));
        QVERIFY(d);
        QVERIFY(d->hasTime());
        QCOMPARE(d->dateTime().date(), QDate(2024, 1, 15));
        QCOMPARE(d->dateTime().time(), QTime(14, 30, 0));
    }

    void testParseDateTimeWithSpaceSeparator()
    {
        auto d = DateValue::parseFromString(QStringLiteral("2024-01-15 14:30:00"));
        QVERIFY(d);
        QVERIFY(d->hasTime());
    }

    void testParseDateTimeWithMilliseconds()
    {
        auto d = DateValue::parseFromString(QStringLiteral("2024-01-15T14:30:45.123"));
        QVERIFY(d);
        QCOMPARE(d->dateTime().time().msec(), 123);
    }

    void testParseRejectsMalformed()
    {
        QVERIFY(!DateValue::parseFromString(QStringLiteral("not a date")));
        QVERIFY(!DateValue::parseFromString(QStringLiteral("2024/01/15")));
        QVERIFY(!DateValue::parseFromString(QStringLiteral("2024-13-99")));
    }

    void testDateObjectAccessFields()
    {
        auto d = DateValue::parseFromString(QStringLiteral("2024-03-07T09:15:30.456"));
        QVERIFY(d);
        auto year = d->objectAccess(QStringLiteral("year"));
        QCOMPARE(std::static_pointer_cast<NumberValue>(year)->data(), 2024.0);
        auto month = d->objectAccess(QStringLiteral("month"));
        QCOMPARE(std::static_pointer_cast<NumberValue>(month)->data(), 3.0);
        auto day = d->objectAccess(QStringLiteral("day"));
        QCOMPARE(std::static_pointer_cast<NumberValue>(day)->data(), 7.0);
        auto hour = d->objectAccess(QStringLiteral("hour"));
        QCOMPARE(std::static_pointer_cast<NumberValue>(hour)->data(), 9.0);
        auto ms = d->objectAccess(QStringLiteral("millisecond"));
        QCOMPARE(std::static_pointer_cast<NumberValue>(ms)->data(), 456.0);
        auto ts = d->objectAccess(QStringLiteral("timestamp"));
        QCOMPARE(ts->type(), QStringLiteral("Number"));
    }

    void testDateEqualsIgnoresTimeFlagDifference()
    {
        auto a = DateValue::parseFromString(QStringLiteral("2024-01-15"));
        auto b = DateValue::parseFromString(QStringLiteral("2024-01-15T00:00:00"));
        QVERIFY(a && b);
        // They have the same underlying timestamp but different time-flag.
        // Per audit §4.1: DateValue.equals is time-flag-aware — these are
        // NOT equal because one has hasTime() == false.
        QVERIFY(!a->equals(*b));
    }

    void testDateLooseEqualsWithString()
    {
        auto d = DateValue::parseFromString(QStringLiteral("2024-01-15"));
        QVERIFY(d);
        StringValue s(QStringLiteral("2024-01-15"));
        QVERIFY(d->looseEquals(s));
    }

    void testRelativeDatePastDays()
    {
        // Fixture: 5 days ago.
        QDateTime past = QDateTime::currentDateTime().addDays(-5);
        RelativeDateValue r(past, true);
        const QString s = r.toString();
        QVERIFY(s.contains(QLatin1String("days ago")));
    }

    // --- list date aggregates (now fully working with DateValue) ---

    void testListEarliestLatest()
    {
        auto d1 = DateValue::parseFromString(QStringLiteral("2024-01-01"));
        auto d2 = DateValue::parseFromString(QStringLiteral("2024-06-15"));
        auto d3 = DateValue::parseFromString(QStringLiteral("2024-12-31"));
        QVector<ValuePtr> v {d1, d2, d3};
        auto l = std::make_shared<ListValue>(v);

        auto e = l->earliest();
        QCOMPARE(e->type(), QStringLiteral("Date"));
        QCOMPARE(std::static_pointer_cast<DateValue>(e)->dateTime().date(),
                 QDate(2024, 1, 1));

        auto late = l->latest();
        QCOMPARE(std::static_pointer_cast<DateValue>(late)->dateTime().date(),
                 QDate(2024, 12, 31));
    }
};

QTEST_APPLESS_MAIN(TestDateValue)
#include "tst_value_date.moc"
