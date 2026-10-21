// SPDX-License-Identifier: GPL-3.0-or-later
//
// The one UTF-8 BOM rule (Obsidian spec §3). It used to live as two
// identical private copies — one in Vault::read, one in
// MetadataCache::rebuildVault — and now lives in Corbomite::stripUtf8Bom.
//
// The end-to-end invariant it exists for (the read path and the index path
// must agree on a BOM'd note's hash) is pinned by tst_metadatacache_bom.
// These cases pin the helper itself, so the edge cases are not left to
// whichever caller happens to exercise them.
#include <QTest>

#include "corbomite/storage/TextBytes.h"

using namespace Corbomite;

class TestTextBytes : public QObject
{
    Q_OBJECT

private:
    static QByteArray bom()
    {
        return QByteArray::fromHex("efbbbf");
    }

private Q_SLOTS:

    void testLeadingBomIsStripped()
    {
        QCOMPARE(stripUtf8Bom(bom() + "hello"), QByteArray("hello"));
    }

    void testBomOnlyBecomesEmpty()
    {
        QCOMPARE(stripUtf8Bom(bom()), QByteArray());
    }

    void testNoBomIsUnchanged()
    {
        QCOMPARE(stripUtf8Bom("hello"), QByteArray("hello"));
    }

    void testBomNotAtTheStartIsUntouched()
    {
        // Only a *leading* BOM is a BOM.
        const QByteArray input = "hi" + bom() + "there";
        QCOMPARE(stripUtf8Bom(input), input);
    }

    void testShortInputIsUnchanged()
    {
        QCOMPARE(stripUtf8Bom(QByteArray()), QByteArray());
        QCOMPARE(stripUtf8Bom(QByteArray("a")), QByteArray("a"));
        QCOMPARE(stripUtf8Bom(QByteArray("ab")), QByteArray("ab"));
        // A two-byte prefix of the BOM is not a BOM.
        QCOMPARE(stripUtf8Bom(QByteArray::fromHex("efbb")), QByteArray::fromHex("efbb"));
    }
};

QTEST_MAIN(TestTextBytes)
#include "tst_textbytes.moc"
