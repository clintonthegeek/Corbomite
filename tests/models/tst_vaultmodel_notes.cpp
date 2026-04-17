// SPDX-License-Identifier: GPL-3.0-or-later
//
// Exercises the note-operation methods absorbed onto VaultModel in
// Q.0 Phase 8 T8.4 (createNote/saveNote/renameNoteByPath/
// deleteNoteByPath). Formerly tst_noteservice.cpp; NoteService was
// deleted in T8.4.

#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "corbomite/models/VaultModel.h"
#include "corbomite/core/NoteDocument.h"

class TestVaultModelNotes : public QObject {
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
    void testCreateNote()
    {
        QTemporaryDir tmp;
        Corbomite::VaultModel vault;
        vault.open(tmp.path());

        auto *doc = vault.createNote(QStringLiteral("new-note"), QString());

        QVERIFY(doc != nullptr);
        QCOMPARE(doc->relativePath(), QStringLiteral("new-note.md"));
        QVERIFY(QFileInfo::exists(tmp.path() + "/new-note.md"));
        QVERIFY(vault.noteExists(QStringLiteral("new-note.md")));
    }

    void testCreateNoteInFolder()
    {
        QTemporaryDir tmp;
        Corbomite::VaultModel vault;
        vault.open(tmp.path());

        auto *doc = vault.createNote(QStringLiteral("note"), QStringLiteral("subfolder"));

        QVERIFY(doc != nullptr);
        QCOMPARE(doc->relativePath(), QStringLiteral("subfolder/note.md"));
        QVERIFY(QFileInfo::exists(tmp.path() + "/subfolder/note.md"));
    }

    void testOpenDocument()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/note.md", QStringLiteral("# Hello"));
        Corbomite::VaultModel vault;
        vault.open(tmp.path());

        auto *doc = vault.openDocument(QStringLiteral("note.md"));

        QVERIFY(doc != nullptr);
        QCOMPARE(doc->markdown(), QStringLiteral("# Hello"));
    }

    void testSaveNote()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/note.md", QStringLiteral("old content"));
        Corbomite::VaultModel vault;
        vault.open(tmp.path());

        auto *doc = vault.openDocument(QStringLiteral("note.md"));
        doc->setMarkdown(QStringLiteral("new content"));
        QVERIFY(doc->isModified());

        QSignalSpy savedSpy(&vault, &Corbomite::VaultModel::noteSaved);
        bool saved = vault.saveNote(doc);
        QVERIFY(saved);
        QVERIFY(!doc->isModified());
        QCOMPARE(savedSpy.count(), 1);

        // Verify on disk
        QFile f(tmp.path() + "/note.md");
        f.open(QIODevice::ReadOnly);
        QCOMPARE(QString::fromUtf8(f.readAll()), QStringLiteral("new content"));
    }

    void testRenameNoteByPath()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/old.md", QStringLiteral("content"));
        Corbomite::VaultModel vault;
        vault.open(tmp.path());

        bool renamed = vault.renameNoteByPath(
            QStringLiteral("old.md"), QStringLiteral("new.md"));
        QVERIFY(renamed);
        QVERIFY(!QFileInfo::exists(tmp.path() + "/old.md"));
        QVERIFY(QFileInfo::exists(tmp.path() + "/new.md"));
        QVERIFY(!vault.noteExists(QStringLiteral("old.md")));
        QVERIFY(vault.noteExists(QStringLiteral("new.md")));
    }

    void testDeleteNoteByPath()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/doomed.md");
        Corbomite::VaultModel vault;
        vault.open(tmp.path());

        bool deleted = vault.deleteNoteByPath(QStringLiteral("doomed.md"));
        QVERIFY(deleted);
        QVERIFY(!QFileInfo::exists(tmp.path() + "/doomed.md"));
        QVERIFY(!vault.noteExists(QStringLiteral("doomed.md")));
    }

    void testCreateNoteDuplicateAppendsSuffix()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/note.md");
        Corbomite::VaultModel vault;
        vault.open(tmp.path());

        auto *doc = vault.createNote(QStringLiteral("note"), QString());

        QVERIFY(doc != nullptr);
        // Should create "note 1.md" since "note.md" already exists
        QCOMPARE(doc->relativePath(), QStringLiteral("note 1.md"));
    }
};

QTEST_MAIN(TestVaultModelNotes)
#include "tst_vaultmodel_notes.moc"
