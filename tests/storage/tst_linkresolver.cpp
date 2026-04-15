// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/storage/LinkResolver.h"

using namespace Corbomite;

class TestLinkResolver : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // --- Step 1: empty linktext → self ---

    void step1_emptyLinktextResolvesToSelf()
    {
        LinkResolver r;
        r.setVaultPaths({QStringLiteral("a.md"), QStringLiteral("b/c.md")});
        const auto result = r.resolve(QStringLiteral("b/c.md"), QString());
        QVERIFY(result.resolved);
        QCOMPARE(result.path, QStringLiteral("b/c.md"));
    }

    void step1_emptyLinktextWithoutSourceUnresolved()
    {
        LinkResolver r;
        r.setVaultPaths({QStringLiteral("a.md")});
        const auto result = r.resolve(QString(), QString());
        QVERIFY(!result.resolved);
        QVERIFY(result.path.isEmpty());
    }

    // --- Step 2 + 3: extension / bare ---

    void step2_bareBasenameResolves()
    {
        LinkResolver r;
        r.setVaultPaths({QStringLiteral("Note.md")});
        const auto result = r.resolve(QStringLiteral("src.md"), QStringLiteral("Note"));
        QVERIFY(result.resolved);
        QCOMPARE(result.path, QStringLiteral("Note.md"));
    }

    void step2_explicitExtensionResolves()
    {
        LinkResolver r;
        r.setVaultPaths({QStringLiteral("Image.png")});
        const auto result = r.resolve(QStringLiteral("src.md"), QStringLiteral("Image.png"));
        QVERIFY(result.resolved);
        QCOMPARE(result.path, QStringLiteral("Image.png"));
    }

    void step2_caseInsensitiveLookup()
    {
        LinkResolver r;
        r.setVaultPaths({QStringLiteral("folder/MyNote.md")});
        const auto result = r.resolve(QStringLiteral("src.md"), QStringLiteral("mynote"));
        QVERIFY(result.resolved);
        QCOMPARE(result.path, QStringLiteral("folder/MyNote.md"));
    }

    // --- Step 4: relative paths ---

    void step4_dotSlashSibling()
    {
        LinkResolver r;
        r.setVaultPaths({
            QStringLiteral("folder/a.md"),
            QStringLiteral("folder/b.md"),
        });
        const auto result = r.resolve(QStringLiteral("folder/a.md"),
                                      QStringLiteral("./b"));
        QVERIFY(result.resolved);
        QCOMPARE(result.path, QStringLiteral("folder/b.md"));
    }

    void step4_dotDotParent()
    {
        LinkResolver r;
        r.setVaultPaths({
            QStringLiteral("top.md"),
            QStringLiteral("folder/a.md"),
        });
        const auto result = r.resolve(QStringLiteral("folder/a.md"),
                                      QStringLiteral("../top"));
        QVERIFY(result.resolved);
        QCOMPARE(result.path, QStringLiteral("top.md"));
    }

    void step4_dotRelativeMissReturnsUnresolved()
    {
        LinkResolver r;
        r.setVaultPaths({QStringLiteral("elsewhere/x.md")});
        const auto result = r.resolve(QStringLiteral("folder/a.md"),
                                      QStringLiteral("./missing"));
        QVERIFY(!result.resolved);
    }

    // --- Step 5: rooted absolute ---

    void step5_rootedExactMatch()
    {
        LinkResolver r;
        r.setVaultPaths({
            QStringLiteral("deep/nested/target.md"),
            QStringLiteral("target.md"),
        });
        const auto result = r.resolve(QStringLiteral("src.md"),
                                      QStringLiteral("/deep/nested/target"));
        QVERIFY(result.resolved);
        QCOMPARE(result.path, QStringLiteral("deep/nested/target.md"));
    }

    void step5_rootedMissReturnsUnresolvedEvenWithBasenameInVault()
    {
        // Audit §8.5: rooted miss does NOT fall through to step 6.
        LinkResolver r;
        r.setVaultPaths({QStringLiteral("elsewhere/target.md")});
        const auto result = r.resolve(QStringLiteral("src.md"),
                                      QStringLiteral("/wrongpath/target"));
        QVERIFY(!result.resolved);
    }

    // --- Step 6: shortest-path-wins under ambiguity ---

    void step6_shortestPathWins()
    {
        LinkResolver r;
        r.setVaultPaths({
            QStringLiteral("a/b/c/Note.md"),
            QStringLiteral("Note.md"),
            QStringLiteral("d/Note.md"),
        });
        // From an unrelated source, shortest path wins.
        const auto result = r.resolve(QStringLiteral("other/src.md"),
                                      QStringLiteral("Note"));
        QVERIFY(result.resolved);
        QCOMPARE(result.path, QStringLiteral("Note.md"));
    }

    void step6_sameFolderBeatsShorter()
    {
        LinkResolver r;
        r.setVaultPaths({
            QStringLiteral("Note.md"),              // depth 0
            QStringLiteral("folder/Note.md"),       // depth 1 — same-folder as source
        });
        const auto result = r.resolve(QStringLiteral("folder/src.md"),
                                      QStringLiteral("Note"));
        QVERIFY(result.resolved);
        // Same-folder bucket wins even though root is shorter.
        QCOMPARE(result.path, QStringLiteral("folder/Note.md"));
    }

    void step6_alphaTiebreakOnEqualDepth()
    {
        LinkResolver r;
        r.setVaultPaths({
            QStringLiteral("zfolder/Note.md"),
            QStringLiteral("afolder/Note.md"),
        });
        const auto result = r.resolve(QStringLiteral("src.md"),
                                      QStringLiteral("Note"));
        QVERIFY(result.resolved);
        QCOMPARE(result.path, QStringLiteral("afolder/Note.md"));
    }

    // --- subpath passthrough ---

    void subpath_headingSplitPreserved()
    {
        LinkResolver r;
        r.setVaultPaths({QStringLiteral("Note.md")});
        const auto result = r.resolve(QStringLiteral("src.md"),
                                      QStringLiteral("Note#Section"));
        QVERIFY(result.resolved);
        QCOMPARE(result.path, QStringLiteral("Note.md"));
        QCOMPARE(result.subpath, QStringLiteral("#Section"));
    }

    void subpath_blockIdPreserved()
    {
        LinkResolver r;
        r.setVaultPaths({QStringLiteral("Note.md")});
        const auto result = r.resolve(QStringLiteral("src.md"),
                                      QStringLiteral("Note#^block"));
        QVERIFY(result.resolved);
        QCOMPARE(result.path, QStringLiteral("Note.md"));
        QCOMPARE(result.subpath, QStringLiteral("#^block"));
    }

    void subpath_emptyWhenNoHash()
    {
        LinkResolver r;
        r.setVaultPaths({QStringLiteral("Note.md")});
        const auto result = r.resolve(QStringLiteral("src.md"),
                                      QStringLiteral("Note"));
        QVERIFY(result.subpath.isEmpty());
    }

    // --- incremental add/remove ---

    void addAndRemovePath()
    {
        LinkResolver r;
        r.addVaultPath(QStringLiteral("a.md"));
        QCOMPARE(r.candidateCount(QStringLiteral("a.md")), 1);

        r.addVaultPath(QStringLiteral("b/a.md"));
        QCOMPARE(r.candidateCount(QStringLiteral("a.md")), 2);

        r.removeVaultPath(QStringLiteral("a.md"));
        QCOMPARE(r.candidateCount(QStringLiteral("a.md")), 1);
        const auto result = r.resolve(QStringLiteral("src.md"),
                                      QStringLiteral("a"));
        QVERIFY(result.resolved);
        QCOMPARE(result.path, QStringLiteral("b/a.md"));
    }

    // --- unresolved miss ---

    void unresolvedBasenameReturnsEmpty()
    {
        LinkResolver r;
        r.setVaultPaths({QStringLiteral("a.md")});
        const auto result = r.resolve(QStringLiteral("src.md"),
                                      QStringLiteral("nonexistent"));
        QVERIFY(!result.resolved);
        QVERIFY(result.path.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestLinkResolver)
#include "tst_linkresolver.moc"
