// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/bases/Formula.h"
#include "corbomite/bases/FunctionRegistry.h"
#include "corbomite/bases/Values.h"
#include "corbomite/bases/VaultResolver.h"

#include <QHash>
#include <QSet>

using namespace Corbomite::Bases;

namespace {

ValuePtr run(const QString &source, const EvalContext &ctx)
{
    Formula f(source);
    return f.getValue(ctx);
}

class NullCtx : public EvalContext
{
public:
    ValuePtr getByIdentifier(const QString &) const override { return nullptr; }
};

class FakeResolver : public VaultResolver
{
public:
    QSet<QString> files;                 // paths that exist
    QHash<QString, QString> linkMap;     // linkData -> canonical path

    ValuePtr fileAt(const QString &p) const override
    {
        return files.contains(p) ? std::static_pointer_cast<Value>(
                   std::make_shared<FileValue>(nullptr, nullptr))
                                 : NullValue::instance();
    }
    QString resolveLinkTarget(const QString &linkData, const QString &) const override
    {
        // sourcePath unused in the fake; the real resolver uses it for relative/short-link resolution.
        return linkMap.value(linkData);  // "" if absent
    }
};

class VaultCtx : public EvalContext
{
public:
    const FakeResolver *res = nullptr;
    QHash<QString, ValuePtr> ids;        // identifier -> value (for lnk.* tests)

    ValuePtr getByIdentifier(const QString &n) const override { return ids.value(n); }
    const VaultResolver *vault() const override { return res; }
};

}  // namespace

class TestBuiltins : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    // ----- Global functions -----

    void testNow() { NullCtx c; auto v = run(QStringLiteral("now()"), c); QCOMPARE(v->type(), QStringLiteral("Date")); }
    void testToday() { NullCtx c; auto v = run(QStringLiteral("today()"), c); QCOMPARE(v->type(), QStringLiteral("Date")); }
    void testDateCtor() { NullCtx c; auto v = run(QStringLiteral("date('2024-01-15')"), c); QCOMPARE(v->type(), QStringLiteral("Date")); }
    void testMinVariadic() { NullCtx c; auto v = run(QStringLiteral("min(3, 1, 2)"), c); QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 1.0); }
    void testMaxVariadic() { NullCtx c; auto v = run(QStringLiteral("max(3, 1, 2)"), c); QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 3.0); }
    void testListWraps() { NullCtx c; auto v = run(QStringLiteral("list(5)"), c); QCOMPARE(v->type(), QStringLiteral("List")); QCOMPARE(std::static_pointer_cast<ListValue>(v)->length(), 1); }
    void testListIdentity() { NullCtx c; auto v = run(QStringLiteral("list([1,2,3])"), c); QCOMPARE(std::static_pointer_cast<ListValue>(v)->length(), 3); }
    void testNumberCoerceBool() { NullCtx c; auto v = run(QStringLiteral("number(true)"), c); QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 1.0); }
    void testNumberCoerceString() { NullCtx c; auto v = run(QStringLiteral("number('3.14')"), c); QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 3.14); }
    void testDurationCtor() { NullCtx c; auto v = run(QStringLiteral("duration('5 days')"), c); QCOMPARE(v->type(), QStringLiteral("Duration")); }
    void testIconCtor() { NullCtx c; auto v = run(QStringLiteral("icon('star')"), c); QCOMPARE(v->type(), QStringLiteral("Icon")); }
    void testHtmlCtor() { NullCtx c; auto v = run(QStringLiteral("html('<b>x</b>')"), c); QCOMPARE(v->type(), QStringLiteral("HTML")); }
    void testEscapeHTML() { NullCtx c; auto v = run(QStringLiteral("escapeHTML('<a>')"), c); QCOMPARE(v->toString(), QStringLiteral("&lt;a&gt;")); }

    // ----- Hard-cased `if` -----

    void testIfTrue() { NullCtx c; auto v = run(QStringLiteral("if(true, 1, 2)"), c); QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 1.0); }
    void testIfFalse() { NullCtx c; auto v = run(QStringLiteral("if(false, 1, 2)"), c); QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 2.0); }
    void testIfElseDefaultsToNull() { NullCtx c; auto v = run(QStringLiteral("if(false, 1)"), c); QCOMPARE(v->type(), QStringLiteral("Null")); }

    // ----- String methods -----

    void testStringStartsEnds()
    {
        NullCtx c;
        QVERIFY(run(QStringLiteral("'hello'.startsWith('he')"), c)->isTruthy());
        QVERIFY(run(QStringLiteral("'hello'.endsWith('lo')"), c)->isTruthy());
        QVERIFY(!run(QStringLiteral("'hello'.startsWith('x')"), c)->isTruthy());
    }
    void testStringTrim() { NullCtx c; QCOMPARE(run(QStringLiteral("'  x  '.trim()"), c)->toString(), QStringLiteral("x")); }
    void testStringTitle() { NullCtx c; QCOMPARE(run(QStringLiteral("'foo bar'.title()"), c)->toString(), QStringLiteral("Foo Bar")); }
    void testStringReplace() { NullCtx c; QCOMPARE(run(QStringLiteral("'abc'.replace('b', 'X')"), c)->toString(), QStringLiteral("aXc")); }
    void testStringLower() { NullCtx c; QCOMPARE(run(QStringLiteral("'ABC'.lower()"), c)->toString(), QStringLiteral("abc")); }
    void testStringSplit()
    {
        NullCtx c;
        auto v = run(QStringLiteral("'a,b,c'.split(',')"), c);
        QCOMPARE(std::static_pointer_cast<ListValue>(v)->length(), 3);
    }
    void testStringContains() { NullCtx c; QVERIFY(run(QStringLiteral("'hello'.contains('ell')"), c)->isTruthy()); }
    void testStringSlice() { NullCtx c; QCOMPARE(run(QStringLiteral("'hello'.slice(1, 4)"), c)->toString(), QStringLiteral("ell")); }
    void testStringRepeat() { NullCtx c; QCOMPARE(run(QStringLiteral("'ab'.repeat(3)"), c)->toString(), QStringLiteral("ababab")); }

    // ----- Number methods -----

    void testNumberRound() { NullCtx c; QCOMPARE(std::static_pointer_cast<NumberValue>(run(QStringLiteral("3.14.round(1)"), c))->data(), 3.1); }
    void testNumberCeil() { NullCtx c; QCOMPARE(std::static_pointer_cast<NumberValue>(run(QStringLiteral("3.1.ceil()"), c))->data(), 4.0); }
    void testNumberFloor() { NullCtx c; QCOMPARE(std::static_pointer_cast<NumberValue>(run(QStringLiteral("3.9.floor()"), c))->data(), 3.0); }
    void testNumberAbs() { NullCtx c; QCOMPARE(std::static_pointer_cast<NumberValue>(run(QStringLiteral("(-5).abs()"), c))->data(), 5.0); }
    void testNumberToFixed() { NullCtx c; QCOMPARE(run(QStringLiteral("3.14.toFixed(1)"), c)->toString(), QStringLiteral("3.1")); }

    // ----- List methods (non-lambda) -----

    void testListSum() { NullCtx c; QCOMPARE(std::static_pointer_cast<NumberValue>(run(QStringLiteral("[1,2,3].sum()"), c))->data(), 6.0); }
    void testListJoin() { NullCtx c; QCOMPARE(run(QStringLiteral("['a','b','c'].join('-')"), c)->toString(), QStringLiteral("a-b-c")); }
    void testListSort()
    {
        NullCtx c;
        auto v = run(QStringLiteral("[3,1,2].sort()"), c);
        auto *l = static_cast<ListValue *>(v.get());
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(0))->data(), 1.0);
    }
    void testListContains() { NullCtx c; QVERIFY(run(QStringLiteral("[1,2,3].contains(2)"), c)->isTruthy()); }

    // ----- List lambda methods (hard-cased) -----

    void testListMap()
    {
        NullCtx c;
        auto v = run(QStringLiteral("[1,2,3].map(value * 2)"), c);
        auto *l = static_cast<ListValue *>(v.get());
        QCOMPARE(l->length(), 3);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(0))->data(), 2.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(2))->data(), 6.0);
    }

    void testListFilter()
    {
        NullCtx c;
        auto v = run(QStringLiteral("[1,2,3,4,5].filter(value > 2)"), c);
        auto *l = static_cast<ListValue *>(v.get());
        QCOMPARE(l->length(), 3);
        QCOMPARE(std::static_pointer_cast<NumberValue>(l->get(0))->data(), 3.0);
    }

    void testListReduce()
    {
        NullCtx c;
        auto v = run(QStringLiteral("[1,2,3].reduce(acc + value, 0)"), c);
        QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 6.0);
    }

    // ----- Object methods -----

    void testObjectKeysValues()
    {
        auto obj = std::make_shared<ObjectValue>();
        obj->set(QStringLiteral("a"), std::make_shared<NumberValue>(1));
        obj->set(QStringLiteral("b"), std::make_shared<NumberValue>(2));
        LambdaContext c([&obj](const QString &n) -> ValuePtr {
            if (n == QLatin1String("o")) return obj;
            return nullptr;
        });
        auto v = run(QStringLiteral("o.keys()"), c);
        QCOMPARE(std::static_pointer_cast<ListValue>(v)->length(), 2);
    }

    // ----- Regex -----

    void testRegexMatches()
    {
        NullCtx c;
        QVERIFY(run(QStringLiteral("/foo/i.matches('FOO')"), c)->isTruthy());
    }

    // ----- file() global via VaultResolver -----

    void testFileGlobalResolvesViaVault()
    {
        FakeResolver r; r.files.insert(QStringLiteral("Notes/X.md"));
        VaultCtx c; c.res = &r;
        auto v = run(QStringLiteral("file('Notes/X.md')"), c);
        QCOMPARE(v->type(), QStringLiteral("File"));
    }

    void testFileGlobalUnboundReturnsNull()
    {
        NullCtx c;
        auto v = run(QStringLiteral("file('Notes/X.md')"), c);
        QCOMPARE(v->type(), QStringLiteral("Null"));
    }

    void testFileGlobalNoArgsReturnsNull()
    {
        // file() with no path arg: the DSL passes an empty args vector; the lambda
        // calls fileAt("") which returns Null when the path is absent from the vault.
        FakeResolver r;  // files is empty
        VaultCtx c; c.res = &r;
        auto v = run(QStringLiteral("file()"), c);
        QCOMPARE(v->type(), QStringLiteral("Null"));
    }

    // ----- LinkValue.asFile + linksTo via VaultResolver -----

    void testLinkAsFileResolves()
    {
        FakeResolver r;
        r.linkMap.insert(QStringLiteral("Foo"), QStringLiteral("Notes/Foo.md"));
        r.files.insert(QStringLiteral("Notes/Foo.md"));
        VaultCtx c; c.res = &r;
        c.ids.insert(QStringLiteral("lnk"),
                     std::make_shared<LinkValue>(QStringLiteral("Foo"),
                                                 QStringLiteral("src.md")));
        auto v = run(QStringLiteral("lnk.asFile()"), c);
        QCOMPARE(v->type(), QStringLiteral("File"));
    }

    void testLinkLinksToResolvesCanonical()
    {
        FakeResolver r;
        r.linkMap.insert(QStringLiteral("Foo"), QStringLiteral("Notes/Foo.md"));
        r.linkMap.insert(QStringLiteral("Notes/Foo.md"), QStringLiteral("Notes/Foo.md"));
        VaultCtx c; c.res = &r;
        c.ids.insert(QStringLiteral("lnk"),
                     std::make_shared<LinkValue>(QStringLiteral("Foo"),
                                                 QStringLiteral("src.md")));
        auto yes = run(QStringLiteral("lnk.linksTo('Notes/Foo.md')"), c);
        auto no  = run(QStringLiteral("lnk.linksTo('Other.md')"), c);
        QCOMPARE(std::static_pointer_cast<BooleanValue>(yes)->data(), true);
        QCOMPARE(std::static_pointer_cast<BooleanValue>(no)->data(), false);
    }

    void testLinkAsFileReturnsNullWhenNotFound()
    {
        FakeResolver r;
        r.linkMap.insert(QStringLiteral("Foo"), QStringLiteral("Notes/Foo.md"));
        // note: files is empty — the resolved path does not exist
        VaultCtx c; c.res = &r;
        c.ids.insert(QStringLiteral("lnk"),
                     std::make_shared<LinkValue>(QStringLiteral("Foo"),
                                                 QStringLiteral("src.md")));
        auto v = run(QStringLiteral("lnk.asFile()"), c);
        QCOMPARE(v->type(), QStringLiteral("Null"));
    }

    void testLinkLinksToUnresolvedReturnsFalse()
    {
        FakeResolver r;  // empty linkMap: the link resolves to nothing
        VaultCtx c; c.res = &r;
        c.ids.insert(QStringLiteral("lnk"),
                     std::make_shared<LinkValue>(QStringLiteral("Ghost"),
                                                 QStringLiteral("src.md")));
        auto v = run(QStringLiteral("lnk.linksTo('Anything.md')"), c);
        QCOMPARE(std::static_pointer_cast<BooleanValue>(v)->data(), false);
    }

    // ----- Error paths -----

    void testUnknownFunctionError()
    {
        NullCtx c;
        auto v = run(QStringLiteral("doesNotExist()"), c);
        QCOMPARE(v->type(), QStringLiteral("Error"));
    }

    // ----- Formula wrapper -----

    void testFormulaInvalid()
    {
        Formula f(QStringLiteral("+++"));
        QVERIFY(!f.isValid());
    }

    void testFormulaRoundTripSource()
    {
        Formula f(QStringLiteral("a + b"));
        QCOMPARE(f.toString(), QStringLiteral("a + b"));
    }
};

QTEST_APPLESS_MAIN(TestBuiltins)
#include "tst_builtins.moc"
