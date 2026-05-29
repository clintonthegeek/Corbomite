// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterSpec.h"

namespace Corbomite::Bases {

namespace {

// Recursive backend->spec, preserving leaf-vs-group kind (no top-level wrapping).
FilterSpec specOf(const FilterPtr &node)
{
    if (auto rule = std::dynamic_pointer_cast<FilterRule>(node))
        return FilterSpec::leaf(rule->rule().source());
    if (auto conj = std::dynamic_pointer_cast<FilterConjunction>(node)) {
        QVector<FilterSpec> kids;
        for (const auto &c : conj->children()) kids.push_back(specOf(c));
        return FilterSpec::group(conj->conj(), std::move(kids));
    }
    return FilterSpec::group(Conj::And);  // null/unknown -> empty group
}

}  // namespace

FilterSpec fromFilter(const FilterPtr &node)
{
    if (!node) return FilterSpec::group(Conj::And);
    FilterSpec s = specOf(node);
    if (s.kind == FilterSpec::Kind::Leaf)         // bare top-level rule
        return FilterSpec::group(Conj::And, { s });
    return s;
}

FilterPtr toFilter(const FilterSpec &spec)
{
    if (spec.kind == FilterSpec::Kind::Leaf) {
        if (spec.expression.trimmed().isEmpty()) return nullptr;  // drop blanks
        return std::make_shared<FilterRule>(Formula(spec.expression));
    }
    QVector<FilterPtr> kids;
    for (const auto &c : spec.children) {
        if (FilterPtr p = toFilter(c)) kids.push_back(p);
    }
    if (kids.isEmpty()) return nullptr;
    return FilterConjunction(spec.conj, kids).optimize();
}

}  // namespace Corbomite::Bases
