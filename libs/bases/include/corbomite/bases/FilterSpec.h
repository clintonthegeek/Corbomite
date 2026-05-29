// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FilterTree.h"   // FilterPtr, Conj

#include <QString>
#include <QVector>

namespace Corbomite::Bases {

/// Mutable, copyable mirror of a filter tree used by the filter-builder UI.
/// A node is either a Leaf (a raw predicate string) or a Group (And/Or/Not over
/// ordered children). Converted to/from the immutable FilterNode tree by the
/// pure functions below.
struct FilterSpec
{
    enum class Kind { Leaf, Group };
    Kind kind = Kind::Group;
    QString expression;            ///< Leaf only: the predicate source.
    Conj conj = Conj::And;         ///< Group only.
    QVector<FilterSpec> children;  ///< Group only, ordered.

    static FilterSpec leaf(const QString &expr)
    {
        FilterSpec s; s.kind = Kind::Leaf; s.expression = expr; return s;
    }
    static FilterSpec group(Conj c, QVector<FilterSpec> kids = {})
    {
        FilterSpec s; s.kind = Kind::Group; s.conj = c; s.children = std::move(kids); return s;
    }

    bool operator==(const FilterSpec &o) const
    {
        return kind == o.kind && expression == o.expression
            && conj == o.conj && children == o.children;
    }
};

/// Backend tree -> editable spec. The result is always a Group (the dialog
/// always shows a top-level group): nullptr -> empty And-group; a bare
/// FilterRule -> And-group wrapping one leaf; a FilterConjunction -> a group
/// with converted children.
FilterSpec fromFilter(const FilterPtr &node);

/// Editable spec -> backend tree. Leaf with blank (whitespace-only) text -> null
/// (dropped). Group: convert children, drop nulls; empty -> null; otherwise build
/// a FilterConjunction and return optimize() (single-child And/Or collapses to the
/// bare child; Not and multi-child preserved).
FilterPtr toFilter(const FilterSpec &spec);

}  // namespace Corbomite::Bases
