// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestFileManagerFrontmatter : public QObject
{
    Q_OBJECT
private slots:
    void addsFrontMatterWhenAbsent();
    void mutatesExistingFrontMatter();
    void preservesBodyVerbatim();
    void noopOnNonMarkdown();
    void preservesKeyOrderOnMutation();
    void appendsNewKeysAfterOriginalOrder();
    void emptiedFrontmatterStripsBlockEntirely();
};

namespace {
void writeFile(const QString &p, const QByteArray &b)
{
    QFile f(p);
    f.open(QIODevice::WriteOnly);
    f.write(b);
    f.close();
}
QByteArray readFileAll(const QString &p)
{
    QFile f(p);
    f.open(QIODevice::ReadOnly);
    return f.readAll();
}
}

void TestFileManagerFrontmatter::addsFrontMatterWhenAbsent()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "body");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    QVERIFY(fm.processFrontMatter(tf, [](QVariantMap &m) {
        m.insert(QStringLiteral("tag"), QStringLiteral("new"));
    }));

    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: new"));
    QVERIFY(after.contains("body"));
}

void TestFileManagerFrontmatter::mutatesExistingFrontMatter()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\ntag: old\n---\nbody\n");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    fm.processFrontMatter(tf, [](QVariantMap &m) {
        m.insert(QStringLiteral("tag"), QStringLiteral("new"));
    });

    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: new"));
    QVERIFY(!after.contains("tag: old"));
    QVERIFY(after.contains("body"));
}

void TestFileManagerFrontmatter::preservesBodyVerbatim()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\nk: v\n---\nLine1\nLine2\n");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    fm.processFrontMatter(tf, [](QVariantMap &) {});

    QVERIFY(readFileAll(dir.path() + "/a.md").contains("Line1\nLine2"));
}

void TestFileManagerFrontmatter::noopOnNonMarkdown()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.canvas", "{}");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);

    auto *tf = vault.getFileByPath(QStringLiteral("a.canvas"));
    QVERIFY(tf);
    QCOMPARE(fm.processFrontMatter(tf, [](QVariantMap &) {}), false);
}

// Regression: processFrontMatter must preserve the original key order of the
// frontmatter block on mutation. Round-tripping through QVariantMap (sorted
// QMap) reorders keys alphabetically on every save, causing diff churn for
// vaults shared with Obsidian (which preserves insertion order).
void TestFileManagerFrontmatter::preservesKeyOrderOnMutation()
{
    QTemporaryDir dir;
    // Anti-alphabetical order: title, tags, aliases.
    writeFile(dir.path() + "/a.md",
              "---\ntitle: T\ntags:\n  - one\naliases:\n  - A\n---\nbody\n");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    QVERIFY(fm.processFrontMatter(tf, [](QVariantMap &m) {
        m.insert(QStringLiteral("title"), QStringLiteral("T2"));
    }));

    const QString after = QString::fromUtf8(readFileAll(dir.path() + "/a.md"));
    const int titlePos = after.indexOf(QStringLiteral("title:"));
    const int tagsPos = after.indexOf(QStringLiteral("tags:"));
    const int aliasesPos = after.indexOf(QStringLiteral("aliases:"));
    QVERIFY(titlePos >= 0 && tagsPos >= 0 && aliasesPos >= 0);
    QVERIFY2(titlePos < tagsPos, "title must precede tags (original order)");
    QVERIFY2(tagsPos < aliasesPos, "tags must precede aliases (original order)");
}

// Regression: keys added by the mutator must be appended *after* the
// original keys, not interleaved alphabetically.
void TestFileManagerFrontmatter::appendsNewKeysAfterOriginalOrder()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md",
              "---\ntitle: T\nzebra: Z\n---\nbody\n");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    QVERIFY(fm.processFrontMatter(tf, [](QVariantMap &m) {
        // "alpha" sorts before existing keys; without an order-preserving
        // pass it would land first in the output.
        m.insert(QStringLiteral("alpha"), QStringLiteral("A"));
    }));

    const QString after = QString::fromUtf8(readFileAll(dir.path() + "/a.md"));
    const int titlePos = after.indexOf(QStringLiteral("title:"));
    const int zebraPos = after.indexOf(QStringLiteral("zebra:"));
    const int alphaPos = after.indexOf(QStringLiteral("alpha:"));
    QVERIFY(titlePos >= 0 && zebraPos >= 0 && alphaPos >= 0);
    QVERIFY2(titlePos < zebraPos, "title must precede zebra (original order)");
    QVERIFY2(zebraPos < alphaPos, "alpha (new) must come after originals");
}

// Regression: when the mutator empties the map (deletes the last key), the
// frontmatter block must be stripped entirely. Markoff::Document::withFrontmatter
// emits `---\n\n---\n` for an empty map; Obsidian instead removes the block.
// Round-trip parity requires Corbomite to drop the now-empty fence too.
void TestFileManagerFrontmatter::emptiedFrontmatterStripsBlockEntirely()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\nonly: key\n---\nbody\n");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    QVERIFY(fm.processFrontMatter(tf, [](QVariantMap &m) {
        m.remove(QStringLiteral("only"));
    }));

    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY2(!after.contains("---"),
             "fence must be gone when mutator emptied the map");
    QVERIFY(after.contains("body"));
}

QTEST_MAIN(TestFileManagerFrontmatter)
#include "tst_file_manager_frontmatter.moc"
