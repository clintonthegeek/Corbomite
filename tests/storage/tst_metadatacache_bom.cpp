// SPDX-License-Identifier: GPL-3.0-or-later
//
// Regression test: rebuildVault must strip a leading UTF-8 BOM so that
// BOM'd files produce the same cache-hash as the same file read through
// Vault::read (which strips the BOM). Without the strip in rebuildVault
// the raw-bytes SHA-256 differs from the stripped-bytes SHA-256, causing
// unnecessary re-parses and a hash inconsistency between the two read paths.
//
// Note: Qt's QString::fromUtf8() automatically strips a UTF-8 BOM, so
// frontmatter YAML is parsed correctly in both code paths regardless.
// The observable regression is therefore the hash divergence: a file read
// through Vault::read lands with hash H1 (BOM-stripped), but the same file
// loaded through rebuildVault lands with hash H2 (BOM-present) != H1.
// After the fix, both paths produce H1.

#include <QTest>
#include <QSignalSpy>

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"

using namespace Corbomite;

namespace {
QString sha256Hex(const QByteArray &data)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}
} // namespace

class TestMetadataCacheBom : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // 1. A BOM-prefixed file fed through rebuildVault must produce the same
    //    cache-hash as the BOM-stripped bytes. This verifies the read path
    //    inside rebuildVault strips the BOM before hashing and parsing,
    //    matching Vault::read semantics (Obsidian spec §3).
    void testRebuildVaultStripsUtf8Bom()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QByteArray bom = QByteArray::fromHex("efbbbf");
        const QByteArray strippedBody =
            "---\n"
            "title: Hello\n"
            "tags: [foo]\n"
            "---\n"
            "# Body heading\n"
            "#bar\n";
        const QByteArray fileBytes = bom + strippedBody;

        // Hash of the stripped bytes — this is what Vault::read would produce
        // and therefore what onFileChanged should see from rebuildVault.
        const QString expectedHash = sha256Hex(strippedBody);

        const QString fileName = QStringLiteral("bom_note.md");
        QFile f(QDir(dir.path()).filePath(fileName));
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write(fileBytes) == fileBytes.size());
        f.close();

        const QStringList rel = {fileName};
        LinkResolver resolver;
        resolver.setVaultPaths(rel);

        MetadataCache cache(resolver);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        cache.rebuildVault(dir.path(), rel);

        // Wait up to 5s for the async worker + debounce to complete.
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);

        QCOMPARE(cache.fileCount(), 1);

        // The hash stored in the cache must equal the stripped-body hash.
        // Before the fix, this would be sha256(BOM + body) != sha256(body).
        const QString actualHash = cache.getFileHash(fileName);
        QVERIFY(!actualHash.isEmpty());
        QCOMPARE(actualHash, expectedHash);

        // Frontmatter must be indexed (verifies parse ran correctly).
        const auto maybe = cache.getFileCache(fileName);
        QVERIFY(maybe.has_value());
        QVERIFY2(maybe->frontmatter.has_value(),
                 "frontmatter must be indexed for a BOM-prefixed file");
        const QJsonObject fm = *maybe->frontmatter;
        QVERIFY(fm.contains(QStringLiteral("title")));
        QCOMPARE(fm.value(QStringLiteral("title")).toString(),
                 QStringLiteral("Hello"));

        // Headings must also be present.
        QVERIFY(maybe->headings.has_value());
        QVERIFY(!maybe->headings->isEmpty());
        QCOMPARE(maybe->headings->at(0).heading, QStringLiteral("Body heading"));
    }

    // 2. A non-BOM file must be unaffected by the BOM-strip logic.
    void testRebuildVaultNonBomFileUnaffected()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QByteArray body =
            "---\n"
            "title: Plain\n"
            "---\n"
            "# Plain heading\n";
        const QString expectedHash = sha256Hex(body);

        const QString fileName = QStringLiteral("plain_note.md");
        QFile f(QDir(dir.path()).filePath(fileName));
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write(body) == body.size());
        f.close();

        const QStringList rel = {fileName};
        LinkResolver resolver;
        resolver.setVaultPaths(rel);

        MetadataCache cache(resolver);
        QSignalSpy finishedSpy(&cache, &MetadataCache::indexFinished);

        cache.rebuildVault(dir.path(), rel);
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);

        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.getFileHash(fileName), expectedHash);

        const auto maybe = cache.getFileCache(fileName);
        QVERIFY(maybe.has_value());
        QVERIFY(maybe->frontmatter.has_value());
        QCOMPARE(maybe->frontmatter->value(QStringLiteral("title")).toString(),
                 QStringLiteral("Plain"));
    }
};

QTEST_MAIN(TestMetadataCacheBom)
#include "tst_metadatacache_bom.moc"
