// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/search/FuzzyMatcher.h"

using namespace Corbomite;

class TestFuzzyMatcher : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // --- prepareQuery tokenisation ---

    void testPrepareQueryEmpty()
    {
        auto q = FuzzyMatcher::prepareQuery(QString());
        QVERIFY(q.isEmpty());
        QCOMPARE(q.tokens.size(), 0);
        QCOMPARE(q.fuzzy, QString());
    }

    void testPrepareQueryLowercasesTokens()
    {
        auto q = FuzzyMatcher::prepareQuery(QStringLiteral("FooBar"));
        QCOMPARE(q.query, QStringLiteral("FooBar"));   // case-preserved
        QCOMPARE(q.tokens, QStringList{QStringLiteral("foobar")});
        QCOMPARE(q.fuzzy, QStringLiteral("foobar"));
    }

    void testPrepareQueryWhitespaceFlushes()
    {
        auto q = FuzzyMatcher::prepareQuery(QStringLiteral("foo  bar"));
        QCOMPARE(q.tokens.size(), 2);
        QCOMPARE(q.tokens.at(0), QStringLiteral("foo"));
        QCOMPARE(q.tokens.at(1), QStringLiteral("bar"));
        QCOMPARE(q.fuzzy, QStringLiteral("foobar"));   // spaces removed
    }

    void testPrepareQueryPunctuationSingletons()
    {
        // search.md §1: "my-file.md" → ["my","-","file",".","md"]
        auto q = FuzzyMatcher::prepareQuery(QStringLiteral("my-file.md"));
        QCOMPARE(q.tokens.size(), 5);
        QCOMPARE(q.tokens.at(0), QStringLiteral("my"));
        QCOMPARE(q.tokens.at(1), QStringLiteral("-"));
        QCOMPARE(q.tokens.at(2), QStringLiteral("file"));
        QCOMPARE(q.tokens.at(3), QStringLiteral("."));
        QCOMPARE(q.tokens.at(4), QStringLiteral("md"));
    }

    void testPrepareQueryCJKCodepointSplit()
    {
        // search.md §1: "日本" → two singleton tokens.
        auto q = FuzzyMatcher::prepareQuery(QString::fromUtf8("日本"));
        QCOMPARE(q.tokens.size(), 2);
    }

    // --- prepareSimpleSearch ---

    void testPrepareSimpleSearchNoCJKSplit()
    {
        auto q = FuzzyMatcher::prepareSimpleSearch(QString::fromUtf8("日本"));
        QCOMPARE(q.tokens.size(), 1);   // simple mode keeps as one whitespace-token
        QVERIFY(q.simple);
    }

    void testPrepareSimpleSearchNoPunctSplit()
    {
        auto q = FuzzyMatcher::prepareSimpleSearch(QStringLiteral("my-file.md"));
        QCOMPARE(q.tokens.size(), 1);
        QCOMPARE(q.tokens.at(0), QStringLiteral("my-file.md"));
    }

    // --- fuzzySearch core behaviour ---

    void testEmptyQueryAlwaysMatches()
    {
        auto q = FuzzyMatcher::prepareQuery(QString());
        auto m = FuzzyMatcher::fuzzySearch(q, QStringLiteral("any text"));
        QVERIFY(m.has_value());
        QCOMPARE(m->score, 0.0);
        QCOMPARE(m->matches.size(), 0);
    }

    void testWordTokenMatchOrderPreserving()
    {
        // search.md §8: ["foo","bar"] does NOT match "barxxfoo" (l→r progression).
        auto q = FuzzyMatcher::prepareQuery(QStringLiteral("foo bar"));
        auto m = FuzzyMatcher::fuzzySearch(q, QStringLiteral("barxxfoo"));
        QVERIFY(!m.has_value());
    }

    void testWordTokenMatchInOrder()
    {
        // tokens ["foo","bar"] against "foo bar": the space at index 3 is in
        // neither range, so we get [0,3) + [4,7) — two non-touching ranges.
        auto q = FuzzyMatcher::prepareQuery(QStringLiteral("foo bar"));
        auto m = FuzzyMatcher::fuzzySearch(q, QStringLiteral("foo bar"));
        QVERIFY(m.has_value());
        QCOMPARE(m->matches.size(), 2);
        QCOMPARE(m->matches.at(0), qMakePair(0, 3));
        QCOMPARE(m->matches.at(1), qMakePair(4, 7));
    }

    void testCharFuzzyAcronymMatch()
    {
        // search.md §9: "mdf" → "getMarkdownFiles" via char-fuzzy at camelCase boundaries.
        auto q = FuzzyMatcher::prepareQuery(QStringLiteral("mdf"));
        auto m = FuzzyMatcher::fuzzySearch(q, QStringLiteral("getMarkdownFiles"));
        QVERIFY(m.has_value());
        QCOMPARE(m->matches.size(), 3);   // three runs of length 1
    }

    void testStartBiasFsAcronymInFileSystemAdapter()
    {
        // search.md §11 plan: "fs" matching "FileSystemAdapter" should score high
        // due to start-bias + camelCase-boundary acronym match (positions 0, 4).
        auto q = FuzzyMatcher::prepareQuery(QStringLiteral("fs"));
        auto m = FuzzyMatcher::fuzzySearch(q, QStringLiteral("FileSystemAdapter"));
        QVERIFY(m.has_value());
        QCOMPARE(m->matches.size(), 2);
        QCOMPARE(m->matches.at(0).first, 0);   // F
        QCOMPARE(m->matches.at(1).first, 4);   // S
    }

    void testTouchingRangesMerged()
    {
        // search.md §2: matches[i][1] == matches[i+1][0] coalesced into one range.
        // "ab" word-token finds [0,2); since one token there's nothing to merge,
        // but punctuation-singleton form "a-b" against "a-b" gives 3 ranges that
        // ALL touch and should collapse to one [0,3).
        auto q = FuzzyMatcher::prepareQuery(QStringLiteral("a-b"));
        auto m = FuzzyMatcher::fuzzySearch(q, QStringLiteral("a-b"));
        QVERIFY(m.has_value());
        QCOMPARE(m->matches.size(), 1);
        QCOMPARE(m->matches.at(0).first, 0);
        QCOMPARE(m->matches.at(0).second, 3);
    }

    void testNonOverlappingMergeSorted()
    {
        // After merging touching ranges, consecutive ranges must have a strict gap.
        auto q = FuzzyMatcher::prepareQuery(QStringLiteral("foo bar"));
        auto m = FuzzyMatcher::fuzzySearch(q, QStringLiteral("foo xx bar"));
        QVERIFY(m.has_value());
        QCOMPARE(m->matches.size(), 2);
        QVERIFY(m->matches.at(0).second < m->matches.at(1).first);
    }

    // --- scoring formula ordering ---

    void testStartBiasPrefersEarlierMatch()
    {
        // Same token at position 0 vs position 5: position 0 wins (start_bias term).
        auto q = FuzzyMatcher::prepareQuery(QStringLiteral("foo"));
        auto early = FuzzyMatcher::fuzzySearch(q, QStringLiteral("foo bar"));
        auto late = FuzzyMatcher::fuzzySearch(q, QStringLiteral("xxxxx foo"));
        QVERIFY(early && late);
        QVERIFY(early->score > late->score);
    }

    void testLengthBiasPrefersShorter()
    {
        auto q = FuzzyMatcher::prepareQuery(QStringLiteral("foo"));
        auto shortHay = FuzzyMatcher::fuzzySearch(q, QStringLiteral("foo"));
        auto longHay = FuzzyMatcher::fuzzySearch(q, QStringLiteral("foooooooooooooooo"));
        QVERIFY(shortHay && longHay);
        QVERIFY(shortHay->score > longHay->score);
    }

    void testContiguityDominatesScore()
    {
        // One contiguous run > two runs with same start position. Run-count
        // term is the largest single contributor.
        auto q1 = FuzzyMatcher::prepareQuery(QStringLiteral("foobar"));
        auto contig = FuzzyMatcher::fuzzySearch(q1, QStringLiteral("foobar"));
        auto q2 = FuzzyMatcher::prepareQuery(QStringLiteral("foo bar"));
        auto split = FuzzyMatcher::fuzzySearch(q2, QStringLiteral("foo zzz bar"));
        QVERIFY(contig && split);
        QVERIFY(contig->score > split->score);
    }

    // --- sortSearchResults ---

    void testSortDescending()
    {
        QVector<FuzzyMatch> v;
        v.append(FuzzyMatch{-5.0, {}});
        v.append(FuzzyMatch{0.0, {}});
        v.append(FuzzyMatch{-1.0, {}});
        FuzzyMatcher::sortSearchResults(v);
        QCOMPARE(v.at(0).score, 0.0);
        QCOMPARE(v.at(1).score, -1.0);
        QCOMPARE(v.at(2).score, -5.0);
    }
};

QTEST_APPLESS_MAIN(TestFuzzyMatcher)
#include "tst_fuzzymatcher.moc"
