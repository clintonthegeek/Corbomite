// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/bases/Parser.h"
#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

namespace {

template <typename T>
const T *as(const Expr *e)
{
    return dynamic_cast<const T *>(e);
}

}  // namespace

class TestParser : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void testEmpty()
    {
        auto e = Parser::parse(QString{});
        QVERIFY(as<EmptyExpr>(e.get()));
    }

    void testNumberLiteral()
    {
        auto e = Parser::parse(QStringLiteral("42"));
        auto *lit = as<LiteralExpr>(e.get());
        QVERIFY(lit);
        QCOMPARE(std::static_pointer_cast<NumberValue>(lit->value)->data(), 42.0);
    }

    void testConstantFoldNegativeNumber()
    {
        auto e = Parser::parse(QStringLiteral("-42"));
        auto *lit = as<LiteralExpr>(e.get());
        QVERIFY(lit);
        QCOMPARE(std::static_pointer_cast<NumberValue>(lit->value)->data(), -42.0);
    }

    void testUnaryMinusOnIdentifier()
    {
        auto e = Parser::parse(QStringLiteral("-a"));
        auto *u = as<UnaryExpr>(e.get());
        QVERIFY(u);
        QCOMPARE(static_cast<int>(u->op), static_cast<int>(UnOp::Negate));
        QVERIFY(as<IdentExpr>(u->operand.get()));
    }

    void testPrecedenceMultBeforeAdd()
    {
        auto e = Parser::parse(QStringLiteral("a + b * c"));
        auto *bin = as<BinaryExpr>(e.get());
        QVERIFY(bin);
        QCOMPARE(static_cast<int>(bin->op), static_cast<int>(BinOp::Add));
        QVERIFY(as<IdentExpr>(bin->left.get()));
        auto *rhs = as<BinaryExpr>(bin->right.get());
        QVERIFY(rhs);
        QCOMPARE(static_cast<int>(rhs->op), static_cast<int>(BinOp::Mul));
    }

    void testParenGrouping()
    {
        auto e = Parser::parse(QStringLiteral("(a + b) * c"));
        auto *bin = as<BinaryExpr>(e.get());
        QVERIFY(bin);
        QCOMPARE(static_cast<int>(bin->op), static_cast<int>(BinOp::Mul));
        QVERIFY(as<BinaryExpr>(bin->left.get()));
    }

    void testLogicalPrecedence()
    {
        // `||` binds looser than `&&`.
        auto e = Parser::parse(QStringLiteral("a || b && c"));
        auto *bin = as<BinaryExpr>(e.get());
        QVERIFY(bin);
        QCOMPARE(static_cast<int>(bin->op), static_cast<int>(BinOp::OrOr));
        auto *rhs = as<BinaryExpr>(bin->right.get());
        QVERIFY(rhs);
        QCOMPARE(static_cast<int>(rhs->op), static_cast<int>(BinOp::AndAnd));
    }

    void testUnaryNot()
    {
        auto e = Parser::parse(QStringLiteral("!a"));
        auto *u = as<UnaryExpr>(e.get());
        QVERIFY(u);
        QCOMPARE(static_cast<int>(u->op), static_cast<int>(UnOp::Not));
    }

    void testMemberAccess()
    {
        auto e = Parser::parse(QStringLiteral("a.b.c"));
        auto *m = as<MemberExpr>(e.get());
        QVERIFY(m);
        QCOMPARE(m->member, QStringLiteral("c"));
        auto *inner = as<MemberExpr>(m->object.get());
        QVERIFY(inner);
        QCOMPARE(inner->member, QStringLiteral("b"));
        QVERIFY(as<IdentExpr>(inner->object.get()));
    }

    void testIndexAccess()
    {
        auto e = Parser::parse(QStringLiteral("a[0]"));
        auto *idx = as<IndexExpr>(e.get());
        QVERIFY(idx);
        QVERIFY(as<IdentExpr>(idx->object.get()));
        auto *lit = as<LiteralExpr>(idx->index.get());
        QVERIFY(lit);
        QCOMPARE(std::static_pointer_cast<NumberValue>(lit->value)->data(), 0.0);
    }

    void testCall()
    {
        auto e = Parser::parse(QStringLiteral("f(1, 2)"));
        auto *c = as<CallExpr>(e.get());
        QVERIFY(c);
        QVERIFY(as<IdentExpr>(c->callee.get()));
        QCOMPARE(c->args.size(), 2);
    }

    void testMemberCall()
    {
        auto e = Parser::parse(QStringLiteral("a.b(1)"));
        auto *c = as<CallExpr>(e.get());
        QVERIFY(c);
        QVERIFY(as<MemberExpr>(c->callee.get()));
        QCOMPARE(c->args.size(), 1);
    }

    void testArrayLiteral()
    {
        auto e = Parser::parse(QStringLiteral("[1, 2, 3]"));
        auto *a = as<ArrayExpr>(e.get());
        QVERIFY(a);
        QCOMPARE(a->elems.size(), 3);
    }

    void testLeftAssocSubtract()
    {
        // a - b - c  =>  (a - b) - c
        auto e = Parser::parse(QStringLiteral("a - b - c"));
        auto *top = as<BinaryExpr>(e.get());
        QVERIFY(top);
        QCOMPARE(static_cast<int>(top->op), static_cast<int>(BinOp::Sub));
        auto *left = as<BinaryExpr>(top->left.get());
        QVERIFY(left);
        QCOMPARE(static_cast<int>(left->op), static_cast<int>(BinOp::Sub));
        QVERIFY(as<IdentExpr>(top->right.get()));
    }

    void testMalformedYieldsInvalid()
    {
        auto e = Parser::parse(QStringLiteral("+++"));
        QVERIFY(as<InvalidExpr>(e.get()));
    }

    void testTrailingGarbageYieldsInvalid()
    {
        auto e = Parser::parse(QStringLiteral("a b"));
        QVERIFY(as<InvalidExpr>(e.get()));
    }

    void testEqualityOperators()
    {
        auto e = Parser::parse(QStringLiteral("a == b"));
        auto *bin = as<BinaryExpr>(e.get());
        QVERIFY(bin);
        QCOMPARE(static_cast<int>(bin->op), static_cast<int>(BinOp::Eq));
    }

    void testRelational()
    {
        auto e = Parser::parse(QStringLiteral("a < b"));
        auto *bin = as<BinaryExpr>(e.get());
        QVERIFY(bin);
        QCOMPARE(static_cast<int>(bin->op), static_cast<int>(BinOp::Lt));
    }

    void testNullLiteral()
    {
        auto e = Parser::parse(QStringLiteral("null"));
        auto *lit = as<LiteralExpr>(e.get());
        QVERIFY(lit);
        QCOMPARE(lit->value->type(), QStringLiteral("Null"));
    }

    void testBooleanLiterals()
    {
        auto et = Parser::parse(QStringLiteral("true"));
        auto *tl = as<LiteralExpr>(et.get());
        QVERIFY(tl);
        QVERIFY(tl->value->isTruthy());

        auto ef = Parser::parse(QStringLiteral("false"));
        auto *fl = as<LiteralExpr>(ef.get());
        QVERIFY(fl);
        QVERIFY(!fl->value->isTruthy());
    }
};

QTEST_APPLESS_MAIN(TestParser)
#include "tst_parser.moc"
