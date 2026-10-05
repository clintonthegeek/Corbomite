// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultWatcher : public QObject
{
    Q_OBJECT
private slots:
    void externalCreateEmitsCreated();
    void externalModifyEmitsModified();
    void externalDeleteEmitsDeletedWithTombstone();
    void externalRenameEmitsRenamed();
    void externalFolderRenameUpdatesDescendants();
    void transientEnoentEmitsNoDeleteCreatePair();
    void unloadEmitsClosed();
    void externalChangeEmitsRaw();
    void externalConfigJsonEmitsConfigChanged();
    void externalNonConfigDoesNotEmitConfigChanged();
    void batchCreateEmitsInAscendingPathOrder();
};

namespace {
void writeFile(const QString &path, const QByteArray &body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path); f.open(QIODevice::WriteOnly); f.write(body);
}
}

void TestVaultWatcher::externalCreateEmitsCreated()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::created);
    writeFile(dir.path() + "/new.md", "x");
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
}

void TestVaultWatcher::externalModifyEmitsModified()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "one");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::modified);
    // Ensure the mtime actually ticks forward across filesystem resolutions.
    QTest::qSleep(1100);
    writeFile(dir.path() + "/a.md", "two");
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
}

void TestVaultWatcher::externalDeleteEmitsDeletedWithTombstone()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *f = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(f);

    QSignalSpy spy(&vault, &Corbomite::Vault::deletedFile);
    QFile::remove(dir.path() + "/a.md");
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);

    auto *argPtr = qvariant_cast<Corbomite::TAbstractFile *>(spy.at(0).at(0));
    QVERIFY(argPtr);
    QCOMPARE(argPtr->deleted, true);
}

void TestVaultWatcher::externalRenameEmitsRenamed()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::renamed);
    QFile::rename(dir.path() + "/a.md", dir.path() + "/b.md");
    // Renamed or the fallback delete+create — both acceptable since rename
    // detection is a best-effort optimisation over identical mtime.
    QSignalSpy spyDel(&vault, &Corbomite::Vault::deletedFile);
    QSignalSpy spyNew(&vault, &Corbomite::Vault::created);
    QTRY_VERIFY_WITH_TIMEOUT(
        spy.count() >= 1 || (spyDel.count() >= 1 && spyNew.count() >= 1),
        5000);
}

void TestVaultWatcher::externalFolderRenameUpdatesDescendants()
{
    QTemporaryDir dir;
    // Two files written back-to-back land in the same millisecond mtime
    // on most filesystems — exactly the ambiguous-pairing case Watcher's
    // basename-preference fix exists for. Without it, the rename detector
    // can cross-wire which old path maps to which new path.
    writeFile(dir.path() + "/Old/note.md", "x");
    writeFile(dir.path() + "/Old/Sub/nested.md", "y");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *originalNote = vault.getFileByPath(QStringLiteral("Old/note.md"));
    auto *originalNested = vault.getFileByPath(QStringLiteral("Old/Sub/nested.md"));
    QVERIFY(originalNote);
    QVERIFY(originalNested);

    QSignalSpy spy(&vault, &Corbomite::Vault::renamed);
    QDir(dir.path()).rename(QStringLiteral("Old"), QStringLiteral("New"));
    // The watcher has no folder-level rename concept (it diffs individual
    // files only), so a folder rename surfaces as one `renamed` per file
    // it contains — 2 here, not a single folder-level event.
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 2, 5000);

    QVERIFY(!vault.getFileByPath(QStringLiteral("Old/note.md")));
    QVERIFY(!vault.getFileByPath(QStringLiteral("Old/Sub/nested.md")));
    // Identity, not just path existence: the same TFile object must have
    // followed its own file, not been cross-wired with the other one.
    QCOMPARE(vault.getFileByPath(QStringLiteral("New/note.md")), originalNote);
    QCOMPARE(vault.getFileByPath(QStringLiteral("New/Sub/nested.md")), originalNested);
}

void TestVaultWatcher::transientEnoentEmitsNoDeleteCreatePair()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "one");
    writeFile(dir.path() + "/anchor.md", "anchor");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *a = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(a);

    QSignalSpy delSpy(&vault, &Corbomite::Vault::deletedFile);
    QSignalSpy creSpy(&vault, &Corbomite::Vault::created);
    QSignalSpy modSpy(&vault, &Corbomite::Vault::modified);

    // Keep a.md and anchor.md off the same millisecond so the watcher's
    // best-effort rename pairing cannot swallow anchor's `modified` and hide
    // the fact that a drain ran.
    QTest::qSleep(1100);

    // The atomic-save shape: the path goes ENOENT, the replacement lands a
    // moment later. QSaveFile (what FileSystemAdapter writes with) and most
    // editors and sync clients do exactly this.
    QVERIFY(QFile::remove(dir.path() + "/a.md"));
    writeFile(dir.path() + "/anchor.md", "anchor-two");

    // anchor's `modified` proves a drain ran while a.md was absent, so the
    // 100 ms grace is armed rather than merely imminent.
    QTRY_VERIFY_WITH_TIMEOUT(modSpy.count() >= 1, 5000);
    QVERIFY(!QFileInfo::exists(dir.path() + "/a.md"));

    // Reappear inside the window the drain just opened.
    writeFile(dir.path() + "/a.md", "two");

    // Well past both the 50 ms drain interval and the 100 ms grace.
    QTest::qSleep(500);

    QCOMPARE(delSpy.count(), 0);
    QCOMPARE(creSpy.count(), 0);
    QCOMPARE(vault.getFileByPath(QStringLiteral("a.md")), a);
    QCOMPARE(a->deleted, false);
}

void TestVaultWatcher::unloadEmitsClosed()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::closed);
    vault.unload();
    QCOMPARE(spy.count(), 1);
}

void TestVaultWatcher::externalChangeEmitsRaw()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "one");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy rawSpy(&vault, &Corbomite::Vault::raw);
    QTest::qSleep(1100);
    writeFile(dir.path() + "/a.md", "two-different");
    QTRY_VERIFY_WITH_TIMEOUT(rawSpy.count() >= 1, 5000);
    QCOMPARE(rawSpy.first().at(0).toString(), QStringLiteral("a.md"));
}

void TestVaultWatcher::externalConfigJsonEmitsConfigChanged()
{
    QTemporaryDir dir;
    QDir().mkpath(dir.path() + "/.obsidian");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy rawSpy(&vault, &Corbomite::Vault::raw);
    QSignalSpy cfgSpy(&vault, &Corbomite::Vault::configChanged);
    writeFile(dir.path() + "/.obsidian/appearance.json",
              R"({"theme":"obsidian"})");
    QTRY_VERIFY_WITH_TIMEOUT(cfgSpy.count() >= 1, 5000);
    QCOMPARE(cfgSpy.first().at(0).toString(),
             QStringLiteral(".obsidian/appearance.json"));
    QVERIFY(rawSpy.count() >= 1);
}

void TestVaultWatcher::externalNonConfigDoesNotEmitConfigChanged()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/note.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy rawSpy(&vault, &Corbomite::Vault::raw);
    QSignalSpy cfgSpy(&vault, &Corbomite::Vault::configChanged);
    QTest::qSleep(1100);
    writeFile(dir.path() + "/note.md", "y-different");
    QTRY_VERIFY_WITH_TIMEOUT(rawSpy.count() >= 1, 5000);
    QCOMPARE(cfgSpy.count(), 0);
}

// Watcher::drainPending builds its create/delete lists by iterating QHashes,
// whose order is randomised per process, and then pairs deletes against creates
// with a first-match search over those lists. Both the emission order and the
// pairing therefore varied from launch to launch for the same sequence of
// filesystem events.
void TestVaultWatcher::batchCreateEmitsInAscendingPathOrder()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::created);

    // One batch, written without letting the event loop run, so the 50ms drain
    // timer sees all of them in a single drain.
    for (const char *rel : {"f.md", "c.md", "e.md", "a.md", "d.md", "b.md"}) {
        writeFile(dir.path() + "/" + rel, "x");
    }

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 6, 5000);

    QStringList seen;
    for (const QList<QVariant> &args : spy)
        seen << args.first().value<Corbomite::TAbstractFile *>()->path;
    QStringList sorted = seen;
    sorted.sort();
    QCOMPARE(seen, sorted);
}

QTEST_MAIN(TestVaultWatcher)
#include "tst_vault_watcher.moc"
