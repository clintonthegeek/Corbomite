// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/PropertyTypeInference.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace Corbomite {

namespace {

// Strict ISO date: YYYY-MM-DD (exactly 10 chars, no time suffix).
bool isStrictIsoDate(const QString &s)
{
    if (s.size() != 10) return false;
    if (s.at(4) != QLatin1Char('-') || s.at(7) != QLatin1Char('-')) return false;
    const QDate d = QDate::fromString(s, Qt::ISODate);
    return d.isValid();
}

// Strict ISO datetime: must contain a 'T' (or space) separator AND parse
// as ISODate/ISODateWithMs. Rejects bare dates (no time component).
bool isStrictIsoDateTime(const QString &s)
{
    if (s.size() < 16) return false;
    // Must have a time separator. ISO 8601 allows 'T'; some producers
    // emit a space (YAML convention). Be strict: require 'T' to avoid
    // greedy matching on multi-line text.
    if (!s.contains(QLatin1Char('T'))) return false;
    QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    }
    return dt.isValid();
}

}  // namespace

PropertyType inferPropertyType(const Markoff::YamlValue &value)
{
    using Kind = Markoff::YamlValue::Kind;
    switch (value.kind()) {
    case Kind::Bool:
        return PropertyType::Checkbox;
    case Kind::Int:
    case Kind::Double:
        return PropertyType::Number;
    case Kind::Seq:
        return PropertyType::List;
    case Kind::String: {
        const QString s = value.asString();
        if (isStrictIsoDateTime(s)) return PropertyType::DateTime;
        if (isStrictIsoDate(s)) return PropertyType::Date;
        return PropertyType::Text;
    }
    case Kind::Map:
    case Kind::Null:
    default:
        return PropertyType::Text;
    }
}

Markoff::YamlValue qJsonValueToYaml(const QJsonValue &v)
{
    auto root = Markoff::YamlValue::emptyMap();
    // We can't return a free-standing scalar because YamlValue has no
    // public scalar-factory. Strategy: wrap under a single key "_" and
    // return the child via get("_"). This keeps all downstream code
    // uniform.
    constexpr auto kKey = "_";
    const QString key = QString::fromLatin1(kKey);

    switch (v.type()) {
    case QJsonValue::Bool:
        root.setBool(key, v.toBool());
        break;
    case QJsonValue::Double: {
        const double d = v.toDouble();
        // Preserve int semantics when the number has no fractional part
        // and fits in int64 range.
        const double truncd = static_cast<double>(static_cast<int64_t>(d));
        if (d == truncd && d >= -9.2233720368547758e18 && d <= 9.2233720368547758e18) {
            root.setInt(key, static_cast<int64_t>(d));
        } else {
            root.setDouble(key, d);
        }
        break;
    }
    case QJsonValue::String:
        root.setString(key, v.toString());
        break;
    case QJsonValue::Array: {
        auto seq = root.setSeqNode(key);
        const QJsonArray arr = v.toArray();
        for (const auto &item : arr) {
            // Flatten non-string scalars to strings on round-trip.
            // (Phase 1 limitation for List-type editor.)
            if (item.isString()) {
                seq.appendString(item.toString());
            } else if (item.isBool()) {
                seq.appendString(item.toBool() ? QStringLiteral("true")
                                               : QStringLiteral("false"));
            } else if (item.isDouble()) {
                seq.appendString(QString::number(item.toDouble()));
            } else if (item.isNull()) {
                seq.appendString(QString());
            } else {
                // Nested object/array → stringify via json
                seq.appendString(QString());
            }
        }
        break;
    }
    case QJsonValue::Object: {
        // Map — recursively set each key.
        auto child = root.setMap(key);
        const QJsonObject obj = v.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (it.value().isBool()) {
                child.setBool(it.key(), it.value().toBool());
            } else if (it.value().isDouble()) {
                const double d = it.value().toDouble();
                const double truncd = static_cast<double>(static_cast<int64_t>(d));
                if (d == truncd) child.setInt(it.key(), static_cast<int64_t>(d));
                else child.setDouble(it.key(), d);
            } else if (it.value().isString()) {
                child.setString(it.key(), it.value().toString());
            } else {
                child.setNull(it.key());
            }
        }
        break;
    }
    case QJsonValue::Null:
    case QJsonValue::Undefined:
    default:
        root.setNull(key);
        break;
    }

    return root.get(key);
}

}  // namespace Corbomite
