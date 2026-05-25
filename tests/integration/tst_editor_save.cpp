// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/core/NoteDocument.h"

class TestEditorSave : public QObject {
    Q_OBJECT

    void createFile(const QString &path, const QString &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
    }

private Q_SLOTS:
    void testOpenModifySave()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/test.md", QStringLiteral("original content"));

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(tmp.path());

        auto *doc = vault.openDocument(QStringLiteral("test.md"));
        // serializeForSave() canonicalises to a single trailing newline
        // (CommonMark; Markoff B1 buffer convention), so round-trips of
        // content lacking one gain a terminal "\n". Accepted 2026-05-25.
        QCOMPARE(doc->markdown(), QStringLiteral("original content\n"));
        QVERIFY(!doc->isModified());

        doc->setMarkdown(QStringLiteral("modified content"));
        QVERIFY(doc->isModified());

        QVERIFY(vault.saveDocument(doc));
        QVERIFY(!doc->isModified());

        // Read directly from disk
        QFile f(tmp.path() + "/test.md");
        f.open(QIODevice::ReadOnly);
        QCOMPARE(QString::fromUtf8(f.readAll()), QStringLiteral("modified content\n"));
    }

    void testSavePreservesUtf8()
    {
        QTemporaryDir tmp;
        QString content = QString::fromUtf8(u8"日本語 café 🎉 résumé");
        createFile(tmp.path() + "/utf8.md", content);

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(tmp.path());

        auto *doc = vault.openDocument(QStringLiteral("utf8.md"));
        // Canonical trailing newline on serialize (see testOpenModifySave).
        QCOMPARE(doc->markdown(), content + QStringLiteral("\n"));

        // Modify and save
        QString newContent = content + QStringLiteral("\n\nMore text");
        doc->setMarkdown(newContent);
        QVERIFY(vault.saveDocument(doc));

        // Verify
        QFile f(tmp.path() + "/utf8.md");
        f.open(QIODevice::ReadOnly);
        QCOMPARE(QString::fromUtf8(f.readAll()), newContent + QStringLiteral("\n"));
    }

    void testCreateAndSave()
    {
        QTemporaryDir tmp;
        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(tmp.path());
        Corbomite::FileManager fileManager(&vault, nullptr);

        auto *tf = fileManager.createMarkdownNote(QStringLiteral("brand-new"), QString());
        QVERIFY(tf);
        auto *doc = vault.openDocument(tf->path);
        QVERIFY(doc);

        doc->setMarkdown(QStringLiteral("# Brand New Note\n\nContent here."));
        QVERIFY(vault.saveDocument(doc));

        QFile f(tmp.path() + "/brand-new.md");
        f.open(QIODevice::ReadOnly);
        // Canonical trailing newline on save (see testOpenModifySave).
        QCOMPARE(QString::fromUtf8(f.readAll()), QStringLiteral("# Brand New Note\n\nContent here.\n"));
    }
};

QTEST_MAIN(TestEditorSave)
#include "tst_editor_save.moc"
