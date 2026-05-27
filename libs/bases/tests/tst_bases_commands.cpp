// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/bases/BasesCommands.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

using namespace Corbomite::Bases;

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
}  // namespace

class TestBasesCommands : public QObject
{
    Q_OBJECT
private slots:
    void redoWritesNewValue();
    void undoRestoresOldValue();
    void redoAfterUndoReapplies();
    void driftBeforeUndoIsSkipped();
    void undoRemovesKeyAbsentBeforeEdit();
    void textContainsKey();
};

void TestBasesCommands::redoWritesNewValue()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\ntag: old\n---\nbody\n");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);
    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);

    CmdSetFrontMatter cmd(&fm, tf, QStringLiteral("tag"),
                          QStringLiteral("new"), nullptr);
    cmd.redo();

    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: new"));
    QVERIFY(!after.contains("tag: old"));
}

void TestBasesCommands::undoRestoresOldValue()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\ntag: old\n---\nbody\n");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);
    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));

    CmdSetFrontMatter cmd(&fm, tf, QStringLiteral("tag"),
                          QStringLiteral("new"), nullptr);
    cmd.redo();
    cmd.undo();

    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: old"));
    QVERIFY(!after.contains("tag: new"));
}

void TestBasesCommands::redoAfterUndoReapplies()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\ntag: old\n---\nbody\n");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);
    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));

    CmdSetFrontMatter cmd(&fm, tf, QStringLiteral("tag"),
                          QStringLiteral("new"), nullptr);
    cmd.redo();
    cmd.undo();
    cmd.redo();

    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: new"));
}

void TestBasesCommands::driftBeforeUndoIsSkipped()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\ntag: old\n---\nbody\n");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);
    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));

    int notifyCount = 0;
    CmdSetFrontMatter cmd(&fm, tf, QStringLiteral("tag"),
                          QStringLiteral("new"),
                          [&](const QString &) { ++notifyCount; });
    cmd.redo();   // disk: tag: new

    // Simulate an external edit to the same key.
    fm.processFrontMatter(tf, [](QVariantMap &m) {
        m.insert(QStringLiteral("tag"), QStringLiteral("external"));
    });

    cmd.undo();   // must NOT clobber the external value
    QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: external"));
    QVERIFY(!after.contains("tag: old"));
    QCOMPARE(notifyCount, 1);

    // Neutralized: a subsequent redo is also a no-op.
    cmd.redo();
    after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: external"));
    QCOMPARE(notifyCount, 1);   // not called again
}

void TestBasesCommands::undoRemovesKeyAbsentBeforeEdit()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\nkeep: yes\n---\nbody\n");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);
    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));

    CmdSetFrontMatter cmd(&fm, tf, QStringLiteral("brandnew"),
                          QStringLiteral("v"), nullptr);
    cmd.redo();
    QVERIFY(readFileAll(dir.path() + "/a.md").contains("brandnew: v"));

    cmd.undo();
    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(!after.contains("brandnew"));   // key fully removed
    QVERIFY(after.contains("keep: yes"));   // untouched key preserved
}

void TestBasesCommands::textContainsKey()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    Corbomite::FileManager fm(&vault, nullptr);
    CmdSetFrontMatter cmd(&fm, nullptr, QStringLiteral("status"),
                          QStringLiteral("x"), nullptr);
    QVERIFY(cmd.text().contains(QStringLiteral("status")));
}

QTEST_MAIN(TestBasesCommands)
#include "tst_bases_commands.moc"
