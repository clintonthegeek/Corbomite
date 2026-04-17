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

QTEST_MAIN(TestFileManagerFrontmatter)
#include "tst_file_manager_frontmatter.moc"
