// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Evaluator.h"

#include "corbomite/bases/Parser.h"
#include "corbomite/bases/Values.h"

#include <KLocalizedString>

#include <cmath>

namespace Corbomite::Bases {

namespace {

inline const Value *nullIfNullValue(const Value *v)
{
    return dynamic_cast<const NullValue *>(v) ? v : nullptr;
}

std::shared_ptr<FormulaErrorValue> makeError(const QString &msg)
{
    return std::make_shared<FormulaErrorValue>(msg);
}

}  // namespace

ValuePtr Evaluator::eval(const Expr &expr, const EvalContext &ctx) const
{
    if (auto *l = dynamic_cast<const LiteralExpr *>(&expr))
        return l->value;
    if (auto *i = dynamic_cast<const IdentExpr *>(&expr)) {
        auto v = ctx.getByIdentifier(i->name);
        return v ? v : NullValue::instance();
    }
    if (auto *b = dynamic_cast<const BinaryExpr *>(&expr))
        return evalBinary(*b, ctx);
    if (auto *u = dynamic_cast<const UnaryExpr *>(&expr))
        return evalUnary(*u, ctx);
    if (auto *c = dynamic_cast<const CallExpr *>(&expr))
        return evalCall(*c, ctx);
    if (auto *idx = dynamic_cast<const IndexExpr *>(&expr))
        return evalIndex(*idx, ctx);
    if (auto *m = dynamic_cast<const MemberExpr *>(&expr))
        return evalMember(*m, ctx);
    if (auto *a = dynamic_cast<const ArrayExpr *>(&expr))
        return evalArray(*a, ctx);
    if (auto *inv = dynamic_cast<const InvalidExpr *>(&expr))
        return makeError(inv->message);
    if (dynamic_cast<const EmptyExpr *>(&expr))
        return NullValue::instance();
    return makeError(QStringLiteral("unknown AST node"));
}

ValuePtr Evaluator::evaluate(const QString &source,
                             const EvalContext &ctx,
                             FunctionRegistry *funcs)
{
    QString err;
    auto root = Parser::parse(source, &err);
    Evaluator e(funcs);
    return e.eval(*root, ctx);
}

ValuePtr Evaluator::evalArray(const ArrayExpr &a, const EvalContext &ctx) const
{
    QVector<ValuePtr> items;
    items.reserve(static_cast<int>(a.elems.size()));
    for (const auto &ep : a.elems) items.push_back(eval(*ep, ctx));
    return std::make_shared<ListValue>(items);
}

ValuePtr Evaluator::evalMember(const MemberExpr &e, const EvalContext &ctx) const
{
    auto obj = eval(*e.object, ctx);
    if (!obj) return NullValue::instance();
    auto v = obj->objectAccess(e.member);
    return v ? v : NullValue::instance();
}

ValuePtr Evaluator::evalIndex(const IndexExpr &e, const EvalContext &ctx) const
{
    auto obj = eval(*e.object, ctx);
    auto idx = eval(*e.index, ctx);
    if (!obj || dynamic_cast<NullValue *>(obj.get())) return NullValue::instance();
    if (!idx || dynamic_cast<NullValue *>(idx.get())) return NullValue::instance();
    if (auto *list = dynamic_cast<ListValue *>(obj.get())) {
        auto *n = dynamic_cast<NumberValue *>(idx.get());
        if (!n) return makeError(i18n("list index must be a number"));
        const int i = static_cast<int>(n->data());
        return list->get(i);
    }
    if (auto *objv = dynamic_cast<ObjectValue *>(obj.get())) {
        return objv->getInsensitive(idx->toString());
    }
    return makeError(i18n("type %1 does not support indexing", obj->type()));
}

ValuePtr Evaluator::evalUnary(const UnaryExpr &u, const EvalContext &ctx) const
{
    auto v = eval(*u.operand, ctx);
    if (!v) return NullValue::instance();
    if (u.op == UnOp::Not) {
        // addendum §4.4: !Null -> Null (propagation, not true).
        if (dynamic_cast<NullValue *>(v.get())) return NullValue::instance();
        return std::make_shared<BooleanValue>(!v->isTruthy());
    }
    // Negate.
    if (dynamic_cast<NullValue *>(v.get())) return NullValue::instance();
    if (auto *n = dynamic_cast<NumberValue *>(v.get()))
        return std::make_shared<NumberValue>(-n->data());
    return makeError(i18n("cannot negate %1", v->type()));
}

ValuePtr Evaluator::evalBinary(const BinaryExpr &b, const EvalContext &ctx) const
{
    if (b.op == BinOp::OrOr || b.op == BinOp::AndAnd)
        return applyLogical(b.op, *b.left, *b.right, ctx);

    auto l = eval(*b.left, ctx);
    auto r = eval(*b.right, ctx);

    // FormulaErrorValue propagates through arithmetic/comparison.
    if (dynamic_cast<FormulaErrorValue *>(l.get())) return l;
    if (dynamic_cast<FormulaErrorValue *>(r.get())) return r;

    switch (b.op) {
    case BinOp::Eq:  return applyEq(l.get(), r.get(), /*invert=*/false);
    case BinOp::Neq: return applyEq(l.get(), r.get(), /*invert=*/true);
    case BinOp::Lt:
    case BinOp::Gt:
    case BinOp::LtEq:
    case BinOp::GtEq:
        return applyRelational(b.op, l.get(), r.get());
    case BinOp::Add:
    case BinOp::Sub:
    case BinOp::Mul:
    case BinOp::Div:
    case BinOp::Mod:
        return applyArithmetic(b.op, l.get(), r.get());
    default:
        return makeError(i18n("unhandled binary operator"));
    }
}

ValuePtr Evaluator::applyLogical(BinOp op, const Expr &leftExpr, const Expr &rightExpr,
                                 const EvalContext &ctx) const
{
    auto l = eval(leftExpr, ctx);
    const bool lt = l && l->isTruthy();
    if (op == BinOp::OrOr) {
        if (lt) return std::make_shared<BooleanValue>(true);
        auto r = eval(rightExpr, ctx);
        return std::make_shared<BooleanValue>(r && r->isTruthy());
    }
    // AndAnd: short-circuit on falsy left.
    if (!lt) return std::make_shared<BooleanValue>(false);
    auto r = eval(rightExpr, ctx);
    return std::make_shared<BooleanValue>(r && r->isTruthy());
}

// ----- equality -----

ValuePtr Evaluator::applyEq(const Value *l, const Value *r, bool invert)
{
    const bool eq = Value::staticLooseEquals(l, r);
    return std::make_shared<BooleanValue>(invert ? !eq : eq);
}

// ----- relational -----

ValuePtr Evaluator::applyRelational(BinOp op, const Value *l, const Value *r)
{
    if (nullIfNullValue(l) || nullIfNullValue(r))
        return NullValue::instance();

    double lv = 0.0, rv = 0.0;
    bool numeric = false;

    auto *ln = dynamic_cast<const NumberValue *>(l);
    auto *rn = dynamic_cast<const NumberValue *>(r);
    auto *ld = dynamic_cast<const DateValue *>(l);
    auto *rd = dynamic_cast<const DateValue *>(r);
    auto *ldu = dynamic_cast<const DurationValue *>(l);
    auto *rdu = dynamic_cast<const DurationValue *>(r);
    auto *ls = dynamic_cast<const StringValue *>(l);
    auto *rs = dynamic_cast<const StringValue *>(r);

    // Date ↔ Date.
    if (ld && rd) {
        lv = static_cast<double>(ld->dateTime().toMSecsSinceEpoch());
        rv = static_cast<double>(rd->dateTime().toMSecsSinceEpoch());
        numeric = true;
    }
    // Date ↔ String(coerce to Date).
    else if (ld && rs && !rn) {
        auto coerced = DateValue::parseFromString(rs->data());
        if (coerced) {
            lv = static_cast<double>(ld->dateTime().toMSecsSinceEpoch());
            rv = static_cast<double>(coerced->dateTime().toMSecsSinceEpoch());
            numeric = true;
        }
    }
    // String(coerce) ↔ Date.
    else if (rd && ls && !ln) {
        auto coerced = DateValue::parseFromString(ls->data());
        if (coerced) {
            lv = static_cast<double>(coerced->dateTime().toMSecsSinceEpoch());
            rv = static_cast<double>(rd->dateTime().toMSecsSinceEpoch());
            numeric = true;
        }
    }
    // Duration ↔ Duration.
    else if (ldu && rdu) {
        lv = static_cast<double>(ldu->totalMilliseconds());
        rv = static_cast<double>(rdu->totalMilliseconds());
        numeric = true;
    }
    // Duration ↔ String(coerce).
    else if (ldu && rs && !rn) {
        auto coerced = DurationValue::parseFromString(rs->data());
        if (coerced) {
            lv = static_cast<double>(ldu->totalMilliseconds());
            rv = static_cast<double>(coerced->totalMilliseconds());
            numeric = true;
        }
    }
    // Number ↔ Number.
    else if (ln && rn) {
        lv = ln->data();
        rv = rn->data();
        numeric = true;
    }

    if (numeric) {
        bool result = false;
        switch (op) {
        case BinOp::Lt:   result = lv <  rv; break;
        case BinOp::Gt:   result = lv >  rv; break;
        case BinOp::LtEq: result = lv <= rv; break;
        case BinOp::GtEq: result = lv >= rv; break;
        default: break;
        }
        return std::make_shared<BooleanValue>(result);
    }

    // Fallback: locale-aware string comparison.
    const QString ls2 = l ? l->toString() : QString{};
    const QString rs2 = r ? r->toString() : QString{};
    const int cmp = QString::localeAwareCompare(ls2, rs2);
    bool result = false;
    switch (op) {
    case BinOp::Lt:   result = cmp <  0; break;
    case BinOp::Gt:   result = cmp >  0; break;
    case BinOp::LtEq: result = cmp <= 0; break;
    case BinOp::GtEq: result = cmp >= 0; break;
    default: break;
    }
    return std::make_shared<BooleanValue>(result);
}

// ----- arithmetic -----

ValuePtr Evaluator::applyArithmetic(BinOp op, const Value *l, const Value *r)
{
    if (nullIfNullValue(l) || nullIfNullValue(r))
        return NullValue::instance();

    auto *ln  = dynamic_cast<const NumberValue *>(l);
    auto *rn  = dynamic_cast<const NumberValue *>(r);
    auto *ld  = dynamic_cast<const DateValue *>(l);
    auto *rd  = dynamic_cast<const DateValue *>(r);
    auto *ldu = dynamic_cast<const DurationValue *>(l);
    auto *rdu = dynamic_cast<const DurationValue *>(r);
    auto *ls  = dynamic_cast<const StringValue *>(l);
    auto *rs  = dynamic_cast<const StringValue *>(r);

    // Number op Number.
    if (ln && rn) {
        const double a = ln->data(), b = rn->data();
        switch (op) {
        case BinOp::Add: return std::make_shared<NumberValue>(a + b);
        case BinOp::Sub: return std::make_shared<NumberValue>(a - b);
        case BinOp::Mul: return std::make_shared<NumberValue>(a * b);
        case BinOp::Div: return std::make_shared<NumberValue>(a / b);
        case BinOp::Mod: return std::make_shared<NumberValue>(std::fmod(a, b));
        default: break;
        }
    }

    // Date +- String(coerce to Duration).
    std::shared_ptr<DurationValue> coercedRhsDur;
    if (ld && !rdu && rs) {
        coercedRhsDur = DurationValue::parseFromString(rs->data());
        if (coercedRhsDur) rdu = coercedRhsDur.get();
    }

    // Date +- Duration.
    if (ld && rdu && (op == BinOp::Add || op == BinOp::Sub))
        return rdu->addToDate(*ld, op == BinOp::Sub);

    // Duration +- Duration.
    if (ldu && rdu) {
        if (op == BinOp::Add)
            return std::make_shared<DurationValue>(ldu->plus(rdu->components()));
        if (op == BinOp::Sub)
            return std::make_shared<DurationValue>(ldu->minus(rdu->components()));
    }

    // Duration * or / Number (not commutative — addendum §4.3).
    if (ldu && rn && (op == BinOp::Mul || op == BinOp::Div)) {
        const double n = rn->data();
        return std::make_shared<DurationValue>(
            ldu->timesScalar(op == BinOp::Div ? 1.0 / n : n));
    }

    // Date - Date -> Duration.fromMilliseconds.
    if (ld && rd && op == BinOp::Sub) {
        const qint64 delta = ld->dateTime().toMSecsSinceEpoch()
                           - rd->dateTime().toMSecsSinceEpoch();
        return DurationValue::fromMilliseconds(delta);
    }

    // List + List concat.
    if (op == BinOp::Add) {
        auto *ll = dynamic_cast<const ListValue *>(l);
        auto *rl = dynamic_cast<const ListValue *>(r);
        if (ll && rl) return ll->concat(*rl);
    }

    // `+` with any StringValue -> coerce both sides and concatenate.
    if (op == BinOp::Add && (ls || rs)) {
        const QString lstr = l ? l->toString() : QString{};
        const QString rstr = r ? r->toString() : QString{};
        return std::make_shared<StringValue>(lstr + rstr);
    }

    return makeError(i18n("invalid operator between %1 and %2",
                          l ? l->type() : QStringLiteral("null"),
                          r ? r->type() : QStringLiteral("null")));
}

ValuePtr Evaluator::evalCall(const CallExpr &c, const EvalContext &ctx) const
{
    // Without a function registry, no call can be resolved. Evaluator
    // still needs to evaluate args so side-effect-free expressions run
    // (but we have no side effects). Return an error referencing the
    // callee name.
    QString calleeName;
    if (auto *ident = dynamic_cast<IdentExpr *>(c.callee.get()))
        calleeName = ident->name;
    else if (auto *mem = dynamic_cast<MemberExpr *>(c.callee.get()))
        calleeName = mem->member;
    // Phase 5 wires m_funcs + hard-cased specials; until then:
    Q_UNUSED(ctx);
    return makeError(i18n("unknown function '%1'", calleeName));
}

}  // namespace Corbomite::Bases
