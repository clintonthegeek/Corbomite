// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTemporaryDir>
#include <QTest>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"

#include <markoff/parser/Document.h>
#include <markoff/parser/YamlValue.h>

using namespace Corbomite;

class TestSetFrontMatter : public QObject
{
    Q_OBJECT

    // Seed a file by creating it in the vault (vault must be loaded).
    TFile *seed(Vault &v, const QString &rel, const QByteArray &body)
    {
        TFile *f = v.create(rel, body);
        Q_ASSERT(f);
        return f;
    }
    // Read back from disk via Vault.
    QString readBack(Vault &v, TFile *f) { return QString::fromUtf8(v.read(f)); }

private slots:
    void writesEntriesInGivenOrder();
    void omittedKeyIsDeleted();
    void emptyListStripsFrontmatter();
    void preserveFromDiskKeepsNestedMapVerbatim();
    void renameShapedChangeRoundTrips();
    void nonMarkdownReturnsFalse();
};

void TestSetFrontMatter::writesEntriesInGivenOrder()
{
    FileSystemAdapter fs; QTemporaryDir dir; Vault v(&fs); v.load(dir.path());
    FileManager fm(&v, nullptr);
    TFile *f = seed(v, QStringLiteral("n.md"),
                    "---\nalpha: 1\nbeta: 2\n---\nbody\n");

    QList<FileManager::FrontMatterEntry> entries{
        {QStringLiteral("beta"),  QVariant::fromValue<qlonglong>(2), false},
        {QStringLiteral("gamma"), QStringLiteral("g"),               false},
        {QStringLiteral("alpha"), QVariant::fromValue<qlonglong>(1), false},
    };
    QVERIFY(fm.setFrontMatter(f, entries));

    auto doc = Markoff::Document::fromMarkdown(readBack(v, f));
    QCOMPARE(doc->parsedFrontmatter().keys(),
             QStringList({QStringLiteral("beta"), QStringLiteral("gamma"),
                          QStringLiteral("alpha")}));
}

void TestSetFrontMatter::omittedKeyIsDeleted()
{
    FileSystemAdapter fs; QTemporaryDir dir; Vault v(&fs); v.load(dir.path());
    FileManager fm(&v, nullptr);
    TFile *f = seed(v, QStringLiteral("n.md"), "---\nkeep: 1\ndrop: 2\n---\nx\n");

    QVERIFY(fm.setFrontMatter(f, {{QStringLiteral("keep"),
                                   QVariant::fromValue<qlonglong>(1), false}}));
    auto doc = Markoff::Document::fromMarkdown(readBack(v, f));
    QCOMPARE(doc->parsedFrontmatter().keys(), QStringList({QStringLiteral("keep")}));
}

void TestSetFrontMatter::emptyListStripsFrontmatter()
{
    FileSystemAdapter fs; QTemporaryDir dir; Vault v(&fs); v.load(dir.path());
    FileManager fm(&v, nullptr);
    TFile *f = seed(v, QStringLiteral("n.md"), "---\nk: 1\n---\nbody\n");

    QVERIFY(fm.setFrontMatter(f, {}));
    const QString out = readBack(v, f);
    QVERIFY(!out.startsWith(QStringLiteral("---")));
    QVERIFY(out.contains(QStringLiteral("body")));
}

void TestSetFrontMatter::preserveFromDiskKeepsNestedMapVerbatim()
{
    FileSystemAdapter fs; QTemporaryDir dir; Vault v(&fs); v.load(dir.path());
    FileManager fm(&v, nullptr);
    TFile *f = seed(v, QStringLiteral("n.md"),
        "---\nscalar: old\nmeta:\n  a: 1\n  b: two\n---\nbody\n");

    QList<FileManager::FrontMatterEntry> entries{
        {QStringLiteral("scalar"), QStringLiteral("new"), false},
        {QStringLiteral("meta"),   QVariant{},            true},
    };
    QVERIFY(fm.setFrontMatter(f, entries));

    auto doc = Markoff::Document::fromMarkdown(readBack(v, f));
    Markoff::YamlValue fmv = doc->parsedFrontmatter();
    QCOMPARE(fmv.get(QStringLiteral("scalar")).asString(), QStringLiteral("new"));
    QVERIFY(fmv.get(QStringLiteral("meta")).isMap());
    QCOMPARE(fmv.get(QStringLiteral("meta")).get(QStringLiteral("a")).asInt(), 1);
    QCOMPARE(fmv.get(QStringLiteral("meta")).get(QStringLiteral("b")).asString(),
             QStringLiteral("two"));
}

void TestSetFrontMatter::renameShapedChangeRoundTrips()
{
    FileSystemAdapter fs; QTemporaryDir dir; Vault v(&fs); v.load(dir.path());
    FileManager fm(&v, nullptr);
    TFile *f = seed(v, QStringLiteral("n.md"), "---\nold: v\nother: 9\n---\nb\n");

    QList<FileManager::FrontMatterEntry> entries{
        {QStringLiteral("new"),   QStringLiteral("v"),               false},
        {QStringLiteral("other"), QVariant::fromValue<qlonglong>(9), false},
    };
    QVERIFY(fm.setFrontMatter(f, entries));
    auto doc = Markoff::Document::fromMarkdown(readBack(v, f));
    QCOMPARE(doc->parsedFrontmatter().keys(),
             QStringList({QStringLiteral("new"), QStringLiteral("other")}));
    QCOMPARE(doc->parsedFrontmatter().get(QStringLiteral("new")).asString(),
             QStringLiteral("v"));
}

void TestSetFrontMatter::nonMarkdownReturnsFalse()
{
    FileSystemAdapter fs; QTemporaryDir dir; Vault v(&fs); v.load(dir.path());
    FileManager fm(&v, nullptr);
    QVERIFY(!fm.setFrontMatter(nullptr, {}));
}

QTEST_MAIN(TestSetFrontMatter)
#include "tst_setfrontmatter.moc"
