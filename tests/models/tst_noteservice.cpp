// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "corbomite/models/NoteService.h"
#include "corbomite/models/VaultModel.h"

class TestNoteService : public QObject {
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
        Corbomite::NoteService service(&vault);

        auto *doc = service.createNote(QStringLiteral("new-note"), QString());

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
        Corbomite::NoteService service(&vault);

        auto *doc = service.createNote(QStringLiteral("note"), QStringLiteral("subfolder"));

        QVERIFY(doc != nullptr);
        QCOMPARE(doc->relativePath(), QStringLiteral("subfolder/note.md"));
        QVERIFY(QFileInfo::exists(tmp.path() + "/subfolder/note.md"));
    }

    void testOpenNote()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/note.md", QStringLiteral("# Hello"));
        Corbomite::VaultModel vault;
        vault.open(tmp.path());
        Corbomite::NoteService service(&vault);

        auto *doc = service.openNote(QStringLiteral("note.md"));

        QVERIFY(doc != nullptr);
        QCOMPARE(doc->markdown(), QStringLiteral("# Hello"));
    }

    void testSaveNote()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/note.md", QStringLiteral("old content"));
        Corbomite::VaultModel vault;
        vault.open(tmp.path());
        Corbomite::NoteService service(&vault);

        auto *doc = service.openNote(QStringLiteral("note.md"));
        doc->setMarkdown(QStringLiteral("new content"));
        QVERIFY(doc->isModified());

        bool saved = service.saveNote(doc);
        QVERIFY(saved);
        QVERIFY(!doc->isModified());

        // Verify on disk
        QFile f(tmp.path() + "/note.md");
        f.open(QIODevice::ReadOnly);
        QCOMPARE(QString::fromUtf8(f.readAll()), QStringLiteral("new content"));
    }

    void testRenameNote()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/old.md", QStringLiteral("content"));
        Corbomite::VaultModel vault;
        vault.open(tmp.path());
        Corbomite::NoteService service(&vault);

        bool renamed = service.renameNote(QStringLiteral("old.md"), QStringLiteral("new.md"));
        QVERIFY(renamed);
        QVERIFY(!QFileInfo::exists(tmp.path() + "/old.md"));
        QVERIFY(QFileInfo::exists(tmp.path() + "/new.md"));
        QVERIFY(!vault.noteExists(QStringLiteral("old.md")));
        QVERIFY(vault.noteExists(QStringLiteral("new.md")));
    }

    void testDeleteNote()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/doomed.md");
        Corbomite::VaultModel vault;
        vault.open(tmp.path());
        Corbomite::NoteService service(&vault);

        bool deleted = service.deleteNote(QStringLiteral("doomed.md"));
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
        Corbomite::NoteService service(&vault);

        auto *doc = service.createNote(QStringLiteral("note"), QString());

        QVERIFY(doc != nullptr);
        // Should create "note 1.md" since "note.md" already exists
        QCOMPARE(doc->relativePath(), QStringLiteral("note 1.md"));
    }
};

QTEST_MAIN(TestNoteService)
#include "tst_noteservice.moc"
