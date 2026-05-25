// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "BasesViewConfig.h"
#include "PropertyId.h"
#include <QVector>

namespace Corbomite::Bases {

/// Mutate `sort` for a header click on `clicked`. Plain click: if `clicked`
/// is the sole/primary key, cycle ASC->DESC->remove; else replace sort with
/// [clicked ASC]. Shift click: if present, cycle that key ASC->DESC->remove
/// in place; else append [clicked ASC] preserving existing keys.
void cycleHeaderSort(QVector<SortKey> &sort, const PropertyId &clicked, bool shiftHeld);

}  // namespace Corbomite::Bases
