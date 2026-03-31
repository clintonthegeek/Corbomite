// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "corbomite/models/VaultModel.h"

class TestVaultModel : public QObject {
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
    void testOpenVault()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + "/note1.md");
        createFile(tmp.path() + "/folder/note2.md");

        Corbomite::VaultModel model;
        QSignalSpy spy(&model, &Corbomite::VaultModel::vaultScanned);
        model.open(tmp.path());

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.path(), tmp.path());
        QCOMPARE(model.allNotes().size(), 2);
    }

    void testVaultName()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::VaultModel model;
        model.open(tmp.path());

        // Name is last component of path
        QVERIFY(!model.name().isEmpty());
    }

    void testNoteExists()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + "/note.md");

        Corbomite::VaultModel model;
        model.open(tmp.path());

        QVERIFY(model.noteExists(QStringLiteral("note.md")));
        QVERIFY(!model.noteExists(QStringLiteral("missing.md")));
    }

    void testOpenDocument()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + "/note.md", QStringLiteral("# Hello\n\nWorld"));

        Corbomite::VaultModel model;
        model.open(tmp.path());

        auto *doc = model.openDocument(QStringLiteral("note.md"));
        QVERIFY(doc != nullptr);
        QCOMPARE(doc->markdown(), QStringLiteral("# Hello\n\nWorld"));
        QCOMPARE(doc->relativePath(), QStringLiteral("note.md"));
    }

    void testOpenDocumentCachesInstance()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + "/note.md");

        Corbomite::VaultModel model;
        model.open(tmp.path());

        auto *doc1 = model.openDocument(QStringLiteral("note.md"));
        auto *doc2 = model.openDocument(QStringLiteral("note.md"));
        QCOMPARE(doc1, doc2); // Same pointer
    }

    void testAddNote()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Corbomite::VaultModel model;
        model.open(tmp.path());
        QCOMPARE(model.allNotes().size(), 0);

        QSignalSpy spy(&model, &Corbomite::VaultModel::noteAdded);
        createFile(tmp.path() + "/new.md");
        model.addNote(QStringLiteral("new.md"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("new.md"));
        QCOMPARE(model.allNotes().size(), 1);
    }

    void testRemoveNote()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + "/note.md");

        Corbomite::VaultModel model;
        model.open(tmp.path());
        QCOMPARE(model.allNotes().size(), 1);

        QSignalSpy spy(&model, &Corbomite::VaultModel::noteRemoved);
        model.removeNote(QStringLiteral("note.md"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.allNotes().size(), 0);
    }

    void testRenameNote()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + "/old.md");

        Corbomite::VaultModel model;
        model.open(tmp.path());

        QSignalSpy spy(&model, &Corbomite::VaultModel::noteRenamed);
        model.renameNote(QStringLiteral("old.md"), QStringLiteral("new.md"));

        QCOMPARE(spy.count(), 1);
        QVERIFY(!model.noteExists(QStringLiteral("old.md")));
        QVERIFY(model.noteExists(QStringLiteral("new.md")));
    }

    void testCloseVault()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + "/note.md");

        Corbomite::VaultModel model;
        model.open(tmp.path());
        QCOMPARE(model.allNotes().size(), 1);

        model.close();
        QCOMPARE(model.allNotes().size(), 0);
        QVERIFY(model.path().isEmpty());
    }

    void testConfigPath()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Corbomite::VaultModel model;
        model.open(tmp.path());

        QCOMPARE(model.configPath(), tmp.path() + QStringLiteral("/.corbomite"));
    }

    void testAllTags()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + "/note1.md", QStringLiteral("# Title\n\nHello #project world #status/active\n"));
        createFile(tmp.path() + "/note2.md", QStringLiteral("Text with #project and #idea\n"));

        Corbomite::VaultModel model;
        model.open(tmp.path());

        auto tags = model.allTags();

        QVERIFY(tags.contains(QStringLiteral("project")));
        QVERIFY(tags.contains(QStringLiteral("status/active")));
        QVERIFY(tags.contains(QStringLiteral("idea")));
        // "project" appears in both files but only listed once
        QCOMPARE(tags.count(QStringLiteral("project")), 1);
        // Sorted alphabetically
        QVERIFY(std::is_sorted(tags.begin(), tags.end()));
    }

    void testAllTagsExcludesCodeBlocks()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + "/note.md", QStringLiteral("Real #tag here\n\n```\n#not-a-tag\n```\n"));

        Corbomite::VaultModel model;
        model.open(tmp.path());

        auto tags = model.allTags();

        QVERIFY(tags.contains(QStringLiteral("tag")));
        QVERIFY(!tags.contains(QStringLiteral("not-a-tag")));
    }

    void testAllTagsEmptyVault()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Corbomite::VaultModel model;
        model.open(tmp.path());

        QVERIFY(model.allTags().isEmpty());
    }
};

QTEST_MAIN(TestVaultModel)
#include "tst_vaultmodel.moc"
