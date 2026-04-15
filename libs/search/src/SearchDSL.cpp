// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/search/SearchDSL.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace Corbomite::SearchDSL {

namespace {

// Operator table mirroring _internal.js:331191-331265. `exclusive` operators
// may not nest inside another exclusive operator (Operator "X" cannot be
// nested within "Y"). `allowSelf` carves out the section→section exception.
// `textOnly` rejects non-text operands (the tag-only-text rule).
struct OperatorSpec {
    bool exclusive = false;
    bool allowSelf = false;
    bool textOnly = false;
};

const QHash<QString, OperatorSpec> &operatorTable()
{
    static const QHash<QString, OperatorSpec> table = {
        {QStringLiteral("path"),       {true,  false, false}},
        {QStringLiteral("file"),       {true,  false, false}},
        {QStringLiteral("content"),    {true,  false, false}},
        {QStringLiteral("tag"),        {true,  false, true }},
        {QStringLiteral("line"),       {true,  false, false}},
        {QStringLiteral("block"),      {true,  false, false}},
        {QStringLiteral("section"),    {true,  true,  false}},
        {QStringLiteral("task"),       {true,  false, false}},
        {QStringLiteral("task-todo"),  {true,  false, false}},
        {QStringLiteral("task-done"),  {true,  false, false}},
        {QStringLiteral("match-case"), {false, false, false}},
        {QStringLiteral("ignore-case"),{false, false, false}},
    };
    return table;
}

enum class Tok {
    End,
    Text,
    Quote,
    Regex,
    Not,        // '-'
    BracketOpen,
    BracketClose,
    ParenOpen,
    ParenClose,
    Colon,
    LessThan,
    GreaterThan,
    Or,
    True,
    False,
    Empty,
};

struct Token {
    Tok kind = Tok::End;
    QString value;
    int offset = 0;
};

bool isDelimiter(QChar c)
{
    if (c.isSpace()) return true;
    switch (c.unicode()) {
    case '"': case '/': case '-': case '[': case ']':
    case '(': case ')': case ':': case '<': case '>':
        return true;
    default:
        return false;
    }
}

class Tokenizer {
public:
    explicit Tokenizer(const QString &input) : m_input(input) {}

    QVector<Token> tokenize()
    {
        QVector<Token> out;
        while (m_pos < m_input.length()) {
            QChar c = m_input.at(m_pos);
            if (c.isSpace()) {
                ++m_pos;
                continue;
            }
            const int start = m_pos;
            switch (c.unicode()) {
            case '"': out.append(readQuote(start)); break;
            case '/': out.append(readRegex(start)); break;
            case '-': out.append({Tok::Not, QStringLiteral("-"), start}); ++m_pos; break;
            case '[': out.append({Tok::BracketOpen, QStringLiteral("["), start}); ++m_pos; break;
            case ']': out.append({Tok::BracketClose, QStringLiteral("]"), start}); ++m_pos; break;
            case '(': out.append({Tok::ParenOpen, QStringLiteral("("), start}); ++m_pos; break;
            case ')': out.append({Tok::ParenClose, QStringLiteral(")"), start}); ++m_pos; break;
            case ':': out.append({Tok::Colon, QStringLiteral(":"), start}); ++m_pos; break;
            case '<': out.append({Tok::LessThan, QStringLiteral("<"), start}); ++m_pos; break;
            case '>': out.append({Tok::GreaterThan, QStringLiteral(">"), start}); ++m_pos; break;
            default:  out.append(readText(start)); break;
            }
        }
        out.append({Tok::End, QString(), m_pos});
        return out;
    }

private:
    Token readQuote(int start)
    {
        ++m_pos;  // consume opening "
        QString body;
        while (m_pos < m_input.length()) {
            QChar c = m_input.at(m_pos);
            if (c == QLatin1Char('\\') && m_pos + 1 < m_input.length()) {
                body.append(m_input.at(m_pos + 1));
                m_pos += 2;
                continue;
            }
            if (c == QLatin1Char('"')) {
                ++m_pos;
                return {Tok::Quote, body, start};
            }
            body.append(c);
            ++m_pos;
        }
        // Unterminated — Obsidian accepts at EOF (search-dsl-spec.md §4).
        return {Tok::Quote, body, start};
    }

    Token readRegex(int start)
    {
        ++m_pos;  // consume opening /
        QString body;
        while (m_pos < m_input.length()) {
            QChar c = m_input.at(m_pos);
            if (c == QLatin1Char('\\') && m_pos + 1 < m_input.length()) {
                QChar next = m_input.at(m_pos + 1);
                if (next == QLatin1Char('/')) {
                    body.append(QLatin1Char('/'));
                } else {
                    body.append(c);
                    body.append(next);
                }
                m_pos += 2;
                continue;
            }
            if (c == QLatin1Char('/')) {
                ++m_pos;
                return {Tok::Regex, body, start};
            }
            body.append(c);
            ++m_pos;
        }
        return {Tok::Regex, body, start};
    }

    Token readText(int start)
    {
        QString body;
        while (m_pos < m_input.length()) {
            const QChar c = m_input.at(m_pos);
            // Internal hyphen between two text chars stays part of the token —
            // covers compound operator names (match-case, ignore-case,
            // task-todo, task-done) that would otherwise split into
            // text-not-text. A leading or trailing `-` (next to whitespace or
            // a delimiter) still tokenizes as Not, preserving `foo -bar`.
            if (c == QLatin1Char('-') && !body.isEmpty()
                && m_pos + 1 < m_input.length()
                && !isDelimiter(m_input.at(m_pos + 1))) {
                body.append(c);
                ++m_pos;
                continue;
            }
            if (isDelimiter(c)) break;
            body.append(c);
            ++m_pos;
        }
        // Case-sensitive barewords (search-dsl-spec.md §1 tokenizer table).
        if (body == QLatin1String("OR"))    return {Tok::Or, body, start};
        if (body == QLatin1String("TRUE"))  return {Tok::True, body, start};
        if (body == QLatin1String("FALSE")) return {Tok::False, body, start};
        if (body == QLatin1String("EMPTY")) return {Tok::Empty, body, start};
        return {Tok::Text, body, start};
    }

    QString m_input;
    int m_pos = 0;
};

class Parser {
public:
    Parser(QVector<Token> tokens) : m_tokens(std::move(tokens)) {}

    ParseResult run()
    {
        ParseResult r;
        SearchNodePtr root = parseOr();
        if (!m_error.isEmpty()) {
            r.error = m_error;
            r.errorOffset = m_errorOffset;
            return r;
        }
        r.root = root;
        return r;
    }

private:
    const Token &peek(int look = 0) const { return m_tokens.at(m_pos + look); }
    const Token &consume() { return m_tokens.at(m_pos++); }

    void fail(const QString &msg, int offset)
    {
        if (!m_error.isEmpty()) return;  // first error wins
        m_error = msg;
        m_errorOffset = offset;
    }

    bool isTerminator(Tok t) const
    {
        return t == Tok::End || t == Tok::ParenClose || t == Tok::BracketClose;
    }

    SearchNodePtr parseOr()
    {
        QVector<SearchNodePtr> branches;
        SearchNodePtr first = parseAnd();
        if (!first) return nullptr;
        branches.append(first);
        while (peek().kind == Tok::Or) {
            consume();
            SearchNodePtr next = parseAnd();
            if (!next) break;  // trailing OR silently discarded (spec §4)
            branches.append(next);
        }
        if (branches.size() == 1) return branches.first();
        return SearchNode::makeOr(std::move(branches));
    }

    SearchNodePtr parseAnd()
    {
        QVector<SearchNodePtr> conjuncts;
        while (true) {
            const Tok t = peek().kind;
            if (isTerminator(t) || t == Tok::Or) break;
            const int before = m_pos;
            SearchNodePtr term = parseTerm();
            if (!term) {
                // parseTerm may return null but still advance (e.g. property
                // stub silently consuming `[…]`). Treat as a no-op term and
                // keep scanning for siblings; only stop when nothing was
                // consumed (genuine end of input).
                if (m_pos == before) break;
                continue;
            }
            conjuncts.append(term);
        }
        if (conjuncts.isEmpty()) return nullptr;
        if (conjuncts.size() == 1) return conjuncts.first();
        return SearchNode::makeAnd(std::move(conjuncts));
    }

    SearchNodePtr parseTerm()
    {
        if (peek().kind == Tok::Not) {
            const int off = consume().offset;
            SearchNodePtr inner = parseTerm();
            if (!inner) {
                fail(QStringLiteral("Dangling '-' with no operand"), off);
                return nullptr;
            }
            return SearchNode::makeNot(inner);
        }
        return parsePrimary();
    }

    SearchNodePtr parsePrimary()
    {
        const Token &t = peek();
        switch (t.kind) {
        case Tok::ParenOpen: {
            consume();
            SearchNodePtr inner = parseOr();
            if (peek().kind == Tok::ParenClose) consume();  // missing ) silently swallowed
            return inner ? SearchNode::makeGroup(inner) : nullptr;
        }
        case Tok::Quote:
            return SearchNode::makePhrase(consume().value);
        case Tok::Regex: {
            const QString pattern = t.value;
            const int off = t.offset;
            consume();
            QRegularExpression rx(pattern);
            if (!rx.isValid()) {
                fail(QStringLiteral("Failed to parse regular expression: %1").arg(pattern), off);
                return nullptr;
            }
            return SearchNode::makeRegex(pattern);
        }
        case Tok::Text:
            return parseTextOrOpCall();
        case Tok::BracketOpen:
            // Property syntax — parsed-but-rejected for now (Phase 4b/Cluster I).
            // Skip until matching ] so the parser can keep moving.
            return parsePropertyStub();
        default:
            return nullptr;
        }
    }

    SearchNodePtr parseTextOrOpCall()
    {
        const Token nameTok = consume();
        if (peek().kind != Tok::Colon) {
            return SearchNode::makeText(nameTok.value);
        }
        // operator-call: name : term
        const auto &table = operatorTable();
        if (!table.contains(nameTok.value)) {
            fail(QStringLiteral("Operator \"%1\" not recognized").arg(nameTok.value),
                 nameTok.offset);
            return nullptr;
        }
        const OperatorSpec spec = table.value(nameTok.value);

        // Exclusive-nesting check (spec §1 RH table).
        if (spec.exclusive) {
            for (const QString &outer : m_exclusiveStack) {
                const bool selfPair = (outer == nameTok.value && spec.allowSelf);
                if (!selfPair) {
                    fail(QStringLiteral("Operator \"%1\" cannot be nested within \"%2\"")
                             .arg(nameTok.value, outer),
                         nameTok.offset);
                    return nullptr;
                }
            }
        }

        consume();  // ':'
        if (spec.exclusive) m_exclusiveStack.append(nameTok.value);
        SearchNodePtr operand = parseTerm();
        if (spec.exclusive) m_exclusiveStack.removeLast();

        if (!operand) {
            // Trailing colon: substitute empty text matcher (spec §4) — that
            // matcher matches nothing, so the whole branch will be no-op-empty.
            operand = SearchNode::makeText(QString());
        }

        if (spec.textOnly) {
            // Unwrap Group to inspect the inner kind (per spec, tag rejects
            // non-text operands at parse).
            SearchNodePtr toCheck = operand;
            while (toCheck && toCheck->kind == SearchNode::Kind::Group) {
                toCheck = toCheck->children.value(0);
            }
            if (!toCheck || toCheck->kind != SearchNode::Kind::Text) {
                fail(QStringLiteral("Operator \"%1\" can only be followed by text")
                         .arg(nameTok.value),
                     nameTok.offset);
                return nullptr;
            }
        }

        return SearchNode::makeOpCall(nameTok.value, operand);
    }

    SearchNodePtr parsePropertyStub()
    {
        // Skip [...] body — depth-balanced. Returns null so the caller treats
        // this term as absent. Property-call is a Phase 4b deliverable.
        Q_ASSERT(peek().kind == Tok::BracketOpen);
        consume();
        int depth = 1;
        while (depth > 0 && peek().kind != Tok::End) {
            const Tok k = consume().kind;
            if (k == Tok::BracketOpen) ++depth;
            else if (k == Tok::BracketClose) --depth;
        }
        return nullptr;
    }

    QVector<Token> m_tokens;
    int m_pos = 0;
    QStringList m_exclusiveStack;
    QString m_error;
    int m_errorOffset = -1;
};

} // namespace

ParseResult parse(const QString &query)
{
    Tokenizer tk(query);
    auto tokens = tk.tokenize();
    Parser p(std::move(tokens));
    return p.run();
}

const QStringList &supportedOperators()
{
    static const QStringList ops = {
        QStringLiteral("path"),
        QStringLiteral("file"),
        QStringLiteral("content"),
        QStringLiteral("tag"),
        QStringLiteral("match-case"),
        QStringLiteral("ignore-case"),
    };
    return ops;
}

} // namespace Corbomite::SearchDSL
