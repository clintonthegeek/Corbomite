// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesViewConfig.h"   // SortKey, GroupBy, BasesViewConfig
#include "PropertyId.h"

#include <QString>
#include <QVector>

#include <optional>

namespace Corbomite::Bases {

class BasesQuery;

// --- column ops (visibility == membership in `order`) ---

/// Remove `pid` from `order` when `visible` is false; insert it at the
/// canonical position (matching its index in `allProps`) when true.
/// No-op if the requested state already holds.
void setColumnVisible(QVector<PropertyId> &order, const PropertyId &pid, bool visible,
                      const QVector<PropertyId> &allProps);

/// Move the column at `from` to `to` (both 0-based). Clamped to valid range.
void moveColumn(QVector<PropertyId> &order, int from, int to);

/// Clear `order`, hiding all columns.
void hideAllColumns(QVector<PropertyId> &order);

// --- sort ops (complements SortCycle::cycleHeaderSort) ---

/// Append `{pid, dir}` to `sort` if `pid` is not already present; no-op otherwise.
void addSortKey(QVector<SortKey> &sort, const PropertyId &pid, const QString &dir);

/// Set the direction of the existing key for `pid` to `dir`; insert if absent.
void setSortDirection(QVector<SortKey> &sort, const PropertyId &pid, const QString &dir);

/// Remove the sort key for `pid` if present; no-op otherwise.
void removeSortKey(QVector<SortKey> &sort, const PropertyId &pid);

// --- group op ---

/// Set `cfg.groupBy` to `{*pid, dir}` when `pid` has a value; clear it otherwise.
void setGroupBy(BasesViewConfig &cfg, const std::optional<PropertyId> &pid, const QString &dir);

// --- view CRUD (operate on BasesQuery::views) ---

/// Deep-copy the view named `name` and append it as `newName`.
/// Returns false (no-op) if `newName` already exists or `name` is not found.
bool duplicateView(BasesQuery &q, const QString &name, const QString &newName);

/// Remove the view named `name`. Returns false (no-op) if it is the last view.
bool deleteView(BasesQuery &q, const QString &name);

/// Rename the view `oldName` to `newName`.
/// Returns false (no-op) if `newName` already exists or `oldName` is not found.
bool renameView(BasesQuery &q, const QString &oldName, const QString &newName);

/// Move the view named `name` to position 0 (making it the default).
/// Returns false if `name` is not found.
bool setDefaultView(BasesQuery &q, const QString &name);

}  // namespace Corbomite::Bases
