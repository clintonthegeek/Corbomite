// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterTree.h"

#include "markoff/parser/YamlValue.h"

#include <QVariantList>
#include <QVariantMap>

namespace Corbomite::Bases {

bool FilterRule::test(const EvalContext &ctx, FunctionRegistry *funcs) const
{
    return m_rule.test(ctx, funcs);
}

bool FilterConjunction::test(const EvalContext &ctx, FunctionRegistry *funcs) const
{
    switch (m_conj) {
    case Conj::And:
        for (const auto &c : m_children)
            if (!c || !c->test(ctx, funcs)) return false;
        return true;
    case Conj::Or:
        for (const auto &c : m_children)
            if (c && c->test(ctx, funcs)) return true;
        return false;
    case Conj::Not:
        // NOR: `not: [a, b]` is `!(a || b)`, i.e. false as soon as any child
        // passes (app/own/1764-jX.js:232-237). Not `!(a && b)` — the filter
        // builder labels the group "None of the following are true" and joins
        // its rows with "or" (app/own/1803-cZ.js:48-66,:136-137).
        for (const auto &c : m_children)
            if (c && c->test(ctx, funcs)) return false;
        return true;
    }
    return false;
}

QVariant FilterConjunction::serialize() const
{
    QVariantList xs;
    xs.reserve(m_children.size());
    for (const auto &c : m_children)
        if (c) xs.append(c->serialize());
    QVariantMap m;
    const char *key = "and";
    if (m_conj == Conj::Or)  key = "or";
    if (m_conj == Conj::Not) key = "not";
    m.insert(QLatin1String(key), xs);
    return m;
}

FilterPtr FilterConjunction::optimize() const
{
    if (m_children.size() == 1 && m_conj != Conj::Not)
        return m_children[0];
    return std::make_shared<FilterConjunction>(m_conj, m_children);
}

FilterPtr parseFilter(const Markoff::YamlValue &node)
{
    if (node.isString()) {
        return std::make_shared<FilterRule>(Formula(node.asString()));
    }
    if (node.isMap()) {
        auto readConj = [&](const char *key, Conj conj) -> FilterPtr {
            const QString k = QLatin1String(key);
            if (!node.contains(k)) return nullptr;
            const auto seq = node.get(k);
            if (!seq.isSeq()) return nullptr;
            QVector<FilterPtr> children;
            for (int i = 0, n = seq.size(); i < n; ++i) {
                auto c = parseFilter(seq.at(i));
                if (c) children.push_back(c);
            }
            return std::make_shared<FilterConjunction>(conj, children);
        };
        if (auto f = readConj("and", Conj::And)) return f;
        if (auto f = readConj("or",  Conj::Or))  return f;
        if (auto f = readConj("not", Conj::Not)) return f;
    }
    return nullptr;
}

}  // namespace Corbomite::Bases
