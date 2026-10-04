// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/search/SearchDSL.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace Corbomite::SearchDSL {

namespace {

// Operator table, mirroring app/own/1175-SP.js:148-221 verbatim — twelve entries,
// no more. `whole-word` is deliberately absent: the corpus contains that string
// only as an icon name for the editor's find/replace toggle
// (app/own/0713-gg.js:11337), never as a DSL operator. Whole-word *semantics* do
// exist, but they are what a quoted phrase does (app/own/1166-ZA.js:90).
//
// Names are matched case-insensitively (app/own/1170-hP.js:9). `exclusive`
// operators may not nest inside another exclusive operator ("Operator \"X\" cannot
// be nested within \"Y\""). `allowSelf` carves out the section→section exception,
// and it is the OUTER operator's flag that decides, so only `section:` self-nests.
// `textOnly` rejects non-text operands (the tag-only-text rule).
struct OperatorSpec {
    bool exclusive = false;
    bool allowSelf = false;
    bool textOnly = false;
};

const QHash<QString, OperatorSpec> &operatorTable()
{
    static const QHash<QString, OperatorSpec> table = {
        {QStringLiteral("match-case"), {false, false, false}},
        {QStringLiteral("ignore-case"),{false, false, false}},
        {QStringLiteral("path"),       {true,  false, false}},
        {QStringLiteral("file"),       {true,  false, false}},
        {QStringLiteral("content"),    {true,  false, false}},
        {QStringLiteral("line"),       {true,  false, false}},
        {QStringLiteral("block"),      {true,  false, false}},
        {QStringLiteral("section"),    {true,  true,  false}},
        {QStringLiteral("task"),       {true,  false, false}},
        {QStringLiteral("task-todo"),  {true,  false, false}},
        {QStringLiteral("task-done"),  {true,  false, false}},
        {QStringLiteral("tag"),        {true,  false, true }},
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

// The characters that flush an accumulating text token, per
// app/own/1173-fP.js:36-79. `"`, `/` and `-` are absent: the tokenizer handles
// them only at a token *start*, where the outer switch turns them into `quote`,
// `regex` and `not` tokens, so mid-token they are ordinary characters. Whitespace
// is handled by `isSpace()` rather than Obsidian's literal-space test.
bool isDelimiter(QChar c)
{
    if (c.isSpace()) return true;
    switch (c.unicode()) {
    case '[': case ']': case '(': case ')':
    case ':': case '<': case '>':
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
            if (isDelimiter(c)) break;
            body.append(c);
            ++m_pos;
        }
        // `-` is NOT a delimiter here: app/own/1173-fP.js:29 only emits a `not`
        // token when the scan *starts* on a hyphen, so a hyphen inside an
        // accumulating token is ordinary. That is what keeps `task-todo:` a
        // single `text` token, and it also makes a trailing hyphen part of the
        // word: `foo-` is text("foo-"), not text("foo") followed by `not`.
        //
        // Case-sensitive barewords (app/own/1173-fP.js:4-24) — `or`, `True` and
        // `empty` are ordinary text.
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
        // `uP` — app/own/1169-uP.js:2-9: `hP` results back-to-back, stopping at
        // the first one that returns null (a terminator token, or a `-`/`>`/`<`
        // with no operand). A null is never skipped over.
        while (true) {
            const Tok t = peek().kind;
            if (isTerminator(t) || t == Tok::Or) break;
            SearchNodePtr term = parseTerm();
            if (!term) break;
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
            return parseProperty();
        case Tok::True:
        case Tok::False:
        case Tok::Empty:
            // `hP.js:68` → `_A`. These compare a frontmatter property value, so
            // they only ever do something useful inside `[key: …]`.
            return SearchNode::makePropertyValue(consume().value);
        case Tok::GreaterThan:
        case Tok::LessThan: {
            // `hP.js:71` → `aP`, which accepts a text or quoted operand only.
            const Token op = consume();
            SearchNodePtr operand = parseTerm();
            if (!operand) return nullptr;
            if (operand->kind != SearchNode::Kind::Text
                && operand->kind != SearchNode::Kind::Phrase) {
                fail(QStringLiteral("\"%1\" operator cannot be applied to this matcher")
                         .arg(op.value),
                     op.offset);
                return nullptr;
            }
            return SearchNode::makeCompare(op.value, operand);
        }
        default:
            // Terminators: `]`, `)`, `or`, `colon`, end-of-input (hP.js:41,66,67,69).
            return nullptr;
        }
    }

    SearchNodePtr parseTextOrOpCall()
    {
        const Token nameTok = consume();
        const QString opName = nameTok.value.toLower();  // hP.js:9
        // hP.js:7 — a `[` anywhere on the stack disables operator recognition, so a
        // colon inside a property body is punctuation, not an operator call.
        if (peek().kind != Tok::Colon || m_operatorStack.contains(QLatin1Char('['))) {
            return SearchNode::makeText(nameTok.value);
        }
        // operator-call: name : term
        const auto &table = operatorTable();
        if (!table.contains(opName)) {
            fail(QStringLiteral("Operator \"%1\" not recognized").arg(nameTok.value),
                 nameTok.offset);
            return nullptr;
        }
        const OperatorSpec spec = table.value(opName);

        // Exclusive-nesting check (hP.js:12-17). The OUTER operator's allowSelf
        // decides, which is why only section→section self-nests.
        if (spec.exclusive) {
            for (const QString &outer : m_operatorStack) {
                const bool selfPair = (outer == opName && spec.allowSelf);
                if (!selfPair) {
                    fail(QStringLiteral("Operator \"%1\" cannot be nested within \"%2\"")
                             .arg(opName, outer),
                         nameTok.offset);
                    return nullptr;
                }
            }
        }

        consume();  // ':'
        if (spec.exclusive) m_operatorStack.append(opName);
        SearchNodePtr operand = parseTerm();
        if (spec.exclusive) m_operatorStack.removeLast();

        if (!operand) {
            // Trailing colon: substitute empty text matcher (hP.js:22) — that
            // matcher matches nothing, so the whole branch will be no-op-empty.
            operand = SearchNode::makeText(QString());
        }

        if (spec.textOnly) {
            // Unwrap Group to inspect the inner kind — a `(` returns its inner
            // node, so `tag:(foo)` is still text (hP.js:52-64).
            SearchNodePtr toCheck = operand;
            while (toCheck && toCheck->kind == SearchNode::Kind::Group) {
                toCheck = toCheck->children.value(0);
            }
            if (!toCheck || toCheck->kind != SearchNode::Kind::Text) {
                fail(QStringLiteral("Operator \"%1\" can only be followed by text")
                         .arg(opName),
                     nameTok.offset);
                return nullptr;
            }
        }

        return SearchNode::makeOpCall(opName, operand);
    }

    SearchNodePtr parseProperty()
    {
        // `[key]` / `[key: value]` — hP.js:24-39 → `JA`. Both halves are full
        // `cP` lists, so `["a" OR "b"]` and `[a: b OR c]` parse.
        if (m_operatorStack.contains(QLatin1Char('['))) {
            fail(QStringLiteral("Property cannot be nested within a property."),
                 peek().offset);
            return nullptr;
        }
        consume();  // '['
        m_operatorStack.append(QStringLiteral("["));

        SearchNodePtr key = parseOr();
        if (!key) key = SearchNode::makeText(QString());  // hP.js:29

        SearchNodePtr value;
        if (peek().kind == Tok::Colon) {
            consume();
            value = parseOr();  // hP.js:34 — a null value is an existence check
        }

        m_operatorStack.removeLast();
        if (peek().kind == Tok::BracketClose) consume();  // hP.js:39, optional
        return SearchNode::makeProperty(std::move(key), std::move(value));
    }

    QVector<Token> m_tokens;
    int m_pos = 0;
    QStringList m_operatorStack;
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
    // Derived from the parser's own table so the two cannot drift. Sorted because
    // QHash has no order and this feeds a UI list.
    static const QStringList ops = [] {
        QStringList out;
        for (auto it = operatorTable().cbegin(); it != operatorTable().cend(); ++it)
            out.append(it.key());
        out.sort();
        return out;
    }();
    return ops;
}

namespace {

// Quote a token for FTS5 MATCH so punctuation/spaces don't blow up the parser.
// FTS5 doubles internal quotes; we keep this simple and just wrap in "".
QString fts5Quote(const QString &term)
{
    QString escaped = term;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

// Walk the AST emitting an FTS5 MATCH fragment. Tag/regex/etc. side-effect
// onto the CompiledPlan (tag lists, unsupported-list). Returns the FTS5 string
// for this subtree, or empty if no FTS5-expressible portion exists.
QString emitFts5(const SearchNodePtr &node, CompiledPlan &plan, bool negated);

// Collect every Text/Phrase literal in a subtree (skipping negated branches).
// Used by match-case to find the terms that must be re-checked with
// Qt::CaseSensitive after FTS5 returns case-folded candidates.
void collectLiteralTerms(const SearchNodePtr &node, QStringList &out)
{
    if (!node) return;
    switch (node->kind) {
    case SearchNode::Kind::Text:
    case SearchNode::Kind::Phrase:
        if (!node->text.isEmpty()) out.append(node->text);
        return;
    case SearchNode::Kind::Not:
        return;  // don't elevate excluded terms into a required case filter
    case SearchNode::Kind::OpCall:
        // Recurse into column-qualifiers and nested match-case/ignore-case.
        if (!node->children.isEmpty())
            collectLiteralTerms(node->children.value(0), out);
        return;
    case SearchNode::Kind::PropertyCall:
    case SearchNode::Kind::PropertyValue:
    case SearchNode::Kind::Compare:
        // A property key is matched by case-insensitive equality, and
        // Compare/PropertyValue do a numeric or identity test — none of these
        // literals are content terms, so none belong in a content case filter.
        return;
    default:
        for (const auto &c : node->children) collectLiteralTerms(c, out);
        return;
    }
}

QString emitOpCall(const SearchNodePtr &node, CompiledPlan &plan, bool negated)
{
    const QString name = node->text;
    const SearchNodePtr operand = node->children.value(0);
    if (!operand) return {};

    if (name == QLatin1String("path") || name == QLatin1String("content")) {
        // Column-qualified FTS5 fragment over our (path, title, content) schema.
        if (operand->kind == SearchNode::Kind::Text || operand->kind == SearchNode::Kind::Phrase) {
            if (operand->text.isEmpty()) return {};
            return name + QLatin1Char(':') + fts5Quote(operand->text);
        }
        plan.unsupported.append(name + QLatin1String(": (non-text operand)"));
        return {};
    }
    if (name == QLatin1String("file")) {
        // file: maps onto our `title` column.
        if (operand->kind == SearchNode::Kind::Text || operand->kind == SearchNode::Kind::Phrase) {
            if (operand->text.isEmpty()) return {};
            return QStringLiteral("title:") + fts5Quote(operand->text);
        }
        plan.unsupported.append(QStringLiteral("file: (non-text operand)"));
        return {};
    }
    if (name == QLatin1String("tag")) {
        QString tag = operand->text;
        if (tag.startsWith(QLatin1Char('#'))) tag.remove(0, 1);
        if (tag.isEmpty()) return {};
        if (negated) plan.excludedTags.append(tag);
        else plan.requiredTags.append(tag);
        return {};
    }
    if (name == QLatin1String("ignore-case")) {
        return emitFts5(operand, plan, negated);
    }
    // match-case: FTS5's default tokeniser is case-folding, so we emit the
    // inner subtree normally (gives us candidate paths) and record the literal
    // terms; the caller re-checks each candidate's content with
    // Qt::CaseSensitive (SQLiteIndex::searchCompiled).
    if (name == QLatin1String("match-case")) {
        collectLiteralTerms(operand, plan.caseSensitiveTerms);
        return emitFts5(operand, plan, negated);
    }
    // line/block/section/task* — recognised by the parser but need a
    // markdown-AST post-filter. The property forms need a frontmatter side
    // table: `[key]` / `[key: value]` match against frontmatter, and TRUE /
    // FALSE / EMPTY / `>` / `<` only ever compare a frontmatter property value
    // (app/own/1163-UA.js:21-28, app/own/1166-ZA.js:190-206).
    plan.unsupported.append(name);
    return {};
}

QString emitFts5(const SearchNodePtr &node, CompiledPlan &plan, bool negated)
{
    if (!node) return {};
    switch (node->kind) {
    case SearchNode::Kind::Text:
        return node->text.isEmpty() ? QString() : fts5Quote(node->text);
    case SearchNode::Kind::Phrase:
        return node->text.isEmpty() ? QString() : fts5Quote(node->text);
    case SearchNode::Kind::Regex:
        // Can't express in FTS5 MATCH; caller applies as a content post-filter.
        if (!node->text.isEmpty())
            plan.regexPatterns.append(node->text);
        return {};
    case SearchNode::Kind::Group:
        return emitFts5(node->children.value(0), plan, negated);
    case SearchNode::Kind::PropertyCall:
        plan.unsupported.append(node->children.size() > 1 ? QStringLiteral("[key: value]")
                                                         : QStringLiteral("[key]"));
        // Descend so a nested form (`[rating: >4]`, `["a" OR "b"]`) reports too.
        for (const auto &c : node->children) emitFts5(c, plan, negated);
        return {};
    case SearchNode::Kind::PropertyValue:
        plan.unsupported.append(node->text);
        return {};
    case SearchNode::Kind::Compare:
        plan.unsupported.append(node->text);
        return {};
    case SearchNode::Kind::Not: {
        // FTS5 NOT is binary (X NOT Y); a top-level bare NOT can't be expressed
        // in MATCH alone. We translate -X as NOT-suffix only when there's a
        // sibling AND term to attach to (handled in And below). At the root,
        // surface as unsupported.
        QString inner = emitFts5(node->children.value(0), plan, !negated);
        if (inner.isEmpty()) return {};
        return QStringLiteral("NOT ") + inner;
    }
    case SearchNode::Kind::And: {
        QStringList parts;
        QStringList notParts;
        for (const auto &child : node->children) {
            if (child->kind == SearchNode::Kind::Not) {
                QString inner = emitFts5(child->children.value(0), plan, !negated);
                if (!inner.isEmpty()) notParts.append(inner);
                continue;
            }
            QString s = emitFts5(child, plan, negated);
            if (!s.isEmpty()) parts.append(s);
        }
        QString joined = parts.join(QStringLiteral(" AND "));
        if (joined.isEmpty() && !notParts.isEmpty()) {
            // No positive sibling — emit a single top-level negation; the
            // outer compile() relocates this into excludedFts5Query, since
            // FTS5 MATCH does not accept a bare leading NOT.
            joined = notParts.size() == 1
                ? QStringLiteral("NOT ") + notParts.first()
                : QStringLiteral("NOT (") + notParts.join(QStringLiteral(" OR "))
                      + QLatin1Char(')');
        } else {
            for (const QString &n : notParts) joined += QStringLiteral(" NOT ") + n;
        }
        return joined;
    }
    case SearchNode::Kind::Or: {
        QStringList parts;
        for (const auto &child : node->children) {
            QString s = emitFts5(child, plan, negated);
            if (!s.isEmpty()) parts.append(s);
        }
        if (parts.isEmpty()) return {};
        return QLatin1Char('(') + parts.join(QStringLiteral(" OR ")) + QLatin1Char(')');
    }
    case SearchNode::Kind::OpCall:
        return emitOpCall(node, plan, negated);
    }
    return {};
}

} // namespace

CompiledPlan compile(const SearchNodePtr &root)
{
    CompiledPlan plan;
    if (!root) return plan;
    plan.fts5Query = emitFts5(root, plan, /*negated=*/false);
    // FTS5 MATCH cannot start with NOT — the binary "X NOT Y" form is fine
    // (handled inside emitFts5's And case) but a top-level "NOT X" produced
    // by parsing `-foo` or `-foo -bar` must be relocated to a side channel
    // the executor applies as a NOT IN sub-query.
    if (plan.fts5Query.startsWith(QStringLiteral("NOT "))) {
        plan.excludedFts5Query = plan.fts5Query.mid(4);
        plan.fts5Query.clear();
    }
    return plan;
}

} // namespace Corbomite::SearchDSL
