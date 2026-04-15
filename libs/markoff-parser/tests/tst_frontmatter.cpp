// SPDX-License-Identifier: GPL-3.0-or-later
#include <QObject>
#include <QTest>
#include <QVariant>
#include <markoff-parser/Document.h>

using namespace Markoff;

class TestFrontmatter : public QObject {
    Q_OBJECT
private slots:
    void standardFrontmatter();
    void listStyleTags();
    void commaStyleTags();
    void emptyFrontmatter();
    void invalidYaml();
    void booleanValues();
    void numericValues();
    void noFrontmatter();
};

void TestFrontmatter::standardFrontmatter()
{
    auto doc = Document::fromMarkdown(QStringLiteral(
        "---\ntitle: My Note\ntags:\n  - foo\n  - bar\naliases:\n  - mn\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 3);
    QCOMPARE(props[0].key, QStringLiteral("title"));
    QCOMPARE(props[0].value.toString(), QStringLiteral("My Note"));
    QCOMPARE(props[1].key, QStringLiteral("tags"));
    QCOMPARE(props[1].value.toStringList(), QStringList({QStringLiteral("foo"), QStringLiteral("bar")}));
    QCOMPARE(props[2].key, QStringLiteral("aliases"));
    QCOMPARE(props[2].value.toStringList(), QStringList({QStringLiteral("mn")}));
}

void TestFrontmatter::listStyleTags()
{
    auto doc = Document::fromMarkdown(QStringLiteral(
        "---\ntags: [alpha, beta]\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 1);
    QCOMPARE(props[0].value.toStringList(), QStringList({QStringLiteral("alpha"), QStringLiteral("beta")}));
}

void TestFrontmatter::commaStyleTags()
{
    auto doc = Document::fromMarkdown(QStringLiteral(
        "---\ntags: alpha, beta\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 1);
    QCOMPARE(props[0].value.toStringList(), QStringList({QStringLiteral("alpha"), QStringLiteral("beta")}));
}

void TestFrontmatter::emptyFrontmatter()
{
    auto doc = Document::fromMarkdown(QStringLiteral("---\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 0);
}

void TestFrontmatter::invalidYaml()
{
    auto doc = Document::fromMarkdown(QStringLiteral(
        "---\n: invalid: yaml: [[\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 0); // graceful empty, no crash
}

void TestFrontmatter::booleanValues()
{
    auto doc = Document::fromMarkdown(QStringLiteral(
        "---\npublish: true\ndraft: false\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 2);
    QCOMPARE(props[0].value.toBool(), true);
    QCOMPARE(props[1].value.toBool(), false);
}

void TestFrontmatter::numericValues()
{
    auto doc = Document::fromMarkdown(QStringLiteral(
        "---\nweight: 42\nrating: 3.5\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 2);
    QCOMPARE(props[0].value.toInt(), 42);
    QCOMPARE(props[1].value.toDouble(), 3.5);
}

void TestFrontmatter::noFrontmatter()
{
    auto doc = Document::fromMarkdown(QStringLiteral("Just body text"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 0);
}

QTEST_APPLESS_MAIN(TestFrontmatter)
#include "tst_frontmatter.moc"
