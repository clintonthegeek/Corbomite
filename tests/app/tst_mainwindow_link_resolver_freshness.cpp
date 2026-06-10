// SPDX-License-Identifier: GPL-3.0-or-later
//
// Task 0.3 — LinkResolver freshness (P1).
//
// Verifies that the app-level LinkResolver stays in sync with the live vault
// during an in-session create / rename / delete, and that non-.md attachments
// are resolvable targets — all driven through the real Vault signal path
// (not by calling addVaultPath/removeVaultPath directly).
//
// Exit conditions:
//   - A note created this session resolves immediately (no reopen).
//   - A renamed note resolves under its new name, not its old.
//   - A deleted note stops resolving.
//   - A non-.md attachment (foo.png) is a resolvable target.
//
// Runs under QT_QPA_PLATFORM=offscreen (no display server required).

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <KAboutData>
#include <KLocalizedString>

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void createFile(const QString &path, const QString &content = QStringLiteral("# Test\n"))
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(content.toUtf8());
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TstMainWindowLinkResolverFreshness : public QObject {
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("link-resolver-freshness test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    // -----------------------------------------------------------------------
    // Test 1: note created in-session resolves without vault reopen.
    // -----------------------------------------------------------------------
    void createNote_resolvesImmediately()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // Seed vault with one existing note so the vault is not empty.
        createFile(tmp.path() + QStringLiteral("/existing.md"));

        CorbomiteApp app;
        MainWindow mw(&app);

        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(200);
        QVERIFY(app.isOpen());

        LinkResolver *lr = mw.linkResolver();
        QVERIFY2(lr, "linkResolver() must be non-null after vault open");

        // "NewNote.md" does not exist yet — resolver should not find it.
        auto before = lr->resolve(QStringLiteral("existing.md"), QStringLiteral("NewNote"));
        QVERIFY2(!before.resolved, "NewNote should not resolve before creation");

        // Create via the Vault API — this fires Vault::created, which the
        // MainWindow handler must forward to the LinkResolver.
        Vault *vault = mw.vaultObj();
        QVERIFY(vault);
        TFile *created = vault->create(QStringLiteral("NewNote.md"), QByteArray("# New\n"));
        QVERIFY2(created, "Vault::create must succeed");

        // Process any queued signals.
        QTest::qWait(50);

        auto after = lr->resolve(QStringLiteral("existing.md"), QStringLiteral("NewNote"));
        QVERIFY2(after.resolved, "NewNote must resolve immediately after in-session create");
        QCOMPARE(after.path, QStringLiteral("NewNote.md"));
    }

    // -----------------------------------------------------------------------
    // Test 2: rename → resolves new name, old name is gone.
    // -----------------------------------------------------------------------
    void renameNote_resolvesNewNameNotOld()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Original.md"));

        CorbomiteApp app;
        MainWindow mw(&app);

        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(200);
        QVERIFY(app.isOpen());

        LinkResolver *lr = mw.linkResolver();
        QVERIFY(lr);

        auto before = lr->resolve(QStringLiteral("x.md"), QStringLiteral("Original"));
        QVERIFY2(before.resolved, "Original must resolve before rename");

        Vault *vault = mw.vaultObj();
        TFile *f = vault->getFileByPath(QStringLiteral("Original.md"));
        QVERIFY2(f, "Original.md must be in the vault tree");

        bool ok = vault->rename(f, QStringLiteral("Renamed.md"));
        QVERIFY2(ok, "Vault::rename must succeed");

        QTest::qWait(50);

        auto afterOld = lr->resolve(QStringLiteral("x.md"), QStringLiteral("Original"));
        QVERIFY2(!afterOld.resolved, "Original must no longer resolve after rename");

        auto afterNew = lr->resolve(QStringLiteral("x.md"), QStringLiteral("Renamed"));
        QVERIFY2(afterNew.resolved, "Renamed must resolve immediately after rename");
        QCOMPARE(afterNew.path, QStringLiteral("Renamed.md"));
    }

    // -----------------------------------------------------------------------
    // Test 3: delete → note stops resolving.
    // -----------------------------------------------------------------------
    void deleteNote_stopsResolving()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/ToDelete.md"));

        CorbomiteApp app;
        MainWindow mw(&app);

        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(200);
        QVERIFY(app.isOpen());

        LinkResolver *lr = mw.linkResolver();
        QVERIFY(lr);

        auto before = lr->resolve(QStringLiteral("x.md"), QStringLiteral("ToDelete"));
        QVERIFY2(before.resolved, "ToDelete must resolve before delete");

        Vault *vault = mw.vaultObj();
        TFile *f = vault->getFileByPath(QStringLiteral("ToDelete.md"));
        QVERIFY2(f, "ToDelete.md must be in the vault tree");

        bool ok = vault->remove(f);
        QVERIFY2(ok, "Vault::remove must succeed");

        QTest::qWait(50);

        auto after = lr->resolve(QStringLiteral("x.md"), QStringLiteral("ToDelete"));
        QVERIFY2(!after.resolved, "ToDelete must no longer resolve after delete");
    }

    // -----------------------------------------------------------------------
    // Test 4: non-.md attachment is a resolvable target.
    // -----------------------------------------------------------------------
    void attachmentFile_isResolvable()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // Write a PNG attachment directly to disk first, so it's picked up
        // at vault-open (tests the "all files, not just .md" initial load).
        createFile(tmp.path() + QStringLiteral("/image.png"), QStringLiteral("PNG"));
        createFile(tmp.path() + QStringLiteral("/note.md"));

        CorbomiteApp app;
        MainWindow mw(&app);

        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(200);
        QVERIFY(app.isOpen());

        LinkResolver *lr = mw.linkResolver();
        QVERIFY(lr);

        // Resolve with explicit extension — Obsidian step 3 handles this.
        auto result = lr->resolve(QStringLiteral("note.md"), QStringLiteral("image.png"));
        QVERIFY2(result.resolved, "image.png attachment must resolve after vault open");
        QCOMPARE(result.path, QStringLiteral("image.png"));
    }

    // -----------------------------------------------------------------------
    // Test 5: attachment created in-session resolves immediately.
    // -----------------------------------------------------------------------
    void createAttachment_resolvesImmediately()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/note.md"));

        CorbomiteApp app;
        MainWindow mw(&app);

        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(200);
        QVERIFY(app.isOpen());

        LinkResolver *lr = mw.linkResolver();
        QVERIFY(lr);

        auto before = lr->resolve(QStringLiteral("note.md"), QStringLiteral("photo.png"));
        QVERIFY2(!before.resolved, "photo.png must not resolve before creation");

        Vault *vault = mw.vaultObj();
        TFile *created = vault->createBinary(QStringLiteral("photo.png"), QByteArray());
        QVERIFY2(created, "Vault::createBinary must succeed");

        QTest::qWait(50);

        auto after = lr->resolve(QStringLiteral("note.md"), QStringLiteral("photo.png"));
        QVERIFY2(after.resolved, "photo.png must resolve immediately after in-session createBinary");
        QCOMPARE(after.path, QStringLiteral("photo.png"));
    }
};

QTEST_MAIN(TstMainWindowLinkResolverFreshness)
#include "tst_mainwindow_link_resolver_freshness.moc"
