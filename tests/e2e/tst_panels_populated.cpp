// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tier A — UI smoke: launch MainWindow on a real display, point it at a
// vault with known link structure, assert the right-sidebar panels show
// non-zero counts. Catches the user-visible class of bug where storage
// is silently empty (e.g. BUG-20260415-000).
//
// Requirements: a running display server (Wayland/X11). DO NOT set
// QT_QPA_PLATFORM=offscreen.

#include <QTest>
#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <KAboutData>
#include <KLocalizedString>

#include "app/MainWindow.h"
#include "app/VaultService.h"
#include "editor/EditorViewManager.h"
#include "editor/NoteEditorWidget.h"
#include "sidebar/BacklinksPanel.h"
#include "sidebar/OutlinksPanel.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/NoteService.h"
#include "corbomite/core/NoteDocument.h"

using namespace Corbomite;

class TestPanelsPopulated : public QObject {
    Q_OBJECT

private:
    VaultService *m_vaultService = nullptr;
    MainWindow *m_mainWindow = nullptr;
    QTemporaryDir m_vaultDir;

    static void writeFile(const QString &path, const QByteArray &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    }

    void settle(int ms = 400) { QTest::qWait(ms); }

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("panels populated"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);

        QVERIFY(m_vaultDir.isValid());
        writeFile(m_vaultDir.path() + "/Hub.md",
                  QByteArrayLiteral("# Hub\n\nRefers to [[Spoke]].\n"));
        writeFile(m_vaultDir.path() + "/Spoke.md",
                  QByteArrayLiteral("# Spoke\n\nNo outgoing links.\n"));

        m_vaultService = new VaultService(this);
        m_mainWindow = new MainWindow(m_vaultService);
        m_mainWindow->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow));
        settle(300);
    }

    void cleanupTestCase()
    {
        delete m_mainWindow;
        m_mainWindow = nullptr;
        delete m_vaultService;
        m_vaultService = nullptr;
    }

    // Open vault, open Hub.md, assert OutlinksPanel header reads "Outgoing Links (1)".
    void hubNoteShowsOneOutgoingLink()
    {
        QVERIFY(m_vaultService->openVault(m_vaultDir.path()));
        settle(800);  // Allow rebuildVault + cache reconcile + panel refresh.

        // Open Hub.md — use openDocument so content is loaded and parsed.
        auto *vault = m_vaultService->vault();
        QVERIFY(vault);
        auto *hubDoc = vault->openDocument(QStringLiteral("Hub.md"));
        QVERIFY(hubDoc);

        // Walk MainWindow's children to find the OutlinksPanel header label.
        auto *outlinks = m_mainWindow->findChild<OutlinksPanel *>();
        QVERIFY(outlinks);
        outlinks->setCurrentNote(hubDoc);
        settle(200);

        auto *header = outlinks->findChild<QLabel *>();
        QVERIFY(header);
        // The header text contains the count in parentheses.
        QVERIFY2(header->text().contains(QStringLiteral("(1)")),
                 qPrintable(QStringLiteral("Header was: ") + header->text()));
    }

    void spokeNoteShowsOneBacklink()
    {
        // Vault already open from previous test (test order matters here;
        // QTest runs methods in declaration order).
        auto *vault = m_vaultService->vault();
        QVERIFY(vault);
        auto *spokeDoc = vault->openDocument(QStringLiteral("Spoke.md"));
        QVERIFY(spokeDoc);

        auto *backlinks = m_mainWindow->findChild<BacklinksPanel *>();
        QVERIFY(backlinks);
        backlinks->setCurrentNote(spokeDoc);
        settle(200);

        auto *header = backlinks->findChild<QLabel *>();
        QVERIFY(header);
        QVERIFY2(header->text().contains(QStringLiteral("(1)")),
                 qPrintable(QStringLiteral("Header was: ") + header->text()));
    }
};

QTEST_MAIN(TestPanelsPopulated)
#include "tst_panels_populated.moc"
