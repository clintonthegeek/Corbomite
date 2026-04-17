// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"

#include <QJsonArray>
#include <QJsonValue>

namespace Corbomite::Bases {

namespace {

// Coerce a raw JSON leaf into a typed Value following audit §1
// ObjectValue.fromFrontMatter rules + DSL-addendum §6.1 parseFromString
// probes. Order matters: Link (strict `[[...]]`) before URL before Date.
ValuePtr coerceLeaf(const QJsonValue &v);

ValuePtr coerceString(const QString &s)
{
    // 1. Wikilink?  [[...]]
    if (s.startsWith(QLatin1String("[[")) && s.endsWith(QLatin1String("]]"))) {
        if (auto link = LinkValue::parseFromString(s))
            return link;
    }
    // 2. URL?  http:// https:// file://
    if (s.startsWith(QLatin1String("http://")) || s.startsWith(QLatin1String("https://"))
        || s.startsWith(QLatin1String("file://"))) {
        return std::make_shared<UrlValue>(s);
    }
    // 3. Date?
    if (auto d = DateValue::parseFromString(s)) return d;
    // 4. Fallback: plain string.
    return std::make_shared<StringValue>(s);
}

ValuePtr coerceArray(const QJsonArray &arr)
{
    QVector<ValuePtr> items;
    items.reserve(arr.size());
    for (const auto &e : arr) items.push_back(coerceLeaf(e));
    return std::make_shared<ListValue>(items);
}

ValuePtr coerceLeaf(const QJsonValue &v)
{
    if (v.isNull()) return NullValue::instance();
    if (v.isBool()) return std::make_shared<BooleanValue>(v.toBool());
    if (v.isDouble()) return std::make_shared<NumberValue>(v.toDouble());
    if (v.isString()) return coerceString(v.toString());
    if (v.isArray()) return coerceArray(v.toArray());
    if (v.isObject()) return ObjectValue::fromFrontMatter(v.toObject());
    return NullValue::instance();
}

}  // namespace

std::shared_ptr<ObjectValue> ObjectValue::fromFrontMatter(const QJsonObject &fm)
{
    auto obj = std::make_shared<ObjectValue>();
    for (auto it = fm.begin(); it != fm.end(); ++it) {
        const QString key = it.key();
        // Special-case `tags` per audit: array-of-strings → ListValue<TagValue>.
        if (key.compare(QLatin1String("tags"), Qt::CaseInsensitive) == 0
            && it.value().isArray()) {
            QVector<ValuePtr> tags;
            for (const auto &e : it.value().toArray()) {
                if (e.isString())
                    tags.push_back(std::make_shared<TagValue>(e.toString()));
            }
            obj->set(key, std::make_shared<ListValue>(tags));
        } else {
            obj->set(key, coerceLeaf(it.value()));
        }
    }
    return obj;
}

void ObjectValue::set(const QString &key, ValuePtr value)
{
    if (!m_data.contains(key)) m_order.append(key);
    m_data.insert(key, std::move(value));
}

ValuePtr ObjectValue::get(const QString &key) const
{
    return m_data.value(key);
}

ValuePtr ObjectValue::getInsensitive(const QString &key) const
{
    // Exact match first (fast path).
    if (auto it = m_data.constFind(key); it != m_data.constEnd())
        return *it;
    const QString lower = key.toLower();
    for (auto it = m_data.constBegin(); it != m_data.constEnd(); ++it) {
        if (it.key().toLower() == lower) return it.value();
    }
    return nullptr;
}

bool ObjectValue::equals(const Value &other) const
{
    auto *rhs = dynamic_cast<const ObjectValue *>(&other);
    if (!rhs) return false;
    if (m_order.size() != rhs->m_order.size()) return false;
    for (const auto &k : m_order) {
        if (!Value::staticEquals(m_data.value(k), rhs->m_data.value(k)))
            return false;
    }
    return true;
}

ValuePtr ObjectValue::objectAccess(const QString &key) const
{
    return getInsensitive(key);
}

QVector<ValuePtr> ObjectValue::values() const
{
    QVector<ValuePtr> out;
    out.reserve(m_order.size());
    for (const auto &k : m_order) out.append(m_data.value(k));
    return out;
}

QVector<std::pair<QString, ValuePtr>> ObjectValue::entries() const
{
    QVector<std::pair<QString, ValuePtr>> out;
    out.reserve(m_order.size());
    for (const auto &k : m_order)
        out.append({k, m_data.value(k)});
    return out;
}

// ----- LambdaObjectValue -----

ValuePtr LambdaObjectValue::objectAccess(const QString &key) const
{
    return m_r ? m_r(key) : nullptr;
}

}  // namespace Corbomite::Bases
