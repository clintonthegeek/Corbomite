// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>

#include "corbomite/storage/CaseSensitivityProbe.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/IgnoreFilter.h"
#include "corbomite/vault/VaultScanner.h"
// VaultTrash deleted in Q.0 Phase 3 Task 3.7 — its behavior lives on
// Vault::trash now, covered by libs/vault/tests/tst_vault_trash.cpp.

using namespace Corbomite;

class TestProbeTrashIgnore : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // --- CaseSensitivityProbe ---

    void probeOnHostFilesystem()
    {
        // We don't assert a specific result — it's filesystem-dependent.
        // We just assert the probe doesn't crash and cleans up after itself.
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        const bool result = CaseSensitivityProbe::isCaseSensitive(&fs, tmp.path());
        (void)result;
        // After the probe, the tmp dir should be clean of any case-probe file.
        const auto entries = fs.list(tmp.path());
        for (const auto &e : entries) {
            QVERIFY(!e.startsWith(QStringLiteral(".case-probe-")));
        }
    }

    void probeHandlesBadDirGracefully()
    {
        FileSystemAdapter fs;
        // Writing to a non-existent deep path should fail and the probe should
        // return conservative default (true — case-sensitive).
        const bool r = CaseSensitivityProbe::isCaseSensitive(
            &fs, QStringLiteral("/definitely/not/a/writable/path"));
        QVERIFY(r);
    }

    // --- VaultTrash tests deleted (class deleted in Q.0 P3 T3.7).
    //     Replacement coverage: libs/vault/tests/tst_vault_trash.cpp. ---

    // --- IgnoreFilter ---

    void plainPrefixMatchesAnchored()
    {
        const auto f = IgnoreFilter::fromPatterns({QStringLiteral("tmp/")});
        QVERIFY(f.matches(QStringLiteral("tmp/scratch.md")));
        QVERIFY(f.matches(QStringLiteral("tmp/nested/file.md")));
        QVERIFY(!f.matches(QStringLiteral("other/tmp/file.md"))); // anchored at start
    }

    void plainPrefixEscapesRegexMetachars()
    {
        const auto f = IgnoreFilter::fromPatterns({QStringLiteral("a.b/")});
        // "a.b/" must match literal, not "aXb/"
        QVERIFY(f.matches(QStringLiteral("a.b/x.md")));
        QVERIFY(!f.matches(QStringLiteral("axb/x.md")));
    }

    void regexForm()
    {
        // /\.(bak|tmp)$/ — anything ending in .bak or .tmp
        const auto f = IgnoreFilter::fromPatterns(
            {QStringLiteral("/\\.(bak|tmp)$/")});
        QVERIFY(f.matches(QStringLiteral("notes.bak")));
        QVERIFY(f.matches(QStringLiteral("notes.tmp")));
        QVERIFY(!f.matches(QStringLiteral("notes.md")));
    }

    void invalidRegexSilentlySkipped()
    {
        const auto f = IgnoreFilter::fromPatterns(
            {QStringLiteral("/(/"), QStringLiteral("scratch/")});
        // Only the valid pattern survives.
        QCOMPARE(f.patternCount(), 1);
        QVERIFY(f.matches(QStringLiteral("scratch/x.md")));
    }

    void emptyFilterMatchesNothing()
    {
        IgnoreFilter f;
        QVERIFY(!f.matches(QStringLiteral("anything.md")));
    }

    // --- IgnoreFilter wired into VaultScanner ---

    void scannerHonoursIgnoreFilter()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        fs.write(tmp.path() + QStringLiteral("/kept.md"), QStringLiteral("a"));
        fs.write(tmp.path() + QStringLiteral("/tmp/scratch.md"),
                 QStringLiteral("b"));
        fs.write(tmp.path() + QStringLiteral("/drafts/inprogress.md"),
                 QStringLiteral("c"));

        VaultScanner scanner;
        scanner.setIgnoreFilter(IgnoreFilter::fromPatterns(
            {QStringLiteral("tmp/"), QStringLiteral("drafts/")}));

        const auto notes = scanner.scan(tmp.path());
        QCOMPARE(notes.size(), 1);
        QCOMPARE(notes[0].relativePath, QStringLiteral("kept.md"));
    }

    void scannerWithoutFilterBehavesAsBefore()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        fs.write(tmp.path() + QStringLiteral("/a.md"), QStringLiteral("a"));
        fs.write(tmp.path() + QStringLiteral("/b/c.md"), QStringLiteral("c"));

        VaultScanner scanner; // no filter installed
        const auto notes = scanner.scan(tmp.path());
        QCOMPARE(notes.size(), 2);
    }
};

QTEST_APPLESS_MAIN(TestProbeTrashIgnore)
#include "tst_probe_trash_ignore.moc"
