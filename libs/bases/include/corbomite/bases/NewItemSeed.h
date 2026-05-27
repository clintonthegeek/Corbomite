// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FilterTree.h"

#include <QPair>
#include <QString>
#include <QVector>

namespace Corbomite::Bases {

/// Computes the frontmatter to seed into a new note so it satisfies a view's
/// filter. Pure: no vault, no widgets. Walks the filter tree collecting
/// top-level AND-context equality constraints (`prop == literal`); OR /
/// negation / non-equality subtrees and `file.*` properties contribute
/// nothing. Equality-derived values override colliding template values.
namespace NewItemSeed {

using SeedList = QVector<QPair<QString, QString>>;  ///< ordered: template keys first, then equality keys.

/// `templateProps` is the (already resolved) frontmatter from the view's
/// newItemTemplate, in source order; empty if no template. `filter` may be
/// null (no filter → template verbatim).
SeedList compute(const FilterPtr &filter, const SeedList &templateProps);

}  // namespace NewItemSeed
}  // namespace Corbomite::Bases
