// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/bases/Lexer.h"

using namespace Corbomite::Bases;

class TestLexer : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void testEmpty()
    {
        Lexer lx(QString{});
        auto toks = lx.tokenize();
        QCOMPARE(toks.size(), 1);
        QCOMPARE(toks[0].kind, TokKind::End);
    }

    void testKeywords()
    {
        Lexer lx(QStringLiteral("null true false"));
        auto toks = lx.tokenize();
        QCOMPARE(toks.size(), 4);
        QCOMPARE(toks[0].kind, TokKind::Null);
        QCOMPARE(toks[1].kind, TokKind::True);
        QCOMPARE(toks[2].kind, TokKind::False);
        QCOMPARE(toks[3].kind, TokKind::End);
    }

    void testIdentifier()
    {
        Lexer lx(QStringLiteral("hello_world $x a1b_2"));
        auto toks = lx.tokenize();
        QCOMPARE(toks.size(), 4);
        QCOMPARE(toks[0].kind, TokKind::Identifier);
        QCOMPARE(toks[0].textValue, QStringLiteral("hello_world"));
        QCOMPARE(toks[1].textValue, QStringLiteral("$x"));
        QCOMPARE(toks[2].textValue, QStringLiteral("a1b_2"));
    }

    void testNumberInteger()
    {
        Lexer lx(QStringLiteral("42"));
        auto toks = lx.tokenize();
        QCOMPARE(toks[0].kind, TokKind::Number);
        QCOMPARE(toks[0].numberValue, 42.0);
    }

    void testNumberDecimal()
    {
        Lexer lx(QStringLiteral("3.14"));
        auto toks = lx.tokenize();
        QCOMPARE(toks[0].kind, TokKind::Number);
        QCOMPARE(toks[0].numberValue, 3.14);
    }

    void testStringDoubleQuoted()
    {
        Lexer lx(QStringLiteral("\"hello\\nworld\""));
        auto toks = lx.tokenize();
        QCOMPARE(toks[0].kind, TokKind::String);
        QCOMPARE(toks[0].textValue, QStringLiteral("hello\nworld"));
    }

    void testStringDoubleQuotedEscapedQuote()
    {
        Lexer lx(QStringLiteral("\"say \\\"hi\\\"\""));
        auto toks = lx.tokenize();
        QCOMPARE(toks[0].kind, TokKind::String);
        QCOMPARE(toks[0].textValue, QStringLiteral("say \"hi\""));
    }

    void testStringSingleQuotedAddendumRewrite()
    {
        // Addendum section 7 rewrite: 'it\'s "cool"' -> it's "cool"
        Lexer lx(QStringLiteral("'it\\'s \"cool\"'"));
        auto toks = lx.tokenize();
        QCOMPARE(toks[0].kind, TokKind::String);
        QCOMPARE(toks[0].textValue, QStringLiteral("it's \"cool\""));
    }

    void testRegexLiteral()
    {
        Lexer lx(QStringLiteral("/foo/i"));
        auto toks = lx.tokenize();
        QCOMPARE(toks[0].kind, TokKind::Regex);
        QCOMPARE(toks[0].textValue, QStringLiteral("foo"));
        QCOMPARE(toks[0].regexFlags, QStringLiteral("i"));
    }

    void testRegexVsDivision()
    {
        // After an identifier, `/` is division.
        Lexer lx1(QStringLiteral("a / b"));
        auto t1 = lx1.tokenize();
        QCOMPARE(t1[1].kind, TokKind::Slash);

        // After `(`, `/` starts a regex.
        Lexer lx2(QStringLiteral("f(/re/)"));
        auto t2 = lx2.tokenize();
        // Sequence: Identifier LParen Regex RParen End
        QCOMPARE(t2[0].kind, TokKind::Identifier);
        QCOMPARE(t2[1].kind, TokKind::LParen);
        QCOMPARE(t2[2].kind, TokKind::Regex);
    }

    void testAllBinaryOperators()
    {
        // `a` between operators forces each `/` position to follow an
        // identifier (so `/` lexes as Slash, not a regex literal).
        Lexer lx(QStringLiteral("a || a && a == a != a <= a >= a < a > a + a - a * a / a % a"));
        auto toks = lx.tokenize();
        QVector<TokKind> expected {
            TokKind::Identifier, TokKind::OrOr,  TokKind::Identifier,
            TokKind::AndAnd,     TokKind::Identifier, TokKind::EqEq,
            TokKind::Identifier, TokKind::BangEq, TokKind::Identifier,
            TokKind::LtEq,       TokKind::Identifier, TokKind::GtEq,
            TokKind::Identifier, TokKind::Lt,    TokKind::Identifier,
            TokKind::Gt,         TokKind::Identifier, TokKind::Plus,
            TokKind::Identifier, TokKind::Minus, TokKind::Identifier,
            TokKind::Star,       TokKind::Identifier, TokKind::Slash,
            TokKind::Identifier, TokKind::Percent, TokKind::Identifier,
            TokKind::End
        };
        QCOMPARE(toks.size(), expected.size());
        for (int i = 0; i < expected.size(); ++i)
            QCOMPARE(toks[i].kind, expected[i]);
    }

    void testPunctuationOperators()
    {
        Lexer lx(QStringLiteral("!x (y) [z] a, b.c"));
        auto toks = lx.tokenize();
        QVector<TokKind> expected {
            TokKind::Bang, TokKind::Identifier,
            TokKind::LParen, TokKind::Identifier, TokKind::RParen,
            TokKind::LBracket, TokKind::Identifier, TokKind::RBracket,
            TokKind::Identifier, TokKind::Comma, TokKind::Identifier,
            TokKind::Dot, TokKind::Identifier,
            TokKind::End
        };
        QCOMPARE(toks.size(), expected.size());
        for (int i = 0; i < expected.size(); ++i)
            QCOMPARE(toks[i].kind, expected[i]);
    }

    void testInvalidCharProducesInvalid()
    {
        Lexer lx(QStringLiteral("@"));
        auto toks = lx.tokenize();
        QCOMPARE(toks[0].kind, TokKind::Invalid);
    }

    void testUnterminatedString()
    {
        Lexer lx(QStringLiteral("\"never closed"));
        auto toks = lx.tokenize();
        QCOMPARE(toks[0].kind, TokKind::Invalid);
    }
};

QTEST_APPLESS_MAIN(TestLexer)
#include "tst_lexer.moc"
