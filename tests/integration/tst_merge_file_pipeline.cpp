// tests/integration/tst_merge_file_pipeline.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster F-a P3.6 — Note Composer pipeline through the real Vault +
// FileManager: append/prepend byte-exact, source frontmatter stripped, source
// trashed, and source==target is a no-op.
#include <QTest>
#include <QFile>
#include <QTemporaryDir>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

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

class TestMergeFilePipeline : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void appendMovesBodyStripsFrontmatterAndTrashesSource()
    {
        QTemporaryDir dir;
        writeFile(dir.path() + "/A.md",
                  "---\ntitle: A\ntags: [x]\n---\n\nAlpha body");
        writeFile(dir.path() + "/B.md", "Bravo body");

        FileSystemAdapter fs;
        Vault vault(&fs);
        vault.load(dir.path());
        FileManager fm(&vault, nullptr);

        auto *a = vault.getFileByPath(QStringLiteral("A.md"));
        auto *b = vault.getFileByPath(QStringLiteral("B.md"));
        QVERIFY(a && b);

        QVERIFY(fm.mergeFileInto(a, b, FileManager::InsertMode::Append));

        // Target holds target + blank line + source body, and none of the
        // source's frontmatter.
        const QString merged = QString::fromUtf8(readFileAll(dir.path() + "/B.md"));
        QCOMPARE(merged, QStringLiteral("Bravo body\n\nAlpha body"));
        QVERIFY(!merged.contains(QStringLiteral("title: A")));

        // Source is trashed (gone from its path).
        QVERIFY(vault.getFileByPath(QStringLiteral("A.md")) == nullptr);
        QVERIFY(!QFile::exists(dir.path() + "/A.md"));
    }

    void prependPutsSourceFirst()
    {
        QTemporaryDir dir;
        writeFile(dir.path() + "/A.md", "Alpha body");
        writeFile(dir.path() + "/B.md", "Bravo body");

        FileSystemAdapter fs;
        Vault vault(&fs);
        vault.load(dir.path());
        FileManager fm(&vault, nullptr);

        auto *a = vault.getFileByPath(QStringLiteral("A.md"));
        auto *b = vault.getFileByPath(QStringLiteral("B.md"));
        QVERIFY(fm.mergeFileInto(a, b, FileManager::InsertMode::Prepend));
        QCOMPARE(QString::fromUtf8(readFileAll(dir.path() + "/B.md")),
                 QStringLiteral("Alpha body\n\nBravo body"));
    }

    void mergingAFileWithItselfIsANoOp()
    {
        QTemporaryDir dir;
        writeFile(dir.path() + "/A.md", "Alpha body");

        FileSystemAdapter fs;
        Vault vault(&fs);
        vault.load(dir.path());
        FileManager fm(&vault, nullptr);

        auto *a = vault.getFileByPath(QStringLiteral("A.md"));
        QVERIFY(a);
        QVERIFY(!fm.mergeFileInto(a, a, FileManager::InsertMode::Append));
        // Untouched.
        QCOMPARE(QString::fromUtf8(readFileAll(dir.path() + "/A.md")),
                 QStringLiteral("Alpha body"));
        QVERIFY(vault.getFileByPath(QStringLiteral("A.md")) != nullptr);
    }
};

QTEST_MAIN(TestMergeFilePipeline)
#include "tst_merge_file_pipeline.moc"
