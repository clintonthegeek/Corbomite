// SPDX-License-Identifier: GPL-3.0-or-later
//
// External-reload dispatch. A clean open document adopts disk bytes through
// NoteDocument::publishMarkdown (Markoff replaceMarkdown). That publication
// keeps accepted history, so undo restores the pre-reload projection. A dirty
// document is not overwritten; the conflict signal carries the disk text.

#include <QTest>
#include <QFile>
#include <QTemporaryDir>
#include <QSignalSpy>

#include "corbomite/vault/Vault.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include <markoff/core/MarkoffDocument.h>

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
    QCOMPARE(note->markdown(), QStringLiteral("initial"));

    // Push a local edit so the undo log is non-empty.
    QVERIFY(note->insertAtFlatOffset(7, QByteArray(" more")));
    QTRY_VERIFY_WITH_TIMEOUT(note->isModified(), 2000);
    QCOMPARE(note->markdown(), QStringLiteral("initial more"));

    // Save to "accept" the current content as the saved state; clears dirty.
    vault.saveDocument(note);
    QVERIFY(!note->isModified());

    // Simulate an external editor changing the file content.
    writeExternalFile(dir.path() + "/t.md", "from outside");

    QSignalSpy reload(note->markoff(), &Markoff::MarkoffDocument::documentChanged);

    // Invoke the watcher handler directly (relative path, no basePath prefix).
    vault.onExternalModified(QStringLiteral("t.md"));

    // Content applied, not dirty.
    QCOMPARE(note->markdown(), QStringLiteral("from outside"));
    QVERIFY(!note->isModified());
    QVERIFY(reload.count() >= 1);

    // replaceMarkdown keeps accepted history. Undoing the reload restores
    // the pre-reload projection instead of silently no-op'ing.
    note->markoff()->undoD2();
    QCOMPARE(note->markdown(), QStringLiteral("initial more"));
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
    QVERIFY(note->insertAtFlatOffset(7, QByteArray(" LOCAL")));
    QTRY_VERIFY_WITH_TIMEOUT(note->isModified(), 2000);
    QCOMPARE(note->markdown(), QStringLiteral("initial LOCAL"));

    // Spy on the conflict signal.
    QSignalSpy conflict(&vault, &Corbomite::Vault::externalReloadConflict);

    // Simulate an external editor changing the file content.
    writeExternalFile(dir.path() + "/t.md", "EXTERNAL");

    // Invoke the watcher handler directly.
    vault.onExternalModified(QStringLiteral("t.md"));

    // Conflict signal fired; doc content NOT auto-changed.
    QCOMPARE(conflict.count(), 1);
    QCOMPARE(note->markdown(), QStringLiteral("initial LOCAL"));  // unchanged

    // Verify the signal carries the right doc and disk content.
    auto *sigDoc = qvariant_cast<Corbomite::NoteDocument *>(conflict.first().at(0));
    QCOMPARE(sigDoc, note);
    QCOMPARE(conflict.first().at(1).toString(), QStringLiteral("EXTERNAL"));

    // UI resolves with "take theirs".
    vault.resolveExternalReload(note, QStringLiteral("EXTERNAL"));
    QCOMPARE(note->markdown(), QStringLiteral("EXTERNAL"));
    QVERIFY(!note->isModified());

    // The resolved replacement is itself an accepted action.
    note->markoff()->undoD2();
    QCOMPARE(note->markdown(), QStringLiteral("initial LOCAL"));
}

QTEST_MAIN(TstVaultExternalReload)
#include "tst_vault_external_reload.moc"
