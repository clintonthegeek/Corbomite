// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

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

QTEST_MAIN(TestFileManagerRename)
#include "tst_file_manager_rename.moc"
