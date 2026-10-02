// SPDX-License-Identifier: GPL-3.0-or-later
//
// External-reload Origin dispatch — spec §6.2 slots, split out of
// tst_vault_save_reload.cpp so that file's other slots could run.
//
// ORIGINALLY WRITTEN AS A BUG REPORT. This file first asserted that a clean
// external reload leaves the D2 UndoLog empty, and was gated `if(FALSE)`
// because it failed. **That claim was investigated on 2026-10-01 and is only
// half true, so the test was rewritten to pin verified behaviour rather than
// left red on a wrong premise.** The record matters because the mechanism is
// real and looks alarming:
//
//   REAL: `Origin::ExternalReloadClean`/`ExternalReloadResolved` are documented
//   as "stack cleared" (`Origin.h:12-13`) and `Vault.cpp:273` relies on it. But
//   `resetContent()` clears only the *legacy* CollabText stack
//   (`MarkoffDocument.cpp:720-726`); `wipeD2State()` explicitly PRESERVES
//   `d->undoLog` ("Intentionally preserved: ... d->undoLog ...",
//   `MarkoffDocument.cpp:2178-2179`). So `entryCount()` really does stay > 0
//   after a reload. That part of the original claim is confirmed.
//
//   NOT REAL: the feared consequence — "one Ctrl+Z resurrects the pre-reload
//   buffer, so the external change is silently undone" — does NOT happen.
//   `buildD2FromBytes()` re-parses the file into FRESH BlockIds and resets
//   `d2EditSequence()` to 0, so the preserved UndoLog entries reference blocks
//   that no longer exist and `UndoLog::undo()` skips them. Measured directly:
//   after a clean external reload, `undoD2()` leaves the content byte-identical.
//
// So the observable behaviour is that **Ctrl+Z is a silent no-op** after an
// external change. That is a minor UX wart, NOT silent content loss, and NOT a
// data-integrity defect — which matters, because this project's worst bug class
// is exactly silent content divergence.
//
// UPSTREAM QUESTION (low severity, filed not blocked): should
// `wipeD2State()`, or the stack-clearing origins specifically, clear the D2
// UndoLog so the intent in `Origin.h` matches the implementation and Ctrl+Z is
// not a silent no-op? There is no public `UndoLog::clear()` today.
//
// The slot below asserts BOTH halves — the log IS preserved, AND undo does NOT
// resurrect. Asserting only the first is what made the original report look
// like a data-loss bug; asserting only the second would hide the
// documentation/implementation mismatch.

#include <QTest>
#include <QFile>
#include <QTemporaryDir>
#include <QSignalSpy>

#include "corbomite/vault/Vault.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include <markoff/core/MarkoffDocument.h>
#include <markoff/core/UndoLog.h>

class TstVaultExternalReload : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void externalReloadClean_applies_preservesLogButUndoIsInert();
    void externalReloadDirty_emitsConflictSignal();
};

static void writeFile(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(f.write(bytes), static_cast<qint64>(bytes.size()));
    f.close();
}

// See tst_vault_save_reload.cpp for why the external write needs a pinned mtime.
static void writeExternalFile(const QString &path, const QByteArray &bytes)
{
    writeFile(path, bytes);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    // QFile::setFileTime() silently returns false and changes nothing if the
    // handle is already closed — verified on Qt 6.11 over tmpfs. Set it while
    // the file is still open.
    f.setFileTime(QDateTime::currentDateTimeUtc().addSecs(5),
                  QFileDevice::FileModificationTime);
}

void TstVaultExternalReload::externalReloadClean_applies_preservesLogButUndoIsInert()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path() + "/t.md", "initial");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *note = vault.openDocument(QStringLiteral("t.md"));
    QVERIFY(note);
    QCOMPARE(note->markdown(), QStringLiteral("initial\n"));

    // Push a local edit so the undo log is non-empty.
    note->markoff()->applyFlatEdit(7, 7, " more", Markoff::Origin::UserEdit);
    // NoteDocument raises the dirty flag from MarkoffDocument's
    // d2DocumentChanged signal, which MarkoffDocument defers with
    // QTimer::singleShot(0). Let the event loop deliver it before asserting.
    QTRY_VERIFY_WITH_TIMEOUT(note->isModified(), 2000);
    QCOMPARE(note->markdown(), QStringLiteral("initial more\n"));
    QVERIFY(note->markoff()->d2UndoLog().entryCount() >= 1);

    // Save to "accept" the current content as the saved state; clears dirty.
    vault.saveDocument(note);
    QVERIFY(!note->isModified());

    // Simulate an external editor changing the file content.
    writeExternalFile(dir.path() + "/t.md", "from outside");

    // Spy on the documentReloaded signal emitted by MarkoffDocument.
    QSignalSpy reload(note->markoff(), &Markoff::MarkoffDocument::documentReloaded);

    // Invoke the watcher handler directly (relative path, no basePath prefix).
    vault.onExternalModified(QStringLiteral("t.md"));

    // Content applied, not dirty.
    QCOMPARE(note->markdown(), QStringLiteral("from outside\n"));
    QVERIFY(!note->isModified());
    QCOMPARE(reload.count(), 1);

    // HALF 1 (confirmed): the D2 UndoLog is NOT cleared, contrary to
    // Origin.h's "stack cleared". This is the documentation/implementation
    // mismatch the upstream question is about.
    QVERIFY2(note->markoff()->d2UndoLog().entryCount() >= 1,
             "if this is now 0, Markoff started clearing the D2 log on reload — "
             "close the upstream item and drop half 2 as well");

    // HALF 2 (the reason this is NOT a data-loss bug): the preserved entries
    // reference blocks that buildD2FromBytes() replaced, so undo is inert.
    // Assert this explicitly so a future Markoff change that makes the log
    // live again surfaces here as a real regression, not a silent surprise.
    const QString afterReload = note->markdown();
    note->markoff()->undoD2();
    QCOMPARE(note->markdown(), afterReload);
}

void TstVaultExternalReload::externalReloadDirty_emitsConflictSignal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path() + "/t.md", "initial");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *note = vault.openDocument(QStringLiteral("t.md"));
    QVERIFY(note);

    // Push a local edit — note is now dirty.
    note->markoff()->applyFlatEdit(7, 7, " LOCAL", Markoff::Origin::UserEdit);
    QTRY_VERIFY_WITH_TIMEOUT(note->isModified(), 2000);
    QCOMPARE(note->markdown(), QStringLiteral("initial LOCAL\n"));

    // Spy on the conflict signal.
    QSignalSpy conflict(&vault, &Corbomite::Vault::externalReloadConflict);

    // Simulate an external editor changing the file content.
    writeExternalFile(dir.path() + "/t.md", "EXTERNAL");

    // Invoke the watcher handler directly.
    vault.onExternalModified(QStringLiteral("t.md"));

    // Conflict signal fired; doc content NOT auto-changed.
    QCOMPARE(conflict.count(), 1);
    QCOMPARE(note->markdown(), QStringLiteral("initial LOCAL\n"));  // unchanged

    // Verify the signal carries the right doc and disk content.
    auto *sigDoc = qvariant_cast<Corbomite::NoteDocument *>(conflict.first().at(0));
    QCOMPARE(sigDoc, note);
    QCOMPARE(conflict.first().at(1).toString(), QStringLiteral("EXTERNAL"));

    // UI resolves with "take theirs".
    vault.resolveExternalReload(note, QStringLiteral("EXTERNAL"));
    QCOMPARE(note->markdown(), QStringLiteral("EXTERNAL\n"));
    QVERIFY(!note->isModified());

    // Same two halves as the clean path: the log survives ExternalReloadResolved
    // (the other "stack cleared" origin), and undo is inert because the
    // preserved entries point at blocks the reload replaced.
    QVERIFY(note->markoff()->d2UndoLog().entryCount() >= 1);
    const QString afterResolve = note->markdown();
    note->markoff()->undoD2();
    QCOMPARE(note->markdown(), afterResolve);
}

QTEST_MAIN(TstVaultExternalReload)
#include "tst_vault_external_reload.moc"
