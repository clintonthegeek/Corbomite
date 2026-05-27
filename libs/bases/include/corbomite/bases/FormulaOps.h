// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Formula.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace Corbomite::Bases {

/// Pure mutators over a formula map + its insertion-order list (used for both
/// BasesQuery::formulas/formulaOrder and summaryFormulas/summaryFormulaOrder).
/// All return true on success, false on a precondition failure (no mutation).
///
/// INVARIANT: every key in `map` appears exactly once in `order`. These
/// functions assume callers maintain it; `rename` refuses to mutate (returns
/// false) rather than corrupt order if it finds the invariant broken.
namespace FormulaOps {

/// Add `name`->`source`. Fails if `name` is empty or already present.
bool add(QHash<QString, Formula> &map, QStringList &order,
         const QString &name, const QString &source);

/// Rename `oldName`->`newName`, preserving the formula + its order position.
/// Does NOT rewrite references. Fails if `oldName` is absent, `newName` is
/// empty, or `newName` collides with a different key.
bool rename(QHash<QString, Formula> &map, QStringList &order,
            const QString &oldName, const QString &newName);

/// Replace the source of an existing `name`. Fails if `name` is absent.
bool setSource(QHash<QString, Formula> &map,
               const QString &name, const QString &source);

/// Remove `name`. Fails if absent.
bool remove(QHash<QString, Formula> &map, QStringList &order,
            const QString &name);

}  // namespace FormulaOps
}  // namespace Corbomite::Bases
