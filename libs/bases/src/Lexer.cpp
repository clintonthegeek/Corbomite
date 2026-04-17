// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Lexer.h"

#include <QChar>
#include <QJsonArray>
#include <QJsonDocument>

namespace Corbomite::Bases {

namespace {

bool isIdentStart(QChar c) { return c.isLetter() || c == QLatin1Char('_') || c == QLatin1Char('$'); }
bool isIdentCont(QChar c)  { return isIdentStart(c) || c.isDigit(); }

}  // namespace

Lexer::Lexer(QString src) : m_src(std::move(src)) {}

bool Lexer::isRegexAllowedAfter(TokKind prev)
{
    switch (prev) {
    case TokKind::Identifier:
    case TokKind::Number:
    case TokKind::String:
    case TokKind::Regex:
    case TokKind::RParen:
    case TokKind::RBracket:
        return false;
    default:
        return true;
    }
}

// Addendum §7: single-quoted string handling.
QString Lexer::applySingleQuoteEscape(const QString &inner)
{
    // 1) Replace `\'` with `'`.
    // 2) Escape naked `"` to `\"`, preserving already-escaped `\\"`.
    // Implemented by splitting on `\\"` so already-escaped sequences survive.
    QString step1;
    step1.reserve(inner.size());
    for (int i = 0; i < inner.size(); ++i) {
        if (inner[i] == QLatin1Char('\\') && i + 1 < inner.size()
            && inner[i + 1] == QLatin1Char('\'')) {
            step1.append(QLatin1Char('\''));
            ++i;
        } else {
            step1.append(inner[i]);
        }
    }
    // Split on `\\"` to protect already-escaped quote sequences.
    QStringList pieces = step1.split(QStringLiteral("\\\""));
    for (auto &p : pieces)
        p.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return pieces.join(QStringLiteral("\\\""));
}

QChar Lexer::peek(int ahead) const
{
    const int i = m_pos + ahead;
    return i < m_src.size() ? m_src[i] : QChar();
}

void Lexer::skipWhitespace()
{
    while (!atEnd() && m_src[m_pos].isSpace()) ++m_pos;
}

Token Lexer::invalid(const QString &msg, int at, int len)
{
    Token t;
    t.kind = TokKind::Invalid;
    t.start = at;
    t.length = len;
    t.errorMessage = msg;
    return t;
}

Token Lexer::lexNumber()
{
    const int start = m_pos;
    while (!atEnd() && m_src[m_pos].isDigit()) ++m_pos;
    if (!atEnd() && m_src[m_pos] == QLatin1Char('.')
        && m_pos + 1 < m_src.size() && m_src[m_pos + 1].isDigit()) {
        ++m_pos;
        while (!atEnd() && m_src[m_pos].isDigit()) ++m_pos;
    }
    Token t;
    t.kind = TokKind::Number;
    t.start = start;
    t.length = m_pos - start;
    bool ok = false;
    t.numberValue = m_src.mid(start, t.length).toDouble(&ok);
    if (!ok) return invalid(QStringLiteral("malformed number"), start, t.length);
    return t;
}

Token Lexer::lexString(QChar quote)
{
    const int start = m_pos;
    ++m_pos;  // consume opening quote
    while (!atEnd()) {
        if (m_src[m_pos] == QLatin1Char('\\') && m_pos + 1 < m_src.size()) {
            m_pos += 2;
            continue;
        }
        if (m_src[m_pos] == quote) break;
        ++m_pos;
    }
    if (atEnd())
        return invalid(QStringLiteral("unterminated string"), start, m_pos - start);
    const int end = m_pos;
    ++m_pos;  // consume closing quote

    const QString innerRaw = m_src.mid(start + 1, end - start - 1);
    QString jsonInner = (quote == QLatin1Char('\''))
        ? applySingleQuoteEscape(innerRaw)
        : innerRaw;

    // Leverage JSON escape handling: wrap in an array and parse.
    const QByteArray wrapped =
        (QLatin1String("[\"") + jsonInner + QLatin1String("\"]")).toUtf8();
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(wrapped, &err);
    Token t;
    t.kind = TokKind::String;
    t.start = start;
    t.length = m_pos - start;
    if (err.error == QJsonParseError::NoError && doc.isArray()
        && doc.array().size() == 1 && doc.array().at(0).isString()) {
        t.textValue = doc.array().at(0).toString();
    } else {
        // Fallback: use raw inner unchanged.
        t.textValue = innerRaw;
    }
    return t;
}

Token Lexer::lexRegex()
{
    const int start = m_pos;
    ++m_pos;  // consume opening `/`
    while (!atEnd() && m_src[m_pos] != QLatin1Char('/')) {
        if (m_src[m_pos] == QLatin1Char('\\') && m_pos + 1 < m_src.size())
            m_pos += 2;
        else
            ++m_pos;
    }
    if (atEnd())
        return invalid(QStringLiteral("unterminated regex"), start, m_pos - start);
    const int end = m_pos;
    ++m_pos;  // consume closing `/`
    // Flags: [gimsuy]*
    const int flagsStart = m_pos;
    while (!atEnd() && QString(QLatin1String("gimsuy")).contains(m_src[m_pos]))
        ++m_pos;
    Token t;
    t.kind = TokKind::Regex;
    t.start = start;
    t.length = m_pos - start;
    t.textValue = m_src.mid(start + 1, end - start - 1);
    t.regexFlags = m_src.mid(flagsStart, m_pos - flagsStart);
    return t;
}

Token Lexer::lexIdentifier()
{
    const int start = m_pos;
    while (!atEnd() && isIdentCont(m_src[m_pos])) ++m_pos;
    Token t;
    t.start = start;
    t.length = m_pos - start;
    const QString text = m_src.mid(start, t.length);
    if (text == QLatin1String("null"))  t.kind = TokKind::Null;
    else if (text == QLatin1String("true"))  t.kind = TokKind::True;
    else if (text == QLatin1String("false")) t.kind = TokKind::False;
    else {
        t.kind = TokKind::Identifier;
        t.textValue = text;
    }
    return t;
}

Token Lexer::nextToken()
{
    skipWhitespace();
    if (atEnd()) {
        Token t; t.kind = TokKind::End; t.start = m_pos; return t;
    }

    const int start = m_pos;
    const QChar c = m_src[m_pos];

    // Number literal.
    if (c.isDigit()) return lexNumber();

    // Identifier / keyword.
    if (isIdentStart(c)) return lexIdentifier();

    // String literal.
    if (c == QLatin1Char('"') || c == QLatin1Char('\''))
        return lexString(c);

    // Single-char punctuation + multi-char operators.
    Token t;
    t.start = start;
    t.length = 1;
    switch (c.unicode()) {
    case '(': t.kind = TokKind::LParen;   ++m_pos; return t;
    case ')': t.kind = TokKind::RParen;   ++m_pos; return t;
    case '[': t.kind = TokKind::LBracket; ++m_pos; return t;
    case ']': t.kind = TokKind::RBracket; ++m_pos; return t;
    case ',': t.kind = TokKind::Comma;    ++m_pos; return t;
    case '.': t.kind = TokKind::Dot;      ++m_pos; return t;
    case '+': t.kind = TokKind::Plus;     ++m_pos; return t;
    case '-': t.kind = TokKind::Minus;    ++m_pos; return t;
    case '*': t.kind = TokKind::Star;     ++m_pos; return t;
    case '%': t.kind = TokKind::Percent;  ++m_pos; return t;
    case '|':
        if (peek(1) == QLatin1Char('|')) {
            t.kind = TokKind::OrOr; t.length = 2; m_pos += 2; return t;
        }
        break;
    case '&':
        if (peek(1) == QLatin1Char('&')) {
            t.kind = TokKind::AndAnd; t.length = 2; m_pos += 2; return t;
        }
        break;
    case '=':
        if (peek(1) == QLatin1Char('=')) {
            t.kind = TokKind::EqEq; t.length = 2; m_pos += 2; return t;
        }
        break;
    case '!':
        if (peek(1) == QLatin1Char('=')) {
            t.kind = TokKind::BangEq; t.length = 2; m_pos += 2; return t;
        }
        t.kind = TokKind::Bang; ++m_pos; return t;
    case '<':
        if (peek(1) == QLatin1Char('=')) {
            t.kind = TokKind::LtEq; t.length = 2; m_pos += 2; return t;
        }
        t.kind = TokKind::Lt; ++m_pos; return t;
    case '>':
        if (peek(1) == QLatin1Char('=')) {
            t.kind = TokKind::GtEq; t.length = 2; m_pos += 2; return t;
        }
        t.kind = TokKind::Gt; ++m_pos; return t;
    case '/':
        if (isRegexAllowedAfter(m_prevKind))
            return lexRegex();
        t.kind = TokKind::Slash; ++m_pos; return t;
    }
    // Unknown character.
    ++m_pos;
    return invalid(QStringLiteral("unexpected character"), start, 1);
}

QVector<Token> Lexer::tokenize()
{
    QVector<Token> out;
    for (;;) {
        Token t = nextToken();
        out.append(t);
        m_prevKind = t.kind;
        if (t.kind == TokKind::End || t.kind == TokKind::Invalid) break;
    }
    return out;
}

}  // namespace Corbomite::Bases
