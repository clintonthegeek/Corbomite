// SPDX-License-Identifier: GPL-3.0-or-later
//
// Regression coverage for Task 0.4 (P1 data-safety): Vault::modify() must
// reconcile an open NoteDocument so a self-write can't leave the live editor
// buffer stale vs disk.
//
// A self-write (modify/process, including FileManager::processFrontMatter)
// stamps the echo-suppression ledger, so onExternalModified suppresses the
// watcher echo. Without reconciliation in modify() itself, a frontmatter /
// process write to a file that ALSO has a live editor NoteDocument leaves the
// in-memory buffer stale with no reload path. The fix is refresh-if-clean /
// signal-if-dirty, reusing the external-reload mechanism:
//   - clean doc → re-sync from the new bytes, end up isModified()==false.
//   - dirty doc → preserve the user's buffer; emit externalReloadConflict.

#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultModifyReconcile : public QObject
{
    Q_OBJECT
private slots:
    void cleanDoc_processRefreshesBuffer();
    void cleanDoc_processFrontMatterRefreshesBuffer();
    void dirtyDoc_processPreservesBufferAndSignalsConflict();
    void byteEqualWrite_isNoOp();
    void noOpenDoc_modifySucceeds();
};

static void writeFile(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(f.write(bytes), static_cast<qint64>(bytes.size()));
    f.close();
}

// Clean case, direct process() path: a self-write through the real RMW cycle
// re-syncs an open clean document and leaves it not-modified.
void TestVaultModifyReconcile::cleanDoc_processRefreshesBuffer()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path() + "/t.md", "original body");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *note = vault.openDocument(QStringLiteral("t.md"));
    QVERIFY(note);
    // markdown() routes through serializeForSave(), which appends the
    // canonical trailing terminator (B1 finalDocumentTerminator == "\n").
    QCOMPARE(note->markdown(), QStringLiteral("original body\n"));
    QVERIFY(!note->isModified());

    auto *tf = vault.getFileByPath(QStringLiteral("t.md"));
    QVERIFY(tf);

    // A self-write through process() (the FileManager::processFrontMatter
    // ancestor path) — rewrites the body.
    QVERIFY(vault.process(tf, [](const QByteArray &) -> QByteArray {
        return QByteArray("replaced body");
    }));

    // The open document must now reflect the new bytes — not the stale buffer.
    QVERIFY2(note->markdown().contains(QStringLiteral("replaced body")),
             qPrintable(QStringLiteral("buffer not refreshed: ") + note->markdown()));
    QVERIFY(!note->markdown().contains(QStringLiteral("original body")));
    // And it must end up clean (consistent with disk), even after the
    // deferred d2DocumentChanged the reset queues fires.
    QTest::qWait(20);
    QVERIFY2(!note->isModified(),
             "clean self-write reconciliation left the doc dirty");
    QVERIFY(note->markdown().contains(QStringLiteral("replaced body")));
}

// Clean case, the actual real-world trigger: FileManager::processFrontMatter
// editing a note's properties while it is open in the editor.
void TestVaultModifyReconcile::cleanDoc_processFrontMatterRefreshesBuffer()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path() + "/note.md", "body text");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *note = vault.openDocument(QStringLiteral("note.md"));
    QVERIFY(note);
    QVERIFY(!note->isModified());
    QVERIFY(!note->markdown().contains(QStringLiteral("status")));

    Corbomite::FileManager fm(&vault, nullptr);
    auto *tf = vault.getFileByPath(QStringLiteral("note.md"));
    QVERIFY(tf);

    // Add a frontmatter key — the canonical processFrontMatter → process →
    // modify chain that triggers the bug.
    QVERIFY(fm.processFrontMatter(tf, [](QVariantMap &m) {
        m.insert(QStringLiteral("status"), QStringLiteral("draft"));
    }));

    // The live buffer must now carry the frontmatter that was written.
    const QString synced = note->markdown();
    QVERIFY2(synced.contains(QStringLiteral("status")),
             qPrintable(QStringLiteral("buffer not refreshed: ") + synced));
    QVERIFY(synced.contains(QStringLiteral("draft")));
    QVERIFY(synced.contains(QStringLiteral("body text")));

    QTest::qWait(20);
    QVERIFY2(!note->isModified(),
             "frontmatter self-write reconciliation left the doc dirty");

    // The refreshed buffer must serialize to the same canonical form the
    // document round-trips to (frontmatter + body, both present). We don't
    // assert byte-equality against the raw disk bytes because the on-disk
    // form is the process() output while markdown() is the D2 re-serialized
    // canonical form — both carry the same logical content.
    QVERIFY(note->markdown().contains(QStringLiteral("status: draft")));
    QVERIFY(note->markdown().contains(QStringLiteral("body text")));
}

// Dirty case: a self-write must NOT clobber the user's unsaved edits. It
// preserves the buffer and emits externalReloadConflict for the editor layer.
void TestVaultModifyReconcile::dirtyDoc_processPreservesBufferAndSignalsConflict()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path() + "/t.md", "disk original");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *note = vault.openDocument(QStringLiteral("t.md"));
    QVERIFY(note);

    // Simulate an unsaved user edit in the live editor.
    note->setMarkdown(QStringLiteral("USER UNSAVED EDITS"));
    QVERIFY(note->isModified());
    const QString userBuffer = note->markdown();
    QVERIFY(userBuffer.contains(QStringLiteral("USER UNSAVED EDITS")));

    QSignalSpy conflict(&vault, &Corbomite::Vault::externalReloadConflict);

    auto *tf = vault.getFileByPath(QStringLiteral("t.md"));
    QVERIFY(tf);

    // A self-write arrives (e.g. a property edit on the same open note).
    QVERIFY(vault.process(tf, [](const QByteArray &) -> QByteArray {
        return QByteArray("SELF-WRITE BODY");
    }));

    // The user's buffer is NOT overwritten.
    QCOMPARE(note->markdown(), userBuffer);
    QVERIFY(note->isModified());

    // The conflict signal fired with this doc and the new (would-be) content.
    QCOMPARE(conflict.count(), 1);
    auto *sigDoc = qvariant_cast<Corbomite::NoteDocument *>(conflict.first().at(0));
    QCOMPARE(sigDoc, note);
    QCOMPARE(conflict.first().at(1).toString(), QStringLiteral("SELF-WRITE BODY"));

    QTest::qWait(20);
    QVERIFY(note->isModified());
    QCOMPARE(note->markdown(), userBuffer);

    // Disk still got the self-write (modify() itself is unaffected).
    QFile g(dir.path() + "/t.md");
    QVERIFY(g.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(g.readAll()), QStringLiteral("SELF-WRITE BODY"));
}

// A write that lands the same bytes the document already holds must not churn
// the CRDT / undo stack, dirty the doc, or fire a conflict.
void TestVaultModifyReconcile::byteEqualWrite_isNoOp()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path() + "/t.md", "same bytes");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *note = vault.openDocument(QStringLiteral("t.md"));
    QVERIFY(note);
    QVERIFY(!note->isModified());

    QSignalSpy conflict(&vault, &Corbomite::Vault::externalReloadConflict);

    auto *tf = vault.getFileByPath(QStringLiteral("t.md"));
    QVERIFY(tf);

    // Write back exactly what the doc already serializes to.
    const QByteArray same = note->markdown().toUtf8();
    QVERIFY(vault.modify(tf, same));

    QTest::qWait(20);
    QVERIFY(!note->isModified());
    QCOMPARE(conflict.count(), 0);
    QVERIFY(note->markdown().contains(QStringLiteral("same bytes")));
}

// Sanity: modify() with no open document still succeeds (reconcile is a no-op).
void TestVaultModifyReconcile::noOpenDoc_modifySucceeds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path() + "/t.md", "one");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("t.md"));
    QVERIFY(tf);
    QVERIFY(vault.modify(tf, QByteArray("two")));

    QFile g(dir.path() + "/t.md");
    QVERIFY(g.open(QIODevice::ReadOnly));
    QCOMPARE(g.readAll(), QByteArray("two"));
}

QTEST_MAIN(TestVaultModifyReconcile)
#include "tst_vault_modify_reconcile.moc"
