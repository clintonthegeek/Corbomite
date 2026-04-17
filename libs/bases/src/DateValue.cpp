// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"

#include <QRegularExpression>
#include <QTime>
#include <QTimeZone>

namespace Corbomite::Bases {

QString DateValue::toString() const
{
    if (m_hasTime)
        return m_dt.toString(Qt::ISODateWithMs);
    return m_dt.date().toString(Qt::ISODate);
}

bool DateValue::equals(const Value &other) const
{
    auto *rhs = dynamic_cast<const DateValue *>(&other);
    if (!rhs) return false;
    // Must agree on the time-portion-present flag to be structurally equal.
    if (m_hasTime != rhs->m_hasTime) return false;
    return m_dt == rhs->m_dt;
}

bool DateValue::looseEquals(const Value &other) const
{
    if (equals(other)) return true;
    // Coerce StringValue to Date via parseFromString and retry.
    if (auto *s = dynamic_cast<const StringValue *>(&other)) {
        if (auto d = parseFromString(s->data()))
            return equals(*d);
    }
    return false;
}

ValuePtr DateValue::objectAccess(const QString &key) const
{
    const QString k = key.toLower();
    if (k == QLatin1String("year"))
        return std::make_shared<NumberValue>(m_dt.date().year());
    if (k == QLatin1String("month"))
        return std::make_shared<NumberValue>(m_dt.date().month());  // 1-based
    if (k == QLatin1String("day"))
        return std::make_shared<NumberValue>(m_dt.date().day());
    if (k == QLatin1String("hour"))
        return std::make_shared<NumberValue>(m_dt.time().hour());
    if (k == QLatin1String("minute"))
        return std::make_shared<NumberValue>(m_dt.time().minute());
    if (k == QLatin1String("second"))
        return std::make_shared<NumberValue>(m_dt.time().second());
    if (k == QLatin1String("millisecond"))
        return std::make_shared<NumberValue>(m_dt.time().msec());
    if (k == QLatin1String("timestamp"))
        return std::make_shared<NumberValue>(static_cast<double>(m_dt.toMSecsSinceEpoch()));
    return nullptr;
}

QStringList DateValue::keys() const
{
    return {
        QStringLiteral("year"),    QStringLiteral("month"),
        QStringLiteral("day"),     QStringLiteral("hour"),
        QStringLiteral("minute"),  QStringLiteral("second"),
        QStringLiteral("millisecond"), QStringLiteral("timestamp")
    };
}

std::shared_ptr<DateValue> DateValue::parseFromString(const QString &text)
{
    // Date-only: YYYY-MM-DD
    static const QRegularExpression dateOnly(
        QStringLiteral(R"(^\d{4}-\d{2}-\d{2}$)"));
    // Date + time: YYYY-MM-DD[ T]HH:MM[:SS[.fff]][TZ]
    static const QRegularExpression dateTime(
        QStringLiteral(R"(^\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}(:\d{2}(\.\d{1,9})?)?(Z|[+-]\d{2}:?\d{2})?$)"));

    if (dateOnly.match(text).hasMatch()) {
        const QDate d = QDate::fromString(text, Qt::ISODate);
        if (!d.isValid()) return nullptr;
        return std::make_shared<DateValue>(QDateTime(d, QTime(0, 0)), false);
    }
    if (dateTime.match(text).hasMatch()) {
        // Normalise separator to 'T' for Qt::ISODateWithMs parsing tolerance.
        QString canonical = text;
        if (canonical.size() > 10 && canonical[10] == QLatin1Char(' '))
            canonical[10] = QLatin1Char('T');
        QDateTime dt = QDateTime::fromString(canonical, Qt::ISODateWithMs);
        if (!dt.isValid()) dt = QDateTime::fromString(canonical, Qt::ISODate);
        if (!dt.isValid()) return nullptr;
        return std::make_shared<DateValue>(dt, true);
    }
    return nullptr;
}

// ----- RelativeDateValue -----

QString RelativeDateValue::toString() const
{
    // Minimal humanize: "N days ago" / "in N days". Follow-up can swap to
    // Corbomite::MomentFormatter humanize helper once one exists.
    const QDateTime now = QDateTime::currentDateTime();
    qint64 secs = m_dt.secsTo(now);
    const bool past = secs >= 0;
    const qint64 abs = past ? secs : -secs;

    auto fmt = [&](qint64 v, const char *unit) {
        const QString base = past
            ? QStringLiteral("%1 %2 ago")
            : QStringLiteral("in %1 %2");
        QString u = QString::fromLatin1(unit);
        if (v != 1) u += QLatin1Char('s');
        return base.arg(v).arg(u);
    };

    if (abs < 60) return past ? QStringLiteral("moments ago") : QStringLiteral("in a moment");
    if (abs < 3600) return fmt(abs / 60, "minute");
    if (abs < 86400) return fmt(abs / 3600, "hour");
    if (abs < 86400 * 30) return fmt(abs / 86400, "day");
    if (abs < 86400 * 365) return fmt(abs / (86400 * 30), "month");
    return fmt(abs / (86400 * 365), "year");
}

// --- list date aggregates (Phase 1 Task 1.4 stubbed these; now real) ---

ValuePtr ListValue::earliest() const
{
    if (m_data.isEmpty()) return NullValue::instance();
    DateValue *best = nullptr;
    for (const auto &v : m_data) {
        auto *d = dynamic_cast<DateValue *>(v.get());
        if (!d) return NullValue::instance();
        if (!best || d->dateTime() < best->dateTime()) best = d;
    }
    return std::make_shared<DateValue>(best->dateTime(), best->hasTime());
}

ValuePtr ListValue::latest() const
{
    if (m_data.isEmpty()) return NullValue::instance();
    DateValue *best = nullptr;
    for (const auto &v : m_data) {
        auto *d = dynamic_cast<DateValue *>(v.get());
        if (!d) return NullValue::instance();
        if (!best || d->dateTime() > best->dateTime()) best = d;
    }
    return std::make_shared<DateValue>(best->dateTime(), best->hasTime());
}

}  // namespace Corbomite::Bases
