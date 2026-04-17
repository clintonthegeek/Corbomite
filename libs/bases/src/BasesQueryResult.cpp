// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesQueryResult.h"

#include "corbomite/bases/FunctionRegistry.h"
#include "corbomite/bases/Values.h"

#include <algorithm>

namespace Corbomite::Bases {

namespace {

int compareValues(const ValuePtr &a, const ValuePtr &b)
{
    // Null last (addendum §8 / audit §8 invariant).
    const bool an = !a || std::dynamic_pointer_cast<NullValue>(a) != nullptr;
    const bool bn = !b || std::dynamic_pointer_cast<NullValue>(b) != nullptr;
    if (an && bn) return 0;
    if (an) return +1;
    if (bn) return -1;

    // Number vs Number.
    auto *an1 = dynamic_cast<NumberValue *>(a.get());
    auto *bn1 = dynamic_cast<NumberValue *>(b.get());
    if (an1 && bn1) {
        const double x = an1->data(), y = bn1->data();
        return (x < y) ? -1 : (x > y ? 1 : 0);
    }

    // Date vs Date.
    auto *ad = dynamic_cast<DateValue *>(a.get());
    auto *bd = dynamic_cast<DateValue *>(b.get());
    if (ad && bd) {
        const qint64 x = ad->dateTime().toMSecsSinceEpoch();
        const qint64 y = bd->dateTime().toMSecsSinceEpoch();
        return (x < y) ? -1 : (x > y ? 1 : 0);
    }

    // Duration vs Duration.
    auto *adu = dynamic_cast<DurationValue *>(a.get());
    auto *bdu = dynamic_cast<DurationValue *>(b.get());
    if (adu && bdu) {
        const qint64 x = adu->totalMilliseconds();
        const qint64 y = bdu->totalMilliseconds();
        return (x < y) ? -1 : (x > y ? 1 : 0);
    }

    // Boolean: false < true.
    auto *ab = dynamic_cast<BooleanValue *>(a.get());
    auto *bb = dynamic_cast<BooleanValue *>(b.get());
    if (ab && bb) {
        const bool x = ab->data(), y = bb->data();
        return (x == y) ? 0 : (x ? 1 : -1);
    }

    // Fallback: locale-aware string compare.
    return QString::localeAwareCompare(a->toString(), b->toString());
}

int signFor(const QString &dir) { return dir.toUpper() == QLatin1String("DESC") ? -1 : 1; }

}  // namespace

BasesQueryResult::BasesQueryResult(const BasesViewConfig &cfg,
                                   QVector<std::shared_ptr<BasesEntry>> entries,
                                   FunctionRegistry *funcs)
    : m_cfg(cfg), m_rows(std::move(entries)), m_funcs(funcs)
{
    applySort();
    applyLimit();
}

void BasesQueryResult::applySort()
{
    if (m_cfg.sort.isEmpty()) return;
    std::sort(m_rows.begin(), m_rows.end(),
        [&](const std::shared_ptr<BasesEntry> &A,
            const std::shared_ptr<BasesEntry> &B) {
            for (const auto &sk : m_cfg.sort) {
                const auto a = A->getValue(sk.property);
                const auto b = B->getValue(sk.property);
                const int c = compareValues(a, b) * signFor(sk.direction);
                if (c != 0) return c < 0;
            }
            return false;
        });
}

void BasesQueryResult::applyLimit()
{
    if (m_cfg.limit > 0 && m_rows.size() > m_cfg.limit) {
        m_rows.resize(m_cfg.limit);
    }
}

const QVector<BasesEntryGroup> &BasesQueryResult::groups() const
{
    if (m_groups) return *m_groups;
    QVector<BasesEntryGroup> out;
    if (!m_cfg.groupBy) {
        BasesEntryGroup g;
        g.entries = m_rows;
        out.push_back(g);
        m_groups = out;
        return *m_groups;
    }
    // Partition entries by the group key's loose-equality.
    for (const auto &row : m_rows) {
        auto key = row->getValue(m_cfg.groupBy->property);
        bool matched = false;
        for (auto &g : out) {
            if (Value::staticLooseEquals(g.key, key)) {
                g.entries.push_back(row);
                matched = true;
                break;
            }
        }
        if (!matched) {
            BasesEntryGroup ng;
            ng.key = key;
            ng.entries.push_back(row);
            out.push_back(ng);
        }
    }
    // Null-keyed groups to the end (audit §8 invariant).
    std::stable_sort(out.begin(), out.end(), [](const BasesEntryGroup &a, const BasesEntryGroup &b) {
        if (!a.hasKey() && b.hasKey()) return false;
        if (a.hasKey() && !b.hasKey()) return true;
        if (!a.hasKey() && !b.hasKey()) return false;
        return compareValues(a.key, b.key) < 0;
    });
    m_groups = out;
    return *m_groups;
}

const QVector<PropertyId> &BasesQueryResult::properties() const
{
    if (m_props) return *m_props;
    QVector<PropertyId> out;
    QSet<PropertyId> seen;
    for (const auto &p : m_cfg.order) {
        if (!seen.contains(p)) { seen.insert(p); out.append(p); }
    }
    // Union with frontmatter keys actually present on rows.
    for (const auto &row : m_rows) {
        for (const auto &k : row->getPropertyKeys()) {
            PropertyId id{PropertyKind::Note, k};
            if (!seen.contains(id)) { seen.insert(id); out.append(id); }
        }
    }
    m_props = out;
    return *m_props;
}

ValuePtr BasesQueryResult::summaryValue(int groupIndex,
                                        const PropertyId &prop,
                                        const QString &summaryFn) const
{
    Q_UNUSED(summaryFn);  // default summaries mapped by name come in Phase 7 Task 7.3 follow-up
    const auto &gs = groups();
    if (groupIndex < 0 || groupIndex >= gs.size()) return NullValue::instance();
    QVector<ValuePtr> vals;
    for (const auto &e : gs[groupIndex].entries) vals.push_back(e->getValue(prop));
    ListValue list(vals);

    // Minimal dispatch covering the addendum §9 default names most useful
    // for MVP tabular summaries. Formula-string-based summaries are a
    // follow-up — this path hard-codes the common cases for speed.
    const QString key = summaryFn.toLower();
    if (key == QLatin1String("sum"))     return list.sum();
    if (key == QLatin1String("min"))     return list.min();
    if (key == QLatin1String("max"))     return list.max();
    if (key == QLatin1String("mean")
     || key == QLatin1String("average")) return list.mean();
    if (key == QLatin1String("median"))  return list.median();
    if (key == QLatin1String("stddev"))  return list.stddev();
    if (key == QLatin1String("unique"))  return std::make_shared<NumberValue>(list.unique()->length());
    if (key == QLatin1String("count"))   return std::make_shared<NumberValue>(list.length());
    return NullValue::instance();
}

}  // namespace Corbomite::Bases
