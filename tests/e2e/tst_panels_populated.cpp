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
#include "app/CorbomiteApp.h"
#include "editor/NoteEditorWidget.h"
#include "editor/MarkdownView.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "sidebar/OutlinksPanel.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/core/NoteDocument.h"

using namespace Corbomite;

class TestPanelsPopulated : public QObject {
    Q_OBJECT

private:
    CorbomiteApp *m_app = nullptr;
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

        m_app = new CorbomiteApp(this);
        m_mainWindow = new MainWindow(m_app);
        m_mainWindow->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow));
        settle(300);
    }

    void cleanupTestCase()
    {
        delete m_mainWindow;
        m_mainWindow = nullptr;
        delete m_app;
        m_app = nullptr;
    }

    // Open vault, open Hub.md, assert OutlinksPanel header reads "Outgoing Links (1)".
    void hubNoteShowsOneOutgoingLink()
    {
        QVERIFY(m_app->openVault(m_vaultDir.path()));
        settle(800);  // Allow rebuildVault + cache reconcile + panel refresh.

        // Open Hub.md — use openDocument so content is loaded and parsed.
        auto *vault = m_mainWindow->vaultObj();
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
        auto *vault = m_mainWindow->vaultObj();
        QVERIFY(vault);
        auto *spokeDoc = vault->openDocument(QStringLiteral("Spoke.md"));
        QVERIFY(spokeDoc);

        // BacklinksView is now an InternalPlugin (Cluster Q Task 13).
        // The plugin's host adds a tool view named
        // "corbomite-backlinks_panel" containing the live plugin widget.
        // Walk the child tree for the QListWidget inside that tool view.
        Q_UNUSED(spokeDoc);
        m_mainWindow->onNoteActivated(QStringLiteral("Spoke.md"));
        settle(300);

        auto *toolView = m_mainWindow->findChild<QWidget *>(
            QStringLiteral("corbomite-backlinks_panel"));
        QVERIFY2(toolView, "BacklinksPlugin tool view was not hosted by MainWindow");
        auto *list = toolView->findChild<QListWidget *>();
        QVERIFY(list);
        auto *header = toolView->findChild<QLabel *>();
        QVERIFY(header);
        QVERIFY2(header->text().contains(QStringLiteral("(1)")),
                 qPrintable(QStringLiteral("Header was: ") + header->text()));
    }
};

QTEST_MAIN(TestPanelsPopulated)
#include "tst_panels_populated.moc"
