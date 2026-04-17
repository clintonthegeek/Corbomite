// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Ast.h"
#include "Lexer.h"

namespace Corbomite::Bases {

/// Pratt-style expression parser.
///
/// Each token kind maps to a (left, right) binding-power pair for infix
/// operators, and an integer prefix-binding-power for unary operators.
/// Higher bp = tighter binding.
///
/// Empty input produces an `EmptyExpr` root. Malformed input produces
/// an `InvalidExpr` root whose `message` describes the failure.
class Parser
{
public:
    explicit Parser(QVector<Token> tokens);

    ExprPtr parseProgram();

    /// One-shot convenience: lex + parse.
    static ExprPtr parse(const QString &source, QString *errorOut = nullptr);

private:
    ExprPtr parseExpression(int minBp);
    ExprPtr parsePrefix();
    ExprPtr parsePostfixChain(ExprPtr lhs);
    ExprPtr parsePrimary();
    ExprPtr parseArrayLiteral();

    struct InfixBp { int left; int right; };
    static InfixBp infixBp(TokKind k);
    static int prefixBp(TokKind k);  // 0 if not unary

    const Token &peek(int ahead = 0) const;
    const Token &advance();
    bool match(TokKind k);
    bool check(TokKind k) const { return peek().kind == k; }

    /// Try constant-folding `UnaryExpr(Negate, LiteralExpr(Number))`
    /// into a single LiteralExpr at build-time (addendum §4.4).
    static ExprPtr foldUnaryNegate(ExprPtr operand);

    QVector<Token> m_toks;
    int m_pos = 0;
    QString m_error;
};

}  // namespace Corbomite::Bases
