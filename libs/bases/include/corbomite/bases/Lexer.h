// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVector>

namespace Corbomite::Bases {

enum class TokKind
{
    // Literals
    Null, True, False, Number, String, Regex, Identifier,
    // Grouping / postfix
    LParen, RParen, LBracket, RBracket, Comma, Dot,
    // Binary
    OrOr, AndAnd,
    EqEq, BangEq,
    Lt, Gt, LtEq, GtEq,
    Plus, Minus, Star, Slash, Percent,
    // Unary
    Bang,
    // Terminator
    End,
    // Error recovery
    Invalid,
};

struct Token
{
    TokKind kind = TokKind::End;
    int start = 0;    ///< Offset into source.
    int length = 0;

    // Literal payload.
    double numberValue = 0.0;
    QString textValue;    ///< Identifier text / string body / regex pattern.
    QString regexFlags;   ///< Present only when kind == Regex.
    QString errorMessage; ///< Present only when kind == Invalid.
};

class Lexer
{
public:
    explicit Lexer(QString src);

    QVector<Token> tokenize();

    /// Decides whether a `/` at `prev`'s position starts a regex literal
    /// vs a division. After binary/unary/`(`/`,`/keywords -> regex; after
    /// identifier/number/`)`/`]`/literal -> division.
    static bool isRegexAllowedAfter(TokKind prev);

    /// Addendum section7: rewrite a single-quoted string's inner content into
    /// the equivalent double-quoted inner, so JSON-escape semantics apply.
    static QString applySingleQuoteEscape(const QString &inner);

private:
    Token nextToken();
    Token lexNumber();
    Token lexString(QChar quote);
    Token lexRegex();
    Token lexIdentifier();
    void skipWhitespace();
    bool atEnd() const { return m_pos >= m_src.size(); }
    QChar peek(int ahead = 0) const;

    Token invalid(const QString &msg, int at, int len = 1);

    QString m_src;
    int m_pos = 0;
    TokKind m_prevKind = TokKind::End;
};

}  // namespace Corbomite::Bases
