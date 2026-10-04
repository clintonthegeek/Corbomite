// SPDX-License-Identifier: GPL-3.0-or-later
//
// `prepareSimpleSearch` — Obsidian's *separate* matcher, not a flag on
// `prepareQuery`. app/own/0789-ab.js splits the query on spaces, 0787-rb.js
// requires every token to occur as an ordered substring, and 0788-ob.js scores
// the merged ranges with the penalty hardcoded to 0 against the ORIGINAL
// candidate length.
#include <QTest>

#include "corbomite/search/FuzzyMatcher.h"

using namespace Corbomite;

class TestFuzzySimpleSearch : public QObject {
    Q_OBJECT

    static std::optional<FuzzyMatch> simple(const QString &query, const QString &candidate)
    {
        return FuzzyMatcher::fuzzySearch(FuzzyMatcher::prepareSimpleSearch(query), candidate);
    }

private Q_SLOTS:
    void testPreparedQuerySplitsOnSpaceOnly()
    {
        const auto q = FuzzyMatcher::prepareSimpleSearch(QStringLiteral("my-file.md 日本"));
        QCOMPARE(q.tokens, QStringList({QStringLiteral("my-file.md"),
                                        QString::fromUtf8("日本")}));
        QVERIFY(q.simple);
    }

    // 0787-rb.js restarts each token's scan at `l = -1`, so the AND is over
    // *substrings*, not over an ordered walk: "foo bar" matches "barxxfoo". The
    // addendum's §C.3 calls `rb` "an ordered substring-AND", which the source
    // does not support — only the cursor *within* one token's occurrences is
    // ordered.
    void testSubstringAndIsUnorderedAcrossTokens()
    {
        QVERIFY(simple(QStringLiteral("foo bar"), QStringLiteral("barxxfoo")).has_value());
        QVERIFY(!simple(QStringLiteral("foo bar"), QStringLiteral("foobaz")).has_value());
    }

    // No char-fuzzy fallback (0789-ab.js has no `fuzzy` alphabet), so "fs" finds
    // "FileSystemAdapter" under prepareQuery but not here.
    void testNoCharFuzzyFallback()
    {
        QVERIFY(FuzzyMatcher::fuzzySearch(FuzzyMatcher::prepareQuery(QStringLiteral("fs")),
                                          QStringLiteral("FileSystemAdapter")).has_value());
        QVERIFY(!simple(QStringLiteral("fs"), QStringLiteral("FileSystemAdapter")).has_value());
    }

    // 0787-rb.js:6 advances by `len + 1`, so a token never reports an occurrence
    // overlapping its own previous one. "aa" against "aaaa" yields one range —
    // not the merged [0,4) an unconstrained scan would give.
    void testOverlappingOccurrencesAreNotReported()
    {
        const auto m = simple(QStringLiteral("aa"), QStringLiteral("aaaa"));
        QVERIFY(m.has_value());
        QCOMPARE(m->matches.size(), 1);
        QCOMPARE(m->matches.at(0), qMakePair(0, 2));
    }

    // 0787-rb.js pushes every non-overlapping occurrence of every token, and
    // `Gy` coalesces them (app/own/0778-Gy.js).
    void testAllOccurrencesAreCollected()
    {
        const auto m = simple(QStringLiteral("aa"), QStringLiteral("aaaaaa"));
        QVERIFY(m.has_value());
        QCOMPARE(m->matches.size(), 2);
        QCOMPARE(m->matches.at(0), qMakePair(0, 2));
        QCOMPARE(m->matches.at(1), qMakePair(3, 5));
    }

    // 0788-ob.js:5 — one run at [0,3), penalty 0, ORIGINAL candidate length:
    //   -(1-1) - 0 - (3-0+1-3)/100 - 0 - 3/10000 = -0.0103
    void testScoreHasNoPenaltyTerm()
    {
        const auto m = simple(QStringLiteral("foo"), QStringLiteral("foo"));
        QVERIFY(m.has_value());
        QCOMPARE(m->score, -0.0103);
    }

    // An all-whitespace query splits into empty tokens, which `rb` skips
    // (0787-rb.js:5) leaving an empty range list. `[]` is truthy in JS, so `ob`
    // still reports a hit — `Jy` returns 0 for an empty range list — whereas the
    // two-pass matcher reports no match at all for the same input.
    void testWhitespaceOnlyQueryScoresZeroWithNoRanges()
    {
        const auto m = simple(QStringLiteral("   "), QStringLiteral("foo"));
        QVERIFY(m.has_value());
        QVERIFY(m->matches.isEmpty());
        QCOMPARE(m->score, 0.0);
        QVERIFY(!FuzzyMatcher::fuzzySearch(FuzzyMatcher::prepareQuery(QStringLiteral("   ")),
                                           QStringLiteral("foo")).has_value());
    }
};

QTEST_APPLESS_MAIN(TestFuzzySimpleSearch)
#include "tst_fuzzy_simple_search.moc"
