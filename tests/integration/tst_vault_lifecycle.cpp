// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include "corbomite/models/NotesTreeModel.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/proxies/VaultProxy.h"
#include "corbomite/core/NoteDocument.h"

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

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(tmp.path());
        QCOMPARE(vault.getMarkdownFiles().size(), 2);

        Corbomite::VaultProxy proxy(&vault,
            {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
             QStringLiteral("vault.events")},
            QStringLiteral("test"));
        Corbomite::NotesTreeModel tree(&proxy);
        QCOMPARE(tree.rowCount(QModelIndex()), 2); // folder + note1.md

        Corbomite::FileManager fileManager(&vault, nullptr);

        // Create note via FileManager
        auto *tf = fileManager.createMarkdownNote(QStringLiteral("new-note"), QString());
        QVERIFY(tf != nullptr);
        QVERIFY(QFileInfo::exists(tmp.path() + "/new-note.md"));
        QCOMPARE(vault.getMarkdownFiles().size(), 3);

        // Open and modify note
        auto *opened = vault.openDocument(QStringLiteral("note1.md"));
        // serializeForSave() canonicalises to a single trailing newline (Markoff B1).
        QCOMPARE(opened->markdown(), QStringLiteral("# Note 1\n"));
        opened->setMarkdown(QStringLiteral("# Modified Note 1"));
        QVERIFY(opened->isModified());

        // Save
        QVERIFY(vault.saveDocument(opened));
        QVERIFY(!opened->isModified());

        // Verify on disk
        QFile f(tmp.path() + "/note1.md");
        f.open(QIODevice::ReadOnly);
        QCOMPARE(QString::fromUtf8(f.readAll()), QStringLiteral("# Modified Note 1\n"));

        // Rename
        QVERIFY(fileManager.renameFileByPath(QStringLiteral("new-note.md"), QStringLiteral("renamed.md")));
        QVERIFY(!vault.getAbstractFileByPath(QStringLiteral("new-note.md")));
        QVERIFY(vault.getAbstractFileByPath(QStringLiteral("renamed.md")));

        // Delete
        QVERIFY(fileManager.trashFileByPath(QStringLiteral("renamed.md")));
        QVERIFY(!QFileInfo::exists(tmp.path() + "/renamed.md"));
        QCOMPARE(vault.getMarkdownFiles().size(), 2);

        // Close vault
        vault.unload();
        QVERIFY(!vault.isLoaded());
    }
};

QTEST_MAIN(TestVaultLifecycle)
#include "tst_vault_lifecycle.moc"
