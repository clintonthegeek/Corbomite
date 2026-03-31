// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mdi/CorbomiteMDI.h"
#include <QLabel>
#include <QCloseEvent>

class KRecentFilesAction;

namespace Corbomite {

class VaultService;
class EditorViewManager;
class NoteEditorWidget;
class FileExplorerPanel;
class SearchPanel;
class NotesTreeModel;
class AutosaveReactor;
class FileWatchReactor;
class SessionManager;
class SQLiteIndex;
class BacklinksPanel;
class OutlinksPanel;
class OutlinePanel;
class LocalGraphPanel;

class MainWindow : public CorbomiteMDI::MainWindow {
    Q_OBJECT

public:
    explicit MainWindow(VaultService *vaultService, QWidget *parent = nullptr);
    ~MainWindow() override;

public Q_SLOTS:
    void onNoteActivated(const QString &relativePath);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupActions();
    void setupSidebars();
    void setupStatusBar();
    void setupEditor();

    // Action handlers
    void openVaultDialog();
    void createNewNote();
    void saveCurrentNote();
    void showQuickSwitcher();
    void showCommandPalette();
    void showSearchPanel();
    void openGraphView();
    void toggleEditorMode();
    void onVaultOpened();
    void onVaultClosed();
    void onCursorInfoChanged(int line, int column, int wordCount);
    void updateVaultActions();
    void updateWindowTitle(NoteEditorWidget *editor = nullptr);

    VaultService *m_vaultService;
    EditorViewManager *m_editorManager = nullptr;
    FileExplorerPanel *m_fileExplorer = nullptr;
    NotesTreeModel *m_treeModel = nullptr;
    QLabel *m_wordCountLabel = nullptr;
    QLabel *m_cursorPosLabel = nullptr;
    AutosaveReactor *m_autosave = nullptr;
    FileWatchReactor *m_fileWatch = nullptr;
    SessionManager *m_sessionManager = nullptr;
    SearchPanel *m_searchPanel = nullptr;
    SQLiteIndex *m_searchIndex = nullptr;
    BacklinksPanel *m_backlinksPanel = nullptr;
    OutlinksPanel *m_outlinksPanel = nullptr;
    OutlinePanel *m_outlinePanel = nullptr;
    LocalGraphPanel *m_localGraphPanel = nullptr;
    KRecentFilesAction *m_recentVaults = nullptr;
};

} // namespace Corbomite
