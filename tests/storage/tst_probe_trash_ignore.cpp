// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>

#include "corbomite/storage/CaseSensitivityProbe.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/IgnoreFilter.h"
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

    // Entries are regexes, not globs (app/own/1306-eR.js:588): a bare entry is
    // escaped and anchored, so a `*` in it is a literal asterisk. `/^Archive/`
    // is what glob-ish intent actually requires.
    void bareEntryIsALiteralNotAGlob()
    {
        const auto f = IgnoreFilter::fromPatterns({QStringLiteral("Archive*")});
        QCOMPARE(f.patternCount(), 1);
        QVERIFY(f.matches(QStringLiteral("Archive*")));
        QVERIFY(!f.matches(QStringLiteral("Archive2026")));
        QVERIFY(!f.matches(QStringLiteral("Archive.md")));

        const auto anchored = IgnoreFilter::fromPatterns({QStringLiteral("/^Archive/")});
        QCOMPARE(anchored.patternCount(), 1);
        QVERIFY(anchored.matches(QStringLiteral("Archive2026")));
        QVERIFY(anchored.matches(QStringLiteral("Archive.md")));
    }

    void regexFormIsUnanchored()
    {
        const auto f = IgnoreFilter::fromPatterns({QStringLiteral("/Archive/")});
        QVERIFY(f.matches(QStringLiteral("Archive/old.md")));
        // Unanchored: matches mid-path, unlike the bare-prefix form.
        QVERIFY(f.matches(QStringLiteral("deep/nested/Archive/old.md")));
        const auto bare = IgnoreFilter::fromPatterns({QStringLiteral("Archive/")});
        QVERIFY(!bare.matches(QStringLiteral("deep/nested/Archive/old.md")));
        QVERIFY(!f.matches(QStringLiteral("notes.md")));
    }

    // Obsidian passes the `i` flag to *both* RegExp constructions
    // (app/own/1306-eR.js:588), so both forms are case-insensitive.
    void bothFormsAreCaseInsensitive_data()
    {
        QTest::addColumn<QString>("entry");
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("matches");

        QTest::newRow("prefix upper")   << "tmp/"        << "TMP/Scratch.md"   << true;
        QTest::newRow("prefix mixed")   << "Drafts/"     << "drafts/wip.md"    << true;
        QTest::newRow("prefix no match")<< "tmp/"        << "notes/tmp/x.md"   << false;
        QTest::newRow("regex upper")    << "/^archive/"  << "ARCHIVE/2026.md"  << true;
        QTest::newRow("regex mixed")    << "/bak$/"      << "notes.BAK"        << true;
        QTest::newRow("regex no match") << "/^archive/"  << "old/archive.md"   << false;
    }

    void bothFormsAreCaseInsensitive()
    {
        QFETCH(QString, entry);
        QFETCH(QString, path);
        QFETCH(bool, matches);
        QCOMPARE(IgnoreFilter::fromPatterns({entry}).matches(path), matches);
    }

    // Each entry is trimmed before use (app/own/1306-eR.js:586), so a stray
    // space in the settings field cannot silently narrow a filter to nothing.
    void entriesAreTrimmedBeforeUse_data()
    {
        QTest::addColumn<QString>("entry");
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("matches");

        QTest::newRow("leading space") << "  tmp/"      << "tmp/x.md"        << true;
        QTest::newRow("trailing space")<< "tmp/  "      << "tmp/x.md"        << true;
        QTest::newRow("regex padded")  << "  /^tmp/  "  << "tmp/x.md"        << true;
        QTest::newRow("pad loses match") << "tmp/ "      << "notes/tmp/x.md"  << false;
    }

    void entriesAreTrimmedBeforeUse()
    {
        QFETCH(QString, entry);
        QFETCH(QString, path);
        QFETCH(bool, matches);
        QCOMPARE(IgnoreFilter::fromPatterns({entry}).matches(path), matches);
    }

    // A whitespace-only entry trims to empty and is dropped
    // (app/own/1306-eR.js:587). Untrimmed it would compile to a prefix regex
    // that matches indented paths.
    void whitespaceOnlyEntriesAreDropped()
    {
        const auto f = IgnoreFilter::fromPatterns({QStringLiteral("   ")});
        QCOMPARE(f.patternCount(), 0);
        QVERIFY(!f.matches(QStringLiteral("  indented.md")));
        QVERIFY(!f.matches(QStringLiteral("normal.md")));
    }

    // The `/…/` form needs length > 2 (app/own/1306-eR.js:588), so a bare
    // `//` is an escaped literal `^//` — not an empty regex that would match
    // every path in the vault.
    void doubleSlashIsALiteralNotAnEmptyRegex()
    {
        const auto f = IgnoreFilter::fromPatterns({QStringLiteral("//")});
        QCOMPARE(f.patternCount(), 1);
        QVERIFY(!f.matches(QStringLiteral("anything.md")));
        QVERIFY(!f.matches(QStringLiteral("dir/file.md")));

        // A single slash is likewise a literal.
        const auto single = IgnoreFilter::fromPatterns({QStringLiteral("/")});
        QCOMPARE(single.patternCount(), 1);
        QVERIFY(!single.matches(QStringLiteral("anything.md")));
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

    // The filter is matched on the full vault-relative path and is
    // case-insensitive; a bare entry is an anchored literal prefix, so
    // `tmp/` does not match a nested `Notes/tmp/x.md`.
    void bareEntryIsAnchoredPrefix()
    {
        const auto f = IgnoreFilter::fromPatterns({QStringLiteral("drafts/")});
        QVERIFY(f.matches(QStringLiteral("drafts/wip.md")));
        QVERIFY(f.matches(QStringLiteral("DRAFTS/WIP.MD")));
        QVERIFY(!f.matches(QStringLiteral("Notes/drafts/wip.md")));
    }

    // A `/…/` entry is a raw unanchored regex.
    void slashedEntryIsUnanchoredRegex()
    {
        const auto f = IgnoreFilter::fromPatterns({QStringLiteral("/^private/")});
        QVERIFY(f.matches(QStringLiteral("private/secret.md")));
        QVERIFY(!f.matches(QStringLiteral("Notes/private/secret.md")));
    }
};

QTEST_APPLESS_MAIN(TestProbeTrashIgnore)
#include "tst_probe_trash_ignore.moc"
