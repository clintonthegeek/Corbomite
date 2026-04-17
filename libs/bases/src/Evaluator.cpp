// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Evaluator.h"

#include "corbomite/bases/FunctionRegistry.h"
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

namespace {

// Addendum §5.2 — iteration-bound shadowing context for list/object
// lambdas (map/filter/reduce).
class ShadowingContext : public EvalContext
{
public:
    ShadowingContext(const EvalContext &outer, QHash<QString, ValuePtr> binds)
        : m_outer(outer), m_binds(std::move(binds)) {}

    ValuePtr getByIdentifier(const QString &name) const override
    {
        auto it = m_binds.constFind(name);
        if (it != m_binds.constEnd()) return *it;
        return m_outer.getByIdentifier(name);
    }

private:
    const EvalContext &m_outer;
    QHash<QString, ValuePtr> m_binds;
};

}  // namespace

ValuePtr Evaluator::evalCall(const CallExpr &c, const EvalContext &ctx) const
{
    // Hard-cased dispatch intercepts BEFORE arg evaluation so lambda bodies
    // can see the iteration-bound scope (addendum §5.2, §8).
    if (auto *ident = dynamic_cast<IdentExpr *>(c.callee.get())) {
        if (ident->name.compare(QLatin1String("if"), Qt::CaseInsensitive) == 0) {
            if (c.args.size() < 2 || c.args.size() > 3)
                return makeError(i18n("if takes 2 or 3 arguments"));
            auto cond = eval(*c.args[0], ctx);
            if (cond && cond->isTruthy()) return eval(*c.args[1], ctx);
            if (c.args.size() == 3) return eval(*c.args[2], ctx);
            return NullValue::instance();
        }
    }
    if (auto *member = dynamic_cast<MemberExpr *>(c.callee.get())) {
        const QString lower = member->member.toLower();
        const bool isListLambda = (lower == QLatin1String("map")
                                || lower == QLatin1String("filter")
                                || lower == QLatin1String("reduce"));
        const bool isObjectLambda = (lower == QLatin1String("map")
                                  || lower == QLatin1String("filter"));
        if (isListLambda) {
            auto subject = eval(*member->object, ctx);
            if (auto *list = dynamic_cast<ListValue *>(subject.get())) {
                if (lower == QLatin1String("map")) {
                    if (c.args.size() != 1)
                        return makeError(i18n("map(expr) takes 1 argument"));
                    QVector<ValuePtr> out;
                    out.reserve(list->length());
                    for (int i = 0; i < list->length(); ++i) {
                        QHash<QString, ValuePtr> b;
                        b[QStringLiteral("index")] = std::make_shared<NumberValue>(i);
                        b[QStringLiteral("value")] = list->get(i);
                        ShadowingContext sc(ctx, b);
                        out.push_back(eval(*c.args[0], sc));
                    }
                    return std::make_shared<ListValue>(out);
                }
                if (lower == QLatin1String("filter")) {
                    if (c.args.size() != 1)
                        return makeError(i18n("filter(pred) takes 1 argument"));
                    QVector<ValuePtr> out;
                    for (int i = 0; i < list->length(); ++i) {
                        QHash<QString, ValuePtr> b;
                        b[QStringLiteral("index")] = std::make_shared<NumberValue>(i);
                        b[QStringLiteral("value")] = list->get(i);
                        ShadowingContext sc(ctx, b);
                        auto pred = eval(*c.args[0], sc);
                        if (pred && pred->isTruthy()) out.push_back(list->get(i));
                    }
                    return std::make_shared<ListValue>(out);
                }
                if (lower == QLatin1String("reduce")) {
                    if (c.args.size() != 2)
                        return makeError(i18n("reduce(expr, initial) takes 2 arguments"));
                    ValuePtr acc = eval(*c.args[1], ctx);
                    for (int i = 0; i < list->length(); ++i) {
                        QHash<QString, ValuePtr> b;
                        b[QStringLiteral("index")] = std::make_shared<NumberValue>(i);
                        b[QStringLiteral("value")] = list->get(i);
                        b[QStringLiteral("acc")]   = acc;
                        ShadowingContext sc(ctx, b);
                        acc = eval(*c.args[0], sc);
                    }
                    return acc ? acc : NullValue::instance();
                }
            }
            if (auto *obj = dynamic_cast<ObjectValue *>(subject.get())) {
                if (isObjectLambda && lower == QLatin1String("map")) {
                    if (c.args.size() != 1)
                        return makeError(i18n("map(expr) takes 1 argument"));
                    QVector<ValuePtr> out;
                    for (const auto &[k, v] : obj->entries()) {
                        QHash<QString, ValuePtr> b;
                        b[QStringLiteral("key")] = std::make_shared<StringValue>(k);
                        b[QStringLiteral("value")] = v;
                        ShadowingContext sc(ctx, b);
                        out.push_back(eval(*c.args[0], sc));
                    }
                    return std::make_shared<ListValue>(out);
                }
                if (isObjectLambda && lower == QLatin1String("filter")) {
                    if (c.args.size() != 1)
                        return makeError(i18n("filter(pred) takes 1 argument"));
                    auto out = std::make_shared<ObjectValue>();
                    for (const auto &[k, v] : obj->entries()) {
                        QHash<QString, ValuePtr> b;
                        b[QStringLiteral("key")] = std::make_shared<StringValue>(k);
                        b[QStringLiteral("value")] = v;
                        ShadowingContext sc(ctx, b);
                        auto pred = eval(*c.args[0], sc);
                        if (pred && pred->isTruthy()) out->set(k, v);
                    }
                    return out;
                }
            }
            // Fall through to the regular registry path — lets
            // per-type-named functions with these same names still resolve
            // on non-list/object subjects (shouldn't exist but harmless).
        }
    }

    // Regular dispatch through FunctionRegistry.
    const BasesFunction *fn = nullptr;
    ValuePtr subject;
    QVector<ValuePtr> args;

    if (auto *member = dynamic_cast<MemberExpr *>(c.callee.get())) {
        subject = eval(*member->object, ctx);
        args.push_back(subject);
        if (m_funcs) fn = m_funcs->findInstance(subject.get(), member->member);
        if (!fn) {
            return makeError(i18n("unknown instance function '%1' on %2",
                                  member->member,
                                  subject ? subject->type() : QStringLiteral("null")));
        }
    } else if (auto *ident = dynamic_cast<IdentExpr *>(c.callee.get())) {
        if (m_funcs) fn = m_funcs->findGlobal(ident->name);
        if (!fn) return makeError(i18n("unknown function '%1'", ident->name));
    } else {
        return makeError(i18n("calling non-callable expression"));
    }

    for (const auto &a : c.args) args.push_back(eval(*a, ctx));
    return fn->apply(ctx, args);
}

}  // namespace Corbomite::Bases
