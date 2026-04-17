// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Ast.h"
#include "EvalContext.h"

namespace Corbomite::Bases {

class FunctionRegistry;  // Phase 5 — forward-decl only.

/// Walks an AST against a context + (optional) function registry and
/// produces a ValuePtr. Runtime type errors are converted to
/// FormulaErrorValue at the offending binary-op / call site and
/// propagate upward via identity semantics.
class Evaluator
{
public:
    explicit Evaluator(FunctionRegistry *funcs = nullptr) : m_funcs(funcs) {}

    ValuePtr eval(const Expr &expr, const EvalContext &ctx) const;

    /// Convenience: one-shot evaluate a source string against a context.
    static ValuePtr evaluate(const QString &source,
                             const EvalContext &ctx,
                             FunctionRegistry *funcs = nullptr);

private:
    ValuePtr evalBinary(const BinaryExpr &b, const EvalContext &ctx) const;
    ValuePtr evalUnary(const UnaryExpr &u, const EvalContext &ctx) const;
    ValuePtr evalCall(const CallExpr &c, const EvalContext &ctx) const;
    ValuePtr evalIndex(const IndexExpr &e, const EvalContext &ctx) const;
    ValuePtr evalMember(const MemberExpr &e, const EvalContext &ctx) const;
    ValuePtr evalArray(const ArrayExpr &a, const EvalContext &ctx) const;

    // Binary dispatch (addendum §4.1 – §4.3).
    static ValuePtr applyEq(const Value *l, const Value *r, bool invert);
    static ValuePtr applyRelational(BinOp op, const Value *l, const Value *r);
    static ValuePtr applyArithmetic(BinOp op, const Value *l, const Value *r);
    // Logical short-circuiting (addendum §3 note).
    ValuePtr applyLogical(BinOp op, const Expr &leftExpr, const Expr &rightExpr,
                          const EvalContext &ctx) const;

    FunctionRegistry *m_funcs;
};

}  // namespace Corbomite::Bases
