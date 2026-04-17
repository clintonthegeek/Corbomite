// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Corbomite::Bases {

QString ListValue::toString() const
{
    QStringList parts;
    parts.reserve(m_data.size());
    for (const auto &v : m_data)
        parts.append(v ? v->toString() : QString{});
    return QStringLiteral("[") + parts.join(QStringLiteral(", ")) + QStringLiteral("]");
}

bool ListValue::equals(const Value &other) const
{
    auto *rhs = dynamic_cast<const ListValue *>(&other);
    if (!rhs) return false;
    if (m_data.size() != rhs->m_data.size()) return false;
    for (int i = 0; i < m_data.size(); ++i) {
        if (!Value::staticEquals(m_data[i].get(), rhs->m_data[i].get()))
            return false;
    }
    return true;
}

ValuePtr ListValue::objectAccess(const QString &key) const
{
    if (key == QLatin1String("length"))
        return std::make_shared<NumberValue>(static_cast<double>(m_data.size()));
    return nullptr;
}

QStringList ListValue::keys() const
{
    return {QStringLiteral("length")};
}

ValuePtr ListValue::get(int i) const
{
    if (i < 0 || i >= m_data.size()) return NullValue::instance();
    return m_data[i];
}

bool ListValue::includes(const ValuePtr &v) const
{
    for (const auto &x : m_data) {
        if (Value::staticLooseEquals(x.get(), v.get()))
            return true;
    }
    return false;
}

std::shared_ptr<ListValue> ListValue::concat(const ListValue &other) const
{
    QVector<ValuePtr> out = m_data;
    out.append(other.m_data);
    return std::make_shared<ListValue>(out);
}

std::shared_ptr<ListValue> ListValue::reverse() const
{
    QVector<ValuePtr> out = m_data;
    std::reverse(out.begin(), out.end());
    return std::make_shared<ListValue>(out);
}

std::shared_ptr<ListValue> ListValue::flatten() const
{
    // One-level spread: ListValue elements are spliced in; non-list elements kept.
    QVector<ValuePtr> out;
    out.reserve(m_data.size());
    for (const auto &v : m_data) {
        if (auto *inner = dynamic_cast<ListValue *>(v.get()))
            out.append(inner->m_data);
        else
            out.append(v);
    }
    return std::make_shared<ListValue>(out);
}

std::shared_ptr<ListValue> ListValue::unique() const
{
    QVector<ValuePtr> out;
    out.reserve(m_data.size());
    for (const auto &v : m_data) {
        bool duplicate = false;
        for (const auto &seen : out) {
            if (Value::staticLooseEquals(v.get(), seen.get())) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) out.append(v);
    }
    return std::make_shared<ListValue>(out);
}

std::shared_ptr<ListValue> ListValue::sort() const
{
    QVector<ValuePtr> out = m_data;
    std::sort(out.begin(), out.end(), [](const ValuePtr &a, const ValuePtr &b) {
        // Nulls last.
        if (!a || dynamic_cast<NullValue *>(a.get())) return false;
        if (!b || dynamic_cast<NullValue *>(b.get())) return true;
        // Numeric first if both are numbers.
        auto *an = dynamic_cast<NumberValue *>(a.get());
        auto *bn = dynamic_cast<NumberValue *>(b.get());
        if (an && bn) return an->data() < bn->data();
        // Else locale-aware string compare.
        return QString::localeAwareCompare(a->toString(), b->toString()) < 0;
    });
    return std::make_shared<ListValue>(out);
}

std::shared_ptr<ListValue> ListValue::slice(int start, int endExclusive) const
{
    const int n = m_data.size();
    const int s = std::clamp(start < 0 ? n + start : start, 0, n);
    const int e = std::clamp(endExclusive < 0 ? n : endExclusive, 0, n);
    if (e <= s) return std::make_shared<ListValue>();
    QVector<ValuePtr> out;
    out.reserve(e - s);
    for (int i = s; i < e; ++i) out.append(m_data[i]);
    return std::make_shared<ListValue>(out);
}

QString ListValue::join(const QString &sep) const
{
    QStringList parts;
    parts.reserve(m_data.size());
    for (const auto &v : m_data)
        parts.append(v ? v->toString() : QString{});
    return parts.join(sep);
}

// --- numeric aggregates ---

static bool collectNumbers(const QVector<ValuePtr> &xs, std::vector<double> &out)
{
    out.clear();
    out.reserve(xs.size());
    for (const auto &v : xs) {
        auto *n = dynamic_cast<NumberValue *>(v.get());
        if (!n) return false;
        out.push_back(n->data());
    }
    return true;
}

ValuePtr ListValue::min() const
{
    std::vector<double> xs;
    if (m_data.isEmpty() || !collectNumbers(m_data, xs))
        return NullValue::instance();
    return std::make_shared<NumberValue>(*std::min_element(xs.begin(), xs.end()));
}

ValuePtr ListValue::max() const
{
    std::vector<double> xs;
    if (m_data.isEmpty() || !collectNumbers(m_data, xs))
        return NullValue::instance();
    return std::make_shared<NumberValue>(*std::max_element(xs.begin(), xs.end()));
}

ValuePtr ListValue::sum() const
{
    std::vector<double> xs;
    if (!collectNumbers(m_data, xs))
        return NullValue::instance();
    double s = 0.0;
    for (double x : xs) s += x;
    return std::make_shared<NumberValue>(s);
}

ValuePtr ListValue::mean() const
{
    std::vector<double> xs;
    if (m_data.isEmpty() || !collectNumbers(m_data, xs))
        return NullValue::instance();
    double s = 0.0;
    for (double x : xs) s += x;
    return std::make_shared<NumberValue>(s / static_cast<double>(xs.size()));
}

ValuePtr ListValue::median() const
{
    std::vector<double> xs;
    if (m_data.isEmpty() || !collectNumbers(m_data, xs))
        return NullValue::instance();
    std::sort(xs.begin(), xs.end());
    const auto n = xs.size();
    const double m = (n % 2)
        ? xs[n / 2]
        : 0.5 * (xs[n / 2 - 1] + xs[n / 2]);
    return std::make_shared<NumberValue>(m);
}

ValuePtr ListValue::stddev() const
{
    std::vector<double> xs;
    if (m_data.isEmpty() || !collectNumbers(m_data, xs))
        return NullValue::instance();
    double s = 0.0;
    for (double x : xs) s += x;
    const double mu = s / static_cast<double>(xs.size());
    double sumSq = 0.0;
    for (double x : xs) {
        const double d = x - mu;
        sumSq += d * d;
    }
    // Population standard deviation (divide by n, not n-1). Addendum §14
    // flags this as unconfirmed between help-doc and impl — pick population
    // and document in the preserved-compat-quirks list.
    return std::make_shared<NumberValue>(std::sqrt(sumSq / static_cast<double>(xs.size())));
}

// earliest/latest implementations live in DateValue.cpp (they depend on
// the full DateValue class definition for dynamic_cast).

}  // namespace Corbomite::Bases
