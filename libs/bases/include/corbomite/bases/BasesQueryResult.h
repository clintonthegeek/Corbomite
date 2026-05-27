// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesEntry.h"
#include "BasesViewConfig.h"
#include "Formula.h"
#include "PropertyId.h"
#include "Values.h"

#include <QHash>
#include <QVector>

#include <memory>
#include <optional>

namespace Corbomite::Bases {

class FunctionRegistry;

struct BasesEntryGroup
{
    QVector<std::shared_ptr<BasesEntry>> entries;
    ValuePtr key;  ///< may be null.

    bool hasKey() const
    {
        return key && !std::dynamic_pointer_cast<NullValue>(key);
    }
};

/// Sorted + optionally grouped set of entries produced by a query's
/// current view config. Constructed anew on every recompute.
class BasesQueryResult
{
public:
    BasesQueryResult(const BasesViewConfig &cfg,
                     QVector<std::shared_ptr<BasesEntry>> entries,
                     FunctionRegistry *funcs = nullptr,
                     const QHash<QString, Formula> *summaryFormulas = nullptr);

    const QVector<std::shared_ptr<BasesEntry>> &rows() const { return m_rows; }

    /// Lazy — computed on first call, cached.
    const QVector<BasesEntryGroup> &groups() const;

    /// Union of configured order + note.* keys seen in rows.
    const QVector<PropertyId> &properties() const;

    /// Default or user-supplied summary by name. nullptr if unknown.
    ValuePtr summaryValue(int groupIndex,
                          const PropertyId &prop,
                          const QString &summaryFn) const;

private:
    void applySort();
    void applyLimit();

    const BasesViewConfig &m_cfg;
    QVector<std::shared_ptr<BasesEntry>> m_rows;
    FunctionRegistry *m_funcs;
    const QHash<QString, Formula> *m_summaryFormulas = nullptr;  // not owned

    mutable std::optional<QVector<BasesEntryGroup>> m_groups;
    mutable std::optional<QVector<PropertyId>> m_props;
};

}  // namespace Corbomite::Bases
