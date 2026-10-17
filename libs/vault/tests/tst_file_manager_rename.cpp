// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QHash>

#include <memory>
#include <optional>

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
    void renameRewritesEveryFrontmatterLinkRegardlessOfKeyOrder();
    void renameMixedFormsInSameBody();
    void renameLeavesUnrelatedLinksAlone();
    void emitsProgressInAscendingSourceOrder();
    void renameRoutesNonMarkdownThroughLinkRewriter();
    void renameWithUnparseableFrontmatterRewritesBodyOnly();
};

namespace {

// Stub LinkRewriter: records the sources it is handed and echoes the mapper
// result, so the test proves the vault-side seam without depending on
// libs/canvas.
class StubRewriter : public Corbomite::LinkRewriter
{
public:
    QString extension() const override { return QStringLiteral("canvas"); }
    std::optional<QByteArray> rewrite(const QString &path, const QByteArray &,
                                      const TargetMapper &mapTarget) override
    {
        calls.append(path);
        const std::optional<QString> mapped = mapTarget(QStringLiteral("Foo.md"));
        if (!mapped.has_value()) return std::nullopt;
        return QByteArray("rewritten:") + mapped->toUtf8();
    }

    QStringList calls;
};

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

void TestFileManagerRename::renameRewritesEveryFrontmatterLinkRegardlessOfKeyOrder()
{
    // `CachedMetadata::frontmatterLinks` is produced by iterating a
    // QJsonObject (MetadataParser.cpp:467-471), so it arrives in sorted-key
    // order, not document order. Keys `zeta` then `alpha` therefore arrive
    // as `alpha` then `zeta` — the reverse of where they sit on disk.
    RenameHarness h;
    primeHarness(h, {
        {QStringLiteral("Foo.md"), "Foo body"},
        {QStringLiteral("Bar.md"), "Bar body"},
        {QStringLiteral("Linker.md"),
         "---\nzeta: \"[[Foo]]\"\nalpha: \"[[Bar]]\"\n---\nbody"},
    });

    auto *foo = h.vault.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    QVERIFY(h.fm->renameFile(foo, QStringLiteral("Renamed.md")));

    QCOMPARE(readFileAll(h.dir.path() + "/Linker.md"),
             QByteArray("---\nzeta: \"[[Renamed]]\"\nalpha: \"[[Bar]]\"\n---\nbody"));
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

// QSet iteration is randomised per process, so the per-linker rewrite order was
// not reproducible. Vault::modify emits `modified` per rewritten file, which is
// the only place that order is observable (linkUpdateProgress carries a counter,
// not a path).
//
// Note this is *not* reachable through the written bytes: for a bare `[[Foo]]`
// every candidate move has the same completeBaseName, so
// `rewriteLinkLiteral` emits the same text whichever move matchMove picks.
void TestFileManagerRename::emitsProgressInAscendingSourceOrder()
{
    RenameHarness h;
    primeHarness(h, {
        {QStringLiteral("Foo.md"), "x"},
        {QStringLiteral("D.md"), "[[Foo]]"},
        {QStringLiteral("A.md"), "[[Foo]]"},
        {QStringLiteral("C.md"), "[[Foo]]"},
        {QStringLiteral("B.md"), "[[Foo]]"},
    });

    QSignalSpy progress(h.fm.get(), &Corbomite::FileManager::linkUpdateProgress);
    QSignalSpy modified(&h.vault, &Corbomite::Vault::modified);

    auto *foo = h.vault.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    QVERIFY(h.fm->renameFile(foo, QStringLiteral("Bar.md")));

    QStringList rewritten;
    for (const QList<QVariant> &args : modified) {
        rewritten << args.first().value<Corbomite::TFile *>()->path;
    }
    QCOMPARE(rewritten, QStringList({QStringLiteral("A.md"),
                                     QStringLiteral("B.md"),
                                     QStringLiteral("C.md"),
                                     QStringLiteral("D.md")}));
    QCOMPARE(progress.count(), 4);
    for (const QString &rel : rewritten) {
        QCOMPARE(readFileAll(h.dir.path() + "/" + rel), QByteArray("[[Bar]]"));
    }
}

// M6.2 seam: a source whose extension has a registered LinkRewriter is
// handed to that rewriter instead of the Markdown byte-splice, and the
// bytes it returns are written through the vault. The canvas rewriter
// itself is tested in libs/canvas.
void TestFileManagerRename::renameRoutesNonMarkdownThroughLinkRewriter()
{
    RenameHarness h;
    primeHarness(h, {
        {QStringLiteral("Foo.md"), "Foo body"},
        {QStringLiteral("board.canvas"),
         QByteArrayLiteral(
             R"({"nodes":[{"id":"n1","type":"file","file":"Foo.md",)"
             R"("x":0,"y":0,"width":250,"height":60}],"edges":[]})")},
    });

    StubRewriter stub;
    h.fm->registerLinkRewriter(&stub);

    auto *foo = h.vault.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    QVERIFY(h.fm->renameFile(foo, QStringLiteral("Bar.md")));

    // The canvas was detected via its indexed embed and routed to the stub,
    // which saw the mapper resolve Foo.md -> Bar.md.
    QCOMPARE(stub.calls, QStringList{QStringLiteral("board.canvas")});
    QCOMPARE(readFileAll(h.dir.path() + "/board.canvas"),
             QByteArray("rewritten:Bar.md"));
}

// Parity with Obsidian's `ZF` failure mode (`app/own/1304-ZF.js:22-24`): when
// the frontmatter will not parse, the body link splices still apply but the
// frontmatter links are silently left stale. Corbomite reaches the same result
// by a different route — the parser yields no `frontmatterLinks` for an
// unparseable block — and this pins it so a future "fix" does not silently
// change the contract.
void TestFileManagerRename::renameWithUnparseableFrontmatterRewritesBodyOnly()
{
    RenameHarness h;
    primeHarness(h, {
        {QStringLiteral("Foo.md"), "Foo body"},
        {QStringLiteral("Linker.md"),
         QByteArrayLiteral("---\ntitle: \"unterminated\nrelated: \"[[Foo]]\"\n---\n"
                            "See [[Foo]]\n")},
    });

    auto *foo = h.vault.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    QVERIFY(h.fm->renameFile(foo, QStringLiteral("Bar.md")));

    const QByteArray after = readFileAll(h.dir.path() + "/Linker.md");
    QVERIFY2(after.contains("See [[Bar]]"), "body link must be rewritten");
    // The unparseable frontmatter is left byte-identical (stale link and all).
    QVERIFY2(after.contains("related: \"[[Foo]]\""),
             "frontmatter must be left untouched on a parse error (Obsidian parity)");
}

QTEST_MAIN(TestFileManagerRename)
#include "tst_file_manager_rename.moc"
