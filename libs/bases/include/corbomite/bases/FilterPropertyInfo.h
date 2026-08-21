// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesEntry.h"
#include "PropertyId.h"

#include <QString>
#include <QVector>

#include <functional>
#include <memory>

namespace Corbomite::Bases {

/// One property as offered to the filter builder's "simple" (point-and-click)
/// leaf editor: which property, what to label it, and its best-guess Value
/// type (drives which operators are offered — see FilterRuleRow).
struct FilterPropertyInfo
{
    PropertyId id;
    QString displayName;
    QString valueType;  ///< Value::type() string, e.g. "String"/"Number"/
                        ///< "Date"/"Boolean"/"List". Falls back to "String"
                        ///< when no sampled row has a non-null value.
};

/// Best-effort type inference: for each property, scans `sampleRows` (in
/// order) for the first non-null/non-empty value and records its runtime
/// `Value::type()`. There is no central type registry (unlike Obsidian's
/// metadataTypeManager) — this samples actual data instead. A property with
/// no non-null value anywhere in the sample defaults to "String" (the
/// widest operator set).
QVector<FilterPropertyInfo> buildFilterPropertyInfos(
    const QVector<PropertyId> &props,
    const QVector<std::shared_ptr<BasesEntry>> &sampleRows,
    const std::function<QString(const PropertyId &)> &displayNameFor);

}  // namespace Corbomite::Bases
