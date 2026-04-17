// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Formula.h"

#include <QVariant>
#include <QVector>

#include <memory>

namespace Markoff {
class YamlValue;
}

namespace Corbomite::Bases {

class FilterNode;
using FilterPtr = std::shared_ptr<FilterNode>;

class FunctionRegistry;

/// Abstract filter-tree node.
class FilterNode
{
public:
    virtual ~FilterNode() = default;
    virtual bool test(const EvalContext &ctx, FunctionRegistry *funcs = nullptr) const = 0;
    /// Render back to YAML shape (string for leaf, map for conjunction).
    /// Returned QVariant is a QString or QVariantMap.
    virtual QVariant serialize() const = 0;
};

/// Leaf filter — single formula string evaluated as a predicate.
class FilterRule : public FilterNode
{
public:
    explicit FilterRule(Formula rule) : m_rule(std::move(rule)) {}

    const Formula &rule() const { return m_rule; }

    bool test(const EvalContext &ctx, FunctionRegistry *funcs = nullptr) const override;
    QVariant serialize() const override { return m_rule.source(); }

private:
    Formula m_rule;
};

enum class Conj { And, Or, Not };

class FilterConjunction : public FilterNode
{
public:
    FilterConjunction(Conj c, QVector<FilterPtr> children)
        : m_conj(c), m_children(std::move(children)) {}

    Conj conj() const { return m_conj; }
    const QVector<FilterPtr> &children() const { return m_children; }

    bool test(const EvalContext &ctx, FunctionRegistry *funcs = nullptr) const override;
    QVariant serialize() const override;

    /// Simplify: collapse single-child conjunctions into their child.
    FilterPtr optimize() const;

private:
    Conj m_conj;
    QVector<FilterPtr> m_children;
};

/// Parse a filter tree from a Markoff::YamlValue node.
///   - scalar string → FilterRule (formula parsed).
///   - map with `and`/`or`/`not` key → FilterConjunction.
///   - anything else → nullptr.
FilterPtr parseFilter(const Markoff::YamlValue &node);

}  // namespace Corbomite::Bases
