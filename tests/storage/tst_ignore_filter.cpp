// SPDX-License-Identifier: GPL-3.0-or-later
//
// The `userIgnoreFilters` grammar (Obsidian app/own/1306-eR.js:588). Every
// gated consumer shares one parser now: `IgnoreFilter::fromAppJson`
// (`Vault::readConfigJson("app")` → filter), used by both the graph gate
// (GraphDataBuilder::ignoreFilterFor, pinned at the graph level in
// tests/graph/tst_graphdatabuilder.cpp) and the search gate
// (SearchView::dropIgnoredPaths + executeSearch, pinned at the plugin level
// in src/plugins/search/tests/tst_search_plugin.cpp).
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include "corbomite/storage/IgnoreFilter.h"

using namespace Corbomite;

static IgnoreFilter fromAppJsonText(const char *appJson)
{
    return IgnoreFilter::fromAppJson(
        QJsonDocument::fromJson(QByteArray(appJson)).object());
}

class TestIgnoreFilter : public QObject
{
    Q_OBJECT

private slots:
    void bareEntryIsAnAnchoredCaseInsensitivePrefix()
    {
        // `tmp/` means the literal prefix "tmp/": the `*` would be escaped,
        // and a mid-path hit is not a prefix hit. Obsidian anchors bare
        // entries with ^ and matches the full vault-relative path, so an
        // entry that is meant as a glob must be written `/^tmp/im` — the
        // addendum's warning (a UI presenting globs would be silently
        // misread), pinned here on the consumer side of the contract.
        const auto ignore = IgnoreFilter::fromPatterns({QStringLiteral("tmp/")});
        QCOMPARE(ignore.patternCount(), 1);
        QVERIFY(ignore.matches(QStringLiteral("tmp/scratch.md")));
        QVERIFY(!ignore.matches(QStringLiteral("Notes/tmp/scratch.md")));
        QVERIFY(!ignore.matches(QStringLiteral("TMPNOTES/a.md")));
    }

    void slashFormIsARawUnanchoredRegex()
    {
        // The inner text is exactly what sits between the outer slashes —
        // there is no trailing-flags syntax (the `i` comes from Obsidian's
        // own construction), and the slash-form needs BOTH outer slashes, so
        // `/^archive/im` is a *bare* entry (an escaped literal prefix) while
        // `/^archive/im/` has body ^archive/im. The correct way to spell
        // "anything starting with archive" is `/^Archive/`.
        const auto ignore =
            IgnoreFilter::fromPatterns({QStringLiteral("/^archive/")});
        QCOMPARE(ignore.patternCount(), 1);
        QVERIFY(ignore.matches(QStringLiteral("archive/2024/a.md")));
        QVERIFY(ignore.matches(QStringLiteral("Archive/2024/a.md")));
        QVERIFY(!ignore.matches(QStringLiteral("same/archive/a.md")));

        const auto bare = IgnoreFilter::fromPatterns(
            {QStringLiteral("/^archive/im")});
        QCOMPARE(bare.patternCount(), 1);
        QVERIFY(bare.matches(QStringLiteral("/^archive/im/x.md")));
        QVERIFY(!bare.matches(QStringLiteral("archive/im/x.md")));

        const auto withText = IgnoreFilter::fromPatterns(
            {QStringLiteral("/^archive/im/")});
        QCOMPARE(withText.patternCount(), 1);
        QVERIFY(withText.matches(QStringLiteral("archive/im/x.md")));
        QVERIFY(!withText.matches(QStringLiteral("archive/2024/a.md")));
    }

    void emptyAndUncompilableEntriesAreDropped()
    {
        // Bare entries are regex-escaped, so a( compiles as the literal
        // prefix a( — the entry that CAN fail to compile is a raw `/…/`
        // body: Obsidian logs a bad-regex diagnostic and drops it, keeping
        // the rest. The trimmed-empty entry is dropped outright.
        QJsonArray filters;
        filters.append(QStringLiteral("  "));
        filters.append(QStringLiteral("a("));
        filters.append(QStringLiteral("ok/"));
        filters.append(QStringLiteral("/bad(/"));
        QJsonObject app;
        app.insert(QStringLiteral("userIgnoreFilters"), filters);

        const auto ignore = IgnoreFilter::fromAppJson(app);
        QCOMPARE(ignore.patternCount(), 2);
        QVERIFY(ignore.matches(QStringLiteral("a(x).md")));
        QVERIFY(ignore.matches(QStringLiteral("ok/x.md")));
        QVERIFY(!ignore.matches(QStringLiteral("notes/x.md")));
    }

    void fromAppJsonOfNonObjectIsEmpty()
    {
        QVERIFY(fromAppJsonText("null").patternCount() == 0);
        QVERIFY(fromAppJsonText("[\"tmp/\"]").patternCount() == 0);
        QVERIFY(fromAppJsonText("\"tmp/\"").patternCount() == 0);
    }

    void fromAppJsonReadsTheArrayAndSkipsNonStrings()
    {
        QJsonArray filters;
        filters.append(QStringLiteral("drafts/"));
        filters.append(7);
        filters.append(QJsonObject{{QStringLiteral("n"), 1}});
        filters.append(QStringLiteral("/^private/"));
        QJsonObject app;
        app.insert(QStringLiteral("userIgnoreFilters"), filters);
        app.insert(QStringLiteral("other"), true);

        const auto ignore = IgnoreFilter::fromAppJson(app);
        QCOMPARE(ignore.patternCount(), 2);
        QVERIFY(ignore.matches(QStringLiteral("drafts/D.md")));
        QVERIFY(ignore.matches(QStringLiteral("private confidential.md")));
        QVERIFY(!ignore.matches(QStringLiteral("Notes/a.md")));
    }

    void fromAppJsonWithOnlyOtherKeysIsEmpty()
    {
        QVERIFY(fromAppJsonText(R"({"showUnsupportedFiles": true})").patternCount() == 0);
    }
};

QTEST_MAIN(TestIgnoreFilter)
#include "tst_ignore_filter.moc"
