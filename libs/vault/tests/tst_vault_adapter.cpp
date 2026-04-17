// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QFileInfo>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/DataAdapter.h"

class TestVaultAdapter : public QObject
{
    Q_OBJECT
private slots:
    void buildTreeUsesAdapter();
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

QTEST_MAIN(TestVaultAdapter)
#include "tst_vault_adapter.moc"
