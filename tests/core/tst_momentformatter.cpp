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
        // Letters known NOT to be tokens (after the Y/Q/e/E/k/Z/L/l
        // additions): b, c, f, i, j, n, o, p, q, r, t, u, v.
        const QDateTime dt = fixture();
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("input-out")),
                 QStringLiteral("input-out"));
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
        // `?` is intentionally not a Moment token in any version. Since the
        // dispatch table got expanded to cover Y/Q/Z/ZZ/L/LL/etc., we use
        // a punctuation literal here.
        const QDateTime dt = fixture();
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("???")),
                 QStringLiteral("???"));
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

    void testYearWithSign()
    {
        // Moment `Y` — full year, no padding (no `+` sign for in-range years).
        const QDateTime dt = fixture();
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("Y")),
                 QStringLiteral("2026"));
    }

    void testQuarter()
    {
        // Q1: Jan-Mar, Q2: Apr-Jun, Q3: Jul-Sep, Q4: Oct-Dec.
        const QDateTime jan(QDate(2026, 1, 15), QTime(0, 0));
        const QDateTime apr(QDate(2026, 4, 15), QTime(0, 0));
        const QDateTime jul(QDate(2026, 7, 15), QTime(0, 0));
        const QDateTime dec(QDate(2026, 12, 15), QTime(0, 0));
        QCOMPARE(MomentFormatter::format(jan, QStringLiteral("Q")),
                 QStringLiteral("1"));
        QCOMPARE(MomentFormatter::format(apr, QStringLiteral("Q")),
                 QStringLiteral("2"));
        QCOMPARE(MomentFormatter::format(jul, QStringLiteral("Q")),
                 QStringLiteral("3"));
        QCOMPARE(MomentFormatter::format(dec, QStringLiteral("Q")),
                 QStringLiteral("4"));
    }

    void testWeekYearGgGggg()
    {
        // 2026-01-01 is a Thursday — ISO week 1 of 2026, week year 2026.
        const QDateTime y(QDate(2026, 1, 1), QTime(0, 0));
        QCOMPARE(MomentFormatter::format(y, QStringLiteral("gg")),
                 QStringLiteral("26"));
        QCOMPARE(MomentFormatter::format(y, QStringLiteral("gggg")),
                 QStringLiteral("2026"));
    }

    void testIsoDayOfWeekE()
    {
        // E: Mon=1, Tue=2, ..., Sun=7.
        const QLocale en(QLocale::English, QLocale::UnitedStates);
        const QDateTime mon(QDate(2026, 4, 13), QTime(0, 0));
        const QDateTime sun(QDate(2026, 4, 12), QTime(0, 0));
        QCOMPARE(MomentFormatter::format(mon, QStringLiteral("E"), en),
                 QStringLiteral("1"));
        QCOMPARE(MomentFormatter::format(sun, QStringLiteral("E"), en),
                 QStringLiteral("7"));
    }

    void testLocaleDayOfWeekE()
    {
        // e: Locale-aware 0-based. en starts on Sun → Sun=0, Sat=6.
        const QLocale en(QLocale::English, QLocale::UnitedStates);
        const QDateTime sun(QDate(2026, 4, 12), QTime(0, 0));
        const QDateTime wed(QDate(2026, 4, 15), QTime(0, 0));
        const QDateTime sat(QDate(2026, 4, 18), QTime(0, 0));
        QCOMPARE(MomentFormatter::format(sun, QStringLiteral("e"), en),
                 QStringLiteral("0"));
        QCOMPARE(MomentFormatter::format(wed, QStringLiteral("e"), en),
                 QStringLiteral("3"));
        QCOMPARE(MomentFormatter::format(sat, QStringLiteral("e"), en),
                 QStringLiteral("6"));
    }

    void testHourKkk()
    {
        // k/kk: 1-24 with midnight rendered as 24.
        const QDateTime midnight(QDate(2026, 4, 15), QTime(0, 30, 0));
        const QDateTime afternoon(QDate(2026, 4, 15), QTime(14, 30, 0));
        QCOMPARE(MomentFormatter::format(midnight, QStringLiteral("k")),
                 QStringLiteral("24"));
        QCOMPARE(MomentFormatter::format(midnight, QStringLiteral("kk")),
                 QStringLiteral("24"));
        QCOMPARE(MomentFormatter::format(afternoon, QStringLiteral("k")),
                 QStringLiteral("14"));
        QCOMPARE(MomentFormatter::format(afternoon, QStringLiteral("kk")),
                 QStringLiteral("14"));
    }

    void testTimezoneOffsetZ()
    {
        // Build a QDateTime with an explicit fixed UTC offset so the test
        // is stable across CI host timezones.
        const QDateTime dt(QDate(2026, 4, 15), QTime(12, 0, 0),
                            QTimeZone::fromSecondsAheadOfUtc(7 * 3600 + 30 * 60));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("Z")),
                 QStringLiteral("+07:30"));
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("ZZ")),
                 QStringLiteral("+0730"));

        const QDateTime west(QDate(2026, 4, 15), QTime(12, 0, 0),
                              QTimeZone::fromSecondsAheadOfUtc(-(5 * 3600)));
        QCOMPARE(MomentFormatter::format(west, QStringLiteral("Z")),
                 QStringLiteral("-05:00"));
        QCOMPARE(MomentFormatter::format(west, QStringLiteral("ZZ")),
                 QStringLiteral("-0500"));
    }

    void testLocaleShortcuts()
    {
        const QDateTime dt = fixture();
        const QLocale en(QLocale::English, QLocale::UnitedStates);

        // LT — h:mm A → "2:30 PM" (no leading zero on hour, AM/PM uppercase).
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("LT"), en),
                 QStringLiteral("2:30 PM"));
        // LTS — h:mm:ss A
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("LTS"), en),
                 QStringLiteral("2:30:45 PM"));
        // L — MM/DD/YYYY
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("L"), en),
                 QStringLiteral("04/15/2026"));
        // l — M/D/YYYY (no leading zeros)
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("l"), en),
                 QStringLiteral("4/15/2026"));
        // LL — MMMM D, YYYY
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("LL"), en),
                 QStringLiteral("April 15, 2026"));
        // ll — MMM D, YYYY
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("ll"), en),
                 QStringLiteral("Apr 15, 2026"));
        // LLL — MMMM D, YYYY h:mm A
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("LLL"), en),
                 QStringLiteral("April 15, 2026 2:30 PM"));
        // lll — MMM D, YYYY h:mm A
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("lll"), en),
                 QStringLiteral("Apr 15, 2026 2:30 PM"));
        // LLLL — dddd, MMMM D, YYYY h:mm A
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("LLLL"), en),
                 QStringLiteral("Wednesday, April 15, 2026 2:30 PM"));
        // llll — ddd, MMM D, YYYY h:mm A
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("llll"), en),
                 QStringLiteral("Wed, Apr 15, 2026 2:30 PM"));
    }

    void testEscapedNewTokensStayLiteral()
    {
        // The new tokens (Y, Q, Z, etc.) inside [...] must remain literal —
        // the escape pass runs before tokenization.
        const QDateTime dt = fixture();
        QCOMPARE(MomentFormatter::format(dt, QStringLiteral("[Y Q ZZ LL]")),
                 QStringLiteral("Y Q ZZ LL"));
    }
};

QTEST_MAIN(TestMomentFormatter)
#include "tst_momentformatter.moc"
