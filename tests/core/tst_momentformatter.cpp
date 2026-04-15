// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QLocale>
#include <QtCore/QTime>
#include <QtCore/QTimeZone>

#include "corbomite/core/MomentFormatter.h"

using namespace Corbomite;

class TestMomentFormatter : public QObject
{
    Q_OBJECT

private:
    // Canonical fixture: Wednesday, April 15 2026 14:30:45.123 local time.
    QDateTime fixture() const
    {
        return QDateTime(QDate(2026, 4, 15), QTime(14, 30, 45, 123));
    }

private Q_SLOTS:

    void initTestCase()
    {
        // Lock locale so day/month name assertions are deterministic.
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    void testEmpty()
    {
        const QDateTime dt = fixture();
        QCOMPARE(MomentFormatter::format(dt, QString{}), QString{});
    }

    void testLiteralPassThrough()
    {
        // Strings with no Moment token characters pass through verbatim.
        // ("hello" contains 'h' which Moment treats as a 12-hour token;
        // use a string composed only of non-token characters.)
        const QDateTime dt = fixture();
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("note-QR")),
                 QStringLiteral("note-QR"));
    }

    void testYearTokens()
    {
        const QDateTime dt = fixture();
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("YYYY")),
                 QStringLiteral("2026"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("YY")),
                 QStringLiteral("26"));
    }

    void testMonthTokens()
    {
        const QDateTime dt = fixture();
        const QLocale en(QLocale::English, QLocale::UnitedStates);
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("M"), en),
                 QStringLiteral("4"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("MM"), en),
                 QStringLiteral("04"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("MMM"), en),
                 QStringLiteral("Apr"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("MMMM"), en),
                 QStringLiteral("April"));
    }

    void testDayTokens()
    {
        const QDateTime dt = fixture();
        const QLocale en(QLocale::English, QLocale::UnitedStates);
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("D"), en),
                 QStringLiteral("15"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("DD"), en),
                 QStringLiteral("15"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("Do"), en),
                 QStringLiteral("15th"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("dddd"), en),
                 QStringLiteral("Wednesday"));
    }

    void testOrdinalDaySuffixes()
    {
        const QLocale en(QLocale::English, QLocale::UnitedStates);
        struct Case { int day; const char *expected; };
        const Case cases[] = {
            {1, "1st"}, {2, "2nd"}, {3, "3rd"}, {4, "4th"},
            {11, "11th"}, {12, "12th"}, {13, "13th"},
            {21, "21st"}, {22, "22nd"}, {23, "23rd"},
            {31, "31st"},
        };
        for (const auto &c : cases) {
            const QDateTime dt(QDate(2026, 1, c.day), QTime(0, 0));
            QCOMPARE(MomentFormatter::format(dt, QStringLiteral("Do"), en),
                     QString::fromLatin1(c.expected));
        }
    }

    void testWeekOfYear()
    {
        // 2026-01-05 is a Monday, in ISO week 2 (week 1 is 2025-12-29..2026-01-04).
        const QDateTime dt(QDate(2026, 1, 5), QTime(0, 0));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("w")),
                 QStringLiteral("2"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("ww")),
                 QStringLiteral("02"));
    }

    void testHourTokens()
    {
        const QDateTime dt = fixture();
        const QLocale en(QLocale::English, QLocale::UnitedStates);
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("HH"), en),
                 QStringLiteral("14"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("H"), en),
                 QStringLiteral("14"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("h"), en),
                 QStringLiteral("2"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("hh"), en),
                 QStringLiteral("02"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("a"), en),
                 QStringLiteral("pm"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("A"), en),
                 QStringLiteral("PM"));
    }

    void testMinuteSecondMs()
    {
        const QDateTime dt = fixture();
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("mm")),
                 QStringLiteral("30"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("m")),
                 QStringLiteral("30"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("ss")),
                 QStringLiteral("45"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("s")),
                 QStringLiteral("45"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("SSS")),
                 QStringLiteral("123"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("SS")),
                 QStringLiteral("12"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("S")),
                 QStringLiteral("1"));
    }

    void testEscapeBrackets()
    {
        const QDateTime dt = fixture();
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("YYYY [at] HH:mm")),
                 QStringLiteral("2026 at 14:30"));
    }

    void testMultipleEscapeBrackets()
    {
        const QDateTime dt = fixture();
        const QLocale en(QLocale::English, QLocale::UnitedStates);
        QCOMPARE(MomentFormatter::format(dt,
                     QStringLiteral("[Year:] YYYY [Month:] MMMM"), en),
                 QStringLiteral("Year: 2026 Month: April"));
    }

    void testUnknownTokenPassesThrough()
    {
        const QDateTime dt = fixture();
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("ZZ")),
                 QStringLiteral("ZZ"));
    }

    void testComplexRealistic()
    {
        const QDateTime dt = fixture();
        const QLocale en(QLocale::English, QLocale::UnitedStates);
        QCOMPARE(MomentFormatter::format(dt,
                     QStringLiteral("YYYY-MM-DD dddd Do [at] HH:mm"), en),
                 QStringLiteral("2026-04-15 Wednesday 15th at 14:30"));
    }

    void testUnixTimestamp()
    {
        // UTC-anchored fixture: 2026-01-01T00:00:00Z → 1767225600 seconds.
        const QDateTime dt(QDate(2026, 1, 1), QTime(0, 0), QTimeZone::UTC);
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("X")),
                 QString::number(dt.toSecsSinceEpoch()));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("x")),
                 QString::number(dt.toMSecsSinceEpoch()));
        // Sanity: value is the expected UNIX seconds.
        QCOMPARE(dt.toSecsSinceEpoch(), qint64(1767225600));
    }

    void testDayOfYear()
    {
        const QDateTime dt(QDate(2026, 1, 15), QTime(0, 0));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("DDD")),
                 QStringLiteral("15"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("DDDD")),
                 QStringLiteral("015"));
    }

    void testLocaleAwareMonthName()
    {
        const QDateTime dt = fixture();
        const QLocale fr(QLocale::French);
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("MMMM"), fr),
                 QStringLiteral("avril"));
    }

    void testDateOnlyFormat()
    {
        const QDateTime dt = fixture();
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("YYYY-MM-DD")),
                 QStringLiteral("2026-04-15"));
    }

    void testDayOfWeekNumericMoment()
    {
        // 2026-04-12 is a Sunday. Moment: Sun=0.
        const QDateTime dt(QDate(2026, 4, 12), QTime(0, 0));
        QCOMPARE(dt.date().dayOfWeek(), 7);  // Qt: Sun=7, sanity check.
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("d")),
                 QStringLiteral("0"));
    }
};

QTEST_MAIN(TestMomentFormatter)
#include "tst_momentformatter.moc"
