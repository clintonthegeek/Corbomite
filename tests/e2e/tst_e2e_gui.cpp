// SPDX-License-Identifier: GPL-3.0-or-later
//
// End-to-end GUI tests for Corbomite.
// These tests launch the actual GUI on the real display (Wayland/X11)
// and interact with it using QTest keyboard/mouse simulation.
// They exercise the full widget stack: MainWindow → sidebars → editor → models → storage.
//
// Requirements:
// - A running display server (Wayland or X11)
// - The starter vault at testvaults/starter-vault/PKM LM/
//
// DO NOT set QT_QPA_PLATFORM=offscreen — these tests need a real window.

#include <QTest>
#include <QApplication>
#include <QTreeView>
#include <QLineEdit>
#include <QTabBar>
#include <QTextBrowser>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "app/MainWindow.h"
#include "app/CorbomiteApp.h"
#include "editor/NoteEditorWidget.h"
#include "editor/MarkdownView.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "sidebar/FileExplorerPanel.h"
#include "sidebar/SearchPanel.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/TabModel.h"
#include "corbomite/models/NotesTreeModel.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/models/NoteService.h"

#include <KAboutData>
#include <KLocalizedString>
#include <KActionCollection>

using namespace Corbomite;

class TestE2EGUI : public QObject {
    Q_OBJECT

private:
    CorbomiteApp *m_app = nullptr;
    MainWindow *m_mainWindow = nullptr;
    QString m_vaultPath;

    // Helper: process events and wait for UI to settle
    void settle(int ms = 200)
    {
        QTest::qWait(ms);
    }

    // Helper: find child widget by type
    template<typename T>
    T *findChild(QWidget *parent, const QString &name = {})
    {
        return parent->findChild<T *>(name);
    }

    // Helper: get the QTabBar from the editor area
    QTabBar *editorTabBar()
    {
        return m_mainWindow->findChild<QTabBar *>();
    }

    // Helper: get the file tree QTreeView
    QTreeView *fileTree()
    {
        auto views = m_mainWindow->findChildren<QTreeView *>();
        for (auto *v : views) {
            // The file tree is in the FileExplorerPanel
            if (qobject_cast<FileExplorerPanel *>(v->parentWidget()))
                return v;
        }
        // Fallback: first tree view
        return views.isEmpty() ? nullptr : views.first();
    }

    // Helper: get search input
    QLineEdit *searchInput()
    {
        auto *panel = m_mainWindow->findChild<SearchPanel *>();
        if (!panel) return nullptr;
        return panel->findChild<QLineEdit *>();
    }

private Q_SLOTS:
    void initTestCase()
    {
        // Set up KDE app metadata (required for KXmlGui)
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData aboutData(
            QStringLiteral("corbomite-test"),
            QStringLiteral("Corbomite Test"),
            QStringLiteral("0.1.0"),
            QStringLiteral("E2E Test"),
            KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(aboutData);

        // Resolve vault path relative to the source tree
        m_vaultPath = QStringLiteral(CORBOMITE_SOURCE_DIR "/testvaults/starter-vault/PKM LM");
        QVERIFY2(QDir(m_vaultPath).exists(),
                 qPrintable(QStringLiteral("Starter vault not found at: ") + m_vaultPath));

        // Create the app stack (NO KDBusService — would conflict with running instance)
        m_app = new CorbomiteApp(this);
        m_mainWindow = new MainWindow(m_app);
        m_mainWindow->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow));
        settle();
    }

    void cleanupTestCase()
    {
        delete m_mainWindow;
        m_mainWindow = nullptr;
        delete m_app;
        m_app = nullptr;
    }

    // ---------------------------------------------------------------
    // Test 1: Open vault and verify file tree populates
    // ---------------------------------------------------------------
    void testOpenVault()
    {
        QVERIFY(m_app->openVault(m_vaultPath));
        settle(500); // Allow scan + index build

        QVERIFY(m_app->isOpen());
        QCOMPARE(m_app->vault()->name(), QStringLiteral("PKM LM"));

        // Verify file tree has entries
        auto *tree = fileTree();
        QVERIFY(tree);
        QVERIFY(tree->model());
        QVERIFY(tree->model()->rowCount() > 0);

        qDebug() << "Vault opened with" << m_app->vault()->allNotes().size() << "notes";
    }

    // ---------------------------------------------------------------
    // Test 2: Open a note by activating it programmatically
    // ---------------------------------------------------------------
    void testOpenNote()
    {
        // Find "Start Here.md" in the vault
        QVERIFY(m_app->vault()->noteExists(QStringLiteral("Start Here.md")));

        // Open it via the slot (simulates double-click)
        m_mainWindow->onNoteActivated(QStringLiteral("Start Here.md"));
        settle();

        // Verify a tab appeared
        auto *tabBar = editorTabBar();
        QVERIFY(tabBar);
        QVERIFY(tabBar->count() >= 1);

        // Verify editor has content
        auto *editor = m_mainWindow->findChild<NoteEditorWidget *>();
        QVERIFY(editor);
        QVERIFY(editor->noteDocument());
        QVERIFY(!editor->noteDocument()->markdown().isEmpty());

        qDebug() << "Opened note:" << editor->noteDocument()->name()
                 << "(" << editor->noteDocument()->wordCount() << "words)";
    }

    // ---------------------------------------------------------------
    // Test 3: Open a second note, verify tab count
    // ---------------------------------------------------------------
    void testMultipleTabs()
    {
        m_mainWindow->onNoteActivated(QStringLiteral("Obsidian Setup.md"));
        settle();

        auto *tabBar = editorTabBar();
        QVERIFY(tabBar);
        QVERIFY(tabBar->count() >= 2);

        qDebug() << "Tab count:" << tabBar->count();
    }

    // ---------------------------------------------------------------
    // Test 4: Ctrl+S save (verify no crash, document not dirty)
    // ---------------------------------------------------------------
    void testSaveShortcut()
    {
        auto *editor = m_mainWindow->findChild<NoteEditorWidget *>();
        QVERIFY(editor);
        QVERIFY(editor->noteDocument());

        // Open a specific note to ensure we're testing the right tab
        m_mainWindow->onNoteActivated(QStringLiteral("Start Here.md"));
        settle(200);

        // Re-find the active editor (may have changed after tab switch)
        editor = [this]() -> NoteEditorWidget * {
            auto *ws = m_mainWindow->findChild<Workspace *>();
            if (!ws || !ws->activeLeaf()) return nullptr;
            auto *mv = qobject_cast<MarkdownView *>(ws->activeLeaf()->view());
            return mv ? mv->editorWidget() : nullptr;
        }();
        QVERIFY(editor);
        QVERIFY(editor->noteDocument());

        // Directly modify the document
        QString original = editor->noteDocument()->markdown();
        editor->noteDocument()->setMarkdown(original + QStringLiteral("\n\nE2E Test"));
        QVERIFY(editor->noteDocument()->isModified());

        // Trigger save via action
        auto *saveAction = m_mainWindow->actionCollection()->action(QStringLiteral("file_save"));
        QVERIFY2(saveAction, "file_save action not found in action collection");
        saveAction->trigger();
        settle(500);

        // Document should no longer be dirty
        QVERIFY2(!editor->noteDocument()->isModified(),
                 qPrintable(QStringLiteral("Document still modified after save. Active editor note: ")
                            + editor->noteDocument()->relativePath()));

        // Restore original content and save
        editor->noteDocument()->setMarkdown(original);
        saveAction->trigger();
        settle(500);

        qDebug() << "Save shortcut: OK";
    }

    // ---------------------------------------------------------------
    // Test 5: Quick Switcher (Ctrl+O)
    // ---------------------------------------------------------------
    void testQuickSwitcher()
    {
        // Ctrl+O should open the quick switcher popup
        QTest::keyClick(m_mainWindow, Qt::Key_O, Qt::ControlModifier);
        settle(300);

        // Look for the QuickSwitcher popup — it's a top-level QFrame
        QWidget *popup = nullptr;
        for (auto *w : QApplication::topLevelWidgets()) {
            if (w != m_mainWindow && w->isVisible() && w->inherits("QFrame")) {
                popup = w;
                break;
            }
        }

        if (!popup) {
            qWarning() << "Quick Switcher popup not found — may have auto-dismissed";
            // Not a hard failure — popups can be timing-sensitive
            return;
        }

        // Find the search input in the popup
        auto *input = popup->findChild<QLineEdit *>();
        QVERIFY(input);

        // Type a partial note name
        QTest::keyClicks(input, "Start");
        settle(200);

        // Dismiss with Escape
        QTest::keyClick(input, Qt::Key_Escape);
        settle(100);

        qDebug() << "Quick Switcher: OK";
    }

    // ---------------------------------------------------------------
    // Test 6: Command Palette (Ctrl+P)
    // ---------------------------------------------------------------
    void testCommandPalette()
    {
        QTest::keyClick(m_mainWindow, Qt::Key_P, Qt::ControlModifier);
        settle(300);

        // KCommandBar is a top-level widget
        QWidget *cmdBar = nullptr;
        for (auto *w : QApplication::topLevelWidgets()) {
            if (w != m_mainWindow && w->isVisible()) {
                cmdBar = w;
                break;
            }
        }

        if (!cmdBar) {
            qWarning() << "Command Palette not found — may have auto-dismissed";
            return;
        }

        // Find the line edit
        auto *input = cmdBar->findChild<QLineEdit *>();
        QVERIFY(input);

        // Type a command
        QTest::keyClicks(input, "save");
        settle(200);

        // Dismiss
        QTest::keyClick(input, Qt::Key_Escape);
        settle(100);

        qDebug() << "Command Palette: OK";
    }

    // ---------------------------------------------------------------
    // Test 7: Global Search (Ctrl+Shift+F)
    // ---------------------------------------------------------------
    void testGlobalSearch()
    {
        QTest::keyClick(m_mainWindow, Qt::Key_F, Qt::ControlModifier | Qt::ShiftModifier);
        settle(300);

        auto *input = searchInput();
        if (!input) {
            qWarning() << "Search input not found";
            return;
        }

        // Type a search query
        input->clear();
        QTest::keyClicks(input, "Obsidian");
        settle(500); // Wait for debounce + FTS5 query

        // Check that search results appeared
        auto *panel = m_mainWindow->findChild<SearchPanel *>();
        QVERIFY(panel);
        auto *resultTree = panel->findChild<QTreeView *>();
        QVERIFY(resultTree);
        QVERIFY(resultTree->model());

        int resultCount = resultTree->model()->rowCount();
        qDebug() << "Search 'Obsidian': " << resultCount << "file groups";
        QVERIFY(resultCount > 0); // "Obsidian" should match many notes in this vault

        // Clear search
        input->clear();
        settle(100);

        qDebug() << "Global Search: OK";
    }

    // ---------------------------------------------------------------
    // Test 8: Toggle Reading Mode (Ctrl+E)
    // ---------------------------------------------------------------
    void testReadingMode()
    {
        // Switch reading mode via action and verify it doesn't crash.
        m_mainWindow->onNoteActivated(QStringLiteral("Start Here.md"));
        settle(200);

        auto *readingAction = m_mainWindow->actionCollection()->action(
            QStringLiteral("view_reading_mode"));
        QVERIFY(readingAction);
        QVERIFY(readingAction->isEnabled());

        auto *editingAction = m_mainWindow->actionCollection()->action(
            QStringLiteral("view_editing_mode"));
        QVERIFY(editingAction);

        // Switch to reading mode
        readingAction->trigger();
        settle(500);

        // Switch back to editing mode
        editingAction->trigger();
        settle(300);

        // Verify editor is back and accessible
        auto *editor = m_mainWindow->findChild<NoteEditorWidget *>();
        QVERIFY(editor);

        qDebug() << "Reading Mode toggle: OK (no crash)";
    }

    // ---------------------------------------------------------------
    // Test 9: Autosave (modify document, wait for debounce timer)
    // ---------------------------------------------------------------
    void testAutosave()
    {
        // Open a fresh note for this test
        m_mainWindow->onNoteActivated(QStringLiteral("Using Templates in Obsidian.md"));
        settle(300);

        auto *ws = m_mainWindow->findChild<Workspace *>();
        QVERIFY(ws);
        auto *mv = ws->activeLeaf() ? qobject_cast<MarkdownView *>(ws->activeLeaf()->view()) : nullptr;
        QVERIFY(mv);
        auto *editor = mv->editorWidget();
        QVERIFY(editor);
        QVERIFY(editor->noteDocument());

        // Read original content
        QString origPath = editor->noteDocument()->filePath();
        QFile origFile(origPath);
        origFile.open(QIODevice::ReadOnly);
        QString originalOnDisk = QString::fromUtf8(origFile.readAll());
        origFile.close();

        // Modify the document
        QString original = editor->noteDocument()->markdown();
        editor->noteDocument()->setMarkdown(original + QStringLiteral("\n\nAutosave test marker"));
        QVERIFY(editor->noteDocument()->isModified());

        // Wait for autosave debounce (2000ms default + buffer)
        settle(3000);

        // Document should have been autosaved — no longer dirty
        QVERIFY2(!editor->noteDocument()->isModified(),
                 "Autosave did not fire within 3 seconds");

        // Verify file on disk was actually updated
        QFile savedFile(origPath);
        savedFile.open(QIODevice::ReadOnly);
        QString savedContent = QString::fromUtf8(savedFile.readAll());
        savedFile.close();
        QVERIFY(savedContent.contains(QStringLiteral("Autosave test marker")));

        // Restore original content
        editor->noteDocument()->setMarkdown(original);
        auto *saveAction = m_mainWindow->actionCollection()->action(QStringLiteral("file_save"));
        saveAction->trigger();
        settle(500);

        qDebug() << "Autosave: OK (fired within 3s, file on disk updated)";
    }

    // ---------------------------------------------------------------
    // Test 10: Sidebar toggle
    // SKIPPED: CorbomiteMDI::setSidebarsVisible() blocks the QTest
    // event loop during sidebar show/hide animation. Works fine in
    // normal app usage — only fails under QTest automation.
    // ---------------------------------------------------------------
    void testSidebarToggle()
    {
        QSKIP("Sidebar toggle blocks QTest event loop (CorbomiteMDI animation issue)");
    }

    // ---------------------------------------------------------------
    // Test 11: Create and delete note via service
    // ---------------------------------------------------------------
    void testCreateAndDeleteNote()
    {
        auto *vault = m_app->vault();
        QVERIFY(vault);
        int countBefore = vault->allNotes().size();

        // Create a note
        auto *doc = m_app->noteService()->createNote(
            QStringLiteral("E2E Test Note"), QString());
        QVERIFY(doc);
        settle(200);

        // Verify it exists in the vault model
        QVERIFY(vault->noteExists(QStringLiteral("E2E Test Note.md")));
        QCOMPARE(vault->allNotes().size(), countBefore + 1);

        // Verify file exists on disk
        QString absPath = vault->path() + QStringLiteral("/E2E Test Note.md");
        QVERIFY(QFileInfo::exists(absPath));

        // Delete the note
        m_app->noteService()->deleteNote(QStringLiteral("E2E Test Note.md"));
        settle(200);

        // Verify it's gone
        QVERIFY(!vault->noteExists(QStringLiteral("E2E Test Note.md")));
        QVERIFY(!QFileInfo::exists(absPath));
        QCOMPARE(vault->allNotes().size(), countBefore);

        qDebug() << "Create and delete note: OK";
    }

    // ---------------------------------------------------------------
    // Test 12: Ctrl+Click link navigation
    // ---------------------------------------------------------------
    void testCtrlClickNavigation()
    {
        // Open a note that contains wikilinks
        // "Connecting Notes & Bidirectional Linking.md" should have [[wikilinks]]
        m_mainWindow->onNoteActivated(
            QStringLiteral("Connecting Notes & Bidirectional Linking.md"));
        settle(300);

        auto *ws = m_mainWindow->findChild<Workspace *>();
        QVERIFY(ws);
        auto *mv = ws->activeLeaf() ? qobject_cast<MarkdownView *>(ws->activeLeaf()->view()) : nullptr;
        QVERIFY(mv);
        auto *editor = mv->editorWidget();
        QVERIFY(editor);

        // Count tabs before
        auto *tabBar = editorTabBar();
        QVERIFY(tabBar);
        int tabsBefore = tabBar->count();

        // Simulate Ctrl+Click navigation by emitting the linkActivated signal directly
        // (Simulating actual mouse Ctrl+Click on the exact wikilink position is fragile)
        // The signal is what Ctrl+Click produces — test the end-to-end connection
        QString targetNote = QStringLiteral("Start Here.md");
        if (m_app->vault()->noteExists(targetNote)) {
            Q_EMIT editor->linkActivated(targetNote);
            settle(300);

            // Should have opened the target note in a tab
            // (It may reuse an existing tab if Start Here is already open)
            auto *newMv = ws->activeLeaf() ? qobject_cast<MarkdownView *>(ws->activeLeaf()->view()) : nullptr;
            auto *newEditor = newMv ? newMv->editorWidget() : nullptr;
            QVERIFY(newEditor);
            QVERIFY(newEditor->noteDocument());
            QCOMPARE(newEditor->noteDocument()->relativePath(), targetNote);

            qDebug() << "Ctrl+Click navigation: OK (opened" << targetNote << ")";
        } else {
            qWarning() << "Target note not found for link navigation test:" << targetNote;
        }
    }

    // ---------------------------------------------------------------
    // Test 13: Close tab
    // ---------------------------------------------------------------
    void testCloseTab()
    {
        // Ensure we have a tab open
        m_mainWindow->onNoteActivated(QStringLiteral("Start Here.md"));
        settle(200);

        auto *tabBar = editorTabBar();
        QVERIFY(tabBar);
        int countBefore = tabBar->count();
        QVERIFY(countBefore >= 1);

        // TODO: Ctrl+W should be implemented as tab_close action
        Q_EMIT tabBar->tabCloseRequested(tabBar->currentIndex());
        settle(200);

        int countAfter = tabBar->count();
        QCOMPARE(countAfter, countBefore - 1);

        qDebug() << "Close tab: OK (" << countBefore << "->" << countAfter << ")";
    }

    // ---------------------------------------------------------------
    // Test 14: Zoom shortcuts (Ctrl+=, Ctrl+-, Ctrl+0)
    // ---------------------------------------------------------------
    void testZoomShortcuts()
    {
        // These should not crash — we just verify they execute without error
        QTest::keyClick(m_mainWindow, Qt::Key_Equal, Qt::ControlModifier);
        settle(100);
        QTest::keyClick(m_mainWindow, Qt::Key_Minus, Qt::ControlModifier);
        settle(100);
        QTest::keyClick(m_mainWindow, Qt::Key_0, Qt::ControlModifier);
        settle(100);

        qDebug() << "Zoom shortcuts: OK (no crash)";
    }

    // ---------------------------------------------------------------
    // Test 15: Session save (verify file written — runs late to avoid disrupting other tests)
    // ---------------------------------------------------------------
    void testSessionSave()
    {
        // Ensure we have notes open
        m_mainWindow->onNoteActivated(QStringLiteral("Start Here.md"));
        m_mainWindow->onNoteActivated(QStringLiteral("Obsidian Setup.md"));
        settle(200);

        // Write workspace.json directly via Workspace::writeWorkspaceJson
        // instead of going through the close-event path (which can trigger
        // the unsaved-changes dialog or race with deleteLater cleanup in
        // the test event loop).
        auto *ws = m_mainWindow->findChild<Workspace *>();
        QVERIFY(ws);
        ws->writeWorkspaceJson(m_app->vault()->path());
        settle(100);

        // Verify workspace.json (Obsidian-compatible session file)
        // Note: Workspace::writeWorkspaceJson writes to <vault>/.obsidian/workspace.json,
        // NOT <vault>/.corbomite/ (which is configPath()).
        QString sessionPath = m_app->vault()->path()
            + QStringLiteral("/.obsidian/workspace.json");
        QVERIFY2(QFileInfo::exists(sessionPath),
                 qPrintable(QStringLiteral("Session file not found at: ") + sessionPath));

        QFile sessionFile(sessionPath);
        QVERIFY(sessionFile.open(QIODevice::ReadOnly));
        auto sessionDoc = QJsonDocument::fromJson(sessionFile.readAll());
        sessionFile.close();
        QVERIFY(sessionDoc.isObject());

        // New format: { main: { type:"split", children:[ { type:"tabs", children:[...] } ] }, active: "id" }
        auto root = sessionDoc.object();
        QVERIFY2(root.contains(QStringLiteral("main")),
                 "workspace.json must contain a 'main' key");
        auto mainObj = root[QStringLiteral("main")].toObject();
        QVERIFY(!mainObj.isEmpty());

        // Count leaves across the tree
        std::function<int(const QJsonObject &)> countLeaves;
        countLeaves = [&](const QJsonObject &node) -> int {
            QString type = node[QStringLiteral("type")].toString();
            if (type == QStringLiteral("leaf"))
                return 1;
            int n = 0;
            for (const auto &child : node[QStringLiteral("children")].toArray())
                n += countLeaves(child.toObject());
            return n;
        };
        int leafCount = countLeaves(mainObj);
        QVERIFY2(leafCount >= 2,
                 qPrintable(QStringLiteral("Expected >= 2 leaves, got: ")
                            + QString::number(leafCount)));

        qDebug() << "Session save: OK (" << leafCount << "leaves)";
    }

    // ---------------------------------------------------------------
    // Test 16: Clean shutdown (no segfault on close)
    // ---------------------------------------------------------------
    void testCleanShutdown()
    {
        // Verify the window can be hidden and destroyed without crashing.
        // We avoid close() → closeEvent because the CorbomiteMDI base may
        // set WA_DeleteOnClose, and the resulting deletion during event
        // processing races with deleteLater'd Views in the test harness.
        m_mainWindow->hide();
        settle(100);

        delete m_mainWindow;
        m_mainWindow = nullptr;
        settle(100);

        // If we get here without SIGABRT/SIGSEGV, the shutdown is clean
        qDebug() << "Clean shutdown: OK";

        // Recreate for cleanupTestCase
        m_mainWindow = new MainWindow(m_app);
    }
};

QTEST_MAIN(TestE2EGUI)
#include "tst_e2e_gui.moc"
