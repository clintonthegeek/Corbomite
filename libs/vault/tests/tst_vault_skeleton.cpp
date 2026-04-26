// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultSkeleton : public QObject
{
    Q_OBJECT
private slots:
    void emptyVaultHasRoot();
    void loadBuildsTree();
    void getAbstractFileByPath();
    void isEmptyTrueWhenOnlyRoot();
    void unloadClearsTree();
    void getNameIsBasenameOfBasePath();
    void caseSensitiveFsProbedAtLoad();
};

namespace {
void writeFile(const QString &path, const QByteArray &body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path); f.open(QIODevice::WriteOnly); f.write(body);
}
}

void TestVaultSkeleton::emptyVaultHasRoot()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    QVERIFY(vault.getRoot() != nullptr);
    QCOMPARE(vault.getRoot()->isRoot(), true);
}

void TestVaultSkeleton::loadBuildsTree()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "a");
    writeFile(dir.path() + "/sub/b.md", "b");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QVERIFY(vault.getRoot() != nullptr);
    QCOMPARE(vault.getFiles().size(), 2);
    QCOMPARE(vault.getMarkdownFiles().size(), 2);
}

void TestVaultSkeleton::getAbstractFileByPath()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "a");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *f = vault.getAbstractFileByPath(QStringLiteral("a.md"));
    QVERIFY(f != nullptr);
    QCOMPARE(f->name, QStringLiteral("a.md"));

    QCOMPARE(vault.getAbstractFileByPath(QStringLiteral("missing.md")),
             static_cast<Corbomite::TAbstractFile *>(nullptr));
}

void TestVaultSkeleton::isEmptyTrueWhenOnlyRoot()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    QCOMPARE(vault.isEmpty(), true);

    QTemporaryDir dir;
    vault.load(dir.path());
    QCOMPARE(vault.isEmpty(), true);

    writeFile(dir.path() + "/a.md", "a");
    vault.load(dir.path());
    QCOMPARE(vault.isEmpty(), false);
}

void TestVaultSkeleton::unloadClearsTree()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "a");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    QVERIFY(vault.isLoaded());

    vault.unload();
    QCOMPARE(vault.isLoaded(), false);
    QCOMPARE(vault.isEmpty(), true);
}

void TestVaultSkeleton::getNameIsBasenameOfBasePath()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    QCOMPARE(vault.getName(), QFileInfo(dir.path()).fileName());
}

// Vault::load probes the underlying filesystem's case-sensitivity once and
// exposes it via isCaseSensitiveFilesystem(). The probe writes a temp file
// + checks the lowercase form for visibility — value is FS-dependent, but
// the call must succeed and not crash. (CI runs on case-sensitive Linux,
// so we additionally assert true here.)
void TestVaultSkeleton::caseSensitiveFsProbedAtLoad()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    QVERIFY(vault.isCaseSensitiveFilesystem());
}

QTEST_MAIN(TestVaultSkeleton)
#include "tst_vault_skeleton.moc"
