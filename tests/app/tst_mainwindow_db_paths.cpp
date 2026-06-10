// SPDX-License-Identifier: GPL-3.0-or-later
//
// Task 0.7 — Move index.sqlite + metadata-cache.db out of .obsidian/
//
// Verifies:
//   1. After vault open, both DB files reside under AppLocalDataLocation
//      (not inside the vault).
//   2. The per-vault subdirectory uses the expected basename+hash keying
//      (PathUtils::vaultLocalDataDir).
//   3. Legacy in-vault DB files (index.sqlite / metadata-cache.db in
//      .obsidian/) are removed on vault open.
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
#include "corbomite/core/PathUtils.h"

using namespace Corbomite;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void createFile(const QString &path, const QByteArray &content = "# Test\n")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(content);
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TstMainWindowDbPaths : public QObject {
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("db-paths test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    // -----------------------------------------------------------------------
    // Test 1: DB files must be under AppLocalDataLocation, not in the vault.
    // -----------------------------------------------------------------------
    void dbFilesAreOutsideVault()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/note.md"));

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(300);
        QVERIFY(app.isOpen());

        const QString localDataRoot =
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QVERIFY(!localDataRoot.isEmpty());

        const QString expectedDir = PathUtils::vaultLocalDataDir(tmp.path());
        QVERIFY(!expectedDir.isEmpty());
        QVERIFY2(expectedDir.startsWith(localDataRoot),
                 qPrintable(QStringLiteral("DB dir '%1' not under '%2'")
                                .arg(expectedDir, localDataRoot)));

        // The DB files must exist at the new location.
        const QString indexPath = expectedDir + QStringLiteral("/index.sqlite");
        const QString cachePath = expectedDir + QStringLiteral("/metadata-cache.db");
        QVERIFY2(QFile::exists(indexPath),
                 qPrintable(QStringLiteral("index.sqlite not found at: ") + indexPath));
        QVERIFY2(QFile::exists(cachePath),
                 qPrintable(QStringLiteral("metadata-cache.db not found at: ") + cachePath));

        // The DB files must NOT exist inside the vault.
        const QString vaultIndex = tmp.path() + QStringLiteral("/.obsidian/index.sqlite");
        const QString vaultCache = tmp.path() + QStringLiteral("/.obsidian/metadata-cache.db");
        QVERIFY2(!QFile::exists(vaultIndex),
                 "index.sqlite must not exist inside vault/.obsidian/");
        QVERIFY2(!QFile::exists(vaultCache),
                 "metadata-cache.db must not exist inside vault/.obsidian/");
    }

    // -----------------------------------------------------------------------
    // Test 2: Legacy in-vault DB files are cleaned up on vault open.
    // -----------------------------------------------------------------------
    void legacyInVaultDbsAreRemovedOnOpen()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/note.md"));

        // Plant legacy DB files in .obsidian/ before opening the vault.
        const QString obsidianDir = tmp.path() + QStringLiteral("/.obsidian");
        QDir().mkpath(obsidianDir);
        createFile(obsidianDir + QStringLiteral("/index.sqlite"),       "SQLite format 3\000");
        createFile(obsidianDir + QStringLiteral("/metadata-cache.db"),  "SQLite format 3\000");
        QVERIFY(QFile::exists(obsidianDir + QStringLiteral("/index.sqlite")));
        QVERIFY(QFile::exists(obsidianDir + QStringLiteral("/metadata-cache.db")));

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(300);
        QVERIFY(app.isOpen());

        // Legacy files must be gone.
        QVERIFY2(!QFile::exists(obsidianDir + QStringLiteral("/index.sqlite")),
                 "Legacy index.sqlite was not removed from .obsidian/");
        QVERIFY2(!QFile::exists(obsidianDir + QStringLiteral("/metadata-cache.db")),
                 "Legacy metadata-cache.db was not removed from .obsidian/");
    }
};

QTEST_MAIN(TstMainWindowDbPaths)
#include "tst_mainwindow_db_paths.moc"
