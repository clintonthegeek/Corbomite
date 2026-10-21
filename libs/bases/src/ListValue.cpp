// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"

#include <QHash>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <utility>
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
    const QString needle = v ? v->toString() : QString{};
    for (const auto &x : m_data) {
        if (auto *tag = dynamic_cast<TagValue *>(x.get())) {
            if (tag->tagMatches(needle)) return true;
        } else if (Value::staticLooseEquals(x.get(), v.get())) {
            return true;
        }
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
    // Bucketed by `toString()`, but within a bucket only a strict `equals`
    // drops the newcomer — so two values that stringify alike but are not
    // `equals` both survive (app/own/1647-tK.js:287-303). Dedup is strict,
    // not loose.
    //
    // The buckets are then concatenated in JS *object* property order, not
    // first-seen order: integer-like keys ascending first, everything else in
    // insertion order. That is why `[10, 2, 1].unique()` comes back
    // `[1, 2, 10]`.
    QHash<QString, QVector<ValuePtr>> buckets;
    QStringList insertionOrder;
    for (const auto &v : m_data) {
        const QString key = v ? v->toString() : QString{};
        if (!buckets.contains(key)) insertionOrder.append(key);
        auto &bucket = buckets[key];
        bool strictEqual = false;
        for (const auto &seen : bucket) {
            if (Value::staticEquals(v.get(), seen.get())) { strictEqual = true; break; }
        }
        if (!strictEqual) bucket.append(v);
    }
    QStringList indexKeys;
    QStringList stringKeys;
    for (const QString &k : std::as_const(insertionOrder)) {
        bool numeric = false;
        const qulonglong asIndex = k.toULongLong(&numeric);
        if (numeric && k == QString::number(asIndex) && asIndex < 0xFFFFFFFFULL)
            indexKeys.append(k);
        else
            stringKeys.append(k);
    }
    std::sort(indexKeys.begin(), indexKeys.end(), [](const QString &a, const QString &b) {
        return a.toULongLong() < b.toULongLong();
    });
    QVector<ValuePtr> out;
    out.reserve(m_data.size());
    for (const QString &k : std::as_const(indexKeys)) out.append(buckets.value(k));
    for (const QString &k : std::as_const(stringKeys)) out.append(buckets.value(k));
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

// `getNumbers()`: the non-numeric elements are *filtered out*, not fatal — so
// [1, 2, "x"].sum() is 3 (app/own/1647-tK.js:234-239).
static std::vector<double> collectNumbers(const QVector<ValuePtr> &xs)
{
    std::vector<double> out;
    out.reserve(xs.size());
    for (const auto &v : xs) {
        if (auto *n = dynamic_cast<NumberValue *>(v.get())) out.push_back(n->data());
    }
    return out;
}

ValuePtr ListValue::min() const
{
    std::vector<double> xs = collectNumbers(m_data);
    if (xs.empty()) return NullValue::instance();
    return std::make_shared<NumberValue>(*std::min_element(xs.begin(), xs.end()));
}

ValuePtr ListValue::max() const
{
    std::vector<double> xs = collectNumbers(m_data);
    if (xs.empty()) return NullValue::instance();
    return std::make_shared<NumberValue>(*std::max_element(xs.begin(), xs.end()));
}

ValuePtr ListValue::sum() const
{
    std::vector<double> xs = collectNumbers(m_data);
    if (xs.empty()) return NullValue::instance();
    double s = 0.0;
    for (double x : xs) s += x;
    return std::make_shared<NumberValue>(s);
}

ValuePtr ListValue::mean() const
{
    // `sum().data / this.length()` — the divisor is the length of the WHOLE
    // list, not of the numbers in it, so [1, 2, "x"].mean() is 1, not 1.5
    // (app/own/1647-tK.js:343-345).
    auto total = std::dynamic_pointer_cast<NumberValue>(sum());
    if (!total) return NullValue::instance();
    return std::make_shared<NumberValue>(total->data() / static_cast<double>(m_data.size()));
}

ValuePtr ListValue::median() const
{
    std::vector<double> xs = collectNumbers(m_data);
    if (xs.empty()) return NullValue::instance();
    std::sort(xs.begin(), xs.end());
    const auto n = xs.size();
    const double m = (n % 2)
        ? xs[n / 2]
        : 0.5 * (xs[n / 2 - 1] + xs[n / 2]);
    return std::make_shared<NumberValue>(m);
}

ValuePtr ListValue::stddev() const
{
    // Population standard deviation over the numbers only (divide by n, not
    // n-1); confirmed in source at app/own/1647-tK.js:352-365, which settles
    // the sample-vs-population question 2026-04-17 §14 left open.
    std::vector<double> xs = collectNumbers(m_data);
    if (xs.empty()) return NullValue::instance();
    auto meanOf = [](const std::vector<double> &v) {
        double s = 0.0;
        for (double x : v) s += x;
        return s / static_cast<double>(v.size());
    };
    const double mu = meanOf(xs);
    std::vector<double> devSq;
    devSq.reserve(xs.size());
    for (double x : xs) devSq.push_back((x - mu) * (x - mu));
    return std::make_shared<NumberValue>(std::sqrt(meanOf(devSq)));
}

// earliest/latest implementations live in DateValue.cpp (they depend on
// the full DateValue class definition for dynamic_cast).

namespace {

/// What `value == true` evaluates to. `Value::staticLooseEquals` is exactly
/// what the `==` operator resolves to (`Evaluator::applyEq` calls it), so a
/// native count and Obsidian's equivalent formula
/// (`values.filter(value == true).length`) cannot drift apart.
bool equalsBoolean(const ValuePtr &v, bool b)
{
    const BooleanValue probe(b);
    return Value::staticLooseEquals(v.get(), &probe);
}

/// How many entries are null. Obsidian's `value.isType("null")`.
int countNull(const QVector<ValuePtr> &xs)
{
    int n = 0;
    for (const auto &v : xs)
        if (!v || dynamic_cast<NullValue *>(v.get())) ++n;
    return n;
}

} // namespace

ValuePtr ListValue::range() const
{
    // Obsidian offers Range as a default for a NumberValue column
    // (`values.max() - values.min()`) and for a DateValue column
    // (`(values.latest() - values.earliest())`), and it sniffs a column's
    // class from its first non-Null entry (addendum 2026-10-03). Same rule
    // here, so one `Range` serves both column classes.
    const Value *sample = nullptr;
    for (const auto &v : m_data) {
        if (!v || dynamic_cast<NullValue *>(v.get())) continue;
        sample = v.get();
        break;
    }
    if (!sample) return NullValue::instance();

    if (dynamic_cast<const DateValue *>(sample)) {
        const auto latestV = latest();
        const auto earliestV = earliest();
        const auto *ld = dynamic_cast<const DateValue *>(latestV.get());
        const auto *ed = dynamic_cast<const DateValue *>(earliestV.get());
        if (!ld || !ed) return NullValue::instance();
        return DurationValue::fromMilliseconds(
            ld->dateTime().toMSecsSinceEpoch() - ed->dateTime().toMSecsSinceEpoch());
    }

    const auto maxV = max();
    const auto minV = min();
    const auto *m = dynamic_cast<const NumberValue *>(maxV.get());
    const auto *n = dynamic_cast<const NumberValue *>(minV.get());
    if (!m || !n) return NullValue::instance();
    return std::make_shared<NumberValue>(m->data() - n->data());
}

ValuePtr ListValue::checked() const
{
    int n = 0;
    for (const auto &v : m_data)
        if (equalsBoolean(v, true)) ++n;
    return std::make_shared<NumberValue>(n);
}

ValuePtr ListValue::unchecked() const
{
    int n = 0;
    for (const auto &v : m_data)
        if (equalsBoolean(v, false)) ++n;
    return std::make_shared<NumberValue>(n);
}

ValuePtr ListValue::empty() const
{
    return std::make_shared<NumberValue>(countNull(m_data));
}

ValuePtr ListValue::filled() const
{
    return std::make_shared<NumberValue>(
        static_cast<double>(m_data.size()) - countNull(m_data));
}

} // namespace Corbomite::Bases
