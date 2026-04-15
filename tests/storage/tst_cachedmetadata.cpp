// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "corbomite/storage/CachedMetadata.h"

using namespace Corbomite;

namespace {

Position makePosition(int startLine, int startCol, int startOffset,
                      int endLine, int endCol, int endOffset)
{
    Position p;
    p.start = Pos{startLine, startCol, startOffset};
    p.end = Pos{endLine, endCol, endOffset};
    return p;
}

} // namespace

class TestCachedMetadata : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testEmptyRoundTrip()
    {
        CachedMetadata c{};
        const QJsonObject j = toJson(c);
        QCOMPARE(j, QJsonObject{});

        const CachedMetadata c2 = fromJson(j);
        QVERIFY(!c2.links.has_value());
        QVERIFY(!c2.embeds.has_value());
        QVERIFY(!c2.tags.has_value());
        QVERIFY(!c2.headings.has_value());
        QVERIFY(!c2.sections.has_value());
        QVERIFY(!c2.listItems.has_value());
        QVERIFY(!c2.footnoteRefs.has_value());
        QVERIFY(!c2.footnotes.has_value());
        QVERIFY(!c2.blocks.has_value());
        QVERIFY(!c2.frontmatter.has_value());
        QVERIFY(!c2.frontmatterLinks.has_value());
        QVERIFY(!c2.frontmatterPosition.has_value());
    }

    void testFullShapeRoundTrip()
    {
        CachedMetadata c;

        // Link at (1,0)-(1,10)
        LinkCache link;
        link.link = QStringLiteral("Note");
        link.original = QStringLiteral("[[Note|Alias]]");
        link.displayText = QStringLiteral("Alias");
        link.position = makePosition(1, 0, 0, 1, 10, 10);
        c.links = QVector<LinkCache>{link};

        // Embed
        LinkCache embed;
        embed.link = QStringLiteral("image.png");
        embed.original = QStringLiteral("![[image.png]]");
        embed.position = makePosition(2, 0, 20, 2, 14, 34);
        c.embeds = QVector<LinkCache>{embed};

        // Tag #foo
        TagCache tag;
        tag.tag = QStringLiteral("#foo");
        tag.position = makePosition(3, 0, 40, 3, 4, 44);
        c.tags = QVector<TagCache>{tag};

        // Heading "## Hi" level 2
        HeadingCache heading;
        heading.heading = QStringLiteral("Hi");
        heading.level = 2;
        heading.position = makePosition(4, 0, 50, 4, 5, 55);
        c.headings = QVector<HeadingCache>{heading};

        // One section of each type (12 total including Unknown)
        const SectionCache::Type allTypes[] = {
            SectionCache::Type::Paragraph,
            SectionCache::Type::Heading,
            SectionCache::Type::List,
            SectionCache::Type::Code,
            SectionCache::Type::Callout,
            SectionCache::Type::Yaml,
            SectionCache::Type::Table,
            SectionCache::Type::Math,
            SectionCache::Type::Html,
            SectionCache::Type::Blockquote,
            SectionCache::Type::ThematicBreak,
        };
        QVector<SectionCache> sections;
        int lineCounter = 5;
        for (SectionCache::Type t : allTypes) {
            SectionCache s;
            s.type = t;
            s.position = makePosition(lineCounter, 0, 0, lineCounter, 1, 1);
            sections.append(s);
            ++lineCounter;
        }
        c.sections = sections;

        // Nested list items with task markers
        ListItemCache li0;
        li0.position = makePosition(20, 0, 100, 20, 5, 105);
        li0.parent = -1;
        li0.task = QStringLiteral(" ");
        ListItemCache li1;
        li1.position = makePosition(21, 2, 110, 21, 7, 115);
        li1.parent = 0;
        li1.task = QStringLiteral("x");
        c.listItems = QVector<ListItemCache>{li0, li1};

        // Footnote definition
        FootnoteCache fn;
        fn.id = QStringLiteral("fn1");
        fn.position = makePosition(30, 0, 200, 30, 10, 210);
        c.footnotes = QVector<FootnoteCache>{fn};

        // Footnote ref
        FootnoteCache fnRef;
        fnRef.id = QStringLiteral("fn1");
        fnRef.position = makePosition(31, 5, 220, 31, 10, 225);
        c.footnoteRefs = QVector<FootnoteCache>{fnRef};

        // Block ^myblock
        BlockCache bc;
        bc.id = QStringLiteral("myblock");
        bc.position = makePosition(40, 0, 300, 40, 8, 308);
        QHash<QString, BlockCache> blocks;
        blocks.insert(QStringLiteral("myblock"), bc);
        c.blocks = blocks;

        // Frontmatter {"title":"Hi"}
        QJsonObject fm;
        fm.insert(QStringLiteral("title"), QStringLiteral("Hi"));
        c.frontmatter = fm;

        // Frontmatter link with dotted key "related.0"
        FrontmatterLinkCache fml;
        fml.link = QStringLiteral("Other");
        fml.original = QStringLiteral("[[Other]]");
        fml.key = QStringLiteral("related.0");
        c.frontmatterLinks = QVector<FrontmatterLinkCache>{fml};

        // Frontmatter position
        c.frontmatterPosition = makePosition(0, 0, 0, 2, 3, 20);

        // Round-trip
        const QJsonObject j = toJson(c);
        const CachedMetadata r = fromJson(j);

        // Field-by-field equality.
        QVERIFY(r.links.has_value());
        QCOMPARE(r.links->size(), 1);
        QCOMPARE(r.links->at(0).link, link.link);
        QCOMPARE(r.links->at(0).original, link.original);
        QVERIFY(r.links->at(0).displayText.has_value());
        QCOMPARE(*r.links->at(0).displayText, *link.displayText);
        QCOMPARE(r.links->at(0).position, link.position);

        QVERIFY(r.embeds.has_value());
        QCOMPARE(r.embeds->size(), 1);
        QCOMPARE(r.embeds->at(0).link, embed.link);
        QCOMPARE(r.embeds->at(0).original, embed.original);
        QVERIFY(!r.embeds->at(0).displayText.has_value());

        QVERIFY(r.tags.has_value());
        QCOMPARE(r.tags->size(), 1);
        QCOMPARE(r.tags->at(0).tag, tag.tag);
        QCOMPARE(r.tags->at(0).position, tag.position);

        QVERIFY(r.headings.has_value());
        QCOMPARE(r.headings->size(), 1);
        QCOMPARE(r.headings->at(0).heading, heading.heading);
        QCOMPARE(r.headings->at(0).level, heading.level);
        QCOMPARE(r.headings->at(0).position, heading.position);

        QVERIFY(r.sections.has_value());
        QCOMPARE(r.sections->size(), sections.size());
        for (int i = 0; i < sections.size(); ++i) {
            QCOMPARE(r.sections->at(i).type, sections.at(i).type);
            QCOMPARE(r.sections->at(i).position, sections.at(i).position);
        }

        QVERIFY(r.listItems.has_value());
        QCOMPARE(r.listItems->size(), 2);
        QCOMPARE(r.listItems->at(0).parent, -1);
        QVERIFY(r.listItems->at(0).task.has_value());
        QCOMPARE(*r.listItems->at(0).task, QStringLiteral(" "));
        QCOMPARE(r.listItems->at(1).parent, 0);
        QCOMPARE(*r.listItems->at(1).task, QStringLiteral("x"));

        QVERIFY(r.footnotes.has_value());
        QCOMPARE(r.footnotes->size(), 1);
        QCOMPARE(r.footnotes->at(0).id, fn.id);
        QCOMPARE(r.footnotes->at(0).position, fn.position);

        QVERIFY(r.footnoteRefs.has_value());
        QCOMPARE(r.footnoteRefs->size(), 1);
        QCOMPARE(r.footnoteRefs->at(0).id, fnRef.id);

        QVERIFY(r.blocks.has_value());
        QCOMPARE(r.blocks->size(), 1);
        QVERIFY(r.blocks->contains(QStringLiteral("myblock")));
        QCOMPARE(r.blocks->value(QStringLiteral("myblock")).id, bc.id);
        QCOMPARE(r.blocks->value(QStringLiteral("myblock")).position, bc.position);

        QVERIFY(r.frontmatter.has_value());
        QCOMPARE(r.frontmatter->value(QStringLiteral("title")).toString(),
                 QStringLiteral("Hi"));

        QVERIFY(r.frontmatterLinks.has_value());
        QCOMPARE(r.frontmatterLinks->size(), 1);
        QCOMPARE(r.frontmatterLinks->at(0).link, fml.link);
        QCOMPARE(r.frontmatterLinks->at(0).key, fml.key);

        QVERIFY(r.frontmatterPosition.has_value());
        QCOMPARE(*r.frontmatterPosition, *c.frontmatterPosition);
    }

    void testPersistedRenameFrontmatterPos()
    {
        CachedMetadata c;
        c.frontmatterPosition = makePosition(0, 0, 0, 1, 2, 3);

        const QJsonObject persisted = toPersistedJson(c);
        QVERIFY(persisted.contains(QStringLiteral("frontmatterPos")));
        QVERIFY(!persisted.contains(QStringLiteral("frontmatterPosition")));

        const CachedMetadata r = fromPersistedJson(persisted);
        QVERIFY(r.frontmatterPosition.has_value());
        QCOMPARE(*r.frontmatterPosition, *c.frontmatterPosition);
    }

    void testSectionTypeUnknownFutureValue()
    {
        QJsonObject posObj;
        QJsonObject startObj;
        startObj.insert(QStringLiteral("line"), 0);
        startObj.insert(QStringLiteral("col"), 0);
        startObj.insert(QStringLiteral("offset"), 0);
        QJsonObject endObj;
        endObj.insert(QStringLiteral("line"), 0);
        endObj.insert(QStringLiteral("col"), 5);
        endObj.insert(QStringLiteral("offset"), 5);
        posObj.insert(QStringLiteral("start"), startObj);
        posObj.insert(QStringLiteral("end"), endObj);

        QJsonObject sectionObj;
        sectionObj.insert(QStringLiteral("type"), QStringLiteral("some-new-obsidian-type"));
        sectionObj.insert(QStringLiteral("position"), posObj);

        QJsonArray arr;
        arr.append(sectionObj);

        QJsonObject j;
        j.insert(QStringLiteral("sections"), arr);

        const CachedMetadata c = fromJson(j);
        QVERIFY(c.sections.has_value());
        QCOMPARE(c.sections->size(), 1);
        QCOMPARE(c.sections->at(0).type, SectionCache::Type::Unknown);
        QCOMPARE(c.sections->at(0).rawType, QStringLiteral("some-new-obsidian-type"));

        const QJsonObject roundTripped = toJson(c);
        const QJsonArray rtSections = roundTripped.value(QStringLiteral("sections")).toArray();
        QCOMPARE(rtSections.size(), 1);
        QCOMPARE(rtSections.at(0).toObject().value(QStringLiteral("type")).toString(),
                 QStringLiteral("some-new-obsidian-type"));
    }

    void testOptionalFieldsOmittedWhenEmpty()
    {
        CachedMetadata c;
        LinkCache l;
        l.link = QStringLiteral("Target");
        l.original = QStringLiteral("[[Target]]");
        l.position = makePosition(0, 0, 0, 0, 10, 10);
        c.links = QVector<LinkCache>{l};

        const QJsonObject j = toJson(c);
        QVERIFY(j.contains(QStringLiteral("links")));
        QVERIFY(!j.contains(QStringLiteral("embeds")));
        QVERIFY(!j.contains(QStringLiteral("tags")));
        QVERIFY(!j.contains(QStringLiteral("headings")));
        QVERIFY(!j.contains(QStringLiteral("sections")));
        QVERIFY(!j.contains(QStringLiteral("listItems")));
        QVERIFY(!j.contains(QStringLiteral("footnoteRefs")));
        QVERIFY(!j.contains(QStringLiteral("footnotes")));
        QVERIFY(!j.contains(QStringLiteral("blocks")));
        QVERIFY(!j.contains(QStringLiteral("frontmatter")));
        QVERIFY(!j.contains(QStringLiteral("frontmatterLinks")));
        QVERIFY(!j.contains(QStringLiteral("frontmatterPosition")));
        QVERIFY(!j.contains(QStringLiteral("frontmatterPos")));
    }

    void testEnumStringMapping()
    {
        struct Case
        {
            SectionCache::Type type;
            QString expected;
        };
        const QVector<Case> cases = {
            {SectionCache::Type::Paragraph, QStringLiteral("paragraph")},
            {SectionCache::Type::Heading, QStringLiteral("heading")},
            {SectionCache::Type::List, QStringLiteral("list")},
            {SectionCache::Type::Code, QStringLiteral("code")},
            {SectionCache::Type::Callout, QStringLiteral("callout")},
            {SectionCache::Type::Yaml, QStringLiteral("yaml")},
            {SectionCache::Type::Table, QStringLiteral("table")},
            {SectionCache::Type::Math, QStringLiteral("math")},
            {SectionCache::Type::Html, QStringLiteral("html")},
            {SectionCache::Type::Blockquote, QStringLiteral("blockquote")},
            {SectionCache::Type::ThematicBreak, QStringLiteral("thematicBreak")},
        };

        CachedMetadata c;
        QVector<SectionCache> sections;
        for (const Case &cs : cases) {
            SectionCache s;
            s.type = cs.type;
            s.position = makePosition(0, 0, 0, 0, 1, 1);
            sections.append(s);
        }
        c.sections = sections;

        const QJsonObject j = toJson(c);
        const QJsonArray arr = j.value(QStringLiteral("sections")).toArray();
        QCOMPARE(arr.size(), cases.size());
        for (int i = 0; i < cases.size(); ++i) {
            QCOMPARE(arr.at(i).toObject().value(QStringLiteral("type")).toString(),
                     cases.at(i).expected);
        }

        // Round-trip and assert the enum values come back.
        const CachedMetadata r = fromJson(j);
        QVERIFY(r.sections.has_value());
        QCOMPARE(r.sections->size(), cases.size());
        for (int i = 0; i < cases.size(); ++i)
            QCOMPARE(r.sections->at(i).type, cases.at(i).type);
    }

    void testBlocksSerialisedAsJsonObjectKeyedById()
    {
        CachedMetadata c;
        QHash<QString, BlockCache> blocks;
        BlockCache a;
        a.id = QStringLiteral("alpha");
        a.position = makePosition(1, 0, 10, 1, 5, 15);
        BlockCache b;
        b.id = QStringLiteral("beta");
        b.position = makePosition(2, 0, 20, 2, 4, 24);
        blocks.insert(QStringLiteral("alpha"), a);
        blocks.insert(QStringLiteral("beta"), b);
        c.blocks = blocks;

        const QJsonObject j = toJson(c);
        QVERIFY(j.value(QStringLiteral("blocks")).isObject());
        const QJsonObject blocksObj = j.value(QStringLiteral("blocks")).toObject();
        QVERIFY(blocksObj.contains(QStringLiteral("alpha")));
        QVERIFY(blocksObj.contains(QStringLiteral("beta")));

        const QJsonObject alpha = blocksObj.value(QStringLiteral("alpha")).toObject();
        QVERIFY(alpha.contains(QStringLiteral("id")));
        QVERIFY(alpha.contains(QStringLiteral("position")));
        QCOMPARE(alpha.value(QStringLiteral("id")).toString(), QStringLiteral("alpha"));

        const QJsonObject beta = blocksObj.value(QStringLiteral("beta")).toObject();
        QVERIFY(beta.contains(QStringLiteral("id")));
        QVERIFY(beta.contains(QStringLiteral("position")));
        QCOMPARE(beta.value(QStringLiteral("id")).toString(), QStringLiteral("beta"));
    }

    void testOptionalDisplayTextAbsentWhenNullopt()
    {
        CachedMetadata c;
        LinkCache l;
        l.link = QStringLiteral("Note");
        l.original = QStringLiteral("[[Note]]");
        l.displayText = std::nullopt;
        l.position = makePosition(0, 0, 0, 0, 8, 8);
        c.links = QVector<LinkCache>{l};

        const QJsonObject j = toJson(c);
        const QJsonArray arr = j.value(QStringLiteral("links")).toArray();
        QCOMPARE(arr.size(), 1);
        const QJsonObject linkObj = arr.at(0).toObject();
        QVERIFY(!linkObj.contains(QStringLiteral("displayText")));

        // Round-trip: still nullopt.
        const CachedMetadata r = fromJson(j);
        QVERIFY(r.links.has_value());
        QVERIFY(!r.links->at(0).displayText.has_value());
    }

    void testNestedListItemParentRoundTrip()
    {
        CachedMetadata c;
        ListItemCache top;
        top.position = makePosition(0, 0, 0, 0, 10, 10);
        top.parent = -1;
        top.task = QStringLiteral(" ");
        ListItemCache child;
        child.position = makePosition(1, 2, 20, 1, 12, 30);
        child.parent = 0;
        child.task = QStringLiteral("x");
        c.listItems = QVector<ListItemCache>{top, child};

        const QJsonObject j = toJson(c);
        const CachedMetadata r = fromJson(j);
        QVERIFY(r.listItems.has_value());
        QCOMPARE(r.listItems->size(), 2);
        QCOMPARE(r.listItems->at(0).parent, -1);
        QCOMPARE(r.listItems->at(1).parent, 0);
    }
};

QTEST_MAIN(TestCachedMetadata)
#include "tst_cachedmetadata.moc"
