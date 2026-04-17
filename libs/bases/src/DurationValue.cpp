// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"

#include <QDate>
#include <QRegularExpression>
#include <QStringList>
#include <QTime>
#include <QTimeZone>

namespace Corbomite::Bases {

namespace {

constexpr qint64 kMsPerSec = 1000LL;
constexpr qint64 kMsPerMinute = 60LL * kMsPerSec;
constexpr qint64 kMsPerHour = 60LL * kMsPerMinute;
constexpr qint64 kMsPerDay = 24LL * kMsPerHour;
// Approximate month / year for `totalMilliseconds` — exact for zero-year /
// zero-month durations, approximate otherwise.
constexpr qint64 kMsPerMonthApprox = 30LL * kMsPerDay;
constexpr qint64 kMsPerYearApprox = 365LL * kMsPerDay + 6LL * kMsPerHour;

}  // namespace

qint64 DurationValue::totalMilliseconds() const
{
    return m_c.years * kMsPerYearApprox
         + m_c.months * kMsPerMonthApprox
         + m_c.days * kMsPerDay
         + m_c.hours * kMsPerHour
         + m_c.minutes * kMsPerMinute
         + m_c.seconds * kMsPerSec
         + m_c.milliseconds;
}

QString DurationValue::toString() const
{
    QStringList parts;
    auto push = [&](qint64 v, const char *unit) {
        if (v == 0) return;
        QString u = QString::fromLatin1(unit);
        if (v != 1) u += QLatin1Char('s');
        parts.append(QStringLiteral("%1 %2").arg(v).arg(u));
    };
    push(m_c.years, "year");
    push(m_c.months, "month");
    push(m_c.days, "day");
    push(m_c.hours, "hour");
    push(m_c.minutes, "minute");
    push(m_c.seconds, "second");
    push(m_c.milliseconds, "millisecond");
    if (parts.isEmpty()) return QStringLiteral("0");
    return parts.join(QLatin1Char(' '));
}

bool DurationValue::equals(const Value &other) const
{
    auto *rhs = dynamic_cast<const DurationValue *>(&other);
    if (!rhs) return false;
    return totalMilliseconds() == rhs->totalMilliseconds();
}

bool DurationValue::looseEquals(const Value &other) const
{
    if (equals(other)) return true;
    if (auto *s = dynamic_cast<const StringValue *>(&other)) {
        if (auto d = parseFromString(s->data()))
            return equals(*d);
    }
    return false;
}

ValuePtr DurationValue::objectAccess(const QString &key) const
{
    const QString k = key.toLower();
    if (k == QLatin1String("years"))    return std::make_shared<NumberValue>(m_c.years);
    if (k == QLatin1String("months"))   return std::make_shared<NumberValue>(m_c.months);
    if (k == QLatin1String("weeks"))
        return std::make_shared<NumberValue>(static_cast<double>(m_c.days) / 7.0);
    if (k == QLatin1String("days"))     return std::make_shared<NumberValue>(m_c.days);
    if (k == QLatin1String("hours"))    return std::make_shared<NumberValue>(m_c.hours);
    if (k == QLatin1String("minutes"))  return std::make_shared<NumberValue>(m_c.minutes);
    if (k == QLatin1String("seconds")) return std::make_shared<NumberValue>(m_c.seconds);
    if (k == QLatin1String("milliseconds"))
        return std::make_shared<NumberValue>(m_c.milliseconds);
    return nullptr;
}

QStringList DurationValue::keys() const
{
    return {
        QStringLiteral("years"), QStringLiteral("months"), QStringLiteral("weeks"),
        QStringLiteral("days"), QStringLiteral("hours"), QStringLiteral("minutes"),
        QStringLiteral("seconds"), QStringLiteral("milliseconds")
    };
}

std::shared_ptr<DateValue> DurationValue::addToDate(const DateValue &d, bool subtract) const
{
    const int sign = subtract ? -1 : 1;
    QDate date = d.dateTime().date();
    date = date.addYears(static_cast<int>(sign * m_c.years));
    date = date.addMonths(static_cast<int>(sign * m_c.months));
    date = date.addDays(static_cast<int>(sign * m_c.days));
    QDateTime dt(date, d.dateTime().time(), d.dateTime().timeRepresentation());
    const qint64 msDelta = sign * (m_c.hours * kMsPerHour
                                   + m_c.minutes * kMsPerMinute
                                   + m_c.seconds * kMsPerSec
                                   + m_c.milliseconds);
    dt = dt.addMSecs(msDelta);
    return std::make_shared<DateValue>(dt, d.hasTime());
}

DurationComponents DurationValue::plus(const DurationComponents &o) const
{
    return {
        m_c.years + o.years, m_c.months + o.months, m_c.days + o.days,
        m_c.hours + o.hours, m_c.minutes + o.minutes,
        m_c.seconds + o.seconds, m_c.milliseconds + o.milliseconds
    };
}

DurationComponents DurationValue::minus(const DurationComponents &o) const
{
    return {
        m_c.years - o.years, m_c.months - o.months, m_c.days - o.days,
        m_c.hours - o.hours, m_c.minutes - o.minutes,
        m_c.seconds - o.seconds, m_c.milliseconds - o.milliseconds
    };
}

DurationComponents DurationValue::timesScalar(double n) const
{
    auto mul = [n](qint64 v) -> qint64 {
        return static_cast<qint64>(static_cast<double>(v) * n);
    };
    return {
        mul(m_c.years), mul(m_c.months), mul(m_c.days), mul(m_c.hours),
        mul(m_c.minutes), mul(m_c.seconds), mul(m_c.milliseconds)
    };
}

std::shared_ptr<DurationValue> DurationValue::parseFromString(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) return nullptr;

    // ISO-8601: P[nY][nM][nW][nD][T[nH][nM][nS]]
    static const QRegularExpression iso(
        QStringLiteral(R"(^P(?:(\d+)Y)?(?:(\d+)M)?(?:(\d+)W)?(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+(?:\.\d+)?)S)?)?$)"));
    auto m = iso.match(t);
    if (m.hasMatch()) {
        DurationComponents c{};
        if (!m.captured(1).isEmpty()) c.years = m.captured(1).toLongLong();
        if (!m.captured(2).isEmpty()) c.months = m.captured(2).toLongLong();
        if (!m.captured(3).isEmpty()) c.days += m.captured(3).toLongLong() * 7;
        if (!m.captured(4).isEmpty()) c.days += m.captured(4).toLongLong();
        if (!m.captured(5).isEmpty()) c.hours = m.captured(5).toLongLong();
        if (!m.captured(6).isEmpty()) c.minutes = m.captured(6).toLongLong();
        if (!m.captured(7).isEmpty()) {
            const double sec = m.captured(7).toDouble();
            c.seconds = static_cast<qint64>(sec);
            c.milliseconds = static_cast<qint64>((sec - c.seconds) * 1000.0);
        }
        if (c.isZero() && t == QLatin1String("P")) return nullptr;
        return std::make_shared<DurationValue>(c);
    }

    // Shorthand: "(-?\d+)\s*(unit)" — unit case-sensitive per addendum §6.1.
    static const QRegularExpression shorthand(
        QStringLiteral(R"(^(-?\d+)\s*(y|year|years|M|month|months|w|week|weeks|d|day|days|h|hour|hours|m|minute|minutes|s|second|seconds|ms|millisecond|milliseconds)$)"));
    m = shorthand.match(t);
    if (!m.hasMatch()) return nullptr;
    const qint64 n = m.captured(1).toLongLong();
    const QString unit = m.captured(2);
    DurationComponents c{};
    if (unit == QLatin1String("y") || unit == QLatin1String("year") || unit == QLatin1String("years"))
        c.years = n;
    else if (unit == QLatin1String("M") || unit == QLatin1String("month") || unit == QLatin1String("months"))
        c.months = n;
    else if (unit == QLatin1String("w") || unit == QLatin1String("week") || unit == QLatin1String("weeks"))
        c.days = n * 7;
    else if (unit == QLatin1String("d") || unit == QLatin1String("day") || unit == QLatin1String("days"))
        c.days = n;
    else if (unit == QLatin1String("h") || unit == QLatin1String("hour") || unit == QLatin1String("hours"))
        c.hours = n;
    else if (unit == QLatin1String("m") || unit == QLatin1String("minute") || unit == QLatin1String("minutes"))
        c.minutes = n;
    else if (unit == QLatin1String("s") || unit == QLatin1String("second") || unit == QLatin1String("seconds"))
        c.seconds = n;
    else if (unit == QLatin1String("ms") || unit == QLatin1String("millisecond") || unit == QLatin1String("milliseconds"))
        c.milliseconds = n;
    else
        return nullptr;
    return std::make_shared<DurationValue>(c);
}

std::shared_ptr<DurationValue> DurationValue::fromMilliseconds(qint64 ms)
{
    DurationComponents c{};
    c.milliseconds = ms;
    // Do not attempt to distribute into larger units — leave as ms for
    // exact arithmetic. Consumers can objectAccess("days") for days-as-ratio.
    return std::make_shared<DurationValue>(c);
}

}  // namespace Corbomite::Bases
