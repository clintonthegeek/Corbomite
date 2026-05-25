// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QFile>
#include <QTemporaryDir>
#include <QSignalSpy>

#include "corbomite/vault/Vault.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include <markoff/core/MarkoffDocument.h>
// TODO(port): MarkdownDelta retired
// include <markoff/MarkdownDelta.h>

class TstVaultSaveReload : public QObject
{
    Q_OBJECT
private slots:
    void saveDocument_writesCanonicalBytesExactly();
    void openDocument_notModifiedAfterHydration();
    void openDocument_newFile_notModified();
    void saveDocument_returnsFalseForUnknownFile();
    void saveDocument_clearsDirtyFlag();
    void saveDocument_emitsSavedAndDocumentSaved();
    // External-reload Origin dispatch (spec §6.2)
    void externalReloadClean_appliesAndClearsStack();
    void externalReloadDirty_emitsConflictSignal();
};

// Helper: write a file to a temporary directory.
static void writeFile(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(f.write(bytes), static_cast<qint64>(bytes.size()));
    f.close();
}

// Helper: read all bytes from a file.
static QByteArray readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

void TstVaultSaveReload::saveDocument_writesCanonicalBytesExactly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Pre-create the file so it's in the vault tree.
    writeFile(dir.path() + "/t.md", "initial");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *note = vault.openDocument(QStringLiteral("t.md"));
    QVERIFY(note);

    // Set new content via markoff() directly with FirstOpen origin.
    // Use u8 literal for the snowman so the compiler encodes it correctly.
    note->markoff()->resetContent(QString::fromUtf8("raw bytes \xe2\x98\x83"),
                                  Markoff::Origin::FirstOpen);
    note->setModified(true);

    QVERIFY(vault.saveDocument(note));

    const QByteArray disk = readFile(dir.path() + "/t.md");
    const QByteArray expected = note->markoff()->toMarkdown().toUtf8();

    // Byte-exact — toMarkdown() must round-trip through UTF-8 unchanged.
    QCOMPARE(disk, expected);
    // Sanity: the snowman is present on disk.
    QVERIFY(disk.contains("\xe2\x98\x83"));
}

void TstVaultSaveReload::openDocument_notModifiedAfterHydration()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QByteArray content = "initial content";
    writeFile(dir.path() + "/t.md", content);

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *note = vault.openDocument(QStringLiteral("t.md"));
    QVERIFY(note);

    // Content should be hydrated correctly.
    QCOMPARE(note->markdown(), QStringLiteral("initial content"));
    // FirstOpen contentsChanged must NOT leave the note dirty.
    QVERIFY(!note->isModified());
}

void TstVaultSaveReload::openDocument_newFile_notModified()
{
    // A file that exists in the vault tree must open without being dirty.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path() + "/new.md", QByteArray{});

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *note = vault.openDocument(QStringLiteral("new.md"));
    QVERIFY(note);
    QVERIFY(!note->isModified());
}

void TstVaultSaveReload::saveDocument_returnsFalseForUnknownFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    // Manually construct a NoteDocument not in the vault tree.
    auto *orphan = new Corbomite::NoteDocument(dir.path(), QStringLiteral("ghost.md"));
    QVERIFY(!vault.saveDocument(orphan));
    delete orphan;
}

void TstVaultSaveReload::saveDocument_clearsDirtyFlag()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path() + "/dirty.md", "hello");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *note = vault.openDocument(QStringLiteral("dirty.md"));
    QVERIFY(note);
    note->setModified(true);
    QVERIFY(note->isModified());

    QVERIFY(vault.saveDocument(note));
    QVERIFY(!note->isModified());
}

void TstVaultSaveReload::saveDocument_emitsSavedAndDocumentSaved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path() + "/sig.md", "content");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *note = vault.openDocument(QStringLiteral("sig.md"));
    QVERIFY(note);

    QSignalSpy noteSaved(note, &Corbomite::NoteDocument::saved);
    QSignalSpy vaultSaved(&vault, &Corbomite::Vault::documentSaved);

    note->setModified(true);
    QVERIFY(vault.saveDocument(note));

    QCOMPARE(noteSaved.count(), 1);
    QCOMPARE(vaultSaved.count(), 1);
    QCOMPARE(vaultSaved.first().first().toString(), QStringLiteral("sig.md"));
}

// ---------------------------------------------------------------------------
// External-reload Origin dispatch tests (spec §6.2, Task 22)
// ---------------------------------------------------------------------------

void TstVaultSaveReload::externalReloadClean_appliesAndClearsStack()
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

    // Push a local edit so the undo stack is non-empty.
    note->markoff()->undoStack()->push(
        new Markoff::MarkdownDelta(note->markoff(), 7, 0, QStringLiteral(" more")));
    QCOMPARE(note->markdown(), QStringLiteral("initial more"));
    QVERIFY(note->markoff()->undoStack()->count() >= 1);

    // Save to "accept" the current content as the saved state; clears dirty.
    vault.saveDocument(note);
    QVERIFY(!note->isModified());

    // Simulate an external editor changing the file content.
    writeFile(dir.path() + "/t.md", "from outside");

    // Spy on the documentReloaded signal emitted by MarkoffDocument.
    QSignalSpy reload(note->markoff(), &Markoff::MarkoffDocument::documentReloaded);

    // Invoke the watcher handler directly (relative path, no basePath prefix).
    vault.onExternalModified(QStringLiteral("t.md"));

    // Content updated, undo stack cleared, not dirty.
    QCOMPARE(note->markdown(), QStringLiteral("from outside"));
    QVERIFY(!note->isModified());
    QCOMPARE(note->markoff()->undoStack()->count(), 0);
    QCOMPARE(reload.count(), 1);
}

void TstVaultSaveReload::externalReloadDirty_emitsConflictSignal()
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
    note->markoff()->undoStack()->push(
        new Markoff::MarkdownDelta(note->markoff(), 7, 0, QStringLiteral(" LOCAL")));
    QCOMPARE(note->markdown(), QStringLiteral("initial LOCAL"));
    QVERIFY(note->isModified());

    // Spy on the conflict signal.
    QSignalSpy conflict(&vault, &Corbomite::Vault::externalReloadConflict);

    // Simulate an external editor changing the file content.
    writeFile(dir.path() + "/t.md", "EXTERNAL");

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
    QCOMPARE(note->markoff()->undoStack()->count(), 0);
}

QTEST_MAIN(TstVaultSaveReload)
#include "tst_vault_save_reload.moc"
