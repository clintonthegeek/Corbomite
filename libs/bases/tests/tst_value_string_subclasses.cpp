// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

class TestStringSubclasses : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    // ----- TagValue -----

    void testTagType()
    {
        TagValue t(QStringLiteral("#foo"));
        QCOMPARE(t.type(), QStringLiteral("Tag"));
    }

    void testTagExactMatch()
    {
        TagValue t(QStringLiteral("#foo"));
        QVERIFY(t.tagMatches(QStringLiteral("#foo")));
    }

    void testTagHierarchicalMatchParent()
    {
        TagValue child(QStringLiteral("#foo/bar"));
        // other = #foo, child = #foo/bar — #foo matches #foo/bar.
        QVERIFY(child.tagMatches(QStringLiteral("#foo")));
    }

    void testTagHierarchicalMatchChild()
    {
        TagValue parent(QStringLiteral("#foo"));
        // One-directional (Obsidian): a parent tag does NOT match a child query.
        // Only the child tag matches the parent query, not vice versa.
        QVERIFY(!parent.tagMatches(QStringLiteral("#foo/bar")));
    }

    void testTagNoPartialMatch()
    {
        TagValue t(QStringLiteral("#foobar"));
        // `#foo` must not match `#foobar` (no `/` boundary).
        QVERIFY(!t.tagMatches(QStringLiteral("#foo")));
    }

    void testTagMatchesOneDirectional()
    {
        TagValue child(QStringLiteral("#parent/child"));
        QVERIFY(child.tagMatches(QStringLiteral("#parent")));          // stored under query
        QVERIFY(child.tagMatches(QStringLiteral("#parent/child")));    // equal
        QVERIFY(!child.tagMatches(QStringLiteral("#parent/other")));   // sibling
        TagValue parent(QStringLiteral("#parent"));
        QVERIFY(!parent.tagMatches(QStringLiteral("#parent/child")));  // reverse must NOT match
    }

    // ----- LinkValue -----

    void testLinkType()
    {
        LinkValue l(QStringLiteral("target"));
        QCOMPARE(l.type(), QStringLiteral("Link"));
    }

    void testLinkToStringPlain()
    {
        LinkValue l(QStringLiteral("Page"));
        QCOMPARE(l.toString(), QStringLiteral("[[Page]]"));
    }

    void testLinkToStringWithDisplay()
    {
        LinkValue l(QStringLiteral("Page"), QString{}, QStringLiteral("Display"));
        QCOMPARE(l.toString(), QStringLiteral("[[Page|Display]]"));
    }

    void testLinkParseFromString()
    {
        auto l = LinkValue::parseFromString(QStringLiteral("[[Foo]]"));
        QVERIFY(l);
        QCOMPARE(l->data(), QStringLiteral("Foo"));
        QVERIFY(l->display().isEmpty());
    }

    void testLinkParseFromStringWithDisplay()
    {
        auto l = LinkValue::parseFromString(QStringLiteral("[[Foo|Bar]]"));
        QVERIFY(l);
        QCOMPARE(l->data(), QStringLiteral("Foo"));
        QCOMPARE(l->display(), QStringLiteral("Bar"));
    }

    void testLinkParseRejectsNonWikilink()
    {
        QVERIFY(!LinkValue::parseFromString(QStringLiteral("Foo")));
        QVERIFY(!LinkValue::parseFromString(QStringLiteral("[Foo]")));
    }

    void testLinkLooseEqualsWithString()
    {
        LinkValue l(QStringLiteral("Page"));
        StringValue s(QStringLiteral("[[Page]]"));
        QVERIFY(l.looseEquals(s));
    }

    // ----- UrlValue -----

    void testUrlType()
    {
        UrlValue u(QStringLiteral("https://example.com"));
        QCOMPARE(u.type(), QStringLiteral("URL"));
        QCOMPARE(u.toString(), QStringLiteral("https://example.com"));
    }

    // ----- IconValue -----

    void testIconType()
    {
        IconValue i(QStringLiteral("star"));
        QCOMPARE(i.type(), QStringLiteral("Icon"));
        QCOMPARE(i.toString(), QStringLiteral("star"));
    }

    // ----- ImageValue -----

    void testImageType()
    {
        ImageValue i(QStringLiteral("img.png"));
        QCOMPARE(i.type(), QStringLiteral("Image"));
    }

    // ----- HTMLValue / MarkdownValue -----

    void testHtmlType()
    {
        HTMLValue h(QStringLiteral("<b>x</b>"));
        QCOMPARE(h.type(), QStringLiteral("HTML"));
    }

    void testMarkdownType()
    {
        MarkdownValue m(QStringLiteral("# Heading"));
        QCOMPARE(m.type(), QStringLiteral("Markdown"));
    }

    // ----- FormulaErrorValue -----

    void testErrorType()
    {
        FormulaErrorValue e(QStringLiteral("oops"));
        QCOMPARE(e.type(), QStringLiteral("Error"));
        QVERIFY(!e.isTruthy());
        QCOMPARE(e.toString(), QStringLiteral("oops"));
        QCOMPARE(e.message(), QStringLiteral("oops"));
    }
};

QTEST_APPLESS_MAIN(TestStringSubclasses)
#include "tst_value_string_subclasses.moc"
