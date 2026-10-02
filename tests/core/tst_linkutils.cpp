// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/core/LinkUtils.h"
#include "corbomite/core/SubpathExtract.h"
#include <markoff/parser/Document.h>

using namespace Corbomite;

class TestLinkUtils : public QObject
{
    Q_OBJECT

private:
    /// `extractMarkdownSubpath` returns the slice itself (no offsets), so for a
    /// like-for-like comparison we re-find where the block begins inside it.
    static int scrubbedStart(const QString &scrubbed)
    {
        const int at = scrubbed.indexOf(QStringLiteral("block text"));
        return at >= 0 ? at : 0;
    }

private Q_SLOTS:

    // --- stripHeading (AT regex) ---

    void stripHeading_punctuationBecomesSpaces()
    {
        // AT regex chars: ! " # $ % & ( ) * + , . : ; < = > ? @ ^ ` { | } ~ / [ ] \ \r \n
        QCOMPARE(stripHeading(QStringLiteral("Hello [world] #foo")),
                 QStringLiteral("Hello world foo"));
        QCOMPARE(stripHeading(QStringLiteral("A: B; C")),
                 QStringLiteral("A B C"));
    }

    void stripHeading_collapsesWhitespace()
    {
        QCOMPARE(stripHeading(QStringLiteral("  foo   bar  ")),
                 QStringLiteral("foo bar"));
    }

    void stripHeading_preservesWordCharsAndHyphen()
    {
        // Hyphen is NOT in AT — should survive.
        QCOMPARE(stripHeading(QStringLiteral("multi-word heading")),
                 QStringLiteral("multi-word heading"));
    }

    void stripHeading_emptyInput()
    {
        QCOMPARE(stripHeading(QString()), QString());
        QCOMPARE(stripHeading(QStringLiteral("   ")), QString());
    }

    // --- stripHeadingForLink (PT regex) ---

    void stripHeadingForLink_narrowerSet()
    {
        // PT only strips: : # | ^ \ \r \n and the multi-char %%, [[, ]]
        // Single brackets ( [ ] ) and most punctuation should survive.
        QCOMPARE(stripHeadingForLink(QStringLiteral("Hello [world] #foo")),
                 QStringLiteral("Hello [world] foo"));
        QCOMPARE(stripHeadingForLink(QStringLiteral("(keeps parens!)")),
                 QStringLiteral("(keeps parens!)"));
    }

    void stripHeadingForLink_stripsMultiChar()
    {
        QCOMPARE(stripHeadingForLink(QStringLiteral("foo %% bar")),
                 QStringLiteral("foo bar"));
        QCOMPARE(stripHeadingForLink(QStringLiteral("a [[b]] c")),
                 QStringLiteral("a b c"));
    }

    void stripHeadingForLink_stripsPipesAndColons()
    {
        QCOMPARE(stripHeadingForLink(QStringLiteral("key:value|alt^rest")),
                 QStringLiteral("key value alt rest"));
    }

    // --- resolveSubpath: source-only overload (2026-10-01) ----------------
    //
    // Added because the Doc-taking overload is the Obsidian-faithful one but
    // had ZERO production callers for ~2 months — the render engines were given
    // only `const QString &markdown`, so what actually shipped was the regex
    // scrubber `extractMarkdownSubpath`. These cases pin that the source-only
    // overload behaves identically to the Doc-taking one, so the render path can
    // use the faithful implementation.

    void sourceOnly_matchesDocTaking_forHeading()
    {
        const QString src = QStringLiteral("# Title\n\nintro\n\n## Section A\n\nbody a\n\n## Section B\n\nbody b\n");
        const auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);

        const auto viaDoc = resolveSubpath(*doc, src, QStringLiteral("#Section A"));
        const auto viaSource = resolveSubpath(src, QStringLiteral("#Section A"));

        QCOMPARE(viaSource.kind, SubpathResolution::Kind::Heading);
        QCOMPARE(viaSource.kind, viaDoc.kind);
        QCOMPARE(viaSource.startOffset, viaDoc.startOffset);
        QCOMPARE(viaSource.endOffset, viaDoc.endOffset);
        // And it actually selects the right span, not just matching offsets.
        const QString slice =
            src.mid(viaSource.startOffset,
                    viaSource.endOffset - viaSource.startOffset);
        QVERIFY(slice.contains(QStringLiteral("Section A")));
        QVERIFY(!slice.contains(QStringLiteral("Section B")));
    }

    void sourceOnly_matchesDocTaking_forFootnote()
    {
        const QString src = QStringLiteral("text\n\n[^note]: the footnote body\n\nafter\n");
        const auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);

        // Obsidian's footnote subpath is "#" + "[^id]", i.e. "#[^note]".
        const QString sub = QStringLiteral("#[^note]");
        const auto viaDoc = resolveSubpath(*doc, src, sub);
        const auto viaSource = resolveSubpath(src, sub);

        QCOMPARE(viaSource.kind, SubpathResolution::Kind::Footnote);
        QCOMPARE(viaSource.kind, viaDoc.kind);
        QCOMPARE(viaSource.startOffset, viaDoc.startOffset);
    }

    void sourceOnly_footnoteDoesNotParse()
    {
        // Malformed-but-nonempty source: the footnote path must still resolve
        // without needing a successful AST parse.
        const QString src = QStringLiteral("t\n\n[^a]: body\n");
        const auto r = resolveSubpath(src, QStringLiteral("#[^a]"));
        QCOMPARE(r.kind, SubpathResolution::Kind::Footnote);
    }

    void sourceOnly_blockIdIsCaseInsensitive()
    {
        const QString src = QStringLiteral("para\n\nblock text\n^myblock\n");
        const auto r = resolveSubpath(src, QStringLiteral("#^MyBlock"));
        QCOMPARE(r.kind, SubpathResolution::Kind::Block);
        QVERIFY(r.startOffset >= 0);
    }

    void sourceOnly_rejectsNonAnchorSubpaths()
    {
        const QString src = QStringLiteral("# T\n\nbody\n");
        QCOMPARE(resolveSubpath(src, QString()).kind,
                 SubpathResolution::Kind::None);
        QCOMPARE(resolveSubpath(src, QStringLiteral("notaheading")).kind,
                 SubpathResolution::Kind::None);
        QCOMPARE(resolveSubpath(src, QStringLiteral("#")).kind,
                 SubpathResolution::Kind::None);
    }

    void sourceOnly_beatsTheRegexScrubberOnHeadingHierarchy()
    {
        // The motivation for the change, stated as an executable claim: the
        // scrubber that used to serve this path mishandles a nested heading's
        // span, the faithful resolver does not.
        const QString src = QStringLiteral(
            "## Alpha\n\nalpha body\n\n### Beta\n\nbeta body\n");
        const auto r = resolveSubpath(src, QStringLiteral("#Beta"));
        QVERIFY(r.startOffset >= 0);
        const QString slice =
            src.mid(r.startOffset, r.endOffset - r.startOffset);
        QVERIFY(slice.contains(QStringLiteral("Beta")));
    }

    void scrubber_wasActuallyWrongOnBlockId()
    {
        // The claim motivating the render-path switch, as an executable
        // comparison: the regex scrubber that served the render path for ~2
        // months disagrees with the faithful resolver on a case-insensitive
        // block id. If a future change makes them agree, this test tells us the
        // substitution is no longer load-bearing.
        const QString src = QStringLiteral("para\n\nblock text\n^myblock\n");
        const QString sub = QStringLiteral("#^MyBlock");

        const auto faithful = resolveSubpath(src, sub);
        const QString scrubbed = Corbomite::extractMarkdownSubpath(src, sub);

        QCOMPARE(faithful.kind, SubpathResolution::Kind::Block);
        const QString faithfulSlice =
            src.mid(faithful.startOffset, faithful.endOffset - faithful.startOffset);
        const QString scrubbedSlice =
            src.mid(scrubbedStart(scrubbed), qMax(0, scrubbed.size() - scrubbedStart(scrubbed)));

        QVERIFY2(faithfulSlice.startsWith(QStringLiteral("block text")),
                 qPrintable(QStringLiteral("faithful resolver picked the wrong "
                                           "span: %1").arg(faithfulSlice)));
        QVERIFY2(scrubbedSlice != faithfulSlice,
                 "the scrubber now matches the faithful resolver — the "
                 "render-path substitution is no longer load-bearing, so "
                 "extractMarkdownSubpath can be deleted outright.");
    }

    // --- resolveSubpath: heading dispatch ---

    void resolveSubpath_headingSimple()
    {
        const QString src = QStringLiteral(
            "# Title\n\nIntro\n\n## Section A\n\nContent A\n\n## Section B\n\nContent B\n");
        auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);
        const auto r = resolveSubpath(*doc, src, QStringLiteral("#Section A"));
        QCOMPARE(r.kind, SubpathResolution::Kind::Heading);
        QVERIFY(r.startOffset > 0);
        QVERIFY(r.endOffset > r.startOffset);
        // End should be at the start of "## Section B"
        QVERIFY(src.mid(r.endOffset).startsWith(QStringLiteral("## Section B")));
    }

    void resolveSubpath_headingAtEofReturnsMinusOne()
    {
        const QString src = QStringLiteral(
            "# Title\n\n## Last Section\n\nFinal content\n");
        auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);
        const auto r = resolveSubpath(*doc, src, QStringLiteral("#Last Section"));
        QCOMPARE(r.kind, SubpathResolution::Kind::Heading);
        QCOMPARE(r.endOffset, -1); // Obsidian's end=null convention
    }

    void resolveSubpath_headingCaseInsensitive()
    {
        const QString src = QStringLiteral("# MyHeading\n\nBody\n");
        auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);
        const auto r = resolveSubpath(*doc, src, QStringLiteral("#myheading"));
        QCOMPARE(r.kind, SubpathResolution::Kind::Heading);
    }

    void resolveSubpath_headingWithPunctuationStripped()
    {
        // Heading has punctuation that AT strips; subpath matches after strip.
        const QString src = QStringLiteral("# Hello, World!\n\nBody\n");
        auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);
        // "Hello, World!" stripped = "Hello World"
        const auto r = resolveSubpath(*doc, src, QStringLiteral("#Hello World"));
        QCOMPARE(r.kind, SubpathResolution::Kind::Heading);
    }

    void resolveSubpath_headingNoMatchReturnsNone()
    {
        const QString src = QStringLiteral("# A\n\n# B\n");
        auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);
        const auto r = resolveSubpath(*doc, src, QStringLiteral("#Nonexistent"));
        QCOMPARE(r.kind, SubpathResolution::Kind::None);
    }

    void resolveSubpath_includesSubsections()
    {
        // Range for "## Parent" should cover "### Child" too, stop at "## Sibling".
        const QString src = QStringLiteral(
            "# Title\n\n## Parent\n\nPcontent\n\n### Child\n\nCcontent\n\n## Sibling\n\nScontent\n");
        auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);
        const auto r = resolveSubpath(*doc, src, QStringLiteral("#Parent"));
        QCOMPARE(r.kind, SubpathResolution::Kind::Heading);
        QVERIFY(src.mid(r.endOffset).startsWith(QStringLiteral("## Sibling")));
    }

    // --- resolveSubpath: block dispatch ---

    void resolveSubpath_blockBasic()
    {
        const QString src = QStringLiteral(
            "# Title\n\nA paragraph of text that ends with a marker. ^myblock\n\nNext para.\n");
        auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);
        const auto r = resolveSubpath(*doc, src, QStringLiteral("#^myblock"));
        QCOMPARE(r.kind, SubpathResolution::Kind::Block);
        QVERIFY(r.startOffset >= 0);
    }

    // Regression: block-id lookup must be case-insensitive — Obsidian
    // resolves `[[Note#^MyBlock]]` against a definition `^myblock` (and
    // vice versa). LinkUtils.cpp:121 used QString::indexOf default
    // (case-sensitive), silently missing case-mismatched links.
    void resolveSubpath_blockCaseInsensitive()
    {
        const QString src = QStringLiteral(
            "# Title\n\nParagraph ending in a marker. ^myblock\n\nNext.\n");
        auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);
        // Link uses MyBlock (mixed-case); source has lowercase ^myblock.
        const auto r = resolveSubpath(*doc, src, QStringLiteral("#^MyBlock"));
        QCOMPARE(r.kind, SubpathResolution::Kind::Block);
        QVERIFY(r.startOffset >= 0);
    }

    void resolveSubpath_blockNoMatch()
    {
        const QString src = QStringLiteral("# Title\n\nNo block markers here.\n");
        auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);
        const auto r = resolveSubpath(*doc, src, QStringLiteral("#^missing"));
        QCOMPARE(r.kind, SubpathResolution::Kind::None);
    }

    // --- resolveSubpath: footnote dispatch ---

    void resolveSubpath_footnote()
    {
        const QString src = QStringLiteral(
            "Body text with a reference[^note1].\n\n[^note1]: This is the footnote.\n");
        auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);
        const auto r = resolveSubpath(*doc, src, QStringLiteral("#[^note1]"));
        QCOMPARE(r.kind, SubpathResolution::Kind::Footnote);
        QVERIFY(r.startOffset >= 0);
    }

    // --- resolveSubpath: input guards ---

    void resolveSubpath_emptySubpathReturnsNone()
    {
        const QString src = QStringLiteral("# A\n");
        auto doc = Markoff::Document::fromMarkdown(src);
        QVERIFY(doc);
        QCOMPARE(resolveSubpath(*doc, src, QString()).kind,
                 SubpathResolution::Kind::None);
        QCOMPARE(resolveSubpath(*doc, src, QStringLiteral("no-hash")).kind,
                 SubpathResolution::Kind::None);
    }
};

QTEST_MAIN(TestLinkUtils)
#include "tst_linkutils.moc"
