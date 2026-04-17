// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ValuePtr.h"

#include <QString>

#include <memory>
#include <vector>

namespace Corbomite::Bases {

class Expr;
using ExprPtr = std::unique_ptr<Expr>;

/// Abstract AST node. Evaluator (Phase 4) visits via virtual dispatch
/// through `Evaluator::eval(const Expr &, const EvalContext &)`.
class Expr
{
public:
    virtual ~Expr() = default;
    virtual QString nodeName() const = 0;
};

/// Null/Bool/Number/String/Regex literal — holds an already-constructed
/// ValuePtr so the evaluator can return it directly.
class LiteralExpr : public Expr
{
public:
    explicit LiteralExpr(ValuePtr v) : value(std::move(v)) {}
    QString nodeName() const override { return QStringLiteral("Literal"); }
    ValuePtr value;
};

/// Bare identifier: `status`, `note`, `file`, `formula`, `this`.
class IdentExpr : public Expr
{
public:
    explicit IdentExpr(QString n) : name(std::move(n)) {}
    QString nodeName() const override { return QStringLiteral("Ident"); }
    QString name;
};

/// Array literal `[e1, e2, ...]`.
class ArrayExpr : public Expr
{
public:
    explicit ArrayExpr(std::vector<ExprPtr> es) : elems(std::move(es)) {}
    QString nodeName() const override { return QStringLiteral("Array"); }
    std::vector<ExprPtr> elems;
};

enum class BinOp
{
    OrOr, AndAnd,
    Eq, Neq,
    Lt, Gt, LtEq, GtEq,
    Add, Sub, Mul, Div, Mod
};

class BinaryExpr : public Expr
{
public:
    BinaryExpr(BinOp op_, ExprPtr l, ExprPtr r)
        : op(op_), left(std::move(l)), right(std::move(r)) {}
    QString nodeName() const override { return QStringLiteral("Binary"); }
    BinOp op;
    ExprPtr left, right;
};

enum class UnOp { Not, Negate };

class UnaryExpr : public Expr
{
public:
    UnaryExpr(UnOp op_, ExprPtr e) : op(op_), operand(std::move(e)) {}
    QString nodeName() const override { return QStringLiteral("Unary"); }
    UnOp op;
    ExprPtr operand;
};

/// `callee(args...)`. callee may be an IdentExpr (global function) or a
/// MemberExpr (instance function on the receiver).
class CallExpr : public Expr
{
public:
    CallExpr(ExprPtr f, std::vector<ExprPtr> a)
        : callee(std::move(f)), args(std::move(a)) {}
    QString nodeName() const override { return QStringLiteral("Call"); }
    ExprPtr callee;
    std::vector<ExprPtr> args;
};

/// `object[index]`.
class IndexExpr : public Expr
{
public:
    IndexExpr(ExprPtr o, ExprPtr i)
        : object(std::move(o)), index(std::move(i)) {}
    QString nodeName() const override { return QStringLiteral("Index"); }
    ExprPtr object, index;
};

/// `object.member`.
class MemberExpr : public Expr
{
public:
    MemberExpr(ExprPtr o, QString m)
        : object(std::move(o)), member(std::move(m)) {}
    QString nodeName() const override { return QStringLiteral("Member"); }
    ExprPtr object;
    QString member;
};

/// Parse-error sentinel. Top-level `InvalidExpr` is handed to the
/// evaluator, which wraps it into a FormulaErrorValue at call time.
class InvalidExpr : public Expr
{
public:
    explicit InvalidExpr(QString msg) : message(std::move(msg)) {}
    QString nodeName() const override { return QStringLiteral("Invalid"); }
    QString message;
};

/// Empty-input sentinel (equivalent to the `BK` class in the addendum).
/// Evaluator returns NullValue.
class EmptyExpr : public Expr
{
public:
    QString nodeName() const override { return QStringLiteral("Empty"); }
};

}  // namespace Corbomite::Bases
