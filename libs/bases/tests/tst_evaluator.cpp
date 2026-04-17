// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/bases/Evaluator.h"
#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

namespace {

ValuePtr eval(const QString &source, const EvalContext &ctx)
{
    return Evaluator::evaluate(source, ctx);
}

// Simple lambda context.
EvalContext *noIdent()
{
    static LambdaContext ctx([](const QString &) -> ValuePtr { return nullptr; });
    return &ctx;
}

}  // namespace

class TestEvaluator : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    // ----- arithmetic -----

    void testNumberAdd()
    {
        auto v = eval(QStringLiteral("1 + 2"), *noIdent());
        QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 3.0);
    }

    void testNumberDivide()
    {
        auto v = eval(QStringLiteral("10 / 4"), *noIdent());
        QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 2.5);
    }

    void testNumberModulo()
    {
        auto v = eval(QStringLiteral("7 % 3"), *noIdent());
        QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 1.0);
    }

    void testStringPlusString()
    {
        auto v = eval(QStringLiteral("'a' + 'b'"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("String"));
        QCOMPARE(v->toString(), QStringLiteral("ab"));
    }

    void testStringPlusNumber()
    {
        auto v = eval(QStringLiteral("'x' + 1"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("String"));
        QCOMPARE(v->toString(), QStringLiteral("x1"));
    }

    void testNullPropagationAdd()
    {
        auto v = eval(QStringLiteral("null + 5"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("Null"));
    }

    void testNullPropagationMul()
    {
        auto v = eval(QStringLiteral("5 * null"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("Null"));
    }

    // ----- comparison -----

    void testNumericEquality()
    {
        auto v = eval(QStringLiteral("3 == 3"), *noIdent());
        QVERIFY(v->isTruthy());
        auto v2 = eval(QStringLiteral("3 != 3"), *noIdent());
        QVERIFY(!v2->isTruthy());
    }

    void testRelational()
    {
        QVERIFY(eval(QStringLiteral("1 < 2"), *noIdent())->isTruthy());
        QVERIFY(eval(QStringLiteral("2 > 1"), *noIdent())->isTruthy());
        QVERIFY(eval(QStringLiteral("2 >= 2"), *noIdent())->isTruthy());
        QVERIFY(eval(QStringLiteral("2 <= 2"), *noIdent())->isTruthy());
    }

    void testRelationalNullPropagates()
    {
        auto v = eval(QStringLiteral("null < 5"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("Null"));
    }

    // ----- logical -----

    void testLogicalReturnsBoolean()
    {
        // addendum §3 note: a || b always produces a fresh BooleanValue.
        auto v = eval(QStringLiteral("true || 42"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("Boolean"));
        QVERIFY(v->isTruthy());
    }

    void testLogicalAnd()
    {
        QVERIFY(!eval(QStringLiteral("false && true"), *noIdent())->isTruthy());
        QVERIFY(eval(QStringLiteral("true && 1"), *noIdent())->isTruthy());
    }

    void testLogicalShortCircuit()
    {
        // Right-hand side not evaluated when left short-circuits — use
        // an expression that would error if evaluated.
        auto v = eval(QStringLiteral("false && (1 + nothere)"),
                      LambdaContext([](const QString &) -> ValuePtr { return nullptr; }));
        QCOMPARE(v->type(), QStringLiteral("Boolean"));
        QVERIFY(!v->isTruthy());
    }

    // ----- unary -----

    void testUnaryNotOnTrue()
    {
        auto v = eval(QStringLiteral("!true"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("Boolean"));
        QVERIFY(!v->isTruthy());
    }

    void testUnaryNotPropagatesNull()
    {
        // addendum §4.4: !Null -> Null (NOT true).
        auto v = eval(QStringLiteral("!null"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("Null"));
    }

    void testConstantFoldedNegative()
    {
        auto v = eval(QStringLiteral("-42"), *noIdent());
        QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), -42.0);
    }

    // ----- identifier + member + index -----

    void testIdentifierResolution()
    {
        LambdaContext ctx([](const QString &n) -> ValuePtr {
            if (n == QLatin1String("x")) return std::make_shared<NumberValue>(5);
            return nullptr;
        });
        auto v = eval(QStringLiteral("x + 1"), ctx);
        QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 6.0);
    }

    void testUnresolvedIdentifierIsNull()
    {
        auto v = eval(QStringLiteral("undefined"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("Null"));
    }

    void testMemberAccess()
    {
        auto obj = std::make_shared<ObjectValue>();
        obj->set(QStringLiteral("status"), std::make_shared<StringValue>(QStringLiteral("open")));
        LambdaContext ctx([&obj](const QString &n) -> ValuePtr {
            if (n == QLatin1String("note")) return obj;
            return nullptr;
        });
        auto v = eval(QStringLiteral("note.status"), ctx);
        QCOMPARE(v->toString(), QStringLiteral("open"));
    }

    void testArrayLiteralAndIndex()
    {
        auto v = eval(QStringLiteral("[10, 20, 30][1]"), *noIdent());
        QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 20.0);
    }

    void testIndexOutOfRangeIsNull()
    {
        auto v = eval(QStringLiteral("[1,2][99]"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("Null"));
    }

    // ----- error propagation -----

    void testInvalidArithmeticBetweenBoolAndString()
    {
        // Boolean - String has no arithmetic rule; should produce a
        // FormulaErrorValue (falsy).
        auto v = eval(QStringLiteral("true - 'x'"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("Error"));
    }

    void testUnknownFunctionIsError()
    {
        auto v = eval(QStringLiteral("doesNotExist(1)"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("Error"));
    }

    void testMalformedSourceProducesError()
    {
        auto v = eval(QStringLiteral("+++"), *noIdent());
        QCOMPARE(v->type(), QStringLiteral("Error"));
    }
};

QTEST_APPLESS_MAIN(TestEvaluator)
#include "tst_evaluator.moc"
