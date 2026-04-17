// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QJsonArray>
#include <QJsonObject>

#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

class TestObjectAndRegex : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    // ----- ObjectValue -----

    void testObjectType()
    {
        ObjectValue o;
        QCOMPARE(o.type(), QStringLiteral("Object"));
        QVERIFY(o.isEmpty());
    }

    void testObjectInsertionOrderPreserved()
    {
        ObjectValue o;
        o.set(QStringLiteral("b"), std::make_shared<NumberValue>(1));
        o.set(QStringLiteral("a"), std::make_shared<NumberValue>(2));
        o.set(QStringLiteral("c"), std::make_shared<NumberValue>(3));
        const auto ks = o.keys();
        QCOMPARE(ks.size(), 3);
        QCOMPARE(ks[0], QStringLiteral("b"));
        QCOMPARE(ks[1], QStringLiteral("a"));
        QCOMPARE(ks[2], QStringLiteral("c"));
    }

    void testObjectCaseInsensitiveLookup()
    {
        ObjectValue o;
        o.set(QStringLiteral("Status"), std::make_shared<StringValue>(QStringLiteral("open")));
        auto v = o.getInsensitive(QStringLiteral("status"));
        QVERIFY(v);
        QCOMPARE(v->toString(), QStringLiteral("open"));
        v = o.objectAccess(QStringLiteral("STATUS"));
        QCOMPARE(v->toString(), QStringLiteral("open"));
    }

    void testFromFrontMatterBasicTypes()
    {
        QJsonObject fm;
        fm[QStringLiteral("s")] = QStringLiteral("hello");
        fm[QStringLiteral("n")] = 42;
        fm[QStringLiteral("b")] = true;
        fm[QStringLiteral("nothing")] = QJsonValue::Null;
        auto o = ObjectValue::fromFrontMatter(fm);
        QCOMPARE(o->get(QStringLiteral("s"))->type(), QStringLiteral("String"));
        QCOMPARE(o->get(QStringLiteral("n"))->type(), QStringLiteral("Number"));
        QCOMPARE(o->get(QStringLiteral("b"))->type(), QStringLiteral("Boolean"));
        QCOMPARE(o->get(QStringLiteral("nothing"))->type(), QStringLiteral("Null"));
    }

    void testFromFrontMatterCoercesLink()
    {
        QJsonObject fm;
        fm[QStringLiteral("ref")] = QStringLiteral("[[Foo]]");
        auto o = ObjectValue::fromFrontMatter(fm);
        auto v = o->get(QStringLiteral("ref"));
        QCOMPARE(v->type(), QStringLiteral("Link"));
    }

    void testFromFrontMatterCoercesUrl()
    {
        QJsonObject fm;
        fm[QStringLiteral("site")] = QStringLiteral("https://example.com");
        auto o = ObjectValue::fromFrontMatter(fm);
        QCOMPARE(o->get(QStringLiteral("site"))->type(), QStringLiteral("URL"));
    }

    void testFromFrontMatterCoercesDate()
    {
        QJsonObject fm;
        fm[QStringLiteral("due")] = QStringLiteral("2024-06-15");
        auto o = ObjectValue::fromFrontMatter(fm);
        QCOMPARE(o->get(QStringLiteral("due"))->type(), QStringLiteral("Date"));
    }

    void testFromFrontMatterTagsSpecialCase()
    {
        QJsonObject fm;
        QJsonArray tags;
        tags.append(QStringLiteral("#a"));
        tags.append(QStringLiteral("#b/c"));
        fm[QStringLiteral("tags")] = tags;
        auto o = ObjectValue::fromFrontMatter(fm);
        auto l = o->get(QStringLiteral("tags"));
        auto *lv = dynamic_cast<ListValue *>(l.get());
        QVERIFY(lv);
        QCOMPARE(lv->length(), 2);
        QCOMPARE(lv->get(0)->type(), QStringLiteral("Tag"));
        QCOMPARE(lv->get(1)->type(), QStringLiteral("Tag"));
    }

    void testFromFrontMatterNestedObject()
    {
        QJsonObject inner;
        inner[QStringLiteral("x")] = 1;
        QJsonObject fm;
        fm[QStringLiteral("nested")] = inner;
        auto o = ObjectValue::fromFrontMatter(fm);
        auto v = o->get(QStringLiteral("nested"));
        QCOMPARE(v->type(), QStringLiteral("Object"));
    }

    void testFromFrontMatterArray()
    {
        QJsonArray arr;
        arr.append(1);
        arr.append(2);
        QJsonObject fm;
        fm[QStringLiteral("xs")] = arr;
        auto o = ObjectValue::fromFrontMatter(fm);
        auto *lv = dynamic_cast<ListValue *>(o->get(QStringLiteral("xs")).get());
        QVERIFY(lv);
        QCOMPARE(lv->length(), 2);
    }

    void testLambdaObjectValue()
    {
        auto lov = std::make_shared<LambdaObjectValue>([](const QString &k) -> ValuePtr {
            if (k == QLatin1String("ppu"))
                return std::make_shared<NumberValue>(42);
            return nullptr;
        });
        auto v = lov->objectAccess(QStringLiteral("ppu"));
        QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 42.0);
        QVERIFY(!lov->objectAccess(QStringLiteral("unknown")));
    }

    // ----- RegExpValue -----

    void testRegexType()
    {
        auto r = RegExpValue::parseFromString(QStringLiteral("/foo/"));
        QVERIFY(r);
        QCOMPARE(r->type(), QStringLiteral("Regex"));
    }

    void testRegexMatches()
    {
        auto r = RegExpValue::parseFromString(QStringLiteral("/^foo/"));
        QVERIFY(r);
        QVERIFY(r->matches(QStringLiteral("foobar")));
        QVERIFY(!r->matches(QStringLiteral("xyzfoo")));
    }

    void testRegexCaseInsensitiveFlag()
    {
        auto r = RegExpValue::parseFromString(QStringLiteral("/FOO/i"));
        QVERIFY(r);
        QVERIFY(r->matches(QStringLiteral("foo")));
    }

    void testRegexRejectsMalformed()
    {
        QVERIFY(!RegExpValue::parseFromString(QStringLiteral("not-a-regex")));
        QVERIFY(!RegExpValue::parseFromString(QStringLiteral("/")));
        QVERIFY(!RegExpValue::parseFromString(QStringLiteral("/unterminated")));
        QVERIFY(!RegExpValue::parseFromString(QStringLiteral("/[bad/")));
    }

    void testRegexToStringRoundtrip()
    {
        auto r = RegExpValue::parseFromString(QStringLiteral("/foo/i"));
        QVERIFY(r);
        QCOMPARE(r->toString(), QStringLiteral("/foo/i"));
    }
};

QTEST_APPLESS_MAIN(TestObjectAndRegex)
#include "tst_value_object_regex.moc"
