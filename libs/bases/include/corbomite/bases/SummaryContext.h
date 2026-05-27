// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "EvalContext.h"

namespace Corbomite::Bases {

/// Binds the identifier `values` to a per-group ListValue for summary-formula
/// evaluation (addendum §9). Every other identifier delegates to an optional
/// parent context; with no parent, unknown identifiers resolve to nullptr.
class SummaryContext : public EvalContext
{
public:
    explicit SummaryContext(ValuePtr values, const EvalContext *parent = nullptr)
        : m_values(std::move(values)), m_parent(parent) {}

    ValuePtr getByIdentifier(const QString &name) const override
    {
        if (name.compare(QLatin1String("values"), Qt::CaseInsensitive) == 0)
            return m_values;
        return m_parent ? m_parent->getByIdentifier(name) : nullptr;
    }

    QStringList keys() const override
    {
        QStringList k = m_parent ? m_parent->keys() : QStringList{};
        k << QStringLiteral("values");
        return k;
    }

    const VaultResolver *vault() const override
    {
        return m_parent ? m_parent->vault() : nullptr;
    }

private:
    ValuePtr m_values;
    const EvalContext *m_parent;
};

}  // namespace Corbomite::Bases
