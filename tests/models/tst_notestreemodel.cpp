// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "corbomite/models/NotesTreeModel.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/proxies/VaultProxy.h"

class TestNotesTreeModel : public QObject {
    Q_OBJECT

    void createFile(const QString &path, const QString &content = QStringLiteral("test"))
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
    }

    static QSet<QString> fullGrants()
    {
        return {QStringLiteral("vault.read"),
                QStringLiteral("vault.write"),
                QStringLiteral("vault.events")};
    }

private Q_SLOTS:
    void testEmptyVault()
    {
        QTemporaryDir tmp;
        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(tmp.path());
        Corbomite::VaultProxy proxy(&vault, fullGrants(), QStringLiteral("test"));

        Corbomite::NotesTreeModel model(&proxy);

        QCOMPARE(model.rowCount(QModelIndex()), 0);
    }

    void testFlatNotes()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/alpha.md");
        createFile(tmp.path() + "/beta.md");

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(tmp.path());
        Corbomite::VaultProxy proxy(&vault, fullGrants(), QStringLiteral("test"));
        Corbomite::NotesTreeModel model(&proxy);

        QCOMPARE(model.rowCount(QModelIndex()), 2);
    }

    void testFolderHierarchy()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/top.md");
        createFile(tmp.path() + "/folder/nested.md");

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(tmp.path());
        Corbomite::VaultProxy proxy(&vault, fullGrants(), QStringLiteral("test"));
        Corbomite::NotesTreeModel model(&proxy);

        // Root should have: "folder" directory + "top.md" file = 2 items
        QCOMPARE(model.rowCount(QModelIndex()), 2);

        // Find the folder and check it has children
        bool foundFolder = false;
        for (int i = 0; i < model.rowCount(QModelIndex()); ++i) {
            auto idx = model.index(i, 0, QModelIndex());
            if (model.data(idx, Corbomite::NotesTreeModel::IsDirectoryRole).toBool()) {
                foundFolder = true;
                QCOMPARE(model.rowCount(idx), 1); // nested.md
            }
        }
        QVERIFY(foundFolder);
    }

    void testPathRole()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/note.md");

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(tmp.path());
        Corbomite::VaultProxy proxy(&vault, fullGrants(), QStringLiteral("test"));
        Corbomite::NotesTreeModel model(&proxy);

        auto idx = model.index(0, 0, QModelIndex());
        QCOMPARE(model.data(idx, Corbomite::NotesTreeModel::PathRole).toString(),
                 QStringLiteral("note.md"));
    }

    void testNameRole()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/My Note.md");

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(tmp.path());
        Corbomite::VaultProxy proxy(&vault, fullGrants(), QStringLiteral("test"));
        Corbomite::NotesTreeModel model(&proxy);

        auto idx = model.index(0, 0, QModelIndex());
        QString name = model.data(idx, Qt::DisplayRole).toString();
        QCOMPARE(name, QStringLiteral("My Note.md"));
    }

    void testDirectoriesBeforeFiles()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/zebra.md");
        createFile(tmp.path() + "/aFolder/note.md");

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(tmp.path());
        Corbomite::VaultProxy proxy(&vault, fullGrants(), QStringLiteral("test"));
        Corbomite::NotesTreeModel model(&proxy);

        // First item should be directory, second should be file
        auto first = model.index(0, 0, QModelIndex());
        auto second = model.index(1, 0, QModelIndex());
        QVERIFY(model.data(first, Corbomite::NotesTreeModel::IsDirectoryRole).toBool());
        QVERIFY(!model.data(second, Corbomite::NotesTreeModel::IsDirectoryRole).toBool());
    }

    void testReactsToNoteAdded()
    {
        QTemporaryDir tmp;
        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(tmp.path());
        Corbomite::VaultProxy proxy(&vault, fullGrants(), QStringLiteral("test"));
        Corbomite::NotesTreeModel model(&proxy);

        QCOMPARE(model.rowCount(QModelIndex()), 0);

        QVERIFY(vault.create(QStringLiteral("added.md"), QByteArray()));

        QCOMPARE(model.rowCount(QModelIndex()), 1);
    }

    void testReactsToNoteRemoved()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/note.md");

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(tmp.path());
        Corbomite::VaultProxy proxy(&vault, fullGrants(), QStringLiteral("test"));
        Corbomite::NotesTreeModel model(&proxy);
        QCOMPARE(model.rowCount(QModelIndex()), 1);

        auto *tf = vault.getFileByPath(QStringLiteral("note.md"));
        QVERIFY(tf);
        QVERIFY(vault.remove(tf));

        QCOMPARE(model.rowCount(QModelIndex()), 0);
    }
};

QTEST_MAIN(TestNotesTreeModel)
#include "tst_notestreemodel.moc"
