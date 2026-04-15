// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/search/SearchDSL.h"

using namespace Corbomite;
using Kind = SearchNode::Kind;

class TestSearchDSL : public QObject {
    Q_OBJECT

private:
    static SearchNodePtr parseOk(const QString &q)
    {
        auto r = SearchDSL::parse(q);
        if (!r.error.isEmpty()) {
            qWarning() << "Unexpected parse error:" << r.error << "@" << r.errorOffset;
        }
        return r.root;
    }

private Q_SLOTS:
    // --- Empty / trivial ---

    void testEmptyQueryReturnsNullRoot()
    {
        auto r = SearchDSL::parse(QString());
        QVERIFY(r.error.isEmpty());
        QVERIFY(!r.root);
    }

    void testWhitespaceOnly()
    {
        auto r = SearchDSL::parse(QStringLiteral("   \t  "));
        QVERIFY(r.error.isEmpty());
        QVERIFY(!r.root);
    }

    void testBareText()
    {
        auto root = parseOk(QStringLiteral("hello"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::Text);
        QCOMPARE(root->text, QStringLiteral("hello"));
    }

    // --- AND / OR / NOT / grouping ---

    void testImplicitAnd()
    {
        auto root = parseOk(QStringLiteral("foo bar baz"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::And);
        QCOMPARE(root->children.size(), 3);
        QCOMPARE(root->children.at(0)->kind, Kind::Text);
        QCOMPARE(root->children.at(2)->text, QStringLiteral("baz"));
    }

    void testOrLowerThanAnd()
    {
        // foo bar OR baz qux  →  (foo AND bar) OR (baz AND qux)
        auto root = parseOk(QStringLiteral("foo bar OR baz qux"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::Or);
        QCOMPARE(root->children.size(), 2);
        QCOMPARE(root->children.at(0)->kind, Kind::And);
        QCOMPARE(root->children.at(1)->kind, Kind::And);
    }

    void testNotPrefix()
    {
        auto root = parseOk(QStringLiteral("foo -bar"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::And);
        QCOMPARE(root->children.size(), 2);
        QCOMPARE(root->children.at(1)->kind, Kind::Not);
        QCOMPARE(root->children.at(1)->children.at(0)->text, QStringLiteral("bar"));
    }

    void testGrouping()
    {
        // foo (bar OR baz)  → AND[foo, Group[Or[bar, baz]]]
        auto root = parseOk(QStringLiteral("foo (bar OR baz)"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::And);
        QCOMPARE(root->children.at(1)->kind, Kind::Group);
        QCOMPARE(root->children.at(1)->children.at(0)->kind, Kind::Or);
    }

    // --- Atoms ---

    void testQuotedPhrase()
    {
        auto root = parseOk(QStringLiteral("\"hello world\""));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::Phrase);
        QCOMPARE(root->text, QStringLiteral("hello world"));
    }

    void testEscapedQuoteInQuotedPhrase()
    {
        auto root = parseOk(QStringLiteral("\"they said \\\"hi\\\"\""));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::Phrase);
        QCOMPARE(root->text, QStringLiteral("they said \"hi\""));
    }

    void testRegex()
    {
        auto root = parseOk(QStringLiteral("/\\d{4}-\\d{2}/"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::Regex);
        QCOMPARE(root->text, QStringLiteral("\\d{4}-\\d{2}"));
    }

    void testInvalidRegexErrors()
    {
        auto r = SearchDSL::parse(QStringLiteral("/[unclosed/"));
        QVERIFY(!r.error.isEmpty());
        QVERIFY(r.error.contains(QStringLiteral("regular expression")));
    }

    // --- Operator calls ---

    void testPathOperator()
    {
        auto root = parseOk(QStringLiteral("path:Daily"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::OpCall);
        QCOMPARE(root->text, QStringLiteral("path"));
        QCOMPARE(root->children.at(0)->kind, Kind::Text);
        QCOMPARE(root->children.at(0)->text, QStringLiteral("Daily"));
    }

    void testTagOperator()
    {
        auto root = parseOk(QStringLiteral("tag:#work"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::OpCall);
        QCOMPARE(root->text, QStringLiteral("tag"));
        // text body includes the # — Obsidian treats the body as opaque text
        QCOMPARE(root->children.at(0)->text, QStringLiteral("#work"));
    }

    void testTagRejectsQuotedOperand()
    {
        auto r = SearchDSL::parse(QStringLiteral("tag:\"foo\""));
        QVERIFY(!r.error.isEmpty());
        QVERIFY(r.error.contains(QStringLiteral("tag")));
    }

    void testOperatorWithQuotedOperand()
    {
        auto root = parseOk(QStringLiteral("path:\"Daily notes/2022-07\""));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::OpCall);
        QCOMPARE(root->children.at(0)->kind, Kind::Phrase);
        QCOMPARE(root->children.at(0)->text, QStringLiteral("Daily notes/2022-07"));
    }

    void testUnrecognizedOperatorErrors()
    {
        auto r = SearchDSL::parse(QStringLiteral("bogus:foo"));
        QVERIFY(!r.error.isEmpty());
        QVERIFY(r.error.contains(QStringLiteral("not recognized")));
    }

    void testExclusiveNestingRejected()
    {
        // path:(file:foo) — file is exclusive, nested in path
        auto r = SearchDSL::parse(QStringLiteral("path:(file:foo)"));
        QVERIFY(!r.error.isEmpty());
        QVERIFY(r.error.contains(QStringLiteral("cannot be nested")));
    }

    void testSectionAllowsSelfNesting()
    {
        auto root = parseOk(QStringLiteral("section:(section:foo)"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::OpCall);
        QCOMPARE(root->text, QStringLiteral("section"));
    }

    void testMatchCaseNotExclusive()
    {
        // match-case wraps an exclusive operator — must succeed.
        auto root = parseOk(QStringLiteral("match-case:path:foo"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::OpCall);
        QCOMPARE(root->text, QStringLiteral("match-case"));
        QCOMPARE(root->children.at(0)->kind, Kind::OpCall);
        QCOMPARE(root->children.at(0)->text, QStringLiteral("path"));
    }

    // --- Edge cases per spec §4 ---

    void testTrailingOrSilent()
    {
        auto root = parseOk(QStringLiteral("foo OR"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::Text);
        QCOMPARE(root->text, QStringLiteral("foo"));
    }

    void testTrailingColonProducesEmptyTextOperand()
    {
        auto root = parseOk(QStringLiteral("path:"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::OpCall);
        QCOMPARE(root->children.at(0)->kind, Kind::Text);
        QVERIFY(root->children.at(0)->text.isEmpty());
    }

    void testUnterminatedQuoteAcceptedAtEof()
    {
        auto root = parseOk(QStringLiteral("\"foo"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::Phrase);
        QCOMPARE(root->text, QStringLiteral("foo"));
    }

    void testUnterminatedRegexAcceptedAtEof()
    {
        auto root = parseOk(QStringLiteral("/foo"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::Regex);
    }

    void testOrIsCaseSensitive()
    {
        // lowercase 'or' is plain text (and joins the AND chain)
        auto root = parseOk(QStringLiteral("foo or bar"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::And);
        QCOMPARE(root->children.size(), 3);
        QCOMPARE(root->children.at(1)->text, QStringLiteral("or"));
    }

    void testPropertyStubSkipped()
    {
        // [aliases] currently parses to nothing, leaving surrounding terms intact.
        auto root = parseOk(QStringLiteral("foo [aliases] bar"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::And);
        // Property-call returns null so we get exactly two siblings.
        QCOMPARE(root->children.size(), 2);
        QCOMPARE(root->children.at(0)->text, QStringLiteral("foo"));
        QCOMPARE(root->children.at(1)->text, QStringLiteral("bar"));
    }

    void testSupportedOperatorsList()
    {
        const auto &ops = SearchDSL::supportedOperators();
        QVERIFY(ops.contains(QStringLiteral("path")));
        QVERIFY(ops.contains(QStringLiteral("tag")));
        QVERIFY(ops.contains(QStringLiteral("match-case")));
    }

    // --- compile() → CompiledPlan ---

    void testCompileBareText()
    {
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("hello")));
        QCOMPARE(plan.fts5Query, QStringLiteral("\"hello\""));
        QVERIFY(plan.requiredTags.isEmpty());
        QVERIFY(plan.unsupported.isEmpty());
    }

    void testCompileAnd()
    {
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("foo bar")));
        QCOMPARE(plan.fts5Query, QStringLiteral("\"foo\" AND \"bar\""));
    }

    void testCompileOr()
    {
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("foo OR bar")));
        QCOMPARE(plan.fts5Query, QStringLiteral("(\"foo\" OR \"bar\")"));
    }

    void testCompilePathOperator()
    {
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("path:Daily")));
        QCOMPARE(plan.fts5Query, QStringLiteral("path:\"Daily\""));
    }

    void testCompileFileMapsToTitleColumn()
    {
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("file:notes")));
        QCOMPARE(plan.fts5Query, QStringLiteral("title:\"notes\""));
    }

    void testCompileTagToRequired()
    {
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("tag:#work")));
        QVERIFY(plan.fts5Query.isEmpty());
        QCOMPARE(plan.requiredTags.size(), 1);
        QCOMPARE(plan.requiredTags.at(0), QStringLiteral("work"));
    }

    void testCompileTagAndContent()
    {
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("tag:#work foo")));
        QCOMPARE(plan.requiredTags.size(), 1);
        QCOMPARE(plan.fts5Query, QStringLiteral("\"foo\""));
    }

    void testCompileNotMaintainsBinaryNot()
    {
        // foo -bar  →  "foo" NOT "bar"  (FTS5 binary NOT)
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("foo -bar")));
        QCOMPARE(plan.fts5Query, QStringLiteral("\"foo\" NOT \"bar\""));
    }

    void testCompileRegexFlaggedUnsupported()
    {
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("/abc/")));
        QVERIFY(plan.fts5Query.isEmpty());
        QVERIFY(!plan.unsupported.isEmpty());
        QVERIFY(plan.unsupported.first().contains(QStringLiteral("regex")));
    }

    void testCompileMatchCaseFlaggedAndUnwrapped()
    {
        // match-case currently can't be honoured by FTS5; we still compile the
        // inner subtree and surface a note in `unsupported`.
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("match-case:foo")));
        QCOMPARE(plan.fts5Query, QStringLiteral("\"foo\""));
        QVERIFY(!plan.unsupported.isEmpty());
    }

    void testCompileIgnoreCaseTransparent()
    {
        // ignore-case is the FTS5 default — it should add nothing to unsupported.
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("ignore-case:foo")));
        QCOMPARE(plan.fts5Query, QStringLiteral("\"foo\""));
        QVERIFY(plan.unsupported.isEmpty());
    }

    void testCompileEmptyAst()
    {
        auto plan = SearchDSL::compile(nullptr);
        QVERIFY(plan.fts5Query.isEmpty());
        QVERIFY(plan.requiredTags.isEmpty());
    }

    void testCompileQuoteEscaping()
    {
        // Double-quote inside the term must be doubled per FTS5.
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("\"he said \\\"hi\\\"\"")));
        QCOMPARE(plan.fts5Query, QStringLiteral("\"he said \"\"hi\"\"\""));
    }
};

QTEST_APPLESS_MAIN(TestSearchDSL)
#include "tst_searchdsl.moc"
