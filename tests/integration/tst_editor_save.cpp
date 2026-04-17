// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "corbomite/models/VaultModel.h"
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

        Corbomite::VaultModel vault;
        vault.open(tmp.path());

        auto *doc = vault.openDocument(QStringLiteral("test.md"));
        QCOMPARE(doc->markdown(), QStringLiteral("original content"));
        QVERIFY(!doc->isModified());

        doc->setMarkdown(QStringLiteral("modified content"));
        QVERIFY(doc->isModified());

        QVERIFY(vault.saveNote(doc));
        QVERIFY(!doc->isModified());

        // Read directly from disk
        QFile f(tmp.path() + "/test.md");
        f.open(QIODevice::ReadOnly);
        QCOMPARE(QString::fromUtf8(f.readAll()), QStringLiteral("modified content"));
    }

    void testSavePreservesUtf8()
    {
        QTemporaryDir tmp;
        QString content = QString::fromUtf8(u8"日本語 café 🎉 résumé");
        createFile(tmp.path() + "/utf8.md", content);

        Corbomite::VaultModel vault;
        vault.open(tmp.path());

        auto *doc = vault.openDocument(QStringLiteral("utf8.md"));
        QCOMPARE(doc->markdown(), content);

        // Modify and save
        QString newContent = content + QStringLiteral("\n\nMore text");
        doc->setMarkdown(newContent);
        QVERIFY(vault.saveNote(doc));

        // Verify
        QFile f(tmp.path() + "/utf8.md");
        f.open(QIODevice::ReadOnly);
        QCOMPARE(QString::fromUtf8(f.readAll()), newContent);
    }

    void testCreateAndSave()
    {
        QTemporaryDir tmp;
        Corbomite::VaultModel vault;
        vault.open(tmp.path());

        auto *doc = vault.createNote(QStringLiteral("brand-new"), QString());
        QVERIFY(doc != nullptr);

        doc->setMarkdown(QStringLiteral("# Brand New Note\n\nContent here."));
        QVERIFY(vault.saveNote(doc));

        QFile f(tmp.path() + "/brand-new.md");
        f.open(QIODevice::ReadOnly);
        QCOMPARE(QString::fromUtf8(f.readAll()), QStringLiteral("# Brand New Note\n\nContent here."));
    }
};

QTEST_MAIN(TestEditorSave)
#include "tst_editor_save.moc"
