// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FilterTree.h"
#include "PropertyId.h"

#include <QHash>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <memory>
#include <optional>

namespace Markoff {
class YamlValue;
}

namespace Corbomite::Bases {

struct SortKey
{
    PropertyId property;
    QString direction;  ///< "ASC" or "DESC".
};

struct GroupBy
{
    PropertyId property;
    QString direction;
};

/// Per-property settings (optional).
struct PropertyConfig
{
    QString displayName;
    QVariantMap unrecognizedData;
};

/// One named view within a `.base` file.
class BasesViewConfig
{
public:
    QString type;  ///< "table" or plugin-registered view-type name.
    QString name;  ///< unique within `BasesQuery::views`.

    FilterPtr filters;                     ///< optional per-view filters (AND-merged with global).
    QVector<PropertyId> order;             ///< visible-column order.
    QVector<SortKey> sort;                 ///< multi-key sort.
    std::optional<GroupBy> groupBy;
    int limit = 0;                         ///< 0 == unlimited (audit invariant).
    QHash<PropertyId, QString> summaries;  ///< propId → summary-fn-name.
    QVariantMap data;                      ///< free-form view-type-specific options.
    QVariantMap unrecognizedData;          ///< forward-compat round-trip.

    /// Parse one YAML map node into a view config.
    static std::unique_ptr<BasesViewConfig> fromYaml(const Markoff::YamlValue &node,
                                                     QString *errorOut);

    /// Emit as a YAML-ready QVariantMap (key order canonical then
    /// unrecognizedData appended).
    QVariantMap toMap() const;
};

}  // namespace Corbomite::Bases
