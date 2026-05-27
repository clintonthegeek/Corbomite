// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/NewItemSeed.h"

#include "corbomite/bases/Ast.h"
#include "corbomite/bases/Formula.h"
#include "corbomite/bases/Values.h"

namespace Corbomite::Bases {
namespace {

/// Returns the frontmatter property key for an expression node, or an empty
/// string if the expression refers to a system property (file.*, formula,
/// this, note) or is not a simple identifier / note.member access.
QString propertyKey(const Expr *e)
{
    if (auto *id = dynamic_cast<const IdentExpr *>(e)) {
        const QString &n = id->name;
        // Skip bare system identifiers that are not user frontmatter keys.
        if (n == QLatin1String("file") || n == QLatin1String("formula")
            || n == QLatin1String("this") || n == QLatin1String("note"))
            return {};
        return n;
    }
    if (auto *mem = dynamic_cast<const MemberExpr *>(e)) {
        // `note.prop` → user frontmatter key `prop`.
        auto *obj = dynamic_cast<const IdentExpr *>(mem->object.get());
        if (obj && obj->name == QLatin1String("note"))
            return mem->member;
        // `file.anything` and other member accesses are system properties —
        // skip them.
    }
    return {};
}

void collectFromExpr(const Expr *e, QVector<QPair<QString, QString>> &out)
{
    if (!e) return;
    if (auto *bin = dynamic_cast<const BinaryExpr *>(e)) {
        if (bin->op == BinOp::AndAnd) {
            // Both sides are in an AND context — recurse into each.
            collectFromExpr(bin->left.get(), out);
            collectFromExpr(bin->right.get(), out);
            return;
        }
        if (bin->op == BinOp::Eq) {
            const Expr *lhs = bin->left.get();
            const Expr *rhs = bin->right.get();
            auto *litR = dynamic_cast<const LiteralExpr *>(rhs);
            auto *litL = dynamic_cast<const LiteralExpr *>(lhs);
            QString key;
            ValuePtr lit;
            if (litR) { key = propertyKey(lhs); lit = litR->value; }
            else if (litL) { key = propertyKey(rhs); lit = litL->value; }
            if (!key.isEmpty() && lit)
                out.append({key, lit->toString()});
        }
        // All other binary ops (!=, <, >, ||, etc.) contribute nothing.
    }
    // UnaryExpr, CallExpr, etc. — contribute nothing.
}

void collectFromFilter(const FilterPtr &f, QVector<QPair<QString, QString>> &out)
{
    if (!f) return;
    if (auto r = std::dynamic_pointer_cast<FilterRule>(f)) {
        collectFromExpr(r->rule().ast(), out);
        return;
    }
    if (auto cj = std::dynamic_pointer_cast<FilterConjunction>(f)) {
        if (cj->conj() == Conj::And) {
            for (const auto &child : cj->children())
                collectFromFilter(child, out);
        }
        // Or / Not conjunctions contribute nothing.
    }
}

}  // namespace

NewItemSeed::SeedList NewItemSeed::compute(const FilterPtr &filter, const SeedList &templateProps)
{
    SeedList seeds = templateProps;

    QVector<QPair<QString, QString>> equalities;
    collectFromFilter(filter, equalities);

    for (const auto &eq : equalities) {
        bool overrode = false;
        for (auto &existing : seeds) {
            if (existing.first == eq.first) {
                existing.second = eq.second;
                overrode = true;
                break;
            }
        }
        if (!overrode)
            seeds.append(eq);
    }

    return seeds;
}

}  // namespace Corbomite::Bases
