// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/NotesTreeModel.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/Vault.h"

class TestVaultLifecycle : public QObject {
    Q_OBJECT

    void createFile(const QString &path, const QString &content = QStringLiteral("# Test\n"))
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
    }

private Q_SLOTS:
    void testFullLifecycle()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + "/note1.md", QStringLiteral("# Note 1"));
        createFile(tmp.path() + "/folder/note2.md", QStringLiteral("# Note 2"));

        // Open legacy VaultModel (drives the note-operation methods
        // absorbed from the deleted NoteService) + canonical Vault
        // (drives NotesTreeModel) in parallel — matches MainWindow
        // wiring during the Q.0 migration window.
        Corbomite::VaultModel vault;
        vault.open(tmp.path());
        QCOMPARE(vault.allNotes().size(), 2);

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vaultObj(&fs);
        vaultObj.load(tmp.path());

        Corbomite::NotesTreeModel tree(&vaultObj);
        QCOMPARE(tree.rowCount(QModelIndex()), 2); // folder + note1.md

        // Create note via VaultModel
        auto *doc = vault.createNote(QStringLiteral("new-note"), QString());
        QVERIFY(doc != nullptr);
        QVERIFY(QFileInfo::exists(tmp.path() + "/new-note.md"));
        QCOMPARE(vault.allNotes().size(), 3);

        // VaultModel writes directly via FileSystemAdapter, bypassing the
        // canonical Vault; production drives tree updates through Vault's
        // watcher, which is asynchronous. Rebind a fresh tree after a
        // reload for deterministic assertions — mirrors MainWindow's
        // tear-down/rebuild on vault switch.
        vaultObj.unload();
        vaultObj.load(tmp.path());
        Corbomite::NotesTreeModel tree2(&vaultObj);
        QCOMPARE(tree2.rowCount(QModelIndex()), 3); // folder + note1 + new-note

        // Open and modify note
        auto *opened = vault.openDocument(QStringLiteral("note1.md"));
        QCOMPARE(opened->markdown(), QStringLiteral("# Note 1"));
        opened->setMarkdown(QStringLiteral("# Modified Note 1"));
        QVERIFY(opened->isModified());

        // Save
        QVERIFY(vault.saveNote(opened));
        QVERIFY(!opened->isModified());

        // Verify on disk
        QFile f(tmp.path() + "/note1.md");
        f.open(QIODevice::ReadOnly);
        QCOMPARE(QString::fromUtf8(f.readAll()), QStringLiteral("# Modified Note 1"));

        // Rename
        QVERIFY(vault.renameNoteByPath(QStringLiteral("new-note.md"), QStringLiteral("renamed.md")));
        QVERIFY(!vault.noteExists(QStringLiteral("new-note.md")));
        QVERIFY(vault.noteExists(QStringLiteral("renamed.md")));

        // Delete
        QVERIFY(vault.deleteNoteByPath(QStringLiteral("renamed.md")));
        QVERIFY(!QFileInfo::exists(tmp.path() + "/renamed.md"));
        QCOMPARE(vault.allNotes().size(), 2);

        // Close vault
        vault.close();
        QCOMPARE(vault.allNotes().size(), 0);
        QVERIFY(vault.path().isEmpty());
    }
};

QTEST_MAIN(TestVaultLifecycle)
#include "tst_vault_lifecycle.moc"
