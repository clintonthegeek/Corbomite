// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Parser.h"

#include "corbomite/bases/Values.h"

#include <QRegularExpression>

namespace Corbomite::Bases {

Parser::Parser(QVector<Token> tokens) : m_toks(std::move(tokens)) {}

const Token &Parser::peek(int ahead) const
{
    static const Token kEnd { TokKind::End, 0, 0 };
    const int i = m_pos + ahead;
    return i < m_toks.size() ? m_toks[i] : kEnd;
}

const Token &Parser::advance()
{
    const Token &t = peek();
    if (m_pos < m_toks.size()) ++m_pos;
    return t;
}

bool Parser::match(TokKind k)
{
    if (peek().kind == k) { advance(); return true; }
    return false;
}

Parser::InfixBp Parser::infixBp(TokKind k)
{
    switch (k) {
    case TokKind::OrOr:     return {1, 2};
    case TokKind::AndAnd:   return {3, 4};
    case TokKind::EqEq:
    case TokKind::BangEq:   return {5, 6};
    case TokKind::Lt:
    case TokKind::Gt:
    case TokKind::LtEq:
    case TokKind::GtEq:     return {7, 8};
    case TokKind::Plus:
    case TokKind::Minus:    return {9, 10};
    case TokKind::Star:
    case TokKind::Slash:
    case TokKind::Percent:  return {11, 12};
    default: return {0, 0};
    }
}

int Parser::prefixBp(TokKind k)
{
    return (k == TokKind::Bang || k == TokKind::Minus) ? 13 : 0;
}

ExprPtr Parser::foldUnaryNegate(ExprPtr operand)
{
    if (auto *lit = dynamic_cast<LiteralExpr *>(operand.get())) {
        if (auto *n = dynamic_cast<NumberValue *>(lit->value.get())) {
            return std::make_unique<LiteralExpr>(
                std::make_shared<NumberValue>(-n->data()));
        }
    }
    return std::make_unique<UnaryExpr>(UnOp::Negate, std::move(operand));
}

ExprPtr Parser::parsePrimary()
{
    const Token t = peek();
    switch (t.kind) {
    case TokKind::Null:
        advance();
        return std::make_unique<LiteralExpr>(NullValue::instance());
    case TokKind::True:
        advance();
        return std::make_unique<LiteralExpr>(std::make_shared<BooleanValue>(true));
    case TokKind::False:
        advance();
        return std::make_unique<LiteralExpr>(std::make_shared<BooleanValue>(false));
    case TokKind::Number:
        advance();
        return std::make_unique<LiteralExpr>(std::make_shared<NumberValue>(t.numberValue));
    case TokKind::String:
        advance();
        return std::make_unique<LiteralExpr>(std::make_shared<StringValue>(t.textValue));
    case TokKind::Regex: {
        advance();
        auto re = RegExpValue::parseFromString(
            QStringLiteral("/%1/%2").arg(t.textValue, t.regexFlags));
        if (!re) {
            m_error = QStringLiteral("invalid regex");
            return std::make_unique<InvalidExpr>(m_error);
        }
        return std::make_unique<LiteralExpr>(re);
    }
    case TokKind::Identifier:
        advance();
        return std::make_unique<IdentExpr>(t.textValue);
    case TokKind::LParen: {
        advance();
        auto inner = parseExpression(0);
        if (!match(TokKind::RParen)) {
            m_error = QStringLiteral("expected ')'");
            return std::make_unique<InvalidExpr>(m_error);
        }
        return inner;
    }
    case TokKind::LBracket:
        return parseArrayLiteral();
    case TokKind::Invalid:
        advance();
        m_error = t.errorMessage.isEmpty()
                      ? QStringLiteral("invalid token")
                      : t.errorMessage;
        return std::make_unique<InvalidExpr>(m_error);
    default:
        m_error = QStringLiteral("expected primary");
        return std::make_unique<InvalidExpr>(m_error);
    }
}

ExprPtr Parser::parseArrayLiteral()
{
    advance();  // consume '['
    std::vector<ExprPtr> elems;
    if (!check(TokKind::RBracket)) {
        for (;;) {
            elems.push_back(parseExpression(0));
            if (!match(TokKind::Comma)) break;
            if (check(TokKind::RBracket)) break;  // trailing comma
        }
    }
    if (!match(TokKind::RBracket)) {
        m_error = QStringLiteral("expected ']' in array literal");
        return std::make_unique<InvalidExpr>(m_error);
    }
    return std::make_unique<ArrayExpr>(std::move(elems));
}

ExprPtr Parser::parsePrefix()
{
    const int pbp = prefixBp(peek().kind);
    if (pbp > 0) {
        const TokKind op = peek().kind;
        advance();
        auto rhs = parseExpression(pbp);
        if (op == TokKind::Bang)
            return std::make_unique<UnaryExpr>(UnOp::Not, std::move(rhs));
        // Minus -- try constant-fold numeric literal per addendum §4.4.
        return foldUnaryNegate(std::move(rhs));
    }
    return parsePostfixChain(parsePrimary());
}

ExprPtr Parser::parsePostfixChain(ExprPtr lhs)
{
    for (;;) {
        const Token t = peek();
        if (t.kind == TokKind::LParen) {
            advance();
            std::vector<ExprPtr> args;
            if (!check(TokKind::RParen)) {
                for (;;) {
                    args.push_back(parseExpression(0));
                    if (!match(TokKind::Comma)) break;
                    if (check(TokKind::RParen)) break;  // trailing comma
                }
            }
            if (!match(TokKind::RParen)) {
                m_error = QStringLiteral("expected ')' after call arguments");
                return std::make_unique<InvalidExpr>(m_error);
            }
            lhs = std::make_unique<CallExpr>(std::move(lhs), std::move(args));
        } else if (t.kind == TokKind::LBracket) {
            advance();
            auto idx = parseExpression(0);
            if (!match(TokKind::RBracket)) {
                m_error = QStringLiteral("expected ']' after index");
                return std::make_unique<InvalidExpr>(m_error);
            }
            lhs = std::make_unique<IndexExpr>(std::move(lhs), std::move(idx));
        } else if (t.kind == TokKind::Dot) {
            advance();
            const Token id = advance();
            if (id.kind != TokKind::Identifier) {
                m_error = QStringLiteral("expected identifier after '.'");
                return std::make_unique<InvalidExpr>(m_error);
            }
            lhs = std::make_unique<MemberExpr>(std::move(lhs), id.textValue);
        } else {
            break;
        }
    }
    return lhs;
}

ExprPtr Parser::parseExpression(int minBp)
{
    auto lhs = parsePrefix();
    for (;;) {
        const Token t = peek();
        const InfixBp bp = infixBp(t.kind);
        if (bp.left == 0 || bp.left < minBp) break;
        advance();
        auto rhs = parseExpression(bp.right);
        BinOp op;
        switch (t.kind) {
        case TokKind::OrOr:   op = BinOp::OrOr;   break;
        case TokKind::AndAnd: op = BinOp::AndAnd; break;
        case TokKind::EqEq:   op = BinOp::Eq;     break;
        case TokKind::BangEq: op = BinOp::Neq;    break;
        case TokKind::Lt:     op = BinOp::Lt;     break;
        case TokKind::Gt:     op = BinOp::Gt;     break;
        case TokKind::LtEq:   op = BinOp::LtEq;   break;
        case TokKind::GtEq:   op = BinOp::GtEq;   break;
        case TokKind::Plus:   op = BinOp::Add;    break;
        case TokKind::Minus:  op = BinOp::Sub;    break;
        case TokKind::Star:   op = BinOp::Mul;    break;
        case TokKind::Slash:  op = BinOp::Div;    break;
        case TokKind::Percent:op = BinOp::Mod;    break;
        default:
            m_error = QStringLiteral("unhandled binary operator");
            return std::make_unique<InvalidExpr>(m_error);
        }
        lhs = std::make_unique<BinaryExpr>(op, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

ExprPtr Parser::parseProgram()
{
    if (check(TokKind::End) || m_toks.isEmpty())
        return std::make_unique<EmptyExpr>();
    auto root = parseExpression(0);
    // Trailing garbage -> Invalid.
    if (!check(TokKind::End)) {
        if (m_error.isEmpty())
            m_error = QStringLiteral("unexpected trailing input");
        return std::make_unique<InvalidExpr>(m_error);
    }
    // If we recovered from any parse error, surface the top-level as
    // Invalid so the caller doesn't act on a malformed AST (addendum §1
    // "RK invalid sentinel" behaviour).
    if (!m_error.isEmpty())
        return std::make_unique<InvalidExpr>(m_error);
    return root;
}

ExprPtr Parser::parse(const QString &source, QString *errorOut)
{
    Lexer lx(source);
    Parser p(lx.tokenize());
    auto root = p.parseProgram();
    if (errorOut) *errorOut = p.m_error;
    return root;
}

}  // namespace Corbomite::Bases
