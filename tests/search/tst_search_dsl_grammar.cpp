// SPDX-License-Identifier: GPL-3.0-or-later
//
// Grammar coverage for the parts of app/own/1170-hP.js that the parser used to
// lex but not build: the `whole-word`-is-not-an-operator correction, the
// case-insensitive operator lookup, TRUE / FALSE / EMPTY, bare `>` / `<`, and
// `[key]` / `[key: value]`.
//
// See docs/obsidian-audit/addenda/2026-10-03-search-dsl-grammar-and-quick-switcher-scoring.md.
#include <QTest>

#include "corbomite/search/SearchDSL.h"

using namespace Corbomite;
using Kind = SearchNode::Kind;

class TestSearchDslGrammar : public QObject {
    Q_OBJECT

    static SearchNodePtr parseOk(const QString &q)
    {
        auto r = SearchDSL::parse(q);
        if (!r.error.isEmpty()) qWarning() << "Unexpected parse error:" << r.error;
        return r.root;
    }

private Q_SLOTS:
    // --- flags: the only two are match-case and ignore-case ---

    // app/own/1175-SP.js:149-158. Neither takes an operand slot of its own kind;
    // they wrap whatever follows.
    void testMatchCaseFlag()
    {
        auto root = parseOk(QStringLiteral("match-case:foo"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::OpCall);
        QCOMPARE(root->text, QStringLiteral("match-case"));
        QCOMPARE(root->children.at(0)->kind, Kind::Text);
    }

    void testIgnoreCaseFlag()
    {
        auto root = parseOk(QStringLiteral("ignore-case:foo"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::OpCall);
        QCOMPARE(root->text, QStringLiteral("ignore-case"));
        QCOMPARE(root->children.at(0)->kind, Kind::Text);
    }

    // `whole-word` is not a DSL operator anywhere in 1.13.7. The only three
    // occurrences of the string in the corpus are an icon-name key for the
    // editor's find/replace whole-word toggle (app/own/0713-gg.js:11337).
    // Whole-word *semantics* exist, but they are what a quoted phrase does
    // (app/own/1166-ZA.js:90), never a flag.
    void testWholeWordIsNotAnOperator()
    {
        auto r = SearchDSL::parse(QStringLiteral("whole-word:foo"));
        QVERIFY(!r.error.isEmpty());
        QVERIFY(r.error.contains(QStringLiteral("not recognized")));
        QVERIFY(!SearchDSL::supportedOperators().contains(QStringLiteral("whole-word")));
    }

// The operators of app/own/1175-SP.js:148-221, exactly. That is twelve, not
    // the "eleven" the addendum's §A.3 prose claims — its own listing already
    // enumerates twelve.
    void testOperatorTableMatchesSource()
    {
        const auto &ops = SearchDSL::supportedOperators();
        QCOMPARE(ops.size(), 12);
        for (const auto *op : {"match-case", "ignore-case", "path", "file", "content",
                               "line", "block", "section", "task", "task-todo", "task-done",
                               "tag"}) {
            QVERIFY2(ops.contains(QString::fromLatin1(op)), op);
        }
    }

    // app/own/1170-hP.js:9 — `i.toLowerCase()` before the table lookup.
    void testOperatorNamesAreCaseInsensitive()
    {
        auto root = parseOk(QStringLiteral("MATCH-CASE:foo"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::OpCall);
        QCOMPARE(root->text, QStringLiteral("match-case"));
        QVERIFY(SearchDSL::parse(QStringLiteral("PATH:x")).error.isEmpty());
    }

    // --- TRUE / FALSE / EMPTY (app/own/1170-hP.js:68 → 1163-UA.js:21) ---

    void testBooleanKeywords()
    {
        for (const auto *kw : {"TRUE", "FALSE", "EMPTY"}) {
            auto root = parseOk(QString::fromLatin1(kw));
            QVERIFY(root);
            QCOMPARE(root->kind, Kind::PropertyValue);
            QCOMPARE(root->text, QString::fromLatin1(kw));
        }
    }

    // A bare top-level EMPTY matched nothing before this was a parse case: the
    // token was produced by the lexer and then fell through to
    // `default: return nullptr`, which `uP` treats as end-of-list — so the whole
    // conjunct list after it was silently truncated.
    void testBareKeywordDoesNotTruncateTheConjunctList()
    {
        auto root = parseOk(QStringLiteral("EMPTY foo"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::And);
        QCOMPARE(root->children.size(), 2);
        QCOMPARE(root->children.at(0)->kind, Kind::PropertyValue);
        QCOMPARE(root->children.at(1)->text, QStringLiteral("foo"));
    }

    void testKeywordComparisonIsCaseSensitive()
    {
        // app/own/1170-hP.js:4-24 — the four bareword comparisons are exact.
        for (const auto *kw : {"true", "empty", "False"}) {
            auto root = parseOk(QString::fromLatin1(kw));
            QVERIFY(root);
            QCOMPARE(root->kind, Kind::Text);
        }
    }

    // --- bare > and < (app/own/1170-hP.js:71 → 1166-ZA.js:190) ---

    void testBareComparisons()
    {
        auto gt = parseOk(QStringLiteral(">4"));
        QVERIFY(gt);
        QCOMPARE(gt->kind, Kind::Compare);
        QCOMPARE(gt->text, QStringLiteral(">"));
        QCOMPARE(gt->children.at(0)->kind, Kind::Text);
        QCOMPARE(gt->children.at(0)->text, QStringLiteral("4"));

        auto lt = parseOk(QStringLiteral("<10"));
        QVERIFY(lt);
        QCOMPARE(lt->kind, Kind::Compare);
        QCOMPARE(lt->text, QStringLiteral("<"));
        QCOMPARE(lt->children.at(0)->text, QStringLiteral("10"));
    }

    // `>` also used to truncate the conjunct list instead of building a node.
    void testBareComparisonDoesNotTruncateTheConjunctList()
    {
        auto root = parseOk(QStringLiteral(">5 foo"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::And);
        QCOMPARE(root->children.size(), 2);
        QCOMPARE(root->children.at(0)->kind, Kind::Compare);
        QCOMPARE(root->children.at(1)->text, QStringLiteral("foo"));
    }

    // app/own/1166-ZA.js:193-197 — `aP` takes a text or quoted operand only.
    void testComparisonRejectsNonTextOperand()
    {
        auto r = SearchDSL::parse(QStringLiteral(">(a b)"));
        QVERIFY(!r.error.isEmpty());
        QVERIFY(r.error.contains(QStringLiteral("cannot be applied")));
    }

    // The real-world form: a comparison inside a property value.
    void testComparisonInsidePropertyValue()
    {
        auto root = parseOk(QStringLiteral("[rating: >4]"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::PropertyCall);
        QCOMPARE(root->children.at(0)->text, QStringLiteral("rating"));
        QCOMPARE(root->children.at(1)->kind, Kind::Compare);
        QCOMPARE(root->children.at(1)->text, QStringLiteral(">"));
        QCOMPARE(root->children.at(1)->children.at(0)->text, QStringLiteral("4"));
    }

    // --- property syntax (app/own/1170-hP.js:24-39) ---

    void testEmptyPropertyKeyDefaultsToEmptyText()
    {
        auto root = parseOk(QStringLiteral("[]"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::PropertyCall);
        QCOMPARE(root->children.size(), 1);
        QCOMPARE(root->children.at(0)->kind, Kind::Text);
        QVERIFY(root->children.at(0)->text.isEmpty());
    }

    void testTrailingColonInPropertyIsAnExistenceCheck()
    {
        // app/own/1170-hP.js:34 assigns `cP`'s result without a null guard, so a
        // value-less `[key:]` stays null and reads as an existence check.
        auto root = parseOk(QStringLiteral("[author:]"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::PropertyCall);
        QCOMPARE(root->children.size(), 1);
        QCOMPARE(root->children.at(0)->text, QStringLiteral("author"));
    }

    // app/own/1170-hP.js:7 — a `[` on the stack switches operator recognition off,
    // so `[path:x]` is a property named "path", not a path: operator call.
    void testPropertyBodyDoesNotRecogniseOperators()
    {
        auto root = parseOk(QStringLiteral("[path:x]"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::PropertyCall);
        QCOMPARE(root->children.at(0)->kind, Kind::Text);
        QCOMPARE(root->children.at(0)->text, QStringLiteral("path"));
        QCOMPARE(root->children.at(1)->text, QStringLiteral("x"));
    }

    // app/own/1170-hP.js:25
    void testNestedPropertyRejected()
    {
        auto r = SearchDSL::parse(QStringLiteral("[[a]]"));
        QVERIFY(!r.error.isEmpty());
        QVERIFY(r.error.contains(QStringLiteral("cannot be nested")));
    }

    // --- tokeniser: `-`, `/` and `"` are only special at a token start ---
    // app/own/1173-fP.js:29 tests the leading character only; once the scan has
    // started those three are ordinary characters.

    // `readText` used to drop a hyphen whose next character was a delimiter, so
    // `foo-` lexed as text("foo") + not, which silently dropped the rest of the
    // query. Obsidian yields text("foo-").
    void testTrailingHyphenStaysInTheWord()
    {
        auto root = parseOk(QStringLiteral("foo-"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::Text);
        QCOMPARE(root->text, QStringLiteral("foo-"));
    }

    void testLeadingHyphenIsStillNegation()
    {
        auto root = parseOk(QStringLiteral("foo -bar"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::And);
        QCOMPARE(root->children.at(1)->kind, Kind::Not);
        QCOMPARE(root->children.at(1)->children.at(0)->text, QStringLiteral("bar"));
    }

    void testHyphenatedOperatorNameIsOneToken()
    {
        auto root = parseOk(QStringLiteral("task-todo:x"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::OpCall);
        QCOMPARE(root->text, QStringLiteral("task-todo"));
    }

    void testSlashInsideWordIsNotARegex()
    {
        auto root = parseOk(QStringLiteral("a/b"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::Text);
        QCOMPARE(root->text, QStringLiteral("a/b"));
    }

    void testQuoteInsideWordIsNotAQuoteToken()
    {
        auto root = parseOk(QStringLiteral("ab\"cd"));
        QVERIFY(root);
        QCOMPARE(root->kind, Kind::Text);
        QCOMPARE(root->text, QStringLiteral("ab\"cd"));
    }

    // --- compile(): the new node kinds need a frontmatter side table ---

    void testPropertyCompilesToUnsupported()
    {
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("[author:x]")));
        QVERIFY(plan.fts5Query.isEmpty());
        QCOMPARE(plan.unsupported, QStringList{QStringLiteral("[key: value]")});
    }

    void testExistencePropertyCompilesToUnsupported()
    {
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("[aliases]")));
        QCOMPARE(plan.unsupported, QStringList{QStringLiteral("[key]")});
    }

    void testComparisonCompilesToUnsupported()
    {
        // The nested Compare inside the property value is reported too.
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("[rating: >4]")));
        QCOMPARE(plan.unsupported,
                 QStringList({QStringLiteral("[key: value]"), QStringLiteral(">")}));
    }

    void testKeywordCompilesToUnsupported()
    {
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("TRUE")));
        QCOMPARE(plan.unsupported, QStringList{QStringLiteral("TRUE")});
    }

    // A property key or value is not a content term, so match-case must not turn
    // it into a case-sensitive content filter.
    void testMatchCaseDoesNotLiftPropertyLiterals()
    {
        auto plan = SearchDSL::compile(parseOk(QStringLiteral("match-case:[author:x]")));
        QVERIFY(plan.caseSensitiveTerms.isEmpty());
        QCOMPARE(plan.unsupported, QStringList{QStringLiteral("[key: value]")});
    }
};

QTEST_APPLESS_MAIN(TestSearchDslGrammar)
#include "tst_search_dsl_grammar.moc"
