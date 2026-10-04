// SPDX-License-Identifier: GPL-3.0-or-later
//
// The case-fidelity penalty rule of app/own/0783-eb.js:9-18.
//
// The five scoring constants in `Jy` (app/own/0782-Jy.js) matched all along; the
// predicate that decides *when* a landing owes a penalty did not, and the strict
// pass was a different algorithm. Every expectation is pinned to an exact score,
// so a drift in any of the five terms or in the predicate itself turns red rather
// than quietly reordering results. Each "pre-fix" figure below was measured by
// running the old matcher, not asserted from a document.
#include <QTest>

#include "corbomite/search/FuzzyMatcher.h"

using namespace Corbomite;

class TestFuzzyPenaltyRule : public QObject {
    Q_OBJECT

    static FuzzyMatch score(const QString &query, const QString &candidate)
    {
        auto m = FuzzyMatcher::fuzzySearch(FuzzyMatcher::prepareQuery(query), candidate);
        return m ? *m : FuzzyMatch{};
    }

private Q_SLOTS:
    // app/own/0783-eb.js:12-17 — in strict (char-fuzzy) mode a mid-word landing is
    // refused unless it sits exactly on the cursor, after which the cursor slides
    // one token length and the same token is retried. The penalty is only ever
    // charged on the non-strict branch (`eb.js:17`), so a strict pass never owes
    // one.
    //
    // "md" against "m1d": the word-token pass fails, then 'm' takes the cursor to
    // 1 and 'd' first occurs at 2; the cursor slides to 2 and the retry is
    // accepted. Two runs, penalty 0:
    //   -(2-1) - 0/10 - (3-0+1-2)/100 - 0/1000 - 3/10000 = -1.0203
    // The pre-fix matcher produced the same ranges but charged a penalty. Its
    // recorded value of -1.1303 in the 2026-10-03 addendum is a mis-addition —
    // the terms it lists sum to -1.1203, which is what the binary returns:
    //   -1 - 0.1 - 0.02 - 0 - 0.0003                       = -1.1203
    void testStrictPassNeverChargesPenalty()
    {
        const auto m = score(QStringLiteral("md"), QStringLiteral("m1d"));
        QCOMPARE(m.matches.size(), 2);
        QCOMPARE(m.matches.at(0), qMakePair(0, 1));
        QCOMPARE(m.matches.at(1), qMakePair(2, 3));
        QCOMPARE(m.score, -1.0203);
    }

    // app/own/0783-eb.js:13-14 re-anchors at the cursor, sliding it forward one
    // token length per retry. It does NOT scan ahead for a later word-boundary
    // occurrence, which is what the pre-fix matcher did.
    //
    // "ac" against "a1c c": 'a'@0 takes the cursor to 1; 'c' first occurs at 2,
    // clause B fires (a digit is not whitespace, punctuation or CJK), and 2 is not
    // the cursor, so the cursor slides to 2 and the retry is accepted there — the
    // boundary 'c'@4 is never reached.
    //   -(2-1) - 0 - (3-0+1-2)/100 - 0 - 5/10000 = -1.0205
    // The pre-fix matcher probed forward past the cursor and took the
    // word-boundary 'c'@4, giving:
    //   -(2-1) - 0 - (5-0+1-2)/100 - 0 - 5/10000 = -1.0405
    void testStrictPassAnchorsAtCursorNotAtNextBoundary()
    {
        const auto m = score(QStringLiteral("ac"), QStringLiteral("a1c c"));
        QCOMPARE(m.matches.size(), 2);
        QCOMPARE(m.matches.at(0), qMakePair(0, 1));
        QCOMPARE(m.matches.at(1), qMakePair(2, 3));
        QCOMPARE(m.score, -1.0205);
    }

    // The same cursor re-anchor where the offsets happen to agree: "abc" against
    // "a-bc bc" yields [0,2) and [2,4) either way, but the pre-fix matcher
    // charged a penalty for the 'c' hit.
    //   -(2-1) - 0 - (4-0+1-3)/100 - 0 - 7/10000 = -1.0207   (penalty 0)
    //   -(2-1) - 1/10 - (4-0+1-3)/100 - 0 - 7/10000 = -1.1207 (penalty 1)
    void testStrictPassCursorReAnchorKeepsAdjacentHit()
    {
        const auto m = score(QStringLiteral("abc"), QStringLiteral("a-bc bc"));
        QCOMPARE(m.matches.size(), 2);
        QCOMPARE(m.matches.at(0), qMakePair(0, 1));
        QCOMPARE(m.matches.at(1), qMakePair(2, 4));
        QCOMPARE(m.score, -1.0207);
    }

    // app/own/0783-eb.js:11 clause B is gated on the matched character:
    // `h.toUpperCase() !== h`. A digit has no distinct uppercase form, so
    // matching one mid-word is a clean landing.
    //
    // "1" against "a1b": one run at [1,2), penalty 0:
    //   -(1-1) - 0 - (2-1+1-1)/100 - 1/1000 - 3/10000 = -0.0113
    // The pre-fix predicate asked whether the *previous* character was lowercase
    // and the current one uppercase, which a digit fails both halves of:
    //   -0.1 - 0.01 - 0.001 - 0.0003                 = -0.1113
    void testUncasedMatchedCharIsNeverMidWord()
    {
        const auto m = score(QStringLiteral("1"), QStringLiteral("a1b"));
        QCOMPARE(m.matches.size(), 1);
        QCOMPARE(m.matches.at(0), qMakePair(1, 2));
        QCOMPARE(m.score, -0.0113);
    }

    // app/own/0783-eb.js:11 clause A reads `h.toLowerCase() !== h &&
    // p.toLowerCase() !== p` — BOTH must be cased-upper. A digit or symbol before
    // an uppercase letter is therefore a clean landing, not a penalty.
    //
    // "bc" against "a1Bc": one run at [2,4), penalty 0:
    //   -(1-1) - 0 - (4-2+1-2)/100 - 2/1000 - 4/10000 = -0.0124
    // Pre-fix:
    //   -0.1 - 0.01 - 0.002 - 0.0004                  = -0.1124
    void testUncasedCharBeforeUppercaseIsNotMidWord()
    {
        const auto m = score(QStringLiteral("bc"), QStringLiteral("a1Bc"));
        QCOMPARE(m.matches.size(), 1);
        QCOMPARE(m.matches.at(0), qMakePair(2, 4));
        QCOMPARE(m.score, -0.0124);
    }

    // app/own/0783-eb.js:9 skips the whole penalty block when the matched
    // character is punctuation (`!Yy.test(h)`), whatever precedes it.
    //
    // "-" against "a-b": one run at [1,2), penalty 0:
    //   -(1-1) - 0 - (2-1+1-1)/100 - 1/1000 - 3/10000 = -0.0113
    // Pre-fix: -0.1 - 0.01 - 0.001 - 0.0003 = -0.1113.
    void testMatchedPunctuationCharIsNeverMidWord()
    {
        const auto m = score(QStringLiteral("-"), QStringLiteral("a-b"));
        QCOMPARE(m.matches.size(), 1);
        QCOMPARE(m.matches.at(0), qMakePair(1, 2));
        QCOMPARE(m.score, -0.0113);
    }

    // The same guard's CJK half: `!Qy.test(h)`, app/own/0783-eb.js:9.
    void testMatchedCjkCharIsNeverMidWord()
    {
        const auto m = score(QString::fromUtf8("日"), QString::fromUtf8("a日b"));
        QCOMPARE(m.matches.size(), 1);
        QCOMPARE(m.matches.at(0), qMakePair(1, 2));
        QCOMPARE(m.score, -0.0113);
    }

    // A hit inside an all-caps run IS mid-word, per clause A as written: h ("B")
    // and p ("A") are both cased-upper.
    //
    // "bc" against "ABc": one run at [1,3), penalty 1:
    //   -(1-1) - 1/10 - (3-1+1-2)/100 - 1/1000 - 3/10000 = -0.1113
    //
    // The 2026-10-03 addendum's §F5(a) worked example claims Obsidian scores
    // -0.0113 here, arguing clause A fails because `p.toLowerCase() === p` for
    // p = "A". That is false — "A".toLowerCase() is "a" — so clause A passes and
    // the penalty is charged. Obsidian and Corbomite agree on this input; the
    // -0.0113 vs -0.1113 pair the addendum reports is real, but belongs to
    // testUncasedMatchedCharIsNeverMidWord.
    void testHitInsideAllCapsRunIsMidWord()
    {
        const auto m = score(QStringLiteral("bc"), QStringLiteral("ABc"));
        QCOMPARE(m.matches.size(), 1);
        QCOMPARE(m.matches.at(0), qMakePair(1, 3));
        QCOMPARE(m.score, -0.1113);
    }
};

QTEST_APPLESS_MAIN(TestFuzzyPenaltyRule)
#include "tst_fuzzy_penalty_rule.moc"
