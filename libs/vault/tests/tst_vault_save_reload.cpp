// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QFile>
#include <QTemporaryDir>
#include <QSignalSpy>

#include "corbomite/vault/Vault.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include <markoff/MarkoffDocument.h>

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

QTEST_MAIN(TstVaultSaveReload)
#include "tst_vault_save_reload.moc"
