// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster V.2 Phase 3 end-to-end test: proves the wired
// MetadataCache::open / close path (which delegates to
// CachedMetadataStore::loadInto / persistFrom) survives a real-vault
// open / populate / close / reopen cycle. Mirrors the lifecycle invoked
// by MainWindow::onVaultOpened / onVaultClosed.

#include <QTest>
#include <QSignalSpy>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"

using namespace Corbomite;

namespace {

void writeFile(const QString &absPath, const QByteArray &body)
{
    QFile f(absPath);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(absPath));
    f.write(body);
    f.close();
}

} // namespace

class TestCachedMetadataStoreE2E : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // End-to-end: open MetadataCache against a fresh DB, populate via the
    // real rebuildVault path (the same call site MainWindow::onVaultOpened
    // uses), close (which persists), reopen, and verify the snapshot was
    // restored. This is the integration gate for the Phase 3 wiring; the
    // unit-level loadInto/persistFrom round-trip is covered by
    // tst_cachedmetadatastore.cpp.
    void testRealVaultRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString vaultPath = dir.path();
        QVERIFY(QDir(vaultPath).mkpath(QStringLiteral(".obsidian")));

        writeFile(QDir(vaultPath).filePath(QStringLiteral("a.md")),
                  QByteArrayLiteral("# A\n\nlink to [[b]]\n"));
        writeFile(QDir(vaultPath).filePath(QStringLiteral("b.md")),
                  QByteArrayLiteral("# B\n\nback to [[a]]\n"));
        writeFile(QDir(vaultPath).filePath(QStringLiteral("c.md")),
                  QByteArrayLiteral("# C\n\n#tag\n"));

        const QStringList notePaths = {
            QStringLiteral("a.md"),
            QStringLiteral("b.md"),
            QStringLiteral("c.md"),
        };

        const QString dbPath =
            QDir(vaultPath).filePath(QStringLiteral(".obsidian/metadata-cache.db"));

        // Round 1: open + populate via rebuildVault + close. close()
        // routes through MetadataCache::persistNow ->
        // CachedMetadataStore::persistFrom.
        {
            LinkResolver resolver;
            resolver.setVaultPaths(notePaths);

            MetadataCache cache(resolver);
            QVERIFY(cache.open(dbPath));

            QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);
            cache.rebuildVault(vaultPath, notePaths);
            QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);

            QCOMPARE(cache.fileCount(), 3);

            cache.close();
        }

        QVERIFY2(QFile::exists(dbPath),
                 "metadata-cache.db should exist after close()");

        // Round 2: fresh MetadataCache, reopen the same DB, verify state
        // was loaded by open() -> CachedMetadataStore::loadInto. No
        // rebuildVault call -- we are testing pure restore.
        {
            LinkResolver resolver;
            resolver.setVaultPaths(notePaths);

            MetadataCache cache(resolver);
            QVERIFY(cache.open(dbPath));

            QCOMPARE(cache.fileCount(), 3);

            const auto entries = cache.pathToFileEntrySnapshot();
            QCOMPARE(entries.size(), 3);
            QVERIFY(entries.contains(QStringLiteral("a.md")));
            QVERIFY(entries.contains(QStringLiteral("b.md")));
            QVERIFY(entries.contains(QStringLiteral("c.md")));

            // Spot-check that parsed metadata (not just the path entry)
            // round-tripped: each note has its heading restored.
            const auto a = cache.getFileCache(QStringLiteral("a.md"));
            QVERIFY(a.has_value());
            QVERIFY(a->headings.has_value());
            QCOMPARE(a->headings->at(0).heading, QStringLiteral("A"));

            const auto b = cache.getFileCache(QStringLiteral("b.md"));
            QVERIFY(b.has_value());
            QVERIFY(b->headings.has_value());
            QCOMPARE(b->headings->at(0).heading, QStringLiteral("B"));

            const auto c = cache.getFileCache(QStringLiteral("c.md"));
            QVERIFY(c.has_value());
            QVERIFY(c->headings.has_value());
            QCOMPARE(c->headings->at(0).heading, QStringLiteral("C"));

            cache.close();
        }
    }
};

QTEST_MAIN(TestCachedMetadataStoreE2E)
#include "tst_cachedmetadatastore_e2e.moc"
