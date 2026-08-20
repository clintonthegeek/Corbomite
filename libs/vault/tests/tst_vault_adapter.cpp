// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QFileInfo>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/core/DataAdapter.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/core/NoteDocument.h"

class TestVaultAdapter : public QObject
{
    Q_OBJECT
private slots:
    void buildTreeUsesAdapter();
    void saveDocument_routesThroughAdapterWriteBinary();
};

namespace {
class RecordingAdapter : public Corbomite::DataAdapter
{
public:
    mutable int statCalls = 0;
    mutable int listCalls = 0;
    bool exists(const QString &) const override { return true; }
    std::optional<QString> read(const QString &) const override { return {}; }
    std::optional<QByteArray> readBinary(const QString &) const override { return {}; }
    Corbomite::FileStat stat(const QString &p) const override
    {
        ++statCalls;
        QFileInfo fi(p);
        Corbomite::FileStat s;
        s.exists      = fi.exists();
        s.isDirectory = fi.isDir();
        s.isFile      = fi.isFile();
        s.sizeBytes   = fi.size();
        s.mtimeMs     = fi.lastModified().toMSecsSinceEpoch();
        s.ctimeMs     = fi.birthTime().toMSecsSinceEpoch();
        return s;
    }
    QStringList list(const QString &dir) const override
    {
        ++listCalls;
        return QDir(dir).entryList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    }
    bool write(const QString &, const QString &,
               const Corbomite::WriteHints & = {}) override { return false; }
    bool writeBinary(const QString &, const QByteArray &,
                     const Corbomite::WriteHints & = {}) override { return false; }
    bool rename(const QString &, const QString &) override { return false; }
    bool remove(const QString &) override { return false; }
    bool rmdir(const QString &) override { return false; }
    bool mkpath(const QString &) override { return true; }
    bool moveToTrash(const QString &) override { return false; }
};

/// Wraps FileSystemAdapter and records writeBinary calls so tests can verify
/// that Vault::saveDocument routes through the adapter rather than a raw QFile.
class SpyAdapter : public Corbomite::FileSystemAdapter
{
public:
    int writeBinaryCalls = 0;
    QString lastWritePath;
    QByteArray lastWriteContent;

    bool writeBinary(const QString &absolutePath,
                     const QByteArray &content,
                     const Corbomite::WriteHints &hints = {}) override
    {
        ++writeBinaryCalls;
        lastWritePath    = absolutePath;
        lastWriteContent = content;
        return Corbomite::FileSystemAdapter::writeBinary(absolutePath, content, hints);
    }
};
}

void TestVaultAdapter::buildTreeUsesAdapter()
{
    QTemporaryDir dir;
    QDir(dir.path()).mkpath(QStringLiteral("sub"));
    QFile a(dir.path() + "/a.md");
    a.open(QIODevice::WriteOnly); a.write("a"); a.close();
    QFile b(dir.path() + "/sub/b.md");
    b.open(QIODevice::WriteOnly); b.write("b"); b.close();

    RecordingAdapter adapter;
    Corbomite::Vault vault(&adapter);
    vault.load(dir.path());

    QVERIFY(adapter.listCalls > 0);
    QVERIFY(adapter.statCalls > 0);
    QCOMPARE(vault.getFiles().size(), 2);
    QCOMPARE(vault.getMarkdownFiles().size(), 2);
}

void TestVaultAdapter::saveDocument_routesThroughAdapterWriteBinary()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Pre-create the file so it is in the vault tree.
    const QString filePath = dir.path() + QStringLiteral("/note.md");
    QFile f(filePath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("initial");
    f.close();

    SpyAdapter adapter;
    Corbomite::Vault vault(&adapter);
    vault.load(dir.path());

    auto *note = vault.openDocument(QStringLiteral("note.md"));
    QVERIFY(note);

    note->setModified(true);

    const int writesBefore = adapter.writeBinaryCalls;
    QVERIFY(vault.saveDocument(note));

    // The adapter's writeBinary must have been called exactly once for the save.
    QCOMPARE(adapter.writeBinaryCalls, writesBefore + 1);
    QVERIFY(adapter.lastWritePath.endsWith(QStringLiteral("/note.md")));
}

QTEST_MAIN(TestVaultAdapter)
#include "tst_vault_adapter.moc"
