// SPDX-License-Identifier: GPL-3.0-or-later
//
// Container iteration order must not reach anything a user or the filesystem
// can observe.
//
// Three containers in this corner of the tree iterate in an order that is not
// a function of their contents:
//
//   * `Vault::m_fileMap` is a `std::unordered_map<QString, …, QStringStdHash>`,
//     so `getFiles()` / `getMarkdownFiles()` / `getAllLoadedFiles()` returned
//     bucket order — which depends on the order entries were inserted, i.e. on
//     the vault's directory-listing and creation history rather than on the
//     vault. Obsidian's three equivalents walk the tree or iterate `fileMap`
//     in insertion order (`app/own/1129-UD.js:546-551,602-622`), both stable.
//     `MainWindow` seeds `LinkResolver::setVaultPaths` straight from
//     `getFiles()`, so the resolver inherited the same instability.
//   * `MetadataCache::m_pathToFileEntry` is a QHash, and QHash iteration order
//     is randomised per process, so `allPaths()` returned a different order on
//     every launch. That order reaches disk: `SQLiteIndex::reconcileWithCache`
//     inserts rows in it, so the on-disk row order of `links` differed run to
//     run.
//   * `LinkResolver`'s candidate comparator tied on equal depth + equal
//     lowercased path (e.g. `B/Foo.md` vs `b/foo.md`), leaving `std::sort` to
//     pick the winner from arrival order — so which file a `[[Foo]]` opened was
//     not reproducible.

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include "corbomite/core/DataAdapter.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

namespace {

QStringList pathsOf(const QVector<TFile *> &files)
{
    QStringList out;
    for (const TFile *f : files) out << f->path;
    return out;
}

QStringList pathsOf(const QVector<TAbstractFile *> &files)
{
    QStringList out;
    for (const TAbstractFile *f : files) out << f->path;
    return out;
}

} // namespace

class TstIterationOrder : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void vaultListsAreInAscendingPathOrder();
    void vaultListsAreSortedForFoldersToo();
    void metadataCacheAllPathsIsSorted();
    void sqliteIndexRowsFollowAllPathsOrder();
    void cachedMetadataStoreWritesRowsInAscendingKeyOrder();
    void rebuildVaultDeletesStaleEntriesInAscendingPathOrder();
    void linkResolverIgnoresVaultPathArrivalOrder();
};

// The files are created in reverse-alphabetical order and live in two folders,
// so a hash-order or directory-listing-order result cannot coincide with
// ascending path order for more than one of the permutations.
void TstIterationOrder::vaultListsAreInAscendingPathOrder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FileSystemAdapter fs;
    Vault vault(&fs);
    vault.load(dir.path());

    for (const char *rel : {"zeta.md", "mid.md", "alpha.md", "notes/b.md",
                            "notes/a.md", "notes/deep/c.md"}) {
        QVERIFY(vault.create(QString::fromLatin1(rel), "body"));
    }

    const QStringList expected{
        QStringLiteral("alpha.md"), QStringLiteral("mid.md"),
        QStringLiteral("notes/a.md"), QStringLiteral("notes/b.md"),
        QStringLiteral("notes/deep/c.md"), QStringLiteral("zeta.md")};
    QCOMPARE(pathsOf(vault.getFiles()), expected);
    QCOMPARE(pathsOf(vault.getMarkdownFiles()), expected);

    QStringList nonMarkdown{QStringLiteral("alpha.md"),
                            QStringLiteral("mid.md"),
                            QStringLiteral("notes/a.md"),
                            QStringLiteral("notes/b.md"),
                            QStringLiteral("notes/deep/c.md"),
                            QStringLiteral("zeta.md")};
    nonMarkdown.sort();
    QCOMPARE(pathsOf(vault.getFiles()), nonMarkdown);

    // getAllLoadedFiles() covers folders as well and still excludes the root.
    const QStringList all = pathsOf(vault.getAllLoadedFiles());
    QCOMPARE(all.size(), 8);   // 6 files + notes + notes/deep
    QVERIFY(!all.contains(QStringLiteral("/")));
    QStringList sorted = all;
    sorted.sort();
    QCOMPARE(all, sorted);
    QVERIFY(all.contains(QStringLiteral("notes")));
    QVERIFY(all.contains(QStringLiteral("notes/deep")));
}

// Non-markdown entries must not disturb the ordering of the markdown ones, and
// the full list has to interleave folders and files by path.
void TstIterationOrder::vaultListsAreSortedForFoldersToo()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FileSystemAdapter fs;
    Vault vault(&fs);
    vault.load(dir.path());

    for (const char *rel : {"z.md", "img/mid.png", "m.md", "img/a.png",
                            "b.txt", "a.md"}) {
        const QString r = QString::fromLatin1(rel);
        QVERIFY(vault.createBinary(r, "body"));
    }

    QCOMPARE(pathsOf(vault.getMarkdownFiles()),
             QStringList({QStringLiteral("a.md"), QStringLiteral("m.md"),
                          QStringLiteral("z.md")}));
    QCOMPARE(pathsOf(vault.getFiles()),
             QStringList({QStringLiteral("a.md"), QStringLiteral("b.txt"),
                          QStringLiteral("img/a.png"),
                          QStringLiteral("img/mid.png"),
                          QStringLiteral("m.md"), QStringLiteral("z.md")}));

    const QStringList all = pathsOf(vault.getAllLoadedFiles());
    QStringList sorted = all;
    sorted.sort();
    QCOMPARE(all, sorted);
}

void TstIterationOrder::metadataCacheAllPathsIsSorted()
{
    LinkResolver resolver;
    MetadataCache cache(resolver);

    // Fed in a deliberately unsorted order with distinct content so nothing is
    // short-circuited away.
    const QStringList fed{QStringLiteral("zeta.md"), QStringLiteral("mid.md"),
                          QStringLiteral("alpha.md"), QStringLiteral("notes/b.md"),
                          QStringLiteral("notes/a.md")};
    int i = 0;
    for (const QString &rel : fed) {
        cache.onFileChanged(rel, QByteArray("body ") + QByteArray::number(i++),
                            1000 + i);
    }
    QTRY_COMPARE(cache.fileCount(), fed.size());

    QStringList paths = cache.allPaths();
    QCOMPARE(paths.size(), fed.size());
    QStringList sorted = fed;
    sorted.sort();
    QCOMPARE(paths, sorted);
}

void TstIterationOrder::sqliteIndexRowsFollowAllPathsOrder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LinkResolver resolver;
    MetadataCache cache(resolver);

    QStringList notes;
    for (const char *rel : {"zeta.md", "mid.md", "alpha.md", "notes/b.md",
                            "notes/a.md"}) {
        const QString r = QString::fromLatin1(rel);
        notes << r;
        resolver.addVaultPath(r);
    }

    QSignalSpy finished(&cache, &MetadataCache::indexFinished);
    for (int i = 0; i < notes.size(); ++i) {
        cache.onFileChanged(notes.at(i), "[[alpha]] and [[notes/b]]\n", 2000 + i);
    }
    QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), notes.size(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1, 5000);

    // Attach the index only now. setMetadataCache() runs reconcileWithCache()
    // precisely for the case where the cache was populated without any
    // cacheChanged (SQLiteIndex.cpp:80-84), so every row lands in allPaths()
    // order and allPaths() is the only thing deciding the on-disk row order.
    SQLiteIndex index;
    QVERIFY(index.open(dir.path() + "/index.db"));
    index.setMetadataCache(&cache);

    QTRY_COMPARE_WITH_TIMEOUT(index.allLinks().size(), 10, 5000);

    // Stored order, read back with no ORDER BY, must be ascending by source.
    QStringList sources;
    for (const LinkInfo &l : index.allLinks()) {
        if (!sources.contains(l.sourcePath)) sources << l.sourcePath;
    }
    QStringList sorted = sources;
    sorted.sort();
    QCOMPARE(sources, sorted);
}

void TstIterationOrder::cachedMetadataStoreWritesRowsInAscendingKeyOrder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.path() + "/cache.db";

    {
        LinkResolver resolver;
        MetadataCache cache(resolver);
        QVERIFY(cache.open(dbPath));
        const QStringList fed{QStringLiteral("zeta.md"), QStringLiteral("mid.md"),
                              QStringLiteral("alpha.md"),
                              QStringLiteral("notes/b.md"),
                              QStringLiteral("notes/a.md")};
        for (int i = 0; i < fed.size(); ++i) {
            cache.onFileChanged(fed.at(i),
                                QByteArray("body ") + QByteArray::number(i),
                                3000 + i);
        }
        QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), fed.size(), 5000);
        cache.close();   // flushes through CachedMetadataStore::persistFrom
    }

    // Read the file_cache rows back in rowid (= insertion) order with a second
    // connection. This is the order the on-disk file has, and it is what a
    // byte-level diff of two runs' cache files would show.
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                QStringLiteral("roworder"));
    db.setDatabaseName(dbPath);
    QVERIFY(db.open());
    QStringList paths;
    {
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral(
            "SELECT path FROM file_cache ORDER BY rowid")));
        while (q.next()) paths << q.value(0).toString();
    }
    db.close();
    QSqlDatabase::removeDatabase(QStringLiteral("roworder"));

    QStringList expected{QStringLiteral("alpha.md"), QStringLiteral("mid.md"),
                         QStringLiteral("notes/a.md"),
                         QStringLiteral("notes/b.md"),
                         QStringLiteral("zeta.md")};
    QCOMPARE(paths, expected);
}

void TstIterationOrder::rebuildVaultDeletesStaleEntriesInAscendingPathOrder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LinkResolver resolver;
    MetadataCache cache(resolver);
    const QStringList fed{QStringLiteral("zeta.md"), QStringLiteral("mid.md"),
                          QStringLiteral("alpha.md")};
    for (int i = 0; i < fed.size(); ++i) {
        cache.onFileChanged(fed.at(i), QByteArray("body ") + QByteArray::number(i),
                            4000 + i);
    }
    QTRY_COMPARE_WITH_TIMEOUT(cache.fileCount(), fed.size(), 5000);

    // An empty canonical list retires everything; the cacheDeleted fan-out
    // order is what a plugin's reconcile sees on vault open.
    QSignalSpy deleted(&cache, &MetadataCache::cacheDeleted);
    cache.rebuildVault(dir.path(), {});
    QCOMPARE(deleted.count(), 3);
    QStringList seen;
    for (const QList<QVariant> &args : deleted) seen << args.first().toString();
    QCOMPARE(seen, QStringList({QStringLiteral("alpha.md"),
                                QStringLiteral("mid.md"),
                                QStringLiteral("zeta.md")}));
}

void TstIterationOrder::linkResolverIgnoresVaultPathArrivalOrder()
{
    // `B/Foo.md` and `b/foo.md` are the pair a vault authored on a
    // case-insensitive filesystem can end up with on a case-sensitive one.
    // They tie on depth and on lowercased path, so before the raw-path
    // tiebreak the winner was whichever arrived first.
    const QString a = QStringLiteral("B/Foo.md");
    const QString b = QStringLiteral("b/foo.md");
    const QString source = QStringLiteral("notes/src.md");

    LinkResolver forward;
    forward.setVaultPaths({a, b, source});
    LinkResolver reverse;
    reverse.setVaultPaths({b, a, source});

    const ResolvedLink f = forward.resolve(source, QStringLiteral("Foo"));
    const ResolvedLink r = reverse.resolve(source, QStringLiteral("Foo"));
    QVERIFY(f.resolved);
    QVERIFY(r.resolved);
    QCOMPARE(f.path, r.path);
    // Codepoint order, so `B` before `b`.
    QCOMPARE(f.path, a);

    // Same for the rooted branch, which reads the lowercased-key map directly.
    LinkResolver rootedForward;
    rootedForward.setVaultPaths({a, b});
    LinkResolver rootedReverse;
    rootedReverse.setVaultPaths({b, a});
    QCOMPARE(rootedForward.resolve(QString(), QStringLiteral("/B/Foo.md")).path,
             rootedReverse.resolve(QString(), QStringLiteral("/B/Foo.md")).path);
}

QTEST_MAIN(TstIterationOrder)
#include "tst_iteration_order.moc"