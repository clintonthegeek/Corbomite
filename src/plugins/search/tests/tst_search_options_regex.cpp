// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "../SearchView.h"

#include "corbomite/search/SearchDSL.h"

using namespace Corbomite;

// Covers docs/punch-list.md "Search panel has no regex / match-case
// toggles": SearchView::planForQuery() is the pure query -> CompiledPlan
// translation the toolbar's "Match case" / "Regex" QToolButtons drive on
// every toggled search re-run. Exercised as a static/pure function so the
// DSL-plan wiring is verified without needing a live display or a
// SQLiteIndex-backed SearchProxy — see tst_search_plugin.cpp / tst_e2e_gui
// for the widget-construction and live-verification side of this feature.
class TestSearchOptionsRegex : public QObject
{
    Q_OBJECT
private slots:
    void regexToggleProducesNonEmptyRegexPatterns();
    void regexOffLeavesRegexPatternsEmpty();
    void matchCaseToggleProducesCaseSensitiveTerms();
    void matchCaseOffLeavesCaseSensitiveTermsEmpty();
    void bothTogglesCombine();
    void invalidRegexReportsError();
};

void TestSearchOptionsRegex::regexToggleProducesNonEmptyRegexPatterns()
{
    QString error;
    auto plan = SearchView::planForQuery(QStringLiteral("h[ae]llo"),
                                          /*matchCase=*/false, /*regex=*/true, &error);
    QVERIFY(error.isEmpty());
    QVERIFY(!plan.regexPatterns.isEmpty());
    QCOMPARE(plan.regexPatterns.first(), QStringLiteral("h[ae]llo"));
    // A pure regex query has no FTS5-expressible fragment of its own.
    QVERIFY(plan.fts5Query.isEmpty());
}

void TestSearchOptionsRegex::regexOffLeavesRegexPatternsEmpty()
{
    QString error;
    auto plan = SearchView::planForQuery(QStringLiteral("h[ae]llo"),
                                          /*matchCase=*/false, /*regex=*/false, &error);
    QVERIFY(error.isEmpty());
    QVERIFY(plan.regexPatterns.isEmpty());
}

void TestSearchOptionsRegex::matchCaseToggleProducesCaseSensitiveTerms()
{
    QString error;
    auto plan = SearchView::planForQuery(QStringLiteral("Hello"),
                                          /*matchCase=*/true, /*regex=*/false, &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(plan.caseSensitiveTerms, QStringList{QStringLiteral("Hello")});
}

void TestSearchOptionsRegex::matchCaseOffLeavesCaseSensitiveTermsEmpty()
{
    QString error;
    auto plan = SearchView::planForQuery(QStringLiteral("Hello"),
                                          /*matchCase=*/false, /*regex=*/false, &error);
    QVERIFY(error.isEmpty());
    QVERIFY(plan.caseSensitiveTerms.isEmpty());
}

void TestSearchOptionsRegex::bothTogglesCombine()
{
    QString error;
    auto plan = SearchView::planForQuery(QStringLiteral("tag:#work foo"),
                                          /*matchCase=*/true, /*regex=*/false, &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(plan.requiredTags, QStringList{QStringLiteral("work")});
    QCOMPARE(plan.caseSensitiveTerms, QStringList{QStringLiteral("foo")});
}

void TestSearchOptionsRegex::invalidRegexReportsError()
{
    QString error;
    auto plan = SearchView::planForQuery(QStringLiteral("h[ello"),
                                          /*matchCase=*/false, /*regex=*/true, &error);
    QVERIFY(!error.isEmpty());
    QVERIFY(plan.regexPatterns.isEmpty());
}

QTEST_MAIN(TestSearchOptionsRegex)
#include "tst_search_options_regex.moc"
