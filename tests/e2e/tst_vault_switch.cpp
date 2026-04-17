// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reproduction test for project_vault_switching memory.
// Exercises the full MainWindow + CorbomiteApp path when the user opens
// a second vault while a vault is already live. Pre-3b this reportedly
// crashed; we need to know if 3b's session rework happened to resolve
// it (narrower Phase 4 scope) or not (we need full Kate destroy-rebuild).

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QSignalSpy>

#include <KAboutData>
#include <KLocalizedString>

#include "app/MainWindow.h"
#include "app/CorbomiteApp.h"
#include "corbomite/models/VaultModel.h"

using namespace Corbomite;

class TestVaultSwitch : public QObject {
    Q_OBJECT

private:
    CorbomiteApp *m_app = nullptr;
    MainWindow *m_mainWindow = nullptr;
    QTemporaryDir m_vaultA;
    QTemporaryDir m_vaultB;

    void createFile(const QString &path, const QString &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content.toUtf8());
    }

    void settle(int ms = 300) { QTest::qWait(ms); }

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("vault-switch repro"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);

        QVERIFY(m_vaultA.isValid());
        QVERIFY(m_vaultB.isValid());
        createFile(m_vaultA.path() + "/a1.md", QStringLiteral("# A1\n"));
        createFile(m_vaultA.path() + "/a2.md", QStringLiteral("# A2\n"));
        createFile(m_vaultB.path() + "/b1.md", QStringLiteral("# B1\n"));

        m_app = new CorbomiteApp(this);
        m_mainWindow = new MainWindow(m_app);
        m_mainWindow->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow));
        settle(200);
    }

    void cleanupTestCase()
    {
        delete m_mainWindow;
        m_mainWindow = nullptr;
        delete m_app;
        m_app = nullptr;
    }

    // Open A, open a note, then open B while A is live.
    // Pre-3b claim: this crashes.
    void testSwitchVaultWhileLive()
    {
        QVERIFY(m_app->openVault(m_vaultA.path()));
        settle(500);
        QVERIFY(m_app->isOpen());
        QCOMPARE(m_app->vault()->allNotes().size(), 2);

        // Open a note so there is editor state to tear down
        m_mainWindow->onNoteActivated(QStringLiteral("a1.md"));
        settle(200);

        // Now open a second vault while A is live — replicates the user
        // flow: File → Open Vault while a vault is loaded.
        // CorbomiteApp::openVault internally calls closeVault first when
        // something is open (if that's how it works today), otherwise we
        // need to call closeVault explicitly.
        QSignalSpy closedSpy(m_app, &CorbomiteApp::vaultClosed);
        QSignalSpy openedSpy(m_app, &CorbomiteApp::vaultOpened);

        // Mimic MainWindow::openVaultDialog path (minus the QFileDialog).
        // That path calls m_editorManager->queryClose() then openVault();
        // closeVault is only called explicitly by the Close Vault action.
        // The in-process swap goes: openVault(B) with A still open.
        const bool opened = m_app->openVault(m_vaultB.path());
        settle(500);

        QVERIFY2(opened, "openVault(B) while A is live failed");
        QVERIFY(m_app->isOpen());
        QCOMPARE(m_app->vault()->allNotes().size(), 1);

        qDebug() << "Vault swap A→B: survived. closedSpy=" << closedSpy.count()
                 << "openedSpy=" << openedSpy.count();
    }

    // Now swap B→A to exercise the other direction.
    void testSwitchVaultBack()
    {
        QVERIFY(m_app->isOpen());
        const bool opened = m_app->openVault(m_vaultA.path());
        settle(500);
        QVERIFY(opened);
        QCOMPARE(m_app->vault()->allNotes().size(), 2);
        qDebug() << "Vault swap B→A: survived.";
    }

    // Several rapid swaps — stresses the teardown ordering.
    void testRapidSwaps()
    {
        for (int i = 0; i < 3; ++i) {
            QVERIFY(m_app->openVault(m_vaultB.path()));
            settle(150);
            QVERIFY(m_app->openVault(m_vaultA.path()));
            settle(150);
        }
        QVERIFY(m_app->isOpen());
        qDebug() << "3 rapid A↔B swaps: survived.";
    }
};

QTEST_MAIN(TestVaultSwitch)
#include "tst_vault_switch.moc"
