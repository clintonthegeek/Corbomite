// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QString>

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataParser.h"

using namespace Corbomite;

namespace {

QByteArray md(const char *literal)
{
    return QByteArray(literal);
}

LinkResolver makeResolver(const QStringList &paths)
{
    LinkResolver r;
    r.setVaultPaths(paths);
    return r;
}

} // namespace

class TestMetadataParser : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // 1. Empty content -> canonical empty-SHA-256 hash; every field nullopt.
    void testParseEmpty()
    {
        LinkResolver resolver;
        ParsedNote r = MetadataParser::parse(QByteArray(), QStringLiteral("note.md"),
                                             resolver);
        QCOMPARE(r.hash,
                 QStringLiteral(
                     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));

        QVERIFY(!r.cache.links.has_value());
        QVERIFY(!r.cache.embeds.has_value());
        QVERIFY(!r.cache.tags.has_value());
        QVERIFY(!r.cache.headings.has_value());
        QVERIFY(!r.cache.sections.has_value());
        QVERIFY(!r.cache.listItems.has_value());
        QVERIFY(!r.cache.footnoteRefs.has_value());
        QVERIFY(!r.cache.footnotes.has_value());
        QVERIFY(!r.cache.blocks.has_value());
        QVERIFY(!r.cache.frontmatter.has_value());
        QVERIFY(!r.cache.frontmatterLinks.has_value());
        QVERIFY(!r.cache.frontmatterPosition.has_value());
    }

    // 2. Headings at three levels.
    void testParseHeadingsOnly()
    {
        LinkResolver resolver;
        ParsedNote r = MetadataParser::parse(md("# H1\n## H2\n### H3\n"),
                                             QStringLiteral("n.md"), resolver);
        QVERIFY(r.cache.headings.has_value());
        QCOMPARE(r.cache.headings->size(), 3);
        QCOMPARE(r.cache.headings->at(0).level, 1);
        QCOMPARE(r.cache.headings->at(1).level, 2);
        QCOMPARE(r.cache.headings->at(2).level, 3);
        QCOMPARE(r.cache.headings->at(0).heading, QStringLiteral("H1"));
        QCOMPARE(r.cache.headings->at(1).heading, QStringLiteral("H2"));
        QCOMPARE(r.cache.headings->at(2).heading, QStringLiteral("H3"));
        // Positions: H1 line 0, H2 line 1, H3 line 2.
        QCOMPARE(r.cache.headings->at(0).position.start.line, 0);
        QCOMPARE(r.cache.headings->at(1).position.start.line, 1);
        QCOMPARE(r.cache.headings->at(2).position.start.line, 2);
        QCOMPARE(r.cache.headings->at(0).position.start.col, 0);
    }

    // 3. Wikilink with subpath.
    void testParseWikilinkWithSubpath()
    {
        LinkResolver resolver =
            makeResolver({QStringLiteral("Target.md")});
        ParsedNote r = MetadataParser::parse(md("[[Target#section]]\n"),
                                             QStringLiteral("note.md"), resolver);
        QVERIFY(r.cache.links.has_value());
        QCOMPARE(r.cache.links->size(), 1);
        QCOMPARE(r.cache.links->at(0).link,
                 QStringLiteral("Target.md#section"));
    }

    // 4. Wikilink with alias.
    void testParseWikilinkWithAlias()
    {
        LinkResolver resolver =
            makeResolver({QStringLiteral("Target.md")});
        ParsedNote r = MetadataParser::parse(md("[[Target|Display Text]]\n"),
                                             QStringLiteral("note.md"), resolver);
        QVERIFY(r.cache.links.has_value());
        QCOMPARE(r.cache.links->size(), 1);
        const LinkCache &lc = r.cache.links->at(0);
        QCOMPARE(lc.link, QStringLiteral("Target.md"));
        QVERIFY(lc.displayText.has_value());
        QCOMPARE(*lc.displayText, QStringLiteral("Display Text"));
        QCOMPARE(lc.original, QStringLiteral("Target|Display Text"));
    }

    // 5. Embed vs link split.
    void testParseEmbedVsLink()
    {
        LinkResolver resolver =
            makeResolver({QStringLiteral("Note.md"), QStringLiteral("Image.png")});
        ParsedNote r = MetadataParser::parse(md("[[Note]]\n![[Image.png]]\n"),
                                             QStringLiteral("note.md"), resolver);
        QVERIFY(r.cache.links.has_value());
        QVERIFY(r.cache.embeds.has_value());
        QCOMPARE(r.cache.links->size(), 1);
        QCOMPARE(r.cache.embeds->size(), 1);
    }

    // 6. Inline tags.
    void testParseTagsInline()
    {
        LinkResolver resolver;
        ParsedNote r = MetadataParser::parse(md("#foo #bar/baz\n"),
                                             QStringLiteral("n.md"), resolver);
        QVERIFY(r.cache.tags.has_value());
        QCOMPARE(r.cache.tags->size(), 2);
        QSet<QString> found;
        for (const TagCache &t : *r.cache.tags)
            found.insert(t.tag);
        QVERIFY(found.contains(QStringLiteral("#foo")));
        QVERIFY(found.contains(QStringLiteral("#bar/baz")));
    }

    // 7. Frontmatter tags array merged with inline tags.
    void testParseFrontmatterTagsArrayMerged()
    {
        LinkResolver resolver;
        ParsedNote r = MetadataParser::parse(
            md("---\ntags: [alpha, beta]\n---\n#inline\n"),
            QStringLiteral("n.md"), resolver);
        QVERIFY(r.cache.tags.has_value());
        QCOMPARE(r.cache.tags->size(), 3);
        QSet<QString> found;
        for (const TagCache &t : *r.cache.tags)
            found.insert(t.tag);
        QCOMPARE(found,
                 (QSet<QString>{QStringLiteral("#alpha"), QStringLiteral("#beta"),
                                QStringLiteral("#inline")}));
    }

    // 8. Frontmatter singular `tag:` string.
    void testParseFrontmatterTagSingularString()
    {
        LinkResolver resolver;
        ParsedNote r = MetadataParser::parse(md("---\ntag: single\n---\n"),
                                             QStringLiteral("n.md"), resolver);
        QVERIFY(r.cache.tags.has_value());
        QCOMPARE(r.cache.tags->size(), 1);
        QCOMPARE(r.cache.tags->at(0).tag, QStringLiteral("#single"));
    }

    // 9. Frontmatter links with dotted-key paths.
    void testParseFrontmatterLinksDottedKey()
    {
        LinkResolver resolver;
        ParsedNote r = MetadataParser::parse(
            md("---\n"
               "project: \"[[Project A]]\"\n"
               "related: [\"[[Note 1]]\", \"[[Note 2]]\"]\n"
               "nested:\n"
               "  key: \"[[Deep]]\"\n"
               "---\n"),
            QStringLiteral("n.md"), resolver);
        QVERIFY(r.cache.frontmatterLinks.has_value());
        QCOMPARE(r.cache.frontmatterLinks->size(), 4);
        QSet<QString> keys;
        for (const FrontmatterLinkCache &f : *r.cache.frontmatterLinks)
            keys.insert(f.key);
        QCOMPARE(keys,
                 (QSet<QString>{QStringLiteral("project"),
                                QStringLiteral("related.0"),
                                QStringLiteral("related.1"),
                                QStringLiteral("nested.key")}));
    }

    // 10. BlockId syntax.
    void testParseBlockIdSyntax()
    {
        LinkResolver resolver;
        ParsedNote r = MetadataParser::parse(md("Paragraph.\n^myblock\n"),
                                             QStringLiteral("n.md"), resolver);
        QVERIFY(r.cache.blocks.has_value());
        QCOMPARE(r.cache.blocks->size(), 1);
        QVERIFY(r.cache.blocks->contains(QStringLiteral("myblock")));
        QCOMPARE(r.cache.blocks->value(QStringLiteral("myblock")).id,
                 QStringLiteral("myblock"));
    }

    // 11. Footnote definition + reference.
    void testParseFootnoteDefAndRef()
    {
        LinkResolver resolver;
        ParsedNote r = MetadataParser::parse(
            md("See [^1].\n\n[^1]: A definition.\n"), QStringLiteral("n.md"),
            resolver);
        QVERIFY(r.cache.footnoteRefs.has_value());
        QCOMPARE(r.cache.footnoteRefs->size(), 1);
        QCOMPARE(r.cache.footnoteRefs->at(0).id, QStringLiteral("1"));
        QVERIFY(r.cache.footnotes.has_value());
        QCOMPARE(r.cache.footnotes->size(), 1);
        QCOMPARE(r.cache.footnotes->at(0).id, QStringLiteral("1"));
    }

    // 12. Callout becomes a Callout section.
    void testParseCalloutIsSection()
    {
        LinkResolver resolver;
        ParsedNote r = MetadataParser::parse(
            md("# H\n\n> [!note] Title\n> Body\n"), QStringLiteral("n.md"),
            resolver);
        QVERIFY(r.cache.sections.has_value());
        bool sawCallout = false;
        for (const SectionCache &s : *r.cache.sections) {
            if (s.type == SectionCache::Type::Callout)
                sawCallout = true;
        }
        QVERIFY(sawCallout);
    }

    // 13. Nested task list-items.
    void testParseListItemsNestedTask()
    {
        LinkResolver resolver;
        ParsedNote r = MetadataParser::parse(md("- [ ] Top\n  - [x] Nested\n"),
                                             QStringLiteral("n.md"), resolver);
        QVERIFY(r.cache.listItems.has_value());
        QCOMPARE(r.cache.listItems->size(), 2);
        QVERIFY(r.cache.listItems->at(0).task.has_value());
        QCOMPARE(*r.cache.listItems->at(0).task, QStringLiteral(" "));
        QVERIFY(r.cache.listItems->at(1).task.has_value());
        QCOMPARE(*r.cache.listItems->at(1).task, QStringLiteral("x"));
        QCOMPARE(r.cache.listItems->at(0).parent, -1);
        QCOMPARE(r.cache.listItems->at(1).parent, 0);
    }

    // 14. Identical bytes -> identical hash.
    void testParseIdenticalContentSameHash()
    {
        LinkResolver resolver;
        QByteArray bytes = md("# Hi\n\nHello world.\n");
        ParsedNote a = MetadataParser::parse(bytes, QStringLiteral("n.md"),
                                             resolver);
        ParsedNote b = MetadataParser::parse(bytes, QStringLiteral("n.md"),
                                             resolver);
        QCOMPARE(a.hash, b.hash);
        QCOMPARE(a.hash.size(), 64);
    }

    // 15. One byte different -> different hash.
    void testParseDifferentContentDifferentHash()
    {
        LinkResolver resolver;
        QByteArray a = md("Hello.\n");
        QByteArray b = md("Hello!\n");
        ParsedNote ra = MetadataParser::parse(a, QStringLiteral("n.md"), resolver);
        ParsedNote rb = MetadataParser::parse(b, QStringLiteral("n.md"), resolver);
        QVERIFY(ra.hash != rb.hash);
    }

    // 16. Heading position line/col/offset correctness.
    void testParseHeadingPositionLineColumn()
    {
        LinkResolver resolver;
        // Fixture: "Hello\n# Heading\n"
        //   H starts at line 1, col 0, offset 6.
        //   H ends at line 1, col 9, offset 15 (length 9).
        ParsedNote r = MetadataParser::parse(md("Hello\n# Heading\n"),
                                             QStringLiteral("n.md"), resolver);
        QVERIFY(r.cache.headings.has_value());
        QCOMPARE(r.cache.headings->size(), 1);
        const HeadingCache &h = r.cache.headings->at(0);
        QCOMPARE(h.position.start.line, 1);
        QCOMPARE(h.position.start.col, 0);
        QCOMPARE(h.position.start.offset, 6);
        QCOMPARE(h.position.end.line, 1);
        QCOMPARE(h.position.end.offset, 15);
    }

    // 17. Unresolved wikilink passes through.
    void testUnresolvedLinkPassthrough()
    {
        LinkResolver resolver; // empty
        ParsedNote r = MetadataParser::parse(md("[[DoesNotExist]]\n"),
                                             QStringLiteral("n.md"), resolver);
        QVERIFY(r.cache.links.has_value());
        QCOMPARE(r.cache.links->size(), 1);
        QCOMPARE(r.cache.links->at(0).link, QStringLiteral("DoesNotExist"));
    }

    // 18. OffsetToPosConverter (invoked via parse) — monotonic + non-monotonic.
    // The converter is file-local; exercise it through parse() by asking for
    // positions at three specific offsets and checking line/col correctness,
    // then a lower-offset query.
    void testOffsetToPosConverterMonotonic()
    {
        LinkResolver resolver;
        // Source: "#a\n#b\n#c/d\n" — three tags on three lines.
        ParsedNote r = MetadataParser::parse(md("#a\n#b\n#c/d\n"),
                                             QStringLiteral("n.md"), resolver);
        QVERIFY(r.cache.tags.has_value());
        QCOMPARE(r.cache.tags->size(), 3);
        // Monotonic order: lines 0, 1, 2.
        QCOMPARE(r.cache.tags->at(0).position.start.line, 0);
        QCOMPARE(r.cache.tags->at(0).position.start.col, 0);
        QCOMPARE(r.cache.tags->at(1).position.start.line, 1);
        QCOMPARE(r.cache.tags->at(1).position.start.col, 0);
        QCOMPARE(r.cache.tags->at(2).position.start.line, 2);
        QCOMPARE(r.cache.tags->at(2).position.start.col, 0);

        // Non-monotonic: the heading pass happens before the link/tag/etc.
        // passes. We construct a fixture where offsets interleave across
        // fields so the shared converter would see a non-monotonic call,
        // but parse() isolates headings to their own pass. We instead
        // directly exercise the non-monotonic fallback by parsing a doc
        // whose frontmatter-position-computation must precede the body
        // passes (frontmatterPosition uses a separate converter). The best
        // observable check: parse a doc with content spread across many
        // lines and confirm later-field positions remain correct even when
        // the heading pass has advanced the converter far forward.
        ParsedNote r2 = MetadataParser::parse(
            md("# H\n\n- one\n- two\n\n#tag\n"),
            QStringLiteral("n.md"), resolver);
        // The tag is on line 5 (0-indexed).
        QVERIFY(r2.cache.tags.has_value());
        QCOMPARE(r2.cache.tags->size(), 1);
        QCOMPARE(r2.cache.tags->at(0).position.start.line, 5);
        // The list items are on lines 2 and 3 — these are produced AFTER
        // the heading-pass has already advanced the main converter to
        // wherever the heading ends. But listItems uses its own converter
        // (liConv), so positions remain correct regardless of ordering.
        QVERIFY(r2.cache.listItems.has_value());
        QCOMPARE(r2.cache.listItems->size(), 2);
        QCOMPARE(r2.cache.listItems->at(0).position.start.line, 2);
        QCOMPARE(r2.cache.listItems->at(1).position.start.line, 3);
    }
};

QTEST_MAIN(TestMetadataParser)
#include "tst_metadataparser.moc"
