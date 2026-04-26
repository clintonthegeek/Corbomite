// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QHash>

#include <memory>

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/LinkResolver.h"

class TestFileManagerRename : public QObject
{
    Q_OBJECT
private slots:
    void renameRewritesWikiLinks();
    void emitsProgress();
    void renameEmitsStartAndFinishWithoutCache();
    void renameRewritesMarkdownStyleLinks();
    void renameRewritesFullPathWikiLinks();
    void renameRewritesFrontmatterMarkdownLinks();
    void renameMixedFormsInSameBody();
    void renameLeavesUnrelatedLinksAlone();
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

void TestFileManagerRename::renameRewritesWikiLinks()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/Foo.md", "Foo body");
    writeFile(dir.path() + "/Linker.md", "See [[Foo]]");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    Corbomite::LinkResolver resolver;
    resolver.setVaultPaths({QStringLiteral("Foo.md"), QStringLiteral("Linker.md")});
    Corbomite::MetadataCache cache(resolver);
    cache.open(dir.path() + "/.corbomite/metadata-cache.db");
    QSignalSpy indexSpy(&cache, &Corbomite::MetadataCache::indexFinished);
    cache.rebuildVault(dir.path(),
                       {QStringLiteral("Foo.md"), QStringLiteral("Linker.md")});
    QTRY_VERIFY_WITH_TIMEOUT(indexSpy.count() >= 1, 10000);

    Corbomite::FileManager fm(&vault, &cache);
    auto *foo = vault.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    QVERIFY(fm.renameFile(foo, QStringLiteral("Bar.md")));

    QCOMPARE(readFileAll(dir.path() + "/Linker.md"), QByteArray("See [[Bar]]"));
}

void TestFileManagerRename::emitsProgress()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/Foo.md", "x");
    writeFile(dir.path() + "/A.md", "[[Foo]]");
    writeFile(dir.path() + "/B.md", "[[Foo]]");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    Corbomite::LinkResolver resolver;
    resolver.setVaultPaths({QStringLiteral("Foo.md"),
                            QStringLiteral("A.md"),
                            QStringLiteral("B.md")});
    Corbomite::MetadataCache cache(resolver);
    cache.open(dir.path() + "/.corbomite/metadata-cache.db");
    QSignalSpy indexSpy(&cache, &Corbomite::MetadataCache::indexFinished);
    cache.rebuildVault(dir.path(), {QStringLiteral("Foo.md"),
                                    QStringLiteral("A.md"),
                                    QStringLiteral("B.md")});
    QTRY_VERIFY_WITH_TIMEOUT(indexSpy.count() >= 1, 10000);

    Corbomite::FileManager fm(&vault, &cache);
    QSignalSpy spy(&fm, &Corbomite::FileManager::linkUpdateProgress);
    auto *foo = vault.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    fm.renameFile(foo, QStringLiteral("Bar.md"));

    QVERIFY(spy.count() >= 2);
}

void TestFileManagerRename::renameEmitsStartAndFinishWithoutCache()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    Corbomite::FileManager fm(&vault, /*cache=*/nullptr);
    QSignalSpy startSpy(&fm, &Corbomite::FileManager::renameStarted);
    QSignalSpy finishSpy(&fm, &Corbomite::FileManager::renameFinished);

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(fm.renameFile(tf, QStringLiteral("b.md")));
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(finishSpy.count(), 1);
}

namespace {
// Common harness: write `files`, load vault, prime metadata cache (with
// LinkResolver covering every path), construct FileManager. Returns the
// FileManager via `fm` out-param so tests can drive it directly.
struct RenameHarness {
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault;
    Corbomite::LinkResolver resolver;
    Corbomite::MetadataCache cache;
    std::unique_ptr<Corbomite::FileManager> fm;

    RenameHarness() : vault(&fs), cache(resolver) {}
};

void primeHarness(RenameHarness &h, const QHash<QString, QByteArray> &files)
{
    QStringList paths;
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        const QString full = h.dir.path() + QLatin1Char('/') + it.key();
        const QFileInfo info(full);
        QDir().mkpath(info.absolutePath());
        writeFile(full, it.value());
        paths.append(it.key());
    }
    h.vault.load(h.dir.path());
    h.resolver.setVaultPaths(paths);
    h.cache.open(h.dir.path() + "/.corbomite/metadata-cache.db");
    QSignalSpy indexSpy(&h.cache, &Corbomite::MetadataCache::indexFinished);
    h.cache.rebuildVault(h.dir.path(), paths);
    // indexFinished fires after allLinksResolved (10ms debounce), so once
    // it lands, link resolution has completed.
    QTRY_VERIFY_WITH_TIMEOUT(indexSpy.count() >= 1, 10000);
    h.fm = std::make_unique<Corbomite::FileManager>(&h.vault, &h.cache);
}
}

void TestFileManagerRename::renameRewritesMarkdownStyleLinks()
{
    RenameHarness h;
    primeHarness(h, {
        {QStringLiteral("Foo.md"), "Foo body"},
        {QStringLiteral("Linker.md"), "See [Display](Foo.md) for details."},
    });

    auto *foo = h.vault.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    QVERIFY(h.fm->renameFile(foo, QStringLiteral("Bar.md")));

    QCOMPARE(readFileAll(h.dir.path() + "/Linker.md"),
             QByteArray("See [Display](Bar.md) for details."));
}

void TestFileManagerRename::renameRewritesFullPathWikiLinks()
{
    RenameHarness h;
    primeHarness(h, {
        {QStringLiteral("notes/Foo.md"), "Foo body"},
        {QStringLiteral("Linker.md"), "Ref [[notes/Foo]] here."},
    });

    auto *foo = h.vault.getFileByPath(QStringLiteral("notes/Foo.md"));
    QVERIFY(foo);
    QVERIFY(h.fm->renameFile(foo, QStringLiteral("notes/Bar.md")));

    QCOMPARE(readFileAll(h.dir.path() + "/Linker.md"),
             QByteArray("Ref [[notes/Bar]] here."));
}

void TestFileManagerRename::renameRewritesFrontmatterMarkdownLinks()
{
    RenameHarness h;
    primeHarness(h, {
        {QStringLiteral("Foo.md"), "Foo body"},
        {QStringLiteral("Linker.md"),
         "---\nrelated: \"[Display](Foo.md)\"\n---\nbody"},
    });

    auto *foo = h.vault.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    QVERIFY(h.fm->renameFile(foo, QStringLiteral("Bar.md")));

    QCOMPARE(readFileAll(h.dir.path() + "/Linker.md"),
             QByteArray("---\nrelated: \"[Display](Bar.md)\"\n---\nbody"));
}

void TestFileManagerRename::renameMixedFormsInSameBody()
{
    RenameHarness h;
    primeHarness(h, {
        {QStringLiteral("Foo.md"), "Foo body"},
        {QStringLiteral("Linker.md"),
         "[[Foo]] and [[Foo|alias]] and [[Foo#heading]] and [Display](Foo.md)"},
    });

    auto *foo = h.vault.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    QVERIFY(h.fm->renameFile(foo, QStringLiteral("Bar.md")));

    QCOMPARE(readFileAll(h.dir.path() + "/Linker.md"),
             QByteArray(
                 "[[Bar]] and [[Bar|alias]] and [[Bar#heading]] and [Display](Bar.md)"));
}

void TestFileManagerRename::renameLeavesUnrelatedLinksAlone()
{
    RenameHarness h;
    primeHarness(h, {
        {QStringLiteral("Foo.md"), "Foo body"},
        {QStringLiteral("Other.md"), "Other body"},
        {QStringLiteral("Linker.md"),
         "[[Foo]] [[Other]] [Display](Other.md)"},
    });

    auto *foo = h.vault.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    QVERIFY(h.fm->renameFile(foo, QStringLiteral("Bar.md")));

    QCOMPARE(readFileAll(h.dir.path() + "/Linker.md"),
             QByteArray("[[Bar]] [[Other]] [Display](Other.md)"));
}

QTEST_MAIN(TestFileManagerRename)
#include "tst_file_manager_rename.moc"
